#pragma once

#include <cstddef>
#include <functional>
#include <memory>

class TcpConnection;
class Buffer;
class Timestamp;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

// 事件回调函数
using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
using CloseCallback = std::function<void(const TcpConnectionPtr&)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;
using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer*, Timestamp)>;
// 高水位回调, 用于平衡发送速率和接收速率
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr&, size_t)>;