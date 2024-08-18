#pragma once
#include "Header_file.hpp"
#include "MysqlMode.hpp"

class GetUser
{
public:
    GetUser() {}
    void signup(const wfrest::HttpReq *req, wfrest::HttpResp *resp, SeriesWork *series)
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
        auto mysqltask = WFTaskFactory::create_mysql_task(url, 0, bind(&MysqlMode::WriteUser, &mysqlmode_, _1));
        mysqltask->get_req()->set_query(sql);
        mysqltask->user_data = resp;
        series->push_back(mysqltask);
    }
    void login(const wfrest::HttpReq *req, wfrest::HttpResp *resp, SeriesWork *series)
    {
        // 1.按urlencoded的形式去解析报体数据
        map<string, string> &form_kv = req->form_kv();
        string username = form_kv["username"];
        string password = form_kv["password"];
        // 2.查询数据库
        string sql = "select user_pwd from clouddrive.tbl_user WHERE user_name='" + username + "' LIMIT 1;";
        auto readtask = WFTaskFactory::create_mysql_task(url, 0, bind(&MysqlMode::FindUser, &mysqlmode_, _1));
        readtask->get_req()->set_query(sql);
        series->push_back(readtask);
        UserInfo *userinfo = new UserInfo;
        userinfo->username = username;
        userinfo->password = password;
        userinfo->resp = resp;
        series->set_context(userinfo);
        series->set_callback([userinfo](const SeriesWork *series)
                             {  delete static_cast<UserInfo*>(series->get_context());});
    }
    void UserState(const wfrest::HttpReq *req, wfrest::HttpResp *resp, SeriesWork *series)
    {
        // 1.获取请求参数
        auto userInfo = req->query_list();
        string username = userInfo["username"];
        // 2.检验token是否合法,-->快速鉴权
        string token=userInfo["token"];
        if(token.size()!=40)
        {
                resp->set_status_code("404");
                return;
        }
#ifdef DEBUG
        cout << "user_info_username:" << userInfo["username"] << " user_info_token: " << token << endl;
#endif
       

        // 3.查询数据库
        string sql = "SELECT user_name,signup_at FROM clouddrive.tbl_user WHERE user_name='" + userInfo["username"] + "' LIMIT 1";
        auto readtask = WFTaskFactory::create_mysql_task(url, 0, bind(&MysqlMode::GetUserState, &mysqlmode_, _1));
        readtask->get_req()->set_query(sql);
        readtask->user_data = resp;
        series->push_back(readtask);
    }
    void FileList(const wfrest::HttpReq *req, wfrest::HttpResp *resp, SeriesWork *series)
    {
        // 1.获取请求参数
        auto userInfo = req->query_list();
        string username = userInfo["username"];
#ifdef DEBUG
        cout << "user_info:" << userInfo["username"] << " user_info_token " << userInfo["token"] << endl;
#endif
        // 2.检验token是否合法
        string token=userInfo["token"];
        if(token.size()!=40)
        {
                resp->set_status_code("404");
                return;
        }
        auto form_kv = req->form_kv();
        string limit = form_kv["limit"];
        // 根据用户名查找tbl_user_file
        string sql = "SELECT file_sha1, file_name, file_size, upload_at, last_update FROM clouddrive.tbl_user_file WHERE user_name='" + username + "' LIMIT " + limit + ";";
#ifdef DEBUG
        cout << "sql:" << sql << endl;
#endif
        // 3.查询数据库
        auto readtask = WFTaskFactory::create_mysql_task(url, 0, bind(&MysqlMode::GetFileList, &mysqlmode_, _1));
        readtask->get_req()->set_query(sql);
        readtask->user_data = resp;
        series->push_back(readtask);
    }

private:
    MysqlMode mysqlmode_;
    std::string url = "mysql://root:543212345@localhost";
};