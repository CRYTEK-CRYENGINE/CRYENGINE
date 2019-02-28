// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Class that loads a config file with the ability to restore the
						 cvars to the values they were before loading them.

-------------------------------------------------------------------------
History:
- 25:07:2012  : Refactored into a class by Martin Sherburn

*************************************************************************/

#ifndef __REVERTIBLE_CONFIG_LOADER_H__
#define __REVERTIBLE_CONFIG_LOADER_H__

#pragma once

#include "Utility/SingleAllocTextBlock.h"

class CRevertibleConfigLoader : public ILoadConfigurationEntrySink
{
public:
	CRevertibleConfigLoader(int maxCvars, int maxTextBufferSize);
	~CRevertibleConfigLoader() {}

	/* ILoadConfigurationEntrySink */
	virtual void OnLoadConfigurationEntry( const char *szKey, const char *szValue, const char *szGroup );
	/* ILoadConfigurationEntrySink */

	void LoadConfiguration(const char *szConfig);
	void ApplyAndStoreCVar(const char *szKey, const char *szValue);
	void RevertCVarChanges();

	void SetAllowCheatCVars(bool allow) { m_allowCheatCVars = allow; }

protected:
	struct SSavedCVar
	{
		const char * m_name;
		const char * m_value;
	};

	std::vector<SSavedCVar> m_savedCVars;
	CSingleAllocTextBlock m_cvarsTextBlock;
	bool m_allowCheatCVars;
};

#endif // ~__REVERTIBLE_CONFIG_LOADER_H__
