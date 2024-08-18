#include "Header_file.hpp"
#include "MysqlMode.hpp"
class File
{
public:
    void UpFile(const wfrest::HttpReq *req, wfrest::HttpResp *resp, SeriesWork *series)
    {
        auto userInfo = req->query_list();
        string username = userInfo["username"];
        string token = userInfo["token"];
#ifdef DEBUG
        cout << "uploadname:" << username << "  token:" << token << endl;
#endif
        // token 鉴权
        string Sql = "SELECT user_token  FROM clouddrive.tbl_user_token WHERE user_name = '" + username + "';";
        auto tokentask = WFTaskFactory::create_mysql_task(url, 0, bind(&MysqlMode::GetToken, &mysqlmode_, std::placeholders::_1));
        tokentask->get_req()->set_query(Sql);
        tokentask->user_data = (void *)req;
        UserInfo *userinfo = new UserInfo();
        userinfo->username = username;
        userinfo->token = token;
        userinfo->resp = resp;
        series->push_back(tokentask);
        series->set_context(userinfo);
        series->set_callback([](const SeriesWork *series)
                             { delete static_cast<UserInfo *>(series->get_context()); });
        // 秒传功能

        // 向数据库的tbl_file和tbl_user_file写入上传的文件信息
    }
    void DownFile(const wfrest::HttpReq *req, wfrest::HttpResp *resp, SeriesWork *series)
    {
        auto fileInfo = req->query_list();
        string sha1 = fileInfo["filehash"];

        string sql = "SELECT file_name FROM  clouddrive.tbl_file WHERE file_sha1 = '" + sha1 + "';";
#ifdef DEBUG
        cout << "shal:" << sha1 << endl;
        cout << "downsql:" << sql << endl;
#endif
        auto readtask = WFTaskFactory::create_mysql_task(url, 0, bind(&MysqlMode::GetFileMes, &mysqlmode_, std::placeholders::_1));
        readtask->get_req()->set_query(sql);
        readtask->user_data = resp;
        series->push_back(readtask);
    }

private:
    MysqlMode mysqlmode_;
    string url = "mysql://root:543212345@localhost";
};