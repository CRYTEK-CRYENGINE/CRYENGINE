// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   config.h
//  Version:     v1.00
//  Created:     4/11/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __config_h__
#define __config_h__
#pragma once

#include "iconfig.h"
#include <CryCore/StlUtils.h>
#include "StringHelpers.h"

class CPropertyVars;

/** Implementation of IConfig interface.
*/
class Config 
	: public IConfig
{
public:
	Config();
	virtual ~Config();

	void SetConfigKeyRegistry(IConfigKeyRegistry* pConfigKeyRegistry);
	IConfigKeyRegistry* GetConfigKeyRegistry() const;

	// interface IConfigSink ------------------------------------------

	virtual void SetKeyValue(EConfigPriority ePri, const char* key, const char* value);

	// interface IConfig ----------------------------------------------

	virtual void Release() { delete this; };
	virtual const Config* GetInternalRepresentation() const;
	virtual bool HasKeyRegistered(const char* key) const;
	virtual bool HasKeyMatchingWildcards(const char* wildcards) const;
	virtual bool GetKeyValue(const char* key, const char* & value, int ePriMask) const;
	virtual int GetSum(const char* key) const;
	virtual void GetUnknownKeys(std::vector<string>& unknownKeys) const;
	virtual void AddConfig(const IConfig* inpConfig);
	virtual void Clear();
	virtual uint32 ClearPriorityUsage(int ePriMask);
	virtual uint32 CountPriorityUsage(int ePriMask) const;
	virtual void CopyToConfig(EConfigPriority ePri, IConfigSink* pDestConfig) const;
	virtual void CopyToPropertyVars(CPropertyVars& properties) const;

private: // ---------------------------------------------------------------------

	struct MapKey
	{
		string m_sKeyName;
		EConfigPriority m_eKeyPri;

		bool operator<(const MapKey& b) const
		{
			// sort by m_sKeyName
			const int cmp = StringHelpers::CompareIgnoreCase(m_sKeyName, b.m_sKeyName);
			if (cmp != 0)
			{
				return (cmp < 0);
			}

			// sort by m_eKeyPri (higher priority goes first)
			return (m_eKeyPri > b.m_eKeyPri);
		}
	};

	typedef std::map<MapKey, string> Map;

	Map m_map;
	IConfigKeyRegistry* m_pConfigKeyRegistry;   // used to verify key registration
};

#endif // __config_h__
