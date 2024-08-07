// #pragma once
#include <iostream>
#include <workflow/WFGlobal.h>
#include <workflow/WFFacilities.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFHttpServer.h>
#include <workflow/HttpMessage.h>
#include <workflow/Workflow.h>
#include <workflow/WFRedisServer.h>
#include <workflow/RedisMessage.h>
#include <string>
#include <signal.h>
#include <nlohmann/json.hpp>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

using namespace std;
using namespace protocol;
using json = nlohmann::json;

static WFFacilities::WaitGroup waitgroup(1);
void sigHandler(int num)
{
    waitgroup.done();
    fprintf(stderr, "wait group is done\n");
}

void process(WFHttpTask *servertask)
{
    HttpRequest *req = servertask->get_req();
    HttpResponse *resp = servertask->get_resp();
    // 获取请求行信息
    string method = req->get_method();
    string url = req->get_request_uri();
    // 解析url=路径+查询
    string path = url.substr(0, url.find("?"));
    string query = url.substr(url.find("?") + 1);
    // 初始化
    if (method == "POST" && path == "/file/mupload/init")
    {
        // 1.读取请求报文，获取请求报文体
        const void *body;
        size_t size;
        req->get_parsed_body(&body, &size);
        // 2.将报文体转换为json对象
        json mes = json::parse(static_cast<const char *>(body));
        string filename = mes["filename"];
        string filehash = mes["filehash"];
        int filesize = mes["filesize"];
#ifdef DEBUG
        fprintf(stderr, "filename=%s,filehash=%s,filesize=%d\n", filename.c_str(), filehash.c_str(), filesize);
#endif
        // 3.初始化分块信息 uploadID分块
        char buff[20];
        time_t now = time(nullptr);
        strftime(buff, sizeof(buff), "%H:%M", localtime(&now));
        string uploadid = "hzz" + string(buff);
        int chunksize = 1024 * 1024;
        int chunkcount = filesize / chunksize + (filesize % chunksize == 0 ? 0 : 1);
#ifdef DEBUG
        fprintf(stderr, "loadid=%s,chunksize=%d,chunkcount=%d\n", uploadid.c_str(), chunksize, chunkcount);
#endif
        // 4.将数据加载的Redis
        vector<vector<string>> argVec = {
            {uploadid, "chunkcount", to_string(chunkcount)},
            {uploadid, "filehash", filehash},
            {uploadid, "filesize", to_string(filesize)}};
        for (int i = 0; i < 3; i++)
        {
            auto redistask = WFTaskFactory::create_redis_task("redis://127.0.0.1:6379", 2, nullptr);
            RedisRequest *res = redistask->get_req();
            res->set_request("HSET", argVec[i]);
            redistask->start();
        }
        // 5.给客服端返回响应
        json uppartInfo;
        uppartInfo["status"] = "OK";
        uppartInfo["uploadid"] = uploadid;
        uppartInfo["chunksize"] = chunksize;
        uppartInfo["chunkcount"] = chunkcount;
        uppartInfo["filesize"] = filesize;
        uppartInfo["filehash"] = filesize;
        resp->append_output_body(uppartInfo.dump());
    }
    // 上传
    else if (method == "POST" && path == "/file/mupload/uppart")
    {
        // 上传单个分块
        // 1.解析用户请求，提取出uploadid和chkidx
        string uploadidkv = query.substr(0, query.find("&"));
        string chkidxkv = query.substr(query.find("&") + 1);
        string uploadid = uploadidkv.substr(uploadidkv.find("=") + 1);
        string chkidx = chkidxkv.substr(chkidxkv.find("=") + 1);
        // 2.获取文件hash，创建目录，写入分块
        auto redistask = WFTaskFactory::create_redis_task("redis://127.0.0.1:6379", 2, [chkidx, req](WFRedisTask *redistask)
                                                          {
                                                              RedisResponse *resp = redistask->get_resp();
                                                              int state = redistask->get_state();
                                                              int error = redistask->get_error();
                                                              RedisValue value;
                                                              switch ((state))
                                                              {
                                                              case WFT_STATE_SYS_ERROR:
                                                                  fprintf(stderr, "system error:%s\n", strerror(error));
                                                                  break;
                                                              case WFT_STATE_DNS_ERROR:
                                                                  fprintf(stderr, "dns error:%s\n", strerror(error));
                                                                  break;
                                                              case WFT_STATE_SUCCESS:
                                                                  resp->get_result(value);
                                                                  {
                                                                      if (value.is_error())
                                                                      {
                                                                          fprintf(stderr, "redis error\n");
                                                                          state = WFT_STATE_TASK_ERROR;
                                                                      }
                                                                      break;
                                                                  }
                                                              default:
                                                                  break;
                                                              }
                                                              if (state != WFT_STATE_SUCCESS)
                                                              {
                                                                  fprintf(stderr, "failed\n");
                                                                  return;
                                                              }
                                                              string filehash = value.string_value();
                                                              mkdir(filehash.c_str(), 0777);
                                                              string filepath = filehash + "/" + chkidx;
                                                              int fd = open(filepath.c_str(), O_RDWR | O_CREAT | O_APPEND, 0666);
                                                              // 写入报文发送来的文件
                                                              const void *body;
                                                              size_t size;
                                                              req->get_parsed_body(&body, &size);
                                                              write(fd, body, size);
                                                              close(fd); });
        redistask->get_req()->set_request("HGET", {uploadid, "filehash"}); // 设置任务请求
        series_of(servertask)->push_back(redistask);
        // 3.写入分块完后，将进度写入redis
        auto redistask2 = WFTaskFactory::create_redis_task("redis://127.0.0.1:6379", 2, nullptr);
        redistask2->get_req()->set_request("HSET", {uploadid, "chkidx" + chkidx, "1"});
        series_of(servertask)->push_back(redistask2);
        // 4.回复响应
        resp->append_output_body("OK");
    }
    // 合并完成
    else if (method == "POST" && path == "/file/mupload/complete")
    {
        // 合并分块
        // 1.解析用户请求，提取出uploadid
        string uploadid = query.substr(query.find("=") + 1);
        auto redistask = WFTaskFactory::create_redis_task("redis://127.0.0.1:6379", 2, [resp](WFRedisTask *redistask)
                                                          {
                RedisResponse *redisresp=redistask->get_resp();
                RedisValue value;
                redisresp->get_result(value);
                int chunkcount;
                int chunknow=0;
                string filehash;
                //2.根据uploadid查询进度，HGETALL uploadid
                for(int i=0;i<value.arr_size();i+=2)
                {
                    string key=value.arr_at(i).string_value();
                    string val=value.arr_at(i+1).string_value();
// #ifdef DEBUG
//                 cout<<"key:"<<key<<endl;
//                 cout<<"val:"<<val<<endl;
// #endif
                    if(key=="chunkcount")
                    {
                       chunkcount= stoi(val.c_str());
                    }
                    if(key=="filehash")
                    {
                        filehash=val;
                    }
                    if(key.substr(0,6)=="chkidx")//说明已经完成
                    {
                        ++chunknow;
                        //合并
                        string filepath = filehash + "/" + key.substr(6);
                        int index=stoi(key.substr(6));
                        string ALLfilepath = filehash + "/" + "all";
                        int fd1=open(filepath.c_str(),O_RDWR);
                        int fd2=open(ALLfilepath.c_str(),O_RDWR|O_APPEND|O_CREAT,0666);
                        char buff[1024*1024];
                        ssize_t n=read(fd1,buff,sizeof(buff));
                        write(fd2,buff,n);
                        close(fd1);
                        close(fd2);
                    }
                }
#ifdef DEBUG
                cout<<"chunkcount:"<<chunkcount<<endl;
                cout<<"chunknow:"<<chunknow<<endl;
#endif
                //3.chunkcount
                if(chunknow==chunkcount)
                {
                    resp->append_output_body("Success");
                }
                else
                    resp->append_output_body("Failed"); });
        redistask->get_req()->set_request("HGETALL", {uploadid}); // 设置任务请求
        series_of(servertask)->push_back(redistask);

        // 4.chkidx *
    }
}

int main()
{
    signal(SIGINT, sigHandler);
    WFHttpServer server(process);
    if (server.start(8080) == 0)
    {
        waitgroup.wait(); // 确保主进程阻塞
        server.stop();
    }
    else
    {
        fprintf(stderr, "server start fail\n");
    }
}