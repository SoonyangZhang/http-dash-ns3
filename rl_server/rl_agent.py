import tensorflow.compat.v1 as tf
from tensorflow.compat.v1 import ConfigProto
import multiprocessing as mp
import os,time,signal,subprocess
import numpy as np
import random 
import a2c_net as network
import os
import logging
import file_op as fp
import piero_message as pmsg
BUFFER_NORM_FACTOR = 10.0 #seconds
Mbps=1000000.0
RAND_RANGE = 10000
S_DIM = [2,5]
A_DIM = 6
ACTOR_LR_RATE =1e-4
MODEL_SAVE_INTERVAL =5
model_dir="model_data/"
NS3_PATH="/home/ipcom/zsy/ns-allinone-3.31/ns-3.31/build/scratch/"
TRAIN_BW_FOLDER="/home/ipcom/zsy/ns-allinone-3.31/ns-3.31/bw_data/cooked_traces/"
TEST_BW_FOLDER="/home/ipcom/zsy/ns-allinone-3.31/ns-3.31/bw_data/cooked_test_traces/"
AGENT_MSG_STATEBATCH=0x00
AGENT_MSG_REWARDENTROPY=0x01
AGENT_MSG_NETPARAM=0x02
AGENT_MSG_NS3ARGS=0x03
AGENT_MSG_STOP=0x04
class StateBatch:
    def __init__(self,agent_id,s_batch, a_batch, p_batch, v_batch):
        self.agent_id=agent_id
        self.s_batch=s_batch
        self.a_batch=a_batch
        self.p_batch=p_batch
        self.v_batch=v_batch
class RewardEntropy:
    def __init__(self,trace_id,reward,entropy):
        self.trace_id=trace_id
        self.reward=reward
        self.entropy=entropy
class ActorParameter:
    def __init__(self,params):
        self.params=params
class Ns3Args:
    def __init__(self,train_or_test,group_id,trace_id_list):
        self.train_or_test=train_or_test
        self.group_id=group_id
        self.trace_id_list=trace_id_list
class CentralAgent(mp.Process):
    def __init__(self,num_agent,id_span,left,right,control_msg_pipes):
        self.num_agent=num_agent
        self.id_span=id_span
        self.left=left
        self.right=right
        self.control_msg_pipes=control_msg_pipes
        self.s_dim=S_DIM
        self.a_dim=A_DIM
        self.lr_rate = ACTOR_LR_RATE
        self.group_id=-1
        self.epoch=0
        self.sess=None
        self.actor=None
        self.terminate=False
        self.state_batchs=[]
        self.reward_entropy_list=[]
        self.train_trace_id_list=[]
        self.test_trace_id_list=[]
        self.test_trace_index=0
        self.test_expect_count=0
        self.train_mode=True
        self.can_send_train_args=False
        self.can_send_test_args=False
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
        #self.sess=tf.Session(config=config)
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
        for i in range(len(self.control_msg_pipes)):
            self.control_msg_pipes[i][1].close()
        num_train_traces=fp.count_files(TRAIN_BW_FOLDER)
        num_test_traces=5#fp.count_files(TEST_BW_FOLDER)
        self.train_trace_id_list=[i for i in range(num_train_traces)]
        self.test_trace_id_list=[i for i in range(num_test_traces)]
        fp.mkdir(model_dir)
        self._init_tensorflow()
        saver= tf.train.Saver()
        string="nn_model_all.ckpt"
        model_recover=model_dir+string
        if fp.check_filename_contain(model_dir,string):
            saver.restore(sess,model_recover)
        self.group_id=self.left
        self._write_net_param()
        self.can_send_train_args=True
        self._send_train_args(self.group_id)
        while not self.terminate:
            self._check_control_msg_pipe()
            if self.train_mode:
                self._process_train_mode()
                if self.group_id>=self.right:
                    self._stop_agents()
                    break
            else:
                self._process_test_mode()
        saver.save(self.sess,model_recover)
        self.sess.close()
        for i in range(len(self.control_msg_pipes)):
            self.control_msg_pipes[i][0].close()
        if self.terminate:
            self.logger.debug("terminate signal")
    def _process_train_mode(self):
        if self.can_send_train_args:
            self._send_train_args(self.group_id)
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
            self.group_id+=1
            self.epoch+=1
            self.logger.debug("train {}".format(self.group_id))
            self.actor.train(self.sess,s_batch, a_batch, p_batch, v_batch, self.epoch)
            if self.group_id <self.right:
                self._write_net_param()
                self.can_send_train_args=True
                if self.epoch%MODEL_SAVE_INTERVAL==0:
                    self.train_mode=False
                    self.reward_entropy_list=[]
                    self.test_trace_index=0
                    self.test_expect_count=0
                    self.can_send_test_args=True
    def _process_test_mode(self):
        if self.test_expect_count>0 and len(self.reward_entropy_list)==self.test_expect_count:
            sz=len(self.test_trace_id_list)
            if len(self.reward_entropy_list)==sz:
                self.train_mode=True
                self.can_send_test_args=False
                self._reward_entropy_mean()
            else:
                self.can_send_test_args=True
        if self.can_send_test_args:
            self._send_test_args()
    def _reward_entropy_mean(self):
        sz=len(self.reward_entropy_list)
        rewards=[]
        entropys=[]
        for i in range(sz):
            rewards.append(self.reward_entropy_list[i].reward)
            entropys.append(self.reward_entropy_list[i].entropy)
        self.logger.debug("reward {} entropy {}".format(np.min(rewards),np.min(entropys)))
    def _check_control_msg_pipe(self):
        for i in range(len(self.control_msg_pipes)):
            if self.control_msg_pipes[i][0].poll(0):
                msg=None
                try:
                    type,msg=self.control_msg_pipes[i][0].recv()
                except EOFError:
                    self.logger.debug("error when read {}".format(i+1))
                if msg and type==AGENT_MSG_STATEBATCH:
                    self.state_batchs.append(msg)
                if msg and type==AGENT_MSG_REWARDENTROPY:
                    self.reward_entropy_list.append(msg)
    def _write_net_param(self):
        params=self.actor.get_network_params(self.sess)
        actor_param=ActorParameter(params)
        for i in range(len(self.control_msg_pipes)):
            self.control_msg_pipes[i][0].send((AGENT_MSG_NETPARAM,actor_param))
    def _send_train_args(self,group_id):
        sample=random.sample(self.train_trace_id_list,self.num_agent)
        remain=self.num_agent
        off=0
        for i in range(len(self.control_msg_pipes)):
            l=[]
            n=min(remain,self.id_span)
            for j in range(n):
                l.append(sample[off+j])
            args=Ns3Args(True,group_id,l)
            self.control_msg_pipes[i][0].send((AGENT_MSG_NS3ARGS,args))
            remain=remain-n
            off+=n
        self.can_send_train_args=False
    def _send_test_args(self):
        group_id=2233
        sz=len(self.test_trace_id_list)
        tasks=min(self.num_agent,sz-self.test_expect_count)
        self.test_expect_count+=tasks
        i=0
        while tasks>0:
            l=[]
            n=min(tasks,self.id_span)
            for j in range(n):
                l.append(self.test_trace_id_list[self.test_trace_index+j])
            args=Ns3Args(False,group_id,l)
            self.control_msg_pipes[i][0].send((AGENT_MSG_NS3ARGS,args))
            tasks=tasks-n
            self.test_trace_index+=n
            i+=1
        self.can_send_test_args=False
    def _stop_agents(self):
        for i in range(len(self.control_msg_pipes)):
            self.control_msg_pipes[i][0].send((AGENT_MSG_STOP,'deadbeaf'))
class Agent(object):
    def __init__(self,context,agent_id):
        self.context=context
        self.agent_id=agent_id
        self.train_or_test=True
        self.group_id=0
        self.trace_id=0
        self.s_dim=S_DIM
        self.a_dim=A_DIM
        self.state = np.zeros((self.s_dim[0], self.s_dim[1]),dtype=np.float32)
        self.lr_rate = ACTOR_LR_RATE
        self.ns3=None
        self.graph=None
        self.sess=None
        self.actor=None
        self.logger = logging.getLogger("rl")
        self.s_batch=[]
        self.a_batch=[]
        self.p_batch=[]
        self.r_batch=[]
        self.entropy_record=[]
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
        self.graph=tf.Graph()
        #config = ConfigProto()
        #config.gpu_options.allow_growth = True
        #self.sess = tf.Session(graph=self.graph,config=config)
        self.sess = tf.Session(graph=self.graph)
        with self.graph.as_default():
            scope="actor"+str(self.agent_id)
            self.actor=network.Network(self.s_dim,self.a_dim,self.lr_rate,scope)
            init = tf.global_variables_initializer() 
            self.sess.run(init)
    def process_request(self,info):
        if self.train_or_test:
            return self._train_process_request(info)
        else:
            return self._test_process_request(info)
    def _train_process_request(self,info):
        res=None
        done=False
        if info and info.group_id!=self.group_id:
            ## where is the request come from?
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
                batch=StateBatch(self.agent_id,self.s_batch, self.a_batch, self.p_batch, v_batch)
                self.context.send_state_batch(batch)
                if self.group_id%10==0:
                    self.logger.debug("{} {}".format(self.agent_id,self.group_id))
        return res
    def _test_process_request(self,info):
        res=None
        if info and info.group_id!=self.group_id:
            ## where is the requese come from?
            res=self._step_action(info.fd,0,1)
        if info and info.group_id==self.group_id and info.request_id==0:
            # the lowest bit rate of first chunk
            self._update_state(info)
            res=self._step_action(info.fd,0,0)
        if info and info.group_id==self.group_id and info.request_id!=0:
            self._update_state(info)
            if not self.first_sample:
                self.r_batch.append(info.r)
            self.first_sample=False
            if not info.last:
                action_prob= self.actor.predict(self.sess,
                    np.reshape(self.state, (1, self.s_dim[0],self.s_dim[1])))
                noise = np.random.gumbel(size=len(action_prob))
                choice= np.argmax(np.log(action_prob) + noise)
                entropy= -np.dot(action_prob, np.log(action_prob))
                self.entropy_record.append(entropy)
                res=self._step_action(info.fd,choice,0)
            if info.last:
                mean_reward=np.mean(self.r_batch)
                mean_entropy=np.mean(self.entropy_record)
                re=RewardEntropy(self.trace_id,mean_reward,mean_entropy)
                self.context.reward_entropy(re)
        return res
    def update_net_param(self,params):
        #synchronization of the network parameters from the coordinator
        self.actor.set_network_params(self.sess,params)
    def create_ns3_env(self,train_or_test,group_id,trace_id):
        self.reset_state()
        self.train_or_test=train_or_test
        self.group_id=group_id
        self.trace_id=trace_id
        exe_cmd=NS3_PATH+"piero-test --rl=true --tr=%s --gr=%s --ag=%s --bwid=%s"
        if train_or_test:
            cmd=exe_cmd%("true",str(group_id),str(self.agent_id),str(trace_id))
        else:
            cmd=exe_cmd%("false",str(group_id),str(self.agent_id),str(trace_id))
        self.ns3=subprocess.Popen(cmd,shell =True)
    def reset_state(self):
        self.state = np.zeros((self.s_dim[0], self.s_dim[1]),dtype=np.float32)
        self.s_batch=[]
        self.a_batch=[]
        self.p_batch=[]
        self.r_batch=[]
        self.entropy_record=[]
        self.last_action=0
        self.last_prob=[]
        self.first_sample=True
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
    def _step_action(self,fd,choice,stop):
        res=pmsg.ResponceInfo(fd,choice,stop)
        return res
    def _stat_out(self,rewards):
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
class AgentManager(mp.Process):
    def __init__(self,first_id,last_id,state_pipe,control_msg_pipe):
        self.first_id=first_id
        self.last_id=last_id
        self.state_pipe=state_pipe
        self.control_msg_pipe=control_msg_pipe
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
        self.control_msg_pipe[0].close()
        for i in range(self.first_id,self.last_id):
            agent=Agent(self,i)
            self.agents.append(agent)
        while not self.terminate:
            deadbeaf=self._check_control_msg_pipe()
            self._check_state_pipe()
            if deadbeaf:
                break
        self.state_pipe[1].close()
        self.control_msg_pipe[1].close()
    def _check_control_msg_pipe(self):
        deadbeaf=False
        msg=None
        if self.control_msg_pipe[1].poll(0):
            try:
                type,msg=self.control_msg_pipe[1].recv()
            except EOFError:
                self.logger.debug("control_msg_pipe read error")
            if msg and type==AGENT_MSG_NETPARAM:
                net_params=msg.params
                for i in range(len(self.agents)):
                    self.agents[i].update_net_param(net_params)
            if msg and type==AGENT_MSG_NS3ARGS:
                self._process_ns3_args(msg)
            if msg and type==AGENT_MSG_STOP:
                deadbeaf=True
        return deadbeaf
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
                if res:
                    self.state_pipe[1].send(res)
    def _process_ns3_args(self,msg):
        assert len(msg.trace_id_list)<=len(self.agents)
        train_or_test=msg.train_or_test
        group_id=msg.group_id
        for i in range(len(msg.trace_id_list)):
            self.agents[i].create_ns3_env(train_or_test,group_id,msg.trace_id_list[i])
    def send_state_batch(self,batch):
        self.control_msg_pipe[1].send((AGENT_MSG_STATEBATCH,batch))
    def reward_entropy(self,re):
        self.control_msg_pipe[1].send((AGENT_MSG_REWARDENTROPY,re))
