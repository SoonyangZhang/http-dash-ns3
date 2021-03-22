import time
class RequestInfo(object):
    def __init__(self,fd,last,request_id,send_time,group_id,agent_id,actions,last_bytes,delay,buffer,r):
        self.fd=fd
        self.last=last
        self.request_id=request_id
        self.send_time=send_time
        t=time.time()
        self.receipt_time=int(round(t * 1000))
        self.group_id=group_id
        self.agent_id=agent_id
        self.actions=actions
        self.last_bytes=last_bytes
        self.delay=delay
        self.buffer=buffer
        self.r=r
class ResponceInfo(object):
    def __init__(self,fd,choice,terminate,downloadDelay=0):
        self.fd=fd
        self.choice=choice
        self.terminate=terminate
        self.downloadDelay=downloadDelay
