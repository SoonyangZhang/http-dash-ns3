#include <unistd.h>
#include <dirent.h>
#include <algorithm>
#include <string>
#include <vector>
#include <iostream>
#include <sys/stat.h> // stat
#include <errno.h>    // errno, ENOENT, EEXIST
#if defined(_WIN32)
#include <direct.h>   // _mkdir
#endif
#include "ns3/core-module.h"
#include "ns3/cardiac-stem-cell-module.h"
using namespace ns3;
namespace ns3{
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
void test_algorithm(std::string &concurrent_id,std::string &agent_id,
                    DatasetDescriptation &des,std::string &algo,std::string &result_folder){
    std::string video_path("/home/zsy/ns-allinone-3.31/ns-3.31/video_data/");
    std::string video_name("video_size_");
    std::string trace;
    std::string delimit("_");
    char buf[FILENAME_MAX];
    memset(buf,0,FILENAME_MAX);        
    std::string parent=std::string (getcwd(buf, FILENAME_MAX));
    std::string folder=parent+ "/traces/"+result_folder;
    makePath(folder);
    trace=folder+"/"+concurrent_id+delimit+agent_id+delimit+algo;
    int n=6;
    std::vector<std::string> video_log;
    for(int i=0;i<n;i++){
        std::string name=video_path+video_name+std::to_string(i);
        video_log.push_back(name);
    }
    Ptr<PieroBiChannel> channel=CreateObject<PieroBiChannel>();
    Time start=MicroSeconds(10);
    Ptr<PieroDashClient> client=CreateObject<PieroDashClient>(video_log,trace,4000,3,channel->GetSocketA(),start);
    client->SetAdaptationAlgorithm(agent_id,algo);
    StopBroadcast *broadcast=client->GetBroadcast();
    Ptr<PieroDashServer> server=CreateObject<PieroDashServer>(channel->GetSocketB(),broadcast);
    server->SetBandwidthTrace(des,Time(0));
    Simulator::Run ();
    Simulator::Destroy();
}
void test_rl_algorithm(std::string &concurrent_id,std::string &agent_id,
                      DatasetDescriptation &des,bool train=false){
    std::string result_folder("test");
    std::string algo("reinforce");
    std::string video_path("/home/zsy/ns-allinone-3.31/ns-3.31/video_data/");
    std::string video_name("video_size_");
    std::string trace;
    std::string delimit("_");
    bool log=true;
    if(train){
        result_folder=std::string("train");
        log=true;
    }
    if(log){
        char buf[FILENAME_MAX];
        memset(buf,0,FILENAME_MAX);        
        std::string parent=std::string (getcwd(buf, FILENAME_MAX));
        std::string folder=parent+ "/traces/"+result_folder;
        makePath(folder);
        trace=folder+"/"+concurrent_id+delimit+agent_id+delimit+algo;
    }
    int n=6;
    std::vector<std::string> video_log;
    for(int i=0;i<n;i++){
        std::string name=video_path+video_name+std::to_string(i);
        video_log.push_back(name);
    }
    Ptr<PieroBiChannel> channel=CreateObject<PieroBiChannel>();
    Time start=MicroSeconds(10);
    Ptr<PieroDashClient> client=CreateObject<PieroDashClient>(video_log,trace,4000,3,channel->GetSocketA(),start);
    g_rl_server_ip="127.0.0.1";
    g_rl_server_port=1234;
    client->SetAdaptationAlgorithm(agent_id,algo);
    StopBroadcast *broadcast=client->GetBroadcast();
    Ptr<PieroDashServer> server=CreateObject<PieroDashServer>(channel->GetSocketB(),broadcast);
    server->SetBandwidthTrace(des,Time(0));
    Simulator::Run ();
    Simulator::Destroy();    
}
int main(int argc, char *argv[]){
    LogComponentEnable("piero",LOG_LEVEL_INFO);
    CommandLine cmd;
    std::string reinforce("false");
    std::string train("false");
    std::string concurrent_id("0");
    std::string agent_id("0");
    cmd.AddValue ("rl", "reinfore",reinforce);
    cmd.AddValue ("tr", "train",train);
    cmd.AddValue ("cu", "concurrent",concurrent_id);
    cmd.AddValue ("ag", "agentid",agent_id);
    cmd.Parse (argc, argv);
    DatasetDescriptation dataset[]{
        {std::string("/home/zsy/ns-allinone-3.31/ns-3.31/bw_data/cooked_traces/"),
        RateTraceType::TIME_BW,TimeUnit::TIME_S,RateUnit::BW_Mbps},
    };
    std::vector<DatasetDescriptation> bw_traces;
    for(size_t i=0;i<sizeof(dataset)/sizeof(dataset[0]);i++){
        std::vector<std::string> files;
        std::string folder=dataset[i].name;
        getFiles(folder,files);
        for (size_t j=0;j<files.size();j++){
            std::string path_name=folder+files[j];
            DatasetDescriptation des(path_name,dataset[i].type,
            dataset[i].time_unit,dataset[i].rate_unit);
            bw_traces.push_back(des);
        }
    }
    std::sort(bw_traces.begin(), bw_traces.end(),compare); 
    std::cout<<"dataset: "<<bw_traces.size()<<std::endl;
    std::string name="/home/zsy/ns-allinone-3.31/ns-3.31/bw_data/trace_0.txt";
    DatasetDescriptation another_sample(name,RateTraceType::TIME_BW,TimeUnit::TIME_MS,RateUnit::BW_Kbps);
    if(reinforce.compare("true")==0){
        bool is_train=false;
        if(train.compare("true")==0){
            is_train=true;
        }
        test_rl_algorithm(concurrent_id,agent_id,another_sample,is_train);
    }else{
        const char *algo[]={"festive","panda","tobasco","osmp","raahs","fdash","sftm","svaa"};
        int n=sizeof(algo)/sizeof(algo[0]);
        std::string result_folder("tradition");
        for(int i=0;i<n;i++){
            std::string algorithm(algo[i]);
            test_algorithm(concurrent_id,agent_id,another_sample,
                            algorithm,result_folder);
            std::cout<<i<<std::endl;
        }
    }
    return 0;
}
