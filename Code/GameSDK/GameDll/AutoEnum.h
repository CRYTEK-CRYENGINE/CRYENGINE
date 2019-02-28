// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   Description:
   Macros for automatically building enumerations and matching char* arrays
   -------------------------------------------------------------------------
   History:
   - 15:07:2009: Created by Tim Furnish

*************************************************************************/

#pragma once

typedef uint32 TBitfield;

#define AUTOENUM_PARAM_1_COMMA(a, ...)                                            a,
#define AUTOENUM_PARAM_1_AS_STRING_COMMA(a, ...)                                  #a,
#define AUTOENUM_DO_BITINDEX(name, ...)                                           BITINDEX_ ## name,
#define AUTOENUM_DO_FLAG(name, ...)                                               name = BIT(BITINDEX_ ## name),
#define AUTOENUM_DO_FLAG_WITHBITSUFFIX(name, ...)                                 name ## _bit = BIT(BITINDEX_ ## name),

#define AUTOENUM_BUILDENUM(list)                                                  enum   {               list(AUTOENUM_PARAM_1_COMMA) }
#define AUTOENUM_BUILDENUMWITHTYPE(t, list)                                       enum t {               list(AUTOENUM_PARAM_1_COMMA) }
#define AUTOENUM_BUILDENUMWITHTYPE_WITHINVALID(t, list, invName)                  enum t { invName = -1, list(AUTOENUM_PARAM_1_COMMA) }
#define AUTOENUM_BUILDENUMWITHTYPE_WITHINVALID_WITHNUM(t, list, invName, numName) enum t { invName = -1, list(AUTOENUM_PARAM_1_COMMA) numName }
#define AUTOENUM_BUILDENUMWITHTYPE_WITHNUM(t, list, numName)                      enum t {               list(AUTOENUM_PARAM_1_COMMA) numName }
#define AUTOENUM_BUILDENUMWITHTYPE_WITHNUMEQUALS_WITHZERO(t, list, numName, num, zeroName) \
	enum t { zeroName = 0, list(AUTOENUM_PARAM_1_COMMA) numName = num }
#define AUTOENUM_BUILDNAMEARRAY(n, list)                                          const char* n[] = { list(AUTOENUM_PARAM_1_AS_STRING_COMMA) }
#define AUTOENUM_BUILDFLAGS_WITHZERO(list, zeroName)                              enum { zeroName = 0, list ## _neg1 = -1, list(AUTOENUM_DO_BITINDEX) list ## _numBits, list(AUTOENUM_DO_FLAG) }
#define AUTOENUM_BUILDFLAGS_WITHZERO_WITHBITSUFFIX(list, zeroName)                enum { zeroName = 0, list ## _neg1 = -1, list(AUTOENUM_DO_BITINDEX) list ## _numBits, list(AUTOENUM_DO_FLAG_WITHBITSUFFIX) }

TBitfield AutoEnum_GetBitfieldFromString(const char* inString, const char** inArray, int arraySize);
bool      AutoEnum_GetEnumValFromString(const char* inString, const char** inArray, int arraySize, int* outVal);

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
string AutoEnum_GetStringFromBitfield(TBitfield bitfield, const char** inArray, int arraySize);
#else
	#define AutoEnum_GetStringFromBitfield PLEASE_ONLY_CALL_AutoEnum_GetStringFromBitfield_IN_DEBUG_CODE
#endif
