// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CrySchematyc/Runtime/RuntimeParams.h>
#include <CrySchematyc/Utils/Scratchpad.h>
#include <CrySchematyc/Utils/SharedString.h>

#include "UnitTests/UnitTestRegistrar.h"

namespace Schematyc
{
namespace RuntimeUnitTests
{

UnitTestResultFlags TestScratchpad()
{
	const int32 valueA = 101;
	const float valueB = 202.0f;
	const CSharedString valueC = "303";

	StackScratchpad stackScratchpad;

	const uint32 posA = stackScratchpad.Add(CAnyConstRef(valueA));
	const uint32 posB = stackScratchpad.Add(CAnyConstRef(valueB));
	const uint32 posC = stackScratchpad.Add(CAnyConstRef(valueC));

	HeapScratchpad heapScratchpad(stackScratchpad);

	const int32* pA = DynamicCast<int32>(heapScratchpad.Get(posA));
	if (!pA || (*pA != valueA))
	{
		return EUnitTestResultFlags::FatalError;
	}

	const float* pB = DynamicCast<float>(heapScratchpad.Get(posB));
	if (!pB || (*pB != valueB))
	{
		return EUnitTestResultFlags::FatalError;
	}

	const CSharedString* pC = DynamicCast<CSharedString>(heapScratchpad.Get(posC));
	if (!pC || (*pC != valueC))
	{
		return EUnitTestResultFlags::FatalError;
	}

	return EUnitTestResultFlags::Success;
}

UnitTestResultFlags TestRuntimeParams()
{
	const int32 valueA = 101;
	const float valueB = 202.0f;
	const CSharedString valueC = "303";

	StackRuntimeParams stackRuntimeParams;

	stackRuntimeParams.AddInput(CUniqueId::FromUInt32('a'), valueA);
	stackRuntimeParams.AddInput(CUniqueId::FromUInt32('b'), valueB);
	stackRuntimeParams.AddInput(CUniqueId::FromUInt32('c'), valueC);

	HeapRuntimeParams heapRuntimeParams(stackRuntimeParams);

	const int32* pA = DynamicCast<int32>(heapRuntimeParams.GetInput(CUniqueId::FromUInt32('a')));
	if (!pA || (*pA != valueA))
	{
		return EUnitTestResultFlags::FatalError;
	}

	const float* pB = DynamicCast<float>(heapRuntimeParams.GetInput(CUniqueId::FromUInt32('b')));
	if (!pB || (*pB != valueB))
	{
		return EUnitTestResultFlags::FatalError;
	}

	const CSharedString* pC = DynamicCast<CSharedString>(heapRuntimeParams.GetInput(CUniqueId::FromUInt32('c')));
	if (!pC || (*pC != valueC))
	{
		return EUnitTestResultFlags::FatalError;
	}

	return EUnitTestResultFlags::Success;
}

UnitTestResultFlags Run()
{
	UnitTestResultFlags resultFlags = EUnitTestResultFlags::Success;
	resultFlags.Add(TestScratchpad());
	resultFlags.Add(TestRuntimeParams());
	return resultFlags;
}

} // RuntimeUnitTests

SCHEMATYC_REGISTER_UNIT_TEST(&RuntimeUnitTests::Run, "Runtime")

} // Schematyc
