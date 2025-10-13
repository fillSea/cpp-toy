#include "inet_address.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <cstdint>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstddef>

InetAddress::InetAddress(std::string ip, uint16_t port){
    bzero(&addr_, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());
    addr_.sin_port = htons(port);
}

// 返回 string 类型的 ip 地址
std::string InetAddress::ToIp() const{
    char buf[64] = {0};
    inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));

    return buf;
}

// 返回 ip:port
std::string InetAddress::ToIpPort() const{
    char buf[64] = {0};
    inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    size_t end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port);
    sprintf(buf + end, ":%u", port);

    return buf;
}

// 返回本地字节序的 port
uint16_t InetAddress::ToPort() const{
    return ntohs(addr_.sin_port);
}

// #include <iostream>
// int main(){
//     InetAddress addr;
//     std::cout << addr.ToIpPort() << std::endl;
//     std::cout << addr.ToIp() << std::endl;
//     std::cout << addr.ToPort() << std::endl;

//     return 0;
// }