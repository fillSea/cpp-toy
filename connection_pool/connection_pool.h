#include <atomic>
#include <memory>
#include <queue>
#include <string>
#include "connection.h"
#include <mutex>

class ConnectionPool{
public:
    // 单例接口
    static ConnectionPool* GetInstance();
    // 获取可用连接
    std::shared_ptr<Connection> GetConnection();
private:
    ConnectionPool();
    // 从配置文件中加载配置
    bool LoadConfig();
    // 运行在独立的线程中, 专门负责生产新连接
    void ProduceConnectionTask();
    // 运行在独立的线程中, 专门负责检查空闲连接
    void CheckIdleConnectionTask();
private:
    std::string ip_;
    unsigned short port_;
    std::string username_;
    std::string password_;
    std::string dbname_;
    int init_size_; // 连接池初始大小
    int max_size_; // 连接池最大大小
    std::atomic<int> current_connection_size_; // 当前连接池大小
    int max_idle_time_; // 连接池最大空闲时间
    int connection_timeout_; // 连接超时时间

    std::queue<Connection*> connection_queue_; // 连接队列
    std::mutex queue_mutex_; // 队列互斥锁
    std::condition_variable cond_;// 条件变量, 用于通知消费者有新连接可用
};