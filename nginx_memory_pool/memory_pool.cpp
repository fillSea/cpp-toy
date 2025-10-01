#include "memory_pool.h"

#include <algorithm>
#include <cstdint>


MemoryPool::MemoryPool(size_t size)
	: root_block_(nullptr),
	  current_block_(nullptr),
	  size_(0),
	  large_(nullptr),
	  cleanup_(nullptr) {
	if (size <= 0) {
		return;
	}
	size = std::max(size, MinPoolSize());
	size = std::min(size, kDefaultPoolSize);
	void* memory = malloc(size);

	if (memory == nullptr) {
		return;
	}
	// 计算可用内存大小
	size_ = size;
	root_block_ = reinterpret_cast<SmallBlock*>(memory);
	current_block_ = root_block_;
	root_block_->start = reinterpret_cast<uint8_t*>(memory) + sizeof(SmallBlock);
	root_block_->end = reinterpret_cast<uint8_t*>(memory) + size;
	root_block_->next = nullptr;
	root_block_->failed = 0;
	// 设置其他成员变量
	large_ = nullptr;
	cleanup_ = nullptr;
}

MemoryPool::~MemoryPool() {
	// 释放内存池
	DestoryPool();
}

// 内存池释放
void MemoryPool::DestoryPool() {
	// 释放外部资源
	for (CleanupBlock* c = cleanup_; c; c = c->next) {
		if (c->handler) {
			c->handler(c->data);
		}
	}

	// 释放大块内存
	for (LargeBlock* l = large_; l; l = l->next) {
		if (l->alloc) {
			free(l->alloc);
		}
	}

	// 释放内存池
	SmallBlock* n = nullptr;
	for (SmallBlock* p = root_block_; p; p = n) {
		n = p->next;
		free(p);
	}

	root_block_ = current_block_ = nullptr;
	cleanup_ = nullptr;
	large_ = nullptr;
}

// 大块内存释放
void MemoryPool::LargeFree(void *p){
    for (LargeBlock *l = large_; l; l = l->next) {
        if (p == l->alloc) {
            free(l->alloc);
            l->alloc = nullptr;
        }
    }
}

// 重置内存池
void MemoryPool::Reset(){
	// 释放外部资源
	for (CleanupBlock* c = cleanup_; c; c = c->next) {
		if (c->handler) {
			c->handler(c->data);
		}
	}

    // 释放大块内存
    for (LargeBlock* l = large_; l; l = l->next) {
        if (l->alloc) {
            free(l->alloc);
            l->alloc = nullptr;
        }
    }

    // 遍历内存池, 重置内存池的情况
	for (SmallBlock* p = root_block_; p; p = p->next) {
        p->start = reinterpret_cast<uint8_t*>(p) + sizeof(SmallBlock);
        p->failed = 0;
    }

    current_block_ = root_block_;
    cleanup_ = nullptr;
    large_ = nullptr;
}

// 支持内存对齐的内存分配
void* MemoryPool::Allocate(size_t size) {
	if (size <= kMaxAlloc) {
		return AllocateSmall(size, true);
	}
	return AllocateLarge(size);
}

// 进行小块的内存分配
void* MemoryPool::AllocateSmall(size_t size, bool align) {
	uint8_t* m = nullptr;
	SmallBlock* p = current_block_;
	do {
		m = p->start;
		// 需要内存对齐, 则对齐指针
		if (align) {
			m = AlignPtr(m, kAlignment);
		}

		// 检查是否有足够的空间
		if (static_cast<size_t>(p->end - m) >= size) {
			p->start = m + size;
			return m;
		}

		p = p->next;
	} while (p);

	// 内存块不够用了
	return AllocateSmallBlock(size);
}

// 申请新的内存池
void* MemoryPool::AllocateSmallBlock(size_t size) {
	// 开辟相同大小的内存池
	uint8_t* m = reinterpret_cast<uint8_t*>(malloc(size_));
	if (m == nullptr) {
		return nullptr;
	}

	SmallBlock* new_block = reinterpret_cast<SmallBlock*>(m);

	// 只初始化必要的成员变量
	new_block->end = m + size_;
	new_block->next = nullptr;
	new_block->failed = 0;

	// 设置其他成员变量
	m += sizeof(SmallBlock);
	m = AlignPtr(m, kAlignment);
	new_block->start = m + size;  // 已经分配了 size 大小的内存

	// 遍历内存池, 增加失败次数, 当失败次数 > 4 时, 改变内存池的头节点
	SmallBlock* p = current_block_;
	for (; p->next; p = p->next) {
		// 增加前面的内存池的失败次数,
		// 当失败次数 > 4 时, 改变内存池的头节点
		if (p->failed++ > 4) {
			current_block_ = p->next;
		}
	}

	// 退出循环后 p 指向最后一个内存池
	++p->failed;
	if (p->failed > 4) {
		current_block_ = new_block;
	}
	// 将内存池连成链表
	p->next = new_block;

	return m;
}

// 大块内存分配
void* MemoryPool::AllocateLarge(size_t size) {
	// 调用 malloc 分配需要使用的内存
	void* p = malloc(size);
	if (p == nullptr) {
		return nullptr;
	}

	size_t n = 0;
	LargeBlock* large = large_;
	// 寻找前面的大块内存的结构体是否存在已经被释放的内存
	// 若有, 则将分配的内存用前面的结构体管理
	for (; large; large = large->next) {
		if (large->alloc == nullptr) {
			large->alloc = p;
			return p;
		}

		// 连续三次以上没找到, 则放弃
		if (n++ > 3) {
			break;
		}
	}

	// 调用小块内存分配函数分配大块内存的结构体
	large = reinterpret_cast<LargeBlock*>(AllocateSmall(sizeof(LargeBlock), true));
	if (large == nullptr) {
		free(p);
		return nullptr;
	}
	// 插入节点
	large->alloc = p;
	large->next = large_;
	large_ = large;

	return p;
}

// 添加自定义的内存释放结构体
// size: 自定义内存释放结构体的数据大小
CleanupBlock* MemoryPool::AddCleanup(size_t size) {
	// 分配结构体, 调用小块内存分配函数
	CleanupBlock* c =
		reinterpret_cast<CleanupBlock*>(AllocateSmall(sizeof(CleanupBlock), true));
	if (c == nullptr) {
		return nullptr;
	}

	// 分配数据, 调用内存分配函数
	if (size) {
		c->data = Allocate(size);
		if (c->data == nullptr) {
			return nullptr;
		}
	} else {
		c->data = nullptr;
	}

	c->handler = nullptr;
	c->next = cleanup_;
	cleanup_ = c;

	return c;
}
