#pragma once
#include "Header_file.hpp"
#include "FileUtil.hpp"

class MysqlMode
{
public:
    MysqlMode() {};
    void CheckOut(WFMySQLTask *task, MySQLResponse *resp)
    {
        wfrest::HttpResp *clientresp = static_cast<wfrest::HttpResp *>(task->user_data);
        // 1.先判断是否进入数据库成功
        if (task->get_state() != WFT_STATE_SUCCESS)
        {
            fprintf(stderr, "error:msg:%s\n", WFGlobal::get_error_string(task->get_state(), task->get_error()));
            clientresp->append_output_body("FALT", 4);
        }
        // 2.检查语法错误

        if (resp->get_packet_type() == MYSQL_PACKET_ERROR)
        {
            fprintf(stderr, "error_code=%d msg=%s\n", resp->get_error_code(), resp->get_error_msg().c_str());
            clientresp->append_output_body("FALT", 4);
        }
    }

    void WriteUser(WFMySQLTask *writetask)
    {
        wfrest::HttpResp *clientresp = static_cast<wfrest::HttpResp *>(writetask->user_data);
        MySQLResponse *resp = writetask->get_resp();
        MySQLResultCursor cursor(resp);
        CheckOut(writetask, resp);

        // 检验读写任务
        if (cursor.get_cursor_status() == MYSQL_STATUS_OK)
        {
            fprintf(stderr, "OK. %llu rows affected. %d warnings. insert id=%lld\n", cursor.get_affected_rows(), cursor.get_warnings(), cursor.get_insert_id());
            clientresp->append_output_body("SUCCESS", 7);
        }
    }
    void WriteToken(WFMySQLTask *writetask)
    {
        UserInfo *userinfo = static_cast<UserInfo *>(writetask->user_data);
        MySQLResponse *resp = writetask->get_resp();
        MySQLResultCursor cursor(resp);
        CheckOut(writetask, resp);
        Json uinfo;
        uinfo["Username"] = userinfo->username;
        uinfo["Token"] = userinfo->token;
        uinfo["Location"] = "/view/home.html";
#ifdef DEBUG
        cout << "Username:" << uinfo["Username"] << "  Token:" << uinfo["Token"] << endl;
#endif
        Json respInfo;
        respInfo["code"] = 0;
        respInfo["msg"] = "OK";
        respInfo["data"] = uinfo;
        userinfo->resp->String(respInfo.dump());
    }
    void FindUser(WFMySQLTask *readtask)
    {
        auto resp = readtask->get_resp();
        MySQLResultCursor cursor(resp);
        CheckOut(readtask, resp);
        vector<vector<MySQLCell>> rows;
        cursor.fetch_all(rows);
        if (rows.empty())
        {
            cout << "No data fetched" << endl;
            return;
        }
        string reallypassword = rows[0][0].as_string();
        UserInfo *userinfo = static_cast<UserInfo *>(series_of(readtask)->get_context());
        char *nowpassword = crypt(userinfo->password.c_str(), "12345678");
#ifdef DEBUG
        cout << "nowpassword:" << nowpassword << endl;
#endif
        if (strcmp(reallypassword.c_str(), nowpassword) != 0)
        {
            userinfo->resp->append_output_body("FALT", 4);
            return;
        }
        // 3.生成一个token，并写入数据库
        Token usertoken(userinfo->username, "12345678");
#ifdef DEBUG
        cout << "token:" << usertoken.token << endl;
#endif
        userinfo->token = usertoken.token;
        string sql = "REPLACE INTO clouddrive.tbl_user_token (user_name,user_token) VALUES ('" + userinfo->username + "','" + usertoken.token + "');";
        auto writetask = WFTaskFactory::create_mysql_task(url, 0, bind(&MysqlMode::WriteToken, this, _1));
        writetask->user_data = userinfo;
        writetask->get_req()->set_query(sql);
        series_of(readtask)->push_back(writetask);
    }

    bool ReadCheck(WFMySQLTask *readtask, vector<vector<MySQLCell>> &rows)
    {
        wfrest::HttpResp *clientresp = static_cast<wfrest::HttpResp *>(readtask->user_data);
        auto resp2 = readtask->get_resp();
        MySQLResultCursor cursor(resp2);
        CheckOut(readtask, resp2);
        cursor.fetch_all(rows);
        if (rows.empty())
        {
            cout << "No data fetched" << endl;
            Json error;
            error["code"] = "-1";
            error["mssg"] = "FAIL";
            clientresp->String(error.dump());
            return false;
        }
        return true;
    }
    void SetFileMes(WFMySQLTask *writetask)
    {
        UserInfo *userinfo = static_cast<UserInfo *>(series_of(writetask)->get_context());
        MySQLResponse *resp = writetask->get_resp();
        MySQLResultCursor cursor(resp);
        CheckOut(writetask, resp);

        // 检验读写任务
        if (cursor.get_cursor_status() == MYSQL_STATUS_OK)
        {
            fprintf(stderr, "OK. %llu rows affected. %d warnings. insert id=%lld\n", cursor.get_affected_rows(), cursor.get_warnings(), cursor.get_insert_id());
            userinfo->resp->headers["Location"] = "/view/home.html";
            userinfo->resp->set_status_code("302"); // 设置重定向状态码
        }
    }
    void GetFilInfo(WFMySQLTask *readtask)
    {
        const wfrest::HttpReq *req = static_cast<const wfrest::HttpReq *>(readtask->user_data);
        UserInfo *userinfo = static_cast<UserInfo *>(series_of(readtask)->get_context());
        auto resp = readtask->get_resp();
        MySQLResultCursor cursor(resp);
        CheckOut(readtask, resp);
        vector<vector<MySQLCell>> rows;
        cursor.fetch_all(rows);
        string filename;
        int filesize;
        string username=userinfo->username;
        if (rows.empty())
        {
            using Form = map<string, pair<string, string>>;
            Form &form = req->form();
            pair<string, string> fileInfo = form["file"];
            filename = fileInfo.first;
            filesize = fileInfo.second.size();
#ifdef DEBUG
            cout << "upfilename:" << filename << " upfilesize:" << filesize << endl;
#endif

            int fd = open(("../tmp/" + filename).c_str(), O_RDWR | O_CREAT | O_EXCL, 0666);
            if (fd < 0)
            {   userinfo->resp->set_status_code("500");
                return;
            }
            int ret = write(fd, fileInfo.second.c_str(), filesize);
            close(fd);
            string filepath = "../tmp/" + filename;
            string sha1 = FileUtil::sha1File(filepath);
            std::string sql = "INSERT INTO clouddrive.tbl_file (file_sha1, file_name, file_size, file_addr, status) VALUES ('" + sha1 + "','" 
                            + filename + "'," 
                            + std::to_string(filesize) + ",'" 
                            + filepath + "', 0);";
            auto writetask = WFTaskFactory::create_mysql_task(url, 0, nullptr);
            writetask->get_req()->set_query(sql);
            series_of(readtask)->push_back(writetask);
        }
        else
        {
            filename= rows[0][0].as_string();
            filesize = rows[0][1].as_ulonglong();
        }
        string filepath = "../tmp/" + filename;
        string sha1 = FileUtil::sha1File(filepath);
        string sql = "INSERT INTO clouddrive.tbl_user_file (user_name,file_sha1,file_name,file_size) VALUES ('" + username + "','" + sha1 + "','" 
                + filename + "'," + std::to_string(filesize) + ");";
        auto writetask = WFTaskFactory::create_mysql_task(url, 0, bind(&MysqlMode::SetFileMes, this, std::placeholders::_1));
        writetask->get_req()->set_query(sql);
        series_of(readtask)->push_back(writetask);
    }
    void GetToken(WFMySQLTask *readtask)
    {
        UserInfo *userinfo = static_cast<UserInfo *>(series_of(readtask)->get_context());
        const wfrest::HttpReq *req = static_cast<const wfrest::HttpReq *>(readtask->user_data);
        vector<vector<MySQLCell>> rows;
        ReadCheck(readtask, rows);
        if (!ReadCheck(readtask, rows))
            return;
        string gettoken = rows[0][0].as_string();
        if (gettoken != userinfo->token)
        {
            userinfo->resp->set_status_code("404");
        }
        else
        {
            //token正确，检验是否可以秒传
            auto userInfo = req->query_list();
            string sha1 = userInfo["fileSha1"];
            string Sql = "SELECT file_name, file_size FROM clouddrive.tbl_file WHERE file_sha1 = '" + sha1 + "';";
#ifdef DEBUG
    cout<<Sql<<endl;
#endif
            auto sha1task = WFTaskFactory::create_mysql_task(url, 0, std::bind(&MysqlMode::GetFilInfo, this, std::placeholders::_1));
            sha1task->get_req()->set_query(Sql);
            sha1task->user_data = (void *)req;
            series_of(readtask)->push_back(sha1task);
        }
    }
    void GetUserState(WFMySQLTask *readtask)
    {
        wfrest::HttpResp *clientresp = static_cast<wfrest::HttpResp *>(readtask->user_data);
        vector<vector<MySQLCell>> rows;
        ReadCheck(readtask, rows);
        if (!ReadCheck(readtask, rows))
            return;
        Json uinfo;
        uinfo["Username"] = rows[0][0].as_string();
        uinfo["SignupAt"] = rows[0][1].as_string();
        Json respinfo;
        respinfo["data"] = uinfo;
        respinfo["code"] = 0;
        respinfo["msg"] = "OK";
        clientresp->String(respinfo.dump());
    }
    void GetFileList(WFMySQLTask *readtask)
    {
        wfrest::HttpResp *clientresp = static_cast<wfrest::HttpResp *>(readtask->user_data);
        vector<vector<MySQLCell>> rows;
        ReadCheck(readtask, rows);
        if (!ReadCheck(readtask, rows))
            return;
        Json resparr;
        for (auto &row : rows)
        {
            Json fileJson;
            fileJson["FileHash"] = row[0].as_string();
            fileJson["FileName"] = row[1].as_string();
            fileJson["FileSize"] = row[2].as_ulonglong();
            fileJson["UploadAt"] = row[3].as_datetime();
            fileJson["LastUpdated"] = row[4].as_datetime();
            resparr.push_back(fileJson);
        }
        clientresp->String(resparr.dump());
    }
   
    void GetFileMes(WFMySQLTask *readtask)
    {
        wfrest::HttpResp *clientresp = static_cast<wfrest::HttpResp *>(readtask->user_data);
        vector<vector<MySQLCell>> rows;
        ReadCheck(readtask, rows);
        if (!ReadCheck(readtask, rows))
            return;
#ifdef DEBUG
        cout << "filepath:" << rows[0][0].as_string() << endl;
#endif

        clientresp->String("http://192.168.88.129:80/" + rows[0][0].as_string());
    }

private:
    std::string url = "mysql://root:543212345@localhost";
};