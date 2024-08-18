#pragma once
#include <iostream>
#include <signal.h>
#include <nlohmann/json.hpp>
#include <workflow/Workflow.h>
#include <workflow/WFFacilities.h>
#include <workflow/WFGlobal.h>
#include <workflow/WFHttpServer.h>
#include <workflow/MySQLResult.h>
#include <workflow/HttpMessage.h>
#include <workflow/RedisMessage.h>
#include <workflow/WFTaskFactory.h>
#include <wfrest/HttpServer.h>
#include <wfrest/json.hpp>
#include <wfrest/MysqlUtil.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <memory>
#include <cstring>
#include <crypt.h>
#include "FileUtil.hpp"

using namespace protocol;
using namespace std;
using Json = nlohmann::json;

static WFFacilities::WaitGroup waitgroup(1);

void sighandler(int num)
{
    waitgroup.done();
    cout << "wait group is done" << endl;
}

void callback(WFMySQLTask *mysqltask)
{
    wfrest::HttpResp *clientresp = static_cast<wfrest::HttpResp *>(mysqltask->user_data);
    // 1.先判断是否进入数据库成功
    if (mysqltask->get_state() != WFT_STATE_SUCCESS)
    {
        fprintf(stderr, "error:msg:%s\n", WFGlobal::get_error_string(mysqltask->get_state(), mysqltask->get_error()));
        clientresp->append_output_body("FALT", 4);
    }

    // 2.检查语法错误
    MySQLResponse *resp = mysqltask->get_resp();
    MySQLResultCursor cursor(resp);
    if (resp->get_packet_type() == MYSQL_PACKET_ERROR)
    {
        fprintf(stderr, "error_code=%d msg=%s\n", resp->get_error_code(), resp->get_error_msg().c_str());
        clientresp->append_output_body("FALT", 4);
    }
    // 3.检验读写任务
    if (cursor.get_cursor_status() == MYSQL_STATUS_OK)
    {
        fprintf(stderr, "OK. %llu rows affected. %d warnings. insert id=%lld\n", cursor.get_affected_rows(), cursor.get_warnings(), cursor.get_insert_id());
        clientresp->append_output_body("SUCCESS", 7);
    }
}
static string url = "mysql://root:543212345@localhost";
int main()
{
    signal(SIGINT, sighandler);
    wfrest::HttpServer server;
    // 得到注册页面
    server.GET("/user/signup", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
               { resp->File("../public/sign.html"); });
    // 按完click to signup后后端执行注册
    server.POST("/user/signup", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp, SeriesWork *series)
                {
                    // 1.按urlencoded的形式去解析报体数据
                    map<string, string> &form_kv = req->form_kv();
                    string username = form_kv["username"];
                    string password = form_kv["password"];
                    // 2.对密码进行加密
                    string salt = "12345678";
                    char *encryptPassword = crypt(password.c_str(), salt.c_str());
                    // 3.把用户信息写进数据库中
                    string sql = "INSERT INTO clouddrive.tbl_user(user_name, user_pwd) VALUES('" + username + "','" + encryptPassword + "');";
                    auto mysqltask = WFTaskFactory::create_mysql_task("mysql://root:543212345@localhost", 0, callback);
                    mysqltask->get_req()->set_query(sql);
                    mysqltask->user_data = resp;
                    series->push_back(mysqltask); });
    // 注册成功后前端代码会自动发送下面请求得到登入页面
    server.GET("/view/signin.html", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
               { resp->File("../public/view/signin.html"); });
    server.GET("/view/home.html", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
               { resp->File("../public/view/home.html"); });
    // 操作登入页面后进行后端进行登入操作
    server.POST("/user/signin", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp, SeriesWork *series)
                {
        //1.按urlencoded的形式去解析报体数据
        map<string,string> &form_kv=req->form_kv();
        string username=form_kv["username"];
        string password=form_kv["password"];
        //2.查询数据库
        
        string sql="select user_pwd from clouddrive.tbl_user WHERE user_name='"+username+"' LIMIT 1;";
        auto readtask=WFTaskFactory::create_mysql_task(url,0,[](WFMySQLTask* readtask){
            auto resp=readtask->get_resp();
            MySQLResultCursor cursor(resp);
            vector<vector<MySQLCell>> rows;
            cursor.fetch_all(rows);
            string reallypassword=rows[0][0].as_string();
            UserInfo* userinfo=static_cast<UserInfo*>(series_of(readtask)->get_context());
            char* nowpassword=crypt(userinfo->password.c_str(),"12345678");
#ifdef DEBUG
            cout<<"nowpassword:"<<nowpassword<<endl;
#endif
            if(strcmp(reallypassword.c_str(),nowpassword)!=0)
            {
                userinfo->resp->append_output_body("FALT",4);
                return;
            }
            //3.生成一个token，并写入数据库
            Token usertoken(userinfo->username,"12345678");
#ifdef DEBUG
            cout<<"token:"<<usertoken.token<<endl;
#endif
            userinfo->token=usertoken.token;
            string sql="REPLACE INTO clouddrive.tbl_user_token (user_name,user_token) VALUES ('"+userinfo->username+"','"
                            +usertoken.token+"');";
            auto writetask= WFTaskFactory::create_mysql_task(url,0,[userinfo](WFMySQLTask* writetask){
                Json uinfo;
                uinfo["Username"] = userinfo->username;
                uinfo["Token"] = userinfo->token;
                uinfo["Location"] = "/view/home.html";
                cout<<"uinfo:"<<uinfo["Username"]<<endl;
                Json respInfo;
                respInfo["code"] = 0;
                respInfo["msg"] = "OK";
                respInfo["data"] = uinfo;
                userinfo->resp->String(respInfo.dump());

            });
            writetask->get_req()->set_query(sql);
            series_of(readtask)->push_back(writetask);

        });
        readtask->get_req()->set_query(sql);
        series->push_back(readtask);
        UserInfo* userinfo=new UserInfo;
        userinfo->username=username;
        userinfo->password=password;
        userinfo->resp=resp;
        series->set_context(userinfo);
        series->set_callback([userinfo](const SeriesWork *series_work){
                delete userinfo;
        }); });

    server.POST("/user/info", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp, SeriesWork *series)
                {
       auto userInfo=req->query_list();
        //2.检验token是否合法
        //3.查询数据库
        cout<<"userinfo[userinfo]:"<<userInfo["username"]<<endl;
        string sql="SELECT user_name,signup_at FROM clouddrive.tbl_user WHERE user_name='"+userInfo["username"]+"' LIMIT 1";
        auto readtask=WFTaskFactory::create_mysql_task(url,0,[resp](WFMySQLTask* readtask){
            if (readtask->get_state() != WFT_STATE_SUCCESS) 
            {
                resp->String("Database connection failed: " + to_string(readtask->get_error()));
                return;
            }
            auto resp2=readtask->get_resp();
            MySQLResultCursor cursor(resp2);
            vector<vector<MySQLCell>> rows;
            cursor.fetch_all(rows);
            Json uinfo;
            uinfo["Username"]=rows[0][0].as_string();
            uinfo["SignupAt"]=rows[0][1].as_string();
            Json respinfo;
            respinfo["data"]=uinfo;
            respinfo["code"]=0;
            respinfo["msg"]="OK";
            resp->String(respinfo.dump());

        });
        readtask->get_req()->set_query(sql);
        series->push_back(readtask); });
    server.POST("/file/query", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp, SeriesWork *series)
                {
        auto userinfo=req->query_list();
        string username=userinfo["username"];
#ifdef DEBUG
        cout<<"query_username:"<<username<<endl;
#endif
        auto form_kv=req->form_kv();
        string limit=form_kv["limit"];
        //根据用户名查找tbl_user_file
       string sql = "SELECT file_sha1, file_name, file_size, upload_at, last_update FROM clouddrive.tbl_user_file WHERE user_name='" 
                    + username + "' LIMIT " + limit + ";";
        cout<<"sql:"<<sql<<endl;
        auto readtask=WFTaskFactory::create_mysql_task(url,0,[resp](WFMySQLTask* readtask){
              if (readtask->get_state() != WFT_STATE_SUCCESS) {
        resp->String("Database connection failed: " + std::to_string(readtask->get_error()));
        return;
    }
    auto respmysql = readtask->get_resp();
    MySQLResultCursor cursor(respmysql);
    vector<vector<MySQLCell>> rows;
    cursor.fetch_all(rows);

    if (rows.empty()) {
        cout << "No data fetched" << endl;      
        return;
    }

    Json resparr;
    for(auto &row : rows) {
        Json fileJson;
        fileJson["FileHash"] = row[0].as_string();
        fileJson["FileName"] = row[1].as_string();
        fileJson["FileSize"] = row[2].as_ulonglong();
        fileJson["UploadAt"] = row[3].as_datetime();
        fileJson["LastUpdated"] = row[4].as_datetime();
        resparr.push_back(fileJson);
    }
    resp->String(resparr.dump());
        });
        readtask->get_req()->set_query(sql);
        series->push_back(readtask); });
    // 得到上传功能页面
    server.GET("/file/upload", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
               { resp->File("../public/function.html"); });
    // 上传功能
    server.POST("/file/upload", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp,SeriesWork* series)
                {
        auto userinfo=req->query_list();
        string username=userinfo["username"];
        cout<<"uploadname:"<<username<<endl;
        using Form=map<string,pair<string,string>>;
        Form &form=req->form();
        pair<string,string> fileinfo=form["file"];
        // map<string,string> &form_kv=req->form_kv();
        // fileinfo.first=form_kv["filename"];
        // fileinfo.second=form_kv["filevalue"];
        string filepath ="../tmp/"+fileinfo.first;
        cout<<filepath<<endl;
        int fd=open(filepath.c_str(),O_RDWR|O_CREAT|O_EXCL,0666);
        if(fd<0)
        {
            resp->set_status_code("500");
            return;
        }
        int ret =write(fd,fileinfo.second.c_str(),fileinfo.second.size());
        close(fd);
        string sha1=FileUtil::sha1File(filepath);
        std::string sql = "INSERT INTO clouddrive.tbl_file (file_sha1, file_name, file_size, file_addr, status) VALUES ('"
                            + sha1 + "','"
                            + fileinfo.first + "',"
                            + std::to_string(fileinfo.second.size()) + ",'"
                            + filepath + "', 0);";
        sql+="INSERT INTO clouddrive.tbl_user_file (user_name,file_sha1,file_name,file_size) VALUES ('"
            +username+"','"
            +sha1 + "','"
            + fileinfo.first + "',"
            + std::to_string(fileinfo.second.size())+");";
        auto mysqltask=WFTaskFactory::create_mysql_task(url,0,[ret,resp](WFMySQLTask* mysqltask){
            if(ret>0)
            {
                resp->headers["Location"]="/view/home.html";
                resp->set_status_code("302"); // 设置重定向状态码
            }
        });
        mysqltask->get_req()->set_query(sql);
        series->push_back(mysqltask);
         });
    // 上传成功后得到的页面
    server.GET("/file/upload/success", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
               { resp->String("SUCCESS"); });
    // 下载功能
    server.GET("/file/download", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
               {
                   map<string, string> fileinfo = req->query_list();
                   string filename = fileinfo["filename"];
                   int filesize = stoi(fileinfo["filesize"]);
                //    string filepath = "tmp/" + filename;
                //    cout << filepath << endl;
                //    int fd = open(filepath.c_str(), O_RDONLY);
                //    if (fd < 0)
                //    {
                //        resp->set_status_code("404");
                //        resp->String("File not found");
                //        return;
                //    }
                //    int size = lseek(fd, 0, SEEK_END);
                //    lseek(fd, 0, SEEK_SET);
                //    unique_ptr<char[]> buff(new char[size]);
                //    ssize_t n = read(fd, buff.get(), size);
                //    close(fd);
                //    if (n < 0)
                //    {
                //        resp->set_status_code("500");
                //        resp->String("Error reading file");
                //        return;
                //    }

                //    resp->append_output_body(buff.get(), size);
                //    resp->headers["Content-Type"] = "application/octet-stream";
                //    resp->headers["Content-Disposition"] = "attachment; filename=\"" + filename + "\"";

                   //使用nginx
                   resp->set_status_code("302");
                   resp->headers["Location"]="http://192.168.88.129:80/"+filename; });
    server.POST("/file/downloadurl",[](const wfrest::HttpReq *req, wfrest::HttpResp *resp,SeriesWork* series){
        auto fileInfo=req->query_list();
        string sha1=fileInfo["filehash"];
        cout<<"shal:"<<sha1<<endl;
        string sql="SELECT file_name FROM  clouddrive.tbl_file WHERE file_sha1 = '"+sha1+"';";
        cout<<"downsql:"<<sql<<endl;
        auto mysqltask=WFTaskFactory::create_mysql_task(url,0,[resp](WFMySQLTask* mysqltask){
            auto respmysql =mysqltask->get_resp();
            MySQLResultCursor cursor(respmysql);
            vector<vector<MySQLCell>> rows;
            cursor.fetch_all(rows);

            if (rows.empty()) {
                cout << "No data fetched" << endl;      
                return;
            }
            cout<<"file_name:"<<rows[0][0].as_string()<<endl;
            cout<<"http://192.168.88.129:80/"+rows[0][0].as_string()<<endl;
            resp->String("http://192.168.88.129:80/"+rows[0][0].as_string());
            
        });
        mysqltask->get_req()->set_query(sql);
        series->push_back(mysqltask);
    });
    if (server.track().start(8080) == 0)
    {
        waitgroup.wait();
        server.stop();
    }
    else
    {
        fprintf(stderr, "server start fail\n");
    }
}