from rl_server import byte_codec
import signal, os
import threading
import socket
import select
import errno
import subprocess
import time
RL_MESSAGE_LABEL=0x00
NS_MESSAGE_LABEL=0x01
TIMEOUT_ADD=1000 #milli seconds
class RequestClientMessage():
    def __init__(self,tr,gid,aid,bid):
        self.tr=tr
        self.gid=gid
        self.aid=aid
        self.bid=bid
        self.timeout=0
        self.p=None
        self.timeout_add(TIMEOUT_ADD)
    def timeout_add(self,add):
        t=time.time()
        self.timeout=int(round(t * 1000))+add
class SocketServer(object):
    def __init__(self, mode, port):
        self._thread = None
        self._thread_terminate = False
        # localhost -> (127.0.0.1)
        # public ->    (0.0.0.0)
        # otherwise, mode is interpreted as an IP address.
        if mode == "localhost":
            self.ip = mode
        elif mode == "public":
            self.ip ="0.0.0.0"
        else:
            self.ip ="127.0.0.1"
        self.port = port
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._socket.setblocking(False)
        self._socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._socket.bind((self.ip, self.port))
        self.connections={}
        self.ns3_process={}
        self.metas={}
        self._socket.listen(128)
        self._epl= select.epoll()
        self._epl.register(self._socket.fileno(),select.EPOLLIN)
        self.log=open("ns3_server.log",'w')
    def __del__(self):
        self.log.close()
    def loop_start(self):
        if self._thread is not None:
            return 
        self._thread_terminate = False
        self._thread =threading.Thread(target=self._thread_main)
        #self._thread.daemon = True
        self._thread.start()
    def loop_stop(self, force=False):
        if self._thread is None:
            return 
        self._thread_terminate = True
        if threading.current_thread() != self._thread:
            self._thread.join()
            self._thread = None
    def loop_once(self):
        epoll_list = self._epl.poll(0)
        for fd,events in epoll_list:
            if fd == self._socket.fileno():
                conn,addr =self._socket.accept()
                conn.setblocking(False)
                self.connections.update({conn.fileno():conn})
                self._epl.register(conn.fileno(), select.EPOLLIN)
            elif events == select.EPOLLIN:
                buffer=b""
                try:
                    while True:
                        msg= self.connections[fd].recv(1500)
                        if msg:
                            buffer+=msg
                        else:
                            if buffer:
                                self.parser_buffer(self.connections[fd],buffer)
                                buffer=b""
                            self._close(fd)
                            break
                except socket.error as e:
                    err = e.args[0]
                    if buffer:
                        self.parser_buffer(self.connections[fd],buffer)
                        self._close(fd)
                    if err == errno.EAGAIN or err == errno.EWOULDBLOCK:
                        pass
                    else:
                        self._close(fd)
        self.check_recreate_process()
        for key in list(self.ns3_process.keys()):
            ret=subprocess.Popen.poll(self.ns3_process[key])
            if ret is not None:
                self.ns3_process.pop(key)
    def _thread_main(self):
        while True:
            if self._thread_terminate is True:
                break
            self.loop_once(retry_first_connection=True)
    def _close(self,fd):
        if fd in self.connections:
            self._epl.unregister(fd)
            self.connections[fd].close()
            self.connections.pop(fd)
    def _close_server_fd(self):
        self._epl.unregister(self._socket.fileno())
        self._socket.close()
    def shutdown(self):
        for fd, conn in self.connections.items():
            self._epl.unregister(fd)
            conn.close()
        self.connections.clear()
        self._close_server_fd()
        self._epl.close()
    def parser_buffer(self,conn,buffer):
        reader=byte_codec.DataReader()
        reader.append(buffer)
        label,_=reader.read_uint8()
        if label==RL_MESSAGE_LABEL:
            tr,_=reader.read_uint8()
            group_id,_=reader.read_uint32()
            agent_id,_=reader.read_uint32()
            bandwith_id,_=reader.read_uint32()
            meta=RequestClientMessage(tr,group_id,agent_id,bandwith_id)
            self.create_ns3_process(meta,True)
        elif label==NS_MESSAGE_LABEL:
            group_id,_=reader.read_uint32()
            agent_id,_=reader.read_uint32()
            print("req: ",group_id,agent_id)
            uuid=group_id*2**32+agent_id
            self.metas.pop(uuid,None)
            writer=byte_codec.DataWriter()
            writer.write_uint32(group_id)
            writer.write_uint32(agent_id)
            conn.sendall(writer.content())
    def check_recreate_process(self):
        t=time.time()
        now=int(round(t * 1000))
        for key in list(self.metas.keys()):
            meta=self.metas[key]
            if now>meta.timeout:
                alive=self.check_process_alive(meta.p)
                if alive:
                    meta.timeout_add(TIMEOUT_ADD)
                else:
                    self.create_ns3_process(meta,False)
    def check_process_alive(self,p):
        alive=False
        if p:
            ret=subprocess.Popen.poll(p)
            if ret is None:
                alive=True
        return alive
    def create_ns3_process(self,meta,first):
        prefix_cmd="./waf --run 'scratch/piero-test --rl=true --tr=%s --gr=%s --ag=%s --bwid=%s'"
        tr=meta.tr
        group_id=meta.gid
        agent_id=meta.aid
        bandwith_id=meta.bid
        print(group_id,agent_id)
        reinforce="true"
        train=""
        if tr:
            train="true"
        else:
            train="false"
        uuid=group_id*2**32+agent_id
        alive=self.check_process_alive(meta.p)
        if not alive:
            cmd=prefix_cmd%(train,str(group_id),str(agent_id),str(bandwith_id))
            p=subprocess.Popen(cmd,shell = True)#,stdout=subprocess.PIPE
            self.ns3_process.update({id(p):p})
            self.metas.pop(uuid,None)
            meta.timeout_add(TIMEOUT_ADD)
            meta.p=p
            self.metas.update({uuid:meta})
        else:
            meta.timeout_add(TIMEOUT_ADD)
        if not first:
            self.log.write(str(group_id)+"\t"+str(agent_id)+"\n")
            self.log.flush()
        time.sleep(0.5)
Terminate=False
def signal_handler(signum, frame):
    global Terminate
    Terminate =True
#netstat -tunlp | grep port
def single_thread():
    server=SocketServer("localhost",3345)
    while True:
        server.loop_once()
        if Terminate:
            server.shutdown()
            break
if __name__ == '__main__':
    signal.signal(signal.SIGTERM, signal_handler)
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGHUP, signal_handler) # ctrl+c
    signal.signal(signal.SIGTSTP, signal_handler) #ctrl+z
    single_thread()
    print("stop")
