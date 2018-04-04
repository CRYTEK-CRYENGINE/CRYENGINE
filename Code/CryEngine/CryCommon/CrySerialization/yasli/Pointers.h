/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once
#include "Assert.h"
#include "Config.h"

namespace yasli{

class RefCounter{
public:
    RefCounter() : refCounter_(0) {}
    ~RefCounter() {}

    int refCount() const{ return refCounter_; }

    void acquire(){ ++refCounter_; }
    int release(){ return --refCounter_; }
private:
    int refCounter_;
};

class PolyRefCounter : public RefCounter{
public:
    virtual ~PolyRefCounter() {}
};

class PolyPtrBase{
public:
    PolyPtrBase()
    : ptr_(0)
    {
    }
    void release(){
        if(ptr_){
            if(!ptr_->release())
                delete ptr_;
            ptr_ = 0;
        }
    }
    void reset(PolyRefCounter* const ptr = 0){
        if(ptr_ != ptr){
            release();
            ptr_ = ptr;
            if(ptr_)
                ptr_->acquire();
        }
    }
protected:
    PolyRefCounter* ptr_;
};

template<class T>
class PolyPtr : public PolyPtrBase{
public:
    PolyPtr()
    : PolyPtrBase()
    {
    }

    PolyPtr(PolyRefCounter* ptr)
    {
        reset(ptr);
    }

	template<class U>
    PolyPtr(U* ptr)
    {
		// TODO: replace with static_assert
		YASLI_ASSERT("PolyRefCounter must be a first base when used with multiple inheritance." && 
			   static_cast<PolyRefCounter*>(ptr) == reinterpret_cast<PolyRefCounter*>(ptr));
        reset(static_cast<PolyRefCounter*>(ptr));
    }

    PolyPtr(const PolyPtr& ptr)
    : PolyPtrBase()
    {
        reset(ptr.ptr_);
    }
    ~PolyPtr(){
        release();
    }
    operator T*() const{ return get(); }
    template<class U>
    operator PolyPtr<U>() const{ return PolyPtr<U>(get()); }
    operator bool() const{ return ptr_ != 0; }

	PolyPtr& operator=(const PolyPtr& ptr){
        reset(ptr.ptr_);
        return *this;
    }
    T* get() const{ return reinterpret_cast<T*>(ptr_); }
    T& operator*() const{
        return *get();
    }
    T* operator->() const{ return get(); }
};

class Archive;
template<class T>
class SharedPtr{
public:
    SharedPtr()
    : ptr_(0){}
    SharedPtr(T* const ptr)
    : ptr_(0)
    {
        reset(ptr);
    }
    SharedPtr(const SharedPtr& ptr)
    : ptr_(0)
    {
        reset(ptr.ptr_);
    }
    ~SharedPtr(){
        release();
    }
    operator T*() const{ return get(); }
    template<class U>
    operator SharedPtr<U>() const{ return SharedPtr<U>(get()); }
    SharedPtr& operator=(const SharedPtr& ptr){
        reset(ptr.ptr_);
        return *this;
    }
    T* get(){ return ptr_; }
    T* get() const{ return ptr_; }
    T& operator*(){
        return *get();
    }
    T* operator->() const{ return get(); }
    void release(){
			if(ptr_){
				if(!ptr_->release())
					delete ptr_;
				ptr_ = 0;
			}
    }
	template<class _T>
    void reset(_T* const ptr){
        if(ptr_ != ptr){
            release();
            ptr_ = ptr;
            if(ptr_)
                ptr_->acquire();
        }
    }
	void reset()
	{
		reset<T>(0);
	}
protected:
    T* ptr_;
};


//////////////////////////////////////////////////////////////////////////

template<class T>
class AutoPtr{
public:
    AutoPtr()
    : ptr_(0)
    {
    }
    AutoPtr(T* ptr)
    : ptr_(0)
    {
        set(ptr);
    }
    ~AutoPtr(){
        release();
    }
    AutoPtr& operator=(T* ptr){
        set(ptr);
        return *this;
    }
    void set(T* ptr){
        if(ptr_ && ptr_ != ptr)
            release();
        ptr_ = ptr;
        
    }
    T* get() const{ return ptr_; }
    operator T*() const{ return get(); }
    void detach(){
        ptr_ = 0;
    }
    void release(){
        delete ptr_;
        ptr_ = 0;
    }
    T& operator*() const{ return *get(); }
    T* operator->() const{ return get(); }
private:
    T* ptr_;
};

class Archive;


template<class Ptr>
struct AsObjectWrapper
{
	Ptr& ptr_;

	AsObjectWrapper(Ptr& ptr)
	: ptr_(ptr)
	{
	}
};


//////////////////////////////////////////////////////////////////////////

class WeakObject
{
public:
	WeakObject() : node_(0) {}

	~WeakObject()
	{
		if(node_) 
			node_->pointer_ = 0;
	}

private:
	struct Node
	{
		WeakObject* pointer_;
		int refCounter_;
	};
	Node* node_;

	WeakObject(const WeakObject &);
	void operator=(const WeakObject &);

	template<class T> friend class WeakPtr;
};

template<class T> class WeakPtr
{
public:
	WeakPtr() : node_(0) {}
	WeakPtr(T *p) { reset(p); }
	WeakPtr(const WeakPtr &p)
	{
		if(p.node_) 
			p.node_->refCounter_++;
		node_ = p.node_;
	}
	~WeakPtr() { release(); }

	operator T*() const { return get(); }
	T* operator->() const { return get(); }

	WeakPtr& operator=(T *p)
	{
		if (!p || get() != p)
		{
			release();
			reset(p);
		}
		return *this;
	}
	WeakPtr& operator=(const WeakPtr &p)
	{
		release();
		if(p.node_) 
			p.node_->refCounter_++;
		node_ = p.node_;
		return *this;
	}

private:
	WeakObject::Node *node_;

	void reset(T *p)
	{
		if(p){
			if(p->node_){
				node_ = p->node_;
				node_->refCounter_++;
			}
			else{
				node_ = p->node_ = new WeakObject::Node;
				node_->pointer_ = p;
				node_->refCounter_ = 1;
			}
		}
		else 
			node_ = 0;
	}

	void release()
	{
		if(node_){
			if(--node_->refCounter_ == 0){
				if(node_->pointer_) 
					node_->pointer_->node_ = 0;
				delete node_;
			}
		}
	}

	T* get() const { return node_ ? static_cast<T*>(node_->pointer_) : 0; }

	friend class WeakObject;
	template<class U> friend class WeakPtr;
};

template<class T>
bool YASLI_SERIALIZE_OVERRIDE(Archive& ar, SharedPtr<T>& ptr, const char* name, const char* label);

template<class T>
bool YASLI_SERIALIZE_OVERRIDE(Archive& ar, PolyPtr<T>& ptr, const char* name, const char* label);

template<class T>
bool YASLI_SERIALIZE_OVERRIDE(Archive& ar, AsObjectWrapper<SharedPtr<T> >& ptr, const char* name, const char* label);

}

#include "PointersImpl.h"
