// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   iconfig.h
//  Version:     v1.00
//  Created:     4/11/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __iconfig_h__
#define __iconfig_h__
#pragma once

#include "Util.h"     // BIT()

class Config;
class CPropertyVars;

enum EConfigPriority
{
	// low priorities first

	eCP_PriorityLowest   = BIT(0),     // used internally
								 
	eCP_PriorityFile     = BIT(1),     // per-file settings
	eCP_PriorityPreset   = BIT(2),     // settings from section [<preset_name>] of rc.ini
	eCP_PriorityRcIni    = BIT(3),     // settings from rc.ini (common + per-platform)
	eCP_PriorityCmdline  = BIT(4),     // rc.exe's command line
	eCP_PriorityProperty = BIT(5),     // settings from RCJob XML properties
	eCP_PriorityJob      = BIT(6),     // per-job configuration
								 
	eCP_PriorityHighest  = BIT(7),     // used internally

	eCP_PriorityAll = eCP_PriorityHighest + (eCP_PriorityHighest - 1)   // binary OR of all possible priority values
};


class IConfigSink
{
public:
	virtual ~IConfigSink()
	{
	}

	// Arguments:
	//   value - can be 0 to delete a key, can be "" to set a key without value (e.g. /refresh)
	virtual void SetKeyValue(EConfigPriority ePri, const char* key, const char* value) = 0;
};


/** Configuration options interface.
*/
class IConfig
	: public IConfigSink
{
public:
	// Delete instance of configuration class.
	virtual void Release() = 0;

	virtual const Config* GetInternalRepresentation() const = 0;

	virtual bool HasKeyRegistered(const char* key) const = 0;

	// Returns true if the config contains a key that matches 'wildcards' name.
	virtual bool HasKeyMatchingWildcards(const char* wildcards) const = 0;

	// Get pointer to ASCIIZ value of a key. The pointer is not modified if the key is not found.
	// Returns:
	//   true if key found, false if not.
	virtual bool GetKeyValue(const char* key, const char* & value, int ePriMask=eCP_PriorityAll) const = 0;

	// Get sum of all values of a key.
	// Returns:
	//   0 if key is not found or has no value
	virtual int GetSum(const char* key) const = 0;

	// Find unknown keys in configuration.
	virtual void GetUnknownKeys(std::vector<string>& unknownKeys) const = 0;

	// Merge configuration.
	virtual void AddConfig( const IConfig *pCfg ) = 0;

	virtual void Clear() = 0;
	virtual uint32 ClearPriorityUsage(int ePriMask) = 0;
	virtual uint32 CountPriorityUsage(int ePriMask) const = 0;

	virtual void CopyToConfig(EConfigPriority ePri, IConfigSink* pDestConfig) const = 0;

	virtual void CopyToPropertyVars(CPropertyVars& properties) const = 0;

	//////////////////////////////////////////////////////////////////////////

	// Check if configuration has a key
	bool HasKey(const char* key, int ePriMask=eCP_PriorityAll) const;

	// Get value of a key
	bool GetAsBool(const char* const key, const bool keyIsNotFoundValue, const bool emptyOrBadValue, int ePriMask=eCP_PriorityAll) const;
	int GetAsInt(const char* const key, const int keyIsNotFoundValue, const int emptyOrBadValue, int ePriMask=eCP_PriorityAll) const;
	float GetAsFloat(const char* const key, const float keyIsNotFoundValue, const float emptyOrBadValue, int ePriMask=eCP_PriorityAll) const;
	string GetAsString(const char* const key, const char* const keyIsNotFoundValue, const char* const emptyOrBadValue, int ePriMask=eCP_PriorityAll) const;

	// str - e.g. "/reduce=2 /space=tangent"
	void SetFromString(EConfigPriority ePri, const char* str);

	static bool IsValidNameChar(const unsigned char c)
	{
		return (c > ' ') && (c != '=') && (c != ';') && (c != ':') && (c != '/');
	}

private:
	// Get value of a key. The value (passed by reference) is not modified if
	// the key is not found or the value attached to the key is empty or bad.
	enum EResult
	{
		eResult_Success,
		eResult_KeyIsNotFound,
		eResult_ValueIsEmptyOrBad,
	};
	EResult Get(const char *key, bool& value, int ePriMask) const;
	EResult Get(const char* key, int& value, int ePriMask) const;
	EResult Get(const char* key, float& value, int ePriMask) const;
	EResult Get(const char* key, string& value, int ePriMask) const;
};


class IConfigKeyRegistry
{
public:
	virtual ~IConfigKeyRegistry()
	{
	}

	virtual void VerifyKeyRegistration(const char* szKey) const = 0;
	virtual bool HasKeyRegistered(const char* szKey) const = 0;
};


#endif // __iconfig_h__
