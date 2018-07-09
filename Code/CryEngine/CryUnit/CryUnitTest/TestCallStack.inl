// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <CrySystem/CryUnitTest.h>
#include "../CallStackBase.h"

CRY_TEST_WITH_FIXTURE(TestCallStackElementDefaultValues, TestCryUnitFixture)
{
	CryUnit::CallStackElement callStackElement;

	ASSERT_ARE_EQUAL("Unknown", callStackElement.GetFunctionName());
	ASSERT_ARE_EQUAL("Unknown", callStackElement.GetModuleName());
	ASSERT_ARE_EQUAL("Unknown", callStackElement.GetFileName());
	ASSERT_ARE_EQUAL(0, callStackElement.GetFileLineNumber());
}

CryUnit::CallStackElement CreateCallStackElement(const char* functionName, const char* fileName)
{
	CryUnit::CallStackElement callStackElement;

	callStackElement.SetFunctionName(functionName);
	callStackElement.SetFileName(fileName);

	return callStackElement;
}

CryUnit::CallStackElement CreateCallStackElement(const char* functionName)
{
	CryUnit::CallStackElement callStackElement;

	callStackElement.SetFunctionName(functionName);

	return callStackElement;
}

CRY_TEST_WITH_FIXTURE(TestCallStackElementFunctionName, TestCryUnitFixture)
{
	CryUnit::CallStackElement callStackElement = CreateCallStackElement("FunctionName");

	ASSERT_ARE_EQUAL("FunctionName", callStackElement.GetFunctionName());
}

CRY_TEST_WITH_FIXTURE(TestCallStackElementModuleName, TestCryUnitFixture)
{
	CryUnit::CallStackElement callStackElement;

	callStackElement.SetModuleName("ModuleName");
	ASSERT_ARE_EQUAL("ModuleName", callStackElement.GetModuleName());
}

CRY_TEST_WITH_FIXTURE(TestCallStackElementFileName, TestCryUnitFixture)
{
	CryUnit::CallStackElement callStackElement;
	callStackElement.SetFileName("FileName");
	ASSERT_ARE_EQUAL("FileName", callStackElement.GetFileName());
}

CRY_TEST_WITH_FIXTURE(TestCallStackElementFileLineNumber, TestCryUnitFixture)
{
	CryUnit::CallStackElement callStackElement;

	callStackElement.SetFileLineNumber(12);
	ASSERT_ARE_EQUAL(12, callStackElement.GetFileLineNumber());
}

CRY_TEST_WITH_FIXTURE(TestCallStackElementIsUnknownByDefault, TestCryUnitFixture)
{
	CryUnit::CallStackElement callStackElement;
	ASSERT_IS_TRUE(callStackElement.IsUnknown());
}

CRY_TEST_WITH_FIXTURE(TestCallStackElementIsUnknownIfUnknownFunctionName, TestCryUnitFixture)
{
	CryUnit::CallStackElement callStackElement;
	callStackElement.SetModuleName("module");
	callStackElement.SetFileName("fileName");
	ASSERT_IS_TRUE(callStackElement.IsUnknown());
}

CRY_TEST_WITH_FIXTURE(TestCallStackElementIsknownIfUnknownModule, TestCryUnitFixture)
{
	CryUnit::CallStackElement callStackElement = CreateCallStackElement("function", "fileName");
	ASSERT_IS_FALSE(callStackElement.IsUnknown());
}

CRY_TEST_WITH_FIXTURE(TestCallStackElementIsUnknownIfUnknownFileName, TestCryUnitFixture)
{
	CryUnit::CallStackElement callStackElement;
	callStackElement.SetFunctionName("function");
	callStackElement.SetModuleName("module");
	ASSERT_IS_TRUE(callStackElement.IsUnknown());
}

CRY_TEST_WITH_FIXTURE(TestCallStackIsEmptyByDefault, TestCryUnitFixture)
{
	CryUnit::CallStackBase callStack;
	ASSERT_ARE_EQUAL(0, callStack.GetSize());
}

CRY_TEST_WITH_FIXTURE(TestCallStackHasSizeOneWhenInsertingAnElement, TestCryUnitFixture)
{
	CryUnit::CallStackBase callStack;

	CryUnit::CallStackElement element = CreateCallStackElement("functionName", "fileName");
	callStack.Add(element);

	ASSERT_ARE_EQUAL(1, callStack.GetSize());
}

CRY_TEST_WITH_FIXTURE(TestCallStackHasSizeZeroWhenInsertingAnUnknownElement, TestCryUnitFixture)
{
	CryUnit::CallStackBase callStack;
	callStack.Add(CryUnit::CallStackElement());
	ASSERT_ARE_EQUAL(0, callStack.GetSize());
}

CRY_TEST_WITH_FIXTURE(TestCallStackHasTwoElements, TestCryUnitFixture)
{
	CryUnit::CallStackBase callStack;

	callStack.Add(CreateCallStackElement("function1", ""));
	callStack.Add(CreateCallStackElement("function2", ""));

	ASSERT_ARE_EQUAL(2, callStack.GetSize());
	ASSERT_ARE_EQUAL("function1", callStack.GetElementByIndex(0).GetFunctionName());
	ASSERT_ARE_EQUAL("function2", callStack.GetElementByIndex(1).GetFunctionName());
}

CRY_TEST_WITH_FIXTURE(TestFilterFirstElement, TestCryUnitFixture)
{
	CryUnit::CallStackBase callStack;

	callStack.Add(CreateCallStackElement("function1", ""));
	callStack.Add(CreateCallStackElement("function2", ""));

	callStack.FilterTopElements(1);

	ASSERT_ARE_EQUAL(1, callStack.GetSize());
	ASSERT_ARE_EQUAL("function2", callStack.GetElementByIndex(0).GetFunctionName());
}

CRY_TEST_WITH_FIXTURE(TestFilterFirstTwoElements, TestCryUnitFixture)
{
	CryUnit::CallStackBase callStack;

	callStack.Add(CreateCallStackElement("function1", ""));
	callStack.Add(CreateCallStackElement("function2", ""));
	callStack.Add(CreateCallStackElement("function3", ""));

	callStack.FilterTopElements(2);

	ASSERT_ARE_EQUAL(1, callStack.GetSize());
	ASSERT_ARE_EQUAL("function3", callStack.GetElementByIndex(0).GetFunctionName());
}

CRY_TEST_WITH_FIXTURE(TestFilterFirstTwoElementsWhenSizeIsTwo, TestCryUnitFixture)
{
	CryUnit::CallStackBase callStack;

	callStack.Add(CreateCallStackElement("function1", ""));
	callStack.Add(CreateCallStackElement("function2", ""));

	callStack.FilterTopElements(2);

	ASSERT_ARE_EQUAL(0, callStack.GetSize());
}

CRY_TEST_WITH_FIXTURE(TestFilterFirstTwoElementsWhenSizeIsOne, TestCryUnitFixture)
{
	CryUnit::CallStackBase callStack;

	callStack.Add(CreateCallStackElement("function1", ""));

	callStack.FilterTopElements(2);

	ASSERT_ARE_EQUAL(0, callStack.GetSize());
}

CRY_TEST_WITH_FIXTURE(TestFilterFirstElementWhenSizeIsZero, TestCryUnitFixture)
{
	CryUnit::CallStackBase callStack;

	callStack.FilterTopElements(1);

	ASSERT_ARE_EQUAL(0, callStack.GetSize());
}

CRY_TEST_WITH_FIXTURE(TestFilterZeroElementWhenSizeIsZero, TestCryUnitFixture)
{
	CryUnit::CallStackBase callStack;

	callStack.FilterTopElements(0);

	ASSERT_ARE_EQUAL(0, callStack.GetSize());
}

CRY_TEST_WITH_FIXTURE(TestFilterZeroElementWhenSizeIsOne, TestCryUnitFixture)
{
	CryUnit::CallStackBase callStack;
	callStack.Add(CreateCallStackElement("function1", ""));

	callStack.FilterTopElements(0);

	ASSERT_ARE_EQUAL(1, callStack.GetSize());
}

CRY_TEST_WITH_FIXTURE(TestFilterBottomFirstElement, TestCryUnitFixture)
{
	CryUnit::CallStackBase callStack;

	callStack.Add(CreateCallStackElement("function1", ""));
	callStack.Add(CreateCallStackElement("function2", ""));

	callStack.FilterBottomElements(1);

	ASSERT_ARE_EQUAL(1, callStack.GetSize());
	ASSERT_ARE_EQUAL("function1", callStack.GetElementByIndex(0).GetFunctionName());
}

CRY_TEST_WITH_FIXTURE(TestFilterBottomFirstTwoElements, TestCryUnitFixture)
{
	CryUnit::CallStackBase callStack;

	callStack.Add(CreateCallStackElement("function1", ""));
	callStack.Add(CreateCallStackElement("function2", ""));
	callStack.Add(CreateCallStackElement("function3", ""));

	callStack.FilterBottomElements(2);

	ASSERT_ARE_EQUAL(1, callStack.GetSize());
	ASSERT_ARE_EQUAL("function1", callStack.GetElementByIndex(0).GetFunctionName());
}

CRY_TEST_WITH_FIXTURE(TestFilterBottomFirstTwoElementsWhenSizeIsTwo, TestCryUnitFixture)
{
	CryUnit::CallStackBase callStack;

	callStack.Add(CreateCallStackElement("function1", ""));
	callStack.Add(CreateCallStackElement("function2", ""));

	callStack.FilterBottomElements(2);

	ASSERT_ARE_EQUAL(0, callStack.GetSize());
}

CRY_TEST_WITH_FIXTURE(TestFilterBottomFirstTwoElementsWhenSizeIsOne, TestCryUnitFixture)
{
	CryUnit::CallStackBase callStack;

	callStack.Add(CreateCallStackElement("function1", ""));

	callStack.FilterBottomElements(2);

	ASSERT_ARE_EQUAL(0, callStack.GetSize());
}

CRY_TEST_WITH_FIXTURE(TestFilterBottomFirstElementWhenSizeIsZero, TestCryUnitFixture)
{
	CryUnit::CallStackBase callStack;

	callStack.FilterBottomElements(1);

	ASSERT_ARE_EQUAL(0, callStack.GetSize());
}

CRY_TEST_WITH_FIXTURE(TestFilterBottomZeroElementWhenSizeIsZero, TestCryUnitFixture)
{
	CryUnit::CallStackBase callStack;

	callStack.FilterBottomElements(0);

	ASSERT_ARE_EQUAL(0, callStack.GetSize());
}

CRY_TEST_WITH_FIXTURE(TestFilterBottomZeroElementWhenSizeIsOne, TestCryUnitFixture)
{
	CryUnit::CallStackBase callStack;
	callStack.Add(CreateCallStackElement("function1", ""));

	callStack.FilterBottomElements(0);

	ASSERT_ARE_EQUAL(1, callStack.GetSize());
}
