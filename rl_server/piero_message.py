class RequestInfo(object):
    def __init__(self,last,request_id,group_id,agent_id,actions,last_bytes,time,buffer,r):
        self.last=last
        self.request_id=request_id
        self.group_id=group_id
        self.agent_id=agent_id
        self.actions=actions
        self.last_bytes=last_bytes
        self.time=time
        self.buffer=buffer
        self.r=r
class ResponceInfo(object):
    def __init__(self,choice,terminate,downloadDelay=0):
        self.choice=choice
        self.terminate=terminate
        self.downloadDelay=downloadDelay
