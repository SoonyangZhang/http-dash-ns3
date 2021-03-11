import os
import numpy as np
def mkdir(path):
    folder = os.path.exists(path)
    if not folder:    
        os.makedirs(path)
def get_files_name(folder):
    list=[]
    num_files_rec=0
    for root,dirs,files in os.walk(folder):
            for each in files:
                list.append(each)
    return list
def ReadRewardInfo(fileName,column):
    r=[]
    line=""
    #the first sample is not counted.
    for index, line in enumerate(open(fileName,'r')):
        lineArr = line.strip().split()
        if len(lineArr)<1:
            continue
        r.append(float(lineArr[column]))
    return r
def stat_out(f,epoch,rewards):
    l=len(rewards)
    rewards_min = np.min(rewards)
    rewards_5per = np.percentile(rewards, 5)
    rewards_mean = np.mean(rewards)
    rewards_median = np.percentile(rewards, 50)
    rewards_95per = np.percentile(rewards, 95)
    rewards_max = np.max(rewards)
    d="\t"
    out=str(epoch)+d+str(l)+d+str(rewards_min)+d+\
        str(rewards_5per)+d+str(rewards_mean)+d+\
        str(rewards_median)+d+str(rewards_95per)+d+\
        str(rewards_max)+"\n"
    f.write(out)
    f.flush()
def pro_tradition():
    root_path="/home/zsy/ns-allinone-3.31/ns-3.31/traces/"
    dir="tradition/"
    algos=["festive","panda","tobasco","osmp","raahs","fdash","sftm","svaa"]
    all_n=142
    new_dir="process/"
    mkdir(root_path+new_dir)
    group_id=0
    agent_id=0
    bid=0
    for i in range(len(algos)):
        dst=root_path+new_dir+algos[i]+".txt"
        f=open(dst,"w")
        for j in range(all_n):
            name="%s_%s_%s_%s_r.txt"%(str(group_id),str(agent_id),str(j),algos[i])
            origin=root_path+dir+name
            r=ReadRewardInfo(origin,1)
            r_copy=[]
            for k in range(1,len(r)-1):
                r_copy.append(r[k])
            stat_out(f,j,r_copy)
        f.close()
def pro_rl():
    root_path="/home/zsy/ns-allinone-3.31/ns-3.31/traces/"
    dir="log_sim_rl/"
    all_n=142
    new_dir="process/"
    mkdir(root_path+new_dir)
    dst=root_path+new_dir+"ppo"+".txt"
    f=open(dst,"w")
    for j in range(all_n):
        name=str(j)+".txt"
        origin=root_path+dir+name
        r=ReadRewardInfo(origin,7)
        r_copy=[]
        for k in range(1,len(r)):
            r_copy.append(r[k])
        stat_out(f,j,r_copy)
    f.close()
if __name__ == '__main__':
    pro_rl()
