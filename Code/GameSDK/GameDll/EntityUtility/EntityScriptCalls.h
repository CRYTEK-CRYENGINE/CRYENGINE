// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Description: Util functions relating to script entity operations.

-------------------------------------------------------------------------
History:
- 22:02:2011: Created by Benito G.R.

*************************************************************************/

#pragma once

#ifndef _ENTITY_SCRIPT_CALLS_H_
#define _ENTITY_SCRIPT_CALLS_H_

#include <CryScriptSystem/IScriptSystem.h>

struct IEntity;

namespace EntityScripts
{
	//////////////////////////////////////////////////////////////////////////
	/// Script function calls

	bool CallScriptFunction(IEntity* pEntity, IScriptTable *pScriptTable, const char *functionName);

	template<typename P1>
	bool CallScriptFunction(IEntity* pEntity, IScriptTable *pScriptTable, const char *functionName, const P1& p1)
	{
		bool result = false;

		if ((pEntity != NULL) && (pScriptTable != NULL))
		{
			IScriptSystem *pScriptSystem = pScriptTable->GetScriptSystem();
			if (pScriptTable->GetValueType(functionName) == svtFunction)
			{
				pScriptSystem->BeginCall(pScriptTable, functionName); 
				pScriptSystem->PushFuncParam(pEntity->GetScriptTable());
				pScriptSystem->PushFuncParam(p1);
				pScriptSystem->EndCall(result);
			}
		}

		return result;
	}

	template<typename P1, typename P2>
	bool CallScriptFunction(IEntity* pEntity, IScriptTable *pScriptTable, const char *functionName, const P1& p1, const P2& p2)
	{
		bool result = false;

		if ((pEntity != NULL) && (pScriptTable != NULL))
		{
			IScriptSystem *pScriptSystem = pScriptTable->GetScriptSystem();
			if (pScriptTable->GetValueType(functionName) == svtFunction)
			{
				pScriptSystem->BeginCall(pScriptTable, functionName); 
				pScriptSystem->PushFuncParam(pEntity->GetScriptTable());
				pScriptSystem->PushFuncParam(p1); 
				pScriptSystem->PushFuncParam(p2);
				pScriptSystem->EndCall(result);
			}
		}

		return result;
	}

	template<typename P1, typename P2, typename P3>
	bool CallScriptFunction(IEntity* pEntity, IScriptTable *pScriptTable, const char *functionName, const P1& p1, const P2& p2, const P3& p3)
	{
		bool result = false;

		if ((pEntity != NULL) && (pScriptTable != NULL))
		{
			IScriptSystem *pScriptSystem = pScriptTable->GetScriptSystem();
			if (pScriptTable->GetValueType(functionName) == svtFunction)
			{
				pScriptSystem->BeginCall(pScriptTable, functionName); 
				pScriptSystem->PushFuncParam(pEntity->GetScriptTable());
				pScriptSystem->PushFuncParam(p1); 
				pScriptSystem->PushFuncParam(p2);
				pScriptSystem->PushFuncParam(p3);
				pScriptSystem->EndCall(result);
			}
		}

		return result;
	}

	template<typename P1, typename P2, typename P3, typename P4>
	bool CallScriptFunction(IEntity* pEntity, IScriptTable *pScriptTable, const char *functionName, const P1& p1, const P2& p2, const P3& p3, const P4& p4)
	{
		bool result = false;

		if ((pEntity != NULL) && (pScriptTable != NULL))
		{
			IScriptSystem *pScriptSystem = pScriptTable->GetScriptSystem();
			if (pScriptTable->GetValueType(functionName) == svtFunction)
			{
				pScriptSystem->BeginCall(pScriptTable, functionName); 
				pScriptSystem->PushFuncParam(pEntity->GetScriptTable());
				pScriptSystem->PushFuncParam(p1); 
				pScriptSystem->PushFuncParam(p2);
				pScriptSystem->PushFuncParam(p3);
				pScriptSystem->PushFuncParam(p4);
				pScriptSystem->EndCall(result);
			}
		}

		return result;
	}

	template<typename P1, typename P2, typename P3, typename P4, typename P5>
	bool CallScriptFunction(IEntity* pEntity, IScriptTable *pScriptTable, const char *functionName, const P1& p1, const P2& p2, const P3& p3, const P4& p4, const P5& p5)
	{
		bool result = false;

		if ((pEntity != NULL) && (pScriptTable != NULL))
		{
			IScriptSystem *pScriptSystem = pScriptTable->GetScriptSystem();
			if (pScriptTable->GetValueType(functionName) == svtFunction)
			{
				pScriptSystem->BeginCall(pScriptTable, functionName); 
				pScriptSystem->PushFuncParam(pEntity->GetScriptTable());
				pScriptSystem->PushFuncParam(p1); 
				pScriptSystem->PushFuncParam(p2);
				pScriptSystem->PushFuncParam(p3);
				pScriptSystem->PushFuncParam(p4);
				pScriptSystem->PushFuncParam(p5);
				pScriptSystem->EndCall(result);
			}
		}

		return result;
	}

	template<typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
	bool CallScriptFunction(IEntity* pEntity, IScriptTable *pScriptTable, const char *functionName, const P1& p1, const P2& p2, const P3& p3, const P4& p4, const P5& p5, const P6& p6)
	{
		bool result = false;

		if ((pEntity != NULL) && (pScriptTable != NULL))
		{
			IScriptSystem *pScriptSystem = pScriptTable->GetScriptSystem();
			if (pScriptTable->GetValueType(functionName) == svtFunction)
			{
				pScriptSystem->BeginCall(pScriptTable, functionName); 
				pScriptSystem->PushFuncParam(pEntity->GetScriptTable());
				pScriptSystem->PushFuncParam(p1); 
				pScriptSystem->PushFuncParam(p2);
				pScriptSystem->PushFuncParam(p3);
				pScriptSystem->PushFuncParam(p4);
				pScriptSystem->PushFuncParam(p5);
				pScriptSystem->PushFuncParam(p6);
				pScriptSystem->EndCall(result);
			}
		}

		return result;
	}

	//////////////////////////////////////////////////////////////////////////
	/// Script property calls

	// Obtain a parameter in the properties table
	template<typename T>bool GetEntityProperty(IEntity* pEntity, const char *name, T &value) 
	{
		SmartScriptTable props;
		IScriptTable* pScriptTable = pEntity->GetScriptTable();
		if (pScriptTable && pScriptTable->GetValue("Properties", props))
		{
			return props->GetValue(name, value);
		}
		return false;
	}

	// Obtain a parameter in a properties sub table
	template<typename T>bool GetEntityProperty(IEntity* pEntity, const char *table, const char *name, T &value)
	{
		SmartScriptTable props;
		IScriptTable* pScriptTable = pEntity->GetScriptTable();
		if (pScriptTable && pScriptTable->GetValue("Properties", props))
		{
			SmartScriptTable subprop;
			if (props->GetValue(table, subprop))
			{
				return subprop->GetValue(name, value);
			}
		}
		return false;
	}

	// Helper for setting an entity property
	template<typename T>void SetEntityProperty(IEntity* pEntity, const char *name, const T &value)
	{
		SmartScriptTable props;
		IScriptTable* pScriptTable = pEntity->GetScriptTable();
		if (pScriptTable && pScriptTable->GetValue("Properties", props))
		{
			props->SetValue(name, value);  
		}
	}

	// Helper for setting an entity property in a sub table
	template<typename T>void SetEntityProperty(IEntity* pEntity, const char *table, const char *name, T &value)
	{
		SmartScriptTable props;
		IScriptTable* pScriptTable = pEntity->GetScriptTable();
		if (pScriptTable && pScriptTable->GetValue("Properties", props))
		{
			SmartScriptTable subprop;
			if (props->GetValue(table, subprop))
			{
				subprop->SetValue(name, value);
			}
		}
	}
};

#endif