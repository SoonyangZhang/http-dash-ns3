# http-dash-ns3
## Introduction  
evaluate http dash adaptation algorithm on ns3  
The adaptation algorithms are refered from https://github.com/haraldott/dash   
and https://github.com/djvergad/dash   
And the reinforce learning module is refered from https://github.com/godka/Pensieve-PPO   
Thanks for their contribution.  
Eight algorithms are implemented:  
- [x] festive: Improving Fairness, Efficiency, and Stability in HTTP-based Adaptive Video Streaming with FESTIVE  
- [x] panda: Probe and Adapt: Rate Adaptation for HTTP Video Streaming At Scale  
- [x] tobasco: Adaptation Algorithm for Adaptive Streaming over HTTP  
- [x] osmp: QDASH: A QoE-aware DASH system  
- [x] raahs: Rate adaptation for adaptive HTTP streaming  
- [x] FDASH: A Fuzzy-Based MPEG/DASH Adaptation Algorithm   
- [x] sftm: Rate adaptation for dynamic adaptive streaming over HTTP in content distribution network  
- [x] svaa: Towards agile and smooth video adaptation in dynamic HTTP streaming  
## Build  
1 copy the folder cardiac-stem-cell to ns3-allinone-3.xx/ns-3.xx/src/  
2 copy the folder hunnan to ns3-allinone-3.xx/ns-3.xx/src/  
3 put piero-rl-train.cc and piero-hunnan-model.cc under scratch to ns3-allinone-3.xx/ns-3.xx/scratch/  
4 copy the folder rl_server to  ns3-allinone-3.xx/ns-3.xx/  
7 copy the folder bw_data to  ns3-allinone-3.xx/ns-3.xx/ and unzip the bandwidth trace  
├── cooked_test_traces   
├── cooked_traces  
└── Oboe_traces  
8 copy the folder video_data to  ns3-allinone-3.xx/ns-3.xx/  
9 create a new folder named traces under ns3-allinone-3.xx/ns-3.xx/  
10 change ns3_path in piero-rl-train.cc (L85) and piero-hunnan-model.cc (L85):  
```
std::string ns3_path="/home/ipcom/zsy/ns-allinone-3.31/ns-3.31/";//(the path of ns3)  
```
11 configure environmental variables:  
```
gedit /etc/profile  
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/zsy/ns-allinone-3.31/ns-3.31/build/lib/  
```
12 Rebuid ns3.  
```
./waf configure  
./waf build  
```
## Run  
- Test the above eight algorithms with cooked_traces    

```
cd ns3-allinone-3.xx/ns-3.xx  
sudo su  
./waf --run "scratch/piero-hunnan-model --it=1"  
```
- Test the above eight algorithms with cooked_test_traces    
```
./waf --run "scratch/piero-hunnan-model --it=2"  
```
- Test the above eight algorithms with Oboe_traces    
```
./waf --run "scratch/piero-hunnan-model --it=3"  
```
## Train a reinforce learning algorithm:  
- The ns3 path in rl_agent.py (Line 27) should be changed:  

```
NS3_PATH="/home/ipcom/zsy/ns-allinone-3.31/ns-3.31/"
```
the parameter MODEL_RECORD_WINDOW=10 in rl_agent.py will records 10 nn model data.

- Initial the train process:  

First, tensorflow shoud be installed with conda. And I use cpu version. 
```
cd ns3-allinone-3.xx/ns-3.xx/rl_server  
source /etc/profile  
conda activate tensorflow  
python ns3-dash-train.py --mode train   
```
the model can be found under ns-allinone-3.31/ns-3.31/rl_server/nn_info/model_data/  
```
#rl_server/rl_agent.py#L301
def _save_model(self,info):  
    new_dir=NN_INFO_STORE_DIR+MODEL_DIR+str(info.epoch)+"/"  
    fp.mkdir(new_dir)  
    pathname=new_dir+"nn_model.ckpt"  
    self.saver.save(self.sess,pathname)  
    heapq.heappush(self.model_info_heap,info)  
    if len(self.model_info_heap)>MODEL_RECORD_WINDOW:  
        old_info=heapq.heappop(self.model_info_heap)  
        old_dir=NN_INFO_STORE_DIR+MODEL_DIR+str(old_info.epoch)+"/"  
        fp.remove_dir(old_dir)  
```
the model_info.txt under model_data folder records average rewards and entropy   
of 10 (MODEL_RECORD_WINDOW) models.
```
#rl_server/rl_agent.py#L188
model_info_file=open(pathname,"w")  
for i in range(len(self.model_info_heap)):  
    info=self.model_info_heap[i]  
    model_info_file.write(str(info.epoch)+"\t"+str(info.reward)+"\t"+  
                          str(info.entropy)+"\n")  
model_info_file.close()  
```
An example of model_info.txt  
```
epoch   average reward      average entorpy  
54500	1.063393632629108	0.086325034  
45200	1.063659756455399	0.10274112  
37600	1.0636309419014085	0.12221253  
42800	1.0660516578638497	0.109190166  
52500	1.0649296068075116	0.08468006  
51400	1.0690481807511738	0.092922606  
47100	1.0670617077464788	0.1007935  
52600	1.0667941314553988	0.08704215  
46000	1.0661074677230047	0.08989506  
44500	1.0694628814553988	0.096657254  
```
the xxxxx_reward_and_entropy.txt under nn_info folder logs out average rewards and entropy during the traing process. Its meaning can be founded in rl_agent.py.  
```
#rl_server/rl_agent.py#L156  
pathname=NN_INFO_STORE_DIR+str(TimeMillis32())+"_reward_and_entropy.txt"  
self.fp_re=open(pathname,"w")  
#rl_server/rl_agent.py#L237  
def _reward_entropy_mean(self):  
    sz=len(self.reward_entropy_list)  
    rewards=[]  
    entropys=[]  
    for i in range(sz):  
        rewards.append(self.reward_entropy_list[i].reward)  
        entropys.append(self.reward_entropy_list[i].entropy)  
    average_reward=np.mean(rewards)  
    average_entropy=np.mean(entropys)  
    out=str(self.epoch)+"\t"+str(average_reward)+"\t"+str(average_entropy)+"\n"  
    self.fp_re.write(out)  
```
- Test an existing nn model  

create a new folder named load_model under ns-allinone-3.31/ns-3.31/rl_server/ and 
copy an existing model data to it.  
an existing model data can be found under rl_server/nn_info/model_data/xxxxx/. xxxxx is epoch index.  

1 test the model with cooked_test_traces  
```
conda activate tensorflow  
python ns3-dash-train.py --mode test   
```
2 test the model with Oboe_traces  
```
conda activate tensorflow  
python ns3-dash-train.py --mode oboe  
```
## Data Processing
The files under script folder are used for data processing and plotting pic.  
├── cdf_plot.sh  
├── cdf_pro.py  
├── reward-entropy-plot.sh  
└── reward_stat.py  
All the collected data can be found under ns-allinone-3.31/ns-3.31/traces/  
- reward-entropy-plot.sh plots xxxxx_reward_and_entropy.txt  

The results on average reward and entropy:  
![avatar](https://github.com/SoonyangZhang/http-dash-ns3/blob/main/results/reward-entropy.png)  
- Put reward_stat.py and cdf_pro.py to ns-allinone-3.31/ns-3.31/traces/  
```
python reward_stat.py  
python cdf_pro.py  
```
reward_stat.py will write the average reward of each test to ns-allinone-3.31/ns-3.31/traces/process  
cdf_pro.py write of reward cdf with different bandwidth trace.  
## Results
test on cooked_test_traces:  
![avatar](https://github.com/SoonyangZhang/http-dash-ns3/blob/main/results/cook-cdf.png)  
![avatar](https://github.com/SoonyangZhang/http-dash-ns3/blob/main/results/cook2-cdf.png)  
test on Oboe_traces:  
![avatar](https://github.com/SoonyangZhang/http-dash-ns3/blob/main/results/oboe-cdf.png)  
![avatar](https://github.com/SoonyangZhang/http-dash-ns3/blob/main/results/oboe2-cdf.png)  

## Conclusion
Some interesting conclusion can be made accoring to resutls.
The model is trained on cooked_traces. When tested on cooked_test_traces, learning based solution outperforms traditional model based solution.  
But when tested on Oboe_traces, several algorithms can gain better performance than learning based solution when the avergage reward is larger than 2.2.  
The reason behind this phenomenon may be the average rate distribution of Oboe_traces is higher than it in cooked_traces.  

