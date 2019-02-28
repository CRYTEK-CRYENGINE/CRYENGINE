// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/StaticInstanceList.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_PreprocessorUtils.h>

#define SCHEMATYC2_REGISTER_UNIT_TEST(function, name) static Schematyc2::CUnitTestRegistrar PP_JOIN_XY(schematycUnitTestRegistrar, __COUNTER__)(function, name);

namespace Schematyc2
{
	enum class EUnitTestResultFlags
	{
		Success       = 0,
		CriticalError = BIT(0),
		FatalError    = BIT(1)
	};

	DECLARE_ENUM_CLASS_FLAGS(EUnitTestResultFlags)

	typedef EUnitTestResultFlags (*UnitTestFunctionPtr)(); 

	class CUnitTestRegistrar : public CStaticInstanceList<CUnitTestRegistrar>
	{
	public:

		CUnitTestRegistrar(UnitTestFunctionPtr pFuntion, const char* szName);

		static void RunUnitTests();

	private:

		UnitTestFunctionPtr m_pFuntion;
		const char*         m_szName;
	};
}