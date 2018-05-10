// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Script Bind for CActionMapManager

   -------------------------------------------------------------------------
   History:
   - 8:11:2004   16:48 : Created by Márcio Martins

*************************************************************************/
#ifndef __SCRIPTBIND_ACTIONMAPMANAGER_H__
#define __SCRIPTBIND_ACTIONMAPMANAGER_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryScriptSystem/IScriptSystem.h>
#include <CryScriptSystem/ScriptHelpers.h>

class CActionMapManager;

class CScriptBind_ActionMapManager :
	public CScriptableBase
{
public:
	CScriptBind_ActionMapManager(ISystem* pSystem, CActionMapManager* pActionMapManager);
	virtual ~CScriptBind_ActionMapManager();

	void         Release() { delete this; };

	virtual void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}

	//! <code>ActionMapManager.EnableActionFilter( name, enable )</code>
	//!		<param name="name">Filter name.</param>
	//!		<param name="enable">True to enable the filter, false otherwise.</param>
	//! <description>Enables a filter for the actions.</description>
	int EnableActionFilter(IFunctionHandler* pH, const char* name, bool enable);

	//! <code>ActionMapManager.EnableActionMap( name, enable )</code>
	//!		<param name="name">Action map name.</param>
	//!		<param name="enable">True to enable the filter, false otherwise.</param>
	//! <description>Enables an action map.</description>
	int EnableActionMap(IFunctionHandler* pH, const char* name, bool enable);

	//! <code>ActionMapManager.EnableActionMapManager( enable, resetStateOnDisable )</code>
	//!		<param name="enable">Enables/Disables ActionMapManager.</param>
	//!		<param name="resetStateOnDisable">Resets the different Action states when ActionMapManager gets disabled.</param>
	//! <description>Enables/Disables ActionMapManager.</description>
	int EnableActionMapManager(IFunctionHandler* pH, bool enable, bool resetStateOnDisable);

	//! <code>ActionMapManager.LoadFromXML( name )</code>
	//!		<param name="name">XML file name.</param>
	//! <description>Loads information from an XML file.</description>
	int LoadFromXML(IFunctionHandler* pH, const char* name);

	//! <code>ActionMapManager.InitActionMaps( path )</code>
	//!		<param name="path">XML file path.</param>
	//! <description>Initializes the action maps and filters found in given file</description>
	int InitActionMaps(IFunctionHandler* pH, const char* path);

	//! <code>ActionMapManager.SetDefaultActionEntity( id, updateAll )</code>
	//!		<param name="id">EntityId of the new default action entity.</param>
	//!		<param name="updateAll">Updates all existing action map assignments.</param>
	//! <description>Sets the new default entity.</description>
	int SetDefaultActionEntity(IFunctionHandler* pH, EntityId id, bool updateAll);

	//! <code>ActionMapManager.GetDefaultActionEntity()</code>
	//! <description>Gets the currently set default action entity.</description>
	int GetDefaultActionEntity(IFunctionHandler* pH);

	//! <code>ActionMapManager.LoadControllerLayoutFile( layoutName )</code>
	//!		<param name="layoutName">layout name.</param>
	//! <description>Loads the given controller layout into the action manager.</description>
	int LoadControllerLayoutFile(IFunctionHandler* pH, const char* layoutName);

	//! <code>ActionMapManager.IsFilterEnabled( filterName )</code>
	//!		<param name="filterName">filter name.</param>
	//! <description>Checks if a filter is currently enabled.</description>
	int IsFilterEnabled(IFunctionHandler* pH, const char* filterName);

private:
	void RegisterGlobals();
	void RegisterMethods();

	ISystem*           m_pSystem;
	CActionMapManager* m_pManager;
};

#endif //__SCRIPTBIND_ACTIONMAPMANAGER_H__
