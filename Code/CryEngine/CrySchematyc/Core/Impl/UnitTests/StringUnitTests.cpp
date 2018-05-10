// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CrySchematyc/Utils/StackString.h>
#include <CrySchematyc/Utils/SharedString.h>

#include "UnitTests/UnitTestRegistrar.h"

namespace Schematyc
{
	namespace StringUnitTests
	{
		bool TestString(IString& value)
		{
			{
				value.assign("abcdef");
				if(strcmp(value.c_str(), "abcdef") != 0)
				{
					return false;
				}
				value.clear();
			}

			{
				const char* szValue = "_abcghi_";
				value.assign(szValue, 1, 6);
				value.insert(3, "def");
				if(strcmp(value.c_str(), "abcdefghi") != 0)
				{
					return false;
				}
				value.clear();
			}
			
			return true;
		}

		UnitTestResultFlags TestStrings()
		{
			{
				CStackString value;
				if (!TestString(value))
				{
					return EUnitTestResultFlags::FatalError;
				}
			}

			{
				CSharedString value;
				if (!TestString(value))
				{
					return EUnitTestResultFlags::FatalError;
				}
			}

			return EUnitTestResultFlags::Success;
		}

		UnitTestResultFlags Run()
		{
			UnitTestResultFlags resultFlags = EUnitTestResultFlags::Success;

			resultFlags.Add(TestStrings());

			return resultFlags;
		}
	}

	SCHEMATYC_REGISTER_UNIT_TEST(&StringUnitTests::Run, "String")
}
