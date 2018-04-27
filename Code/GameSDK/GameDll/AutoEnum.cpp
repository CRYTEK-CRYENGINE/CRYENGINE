// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   Description:
   Macros for automatically building enumerations and matching char* arrays
   -------------------------------------------------------------------------
   History:
   - 6:11:2009: Created by Tim Furnish

*************************************************************************/

#include "StdAfx.h"
#include "AutoEnum.h"
#include "Utility/StringUtils.h"

#define DO_PARSE_BITFIELD_STRING_LOGS 0

TBitfield AutoEnum_GetBitfieldFromString(const char* inString, const char** inArray, int arraySize)
{
	unsigned int reply = 0;

	if (inString && inString[0] != '\0') // Avoid a load of work if the string's NULL or empty
	{
		const char* szStartFrom = inString;

		assert(arraySize > 0);

		char skipThisString[32];
		size_t skipChars = cry_copyStringUntilFindChar(skipThisString, inArray[0], sizeof(skipThisString), '_');
		size_t foundAtIndex = 0;

#if DO_PARSE_BITFIELD_STRING_LOGS
		CryLog("AutoEnum_GetBitfieldFromString: Parsing '%s' (skipping first %d chars '%s%s' of each string in array)", inString, skipChars, skipThisString, skipChars ? "_" : "");
#endif

		do
		{
			char gotToken[32];
			foundAtIndex = cry_copyStringUntilFindChar(gotToken, szStartFrom, sizeof(gotToken), '|');
			szStartFrom += foundAtIndex;

			bool bDone = false;
			for (int i = 0; i < arraySize; ++i)
			{
				if (0 == stricmp(inArray[i] + skipChars, gotToken))
				{
					CRY_ASSERT_MESSAGE((reply & BIT(i)) == 0, string().Format("Bit '%s' already turned on! Does it feature more than once in string '%s'?", gotToken, inString));

#if DO_PARSE_BITFIELD_STRING_LOGS
					CryLog("AutoEnum_GetBitfieldFromString: Token = '%s' = BIT(%d) = %d, remaining string = '%s'", gotToken, i, BIT(i), foundAtIndex ? szStartFrom : "");
#endif

					reply |= BIT(i);
					bDone = true;
					break;
				}
			}
			CRY_ASSERT_MESSAGE(bDone, string().Format("No flag called '%s' in list", gotToken));
		}
		while (foundAtIndex);
	}

	return reply;
}

bool AutoEnum_GetEnumValFromString(const char* inString, const char** inArray, int arraySize, int* outVal)
{
	bool done = false;

	if (inString && (inString[0] != '\0'))  // avoid a load of work if the string's NULL or empty
	{
		CRY_ASSERT(arraySize > 0);

		char skipThisString[32];
		size_t skipChars = cry_copyStringUntilFindChar(skipThisString, inArray[0], sizeof(skipThisString), '_');

#if DO_PARSE_BITFIELD_STRING_LOGS
		CryLog("AutoEnum_GetEnumValFromString: Parsing '%s' (skipping first %d chars '%s%s' of each string in array)", inString, skipChars, skipThisString, skipChars ? "_" : "");
#endif

		for (int i = 0; i < arraySize; i++)
		{
#if DO_PARSE_BITFIELD_STRING_LOGS
			CryLog("AutoEnum_GetEnumValFromString: Searching... Enum val %d = '%s'", i, (inArray[i] + skipChars));
#endif
			if (0 == stricmp((inArray[i] + skipChars), inString))
			{
#if DO_PARSE_BITFIELD_STRING_LOGS
				CryLog("AutoEnum_GetEnumValFromString: Flag '%s' found in enum list as value %d", inString, i);
#endif
				if (outVal)
					(*outVal) = i;
				done = true;
				break;
			}
		}
		CRY_ASSERT_MESSAGE(done, string().Format("No flag called '%s' in enum list", inString));
	}

	return done;
}

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
string AutoEnum_GetStringFromBitfield(TBitfield bitfield, const char** inArray, int arraySize)
{
	string output;
	TBitfield checkThis = 1;

	assert(arraySize > 0);

	char skipThisString[32];
	size_t skipChars = cry_copyStringUntilFindChar(skipThisString, inArray[0], sizeof(skipThisString), '_');

	for (int i = 0; i < arraySize; ++i, checkThis <<= 1)
	{
		if (bitfield & checkThis)
		{
			if (!output.empty())
			{
				output.append("|");
			}
			output.append(inArray[i] + skipChars);
		}
	}

	return string().Format("%s%s", output.empty() ? "none" : output.c_str(), (bitfield >= checkThis) ? ", invalid bits found!" : "");
}
#endif
