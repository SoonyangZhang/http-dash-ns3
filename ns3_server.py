from rl_server import byte_codec
import signal, os
import threading
import socket
import select
import errno
import subprocess
import time
SERVER_ERR_INVAL=-1
class TcpServer:
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
        self._socket.listen(128)
        self._epl= select.epoll()
        self._epl.register(self._socket.fileno(),select.EPOLLIN)
    def loop_start(self):
        if self._thread is not None:
            return 
        self._thread_terminate = False
        self._thread =threading.Thread(target=self._thread_main)
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
        if fd==self._socket.fileno():
            self._epl.unregister(fd)
            self._socket.close()
        elif fd in self.connections:
            self._epl.unregister(fd)
            self.connections[fd].close()
            self.connections.pop(fd)
    def shutdown(self):
        for fd, conn in self.connections.items():
            self._epl.unregister(fd)
            conn.close()
        self.connections.clear()
        self._close(self._socket.fileno())
    def parser_buffer(self,conn,buffer):
        prefix_cmd="./waf --run 'scratch/piero-test --rl=true --tr=%s --gr=%s --ag=%s --bwid=%s'"
        reader=byte_codec.DataReader()
        reader.append(buffer)
        tr=reader.read_uint8()
        group_id,_=reader.read_uint32()
        agent_id,_=reader.read_uint32()
        bandwith_id,_=reader.read_uint32()
        print(group_id,agent_id)
        reinforce="true"
        train=""
        if tr:
            train="true"
        else:
            train="false"
        cmd=prefix_cmd%(train,str(group_id),str(agent_id),str(bandwith_id))
        p= subprocess.Popen(cmd,shell = True,stdout=subprocess.PIPE)
        self.ns3_process.update({id(p):p})
        time.sleep(0.05)
Terminate=False
def signal_handler(signum, frame):
    global Terminate
    Terminate =True
#netstat -tunlp | grep port
def single_thread():
    tcp_server=TcpServer("localhost",3345)
    while True:
        tcp_server.loop_once()
        if Terminate:
            tcp_server.shutdown()
            break
if __name__ == '__main__':
    signal.signal(signal.SIGTERM, signal_handler)
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGHUP, signal_handler) # ctrl+c
    signal.signal(signal.SIGTSTP, signal_handler) #ctrl+z
    single_thread()
    print("stop")
