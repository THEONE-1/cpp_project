#pragma once
#include <iostream>
#include <iomanip>
#include<string>
#include <sstream>
#include <openssl/sha.h>
#include <openssl/md5.h>
#include <fstream>
#include <wfrest/HttpServer.h>
#include <time.h>
using namespace std;
struct UserInfo
{
    string username;
    string password;
    string token;
    wfrest::HttpResp *resp;
};
struct FileInfo
{
    std::string filename_;
    int filesize_;

    FileInfo() = default;  // 使用默认构造函数
    FileInfo(const std::string &filename, int filesize) : filename_(filename), filesize_(filesize) {}
};


class FileUtil
{
public:
    static std::string sha1File(const std::string &input)
    {
        std::ifstream file(input, std::ios::binary); // 确保以二进制模式打开文件
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string contents = buffer.str();

        unsigned char hash[SHA_DIGEST_LENGTH];
        SHA1(reinterpret_cast<const unsigned char *>(contents.c_str()), contents.size(), hash);

        std::stringstream hexstream;
        for (unsigned char i : hash)
        {
            hexstream << std::hex << (int)i;
        }
        return hexstream.str();
    }
};

class Token
{
public:
    Token(string username, string salt)
        : username_(username), salt_(salt)
    {
        // 使用MD5加密
        string tokenGen = username_ + salt_;
        unsigned char md[16];
        MD5((const unsigned char *)tokenGen.c_str(), tokenGen.size(), md);
        char frag[3];
        for (int i = 0; i < 16; i++)
        {
            sprintf(frag, "%02x", md[i]);
            token += frag;
        }
        char timeStamp[9];
        time_t now = time(nullptr);
        strftime(timeStamp, sizeof(timeStamp), "%d%H%M%S", localtime(&now));
        token += timeStamp;
    }

    string token;

private:
    string username_;
    string salt_;
};
