// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CrySchematyc/Reflection/TypeDesc.h>

#include "UnitTests/UnitTestRegistrar.h"
#include "CrySchematyc/Reflection/ReflectionUtils.h"

namespace Schematyc
{
namespace ReflectionUnitTests
{

enum class EType
{
	BaseA,
	BaseB,
	BaseC
};

struct SBaseA
{
	inline SBaseA()
		: type(EType::BaseA)
	{}

	static void ReflectType(CTypeDesc<SBaseA>& desc)
	{
		desc.SetGUID("f4c516fa-05be-4fdd-86f6-9bef1deeec8d"_cry_guid);
	}

	EType type;
};

struct SBaseB
{
	inline SBaseB()
		: type(EType::BaseB)
	{}

	static void ReflectType(CTypeDesc<SBaseB>& desc)
	{
		desc.SetGUID("c8b8de47-1fff-4ec9-8a45-462086611dcb"_cry_guid);
	}

	EType type;
};

struct SBaseC : public SBaseA, public SBaseB
{
	inline SBaseC()
		: type(EType::BaseC)
	{}

	static void ReflectType(CTypeDesc<SBaseC>& desc)
	{
		desc.SetGUID("618307d6-a7c5-4fd4-8859-db67ee998778"_cry_guid);
		desc.AddBase<SBaseA>();
		desc.AddBase<SBaseB>();
	}

	EType type;
};

struct SDerived : public SBaseC
{
	static void ReflectType(CTypeDesc<SDerived>& desc)
	{
		desc.SetGUID("660a3811-7c0c-450e-bd41-5f375cd11771"_cry_guid);
		desc.AddBase<SBaseC>();
	}
};

UnitTestResultFlags Run()
{
	SDerived derived;

	SBaseA* pBaseA = DynamicCast<SBaseA>(&derived);
	if(!pBaseA || (pBaseA->type != EType::BaseA))
	{
		return EUnitTestResultFlags::FatalError;
	}
	
	SBaseB* pBaseB = DynamicCast<SBaseB>(&derived);
	if (!pBaseB || (pBaseB->type != EType::BaseB))
	{
		return EUnitTestResultFlags::FatalError;
	}

	SBaseC* pBaseC = DynamicCast<SBaseC>(&derived);
	if (!pBaseC || (pBaseC->type != EType::BaseC))
	{
		return EUnitTestResultFlags::FatalError;
	}

	return EUnitTestResultFlags::Success;
}

} // ReflectionUnitTests

SCHEMATYC_REGISTER_UNIT_TEST(&ReflectionUnitTests::Run, "Reflection")

} // Schematyc
