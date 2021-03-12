import math
import  os
import numpy as np
def mkdir(path):
    folder = os.path.exists(path)
    if not folder:    
        os.makedirs(path)
class CdfInfo():
    def __init__(self,mark):
        self.mark=mark
        #the num of samples that value <=mark
        self.num=0
        self.index=-1
def get_cdf_list(sort_data,start,points,delta):
    cdf_list=[]
    n=len(sort_data)
    for i in range(points):
        mark=1.0*start+i*delta*1.0
        cdf=CdfInfo(mark);
        num=0
        if len(cdf_list)==0 or cdf_list[-1].index<0:
            for j in range(n):
                if sort_data[j]<=mark:
                    num=num+1
                    cdf.num=num
                    cdf.index=j
                else:
                    break
            cdf_list.append(cdf)
        else:
            last_cdf=cdf_list[-1]
            if last_cdf.num>=n:
                cdf.index=last_cdf.index
                cdf.num=n
            else:
                num=last_cdf.num
                cdf.num=last_cdf.num
                cdf.index=last_cdf.index
                assert num>0,"wrong"
                for j in range(last_cdf.index+1,n):
                    if sort_data[j]<=mark:
                        num=num+1
                        cdf.num=num
                        cdf.index=j
                    else:
                        break
            cdf_list.append(cdf)
    return cdf_list
def test_cdf_list():
    sort_data=[1,2,4,8,10,11,14,16,17,20,22,25,28,30,37,38,40,41,42,43]
    sort_data.sort()
    cdf_list=get_cdf_list(sort_data,sort_data[0],10,5)
    for i in range(len(cdf_list)):
        print (cdf_list[i].index,cdf_list[i].mark,cdf_list[i].num)
def ReadRewardInfo(fileName,column):
    r=[]
    line=""
    #the first sample is not counted.
    for index, line in enumerate(open(fileName,'r')):
        lineArr = line.strip().split()
        r.append(float(lineArr[column]))
    return r
if __name__ == '__main__':
    root_path="/home/zsy/ns-allinone-3.31/ns-3.31/traces/"
    dir="oboe_pro/"
    out_dir="oboe_cdf/"
    mkdir(root_path+out_dir)
    algos=["ppo","festive","panda","tobasco","osmp","raahs","fdash","sftm","svaa"]
    rewards_of_all_algos=[]
    r_min=100
    r_max=-100
    for i in range(len(algos)):
        name=root_path+dir+algos[i]+".txt"
        r=ReadRewardInfo(name,4)
        r.sort()
        rewards_of_all_algos.append(r)
        if r[0]<r_min:
            r_min=r[0]
        if r[-1]>r_max:
            r_max=r[-1]
    r_min_int=math.floor(r_min)
    r_max_int=math.ceil(r_max)
    print(r_min,r_max)
    print(r_min_int,r_max_int)
    length_unit=1.0
    points_unit=10+1
    delta=length_unit/(points_unit-1)
    semgments=r_max_int-r_min_int
    points=semgments*(points_unit-1)+1
    start=1.0*r_min_int
    for i in range(len(algos)):
        name=root_path+out_dir+algos[i]+".txt"
        r=rewards_of_all_algos[i]
        cdf_list=get_cdf_list(r,start,points,delta)
        f=open(name,"w")
        denom=len(r)
        for j in range(len(cdf_list)):
            mark=cdf_list[j].mark
            precision="%.4f"
            mark_str=precision%mark
            rate=cdf_list[j].num*1.0/denom
            rate_str=precision%rate
            out=str(j+1)+"\t"+mark_str+"\t"+rate_str+"\t"+str(cdf_list[j].num)+"\n"
            f.write(out)
        f.close()
        
