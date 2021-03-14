import os
import threading
import subprocess ,signal
from collections import deque
import numpy as np
import socket
import byte_codec as bc
import piero_message as pmsg
BUFFER_NORM_FACTOR = 10.0 #seconds
Mbps=1000000.0
RL_MESSAGE_LABEL=0x00
def count_files(path):
    num_files_rec=0
    for root,dirs,files in os.walk(path):
            for each in files:
                num_files_rec += 1
    return num_files_rec
class AbrEnv(object):
    def __init__(self,server,state_dim):
        self.server=server
        self.s_dim=state_dim
        self.is_train=False
        self.group_id=0
        self.agent_id=0
        self.bandwidth_id=0
        self.state = np.zeros((self.s_dim[0], self.s_dim[1]),dtype=np.float32)
        self.last_info=None
        self.msgQ_condition=threading.Condition()
        self.msgQ=deque()
        self.last_sendts=0
        self.intra_delay=0
        self.trans_delay=0;
        self.msg_count=0;
    def reset(self,is_train,group_id,agent_id,bandwidth_id):
        self.server.unregister_abr(self.group_id,self.agent_id)
        self.is_train=is_train
        self.group_id=group_id
        self.agent_id=agent_id
        self.bandwidth_id=bandwidth_id
        self.state = np.zeros((self.s_dim[0], self.s_dim[1]),dtype=np.float32)
        self.last_info=None
        self.msgQ_condition=threading.Condition()
        self.msgQ=deque()
        self.last_sendts=0
        self.intra_delay=0
        self.trans_delay=0;
        self.msg_count=0;
        self.server.register_abr(self.group_id,self.agent_id,self)
        self.create_ns3_client()
    def stop(self):
        self.server.unregister_abr(self.group_id,self.agent_id)
        self.is_train=False
        self.group_id=0
        self.agent_id=0
        self.bandwidth_id=0
        self.state = np.zeros((self.s_dim[0], self.s_dim[1]),dtype=np.float32)
        self.last_info=None
        self.msgQ_condition=threading.Condition()
        self.msgQ=deque()
    def create_ns3_client(self):
        writer=bc.DataWriter()
        writer.write_uint8(RL_MESSAGE_LABEL)
        if self.is_train:
            writer.write_uint8(1)
        else:
            writer.write_uint8(0)
        writer.write_uint32(self.group_id)
        writer.write_uint32(self.agent_id)
        writer.write_uint32(self.bandwidth_id)
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.connect(('localhost', 3345))
        client.sendall(writer.content())
        client.close()
    def handle_request(self,peer,info):
        if self.group_id==info.group_id and self.agent_id==info.agent_id:
            if self.msg_count>0:
                self.intra_delay+=info.send_time-self.last_sendts
            self.trans_delay+=info.receipt_time-info.send_time
            self.last_sendts=info.send_time
            self.msg_count=self.msg_count+1
            self.msgQ_condition.acquire()
            self.msgQ.append([peer,info])
            self.msgQ_condition.notify()
            self.msgQ_condition.release()
        else:
            choice=np.random.randint(0,info.actions)
            res=pmsg.ResponceInfo(choice,1)
            peer.send_responce(res)
    def process_event(self):
        update=False
        last=False
        peer=None
        info=None
        self.msgQ_condition.acquire()
        tasks=len(self.msgQ)
        if tasks==0:
            self.msgQ_condition.wait()
        if tasks>0:
            peer,info=self.msgQ.popleft()
        self.msgQ_condition.release()
        if peer and info:
            if info.request_id==0:
                self._random_choice(peer,info.actions,0)
                self._update_state(info)
            else:
                self._update_state(info)
                update=True
                if info.last:
                    last=True
        return peer,update,last
    def _update_state(self,info):
        self.last_info=info
        throughput=0.0
        buffer_s=1.0*info.buffer/1000.0/BUFFER_NORM_FACTOR
        if info.delay>0:
            throughput=float(info.last_bytes*8*1000.0)/info.delay
            throughput=throughput/Mbps
        state = np.roll(self.state, -1, axis=1)
        state[0,-1]=throughput
        state[1,-1]=buffer_s
        self.state = state
        #print("abr ",info.request_id,info.last,info.r,throughput)
    def _random_choice(self,peer,actions,terminate):
        choice=np.random.randint(0,actions)
        self.step(peer,choice,terminate)
    def step(self,peer,choice,terminate):
        res=pmsg.ResponceInfo(choice,terminate)
        peer.send_responce(res)
    def get_state_reward(self):
        return self.state,self.last_info.r
    
