#include <netinet/in.h>
#include <string>
#include <cstdint>

// 封装 socket 地址
class InetAddress{
public:
    explicit InetAddress(std::string ip = "127.0.0.1", uint16_t port = 8080);
    explicit InetAddress(const sockaddr_in& addr): addr_(addr){}
    // 返回 string 类型的 ip 地址
    std::string ToIp() const;
    // 返回 ip:port
    std::string ToIpPort() const;
    // 返回本地字节序的 port
    uint16_t ToPort() const;
    const sockaddr_in* GetSockAddr() const {return &addr_;}
    void SetSockAddr(const sockaddr_in& addr) {addr_ = addr;}
private:
    sockaddr_in addr_; // ipv4 地址结构体
};