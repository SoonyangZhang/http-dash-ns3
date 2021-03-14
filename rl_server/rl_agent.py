import tensorflow.compat.v1 as tf
#from tensorflow.compat.v1 import ConfigProto
import threading
import numpy as np
import piero_abrenv as pabr
import a2c_net as network
import os
import file_op as fp
RAND_RANGE = 10000
S_DIM = [2,2]
A_DIM = 6
ACTOR_LR_RATE =1e-4
model_dir="model_data/"
class CentralAgent(object):
    def __init__(self,num_agent,start,stop,net_params_queues,exp_queues):
        self.num_agent=num_agent
        self.start=start
        self.stop=stop
        self.net_params_queues=net_params_queues
        self.exp_queues=exp_queues
        self.s_dim=S_DIM
        self.a_dim=A_DIM
        self.lr_rate = ACTOR_LR_RATE
        self._thread=None
        self.train_done=False
    def peek_done(self):
        return self.train_done
    def loop_start(self):
        if self._thread is not None:
            return
        self._thread = threading.Thread(target=self._thread_main)
        self._thread.start()
    def loop_stop(self):
        if self._thread is None:
            return
        if threading.current_thread() != self._thread:
            self._thread.join()
            self._thread = None
    def _thread_main(self):
        fp.mkdir(model_dir)
        os.environ["CUDA_VISIBLE_DEVICES"]="0"
        tf.disable_v2_behavior()
        #config = ConfigProto()
        #config.gpu_options.allow_growth = True
        #sess=tf.Session(config=config)
        sess=tf.Session()
        actor=network.Network(sess,self.s_dim,self.a_dim,self.lr_rate)
        init = tf.global_variables_initializer() 
        sess.run(init)
        saver= tf.train.Saver()
        string="nn_model_all.ckpt"
        model_recover=model_dir+string
        if fp.check_filename_contain(model_dir,string):
            saver.restore(sess,model_recover)
        self.actor_net_params = actor.get_network_params()
        for i in range(self.num_agent):
            self.net_params_queues[i].put(self.actor_net_params)
        for epoch in range(self.start,self.stop):
            s, a, p, g = [], [], [], []
            for i in range(self.num_agent):
                s_, a_, p_, g_ = self.exp_queues[i].get()
                s += s_
                a += a_
                p += p_
                g += g_
                s_batch = np.stack(s, axis=0)
                a_batch = np.vstack(a)
                p_batch = np.vstack(p)
                v_batch = np.vstack(g)
            actor.train(s_batch, a_batch, p_batch, v_batch, epoch)
            self.actor_net_params = actor.get_network_params()
            for i in range(self.num_agent):
                self.net_params_queues[i].put(self.actor_net_params)            
        self.train_done=True
        saver.save(sess,model_recover)
        sess.close()
class Agent(object):
    def __init__(self,f,tcp_server,agent_id,start,stop,is_train,net_params_queue,exp_queue):
        self.f=f
        self.tcp_server=tcp_server
        self.agent_id=agent_id
        self.start=start
        self.stop=stop
        self.is_train=is_train
        self.net_params_queue=net_params_queue
        self.exp_queue=exp_queue
        self.env=None
        self.s_dim=S_DIM
        self.a_dim=A_DIM
        self.lr_rate = ACTOR_LR_RATE
        self._thread=None
        self._thread_terminate = False
        self.train_done=False
    def peek_done(self):
        return self.train_done
    def loop_start(self):
        if self._thread is not None:
            return
        self._thread_terminate = False
        self._thread = threading.Thread(target=self._thread_main)
        self._thread.start()
    def loop_stop(self):
        if self._thread is None:
            return
        self._thread_terminate = True
        if threading.current_thread() != self._thread:
            self._thread.join()
            self._thread = None
    def _thread_main(self):
        os.environ["CUDA_VISIBLE_DEVICES"]="0"
        tf.disable_v2_behavior()
        self.env=pabr.AbrEnv(self.tcp_server,self.s_dim)
        #config = ConfigProto()
        #config.gpu_options.allow_growth = True
        #sess=tf.Session(config=config)
        sess=tf.Session()
        actor=network.Network(sess,self.s_dim,self.a_dim,self.lr_rate)
        init = tf.global_variables_initializer() 
        sess.run(init)
        # initial synchronization of the network parameters from the coordinator
        actor_net_params = self.net_params_queue.get()
        actor.set_network_params(actor_net_params)
        for group_id in range(self.start,self.stop):
            self.env.reset(self.is_train,group_id,self.agent_id,0)
            last_action=0
            last_prob=[]
            done=False
            s_batch, a_batch, p_batch, r_batch = [], [], [], []
            first_sample=True
            while True:
                peer,update,last=self.env.process_event()
                if update:
                    obs,r=self.env.get_state_reward()
                    if not last:
                        s_batch.append(obs)
                    if not first_sample:
                        action_vec = np.zeros(self.a_dim)
                        action_vec[last_action]=1
                        a_batch.append(action_vec)
                        r_batch.append(r)
                        p_batch.append(last_prob)
                    first_sample=False
                if update and not last:
                    action_prob = actor.predict(
                        np.reshape(obs, (1, self.s_dim[0],self.s_dim[1])))
                    action_cumsum = np.cumsum(action_prob)
                    choice = (action_cumsum > np.random.randint(
                        1, RAND_RANGE) / float(RAND_RANGE)).argmax()
                    self.env.step(peer,choice,0)
                    last_action=choice
                    last_prob=action_prob
                if last:
                    done=True
                    v_batch = actor.compute_v(s_batch, a_batch, r_batch, done)
                    #print(len(s_batch),len(a_batch),len(v_batch),len(p_batch))
                    self.exp_queue.put([s_batch, a_batch, p_batch, v_batch])
                    actor_net_params = self.net_params_queue.get()
                    actor.set_network_params(actor_net_params)
                    if group_id%10==0:
                        print(self.agent_id,group_id)
                    self.stat_out(group_id,r_batch)
                    break
                if self._thread_terminate:
                    break
            self.print_stat_info(group_id)
            if self._thread_terminate:
                break
        sess.close()
        self.train_done=True
        self.env.stop()
    def stat_out(self,epoch,rewards):
        l=len(rewards)
        rewards_min = np.min(rewards)
        rewards_5per = np.percentile(rewards, 5)
        rewards_mean = np.mean(rewards)
        rewards_median = np.percentile(rewards, 50)
        rewards_95per = np.percentile(rewards, 95)
        rewards_max = np.max(rewards)
        d="\t"
        out=str(epoch)+d+str(l)+d+str(rewards_min)+d+\
            str(rewards_5per)+d+str(rewards_mean)+d+\
            str(rewards_median)+d+str(rewards_95per)+d+\
            str(rewards_max)+"\n"
        self.f.write(out)
    def print_stat_info(self,group_id):
        count=self.env.msg_count
        intra_delay_average=0
        trans_delay_average=0
        if count>1:
            temp=1.0*self.env.intra_delay/(count-1)
            intra_delay_average=int(temp)
        if count>0:
            temp=1.0*self.env.trans_delay/count
            trans_delay_average=int(temp)
        print(self.agent_id,group_id,count,intra_delay_average,trans_delay_average)
