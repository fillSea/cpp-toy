#include "buffer.h"

#include <bits/types/struct_iovec.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>

// 从 fd 上读取数据
// save_errno: 读取的错误码
// returns: 读取的字节数
ssize_t Buffer::ReadFd(int fd, int* save_errno) {
	// 采用双缓冲区进行读取
	char extra_buf[65536] = {0};  // 64K

	struct iovec vec[2];

	const size_t writable = WritableBytes();
	vec[0].iov_base = BeginWrite();
	vec[0].iov_len = writable;

	vec[1].iov_base = extra_buf;
	vec[1].iov_len = sizeof(extra_buf);

	// 当空间足够的时候，我们不向extrabuf中写入数据，仅仅当buffer剩余位置不够时，才这样做
	// 调用一次readv，最多读取writable + 65536个数据
	const int iov_cnt = (writable < sizeof(extra_buf)) ? 2 : 1;
	const ssize_t n = ::readv(fd, vec, iov_cnt);

	if (n < 0) {
		*save_errno = errno;
	} else if (n <= writable) {
		writer_index_ += n;
	} else {
		writer_index_ = buffer_.size();
		Append(extra_buf, n - writable);
	}

	return n;
}

// 通过 fd 发送数据
ssize_t Buffer::WriteFd(int fd, int* save_errno){
	ssize_t n = ::write(fd, Peek(), ReadableBytes());
	if (n < 0){
		*save_errno = errno;
	}

	return n;
}