// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#define SCHEMATYC_DELEGATE(functionPtr) (functionPtr)
#define SCHEMATYC_MEMBER_DELEGATE(functionPtr, object) Schematyc::MemberDelegate<decltype(functionPtr)>::Make(&object,functionPtr)

namespace Schematyc
{
template<typename T>
struct MemberDelegate {};

template<typename OBJECT_TYPE, typename RETURN_TYPE, typename ... PARAM_TYPES>
struct MemberDelegate<RETURN_TYPE(OBJECT_TYPE::*)(PARAM_TYPES ...)>
{
	static std::function<RETURN_TYPE(PARAM_TYPES ...)> Make(OBJECT_TYPE* t, RETURN_TYPE(OBJECT_TYPE::*f)(PARAM_TYPES...))
	{
		return [=](PARAM_TYPES... v) { return (t->*f)(std::forward<decltype(v)>(v)...); };
	}
};
} // Schematyc
