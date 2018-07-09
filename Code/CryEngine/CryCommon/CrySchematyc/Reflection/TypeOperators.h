// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/IArchive.h>

#include "CrySchematyc/Utils/IString.h"

namespace Schematyc
{

struct STypeOperators
{
	typedef void (* DefaultConstruct)(void* pPlacement);
	typedef void (* Destruct)(void* pPlacement);
	typedef void (* CopyConstruct)(void* pPlacement, const void* pRHS);
	typedef void (* CopyAssign)(void* pLHS, const void* pRHS);
	typedef bool (* Equals)(const void* pLHS, const void* pRHS);
	typedef bool (* Serialize)(Serialization::IArchive& archive, void* pValue, const char* szName, const char* szLabel);
	typedef void (* ToString)(IString& output, const void* pInput);

	DefaultConstruct defaultConstruct = nullptr;
	Destruct         destruct = nullptr;
	CopyConstruct    copyConstruct = nullptr;
	CopyAssign       copyAssign = nullptr;
	Equals           equals = nullptr;
	Serialize        serialize = nullptr;
	ToString         toString = nullptr;
};

struct SStringOperators
{
	typedef const char*(* GetChars)(const void* pString);

	GetChars getChars = nullptr;
};

struct SArrayOperators
{
	typedef uint32 (*     Size)(const void* pArray);
	typedef void*(*       At)(void* pArray, uint32 pos);
	typedef const void*(* AtConst)(const void* pArray, uint32 pos);
	typedef void (*       PushBack)(void* pArray, const void* pValue);

	Size     size = nullptr;
	At       at = nullptr;
	AtConst  atConst = nullptr;
	PushBack pushBack = nullptr;
};

} // Schematyc

#include "CrySchematyc/Reflection/TypeOperators.inl"
