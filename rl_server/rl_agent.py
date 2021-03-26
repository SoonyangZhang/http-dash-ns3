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
        self.group_id=-1
        self.epoch=0
        self.sess=None
        self.actor=None
        self.terminate=False
        self.state_batchs=[]
        self.logger = logging.getLogger("rl")
        mp.Process.__init__(self)
    def handle_signal(self, signum, frame):
        self.terminate= True
    def stop_process(self):
        self.terminate= True
    def _init_tensorflow(self):
        os.environ["CUDA_VISIBLE_DEVICES"]="0"
        tf.disable_v2_behavior()
        #config = ConfigProto()
        #config.gpu_options.allow_growth = True
        #sess=tf.Session(config=config)
        #self.graph=tf.Graph()
        self.sess = tf.Session()
        scope="central"
        self.actor=network.Network(self.s_dim,self.a_dim,self.lr_rate,scope)
        init = tf.global_variables_initializer() 
        self.sess.run(init)
    def run(self):
        signal.signal(signal.SIGINT,self.handle_signal)
        signal.signal(signal.SIGTERM,self.handle_signal)
        signal.signal(signal.SIGHUP, self.handle_signal) 
        signal.signal(signal.SIGTSTP,self.handle_signal)
        for i in range(len(self.exp_pipes)):
            self.exp_pipes[i][1].close()
        for i in range(len(self.net_params_pipes)):
            self.net_params_pipes[i][1].close()
        
        fp.mkdir(model_dir)
        self._init_tensorflow()
        saver= tf.train.Saver()
        string="nn_model_all.ckpt"
        model_recover=model_dir+string
        if fp.check_filename_contain(model_dir,string):
            saver.restore(sess,model_recover)
        self.group_id=self.left
        self._write_net_param()
        while not self.terminate:
            self._check_exp_pipes()
            if len(self.state_batchs)==self.num_agent:
                data_batchs=self.state_batchs
                self.state_batchs=[]
                s, a, p, g = [], [], [], []
                for i in range(self.num_agent):
                    s +=data_batchs[i].s_batch
                    a +=data_batchs[i].a_batch
                    p +=data_batchs[i].p_batch
                    g +=data_batchs[i].v_batch
                s_batch = np.stack(s, axis=0)
                a_batch = np.vstack(a)
                p_batch = np.vstack(p)
                v_batch = np.vstack(g)
                self.logger.debug("train {}".format(self.group_id))
                self.actor.train(self.sess,s_batch, a_batch, p_batch, v_batch, self.epoch)
                self._write_net_param()
                self.group_id+=1
                self.epoch+=1
            if self.group_id>=self.right:
                break
        saver.save(self.sess,model_recover)
        self.sess.close()
        if self.terminate:
            self.logger.debug("terminate signal")
    def _check_exp_pipes(self):
        for i in range(len(self.exp_pipes)):
            if self.exp_pipes[i][0].poll(0):
                ele=None
                try:
                    ele=self.exp_pipes[i][0].recv()
                except EOFError:
                    self.logger.debug("error when read {}".format(i+1))
                if ele:
                    self.state_batchs.append(ele)
    def _write_net_param(self):
        params=self.actor.get_network_params(self.sess)
        actor_param=ActorParameter(params)
        for i in range(len(self.net_params_pipes)):
            self.net_params_pipes[i][0].send(actor_param)
class Agent(object):
    def __init__(self,context,agent_id):
        self.context=context
        self.agent_id=agent_id
        self.group_id=0
        self.s_dim=S_DIM
        self.a_dim=A_DIM
        self.state = np.zeros((self.s_dim[0], self.s_dim[1]),dtype=np.float32)
        self.lr_rate = ACTOR_LR_RATE
        self.ns3=None
        self.graph=None
        self.sess=None
        self.actor=None
        self.msg_count=0
        self.last_msg_sendts=0
        self.trans_delay=0
        self.intra_delay=0
        self.logger = logging.getLogger("rl")
        self.s_batch=[]
        self.a_batch=[]
        self.p_batch=[]
        self.r_batch=[]
        self.last_action=0
        self.last_prob=[]
        self.first_sample=True
        self._init_tensorflow()
    def __del__(self):
        if self.sess:
            self.sess.close()
    def _init_tensorflow(self):
        os.environ["CUDA_VISIBLE_DEVICES"]="0"
        tf.disable_v2_behavior()
        #config = ConfigProto()
        #config.gpu_options.allow_growth = True
        #sess=tf.Session(config=config)
        self.graph=tf.Graph()
        self.sess = tf.Session(graph=self.graph)
        with self.graph.as_default():
            scope="actor"+str(self.agent_id)
            self.actor=network.Network(self.s_dim,self.a_dim,self.lr_rate,scope)
            init = tf.global_variables_initializer() 
            self.sess.run(init)
    def process_request(self,info):
        res=None
        done=False
        if info and info.group_id!=self.group_id:
            ## where is the requese come from?
            res=self._step_action(info.fd,0,1)
        if info and info.group_id==self.group_id and info.request_id==0:
            # the lowest bit rate of first chunk
            self._update_state(info)
            res=self._step_action(info.fd,0,0)
        if info and info.group_id==self.group_id and info.request_id!=0:
            self._update_state(info)
            if not info.last:
                self.s_batch.append(self.state)
            if not self.first_sample:
                action_vec = np.zeros(self.a_dim)
                action_vec[self.last_action]=1
                self.a_batch.append(action_vec)
                self.r_batch.append(info.r)
                self.p_batch.append(self.last_prob)
            self.first_sample=False
            if not info.last:
                action_prob = self.actor.predict(self.sess,
                    np.reshape(self.state, (1, self.s_dim[0],self.s_dim[1])))
                action_cumsum = np.cumsum(action_prob)
                choice = (action_cumsum > np.random.randint(
                    1, RAND_RANGE) / float(RAND_RANGE)).argmax()
                res=self._step_action(info.fd,choice,0)
                self.last_action=choice
                self.last_prob=action_prob
            if info.last:
                done=True
                v_batch = self.actor.compute_v(self.sess,self.s_batch, self.a_batch, self.r_batch, done)
                batch=StateBatch(self.s_batch, self.a_batch, self.p_batch, v_batch)
                self.context.send_batch(self.agent_id,batch)
                if self.group_id%10==0:
                    self.logger.debug("{} {}".format(self.agent_id,self.group_id))
                #self._stat_out(None,self.r_batch)
                #self._print_stat_info()
        return res
    def update_net_param(self,actor_param):
        #synchronization of the network parameters from the coordinator
        self.actor.set_network_params(self.sess,actor_param.params)
    def create_ns3_env(self,group_id):
        self.reset_state()
        self.group_id=group_id
        exe_cmd=NS3_PATH+"piero-test --rl=true --tr=true --gr=%s --ag=%s --bwid=%s"
        cmd=exe_cmd%(str(group_id),str(self.agent_id),str(0))
        self.ns3=subprocess.Popen(cmd,shell =True)
    def reset_state(self):
        self.state = np.zeros((self.s_dim[0], self.s_dim[1]),dtype=np.float32)
        self.s_batch=[]
        self.a_batch=[]
        self.p_batch=[]
        self.r_batch=[]
        self.last_action=0
        self.last_prob=[]
        self.first_sample=True
        self._stat_info_clear()
    def _update_state(self,info):
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
    def _step_action(self,fd,choice,stop):
        res=pmsg.ResponceInfo(fd,choice,stop)
        return res
    def _stat_out(self,fout,rewards):
        l=len(rewards)
        rewards_min = np.min(rewards)
        rewards_5per = np.percentile(rewards, 5)
        rewards_mean = np.mean(rewards)
        rewards_median = np.percentile(rewards, 50)
        rewards_95per = np.percentile(rewards, 95)
        rewards_max = np.max(rewards)
        d="\t"
        out=str(self.group_id)+d+str(l)+d+str(rewards_min)+d+\
            str(rewards_5per)+d+str(rewards_mean)+d+\
            str(rewards_median)+d+str(rewards_95per)+d+\
            str(rewards_max)+"\n"
        #fout.write(out)
    def _stat_info_clear(self):
        self.msg_count=0
        self.last_msg_sendts=0
        self.trans_delay=0
        self.intra_delay=0
    def _print_stat_info(self):
        count=self.msg_count
        intra_delay_average=0
        trans_delay_average=0
        if count>1:
            temp=1.0*self.intra_delay/(count-1)
            intra_delay_average=int(temp)
        if count>0:
            temp=1.0*self.trans_delay/count
            trans_delay_average=int(temp)
class AgentManager(mp.Process):
    def __init__(self,first_id,last_id,left,right,state_pipe,net_params_pipe,exp_pipe):
        self.first_id=first_id
        self.last_id=last_id
        self.group_id=-1
        self.left=left
        self.right=right
        self.state_pipe=state_pipe
        self.net_params_pipe=net_params_pipe
        self.exp_pipe=exp_pipe
        self.agents=[]
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
        self.state_pipe[0].close()
        self.net_params_pipe[0].close()
        self.exp_pipe[0].close()
        for i in range(self.first_id,self.last_id):
            agent=Agent(self,i)
            self.agents.append(agent)
        self.group_id=self.left
        while not self.terminate:
            self._check_net_params_pipe()
            self._check_state_pipe()
            if self.group_id>self.right:
                break;
    def _check_net_params_pipe(self):
        actor_param=None
        if self.net_params_pipe[1].poll(0):
            try:
                actor_param=self.net_params_pipe[1].recv()
            except EOFError:
                self.logger.debug("actor_net_params error")
            if actor_param:
                if self.group_id<self.right:
                    for i in range(len(self.agents)):
                        self.agents[i].update_net_param(actor_param)
                        self.agents[i].create_ns3_env(self.group_id)
                self.group_id+=1
                self.logger.debug("group id {}".format(self.group_id))
    def _check_state_pipe(self):
        info=None
        res=None
        if self.state_pipe[1].poll(0):
            try:
                info=self.state_pipe[1].recv()
            except EOFError:
                self.logger.debug("info error")
        if info:
            index=info.agent_id-self.first_id
            if index>=0:
                res=self.agents[index].process_request(info)
                self.state_pipe[1].send(res)
    def send_batch(self,agent_id,batch):
        self.exp_pipe[1].send(batch)
