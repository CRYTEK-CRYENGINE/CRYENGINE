// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Script/IScriptFile.h>

namespace Schematyc2
{
	namespace BrowserUtils
	{
		TScriptFile* CreateScriptFile(CWnd* pWnd, CPoint point, const char* szPath);
		IScriptInclude* AddInclude(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID);
		IScriptGroup* AddGroup(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID);
		IScriptEnumeration* AddEnumeration(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID);
		IScriptStructure* AddStructure(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID);
		IScriptSignal* AddSignal(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID);
		IScriptAbstractInterface* AddAbstractInterface(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID);
		IScriptAbstractInterfaceFunction* AddAbstractInterfaceFunction(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID);
		IScriptAbstractInterfaceTask* AddAbstractInterfaceTask(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID);
		IScriptClass* AddClass(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID);
		IScriptStateMachine* AddStateMachine(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID, EStateMachineLifetime lifeTime);
		IScriptVariable* AddVariable(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID);
		IScriptProperty* AddProperty(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID);
		IScriptTimer* AddTimer(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID);
		IScriptComponentInstance* AddComponentInstance(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID);
		IScriptActionInstance* AddActionInstance(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID);
	}
}
