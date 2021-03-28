import byte_codec as bc
class MessageBufferWrap():
    def __init__(self,fd,agent_id,buffer):
        self.fd=fd
        self.agent_id=agent_id
        self.buffer=buffer
class RequestInfo():
    def __init__(self):
        self.last=0
        self.request_id=0
        self.agent_id=0
        self.group_id=0
        self.last_quality=1
        self.actions=0
        self.segment_list=[]
        self.last_bytes=0
        self.delay=0
        self.buffer_level=0
        self.r=0.0
    def decode(self,buffer):
        reader=bc.DataReader()
        reader.append(buffer)
        sum,success=reader.read_varint()
        type,_=reader.read_uint8()
        self.last,_=reader.read_uint8()
        self.request_id,_=reader.read_varint()
        self.agent_id,_=reader.read_varint()
        self.group_id,_=reader.read_varint()
        self.last_quality,_=reader.read_uint8()
        self.actions,_=reader.read_uint8()
        for i in range(self.actions):
            sz,_=reader.read_uint32()
            self.segment_list.append(sz)
        self.last_bytes,_=reader.read_uint32()
        self.delay,_=reader.read_uint32()
        self.buffer_level,_=reader.read_uint32()
        self.r,_=reader.read_double()
class ResponceInfo():
    def __init__(self,fd,choice,terminate,downloadDelay=0):
        self.fd=fd
        self.choice=choice
        self.terminate=terminate
        self.downloadDelay=downloadDelay
