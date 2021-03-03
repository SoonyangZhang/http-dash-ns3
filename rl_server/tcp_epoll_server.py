import signal, os
import threading
import socket
import select
import errno
import numpy as np
import byte_codec as bc
import piero_abrenv as pabr
import piero_message as pmsg
SERVER_ERR_INVAL=-1
class TcpPeer(object):
    def __init__(self,server,conn):
        self.server=server
        self.conn=conn
        self.abr=None
        self.buffer=b''
        self.dead=False
    def incoming_data(self,buffer):
        self.buffer+=buffer
        reader=bc.DataReader()
        reader.append(self.buffer)
        all=len(self.buffer)
        sum,success=reader.read_varint()
        remain=b''
        close=False
        if success:
            if sum<=reader.byte_remain():
                type,_=reader.read_uint8()
                last,_=reader.read_uint8()
                request_id,_=reader.read_uint32()
                group_id,_=reader.read_uint32()
                agent_id,_=reader.read_uint32()
                actions,_=reader.read_uint32()
                last_bytes,_=reader.read_uint64()
                time,_=reader.read_uint64()
                buffer,_=reader.read_uint64()
                reward,_=reader.read_double()
                info=pmsg.RequestInfo(last,request_id,group_id,agent_id,actions,last_bytes,time,buffer,reward)
                pos=reader.cursor()
                if pos<all:
                    remain=self.buffer[pos:all]
                if self.abr is None:
                    self.abr=self.server.get_abr(group_id,agent_id)
                if self.abr:
                    self.abr.handle_request(self,info)
                else:
                    self.handle_request(info)
            self.buffer=remain
            return close
    def handle_request(self,info):
        print (info.group_id,info.agent_id,info.last,info.r)
        choice=np.random.randint(0,info.actions)
        terminate=0
        res=pmsg.ResponceInfo(choice,terminate)
        self.send_responce(res)
    def send_responce(self,res):
        if self.dead is False:
            sum=7
            type=1
            writer=bc.DataWriter()
            writer.write_varint(sum)
            writer.write_uint8(type)
            writer.write_uint8(res.choice)
            writer.write_uint8(res.terminate)
            writer.write_uint32(res.downloadDelay)
            self.conn.sendall(writer.content())
            if res.terminate:
                self.dead=True
    def read_event(self):
        close=False
        buffer=b''
        length=0
        try:
            while True:
                msg=self.conn.recv(1500)
                length+=len(msg)
                if msg:
                    buffer+=msg
                else:
                    if buffer:
                        self.incoming_data(buffer)
                        buffer=b''
                    #print("only close")
                    close=True
                    break
        except socket.error as e:
            err = e.args[0]
            #print ("error: "+str(err))
            if buffer:
                ret=self.incoming_data(buffer)
                if ret:
                   close=True
            if err == errno.EAGAIN or err == errno.EWOULDBLOCK:
                pass
            else:
                close=True
        #print("msglen: "+str(length))
        return close
    def close_fd(self):
        self.dead=True
        self.conn.close()
class TcpServer():
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
        self._abrs_lock = threading.Lock()
        self.abrs={}
        self.peers={}
        self._socket.listen(128)
        self._epl= select.epoll()
        self._epl.register(self._socket.fileno(),select.EPOLLIN)
    def loop_start(self):
        if self._thread is not None:
            return 
        self._thread_terminate = False
        self._thread = threading.Thread(target=self._thread_main)
        #self._thread.daemon = True
        self._thread.start()
    def loop_stop(self, force=False):
        if self._thread is None:
            return SERVER_ERR_INVAL
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
                peer=TcpPeer(self,conn)
                self.peers.update({conn.fileno():peer})
                self._epl.register(conn.fileno(), select.EPOLLIN)
            elif events == select.EPOLLIN:
                ret=self.peers[fd].read_event()
                if ret:
                    #print("close")
                    self._close(fd)
        for fd in list(self.peers.keys()):
            if self.peers[fd].dead:
                self._close(fd)
    def _thread_main(self):
        while True:
            self.loop_once()
            if self._thread_terminate is True:
                self.shutdown()
                break
    def _close(self,fd):
        if fd==self._socket.fileno():
            self._epl.unregister(fd)
            self._socket.close()
        elif fd in self.peers:
            self._epl.unregister(fd)
            self.peers[fd].close_fd()
            self.peers.pop(fd)
    def shutdown(self):
        for fd, peer in self.peers.items():
            self._epl.unregister(fd)
            peer.close_fd()
        self.peers.clear()
        self._close(self._socket.fileno())
    def get_abr(self,gid,aid):
        id=gid*2**32+aid
        env=None
        self._abrs_lock.acquire()
        env=self.abrs.get(id)
        self._abrs_lock.release()
        return env
    def register_abr(self,gid,aid,abr):
        id=gid*2**32+aid
        self._abrs_lock.acquire()
        old=self.abrs.get(id)
        if old is None:
            self.abrs.update({id:abr})
        self._abrs_lock.release()
    def unregister_abr(self,gid,aid):
        id=gid*2**32+aid
        self._abrs_lock.acquire()
        self.abrs.pop(id,None)
        self._abrs_lock.release()
Terminate=False
def signal_handler(signum, frame):
    global Terminate
    Terminate =True
def multi_thread():
    tcp_server=TcpServer("localhost",1234)
    tcp_server.loop_start()
    abr=pabr.AbrEnv(tcp_server,True,0,1,0)
    while True:
        update,last=abr.process_event()
        if update:
            abr.random_choice()
        if Terminate:
            tcp_server.loop_stop()
            break
if __name__ == '__main__':
    signal.signal(signal.SIGTERM, signal_handler)
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGHUP, signal_handler) # ctrl+c
    signal.signal(signal.SIGTSTP, signal_handler) #ctrl+z
    multi_thread()
    print("stop")
