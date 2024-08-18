#include "Header_file.hpp"
#include "GetUser.hpp"
#include "MysqlMode.hpp"
#include "GetUser.hpp"
#include"FileFunction.hpp"
static WFFacilities::WaitGroup waitgroup(1);

static GetUser getuser_;
static File file;
void sighandler(int num)
{
    waitgroup.done();
    cout << "wait group is done" << endl;
}

int main()
{
    wfrest::HttpServer server;
    //得到注册和登入选择页面
    server.GET("/clouddrive",[](const wfrest::HttpReq *req, wfrest::HttpResp *resp){
        resp->File("../public/clouddrive.html");
    });
    // 得到注册页面
    server.GET("/user/signup", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
               { resp->File("../public/sign.html"); });
    // 进行注册
    server.POST("/user/signup", bind(&GetUser::signup, &getuser_, _1, _2, _3));

    // 得到登入页面
    server.GET("/view/signin.html", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
               { resp->File("../public/view/signin.html"); });

    // 进行登入
    server.POST("/user/signin", bind(&GetUser::login, &getuser_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    // 得到用户页面
    server.GET("/view/home.html", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
               { resp->File("../public/view/home.html"); });

    // 加载用户页面的用户信息
    server.POST("/user/info", bind(&GetUser::UserState, &getuser_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    // 加载该用户的文件列表
    server.POST("/file/query", bind(&GetUser::FileList, &getuser_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    // 得到上传功能页面
    server.GET("/file/upload", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
               { resp->File("../public/function.html"); });

    // 上传功能的实现
    server.POST("/file/upload", bind(&File::UpFile, &file, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    // 下载功能的实现
    server.POST("/file/downloadurl", bind(&File::DownFile, &file, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

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
