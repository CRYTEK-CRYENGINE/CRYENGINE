// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:

   -------------------------------------------------------------------------
   History:
   - 8:9:2004   10:32 : Created by Márcio Martins

*************************************************************************/
#ifndef __ACTIONFILTER_H__
#define __ACTIONFILTER_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "IActionMapManager.h"

typedef std::set<ActionId> TFilterActions;

class CActionMapManager;

class CActionFilter :
	public IActionFilter
{
public:
	CActionFilter(CActionMapManager* pActionMapManager, IInput* pInput, const char* name, EActionFilterType type = eAFT_ActionFail);
	virtual ~CActionFilter();

	// IActionFilter
	virtual void        Release() { delete this; };
	virtual void        Filter(const ActionId& action);
	virtual bool        SerializeXML(const XmlNodeRef& root, bool bLoading);
	virtual const char* GetName() { return m_name.c_str(); }
	virtual void        Enable(bool enable);
	virtual bool        Enabled() { return m_enabled; };
	// ~IActionFilter

	bool         ActionFiltered(const ActionId& action);

	virtual void GetMemoryUsage(ICrySizer* pSizer) const;

private:
	bool               m_enabled;
	CActionMapManager* m_pActionMapManager;
	IInput*            m_pInput;
	TFilterActions     m_filterActions;
	EActionFilterType  m_type;
	string             m_name;
};

#endif //__ACTIONFILTER_H__
