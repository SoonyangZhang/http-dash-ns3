import tensorflow.compat.v1 as tf
import threading
import numpy as np
import piero_abrenv as pabr
import a2c_net as network
import os
RAND_RANGE = 10000
S_DIM = [2,2]
A_DIM = 6
ACTOR_LR_RATE =1e-4
class Agent(object):
    def __init__(self,tcp_server,agent_id,is_train):
        self.tcp_server=tcp_server
        self.agent_id=agent_id
        self.is_train=is_train
        self.env=None
        self.s_dim=S_DIM
        self.a_dim=A_DIM
        self.lr_rate = ACTOR_LR_RATE
        self._thread=None
        self._thread_terminate = False
        self.recv_last=False
    def peek_last(self):
        return self.recv_last
    def loop_start(self):
        if self._thread is not None:
            return
        self._thread_terminate = False
        self._thread = threading.Thread(target=self._thread_main)
        self._thread.start()
    def loop_stop(self):
        if self._thread is None:
            return
        self._thread_terminate = True
        if threading.current_thread() != self._thread:
            self._thread.join()
            self._thread = None
    def _thread_main(self):
        os.environ["CUDA_VISIBLE_DEVICES"]="0"
        tf.disable_v2_behavior()
        self.env=pabr.AbrEnv(self.tcp_server,self.s_dim,self.is_train,0,self.agent_id,0)
        sess=tf.Session()
        actor=network.Network(sess,self.s_dim,self.a_dim,self.lr_rate)
        init = tf.global_variables_initializer() 
        sess.run(init)
        while True:
            update,last=self.env.process_event()
            if update and not last:
                obs,r=self.env.get_state_reward()
                action_prob = actor.predict(
                    np.reshape(obs, (1, self.s_dim[0],self.s_dim[1])))
                action_cumsum = np.cumsum(action_prob)
                choice = (action_cumsum > np.random.randint(
                    1, RAND_RANGE) / float(RAND_RANGE)).argmax()
                self.env.step(choice,0)
            if last:
                self.recv_last=True
                break
            if self._thread_terminate:
                break
        sess.close()
        print("agent stop")
