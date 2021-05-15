import tensorflow.compat.v1 as tf
from tensorflow.compat.v1 import ConfigProto
import multiprocessing as mp
import os,time,signal,subprocess
import numpy as np
import random 
import heapq
#import a2c_net as network
import ppo_net as network
import logging
import file_op as fp
import piero_message as pmsg
BUFFER_NORM_FACTOR = 10.0 #seconds
Mbps=1000000.0
VIDEO_BIT_RATE_MAX=5000.0
BYTE_NORM_FACTOR=1000000.0
RAND_RANGE = 10000
S_DIM = [6,8]
A_DIM = 6
ACTOR_LR_RATE =1e-4
MODEL_SAVE_INTERVAL =100
PRINT_LOG_INTERVAL=100
MODEL_DIR="model_data/"
NN_INFO_STORE_DIR="nn_info/"
PRETRAIN_MODEL_DIR="pretrain_dir/"
LOAD_MODEL_DIR="load_model/" #for test agent
NS3_PATH="/home/ipcom/zsy/ns-allinone-3.31/ns-3.31/"
NS3_EXE_PATH=NS3_PATH+"build/scratch/"
EXE_TEST_COOK_TEMPLATE=NS3_EXE_PATH+"piero-rl-train --it=2 --log=%s --gr=%s --ag=%s --bwid=%s"
EXE_TEST_OBOE_TEMPLATE=NS3_EXE_PATH+"piero-rl-train --it=3 --log=%s --gr=%s --ag=%s --bwid=%s"

EXE_TRAIN_TEMPLATE=NS3_EXE_PATH+"piero-rl-train --it=1 --gr=%s --ag=%s --bwid=%s"
EXE_TEST_TEMPLATE=EXE_TEST_COOK_TEMPLATE

COOKED_TEST_TRACES=NS3_PATH+"bw_data/cooked_test_traces/"
OBOE_TRACES=NS3_PATH+"bw_data/Oboe_traces/"

TRAIN_BW_FOLDER=NS3_PATH+"bw_data/cooked_traces/"
TEST_BW_FOLDER=COOKED_TEST_TRACES

VIDEO_BIT_RATE = [300.0,750.0,1200.0,1850.0,2850.0,4300.0]  # Kbps
DEFAULT_QUALITY=1
CHUNK_TIL_VIDEO_END_CAP =50.0
AGENT_MSG_STATEBATCH=0x00
AGENT_MSG_REWARDENTROPY=0x01
AGENT_MSG_NETPARAM=0x02
AGENT_MSG_NS3ARGS=0x03
AGENT_MSG_STOP=0x04
MODEL_RECORD_WINDOW=10
def TimeMillis():
    t=time.time()
    millis=int(round(t * 1000))
    return millis
def TimeMillis32():
    now=TimeMillis()&0xffffffff
    return now
def set_nn_info_store_dir(pathname):
    global NN_INFO_STORE_DIR
    temp=pathname
    sz=len(pathname)
    if '/'!=pathname[sz-1]:
        temp=pathname+"/"
    NN_INFO_STORE_DIR=temp
def set_test_oboe_trace():
    global EXE_TEST_TEMPLATE
    global EXE_TEST_OBOE_TEMPLATE
    global TEST_BW_FOLDER
    global OBOE_TRACES
    EXE_TEST_TEMPLATE=EXE_TEST_OBOE_TEMPLATE
    TEST_BW_FOLDER=OBOE_TRACES
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
    def __init__(self,train_or_test,log,group_id,trace_id_list):
        self.train_or_test=train_or_test
        self.log=log
        self.group_id=group_id
        self.trace_id_list=trace_id_list
class ModelInfo():
    def __init__(self,epoch,reward,entropy):
        self.epoch=epoch
        self.reward=reward
        self.entropy=entropy
    def __lt__(self, other):
        if self.reward<other.reward:
            return True
        else:
            return False
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
        self.saver=None
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
        self.fp_re=None
        self.model_info_heap=[]
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
        num_test_traces=fp.count_files(TEST_BW_FOLDER)
        self.train_trace_id_list=[i for i in range(num_train_traces)]
        self.test_trace_id_list=[i for i in range(num_test_traces)]
        fp.mkdir(NN_INFO_STORE_DIR)
        fp.mkdir(NN_INFO_STORE_DIR+MODEL_DIR)
        pathname=NN_INFO_STORE_DIR+str(TimeMillis32())+"_reward_and_entropy.txt"
        self.fp_re=open(pathname,"w")
        self._init_tensorflow()
        self.saver= tf.train.Saver(max_to_keep=2*MODEL_RECORD_WINDOW)
        if os.path.exists(PRETRAIN_MODEL_DIR):
            name="nn_model.ckpt"
            model_recover=PRETRAIN_MODEL_DIR+name            
            if fp.check_filename_contain(PRETRAIN_MODEL_DIR,name):
                self.logger.debug("model recover")
                self.saver.restore(self.sess,model_recover)
            else:
                self.logger.debug("no model")
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
        self.sess.close()
        self.fp_re.close()
        for i in range(len(self.control_msg_pipes)):
            self.control_msg_pipes[i][0].close()
        if self.terminate:
            self.logger.debug("terminate signal")
        pathname=NN_INFO_STORE_DIR+MODEL_DIR+"model_info.txt"
        model_info_file=open(pathname,"w")
        for i in range(len(self.model_info_heap)):
            info=self.model_info_heap[i]
            model_info_file.write(str(info.epoch)+"\t"+str(info.reward)+"\t"+
                                  str(info.entropy)+"\n")
        model_info_file.close()
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
            if self.epoch%PRINT_LOG_INTERVAL==1:
                self.logger.debug("train {}".format(self.group_id))
            if self.epoch%1000==1:
                self.fp_re.flush()
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
        average_reward=np.mean(rewards)
        average_entropy=np.mean(entropys)
        out=str(self.epoch)+"\t"+str(average_reward)+"\t"+str(average_entropy)+"\n"
        self.fp_re.write(out)
        info=ModelInfo(self.epoch,average_reward,average_entropy)
        self._save_model(info)
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
            args=Ns3Args(True,False,group_id,l)
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
            args=Ns3Args(False,False,group_id,l)
            self.control_msg_pipes[i][0].send((AGENT_MSG_NS3ARGS,args))
            tasks=tasks-n
            self.test_trace_index+=n
            i+=1
        self.can_send_test_args=False
    def _stop_agents(self):
        for i in range(len(self.control_msg_pipes)):
            self.control_msg_pipes[i][0].send((AGENT_MSG_STOP,'deadbeaf'))
    def _save_model(self,info):
        new_dir=NN_INFO_STORE_DIR+MODEL_DIR+str(info.epoch)+"/"
        fp.mkdir(new_dir)
        pathname=new_dir+"nn_model.ckpt"
        self.saver.save(self.sess,pathname)
        heapq.heappush(self.model_info_heap,info)
        if len(self.model_info_heap)>MODEL_RECORD_WINDOW:
            old_info=heapq.heappop(self.model_info_heap)
            old_dir=NN_INFO_STORE_DIR+MODEL_DIR+str(old_info.epoch)+"/"
            fp.remove_dir(old_dir)
class CentralTestAgent(mp.Process):
    def __init__(self,num_agent,id_span,control_msg_pipes):
        self.num_agent=num_agent
        self.id_span=id_span
        self.control_msg_pipes=control_msg_pipes
        self.s_dim=S_DIM
        self.a_dim=A_DIM
        self.lr_rate = ACTOR_LR_RATE
        self.epoch=0
        self.sess=None
        self.actor=None
        self.terminate=False
        self.done=False
        self.reward_entropy_list=[]
        self.test_trace_id_list=[]
        self.test_trace_index=0
        self.test_expect_count=0
        self.can_send_test_args=True
        self.fp_re=None
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
        num_test_traces=fp.count_files(TEST_BW_FOLDER)
        self.test_trace_id_list=[i for i in range(num_test_traces)]
        fp.mkdir(NN_INFO_STORE_DIR+MODEL_DIR)
        self._init_tensorflow()
        saver= tf.train.Saver()
        recovered=False
        if os.path.exists(LOAD_MODEL_DIR):
            name="nn_model.ckpt"
            model_recover=LOAD_MODEL_DIR+name
            if fp.check_filename_contain(LOAD_MODEL_DIR,name):
                self.logger.debug("model recover")
                saver.restore(self.sess,model_recover)
                recovered=True;
            else:
                self.logger.debug("no model")
        if recovered is False:
            self._stop_agents()
            return
        pathname=NN_INFO_STORE_DIR+str(TimeMillis32())+"_test_reward_and_entropy.txt"
        self.fp_re=open(pathname,"w")
        self._write_net_param()
        while not self.terminate:
            self._check_control_msg_pipe()
            self._process_test_mode()
            if self.done:
                self._stop_agents()
                break
        self.sess.close()
        self.fp_re.close()
        for i in range(len(self.control_msg_pipes)):
            self.control_msg_pipes[i][0].close()
        if self.terminate:
            self.logger.debug("terminate signal")
    def _process_test_mode(self):
        if self.test_expect_count>0 and len(self.reward_entropy_list)==self.test_expect_count:
            sz=len(self.test_trace_id_list)
            if len(self.reward_entropy_list)==sz:
                self._reward_entropy_mean()
                self.can_send_test_args=False
                self.done=True
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
        average_reward=np.mean(rewards)
        average_entropy=np.mean(entropys)
        out=str(self.epoch)+"\t"+str(average_reward)+"\t"+str(average_entropy)+"\n"
        self.fp_re.write(out)
    def _check_control_msg_pipe(self):
        for i in range(len(self.control_msg_pipes)):
            if self.control_msg_pipes[i][0].poll(0):
                msg=None
                try:
                    type,msg=self.control_msg_pipes[i][0].recv()
                except EOFError:
                    self.logger.debug("error when read {}".format(i+1))
                if msg and type==AGENT_MSG_STATEBATCH:
                    self._stop_agents()
                    assert 0,"shoule not genetate state msg"
                if msg and type==AGENT_MSG_REWARDENTROPY:
                    self.reward_entropy_list.append(msg)
    def _write_net_param(self):
        params=self.actor.get_network_params(self.sess)
        actor_param=ActorParameter(params)
        for i in range(len(self.control_msg_pipes)):
            self.control_msg_pipes[i][0].send((AGENT_MSG_NETPARAM,actor_param))
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
            args=Ns3Args(False,True,group_id,l)
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
    def process_request(self,msg):
        info=pmsg.RequestInfo()
        info.decode(msg.buffer)
        if self.train_or_test:
            return self._train_process_request(info,msg.fd)
        else:
            return self._test_process_request(info,msg.fd)
    def _train_process_request(self,info,fd):
        res=None
        done=False
        if info and info.group_id!=self.group_id:
            ## where is the request come from?
            res=self._step_action(fd,DEFAULT_QUALITY,1)
        if info and info.group_id==self.group_id and info.request_id==0:
            # the lowest bit rate of first chunk
            self._update_state(info)
            res=self._step_action(fd,DEFAULT_QUALITY,0)
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
                #action_cumsum = np.cumsum(action_prob)
                #choice = (action_cumsum > np.random.randint(
                #    1, RAND_RANGE) / float(RAND_RANGE)).argmax()
                noise = np.random.gumbel(size=len(action_prob))
                choice= np.argmax(np.log(action_prob) + noise)
                res=self._step_action(fd,choice,0)
                self.last_action=choice
                self.last_prob=action_prob
            if info.last:
                done=True
                v_batch = self.actor.compute_v(self.sess,self.s_batch, self.a_batch, self.r_batch, done)
                batch=StateBatch(self.agent_id,self.s_batch, self.a_batch, self.p_batch, v_batch)
                self.context.send_state_batch(batch)
        return res
    def _test_process_request(self,info,fd):
        res=None
        if info and info.group_id!=self.group_id:
            ## where is the requese come from?
            res=self._step_action(fd,DEFAULT_QUALITY,1)
        if info and info.group_id==self.group_id and info.request_id==0:
            # the lowest bit rate of first chunk
            self._update_state(info)
            res=self._step_action(fd,DEFAULT_QUALITY,0)
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
                res=self._step_action(fd,choice,0)
            if info.last:
                mean_reward=np.mean(self.r_batch)
                mean_entropy=np.mean(self.entropy_record)
                re=RewardEntropy(self.trace_id,mean_reward,mean_entropy)
                self.context.reward_entropy(re)
        return res
    def update_net_param(self,params):
        #synchronization of the network parameters from the coordinator
        self.actor.set_network_params(self.sess,params)
    def create_ns3_env(self,train_or_test,log_flag,group_id,trace_id):
        self.reset_state()
        self.train_or_test=train_or_test
        self.group_id=group_id
        self.trace_id=trace_id
        if train_or_test:
            exe_cmd=EXE_TRAIN_TEMPLATE
            cmd=exe_cmd%(str(group_id),str(self.agent_id),str(trace_id))
        else:
            log_out=str("0")
            if log_flag:
                log_out=str("1")
            exe_cmd=EXE_TEST_TEMPLATE
            cmd=exe_cmd%(log_out,str(group_id),str(self.agent_id),str(trace_id))
        self.ns3=subprocess.Popen(cmd,shell =True)
    def reset_state(self):
        self.state = np.zeros((self.s_dim[0], self.s_dim[1]),dtype=np.float32)
        bitrate=1.0*VIDEO_BIT_RATE[DEFAULT_QUALITY]/VIDEO_BIT_RATE_MAX
        for j in range(self.s_dim[1]):
            self.state[0,j]=bitrate
        self.s_batch=[]
        self.a_batch=[]
        self.p_batch=[]
        self.r_batch=[]
        self.entropy_record=[]
        self.last_action=0
        self.last_prob=[]
        self.first_sample=True
    def _update_state(self,info):
        bitrate=1.0*VIDEO_BIT_RATE[info.last_quality]/VIDEO_BIT_RATE_MAX
        buffer_s=1.0*info.buffer_level/1000.0/BUFFER_NORM_FACTOR
        throughput=0.0
        delay_s=0.0
        if info.delay>0:
            throughput=float(info.last_bytes*8*1000.0)/info.delay
            delay_s=1.0*info.delay/1000.0/BUFFER_NORM_FACTOR
            throughput=throughput/Mbps
        self.state = np.roll(self.state, -1, axis=1)
        self.state[0,-1]=bitrate
        self.state[1,-1]=buffer_s
        self.state[2,-1]=throughput
        self.state[3,-1]=delay_s
        next_video_chunk_sizes=[]
        for i in range(0,len(info.segment_list)):
            chunk_size=1.0*info.segment_list[i]/BYTE_NORM_FACTOR
            next_video_chunk_sizes.append(chunk_size)
        self.state[4,:self.a_dim]= np.array(next_video_chunk_sizes)
        self.state[5, -1] = np.minimum(1.0*info.request_id, CHUNK_TIL_VIDEO_END_CAP) / float(CHUNK_TIL_VIDEO_END_CAP)
        #print(info.request_id,info.buffer_level)
        #print(self.state)
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
        msg=None
        res=None
        if self.state_pipe[1].poll(0):
            try:
                msg=self.state_pipe[1].recv()
            except EOFError:
                self.logger.debug("msg error")
        if msg:
            index=msg.agent_id-self.first_id
            if index>=0:
                res=self.agents[index].process_request(msg)
                if res:
                    self.state_pipe[1].send(res)
    def _process_ns3_args(self,msg):
        assert len(msg.trace_id_list)<=len(self.agents)
        train_or_test=msg.train_or_test
        log_flag=msg.log
        group_id=msg.group_id
        for i in range(len(msg.trace_id_list)):
            self.agents[i].create_ns3_env(train_or_test,log_flag,group_id,msg.trace_id_list[i])
    def send_state_batch(self,batch):
        self.control_msg_pipe[1].send((AGENT_MSG_STATEBATCH,batch))
    def reward_entropy(self,re):
        self.control_msg_pipe[1].send((AGENT_MSG_REWARDENTROPY,re))
