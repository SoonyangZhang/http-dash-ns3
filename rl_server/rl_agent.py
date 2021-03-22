import tensorflow.compat.v1 as tf
#from tensorflow.compat.v1 import ConfigProto
import multiprocessing as mp
import os,time,signal,subprocess
import numpy as np
import a2c_net as network
import os
import logging
import file_op as fp
import piero_message as pmsg
BUFFER_NORM_FACTOR = 10.0 #seconds
Mbps=1000000.0
RAND_RANGE = 10000
S_DIM = [2,2]
A_DIM = 6
ACTOR_LR_RATE =1e-4
model_dir="model_data/"
NS3_PATH="/home/zsy/ns-allinone-3.31/ns-3.31/build/scratch/"
class StateBatch(object):
    def __init__(self,s_batch, a_batch, p_batch, v_batch):
        self.s_batch=s_batch
        self.a_batch=a_batch
        self.p_batch=p_batch
        self.v_batch=v_batch
class ActorParameter(object):
    def __init__(self,params):
        self.params=params
class CentralAgent(mp.Process):
    def __init__(self,num_agent,left,right,net_params_pipes,exp_pipes):
        self.num_agent=num_agent
        self.left=left
        self.right=right
        self.net_params_pipes=net_params_pipes
        self.exp_pipes=exp_pipes
        self.s_dim=S_DIM
        self.a_dim=A_DIM
        self.lr_rate = ACTOR_LR_RATE
        self.actor=None
        self.terminate=False
        self.logger = logging.getLogger("rl")
        mp.Process.__init__(self)
    def handle_signal(self, signum, frame):
        self.terminate= True
    def stop_process(self):
        self.terminate= True
    def run(self):
        signal.signal(signal.SIGINT,self.handle_signal)
        signal.signal(signal.SIGTERM,self.handle_signal)
        signal.signal(signal.SIGHUP, self.handle_signal) 
        signal.signal(signal.SIGTSTP,self.handle_signal)
        for i in range(self.num_agent):
            self.exp_pipes[i][1].close()
            self.net_params_pipes[i][1].close()
        fp.mkdir(model_dir)
        os.environ["CUDA_VISIBLE_DEVICES"]="0"
        tf.disable_v2_behavior()
        #config = ConfigProto()
        #config.gpu_options.allow_growth = True
        #sess=tf.Session(config=config)
        sess=tf.Session()
        self.actor=network.Network(sess,self.s_dim,self.a_dim,self.lr_rate)
        init = tf.global_variables_initializer() 
        sess.run(init)
        saver= tf.train.Saver()
        string="nn_model_all.ckpt"
        model_recover=model_dir+string
        if fp.check_filename_contain(model_dir,string):
            saver.restore(sess,model_recover)
        self.write_net_param()
        for epoch in range(self.left,self.right):
            s, a, p, g = [], [], [], []
            if self.terminate:
                break
            while not self.terminate:
                count=0
                for i in range(self.num_agent):
                    if self.exp_pipes[i][0].poll(0):
                        count+=1
                if count==self.num_agent:
                    break
            if not self.terminate:
                data_batchs=[]
                for i in range(self.num_agent):
                    ele=None
                    try:
                        ele=self.exp_pipes[i][0].recv()
                    except EOFError:
                        self.logger.debug("error when read {}".format(i+1))
                    if ele:
                        data_batchs.append(ele)
                assert  len(data_batchs)==self.num_agent
                for i in range(self.num_agent):
                    s +=data_batchs[i].s_batch
                    a +=data_batchs[i].a_batch
                    p +=data_batchs[i].p_batch
                    g +=data_batchs[i].v_batch
                s_batch = np.stack(s, axis=0)
                a_batch = np.vstack(a)
                p_batch = np.vstack(p)
                v_batch = np.vstack(g)
                self.logger.debug("train {}".format(epoch))
                self.actor.train(s_batch, a_batch, p_batch, v_batch, epoch)
                self.write_net_param()
        saver.save(sess,model_recover)
        sess.close()
        if self.terminate:
            self.logger.debug("terminate signal")
    def write_net_param(self):
        params=self.actor.get_network_params()
        actor_param=ActorParameter(params)
        for i in range(self.num_agent):
            self.net_params_pipes[i][0].send(actor_param)
class Agent(mp.Process):
    def __init__(self,pathname,agent_id,left,right,state_pipe,net_params_pipe,exp_pipe):
        self.pathname=pathname
        self.agent_id=agent_id
        self.left=left
        self.right=right
        self.state_pipe=state_pipe
        self.net_params_pipe=net_params_pipe
        self.exp_pipe=exp_pipe
        self.s_dim=S_DIM
        self.a_dim=A_DIM
        self.state = np.zeros((self.s_dim[0], self.s_dim[1]),dtype=np.float32)
        self.lr_rate = ACTOR_LR_RATE
        self.actor=None
        self.msg_count=0
        self.last_msg_sendts=0
        self.trans_delay=0
        self.intra_delay=0
        self.terminate=False
        self.logger = logging.getLogger("rl")
        mp.Process.__init__(self)
    def handle_signal(self, signum, frame):
        self.terminate= True
    def stop_process(self):
        self.terminate= True
    def run(self):
        signal.signal(signal.SIGINT,self.handle_signal)
        signal.signal(signal.SIGTERM,self.handle_signal)
        signal.signal(signal.SIGHUP, self.handle_signal) 
        signal.signal(signal.SIGTSTP,self.handle_signal)
        fout=open(self.pathname,'a')
        self.state_pipe[0].close()
        self.net_params_pipe[0].close()
        self.exp_pipe[0].close()
        exe_cmd=NS3_PATH+"piero-test --rl=true --tr=true --gr=%s --ag=%s --bwid=%s"
        os.environ["CUDA_VISIBLE_DEVICES"]="0"
        tf.disable_v2_behavior()
        #config = ConfigProto()
        #config.gpu_options.allow_growth = True
        #sess=tf.Session(config=config)
        sess=tf.Session()
        self.actor=network.Network(sess,self.s_dim,self.a_dim,self.lr_rate)
        init = tf.global_variables_initializer() 
        sess.run(init)
        self.update_net_param()
        for group_id in range(self.left,self.right):
            cmd=exe_cmd%(str(group_id),str(self.agent_id),str(0))
            p=subprocess.Popen(cmd,shell = True)
            self.stat_info_clear()
            self.reset_state()
            last_action=0
            last_prob=[]
            done=False
            first_sample=True
            s_batch, a_batch, p_batch, r_batch = [], [], [],[]
            if self.terminate:
                break
            while not self.terminate:
                update=False
                while True:
                    if self.state_pipe[1].poll(0):
                        update=True
                        break
                    if self.terminate:
                        break
                    if not update:
                        continue
                info=None
                if update:
                    try:
                        info=self.state_pipe[1].recv()
                    except EOFError:
                        self.logger.debug("info error")
                if info and info.group_id!=group_id:
                    ## where is the requese come from?
                    self.step_action(info.fd,0,1)
                if info and info.group_id==group_id and info.request_id==0:
                    # the lowest bit rate of first chunk
                    self.update_state(info)
                    self.step_action(info.fd,0,0)
                if info and info.group_id==group_id and info.request_id!=0:
                    self.update_state(info)
                    if not info.last:
                        s_batch.append(self.state)
                    if not first_sample:
                        action_vec = np.zeros(self.a_dim)
                        action_vec[last_action]=1
                        a_batch.append(action_vec)
                        r_batch.append(info.r)
                        p_batch.append(last_prob)
                    first_sample=False
                    if not info.last:
                        action_prob = self.actor.predict(
                            np.reshape(self.state, (1, self.s_dim[0],self.s_dim[1])))
                        action_cumsum = np.cumsum(action_prob)
                        choice = (action_cumsum > np.random.randint(
                            1, RAND_RANGE) / float(RAND_RANGE)).argmax()
                        self.step_action(info.fd,choice,0)
                        last_action=choice
                        last_prob=action_prob
                    if info.last:
                        done=True
                        v_batch = self.actor.compute_v(s_batch, a_batch, r_batch, done)
                        batch=StateBatch(s_batch, a_batch, p_batch, v_batch)
                        self.exp_pipe[1].send(batch)
                        self.update_net_param()
                        if group_id%10==0:
                            self.logger.debug("{} {}".format(self.agent_id,group_id))
                        self.stat_out(fout,group_id,r_batch)
                        break
        if self.terminate:
            self.logger.debug("terminate signal")
        sess.close()
        fout.close()
    def update_net_param(self):
        # initial synchronization of the network parameters from the coordinator
        while self.net_params_pipe[1].poll(0)<0:
            if self.terminate:
                break
            continue
        if not self.terminate:
            try:
                actor_param=self.net_params_pipe[1].recv()
                self.actor.set_network_params(actor_param.params)
            except EOFError:
                self.logger.debug("actor_net_params error")
    def reset_state(self):
        self.state = np.zeros((self.s_dim[0], self.s_dim[1]),dtype=np.float32)
    def update_state(self,info):
        throughput=0.0
        buffer_s=1.0*info.buffer/1000.0/BUFFER_NORM_FACTOR
        if info.delay>0:
            throughput=float(info.last_bytes*8*1000.0)/info.delay
            throughput=throughput/Mbps
        state = np.roll(self.state, -1, axis=1)
        state[0,-1]=throughput
        state[1,-1]=buffer_s
        self.state = state
        if self.msg_count>0:
            self.intra_delay+=info.send_time-self.last_msg_sendts
        self.trans_delay+=info.receipt_time-info.send_time
        self.last_msg_sendts=info.send_time
        self.msg_count=self.msg_count+1
    def step_action(self,fd,choice,stop):
        res=pmsg.ResponceInfo(fd,choice,stop)
        self.state_pipe[1].send(res)
    def stat_out(self,fout,epoch,rewards):
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
        #fout.write(out)
    def stat_info_clear(self):
        self.msg_count=0
        self.last_msg_sendts=0
        self.trans_delay=0
        self.intra_delay=0
    def print_stat_info(self,group_id):
        count=self.msg_count
        intra_delay_average=0
        trans_delay_average=0
        if count>1:
            temp=1.0*self.intra_delay/(count-1)
            intra_delay_average=int(temp)
        if count>0:
            temp=1.0*self.trans_delay/count
            trans_delay_average=int(temp)
