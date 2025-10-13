#pragma once
/**
* noncopyable被继承以后，派生类对象可以正常的构造和析构，但是派生类
* 无法进行拷贝构造和赋值操作
*/

class Noncopyable {
public:
	Noncopyable(const Noncopyable&) = delete;
	void operator=(const Noncopyable&) = delete;
    Noncopyable(Noncopyable&&) = delete;
    void operator=(Noncopyable&&) = delete;
protected:
	Noncopyable() = default;
	~Noncopyable() = default;
};