/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once

#include <map>
#include <CrySerialization/yasli/Assert.h>

template<class _Base>
struct Constructor0{
    template<class _Derived>
    void construct(_Derived** ptr) const
    {
        *ptr = new _Derived;
    }
};

template<class _Base, class _Arg1>
struct Constructor1{
    Constructor1(_Arg1 arg1)
    : arg1_(arg1)
    {}

    template<class _Derived>
    void construct(_Derived** ptr) const{
        *ptr = new _Derived(arg1_);
    }
protected:
    _Arg1 arg1_;
};

template<class _Base, class _Arg1, class _Arg2>
struct Constructor2{
    Constructor2(_Arg1 arg1, _Arg2 arg2)
    : arg1_(arg1)
    , arg2_(arg2)
    {}

    template<class _Derived>
    void construct(_Derived** ptr) const{
        *ptr = new _Derived(arg1_, arg2_);
    }
protected:
    _Arg1 arg1_;
    _Arg2 arg2_;
};

template<class _Base, class _Arg1, class _Arg2, class _Arg3>
struct Constructor3{
    Constructor3(_Arg1 arg1, _Arg2 arg2, _Arg3 arg3)
    : arg1_(arg1)
    , arg2_(arg2)
    , arg3_(arg3)
    {}

    template<class _Derived>
    void construct(_Derived** ptr) const{
        *ptr = new _Derived(arg1_, arg2_, arg3_);
    }
protected:
    _Arg1 arg1_;
    _Arg2 arg2_;
    _Arg3 arg3_;
};

template<class _Key, class _Product, class _Constructor = Constructor0<_Product>, class _KeyPred = std::less<_Key> >
class Factory{
public:
    typedef _Constructor Constructor;
    struct CreatorBase{
        virtual ~CreatorBase() {}
        virtual _Product* create(Constructor ctor) const = 0;
    };
    typedef std::map<_Key, CreatorBase*, _KeyPred> Creators;

    template<class _Derived>
    struct Creator : CreatorBase{
        Creator(const _Key& key, Factory& factory){
			factory.add(key, this);
        }
        // virtuals:
        _Product* create(_Constructor ctor) const {
            _Derived* result;
            ctor.construct(&result);
            return result;
        }
        // ^^^
    };

    void add(const _Key& key, CreatorBase *const creator){
        YASLI_ASSERT(creators_.find(key) == creators_.end());
        YASLI_ASSERT(creator);
        creators_[key] = creator;
    }

    _Product* create(const _Key& key, Constructor ctor = Constructor()) const{
        typename Creators::const_iterator it = creators_.find(key);
        if(it != creators_.end()){
            return it->second->create(ctor);
        }
        else
            return 0;
    }

    template<class Arg1>
    _Product* create(const _Key& key, Arg1 arg1) const{
        typename Creators::const_iterator it = creators_.find(key);
        if(it != creators_.end()){
            return it->second->create(Constructor(arg1));
        }
        else
            return 0;
    }

    template<class Arg1, class Arg2>
    _Product* create(const _Key& key, Arg1 arg1, Arg2 arg2) const{
        typename Creators::const_iterator it = creators_.find(key);
        if(it != creators_.end()){
            return it->second->create(Constructor(arg1, arg2));
        }
        else
            return 0;
    }

    std::size_t size() const { return creators_.size(); }

    _Product* createByIndex(int index, Constructor ctor = Constructor()) const{
        YASLI_ASSERT(index >= 0 && index < creators_.size());
        typename Creators::const_iterator it = creators_.begin();
        std::advance(it, index);
        return it->second->create(ctor);
    }


    const Creators& creators() const { return creators_; }

    static Factory& the()
    {
        static Factory genericFactory;
        return genericFactory;
    }
protected:
    Factory() {}
    Creators creators_;
};

#define REGISTER_IN_PROPERTY_ROW_FACTORY(key, product)              \
	static PropertyRowFactory::Creator<product> PropertyRowFactory##product##Creator(key, globalPropertyRowFactory()); 

#define DECLARE_SEGMENT(fileName) int dataSegment##fileName; 

#define FORCE_SEGMENT(fileName) \
	extern int dataSegment##fileName; \
	int* dataSegmentPtr##fileName = &dataSegment##fileName;


