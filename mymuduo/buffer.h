#pragma once

#include <sys/types.h>
#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>


/// @code
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
/// @endcode
// 封装了用户缓冲区
class Buffer {
public:
	static const size_t kCheapPrepend = 8;	  // 在前面预留的字节数
	static const size_t kInitialSize = 1024;  // 缓冲区数据大小

	explicit Buffer(size_t initial_size = kInitialSize)
		: buffer_(kCheapPrepend + initial_size),
		  reader_index_(kCheapPrepend),
		  writer_index_(kCheapPrepend) {}

	// 可读的字节数
	size_t ReadableBytes() const { return writer_index_ - reader_index_; }
	// 可写的字节数
	size_t WritableBytes() const { return buffer_.size() - writer_index_; }
    // 此时的预留空间为多少 此时readIndex前面的空间都可以作为预留空间
	size_t PrependableBytes() const { return reader_index_; }
    // 返回缓冲区中可读数据的起始地址
    const char* Peek() const {return Begin() + reader_index_;}
    // 获取可写的指针
    char* BeginWrite(){return Begin() + writer_index_;}
    const char* BeginWrite() const {return Begin() + writer_index_;}

    // 提取所有字节，注意writeIndex也必须更改
    void RetrieveAll(){
        reader_index_ = writer_index_ = kCheapPrepend;
    }
    // 提取len字节的数据
    void Retrieve(size_t len){
        if (len < ReadableBytes()){
            reader_index_ += len;
        } else {
            // 缓冲区被读完了
            RetrieveAll();
        }
    }
    // 提取所有的字节，作为一个string返回
    // 把onMessage函数上报的Buffer数据 转成string类型的数据返回
    std::string RetrieveAsString(){
        return RetrieveAsString(ReadableBytes());
    }
    // 提取len字节，作为string返回
    std::string RetrieveAsString(size_t len){
        std::string ret(Peek(), len);
        // 移动缓冲区各指针的位置
        Retrieve(len);
        return ret;
    }
    // 确保缓冲区有足够的空间可写
    void EnsureWritableBytes(size_t len){
        if (WritableBytes() < len){
            MakeSpace(len);
        }
    }
    // 把[data, data + len]内存的数据写入到缓冲区
    void Append(const char* data, size_t len){
        EnsureWritableBytes(len);
        std::copy(data, data + len, BeginWrite());
        writer_index_ += len;
    }
    // 从 fd 上读取数据
    ssize_t ReadFd(int fd, int* save_errno);
    // 通过 fd 发送数据
    ssize_t WriteFd(int fd, int* save_errno);
private:
	char* Begin() { return buffer_.data(); }
	const char* Begin() const { return buffer_.data(); }
	// 扩容
	// len: 要写入的数据
	void MakeSpace(size_t len) {
		// 已读的字节数+可写的字节数> 需要写的字节数
		// 移动缓冲区
		if (PrependableBytes() - kCheapPrepend + WritableBytes() >= len) {
			size_t readable = ReadableBytes();
			std::copy(Begin() + reader_index_, Begin() + writer_index_,
					  Begin() + kCheapPrepend);
            reader_index_ = kCheapPrepend;
            writer_index_ = reader_index_ + readable;
		} else {
            // 直接扩容
            buffer_.resize(writer_index_ + len);
        }
	}

private:
	std::vector<char> buffer_;	// 数据缓冲区
	size_t reader_index_;		// 可读起始地址
	size_t writer_index_;		// 可写起始地址
};