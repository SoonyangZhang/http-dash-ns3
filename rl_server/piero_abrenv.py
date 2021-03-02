# cannot pickle '_thread.lock' object
#https://stackoverflow.com/questions/62228507/multiprocessing-with-queue-typeerror-cant-pickle-thread-lock-objects
import os
import threading
import subprocess ,signal
from collections import deque
import numpy as np
import socket
import byte_codec as bc
import piero_message as pmsg
Terminate=False
def signal_handler(signum, frame):
    global Terminate
    Terminate =True
def count_files(path):
    num_files_rec=0
    for root,dirs,files in os.walk(path):
            for each in files:
                num_files_rec += 1
    return num_files_rec
class AbrEnv(object):
    def __init__(self,server,is_train,group_id,agent_id,bandwidth_id):
        self.server=server
        self.is_train=is_train
        self.group_id=group_id
        self.agent_id=agent_id
        self.bandwidth_id=bandwidth_id
        self.msgQ_lock=threading.Lock()
        self.msgQ=deque()
        self.server.register_abr(group_id,agent_id,self)
        self.create_ns3_client()
    def reset(self,is_train,group_id,agent_id,bandwidth_id):
        self.server.unregister_abr(group_id,agent_id)
        self.is_train=is_train
        self.group_id=group_id
        self.agent_id=agent_id
        self.bandwidth_id=bandwidth_id
        self.server.register_abr(group_id,agent_id,self)
        self.create_ns3_client()
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
        self.msgQ_lock.acquire()
        self.msgQ.append([peer,info])
        self.msgQ_lock.release()
    def _handle_request(self,peer,info):
        print ("abr: ",info.request_id,info.last,info.r)
        choice=np.random.randint(0,info.actions)
        terminate=0
        if self.group_id!=info.group_id or self.agent_id!=info.agent_id:
            terminate=1
        res=pmsg.ResponceInfo(choice,terminate)
        peer.send_responce(res)
    def process_event(self):
        list=[]
        self.msgQ_lock.acquire()
        while len(self.msgQ):
            peer,info=self.msgQ.popleft()
            list.append([peer,info])
        self.msgQ_lock.release()
        for i in range (len(list)):
            peer,info=list[i]
            if peer:
                self._handle_request(peer,info)
