import struct
class DataReader(object):
    def __init__(self):
        self.buffer=b''
        self.length=0
        self.pos=0
    def append(self,buffer):
        self.buffer+=buffer
        self.length=len(self.buffer)
    def cursor(self):
        return self.pos
    def byte_remain(self):
        return self.length-self.pos
    def read_uint8(self):
        result=0
        success=False
        if self.pos+1<=self.length:
            temp=bytes(self.buffer[self.pos:self.pos+1])
            result,=struct.unpack("B",temp)
            self.pos+=1
            success=True
        return result,success
    def read_uint16(self):
        result=0
        success=False
        if self.pos+2<=self.length:
            temp=self.buffer[self.pos:self.pos+2]
            result,=struct.unpack("!H",temp)
            self.pos+=2
            success=True
        return result,success
    def read_uint32(self):
        result=0
        success=False
        if self.pos+4<=self.length:
            temp=self.buffer[self.pos:self.pos+4]
            result,=struct.unpack("!I",temp)
            self.pos+=4
            success=True
        return result,success
    def read_uint64(self):
        result=0
        success=False
        if self.pos+8<=self.length:
            temp=self.buffer[self.pos:self.pos+8]
            result,=struct.unpack("!Q",temp)
            self.pos+=8
            success=True
        return result,success
    def read_float(self):
        result=0
        success=False
        if self.pos+4<=self.length:
            temp=self.buffer[self.pos:self.pos+4]
            result,=struct.unpack("!f",temp)
            self.pos+=4
            success=True
        return result,success
    def read_double(self):
        result=0
        success=False
        if self.pos+8<=self.length:
            temp=self.buffer[self.pos:self.pos+8]
            result,=struct.unpack("!d",temp)
            self.pos+=8
            success=True
        return result,success
    def read_varint(self):
        result=0
        success=False
        multi=1
        length=self._varint_len()
        if length>0:
            for i in range(length):
                temp=bytes(self.buffer[self.pos+i:self.pos+i+1])
                v,=struct.unpack("B",temp)
                v=v&127
                result=result+v*multi
                multi*=128
            self.pos+=length
            success=True
        return result,success
    def _varint_len(self):
        length=0
        remain=self.byte_remain()
        decodable=False
        for i in range(remain):
            length+=1
            temp=bytes(self.buffer[self.pos+i:self.pos+i+1])
            v,=struct.unpack("B",temp)
            if v&128==0:
                decodable=True
                break
        if decodable is False:
            length=0
        return length
def varient_length(number):
    length=0;
    if number<=(0x7f):
        length=1;
    elif number<=(0x3fff):
        length=2;
    elif number<=(0x1fffff):
       length=3;
    elif number<=(0xfffffff):
       length=4;
    elif number<=(0x7ffffffff):
       length=5;
    elif number<=(0x3ffffffffff):
       length=6;
    elif number<=(0x1ffffffffffff):
       length=7;
    elif number<=(0xffffffffffffff):
        length=8;
    return length
def varint_encode(number):
    s = b""
    while True:
        byte = number % 128
        number = number // 128
        # If there are more digits to encode, set the top bit of this digit
        if number > 0:
            byte = byte|0x80
        s = s + struct.pack("!B", byte)
        if number == 0:
            return s
class DataWriter(object):
    def __init__(self):
        self.buffer=b''
    def length(self):
        return len(self.buffer)
    def content(self):
        return self.buffer
    def write_uint8(self,v):
        self.buffer+=struct.pack("B",v)
    def write_uint16(self,v):
        self.buffer+=struct.pack("!H",v)
    def write_uint32(self,v):
        self.buffer+=struct.pack("!I",v)
    def write_uint64(self,v):
        self.buffer+=struct.pack("!Q",v)
    def write_float(self,v):
        self.buffer+=struct.pack("!f",v)
    def write_double(self,v):
        self.buffer+=struct.pack("!d",v)
    def write_varint(self,v):
        length=varient_length(v)
        if length>0:
            self.buffer+=varint_encode(v)
        else:
            raise Exception("out of range")
def byte_codec_test():
    writer=DataWriter()
    a=1234
    b=4567
    c=12
    d=8765.4567
    writer.write_uint16(a)
    writer.write_varint(b)
    writer.write_float(d)
    writer.write_double(d)
    reader=DataReader()
    reader.append(writer.content())
    e,_=reader.read_uint16()
    f,_=reader.read_varint()
    g,_=reader.read_float()
    h,_=reader.read_double()
    print(e)
    print(f)
    print(g)
    print(h)