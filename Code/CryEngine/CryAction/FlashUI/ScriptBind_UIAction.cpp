// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ScriptBind_UIAction.cpp
//  Version:     v1.00
//  Created:     10/9/2010 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "ScriptBind_UIAction.h"
#include "FlashUI.h"

//------------------------------------------------------------------------
CScriptBind_UIAction::CScriptBind_UIAction()
{
	CScriptableBase::Init(gEnv->pScriptSystem, gEnv->pSystem);
	SetGlobalName("UIAction");

	RegisterMethods();

	if (gEnv->pFlashUI)
	{
		gEnv->pFlashUI->RegisterModule(this, "CScriptBind_UIAction");
	}
}

//------------------------------------------------------------------------
CScriptBind_UIAction::~CScriptBind_UIAction()
{
	if (gEnv->pFlashUI)
	{
		gEnv->pFlashUI->UnregisterModule(this);
	}
}

//------------------------------------------------------------------------
void CScriptBind_UIAction::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_UIAction::

	SCRIPT_REG_TEMPLFUNC(ReloadElement, "elementName, instanceID");
	SCRIPT_REG_TEMPLFUNC(UnloadElement, "elementName, instanceID");
	SCRIPT_REG_TEMPLFUNC(ShowElement, "elementName, instanceID");
	SCRIPT_REG_TEMPLFUNC(HideElement, "elementName, instanceID");
	SCRIPT_REG_TEMPLFUNC(RequestHide, "elementName, instanceID");

	SCRIPT_REG_TEMPLFUNC(CallFunction, "elementName, instanceID, functionName");
	SCRIPT_REG_TEMPLFUNC(SetVariable, "elementName, instanceID, varName");
	SCRIPT_REG_TEMPLFUNC(GetVariable, "elementName, instanceID, varName");
	SCRIPT_REG_TEMPLFUNC(SetArray, "elementName, instanceID, arrayName");
	SCRIPT_REG_TEMPLFUNC(GetArray, "elementName, instanceID, arrayName");

	SCRIPT_REG_TEMPLFUNC(GotoAndPlay, "elementName, instanceID, mcName, frameNum");
	SCRIPT_REG_TEMPLFUNC(GotoAndStop, "elementName, instanceID, mcName, frameNum");
	SCRIPT_REG_TEMPLFUNC(GotoAndPlayFrameName, "elementName, instanceID, mcName, frameName");
	SCRIPT_REG_TEMPLFUNC(GotoAndStopFrameName, "elementName, instanceID, mcName, frameName");

	SCRIPT_REG_TEMPLFUNC(SetAlpha, "elementName, instanceID, mcName, fAlpha");
	SCRIPT_REG_TEMPLFUNC(GetAlpha, "elementName, instanceID, mcName");
	SCRIPT_REG_TEMPLFUNC(SetVisible, "elementName, instanceID, mcName, bVisible");
	SCRIPT_REG_TEMPLFUNC(IsVisible, "elementName, instanceID, mcName");

	SCRIPT_REG_TEMPLFUNC(SetPos, "elementName, instanceID, mcName, vPos");
	SCRIPT_REG_TEMPLFUNC(GetPos, "elementName, instanceID, mcName");
	SCRIPT_REG_TEMPLFUNC(SetRotation, "elementName, instanceID, mcName, vRotation");
	SCRIPT_REG_TEMPLFUNC(GetRotation, "elementName, instanceID, mcName");
	SCRIPT_REG_TEMPLFUNC(SetScale, "elementName, instanceID, mcName, vScale");
	SCRIPT_REG_TEMPLFUNC(GetScale, "elementName, instanceID, mcName");

	SCRIPT_REG_TEMPLFUNC(StartAction, "actionName");
	SCRIPT_REG_TEMPLFUNC(EndAction, "actionName, disable");
	SCRIPT_REG_TEMPLFUNC(EnableAction, "actionName");
	SCRIPT_REG_TEMPLFUNC(DisableAction, "actionName");

	SCRIPT_REG_TEMPLFUNC(RegisterElementListener, "scripttable, elementName, instanceID, eventName, callback");
	SCRIPT_REG_TEMPLFUNC(RegisterActionListener, "scripttable, actionName, eventName, callback");
	SCRIPT_REG_TEMPLFUNC(RegisterEventSystemListener, "scripttable, eventSystem, eventName, callback");
	SCRIPT_REG_TEMPLFUNC(UnregisterElementListener, "scripttable, callback");
	SCRIPT_REG_TEMPLFUNC(UnregisterActionListener, "scripttable, callback");
	SCRIPT_REG_TEMPLFUNC(UnregisterEventSystemListener, "scripttable, callback");
}

//------------------------------------------------------------------------
IUIElement* CScriptBind_UIAction::GetElement(const char* sName, int instanceID, bool bSupressWarning)
{
	if (gEnv->IsDedicated())
		return NULL;

	CRY_ASSERT_MESSAGE(gEnv->pFlashUI, "FlashUI extension does not exist!");
	if (!gEnv->pFlashUI)
	{
		UIACTION_WARNING("LUA: FlashUI extension does not exist!");
		return NULL;
	}

	IUIElement* pElement = gEnv->pFlashUI->GetUIElement(sName);
	if (pElement && instanceID > 0)
		pElement = pElement->GetInstance(instanceID);

	if (pElement && pElement->IsValid())
		return pElement;

	if (!bSupressWarning)
	{
		UIACTION_WARNING("LUA: Try to access UIElement %s that is not valid!", sName);
	}
	return NULL;
}

//------------------------------------------------------------------------
IUIAction* CScriptBind_UIAction::GetAction(const char* sName)
{
	if (gEnv->IsDedicated())
		return NULL;

	CRY_ASSERT_MESSAGE(gEnv->pFlashUI, "FlashUI extension does not exist!");
	if (!gEnv->pFlashUI)
	{
		UIACTION_WARNING("LUA: FlashUI extension does not exist!");
		return NULL;
	}

	IUIAction* pAction = gEnv->pFlashUI->GetUIAction(sName);
	if (pAction && pAction->IsValid())
		return pAction;

	UIACTION_WARNING("LUA: Try to access UIAction %s that is not valid!", sName);
	return NULL;
}

//------------------------------------------------------------------------
IUIEventSystem* CScriptBind_UIAction::GetEventSystem(const char* sName, IUIEventSystem::EEventSystemType type)
{
	if (gEnv->IsDedicated())
		return NULL;

	CRY_ASSERT_MESSAGE(gEnv->pFlashUI, "FlashUI extension does not exist!");
	if (!gEnv->pFlashUI)
	{
		UIACTION_WARNING("LUA: FlashUI extension does not exist!");
		return NULL;
	}

	return gEnv->pFlashUI->GetEventSystem(sName, type);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::ReloadElement(IFunctionHandler* pH, const char* elementName, int instanceID)
{
	IUIElement* pElement = GetElement(elementName, instanceID);
	if (pElement)
	{
		if (instanceID < 0)
		{
			IUIElementIteratorPtr elements = pElement->GetInstances();
			while (IUIElement* pInstance = elements->Next())
				pInstance->Reload();
		}
		else
		{
			pElement->Reload();
		}
		return pH->EndFunction(true);
	}
	UIACTION_WARNING("LUA: UIElement %s does not exist", elementName);
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::UnloadElement(IFunctionHandler* pH, const char* elementName, int instanceID)
{
	IUIElement* pElement = GetElement(elementName, instanceID);
	if (pElement)
	{
		if (instanceID < 0)
		{
			IUIElementIteratorPtr elements = pElement->GetInstances();
			while (IUIElement* pInstance = elements->Next())
				pInstance->Unload();
		}
		else
		{
			pElement->Unload();
		}
		return pH->EndFunction(true);
	}
	UIACTION_WARNING("LUA: UIElement %s does not exist", elementName);
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::ShowElement(IFunctionHandler* pH, const char* elementName, int instanceID)
{
	IUIElement* pElement = GetElement(elementName, instanceID);
	if (pElement)
	{
		if (instanceID < 0)
		{
			IUIElementIteratorPtr elements = pElement->GetInstances();
			while (IUIElement* pInstance = elements->Next())
				pInstance->SetVisible(true);
		}
		else
		{
			pElement->SetVisible(true);
		}
		return pH->EndFunction(true);
	}
	UIACTION_WARNING("LUA: UIElement %s does not exist", elementName);
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::HideElement(IFunctionHandler* pH, const char* elementName, int instanceID)
{
	IUIElement* pElement = GetElement(elementName, instanceID);
	if (pElement)
	{
		if (instanceID < 0)
		{
			IUIElementIteratorPtr elements = pElement->GetInstances();
			while (IUIElement* pInstance = elements->Next())
				pInstance->SetVisible(false);
		}
		else
		{
			pElement->SetVisible(false);
		}
		return pH->EndFunction(true);
	}
	UIACTION_WARNING("LUA: UIElement %s does not exist", elementName);
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::RequestHide(IFunctionHandler* pH, const char* elementName, int instanceID)
{
	IUIElement* pElement = GetElement(elementName, instanceID);
	if (pElement)
	{
		if (instanceID < 0)
		{
			IUIElementIteratorPtr elements = pElement->GetInstances();
			while (IUIElement* pInstance = elements->Next())
				pInstance->RequestHide();
		}
		else
		{
			pElement->RequestHide();
		}
		return pH->EndFunction(true);
	}
	UIACTION_WARNING("LUA: UIElement %s does not exist", elementName);
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::CallFunction(IFunctionHandler* pH, const char* elementName, int instanceID, const char* functionName)
{
	SUIArguments args;
	if (!SUIToLuaConversationHelper::LuaArgsToUIArgs(pH, 4, args))
	{
		UIACTION_WARNING("LUA: Failed to call function %s on element %s: Invalid arguments", functionName, elementName);
		return pH->EndFunction(false);
	}

	IUIElement* pElement = GetElement(elementName, instanceID, true);
	if (pElement)
	{
		const SUIEventDesc* pFctDesc = pElement->GetFunctionDesc(functionName);
		if (pFctDesc)
		{
			TUIData res;
			bool bFctOk = true;
			if (instanceID < 0)
			{
				IUIElementIteratorPtr elements = pElement->GetInstances();
				while (IUIElement* pInstance = elements->Next())
					bFctOk &= pInstance->CallFunction(pFctDesc, args, &res);
			}
			else
			{
				bFctOk = pElement->CallFunction(pFctDesc, args, &res);
			}
			if (bFctOk)
			{
				string sRes;
				res.GetValueWithConversion(sRes);
				return pH->EndFunction(sRes.c_str());
			}
		}
		UIACTION_WARNING("LUA: UIElement %s does not have function %s", elementName, functionName);
	}
	else if (IUIEventSystem* pEventSystem = GetEventSystem(elementName, IUIEventSystem::eEST_UI_TO_SYSTEM))
	{
		uint eventid = pEventSystem->GetEventId(functionName);
		if (eventid != ~0)
		{
			SUIArguments res = pEventSystem->SendEvent(SUIEvent(eventid, args));
			SmartScriptTable table = gEnv->pScriptSystem->CreateTable();
			SUIToLuaConversationHelper::UIArgsToLuaTable(res, table);
			return pH->EndFunction(table);
		}
		UIACTION_WARNING("LUA: UIEventSystem %s does not have event %s", elementName, functionName);
	}
	else
	{
		UIACTION_WARNING("LUA: UIElement or UIEventSystem %s does not exist", elementName);
	}
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::SetVariable(IFunctionHandler* pH, const char* elementName, int instanceID, const char* varName)
{
	if (pH->GetParamCount() != 4)
	{
		UIACTION_ERROR("LUA: UIAction.SetVariable - wrong number of arguments!");
		return pH->EndFunction(false);
	}

	IUIElement* pElement = GetElement(elementName, instanceID);
	if (pElement)
	{
		const SUIParameterDesc* pVarDesc = pElement->GetVariableDesc(varName);
		if (pVarDesc)
		{
			bool bRet = true;
			TUIData value;
			if (SUIToLuaConversationHelper::LuaArgToUIArg(pH, 4, value))
			{
				bool bVarOk = true;
				if (instanceID < 0)
				{
					IUIElementIteratorPtr elements = pElement->GetInstances();
					while (IUIElement* pInstance = elements->Next())
						bVarOk &= pInstance->SetVariable(pVarDesc, value);
				}
				else
				{
					bVarOk = pElement->SetVariable(pVarDesc, value);
				}

				if (bVarOk)
					return pH->EndFunction(true);
				else
				{
					UIACTION_WARNING("LUA: Element %s has no variable %s", elementName, varName);
				}
			}
			else
			{
				UIACTION_WARNING("LUA: Element %s: wrong datatype for variable %s", elementName, varName);
			}
		}
		else
		{
			UIACTION_WARNING("LUA: Element %s has no variable %s", elementName, varName);
		}
	}
	else
	{
		UIACTION_WARNING("LUA: UIElement %s does not exist", elementName);
	}
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::GetVariable(IFunctionHandler* pH, const char* elementName, int instanceID, const char* varName)
{
	IUIElement* pElement = GetElement(elementName, instanceID);
	if (pElement)
	{
		const SUIParameterDesc* pVarDesc = pElement->GetVariableDesc(varName);
		if (pVarDesc)
		{
			TUIData value;
			if (pElement->GetVariable(pVarDesc, value))
			{
				bool ok;
				ScriptAnyValue res = SUIToLuaConversationHelper::UIValueToLuaValue(value, ok);
				if (!ok)
				{
					UIACTION_WARNING("LUA: Error reading variable %s from UI Element %s", varName, elementName);
				}
				return pH->EndFunctionAny(res);
			}
		}
		UIACTION_WARNING("LUA: Element %s has no variable %s", elementName, varName);
	}
	else
	{
		UIACTION_WARNING("LUA: UIElement %s does not exist", elementName);
	}
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::SetArray(IFunctionHandler* pH, const char* elementName, int instanceID, const char* arrayName, SmartScriptTable values)
{
	IUIElement* pElement = GetElement(elementName, instanceID);
	if (pElement)
	{
		const SUIParameterDesc* pArrayDesc = pElement->GetArrayDesc(arrayName);
		if (pArrayDesc)
		{
			SUIArguments vals;
			if (SUIToLuaConversationHelper::LuaTableToUIArgs(values, vals))
			{
				bool bVarOk = true;
				if (instanceID < 0)
				{
					IUIElementIteratorPtr elements = pElement->GetInstances();
					while (IUIElement* pInstance = elements->Next())
						bVarOk &= pInstance->SetArray(pArrayDesc, vals);
				}
				else
				{
					bVarOk = pElement->SetArray(pArrayDesc, vals);
				}

				if (bVarOk)
					return pH->EndFunction(true);
			}
			UIACTION_ERROR("LUA: Failed to set array %s on Element %s: Invalid arguments", arrayName, elementName);
		}
		UIACTION_WARNING("LUA: Element %s has no array %s", elementName, arrayName);
	}
	else
	{
		UIACTION_WARNING("LUA: UIElement %s does not exist", elementName);
	}
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::GetArray(IFunctionHandler* pH, const char* elementName, int instanceID, const char* arrayName)
{
	IUIElement* pElement = GetElement(elementName, instanceID);
	if (pElement)
	{
		const SUIParameterDesc* pArrayDesc = pElement->GetArrayDesc(arrayName);
		SUIArguments vals;
		if (pArrayDesc && pElement->GetArray(pArrayDesc, vals))
		{
			SmartScriptTable res(m_pSS);
			string val;
			for (int i = 0; i < vals.GetArgCount(); ++i)
			{
				vals.GetArg(i).GetValueWithConversion(val);
				res->SetAt(i + 1, val.c_str());
			}
			return pH->EndFunction(res);
		}
		UIACTION_WARNING("LUA: Element %s has no array %s", elementName, arrayName);
	}
	else
	{
		UIACTION_WARNING("LUA: UIElement %s does not exist", elementName);
	}
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::GotoAndPlay(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName, int frameNum)
{
	IUIElement* pElement = GetElement(elementName, instanceID);
	if (pElement)
	{
		IFlashVariableObject* pMC = pElement->GetMovieClip(mcName);
		if (pMC)
		{
			pMC->GotoAndPlay(frameNum);
			return pH->EndFunction(true);
		}
		UIACTION_WARNING("LUA: Element %s has no movieclip %s", elementName, mcName);
	}
	else
	{
		UIACTION_WARNING("LUA: UIElement %s does not exist", elementName);
	}
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::GotoAndStop(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName, int frameNum)
{
	IUIElement* pElement = GetElement(elementName, instanceID);
	if (pElement)
	{
		IFlashVariableObject* pMC = pElement->GetMovieClip(mcName);
		if (pMC)
		{
			pMC->GotoAndStop(frameNum);
			return pH->EndFunction(true);
		}
		UIACTION_WARNING("LUA: Element %s has no movieclip %s", elementName, mcName);
	}
	else
	{
		UIACTION_WARNING("LUA: UIElement %s does not exist", elementName);
	}
	return pH->EndFunction(false);

}

//------------------------------------------------------------------------
int CScriptBind_UIAction::GotoAndPlayFrameName(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName, const char* frameName)
{
	IUIElement* pElement = GetElement(elementName, instanceID);
	if (pElement)
	{
		IFlashVariableObject* pMC = pElement->GetMovieClip(mcName);
		if (pMC)
		{
			pMC->GotoAndPlay(frameName);
			return pH->EndFunction(true);
		}
		UIACTION_WARNING("LUA: Element %s has no movieclip %s", elementName, mcName);
	}
	else
	{
		UIACTION_WARNING("LUA: UIElement %s does not exist", elementName);
	}
	return pH->EndFunction(false);

}

//------------------------------------------------------------------------
int CScriptBind_UIAction::GotoAndStopFrameName(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName, const char* frameName)
{
	IUIElement* pElement = GetElement(elementName, instanceID);
	if (pElement)
	{
		IFlashVariableObject* pMC = pElement->GetMovieClip(mcName);
		if (pMC)
		{
			pMC->GotoAndStop(frameName);
			return pH->EndFunction(true);
		}
		UIACTION_WARNING("LUA: Element %s has no movieclip %s", elementName, mcName);
	}
	else
	{
		UIACTION_WARNING("LUA: UIElement %s does not exist", elementName);
	}
	return pH->EndFunction(false);

}

//------------------------------------------------------------------------
int CScriptBind_UIAction::SetAlpha(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName, float fAlpha)
{
	IUIElement* pElement = GetElement(elementName, instanceID);
	if (pElement)
	{
		IFlashVariableObject* pMC = pElement->GetMovieClip(mcName);
		if (pMC)
		{
			SFlashDisplayInfo info;
			pMC->GetDisplayInfo(info);
			info.SetAlpha(fAlpha);
			pMC->SetDisplayInfo(info);
			return pH->EndFunction(true);
		}
		UIACTION_WARNING("LUA: Element %s has no movieclip %s", elementName, mcName);
	}
	else
	{
		UIACTION_WARNING("LUA: UIElement %s does not exist", elementName);
	}
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::GetAlpha(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName)
{
	IUIElement* pElement = GetElement(elementName, instanceID);
	if (pElement)
	{
		IFlashVariableObject* pMC = pElement->GetMovieClip(mcName);
		if (pMC)
		{
			SFlashDisplayInfo info;
			pMC->GetDisplayInfo(info);
			return pH->EndFunction(info.GetAlpha());
		}
		UIACTION_WARNING("LUA: Element %s has no movieclip %s", elementName, mcName);
	}
	else
	{
		UIACTION_WARNING("LUA: UIElement %s does not exist", elementName);
	}
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::SetVisible(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName, bool bVisible)
{
	IUIElement* pElement = GetElement(elementName, instanceID);
	if (pElement)
	{
		IFlashVariableObject* pMC = pElement->GetMovieClip(mcName);
		if (pMC)
		{
			SFlashDisplayInfo info;
			pMC->GetDisplayInfo(info);
			info.SetVisible(bVisible);
			pMC->SetDisplayInfo(info);
			return pH->EndFunction(true);
		}
		UIACTION_WARNING("LUA: Element %s has no movieclip %s", elementName, mcName);
	}
	else
	{
		UIACTION_WARNING("LUA: UIElement %s does not exist", elementName);
	}
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::IsVisible(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName)
{
	IUIElement* pElement = GetElement(elementName, instanceID);
	if (pElement)
	{
		IFlashVariableObject* pMC = pElement->GetMovieClip(mcName);
		if (pMC)
		{
			SFlashDisplayInfo info;
			pMC->GetDisplayInfo(info);
			return pH->EndFunction(info.GetVisible());
		}
		UIACTION_WARNING("LUA: Element %s has no movieclip %s", elementName, mcName);
	}
	else
	{
		UIACTION_WARNING("LUA: UIElement %s does not exist", elementName);
	}
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::SetPos(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName, Vec3 vPos)
{
	IUIElement* pElement = GetElement(elementName, instanceID);
	if (pElement)
	{
		IFlashVariableObject* pMC = pElement->GetMovieClip(mcName);
		if (pMC)
		{
			SFlashDisplayInfo info;
			pMC->GetDisplayInfo(info);
			info.SetX(vPos.x);
			info.SetY(vPos.y);
			info.SetZ(vPos.z);
			pMC->SetDisplayInfo(info);
			return pH->EndFunction(true);
		}
		UIACTION_WARNING("LUA: Element %s has no movieclip %s", elementName, mcName);
	}
	else
	{
		UIACTION_WARNING("LUA: UIElement %s does not exist", elementName);
	}
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::GetPos(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName)
{
	IUIElement* pElement = GetElement(elementName, instanceID);
	if (pElement)
	{
		IFlashVariableObject* pMC = pElement->GetMovieClip(mcName);
		if (pMC)
		{
			SFlashDisplayInfo info;
			pMC->GetDisplayInfo(info);
			return pH->EndFunction(Script::SetCachedVector(Vec3(info.GetX(), info.GetY(), info.GetZ()), pH, 1));
		}
		UIACTION_WARNING("LUA: Element %s has no movieclip %s", elementName, mcName);
	}
	else
	{
		UIACTION_WARNING("LUA: UIElement %s does not exist", elementName);
	}
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::SetRotation(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName, Vec3 vRotation)
{
	IUIElement* pElement = GetElement(elementName, instanceID);
	if (pElement)
	{
		IFlashVariableObject* pMC = pElement->GetMovieClip(mcName);
		if (pMC)
		{
			SFlashDisplayInfo info;
			pMC->GetDisplayInfo(info);
			info.SetXRotation(vRotation.x);
			info.SetYRotation(vRotation.y);
			info.SetRotation(vRotation.z);
			pMC->SetDisplayInfo(info);
			return pH->EndFunction(true);
		}
		UIACTION_WARNING("LUA: Element %s has no movieclip %s", elementName, mcName);
	}
	else
	{
		UIACTION_WARNING("LUA: UIElement %s does not exist", elementName);
	}
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::GetRotation(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName)
{
	IUIElement* pElement = GetElement(elementName, instanceID);
	if (pElement)
	{
		IFlashVariableObject* pMC = pElement->GetMovieClip(mcName);
		if (pMC)
		{
			SFlashDisplayInfo info;
			pMC->GetDisplayInfo(info);
			return pH->EndFunction(Script::SetCachedVector(Vec3(info.GetXRotation(), info.GetYRotation(), info.GetRotation()), pH, 1));
		}
		UIACTION_WARNING("LUA: Element %s has no movieclip %s", elementName, mcName);
	}
	else
	{
		UIACTION_WARNING("LUA: UIElement %s does not exist", elementName);
	}
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::SetScale(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName, Vec3 vScale)
{
	IUIElement* pElement = GetElement(elementName, instanceID);
	if (pElement)
	{
		IFlashVariableObject* pMC = pElement->GetMovieClip(mcName);
		if (pMC)
		{
			SFlashDisplayInfo info;
			pMC->GetDisplayInfo(info);
			info.SetXScale(vScale.x);
			info.SetYScale(vScale.y);
			info.SetZScale(vScale.z);
			pMC->SetDisplayInfo(info);
			return pH->EndFunction(true);
		}
		UIACTION_WARNING("LUA: Element %s has no movieclip %s", elementName, mcName);
	}
	else
	{
		UIACTION_WARNING("LUA: UIElement %s does not exist", elementName);
	}
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::GetScale(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName)
{
	IUIElement* pElement = GetElement(elementName, instanceID);
	if (pElement)
	{
		IFlashVariableObject* pMC = pElement->GetMovieClip(mcName);
		if (pMC)
		{
			SFlashDisplayInfo info;
			pMC->GetDisplayInfo(info);
			return pH->EndFunction(Script::SetCachedVector(Vec3(info.GetXScale(), info.GetYScale(), info.GetZScale()), pH, 1));
		}
		UIACTION_WARNING("LUA: Element %s has no movieclip %s", elementName, mcName);
	}
	else
	{
		UIACTION_WARNING("LUA: UIElement %s does not exist", elementName);
	}
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::StartAction(IFunctionHandler* pH, const char* actionName, SmartScriptTable arguments)
{
	IUIAction* pAction = GetAction(actionName);
	if (pAction)
	{
		SUIArguments args;
		if (SUIToLuaConversationHelper::LuaTableToUIArgs(arguments, args))
		{
			gEnv->pFlashUI->GetUIActionManager()->StartAction(pAction, args);
			return pH->EndFunction(true);
		}
		UIACTION_WARNING("LUA: Failed to call UIAction %s: Invalid arguments", actionName);
	}
	else
	{
		UIACTION_WARNING("LUA: UIAction %s does not exist", actionName);
	}
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::EndAction(IFunctionHandler* pH, SmartScriptTable pTable, bool disable, SmartScriptTable arguments)
{
	if (!pTable)
	{
		UIACTION_WARNING("LUA: EndAction received non-valid script table!");
		return pH->EndFunction(false);
	}

	const char* actionName;
	if (pTable->GetValue("__ui_action_name", actionName))
	{
		IUIAction* pAction = GetAction(actionName);
		if (pAction)
		{
			SUIArguments args;
			if (SUIToLuaConversationHelper::LuaTableToUIArgs(arguments, args))
			{
				gEnv->pFlashUI->GetUIActionManager()->EndAction(pAction, args);
				if (disable)
					gEnv->pFlashUI->GetUIActionManager()->EnableAction(pAction, false);
				return pH->EndFunction(true);
			}
			UIACTION_WARNING("LUA: Failed to end UIAction %s: Invalid arguments", actionName);
			return pH->EndFunction(false);
		}
	}
	UIACTION_WARNING("LUA: Failed to end UIAction: Called from different script than UIAction lua script!");
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::EnableAction(IFunctionHandler* pH, const char* actionName)
{
	IUIAction* pAction = GetAction(actionName);
	if (pAction)
	{
		pAction->SetEnabled(true);
		return pH->EndFunction(true);
	}
	UIACTION_WARNING("LUA: UIAction %s does not exist", actionName);
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::DisableAction(IFunctionHandler* pH, const char* actionName)
{
	IUIAction* pAction = GetAction(actionName);
	if (pAction)
	{
		pAction->SetEnabled(false);
		return pH->EndFunction(true);
	}
	UIACTION_WARNING("LUA: UIAction %s does not exist", actionName);
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::RegisterElementListener(IFunctionHandler* pH, SmartScriptTable pTable, const char* elementName, int instanceID, const char* eventName, const char* callback)
{
	if (!pTable)
	{
		UIACTION_WARNING("LUA: RegisterElementListener received non-valid script table!");
		return pH->EndFunction(false);
	}

	IUIElement* pElement = strlen(elementName) > 0 ? GetElement(elementName, 0) : NULL;
	m_ElementCallbacks.Init(pElement);
	m_ElementCallbacks.AddCallback(pTable, callback, SUILuaCallbackInfo<IUIElement>::CreateInfo(pElement, eventName ? eventName : "", instanceID));
	return pH->EndFunction(true);

}

//------------------------------------------------------------------------
int CScriptBind_UIAction::RegisterActionListener(IFunctionHandler* pH, SmartScriptTable pTable, const char* actionName, const char* eventName, const char* callback)
{
	if (!pTable)
	{
		UIACTION_WARNING("LUA: RegisterActionListener received non-valid script table!");
		return pH->EndFunction(false);
	}

	IUIAction* pAction = strlen(actionName) > 0 ? GetAction(actionName) : NULL;
	m_ActionCallbacks.AddCallback(pTable, callback, SUILuaCallbackInfo<IUIAction>::CreateInfo(pAction, eventName ? eventName : ""));
	return pH->EndFunction(true);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::RegisterEventSystemListener(IFunctionHandler* pH, SmartScriptTable pTable, const char* eventSystem, const char* eventName, const char* callback)
{
	if (!pTable)
	{
		UIACTION_WARNING("LUA: RegisterEventSystemListener received non-valid script table!");
		return pH->EndFunction(false);
	}

	IUIEventSystem* pEventSystem = GetEventSystem(eventSystem, IUIEventSystem::eEST_SYSTEM_TO_UI);
	m_EventSystemCallbacks.Init(pEventSystem);
	m_EventSystemCallbacks.AddCallback(pTable, callback, SUILuaCallbackInfo<IUIEventSystem>::CreateInfo(pEventSystem, eventName ? eventName : ""));
	return pH->EndFunction(true);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::UnregisterElementListener(IFunctionHandler* pH, SmartScriptTable pTable, const char* callback)
{
	if (!pTable)
	{
		UIACTION_WARNING("LUA: UnregisterElementListener received non-valid script table!");
		return pH->EndFunction(false);
	}
	m_ElementCallbacks.RemoveCallbacks(pTable, callback);
	return pH->EndFunction(true);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::UnregisterActionListener(IFunctionHandler* pH, SmartScriptTable pTable, const char* callback)
{
	if (!pTable)
	{
		UIACTION_WARNING("LUA: UnregisterActionListener received non-valid script table!");
		return pH->EndFunction(false);
	}
	m_ActionCallbacks.RemoveCallbacks(pTable, callback);
	return pH->EndFunction(true);
}

//------------------------------------------------------------------------
int CScriptBind_UIAction::UnregisterEventSystemListener(IFunctionHandler* pH, SmartScriptTable pTable, const char* callback)
{
	if (!pTable)
	{
		UIACTION_WARNING("LUA: UnregisterEventSystemListener received non-valid script table!");
		return pH->EndFunction(false);
	}
	m_EventSystemCallbacks.RemoveCallbacks(pTable, callback);
	return pH->EndFunction(true);
}

//------------------------------------------------------------------------
//------------------------------------------------------------------------
//------------------------------------------------------------------------
void CScriptBind_UIAction::Reload()
{
	Reset();
}

//------------------------------------------------------------------------
void CScriptBind_UIAction::Reset()
{
	m_ElementCallbacks.Clear();
	m_ActionCallbacks.Clear();
	m_EventSystemCallbacks.Clear();
}

//------------------------------------------------------------------------
//------------------------------------------------------------------------
//------------------------------------------------------------------------
SUIElementLuaCallback::~SUIElementLuaCallback()
{
	Clear();
}

//------------------------------------------------------------------------
void SUIElementLuaCallback::Clear()
{
	if (gEnv->pFlashUI)
	{
		const int count = gEnv->pFlashUI->GetUIElementCount();
		for (int i = 0; i < count; ++i)
		{
			IUIElementIteratorPtr elements = gEnv->pFlashUI->GetUIElement(i)->GetInstances();
			while (IUIElement* pInst = elements->Next())
				pInst->RemoveEventListener(this);
		}
	}
	ClearCallbacks();
}

//------------------------------------------------------------------------
void SUIElementLuaCallback::Init(IUIElement* pElement)
{
	if (gEnv->pFlashUI)
	{
		if (!pElement)
		{
			const int count = gEnv->pFlashUI->GetUIElementCount();
			for (int i = 0; i < count; ++i)
			{
				pElement = gEnv->pFlashUI->GetUIElement(i);
				IUIElementIteratorPtr elements = pElement->GetInstances();
				while (IUIElement* pInst = elements->Next())
				{
					pInst->RemoveEventListener(this); // to avoid double registration
					pInst->AddEventListener(this, "SUIElementLuaCallback");
				}
			}
		}
		else
		{
			IUIElementIteratorPtr elements = pElement->GetInstances();
			while (IUIElement* pInst = elements->Next())
			{
				pInst->RemoveEventListener(this); // to avoid double registration
				pInst->AddEventListener(this, "SUIElementLuaCallback");
			}
		}
	}
}

//------------------------------------------------------------------------
void SUIElementLuaCallback::OnUIEvent(IUIElement* pSender, const SUIEventDesc& event, const SUIArguments& args)
{
	NotifyEvent(SUILuaCallbackInfo<IUIElement>::CreateInfo(pSender, event.sDisplayName), args);
}

//------------------------------------------------------------------------
void SUIElementLuaCallback::OnUIEventEx(IUIElement* pSender, const char* fscommand, const SUIArguments& args, void* pUserData)
{
	SUIArguments luaArgs;
	luaArgs.AddArgument(fscommand);
	luaArgs.AddArguments(args);
	NotifyEvent(SUILuaCallbackInfo<IUIElement>::CreateInfo(pSender, "OnUIEventEx"), luaArgs);
}

//------------------------------------------------------------------------
void SUIElementLuaCallback::OnInit(IUIElement* pSender, IFlashPlayer* pFlashPlayer)
{
	NotifyEvent(SUILuaCallbackInfo<IUIElement>::CreateInfo(pSender, "OnInit"));
}

//------------------------------------------------------------------------
void SUIElementLuaCallback::OnUnload(IUIElement* pSender)
{
	NotifyEvent(SUILuaCallbackInfo<IUIElement>::CreateInfo(pSender, "OnUnload"));
}

//------------------------------------------------------------------------
void SUIElementLuaCallback::OnSetVisible(IUIElement* pSender, bool bVisible)
{
	NotifyEvent(SUILuaCallbackInfo<IUIElement>::CreateInfo(pSender, bVisible ? "OnShow" : "OnHide"));
}

//------------------------------------------------------------------------
void SUIElementLuaCallback::OnInstanceCreated(IUIElement* pSender, IUIElement* pNewInstance)
{
	pNewInstance->RemoveEventListener(this); // to avoid double registration
	pNewInstance->AddEventListener(this, "SUIElementLuaCallback");
}

//------------------------------------------------------------------------
void SUIElementLuaCallback::OnInstanceDestroyed(IUIElement* pSender, IUIElement* pDeletedInstance)
{
	NotifyEvent(SUILuaCallbackInfo<IUIElement>::CreateInfo(pSender, "OnInstanceDestroyed"));
}

//------------------------------------------------------------------------
//------------------------------------------------------------------------
//------------------------------------------------------------------------
SUIActionLuaCallback::SUIActionLuaCallback()
{
	if (gEnv->pFlashUI)
		gEnv->pFlashUI->GetUIActionManager()->AddListener(this, "SUIActionLuaCallback");
}

//------------------------------------------------------------------------
SUIActionLuaCallback::~SUIActionLuaCallback()
{
	if (gEnv->pFlashUI)
		gEnv->pFlashUI->GetUIActionManager()->RemoveListener(this);
}

//------------------------------------------------------------------------
void SUIActionLuaCallback::Clear()
{
	ClearCallbacks();
}

//------------------------------------------------------------------------
void SUIActionLuaCallback::OnStart(IUIAction* pAction, const SUIArguments& args)
{
	NotifyEvent(SUILuaCallbackInfo<IUIAction>::CreateInfo(pAction, "OnStart"), args);
}

//------------------------------------------------------------------------
void SUIActionLuaCallback::OnEnd(IUIAction* pAction, const SUIArguments& args)
{
	NotifyEvent(SUILuaCallbackInfo<IUIAction>::CreateInfo(pAction, "OnEnd"), args);
}

//------------------------------------------------------------------------
//------------------------------------------------------------------------
//------------------------------------------------------------------------
SUIEventSystemLuaCallback::~SUIEventSystemLuaCallback()
{
	Clear();
}

//------------------------------------------------------------------------
void SUIEventSystemLuaCallback::Init(IUIEventSystem* pEventSystem)
{
	if (gEnv->pFlashUI)
	{
		if (!pEventSystem)
		{
			IUIEventSystemIteratorPtr eventSystems = gEnv->pFlashUI->CreateEventSystemIterator(IUIEventSystem::eEST_SYSTEM_TO_UI);
			string name;
			while (pEventSystem = eventSystems->Next(name))
				AddNewListener(pEventSystem);
		}
		else
			AddNewListener(pEventSystem);
	}
}

//------------------------------------------------------------------------
void SUIEventSystemLuaCallback::Clear()
{
	m_Listener.clear();
	ClearCallbacks();
}

//------------------------------------------------------------------------
void SUIEventSystemLuaCallback::OnEvent(IUIEventSystem* pEventSystem, const SUIEvent& event)
{
	NotifyEvent(SUILuaCallbackInfo<IUIEventSystem>::CreateInfo(pEventSystem, pEventSystem->GetEventDesc((int)event.event)->sDisplayName), event.args);
}

//------------------------------------------------------------------------
void SUIEventSystemLuaCallback::AddNewListener(IUIEventSystem* pEventSystem)
{
	for (int i = 0, count = m_Listener.size(); i < count; ++i)
		if (m_Listener[i].GetEventSystem() == pEventSystem)
			return;

	m_Listener.push_back(SEventSystemListener(pEventSystem, this));
}

//------------------------------------------------------------------------
SUIEventSystemLuaCallback::SEventSystemListener::~SEventSystemListener()
{
	Clear();
}

//------------------------------------------------------------------------
void SUIEventSystemLuaCallback::SEventSystemListener::Init(IUIEventSystem* pEventSystem, SUIEventSystemLuaCallback* pOwner)
{
	m_pEventSystem = pEventSystem;
	m_pOwner = pOwner;
	if (m_pEventSystem)
		m_pEventSystem->RegisterListener(this, "SUIEventSystemLuaCallback");
}

//------------------------------------------------------------------------
void SUIEventSystemLuaCallback::SEventSystemListener::Clear()
{
	if (m_pEventSystem)
	{
		m_pEventSystem->UnregisterListener(this); // to avoid double registration
	}
}

//------------------------------------------------------------------------
SUIArgumentsRet SUIEventSystemLuaCallback::SEventSystemListener::OnEvent(const SUIEvent& event)
{
	assert(m_pOwner && m_pEventSystem);
	m_pOwner->OnEvent(m_pEventSystem, event);
	return SUIArguments();
}

//------------------------------------------------------------------------
void SUIEventSystemLuaCallback::SEventSystemListener::OnEventSystemDestroyed(IUIEventSystem* pEventSystem)
{
	m_pEventSystem = NULL;
}

//------------------------------------------------------------------------
//------------------------------------------------------------------------
//------------------------------------------------------------------------
namespace SUIConvHelperTmpl
{
template<class T>
ScriptVarType            GetVarType(T& t, int idx)                 { assert(false); return svtNull; }
template<> ScriptVarType GetVarType(SmartScriptTable& t, int idx)  { return t->GetAtType(idx); }
template<> ScriptVarType GetVarType(IFunctionHandler*& t, int idx) { return t->GetParamType(idx); }

template<class T, class V>
bool                   GetVarValue(T& t, int idx, V& val)                 { assert(false); return false; }
template<class V> bool GetVarValue(SmartScriptTable& t, int idx, V& val)  { return t->GetAt(idx, val); }
template<class V> bool GetVarValue(IFunctionHandler*& t, int idx, V& val) { return t->GetParam(idx, val); }

template<class T>
bool LuaArgToUIArgImpl(T& t, int idx, TUIData& value)
{
	bool bOk = false;
	switch (GetVarType(t, idx))
	{
	case svtBool:
		{
			bool val;
			bOk = GetVarValue(t, idx, val);
			value = TUIData(val);
		}
		break;
	case svtNumber:
		{
			float val;
			bOk = GetVarValue(t, idx, val);
			value = TUIData(val);
		}
		break;
	case svtString:
		{
			const char* val;
			bOk = GetVarValue(t, idx, val);
			value = TUIData(string(val));
		}
		break;
	case svtObject:
		{
			Vec3 val(ZERO);
			bOk = GetVarValue(t, idx, val);
			value = TUIData(val);
		}
		break;
	case svtPointer:
		{
			ScriptHandle sh;
			bOk = GetVarValue(t, idx, sh);
			value = TUIData((EntityId)sh.n);
		}
		break;
	case svtNull:
		CRY_ASSERT_MESSAGE(false, "Invalid data type for UIAction call!");
		break;
	case svtUserData:
		CRY_ASSERT_MESSAGE(false, "Invalid data type for UIAction call!");
		break;
	case svtFunction:
		CRY_ASSERT_MESSAGE(false, "Invalid data type for UIAction call!");
		break;
	}
	return bOk;
}

template<class T>
ScriptAnyValue GetValueRaw(const TUIData& value)
{
	const T* val = value.GetPtr<T>();
	assert(val);
	return ScriptAnyValue(*val);
}

template<class T1>
void SetTableVal(const T1& val, SmartScriptTable table, int idx) { table->SetAt(idx, val); }

template<>
void SetTableVal(const string& val, SmartScriptTable table, int idx) { table->SetAt(idx, val.c_str()); }

template<class T>
void ArgToLuaTable(const SUIArguments& args, SmartScriptTable table, int idx)
{
	T val;
	args.GetArgNoConversation(idx, val);
	SetTableVal<T>(val, table, idx);
}

template<class T>
bool TryArgToLuaTable(const SUIArguments& args, SmartScriptTable table, int idx)
{
	T val;
	if (args.GetArg(idx, val))
	{
		SetTableVal<T>(val, table, idx);
		return true;
	}
	return false;
}
}

//------------------------------------------------------------------------
//------------------------------------------------------------------------
//------------------------------------------------------------------------
ScriptAnyValue SUIToLuaConversationHelper::UIValueToLuaValue(const TUIData& value, bool& ok)
{
	ok = true;
	switch (value.GetType())
	{
	case eUIDT_Bool:
		return SUIConvHelperTmpl::GetValueRaw<bool>(value);
	case eUIDT_Int:
		return SUIConvHelperTmpl::GetValueRaw<int>(value);
	case eUIDT_EntityId:
		return SUIConvHelperTmpl::GetValueRaw<EntityId>(value);
	case eUIDT_Float:
		return SUIConvHelperTmpl::GetValueRaw<float>(value);
	case eUIDT_Vec3:
		return SUIConvHelperTmpl::GetValueRaw<Vec3>(value);
	case eUIDT_String:
	case eUIDT_WString:
		{
			string val;
			if (value.GetValueWithConversion(val))
				return ScriptAnyValue(val.c_str());
		}
	}
	ok = false;
	return ScriptAnyValue(false);
}

//------------------------------------------------------------------------
bool SUIToLuaConversationHelper::LuaTableToUIArgs(SmartScriptTable table, SUIArguments& args)
{
	for (int i = 0; i <= table->Count(); ++i)
	{
		if (i == 0 && table->GetAtType(0) == svtNull)
			continue; // if passing {arg1, arg2, arg3} to scriptbind first entry will be nil

		TUIData val;
		if (!SUIConvHelperTmpl::LuaArgToUIArgImpl(table, i, val))
			return false;
		args.AddArgument(val);
	}
	return true;
}

//------------------------------------------------------------------------
bool SUIToLuaConversationHelper::LuaArgsToUIArgs(IFunctionHandler* pH, int startIdx, SUIArguments& args)
{
	for (int i = startIdx; i <= pH->GetParamCount(); ++i)
	{
		TUIData val;
		if (!LuaArgToUIArg(pH, i, val))
		{
			return false;
		}
		args.AddArgument(val);
	}
	return true;
}

//------------------------------------------------------------------------
bool SUIToLuaConversationHelper::LuaArgToUIArg(IFunctionHandler* pH, int idx, TUIData& value)
{
	if (idx >= 0 && idx <= pH->GetParamCount())
		return SUIConvHelperTmpl::LuaArgToUIArgImpl(pH, idx, value);
	return false;
}

//------------------------------------------------------------------------
bool SUIToLuaConversationHelper::UIArgsToLuaTable(const SUIArguments& args, SmartScriptTable table)
{
	bool res = true;
	for (int i = 0; i < args.GetArgCount(); ++i)
	{
		switch (args.GetArgType(i))
		{
		case eUIDT_Bool:
			SUIConvHelperTmpl::ArgToLuaTable<bool>(args, table, i);
			break;
		case eUIDT_Int:
			SUIConvHelperTmpl::ArgToLuaTable<int>(args, table, i);
			break;
		case eUIDT_Float:
			SUIConvHelperTmpl::ArgToLuaTable<float>(args, table, i);
			break;
		case eUIDT_EntityId:
			SUIConvHelperTmpl::ArgToLuaTable<EntityId>(args, table, i);
			break;
		case eUIDT_Vec3:
			SUIConvHelperTmpl::ArgToLuaTable<Vec3>(args, table, i);
			break;
		case eUIDT_String:
		case eUIDT_WString:
			SUIConvHelperTmpl::ArgToLuaTable<string>(args, table, i);
			break;
		case eUIDT_Any:
			{
				bool ok = SUIConvHelperTmpl::TryArgToLuaTable<int>(args, table, i)
				          || SUIConvHelperTmpl::TryArgToLuaTable<float>(args, table, i)
				          || SUIConvHelperTmpl::TryArgToLuaTable<string>(args, table, i);
				res &= ok;
			}
		default:
			res = false;
		}
	}
	return res;
}

//------------------------------------------------------------------------
