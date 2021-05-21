import signal, os
import multiprocessing as mp
import numpy as np
import rl_agent as ra
import file_op as fp
import tcp_service as ts
import time
import math
import logging
import argparse
Terminate=False
def signal_handler(signum, frame):
    global Terminate
    Terminate =True
def multi_thread_train(num_agent,id_span,left,right):
    state_pipes=[]
    control_msg_pipes=[]
    managers=[]
    num_manager=math.ceil(num_agent/id_span)
    for i in range(num_manager):
        first_id=i*id_span
        last_id=min((i+1)*id_span,num_agent)
        conn1,conn2=mp.Pipe()
        conn3,conn4=mp.Pipe()
        state_pipes.append((conn1,conn2))
        control_msg_pipes.append((conn3,conn4))
        manager=ra.AgentManager(first_id,last_id,state_pipes[i],control_msg_pipes[i])
        managers.append(manager)
        manager.start()
    tcp_server=ts.TcpServer("localhost",1234,state_pipes,id_span)
    coordinator=ra.CentralAgent(num_agent,id_span,left,right,control_msg_pipes)
    coordinator.start()
    for i in range(len(state_pipes)):
        state_pipes[i][1].close()
    while not Terminate:
        tcp_server.loop_once()
        active=0;
        for i in range(num_manager):
            if managers[i].is_alive():
                active+=1
        if coordinator.is_alive():
            active+=1
        if active==0:
            break
    tcp_server.shutdown()
    coordinator.stop_process()
    coordinator.join()
    for i in range(num_manager):
        managers[i].stop_process()
        managers[i].join()
def multi_thread_test(num_agent,id_span):
    state_pipes=[]
    control_msg_pipes=[]
    managers=[]
    num_manager=math.ceil(num_agent/id_span)
    for i in range(num_manager):
        first_id=i*id_span
        last_id=min((i+1)*id_span,num_agent)
        conn1,conn2=mp.Pipe()
        conn3,conn4=mp.Pipe()
        state_pipes.append((conn1,conn2))
        control_msg_pipes.append((conn3,conn4))
        manager=ra.AgentManager(first_id,last_id,state_pipes[i],control_msg_pipes[i])
        managers.append(manager)
        manager.start()
    tcp_server=ts.TcpServer("localhost",1234,state_pipes,id_span)
    coordinator=ra.CentralTestAgent(num_agent,id_span,control_msg_pipes)
    coordinator.start()
    for i in range(len(state_pipes)):
        state_pipes[i][1].close()
    while not Terminate:
        tcp_server.loop_once()
        active=0;
        for i in range(num_manager):
            if managers[i].is_alive():
                active+=1
        if coordinator.is_alive():
            active+=1
        if active==0:
            break
    tcp_server.shutdown()
    coordinator.stop_process()
    coordinator.join()
    for i in range(num_manager):
        managers[i].stop_process()
        managers[i].join()
def start_train():
    NUM_AGENTS=8
    id_span=4
    TRAIN_EPOCH =200000
    fp.remove_dir(ra.NN_INFO_STORE_DIR)
    multi_thread_train(NUM_AGENTS,id_span,0,TRAIN_EPOCH)
def start_test():
    NUM_AGENTS=8
    id_span=4
    multi_thread_test(NUM_AGENTS,id_span)
#python ns3-dash-train.py --mode train
NS3_PATH="/home/ipcom/zsy/ns-allinone-3.31/ns-3.31/"
NS3_LIB_PATH=NS3_PATH+"build/lib/"
NS3_EXE_PATH=NS3_PATH+"build/scratch/"
EXE_TRAIN_COOK_TEMPLATE=NS3_EXE_PATH+"piero-rl-train --it=1 --gr=%s --ag=%s --bwid=%s"
EXE_TEST_COOK_TEMPLATE=NS3_EXE_PATH+"piero-rl-train --it=2 --log=%s --gr=%s --ag=%s --bwid=%s"
EXE_TEST_OBOE_TEMPLATE=NS3_EXE_PATH+"piero-rl-train --it=3 --log=%s --gr=%s --ag=%s --bwid=%s"
COOKED_TRACES=NS3_PATH+"bw_data/cooked_traces/"
COOKED_TEST_TRACES=NS3_PATH+"bw_data/cooked_test_traces/"
OBOE_TRACES=NS3_PATH+"bw_data/Oboe_traces/"
if __name__ == '__main__':
    signal.signal(signal.SIGTERM, signal_handler)
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGHUP, signal_handler) # ctrl+c
    signal.signal(signal.SIGTSTP, signal_handler) #ctrl+z
    parser = argparse.ArgumentParser(description='manual to this script')
    parser.add_argument('--mode', type=str,default ='test')
    args = parser.parse_args()
    mode=args.mode
    #ns3 so library path
    old = os.environ.get("LD_LIBRARY_PATH")
    if old:
        os.environ["LD_LIBRARY_PATH"] = old + ":" +NS3_LIB_PATH
    else:
        os.environ["LD_LIBRARY_PATH"] = NS3_LIB_PATH
    logging.basicConfig(format='[%(filename)s:%(lineno)d] %(message)s')
    logging.getLogger("rl").setLevel(logging.DEBUG)
    last= time.time()
    num_cooked_traces=fp.count_files(COOKED_TRACES)
    num_cooked_test_traces=fp.count_files(COOKED_TEST_TRACES)
    num_oboe_traces=fp.count_files(OBOE_TRACES)
    if mode=="train":
        ra.set_train_template(EXE_TRAIN_COOK_TEMPLATE)
        ra.set_num_train_traces(num_cooked_traces)
        ra.set_test_template(EXE_TEST_COOK_TEMPLATE)
        ra.set_num_test_traces(num_cooked_test_traces)
        start_train()
    elif mode=="test":
        ra.set_train_template(EXE_TRAIN_COOK_TEMPLATE)
        ra.set_num_train_traces(num_cooked_traces)
        ra.set_test_template(EXE_TEST_COOK_TEMPLATE)
        ra.set_num_test_traces(num_cooked_test_traces)
        start_test()
    elif mode=="oboe":
        ra.set_train_template(EXE_TRAIN_COOK_TEMPLATE)
        ra.set_num_train_traces(num_cooked_traces)
        ra.set_test_template(EXE_TEST_OBOE_TEMPLATE)
        ra.set_num_test_traces(num_oboe_traces)
        start_test()
    delta=time.time()-last
    print("stop: "+str(int(round(delta* 1000))))
