import signal, os
import threading
import socket
import select
import errno
import numpy as np
import byte_codec as bc
SERVER_ERR_INVAL=-1
class TcpPeer(object):
    def __init__(self,conn):
        self.conn=conn
        self.buffer=b''
    def incoming_data(self,buffer):
        self.buffer+=buffer
        reader=bc.DataReader()
        reader.append(self.buffer)
        all=len(self.buffer)
        sum,success=reader.read_varint()
        remain=b''
        if success:
            if sum<=reader.byte_remain():
                type,_=reader.read_uint8()
                request_id,_=reader.read_uint32()
                agent_id,_=reader.read_uint32()
                actions,_=reader.read_uint32()
                last_bytes,_=reader.read_uint64()
                time,_=reader.read_uint64()
                buffer,_=reader.read_uint64()
                self.request_handle(request_id,agent_id,actions,last_bytes,time,buffer)
                pos=reader.cursor()
                if pos<all:
                    remain=self.buffer[pos:all]
            self.buffer=remain
            return False
    def request_handle(self,request_id,agent_id,actions,last_bytes,time_ms,buffer_ms):
        #print (request_id,buffer_ms)
        sum=3
        type=1
        choice=np.random.randint(0,actions)
        termimate=0
        writer=bc.DataWriter()
        writer.write_varint(sum)
        writer.write_uint8(type)
        writer.write_uint8(choice)
        writer.write_uint8(termimate)
        self.conn.sendall(writer.content())
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
        self.conn.close()
class TcpServer():
    def __init__(self, mode, port,callback):
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
        self.peers={}
        self.callback =callback
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
                peer=TcpPeer(conn)
                self.peers.update({conn.fileno():peer})
                self._epl.register(conn.fileno(), select.EPOLLIN)
            elif events == select.EPOLLIN:
                ret=self.peers[fd].read_event()
                if ret:
                    #print("close")
                    self._epl.unregister(fd)
                    self.peers[fd].close_fd()
                    self.peers.pop(fd)
    def _thread_main(self):
        while True:
            if self._thread_terminate is True:
                break
            self.loop_once(retry_first_connection=True)
    def _close(self,fd):
        if fd==self._socket.fileno():
            self._epl.unregister(fd)
            self._socket.close()
        elif fd in self.peers:
            self._epl.unregister(fd)
            self.peers[fd].close_fd()
            self.peers.pop(fd)
    def shut_down(self):
        for fd, peer in self.peers.items():
            self._epl.unregister(fd)
            peer.close_fd()
        self.peers.clear()
        self._close(self._socket.fileno())
def echo_back(conn,buffer):
    print(buffer.decode('utf-8'))
    conn.sendall(buffer)
    return True
Terminate=False
def signal_handler(signum, frame):
    global Terminate
    Terminate =True

#netstat -tunlp | grep port
if __name__ == '__main__':
    signal.signal(signal.SIGTERM, signal_handler)
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGHUP, signal_handler) # ctrl+c
    signal.signal(signal.SIGTSTP, signal_handler) #ctrl+z
    echo_server=TcpServer("localhost",1234,echo_back)
    while True:
        echo_server.loop_once()
        if Terminate:
            echo_server.shut_down()
            break
print("stop")
