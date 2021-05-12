#include <chrono>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h> // stat
#include <iostream>
#include <fstream>
#include <string.h>
#include "piero_misc.h"
namespace ns3{
static bool piero_has_digit(std::string & line){
    bool ret=false;
    for(size_t i=0;i<line.size();i++){
        if(isdigit(line[i])){
            ret=true;
            break;
        }
    }
    return ret;
}
//https://stackoverflow.com/questions/675039/how-can-i-create-directory-tree-in-c-linux
static bool IsDirExist(const std::string& path)
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
void ReadSegmentFromFile(std::vector<std::string>& video_log,struct VideoData &video_data){
    for(auto it=video_log.begin();it!=video_log.end();it++){
        const char *name=it->c_str();
        SegmentSize segments;
        std::ifstream file;
        file.open(name);
        if(!file.is_open()){
            std::cout<<"video file path is not right"<<std::endl;
            return;
        }
        int counter=0;
        int64_t sum_bytes=0;
        while(!file.eof()){
            std::string buffer;
            getline(file,buffer);
            if(piero_has_digit(buffer)){
                int size=std::atoi(buffer.c_str());
                segments.push_back(size);
                counter++;
                sum_bytes+=size;
            }
        }
        double average_bitrate=0.0;
        int64_t ms=video_data.segmentDuration;
        if(ms>0&&counter>0){
            average_bitrate=sum_bytes*1000*8*1.0/(ms*counter);
        }
        file.close();
        video_data.representation.push_back(segments);
        video_data.averageBitrate.push_back(average_bitrate);
    }
}
bool MakePath(const std::string& path)
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
            if (!MakePath( path.substr(0, pos) ))
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
        return IsDirExist(path);

    default:
        return false;
    }
}
void GetFiles(std::string &cate_dir,std::vector<std::string> &files)
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
inline int64_t WallTimeNowInUsec(){
    std::chrono::system_clock::duration d = std::chrono::system_clock::now().time_since_epoch();    
    std::chrono::microseconds mic = std::chrono::duration_cast<std::chrono::microseconds>(d);
    return mic.count(); 
}
int64_t PieroTimeMillis(){
    return WallTimeNowInUsec()/1000;
}
int CountFileLines(std::string &name){
    FILE *fp=fopen(name.c_str(), "r");
    char c;
    int lines=0;
    if(!fp){
        return lines;
    }
    for (c = getc(fp); c != EOF; c = getc(fp)){
        if (c == '\n'){
            lines = lines + 1;
        }
    }
    fclose(fp);
    return lines;
}
void BufferSplit(std::string &line,std::vector<std::string>&numbers){
    int n=line.size();
    int start=-1;
    int stop=-1;
    for(int i=0;i<n;i++){
        bool success=isdigit(line[i])||(line[i]=='.');
        if(start==-1&&success){
            start=i;
        }
        if((start!=-1)&&(!success)){
            stop=i;
        }
        if(i==n-1&&success){
            stop=i;
        }
        if(start>=0&&stop>=start){
            std::string one=line.substr(start,stop);
            numbers.push_back(one);
            start=-1;
            stop=-1;
        }
    }
}
}
