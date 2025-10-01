#pragma once
#include <mutex>

// 内存池的粒度信息
constexpr size_t kAlign = 8;					   // 对齐粒度（8字节）
constexpr size_t kMaxBytes = 128;				   // 最大管理字节数
constexpr size_t kFreeLists = kMaxBytes / kAlign;  // 自由链表数量（16个）

// 每一个内存chunk块的头信息
// 既作为 “空闲内存块链表的节点”，又作为 “已分配内存块的用户数据起始标记”
union Chunk {
	union Chunk* next;	// next 指针
	// 用户数据的起始地址
	char data[1]; /* The client sees this.*/
};

class MemoryPool {
public:
	MemoryPool() = default;
	~MemoryPool() = default;
	// 禁用拷贝和移动语义
	MemoryPool(const MemoryPool&) = delete;
	MemoryPool& operator=(const MemoryPool&) = delete;
	MemoryPool(MemoryPool&&) = delete;
	MemoryPool& operator=(MemoryPool&&) = delete;
	// 开辟内存
	void* Allocate(size_t n);
	// 释放内存
	void Deallocate(void* p, size_t n);
	// 内存扩容 & 缩容
	void* Reallocate(void* p, size_t old_size, size_t new_size);
	// 对象构造
	template <typename T, typename... Args>
	// void Construct(T* p, const T& val) { new (p) T(val); }
	void Construct(T* p, Args&&... args) {
		new (p) T(std::forward<Args>(args)...);
	}
	// 对象析构
	template <typename T>
	void Destory(T* p) {
		if (p) {
			p->~T();
		}
	}

private:
	/*将 bytes 上调至最邻近的 8 的倍数*/
	static constexpr size_t RoundUp(size_t bytes) {
		return ((bytes + kAlign - 1) & ~(kAlign - 1));
	}
	/*返回 bytes 大小的chunk块位于 free-list 中的编号*/
	static constexpr size_t FreelistIndex(size_t bytes) {
		return ((bytes + kAlign - 1) / kAlign - 1);
	}
	// 一次性分配多个节点
	void* Refill(size_t n);
	// 分配相应内存字节大小的 chunk 块
	char* AllocChunk(size_t size, int& nobjs);
private:
	// 自由链表数组, 静态链表, 节点之间的内存连续
	// 组织所有自由链表的数组，数组的每一个元素的类型是_Obj*，全部初始化为0
	static Chunk* volatile free_list_[kFreeLists];
	// 已申请内存的 Chunk 块但未分配的情况
	static char* start_free_;  // 未分配的起始地址
	static char* end_free_;	   // 未分配的结束地址
	static size_t heap_size_;  // 已申请的堆的大小
	static std::mutex mtx_;	   // 互斥锁, 保护自由链表的访问
};
