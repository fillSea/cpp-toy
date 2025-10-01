#pragma  once
#include <cstdint>

// 页面大小
constexpr int kPageSize = 4096;
// 能申请的最大字节数, 小于一页
constexpr size_t kMaxAlloc = kPageSize - 1;
// 默认的池的大小
constexpr size_t kDefaultPoolSize = 16 * 1024;
// 对齐字节数
constexpr size_t kAlignment = 16;

// 向上取整对齐
constexpr size_t AlignSize(size_t size, size_t alignment) {
	return (size + alignment - 1) & ~(alignment - 1);
}

// 指针对齐
inline uint8_t *AlignPtr(void *ptr, size_t alignment) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
    return reinterpret_cast<uint8_t*>(aligned_addr);
}

// 大块内存节点
struct LargeBlock {
	LargeBlock *next;  // 下一个大块内存
	void *alloc;	   // 大块内存的起始地址
};

// 小块内存节点
struct SmallBlock {
	uint8_t *start;	   // 下一个可以分配的位置
	uint8_t *end;	   // 内存池的结束位置
	SmallBlock *next;  // 下一个内存池
	int failed;		   // 失败次数
};

// 清理函数
using CleanupHandler = void (*)(void *data);
// 外部资源清理节点
struct CleanupBlock {
	CleanupHandler handler;	 // 清理函数
	void *data;				 // 需要请求的资源
	CleanupBlock *next;		 // 下一个清理节点
};

class MemoryPool {
public:
	MemoryPool(const MemoryPool&) = delete;  // 禁用拷贝构造
    MemoryPool& operator=(const MemoryPool&) = delete;  // 禁用赋值
	// 静态方法，用于计算内存池最小大小
	static size_t MinPoolSize() {
		return AlignSize(sizeof(MemoryPool) + 2 * sizeof(LargeBlock), kAlignment);
	}
    MemoryPool(size_t size = kDefaultPoolSize);
	~MemoryPool();
	// 支持内存对齐的内存分配
	void *Allocate(size_t size);
	// 添加自定义的内存释放结构体
	CleanupBlock *AddCleanup(size_t size);
	// 大块内存释放
	void LargeFree(void *p);
	// 重置内存池
	void Reset();
private:
	// 进行小块的内存分配
	void *AllocateSmall(size_t size, bool align);
	// 大块内存分配
	void *AllocateLarge(size_t size);
	// 申请新的内存池
	void *AllocateSmallBlock(size_t size);
	// 内存池释放
	void DestoryPool();
private:
    SmallBlock* root_block_;		// 根小块内存内存池数据
    SmallBlock* current_block_;		// 当前小块内存内存池数据
	size_t size_;			// 内存池大小
	LargeBlock *large_;		// 大内存块链表
	CleanupBlock *cleanup_;	// 清理函数链表
};