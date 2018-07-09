// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "StlUtils.h"

template<class Object, class... Args>
struct Dispatcher: std::vector<Object*>
{
	typedef void (Object::*Function)(Args...);

	Dispatcher(Function func)
		: m_function(func) {}

	void add(Object* obj)
	{
		stl::push_back_unique(*this, obj);
	}
	void operator()(Args... args) const
	{
		for (auto obj : *this)
			(obj->*m_function)(args...);
	}

private:
	Function m_function;
};
