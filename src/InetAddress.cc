#include"InetAddress.hh"
#include<cstring>  
#include<iostream>
InetAddress::InetAddress(uint16_t port,std::string ip){
    //memset(&addr_,0,sizeof(addr_));
    bzero(&addr_,sizeof(addr_));
    addr_.sin_family = AF_INET;
    // 将主机字节序转换为网络字节序(大端)
    addr_.sin_port = htons(port);
    // 将IP地址字符串转换为网络字节序的整数值
    // sin_addr是一个in_addr结构体,需要通过s_addr成员来设置32位IPv4地址
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());
}
std::string InetAddress::toIp() const{
    // inet_ntoa()函数直接接收in_addr结构体作为参数
    // 因为该函数内部会自动访问s_addr成员来获取IP地址
    return inet_ntoa(addr_.sin_addr);
}
std::string InetAddress::toIpPort() const{
    // 将IP地址和端口号拼接成一个字符串
    return toIp() + ":" + std::to_string(toPort());
}
uint16_t InetAddress::toPort() const{
    // 将网络字节序的端口号转换为主机字节序
    return ntohs(addr_.sin_port);
}

// int main(){
//     InetAddress addr(8080,"127.0.0.1");
//     std::cout << addr.toIpPort() << std::endl;
//     return 0;
// }