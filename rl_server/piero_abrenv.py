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
        self.peerQ=deque()
        self.msgQ_lock=threading.Lock()
        self.msgQ=deque()
    def reset(self,is_train,group_id,agent_id,bandwidth_id):
        self.server.unregister_abr(self.group_id,self.agent_id)
        self.is_train=is_train
        self.group_id=group_id
        self.agent_id=agent_id
        self.bandwidth_id=bandwidth_id
        self._terminate_peer()
        self.state = np.zeros((self.s_dim[0], self.s_dim[1]),dtype=np.float32)
        self.last_info=None
        self.peerQ=deque()
        self.msgQ=deque()
        self.server.register_abr(self.group_id,self.agent_id,self)
        self.create_ns3_client()
    def stop(self):
        self.server.unregister_abr(self.group_id,self.agent_id)
        self.is_train=False
        self.group_id=0
        self.agent_id=0
        self.bandwidth_id=0
        self._terminate_peer()
        self.state = np.zeros((self.s_dim[0], self.s_dim[1]),dtype=np.float32)
        self.last_info=None
        self.peerQ=deque()
        self.msgQ=deque()
    def create_ns3_client(self):
        writer=bc.DataWriter()
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
            self.msgQ_lock.acquire()
            self.msgQ.append([peer,info])
            self.msgQ_lock.release()
        else:
            choice=np.random.randint(0,info.actions)
            res=pmsg.ResponceInfo(choice,1)
            peer.send_responce(res)
    def process_event(self):
        update=False
        last=False
        peer=None
        info=None
        self.msgQ_lock.acquire()
        if len(self.msgQ):
            peer,info=self.msgQ.popleft()
        self.msgQ_lock.release()
        if peer and info:
            self.peerQ.append(peer)
            if info.request_id==0:
                self._random_choice(info.actions,0)
                self._update_state(info)
            else:
                self._update_state(info)
                update=True
                if info.last:
                    last=True
        return update,last
    def _terminate_peer(self):
        list=[]
        self.msgQ_lock.acquire()
        while len(self.msgQ):
            peer,info=self.msgQ.popleft()
            list.append([peer,info])
        self.msgQ_lock.release()
        for i in range(len(list)):
            peer,info=list[i]
            choice=np.random.randint(0,info.actions)
            res=pmsg.ResponceInfo(choice,1)
            peer.send_responce(res)
    def _update_state(self,info):
        self.last_info=info
        throughput=0.0
        buffer_s=1.0*info.buffer/1000.0/BUFFER_NORM_FACTOR
        if info.time>0:
            throughput=float(info.last_bytes*8*1000.0)/info.time
            throughput=throughput/Mbps
        state = np.roll(self.state, -1, axis=1)
        state[0,-1]=throughput
        state[1,-1]=buffer_s
        self.state = state
        #print("abr ",info.request_id,info.last,info.r,throughput)
    def _random_choice(self,actions,terminate):
        choice=np.random.randint(0,actions)
        self.step(choice,terminate)
    def step(self,choice,terminate):
        if len(self.peerQ):
            peer=self.peerQ.popleft()
            res=pmsg.ResponceInfo(choice,terminate)
            peer.send_responce(res)
    def get_state_reward(self):
        return self.state,self.last_info.r
    def random_choice(self):
        self._random_choice(self.last_info.actions,0)
    
