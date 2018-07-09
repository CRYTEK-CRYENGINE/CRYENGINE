// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <functional>

namespace yasli
{

struct CallbackInterface
{
	virtual bool serializeValue(Archive& ar, const char* name, const char* value) = 0;
	virtual CallbackInterface* clone() = 0;
	virtual void release() = 0;
	virtual TypeID type() const = 0;

	typedef std::function<void(void*, const TypeID&)> ApplyFunction;
	virtual void call(const ApplyFunction&) = 0;
};

template<class T, class Decorator=T>
struct CallbackSimple : CallbackInterface
{
	typedef std::function<void(const T&)> CallbackFunction;
	T* value;
	T oldValue;
	CallbackFunction callback;

	CallbackSimple(T* value, const T& oldValue, const std::function<void(const T&)>& callback)
	: value(value)
	, oldValue(oldValue)
	, callback(callback)
	{
	}

	CallbackInterface* clone() { return new CallbackSimple<T>(0, oldValue, callback);	}
	void release() { delete this; }
	bool serializeValue(Archive& ar, const char* name, const char* label) { return ar(*value, name, label); }
	TypeID type() const{ return TypeID::get<T>(); }

	void call(const ApplyFunction& applyFunction)
	{
		T newValue;
		applyFunction((void*)&newValue, TypeID::get<T>());
		if (oldValue != newValue) {
			callback(newValue);
			oldValue = newValue;
		}
	}

};

template<class T, class Decorator=T>
struct CallbackWithDecorator: CallbackInterface
{
	typedef std::function<void(const T&)> CallbackFunction;
	typedef std::function<Decorator (T&)> DecoratorFunction;

	T oldValue;
	T* value;
	CallbackFunction callback;
	DecoratorFunction decorator;

	CallbackWithDecorator(T* value, 
					 const T& oldValue,
					 const CallbackFunction& callback,
					 const DecoratorFunction& decorator)
	: value(value)
	, oldValue(oldValue)
	, callback(callback)
	, decorator(decorator)
	{
	}

	CallbackInterface* clone() { return new CallbackWithDecorator<T, Decorator>(0, oldValue, callback, decorator); }
	void release() { delete this;	}
	bool serializeValue(Archive& ar, const char* name, const char* label) { return ar(decorator(*value), name, label);	}
	TypeID type() const{ return TypeID::get<Decorator>(); }

	void call(const ApplyFunction& applyFunction)
	{
		T newValue;
		Decorator dec = decorator(newValue);
		applyFunction((void*)&dec, TypeID::get<Decorator>());
		if (oldValue != newValue)
		{
			callback(newValue);
			oldValue = newValue;
		}
	}
};



namespace Detail
{

template <typename T>
struct MethodReturnType { typedef void type; };

template <typename ClassType, typename ReturnType, typename Arg0>
struct MethodReturnType<ReturnType(ClassType::*)(Arg0) const> { typedef ReturnType type; };

template<class T>
struct OperatorBracketsReturnType
{
	typedef typename MethodReturnType<decltype(&T::operator())>::type Type;
};

}

template<class T, class CallbackFunc>
CallbackSimple<T> 
Callback(T& value, const CallbackFunc& callback)
{
	return CallbackSimple<T>(&value, value, std::function<void(const T&)>(callback));
}


template<class T, class CallbackFunc, class DecoratorFunc>
CallbackWithDecorator<T, typename Detail::OperatorBracketsReturnType<DecoratorFunc>::Type> 
Callback(T& value, const CallbackFunc& callback, const DecoratorFunc& decorator)
{
	typedef typename Detail::OperatorBracketsReturnType<DecoratorFunc>::Type Decorator;
	return CallbackWithDecorator<T, Decorator>(&value, value,
																						 std::function<void(const T&)>(callback),
																						 std::function<Decorator(T&)>(decorator));
}


template<class T>
bool YASLI_SERIALIZE_OVERRIDE(Archive& ar, CallbackSimple<T>& callback, const char* name, const char* label)
{
	if (ar.isEdit())
		return ar(static_cast<CallbackInterface&>(callback), name, label);
	else
	{
		if (!ar(*callback.value, name, label))
			return false;
		return true;
	}
}

template<class T, class Decorator>
bool YASLI_SERIALIZE_OVERRIDE(Archive& ar, CallbackWithDecorator<T, Decorator>& callback, const char* name, const char* label)
{
	if (ar.isEdit())
		return ar(static_cast<CallbackInterface&>(callback), name, label);
	else
	{
		if (!ar(*callback.value, name, label))
			return false;
		return true;
	}
}


}
