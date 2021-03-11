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
2 put piero-test.cc in scratch to ns3-allinone-3.xx/ns-3.xx/scratch/  
3 copy the folder rl_server to  ns3-allinone-3.xx/ns-3.xx/  
4 put ns3_server.py to ns3-allinone-3.xx/ns-3.xx/  
5 create a new folder named traces under ns3-allinone-3.xx/ns-3.xx/  
6 change the video log data path and bandwidth piero-test.cc   
```
DatasetDescriptation dataset[]{  
    {std::string("/home/zsy/ns-allinone-3.31/ns-3.31/bw_data/cooked_traces/"),  
    RateTraceType::TIME_BW,TimeUnit::TIME_S,RateUnit::BW_Mbps},  
};  
std::string video_path("/home/zsy/ns-allinone-3.31/ns-3.31/video_data/");  
std::string video_name("video_size_");  
std::string name="/home/zsy/ns-allinone-3.31/ns-3.31/bw_data/trace_0.txt";  
```
7 cooked_traces is downloaded from Pensieve-PPO.  
8 Rebuid ns3.  
## Run  
Test the above eight algorithms:    
```
cd ns3-allinone-3.xx/ns-3.xx  
sudo su  
./waf --run "scratch/piero-test --rl=false"
```
Train a reinforce learning algorithm:  
1 start ns3_server  
```
cd ns3-allinone-3.xx/ns-3.xx  
sudo su  
python ns3_server.py  
```
2 start rl server:  
```
cd ns3-allinone-3.xx/ns-3.xx/rl_server  
python tcp_epoll_server  
```
## Results
![avatar](https://github.com/SoonyangZhang/http-dash-ns3/blob/main/results/reward1-cdf.tiff)  



