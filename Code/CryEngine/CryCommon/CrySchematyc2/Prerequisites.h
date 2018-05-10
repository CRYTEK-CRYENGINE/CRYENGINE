// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <limits>

#include <CryEntitySystem/IEntity.h>
#include <CryNetwork/INetwork.h>
#include <CrySerialization/ClassFactory.h>
#include <CrySerialization/Enum.h>
#include <CrySerialization/Forward.h>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/Math.h>
#include <CrySerialization/STL.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_ArrayView.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_PreprocessorUtils.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_ScopedConnection.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_TypeWrapper.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_Signal.h>
#include <CrySchematyc2/GUID.h>

#ifdef _RELEASE
#define SCHEMATYC2_DEBUG_BREAK
#else
#define SCHEMATYC2_DEBUG_BREAK CryDebugBreak();
#endif

#define SCHEMATYC2_ASSERTS_ENABLED 1

#define SCHEMATYC2_FILE_NAME __FILE__

#if defined(SCHEMATYC2_EXPORTS)
#define SCHEMATYC2_API __declspec(dllexport)
#else
#define SCHEMATYC2_API __declspec(dllimport)
#endif

// Copied from CryTpeInfo.cpp.
// #SchematycTODO : Find a better home for this?
#if defined(LINUX) || defined(APPLE) || defined(ORBIS)

inline char* SCHEMATYC2_I64TOA(int64 value, char* szOutput, int32 radix)
{
	if(radix == 10)
	{
		sprintf(szOutput, "%llu", static_cast<unsigned long long>(value));
	}
	else
	{
		sprintf(szOutput, "%llx", static_cast<unsigned long long>(value));
	}
	return szOutput;
}

inline char* SCHEMATYC2_ULTOA(uint32 value, char* szOutput, int32 radix)
{
	if(radix == 10)
	{
		sprintf(szOutput, "%.d", value);
	}
	else
	{
		sprintf(szOutput, "%.x", value);
	}
	return szOutput;
}

#else

#define SCHEMATYC2_I64TOA _i64toa
#define SCHEMATYC2_ULTOA  ultoa

#endif

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif
