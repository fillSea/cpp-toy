#include "memory_pool.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>


// 初始化静态变量
char* MemoryPool::start_free_ = 0;

char* MemoryPool::end_free_ = 0;

size_t MemoryPool::heap_size_ = 0;

Chunk* volatile MemoryPool::free_list_[kFreeLists] = {
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
};

std::mutex MemoryPool::mtx_;

// 分配内存的入口函数, 开辟内存
/* n must be > 0      */
// n: 开辟内存的字节数
void* MemoryPool::Allocate(size_t n) {
	void* ret = 0;

	if (n > (size_t)kMaxBytes) {
		ret = malloc(n);
	} else {
		// 获取当前字节数所在的空闲链表, my_free_list = **, 二级指针
		// free_list_ 隐式转化为指向数组的首元素, 即 **Chunk
		Chunk* volatile* my_free_list = free_list_ + FreelistIndex(n);

		std::lock_guard<std::mutex> lk(mtx_);  // 加锁, RAII类

		Chunk* result = *my_free_list;	// 获取头节点
		if (result == nullptr) {		// 链表为空, 直接分配内存
			ret = Refill(RoundUp(n));
		} else {
			// 链表不为空, 取出头部元素
			*my_free_list = result->next;
			ret = result;
		}
	}

	return ret;
};

// 一次性分配多个节点
void* MemoryPool::Refill(size_t n) {
	int nobjs = 20;	 // 需要开辟的节点个数
	// 开辟内存, 返回开辟内存的起始地址
	// nobjs以引用方式传递, 表示具体分配的节点个数
	char* chunk = AllocChunk(n, nobjs);

	if (1 == nobjs) {
		return chunk;
	}

	// 获取链表首节点
	Chunk* volatile* my_free_list = free_list_ + FreelistIndex(n);

	// 分配的一个节点
	Chunk* result = reinterpret_cast<Chunk*>(chunk);
	// 指向下一个节点
	*my_free_list = reinterpret_cast<Chunk*>(chunk + n);
	// 将分配的连续内存块构造成链表
	// Chunk* current_obj;
	// Chunk* next_obj = *my_free_list;
	Chunk* current_obj = reinterpret_cast<Chunk*>(chunk + n);
	Chunk* next_obj = nullptr;
	for (int i = 2; i < nobjs; i++) {
		// 指向下一个节点
		next_obj = reinterpret_cast<Chunk*>(reinterpret_cast<char*>(current_obj) + n);
		current_obj->next = next_obj;
		current_obj = next_obj;
	}

	current_obj->next = nullptr;

	return result;
}

// 分配相应内存字节大小的 chunk 块
char* MemoryPool::AllocChunk(size_t size, int& nobjs) {
	char* result;
	size_t total_bytes = size * nobjs;			  // 分配的总字节数
	size_t bytes_left = end_free_ - start_free_;  // 剩余未分配的字节数

	// 可以直接分配 nobjs 个节点
	if (bytes_left >= total_bytes) {
		result = start_free_;
		start_free_ += total_bytes;
		return result;
	}

	if (bytes_left >= size) {  // 至少可以分配一个节点
		nobjs = static_cast<int>(bytes_left / size);
		total_bytes = size * nobjs;
		result = start_free_;
		start_free_ += total_bytes;
		return result;
	}

	// 使用剩余的可使用的内存, 分配给需要 bytes_left 字节的链表使用
	if (bytes_left > 0) {
		Chunk* volatile* my_free_list = free_list_ + FreelistIndex(bytes_left);

		reinterpret_cast<Chunk*>(start_free_)->next = *my_free_list;
		*my_free_list = reinterpret_cast<Chunk*>(start_free_);
	}

	// 需要再次申请内存
	size_t bytes_to_get = 2 * total_bytes + RoundUp(heap_size_ >> 4);
	// 重新分配内存
	start_free_ = reinterpret_cast<char*>(malloc(bytes_to_get));

	// 开辟内存失败
	if (nullptr == start_free_) {
		Chunk* volatile* my_free_list;
		Chunk* p;
		// 从后面的可使用的字节数 >= size 的链表中, 取出一个空闲节点使用
		for (size_t i = size; i <= static_cast<size_t>(kMaxBytes);
			 i += static_cast<size_t>(kAlign)) {
			// 取size及其后面的每个链表的地址
			my_free_list = free_list_ + FreelistIndex(i);
			p = *my_free_list;	// 头节点
			// 若存在空闲节点
			if (nullptr != p) {	 // 取出一个>=size的链表的其中一个节点
				*my_free_list = p->next;
				start_free_ = reinterpret_cast<char*>(p);
				end_free_ = start_free_ + i;
				return AllocChunk(size, nobjs);
			}
		}
		// 后面没有可用节点
		end_free_ = nullptr;
		// 采用一级配置器分配内存
		start_free_ = reinterpret_cast<char*>(malloc(bytes_to_get));
	}

	heap_size_ += bytes_to_get;
	end_free_ = start_free_ + bytes_to_get;

	// 递归调用
	return AllocChunk(size, nobjs);
}

// 释放内存
void MemoryPool::Deallocate(void* p, size_t n) {
	if (p == nullptr || n == 0) return;	 // 空指针或0字节无需处理

	// 使用一级配置器分配
	if (n > static_cast<size_t>(kMaxBytes)) {
		free(p);
		return;
	}
	// 使用二级配置器分配
	Chunk* volatile* my_free_list = free_list_ + FreelistIndex(n);
	Chunk* q = reinterpret_cast<Chunk*>(p);

	std::lock_guard<std::mutex> lk(mtx_);
	// 头插
	q->next = *my_free_list;
	*my_free_list = q;
}

// 内存扩容 & 缩容
void* MemoryPool::Reallocate(void* p, size_t old_size, size_t new_size) {
	// 使用一级配置器分配的内存, 使用 realloc 扩容
	if (old_size > static_cast<size_t>(kMaxBytes) &&
		new_size > static_cast<size_t>(kMaxBytes)) {
		return realloc(p, new_size);
	}

	// 使用二级配置器分配的内存

	// 不扩容
	if (RoundUp(old_size) == RoundUp(new_size)) return p;

	void* result = Allocate(new_size);

	if (result == nullptr) {
		return nullptr;
	}

	size_t copy_size = std::min(new_size, old_size);
	memcpy(result, p, copy_size);

	Deallocate(p, old_size);  // 释放旧的内存

	return result;
}
