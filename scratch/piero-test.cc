#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <algorithm>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sys/stat.h> // stat
#include <errno.h>    // errno, ENOENT, EEXIST
#if defined(_WIN32)
#include <direct.h>   // _mkdir
#endif
#include "ns3/core-module.h"
#include "ns3/cardiac-stem-cell-module.h"
#include "ns3/log.h"
using namespace ns3;
namespace ns3{
NS_LOG_COMPONENT_DEFINE("piero-test");
bool compare(const DatasetDescriptation& t1, const DatasetDescriptation & t2) {
    return t1.name<t2.name;
}
}
//https://stackoverflow.com/questions/675039/how-can-i-create-directory-tree-in-c-linux
bool isDirExist(const std::string& path)
{
#if defined(_WIN32)
    struct _stat info;
    if (_stat(path.c_str(), &info) != 0)
    {
        return false;
    }
    return (info.st_mode & _S_IFDIR) != 0;
#else 
    struct stat info;
    if (stat(path.c_str(), &info) != 0)
    {
        return false;
    }
    return (info.st_mode & S_IFDIR) != 0;
#endif
}

bool makePath(const std::string& path)
{
#if defined(_WIN32)
    int ret = _mkdir(path.c_str());
#else
    mode_t mode = 0755;
    int ret = mkdir(path.c_str(), mode);
#endif
    if (ret == 0)
        return true;

    switch (errno)
    {
    case ENOENT:
        // parent didn't exist, try to create it
        {
            int pos = path.find_last_of('/');
            if (pos == std::string::npos)
#if defined(_WIN32)
                pos = path.find_last_of('\\');
            if (pos == std::string::npos)
#endif
                return false;
            if (!makePath( path.substr(0, pos) ))
                return false;
        }
        // now, try to create again
#if defined(_WIN32)
        return 0 == _mkdir(path.c_str());
#else 
        return 0 == mkdir(path.c_str(), mode);
#endif

    case EEXIST:
        // done!
        return isDirExist(path);

    default:
        return false;
    }
}
void getFiles(std::string &cate_dir,std::vector<std::string> &files)
{
    DIR *dir;
    struct dirent *ptr;
    if ((dir=opendir(cate_dir.c_str())) == NULL){
        perror("Open dir error...");
        exit(1);
    }
    
    while ((ptr=readdir(dir)) != NULL)
    {
        if(strcmp(ptr->d_name,".")==0 || strcmp(ptr->d_name,"..")==0)    ///current dir OR parrent dir
                continue;
        else if(ptr->d_type == 8)    ///file
            //printf("d_name:%s/%s\n",basePath,ptr->d_name);
            files.push_back(ptr->d_name);
        else if(ptr->d_type == 10)    ///link file
            //printf("d_name:%s/%s\n",basePath,ptr->d_name);
            continue;
        else if(ptr->d_type == 4)    ///dir
        {
            files.push_back(ptr->d_name);
        }
    }
    closedir(dir);
}
//https://github.com/USC-NSL/Oboe/blob/master/traces/trace_0.txt
void test_algorithm(std::vector<std::string> &video_log,std::vector<double> &average_rate,
                    std::string &group_id,std::string &agent_id,std::string &bid,
                    DatasetDescriptation &des,std::string &algo,std::string &result_folder){
    std::string trace;
    std::string delimit("_");
    char buf[FILENAME_MAX];
    memset(buf,0,FILENAME_MAX);        
    std::string parent=std::string (getcwd(buf, FILENAME_MAX));
    std::string folder=parent+ "/traces/"+result_folder;
    makePath(folder);
    trace=folder+"/"+group_id+delimit+agent_id+delimit+bid+delimit+algo;
    Ptr<PieroBiChannel> channel=CreateObject<PieroBiChannel>();
    Time start=MicroSeconds(10);
    Ptr<PieroDashClient> client=CreateObject<PieroDashClient>(video_log,average_rate,trace,4000,3,channel->GetSocketA(),start);
    client->SetAdaptationAlgorithm(algo,group_id,agent_id);
    StopBroadcast *broadcast=client->GetBroadcast();
    Ptr<PieroDashServer> server=CreateObject<PieroDashServer>(channel->GetSocketB(),broadcast);
    server->SetBandwidthTrace(des,Time(0));
    Simulator::Run ();
    Simulator::Destroy();
}
void test_rl_algorithm(std::vector<std::string> &video_log,std::vector<double> &average_rate,
                    std::string &group_id,std::string &agent_id,std::string &bid,
                      DatasetDescriptation &des,bool log=false){
    std::string result_folder("test");
    std::string algo("reinforce");
    std::string trace;
    std::string delimit("_");
    if(log){
        result_folder=std::string("train");
        char buf[FILENAME_MAX];
        memset(buf,0,FILENAME_MAX);        
        std::string parent=std::string (getcwd(buf, FILENAME_MAX));
        std::string folder=parent+ "/traces/"+result_folder;
        makePath(folder);
        trace=folder+"/"+group_id+delimit+agent_id+delimit+bid+delimit+algo;
    }
    Ptr<PieroBiChannel> channel=CreateObject<PieroBiChannel>();
    Time start=MicroSeconds(10);
    Ptr<PieroDashClient> client=CreateObject<PieroDashClient>(video_log,average_rate,trace,4000,3,channel->GetSocketA(),start);
    g_rl_server_ip="127.0.0.1";
    g_rl_server_port=1234;
    client->SetAdaptationAlgorithm(algo,group_id,agent_id);
    StopBroadcast *broadcast=client->GetBroadcast();
    Ptr<PieroDashServer> server=CreateObject<PieroDashServer>(channel->GetSocketB(),broadcast);
    server->SetBandwidthTrace(des,Time(0));
    Simulator::Run ();
    Simulator::Destroy();    
}
void signal_exit_handler(int sig)
{
    ns3::g_running=false;
}
const char *ns3_server_ip="127.0.0.1";
uint16_t ns3_server_port=3345;
int main(int argc, char *argv[]){
    signal(SIGTERM, signal_exit_handler);
    signal(SIGINT, signal_exit_handler);
    signal(SIGHUP, signal_exit_handler);//ctrl+c
    signal(SIGTSTP, signal_exit_handler);//ctrl+z       
    LogComponentEnable("piero",LOG_LEVEL_ERROR);
    LogComponentEnable("piero_rl",LOG_LEVEL_ERROR);
    LogComponentEnable("piero-test",LOG_LEVEL_INFO);
    CommandLine cmd;
    std::string reinforce("false");
    std::string train("false");
    std::string group_id("0");
    std::string agent_id("0");
    std::string bandwith_id("0");
    std::string bandwidth_trace("cook");
    cmd.AddValue ("rl", "reinfore",reinforce);
    cmd.AddValue ("tr", "train",train);
    cmd.AddValue ("gr", "groupid",group_id);
    cmd.AddValue ("ag", "agentid",agent_id);
    cmd.AddValue ("bwid", "bandwidthid",bandwith_id);
    cmd.AddValue ("bt", "bandwidthtrace",bandwidth_trace);
    cmd.Parse (argc, argv);
    std::string ns3_path="/home/ipcom/zsy/ns-allinone-3.31/ns-3.31/";
    std::string video_path=ns3_path+std::string("video_data/");
    std::string video_name("video_size_");
    int n=6;
    std::vector<std::string> video_log;
    for(int i=0;i<n;i++){
        std::string name=video_path+video_name+std::to_string(i);
        video_log.push_back(name);
    }
    float VIDEO_BIT_RATE[]={300,750,1200,1850,2850,4300}; //kbps
    std::vector<double> average_rate;
    for (int i=0;i<n;i++){
        double bps=VIDEO_BIT_RATE[i]*1000;
        average_rate.push_back(bps);
    }
    std::vector<DatasetDescriptation> bw_traces;
    DatasetDescriptation *dataset_ptr=nullptr;
    size_t dataset_n=0;
    DatasetDescriptation train_dataset[]={
        {ns3_path+std::string("bw_data/cooked_traces/"),
        RateTraceType::TIME_BW,TimeUnit::TIME_S,RateUnit::BW_Mbps},
    };
    DatasetDescriptation test_dataset[]={
        {ns3_path+std::string("bw_data/cooked_test_traces/"),
        RateTraceType::TIME_BW,TimeUnit::TIME_S,RateUnit::BW_Mbps},
    };
    DatasetDescriptation oboe_dataset[]={
        {ns3_path+std::string("bw_data/Oboe_traces/"),
        RateTraceType::TIME_BW,TimeUnit::TIME_S,RateUnit::BW_Mbps},
    };
    if(reinforce.compare("true")==0){
        if(train.compare("true")==0){
            dataset_ptr=train_dataset;
            dataset_n=sizeof(train_dataset)/sizeof(train_dataset[0]);
        }else{
            dataset_ptr=test_dataset;
            dataset_n=sizeof(test_dataset)/sizeof(test_dataset[0]);
            if (bandwidth_trace.compare("oboe")==0){
                dataset_ptr=oboe_dataset;
                dataset_n=sizeof(oboe_dataset)/sizeof(oboe_dataset[0]);                
            }
        }

    }
    for(size_t i=0;i<dataset_n;i++){
        std::vector<std::string> files;
        std::string folder=dataset_ptr[i].name;
        getFiles(folder,files);
        for (size_t j=0;j<files.size();j++){
            std::string path_name=folder+files[j];
            DatasetDescriptation des(path_name,dataset_ptr[i].type,
            dataset_ptr[i].time_unit,dataset_ptr[i].rate_unit);
            bw_traces.push_back(des);
        }
    }
    std::sort(bw_traces.begin(), bw_traces.end(),compare);
    std::string name=ns3_path+std::string("bw_data/Oboe-master/traces/trace_0.txt");
    DatasetDescriptation another_sample(name,RateTraceType::TIME_BW,TimeUnit::TIME_MS,RateUnit::BW_Kbps);
    uint64_t last_time=TimeMillis();
    if(reinforce.compare("true")==0){
        int bid=std::stoi(bandwith_id);
        DatasetDescriptation bandwidth_sample=bw_traces.at(bid%bw_traces.size());
        bool is_train=false;
        if(train.compare("true")==0){
            is_train=true;
        }
        test_rl_algorithm(video_log,average_rate,group_id,agent_id,bandwith_id,bandwidth_sample,is_train);
    }else{
        const char *algo[]={"festive","panda","tobasco","osmp","raahs","fdash","sftm","svaa"};
        int n=sizeof(algo)/sizeof(algo[0]);
        std::string result_folder("tradition");
        if(bandwidth_trace.compare("oboe")==0){
            result_folder=std::string("oboe");
        }
        for(int i=0;i<1/*bw_traces.size()*/;i++){
            DatasetDescriptation bandwidth_sample=bw_traces.at(i);
            auto temp_id=std::to_string(i);
            for(int j=0;j<n;j++){
                std::string algorithm(algo[j]);
                test_algorithm(video_log,average_rate,group_id,agent_id,temp_id,another_sample/*bandwidth_sample*/,
                                algorithm,result_folder);
            }
        }
    }
    uint64_t delta=TimeMillis()-last_time;
    double seconds=1.0*delta/1000;
    if(reinforce.compare("true")==0){
        NS_LOG_INFO(train<<" "<<group_id<<" "<<agent_id<<" "<<bandwith_id<<" "<<seconds);
    }
    return 0;
}
