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
def count_files_name_contain(pathname,child):
    count=0
    file_names=get_files_name(pathname)
    for i in range(len(file_names)):
        if child in file_names[i]:
            count=count+1
    return count
def get_files_name_contain(pathname,child):
    list=[]
    file_names=get_files_name(pathname)
    for i in range(len(file_names)):
        if child in file_names[i]:
            list.append(file_names[i])
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
def stat_out(f,index,rewards):
    l=len(rewards)
    rewards_min = np.min(rewards)
    rewards_5per = np.percentile(rewards, 5)
    rewards_mean = np.mean(rewards)
    rewards_median = np.percentile(rewards, 50)
    rewards_95per = np.percentile(rewards, 95)
    rewards_max = np.max(rewards)
    d="\t"
    out=str(index)+d+str(l)+d+str(rewards_min)+d+\
        str(rewards_5per)+d+str(rewards_mean)+d+\
        str(rewards_median)+d+str(rewards_95per)+d+\
        str(rewards_max)+"\n"
    f.write(out)
    f.flush()
def pro_model(data_folder,result_dir):
    algos=["festive","panda","tobasco","osmp","raahs","fdash","sftm","svaa"]
    samples=count_files_name_contain(data_folder,"festive_r.txt")
    mkdir(result_dir)
    group_id=0
    agent_id=0
    bid=0
    for i in range(len(algos)):
        dst=result_dir+algos[i]+".txt"
        f=open(dst,"w")
        child_str=algos[i]+"_r.txt"
        soruce_files=get_files_name_contain(data_folder,child_str)
        samples=len(soruce_files)
        for j in range(samples):
            name=soruce_files[j]
            origin=data_folder+name
            r=ReadRewardInfo(origin,1)
            r_copy=[]
            for k in range(0,len(r)):
                r_copy.append(r[k])
            stat_out(f,j,r_copy)
        f.close()
def pro_rl(data_folder,result_dir):
    soruce_files=get_files_name_contain(data_folder,"reinforce_r.txt")
    samples=len(soruce_files)
    mkdir(result_dir)
    dst=result_dir+"reinforce.txt"
    f=open(dst,"w")
    for j in range(samples):
        name=soruce_files[j]
        origin=data_folder+name
        r=ReadRewardInfo(origin,1)
        r_copy=[]
        for k in range(0,len(r)):
            r_copy.append(r[k])
        stat_out(f,j,r_copy)
    f.close()
if __name__ == '__main__':
    parent="/home/ipcom/zsy/ns-allinone-3.31/ns-3.31/traces/"
    process_dir=parent+"process/"
    cooked_test_dir=parent+"hunnan_cooked_test/"
    rl_cooked_test_dir=parent+"hunnan_rl_cooked_test/"
    oboe_dir=parent+"hunnan_oboe/"
    rl_oboe_dir=parent+"hunnan_rl_oboe/"
    result_dir=process_dir+"cooked_test/"
    pro_model(cooked_test_dir,result_dir)
    pro_rl(rl_cooked_test_dir,result_dir)
    
    result_dir=process_dir+"oboe/"
    pro_model(oboe_dir,result_dir)
    pro_rl(rl_oboe_dir,result_dir)
