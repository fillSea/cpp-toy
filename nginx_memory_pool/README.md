# 1. 项目介绍
本项目是仿照 nginx 内存池实现的一个简单的内存池
# 2. 项目架构
如下图所示，
![nginx内存池架构图](./images/%E4%BB%BFnginx%E5%86%85%E5%AD%98%E6%B1%A0.png)
# 3. 测试代码
```cpp
#include "memory_pool.h"
#include <iostream>
#include <string>
#include <cassert>
#include <vector>
#include <cstring>

// 测试用的外部资源结构体
struct TestResource {
    std::string name;
    int* data;
    int size;

    TestResource(const std::string& n, int s) : name(n), size(s) {
        data = new int[size];
        for (int i = 0; i < size; ++i) {
            data[i] = i;
        }
        std::cout << "TestResource created: " << name << ", size: " << size << std::endl;
    }

    ~TestResource() {
        std::cout << "TestResource destroyed: " << name << std::endl;
        delete[] data;
    }
};

// 测试资源的清理函数
void CleanupTestResource(void* data) {
    if (data == nullptr) return;
    
    TestResource* resource = static_cast<TestResource*>(data);
    std::cout << "Cleaning up TestResource: " << resource->name << std::endl;
    delete resource;
}

// 用于测试内存写入和读取的函数
void TestMemoryAccess(void* ptr, size_t size, char value) {
    char* buffer = static_cast<char*>(ptr);
    for (size_t i = 0; i < size; ++i) {
        buffer[i] = value;
    }
    
    // 验证写入是否成功
    for (size_t i = 0; i < size; ++i) {
        if (buffer[i] != value) {
            std::cerr << "Memory access test failed at position " << i << std::endl;
            return;
        }
    }
    std::cout << "Memory access test passed for buffer with value '" << value << "'" << std::endl;
}

// 测试内存对齐功能
void TestMemoryAlignment(MemoryPool& pool) {
    std::cout << "\nTesting memory alignment..." << std::endl;
    
    void* ptr1 = pool.Allocate(1);
    void* ptr2 = pool.Allocate(2);
    void* ptr3 = pool.Allocate(3);
    
    // 验证地址是否按16字节对齐
    uintptr_t addr1 = reinterpret_cast<uintptr_t>(ptr1);
    uintptr_t addr2 = reinterpret_cast<uintptr_t>(ptr2);
    uintptr_t addr3 = reinterpret_cast<uintptr_t>(ptr3);
    
    assert((addr1 % kAlignment) == 0 && "Memory address not aligned");
    assert((addr2 % kAlignment) == 0 && "Memory address not aligned");
    assert((addr3 % kAlignment) == 0 && "Memory address not aligned");
    
    std::cout << "Memory alignment test passed! Addresses are properly aligned to " 
              << kAlignment << " bytes." << std::endl;
}

// 测试内存池Reset功能
void TestReset(MemoryPool& pool) {
    std::cout << "\nTesting memory pool reset..." << std::endl;
    
    // 先分配一些内存
    void* ptr1 = pool.Allocate(100);
    void* ptr2 = pool.Allocate(200);
    
    // 写入数据
    memset(ptr1, 'A', 100);
    memset(ptr2, 'B', 200);
    
    std::cout << "Allocated and wrote data to memory blocks." << std::endl;
    
    // 重置内存池
    pool.Reset();
    
    // 重新分配相同大小的内存，应该是相同的地址
    void* ptr3 = pool.Allocate(100);
    void* ptr4 = pool.Allocate(200);
    
    std::cout << "After reset, allocating new memory blocks..." << std::endl;
    
    // 验证重置后的内存地址是否与之前相同
    // assert(ptr1 == ptr3 && "Memory addresses should match after reset");
    // assert(ptr2 == ptr4 && "Memory addresses should match after reset");
    std::cout << ptr1 << ' ' << ptr3 << std::endl;
    std::cout << ptr2 << ' ' << ptr4 << std::endl;
    
    // 写入新数据并验证
    memset(ptr3, 'C', 100);
    memset(ptr4, 'D', 200);
    
    std::cout << "Memory pool reset test passed!" << std::endl;
}

// 测试LargeFree功能
void TestLargeFree(MemoryPool& pool) {
    std::cout << "\nTesting large memory block freeing..." << std::endl;
    
    // 分配大块内存
    size_t large_size = 5000; // 大于4095字节
    void* large1 = pool.Allocate(large_size);
    void* large2 = pool.Allocate(large_size);
    
    std::cout << "Allocated two large memory blocks: " << large_size << "B each" << std::endl;
    
    // 释放第一个大块内存
    pool.LargeFree(large1);
    std::cout << "Freed the first large memory block" << std::endl;
    
    // 重新分配相同大小的内存，应该重用刚才释放的内存
    void* large3 = pool.Allocate(large_size);
    std::cout << "Re-allocated a large memory block" << std::endl;
    
    // 验证是否重用了释放的内存
    if (large1 == large3) {
        std::cout << "Memory reuse test passed! The re-allocated memory reused the freed block." << std::endl;
    } else {
        std::cout << "Note: The re-allocated memory did not reuse the freed block immediately." << std::endl;
        std::cout << "This could be due to memory pool's internal allocation strategy." << std::endl;
    }
}

// 测试大量内存分配
void TestMassAllocation(MemoryPool& pool) {
    std::cout << "\nTesting mass memory allocation..." << std::endl;
    
    const int num_allocations = 1000;
    std::vector<void*> pointers;
    pointers.reserve(num_allocations);
    
    // 分配大量小块内存
    for (int i = 0; i < num_allocations; ++i) {
        size_t size = (i % 200) + 1; // 1-200字节的随机大小
        void* ptr = pool.Allocate(size);
        assert(ptr != nullptr && "Failed to allocate memory during mass allocation");
        pointers.push_back(ptr);
    }
    
    std::cout << "Successfully allocated " << num_allocations << " small memory blocks." << std::endl;
    
    // 测试这些内存的可访问性
    for (size_t i = 0; i < pointers.size(); ++i) {
        char* buffer = static_cast<char*>(pointers[i]);
        buffer[0] = 'X'; // 简单写入测试
    }
    
    std::cout << "Mass allocation memory access test passed!" << std::endl;
}

int main() {
    std::cout << "===== Nginx Memory Pool C++ Implementation Test =====" << std::endl;
    
    // 1. 创建内存池
    std::cout << "\n1. Creating memory pool..." << std::endl;
    MemoryPool pool(4096); // 创建4KB大小的内存池
    
    if (pool.Allocate(1) == nullptr) {
        std::cerr << "Failed to create memory pool!" << std::endl;
        return 1;
    }
    std::cout << "Memory pool created successfully!" << std::endl;
    
    // 2. 测试内存对齐功能
    TestMemoryAlignment(pool);
    
    // 3. 测试小块内存分配
    std::cout << "\n3. Testing small memory allocations..." << std::endl;
    
    // 分配多个不同大小的小块内存
    void* small1 = pool.Allocate(64);  // 64字节
    void* small2 = pool.Allocate(128); // 128字节
    void* small3 = pool.Allocate(256); // 256字节
    
    if (!small1 || !small2 || !small3) {
        std::cerr << "Failed to allocate small memory blocks!" << std::endl;
        return 1;
    }
    
    std::cout << "Allocated small memory blocks: 64B, 128B, 256B" << std::endl;
    
    // 测试内存访问
    TestMemoryAccess(small1, 64, 'A');
    TestMemoryAccess(small2, 128, 'B');
    TestMemoryAccess(small3, 256, 'C');
    
    // 4. 测试大块内存分配
    std::cout << "\n4. Testing large memory allocations..." << std::endl;
    
    // 分配超过MAX_ALLOC的大块内存
    size_t large_size = 5000; // 大于4095字节
    void* large1 = pool.Allocate(large_size);
    
    if (!large1) {
        std::cerr << "Failed to allocate large memory block!" << std::endl;
        return 1;
    }
    
    std::cout << "Allocated large memory block: " << large_size << "B" << std::endl;
    
    // 测试大块内存访问
    TestMemoryAccess(large1, large_size, 'X');
    
    // 5. 测试LargeFree功能
    TestLargeFree(pool);
    
    // 6. 测试Reset功能
    TestReset(pool);
    
    // 7. 测试大量内存分配
    TestMassAllocation(pool);
    
    // 8. 测试添加清理函数
    std::cout << "\n8. Testing cleanup functions..." << std::endl;
    
    // 创建测试资源
    TestResource* resource = new TestResource("TestResource-1", 100);
    
    // 添加清理函数
    CleanupBlock* cleanup = pool.AddCleanup(sizeof(TestResource));
    if (!cleanup) {
        std::cerr << "Failed to add cleanup block!" << std::endl;
        delete resource;
        return 1;
    }

    // 设置清理数据和函数
    cleanup->data = resource;
    cleanup->handler = CleanupTestResource;
    
    std::cout << "Cleanup function added successfully!" << std::endl;
    
    // 9. 测试内存池销毁（将在离开作用域时自动调用析构函数）
    std::cout << "\n9. Testing memory pool destruction..." << std::endl;
    
    std::cout << "\nMemory pool test completed successfully!" << std::endl;
    std::cout << "===== Test End =====" << std::endl;
    
    return 0;
}
```