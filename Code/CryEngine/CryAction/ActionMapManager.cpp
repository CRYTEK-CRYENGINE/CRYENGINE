// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$

   -------------------------------------------------------------------------
   History:
   - 7:9:2004   17:42 : Created by Márcio Martins
   - 15:9:2010  12:30 : Revised by Dean Claassen

*************************************************************************/
#include "StdAfx.h"
#include "ActionMapManager.h"
#include "ActionMap.h"
#include "ActionFilter.h"
#include <CryCore/CryCrc32.h>
#include "GameObjects/GameObject.h"
#include <CryCore/Platform/IPlatformOS.h>

CActionMapManager* CActionMapManager::s_pThis = nullptr;

//------------------------------------------------------------------------
CActionMapManager::CActionMapManager(IInput* pInput)
	: m_loadedXMLPath(""),
	m_pInput(pInput),
	m_enabled(true),
	m_bRefiringInputs(false),
	m_bDelayedRemoveAllRefiringData(false),
	m_bIncomingInputRepeated(false),
	m_bRepeatedInputHoldTriggerFired(false),
	m_currentInputKeyID(eKI_Unknown),
	m_defaultEntityId(INVALID_ENTITYID),
	m_version(-1)    // the first actionmap/filter sets the version
{
	CRY_ASSERT(!s_pThis);
	s_pThis = this;

	if (m_pInput)
	{
		m_pInput->AddEventListener(this);
	}

#ifndef _RELEASE
	i_listActionMaps = 0;
	REGISTER_CVAR(i_listActionMaps, i_listActionMaps, 0, "List action maps/filters on screen (1 = list all, 2 = list blocked inputs only)");
	REGISTER_COMMAND("i_reloadActionMaps", ReloadActionMaps, VF_CHEAT, "Reloads action maps");
#endif
}

//------------------------------------------------------------------------
CActionMapManager::~CActionMapManager()
{
	if (m_pInput)
	{
		m_pInput->RemoveEventListener(this);
	}

	Clear();
	RemoveAllAlwaysActionListeners();
}

//------------------------------------------------------------------------
bool CActionMapManager::OnInputEvent(const SInputEvent& event)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (!m_enabled)
		return false;

	// no actions while console is open
	if (gEnv->pConsole->IsOpened())
		return false;

	if (gEnv->IsEditor() && gEnv->pGameFramework->IsEditing())
		return false;

	if (event.keyName.c_str() && event.keyName.c_str()[0] == 0)
		return false;

	// Filter alt+enter which toggles between fullscreen and windowed mode and shouldn't be processed here
	if ((event.modifiers & eMM_Alt) && event.keyId == eKI_Enter)
		return false;

	// Check if input is repeated and record (Used for bUseHoldTriggerDelayRepeatOverride)
	if (event.keyId < eKI_SYS_Commit) // Ignore special types and unknown
	{
		if (m_currentInputKeyID != event.keyId)
		{
			m_currentInputKeyID = event.keyId;
			m_bIncomingInputRepeated = false;
			SetRepeatedInputHoldTriggerFired(false);
		}
		else
		{
			m_bIncomingInputRepeated = true;
		}
	}

	// Process priority list based on event
	TBindPriorityList priorityList;
	bool bHasItems = CreateEventPriorityList(event, priorityList);
	if (bHasItems)
	{
		return HandleAcceptedEvents(event, priorityList);
	}

	return false; // Not handled
}

//------------------------------------------------------------------------
void CActionMapManager::Update()
{
	if (!m_enabled)
	{
		return;
	}

	// Process refired events
	UpdateRefiringInputs();

	for (TActionMapMap::iterator it = m_actionMaps.begin(); it != m_actionMaps.end(); ++it)
	{
		CActionMap* pAm = it->second;
		if (pAm->Enabled())
		{
			pAm->InputProcessed();
		}
	}

#ifndef _RELEASE
	if (i_listActionMaps)
	{
		const float color[4] = { 1, 1, 1, 1 };
		const float textSize = 1.5f;
		const bool renderAll = (i_listActionMaps != 2);
		const float xMargin = 40.f;

		IFFont* defaultFont = gEnv->pCryFont->GetFont("default");
		float yPos = gEnv->pRenderer->GetOverlayHeight() - 100.f;
		float secondColumnOffset = 0.f;

		STextDrawContext ctx;
		ctx.SetSizeIn800x600(false);
		ctx.SetSize(Vec2(UIDRAW_TEXTSIZEFACTOR * textSize, UIDRAW_TEXTSIZEFACTOR * textSize));

		if (defaultFont)
		{
			TActionFilterMap::iterator it = m_actionFilters.begin();
			TActionFilterMap::iterator itEnd = m_actionFilters.end();
			while (it != itEnd)
			{
				const bool isEnabled = it->second->Enabled();

				if (renderAll || isEnabled)
				{
					string message;
					message.Format("%sFilter '%s' %s", isEnabled ? "$7" : "$5", it->second->GetName(), isEnabled ? "blocking inputs" : "allowing inputs");

					const float width = defaultFont->GetTextSize(message, true, ctx).x + 5.f;
					secondColumnOffset = max(width, secondColumnOffset);

					IRenderAuxText::Draw2dLabel(xMargin, yPos, 1.5f, color, false, "%s", message.c_str());
					yPos -= 15.f;
				}

				++it;
			}
		}

		yPos = gEnv->pRenderer->GetOverlayHeight() - 100.f;

		for (TActionMapMap::iterator it = m_actionMaps.begin(); it != m_actionMaps.end(); ++it)
		{
			CActionMap* pAm = it->second;
			const bool isEnabled = pAm->Enabled();
			if (renderAll || !isEnabled)
			{
				IRenderAuxText::Draw2dLabel(xMargin + secondColumnOffset, yPos, 1.5f, color, false, "%sAction map '%s' %s", isEnabled ? "$3" : "$4", pAm->GetName(), isEnabled ? "enabled" : "disabled");
				yPos -= 15.f;
			}
		}
	}
#endif
}

//------------------------------------------------------------------------
void CActionMapManager::Reset()
{
	for (TActionMapMap::iterator it = m_actionMaps.begin(); it != m_actionMaps.end(); ++it)
	{
		it->second->Reset();
	}
}

//------------------------------------------------------------------------
void CActionMapManager::Clear()
{
	RemoveAllActionMaps();
	RemoveAllFilters();

	m_preloadedControllerLayouts.clear();
}

//------------------------------------------------------------------------
bool CActionMapManager::InitActionMaps(const char* filename)
{
	XmlNodeRef rootNode = GetISystem()->LoadXmlFromFile(filename);
	if (rootNode)
	{
		Clear();
		ClearInputDevicesMappings();
		ClearStoredCurrentInputData();

		XmlNodeRef platformsNode = rootNode->findChild("platforms");
		if (platformsNode)
		{
			XmlNodeRef platform = platformsNode->findChild(GetISystem()->GetPlatformOS()->GetPlatformName());
			if (platform)
			{
				BYTE devices(eAID_KeyboardMouse | eAID_XboxPad | eAID_PS4Pad | eAID_OculusTouch);

				if (!strcmp(platform->getAttr("keyboard"), "0"))      devices &= ~eAID_KeyboardMouse;
				if (!strcmp(platform->getAttr("xboxpad"), "0"))       devices &= ~eAID_XboxPad;
				if (!strcmp(platform->getAttr("ps4pad"), "0"))        devices &= ~eAID_PS4Pad;
				if (!strcmp(platform->getAttr("oculustouch"), "0"))   devices &= ~eAID_OculusTouch;

				if (devices & eAID_KeyboardMouse) AddInputDeviceMapping(eAID_KeyboardMouse, "keyboard");
				if (devices & eAID_XboxPad)       AddInputDeviceMapping(eAID_XboxPad, "xboxpad");
				if (devices & eAID_PS4Pad)        AddInputDeviceMapping(eAID_PS4Pad, "ps4pad");
				if (devices & eAID_OculusTouch)   AddInputDeviceMapping(eAID_OculusTouch, "oculustouch");

				SetLoadFromXMLPath(filename);
				if (LoadFromXML(rootNode))
				{
					BroadcastActionMapEvent(SActionMapEvent(SActionMapEvent::eActionMapManagerEvent_ActionMapsInitialized, (UINT_PTR)0, (UINT_PTR)0));
					return true;
				}
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CActionMapManager::InitActionMaps: Failed to find platform defintions for '%s', action mappings loading will fail", GetISystem()->GetPlatformOS()->GetPlatformName());
			}
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CActionMapManager::InitActionMaps: Failed to find XML node 'platforms', action mappings loading will fail");
		}
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CActionMapManager::InitActionMaps: Failed to open %s, action mappings loading will fail", filename);
	}
	return false;
};

//------------------------------------------------------------------------
bool CActionMapManager::LoadFromXML(const XmlNodeRef& node)
{
	int version = -1;
	if (!node->getAttr("version", version))
	{
		GameWarning("Obsolete action map format - version info is missing");
		return false;
	}
	m_version = version;

	//	load action maps
	int nChildren = node->getChildCount();
	for (int i = 0; i < nChildren; ++i)
	{
		XmlNodeRef child = node->getChild(i);
		if (!strcmp(child->getTag(), "actionmap"))
		{
			const char* actionMapName = child->getAttr("name");
			IActionMap* pActionMap = CreateActionMap(actionMapName);
			if (!pActionMap)
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CActionMapManager::LoadFromXML: Unable to create actionmap: %s", actionMapName);
				continue;
			}

			bool bResult = pActionMap->LoadFromXML(child);
			if (!bResult)
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CActionMapManager::LoadFromXML: Failed to serialize actionmap: %s", actionMapName);
				continue;
			}
		}

		if (!strcmp(child->getTag(), "actionfilter"))
		{
			const char* actionFilterName = child->getAttr("name");
			const char* actionFilterType = child->getAttr("type");
			IActionFilter* pActionFilter = CreateActionFilter(actionFilterName, !stricmp(actionFilterType, "actionPass") ? eAFT_ActionPass : eAFT_ActionFail);
			if (!pActionFilter)
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CActionMapManager::LoadFromXML: Unable to create actionfilter: %s", actionFilterName);
				continue;
			}

			bool bResult = pActionFilter->SerializeXML(child, true);
			if (!bResult)
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CActionMapManager::SerializeXML: Failed to serialize actionfilter: %s", actionFilterName);
				continue;
			}
		}

		if (!strcmp(child->getTag(), "controllerlayouts"))
		{
			int nLayoutChildren = child->getChildCount();
			for (int j = 0; j < nLayoutChildren; ++j)
			{
				XmlNodeRef layoutChild = child->getChild(j);
				if (!PreloadControllerLayout(layoutChild))
				{
					CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CActionMapManager::SerializeXML: Failed to serialize controllerlayout in line %i", j);
				}
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------
bool CActionMapManager::PreloadControllerLayout(const XmlNodeRef& controllerLayout)
{
	const char* szLayoutName = controllerLayout->getAttr("name");
	const char* szLayoutFile = controllerLayout->getAttr("file");

	if (!strcmp(szLayoutName, "") || !strcmp(szLayoutFile, ""))
		return false;

	string layoutPath;
	layoutPath.Format("libs/config/controller/%s", szLayoutFile);

	XmlNodeRef rootNode = GetISystem()->LoadXmlFromFile(layoutPath);
	if (!rootNode)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CActionMapManager::PreloadControllerLayout: Failed to load xmlfile: %s", layoutPath.c_str());
		return false;
	}

	m_preloadedControllerLayouts[szLayoutName] = rootNode;

	return true;
}

bool CActionMapManager::LoadControllerLayoutFile(const char* szLayoutKeyName)
{
	CRY_ASSERT(szLayoutKeyName != NULL);

	TControllerLayouts::const_iterator iter = m_preloadedControllerLayouts.find(szLayoutKeyName);
	if (iter == m_preloadedControllerLayouts.end())
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CActionMapManager::LoadControllerLayoutFile: Failed to find preloaded controller layout for key: %s", szLayoutKeyName);
		return false;
	}

	XmlNodeRef rootNode = iter->second;
	CRY_ASSERT(rootNode != NULL);

	if (!LoadRebindDataFromXML(rootNode))
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CActionMapManager::LoadControllerLayoutFile: Failed to load controller layout: %s", szLayoutKeyName);
		return false;
	}

	return true;
}

//------------------------------------------------------------------------
bool CActionMapManager::LoadRebindDataFromXML(const XmlNodeRef& node)
{
	int ignoreVersion = 0;
	node->getAttr("ignoreVersion", ignoreVersion);

	if (ignoreVersion == 0)
	{
		int version = -1;
		if (node->getAttr("version", version))
		{
			if (version != m_version)
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CActionMapManager::LoadRebindDataFromXML: Failed, version found %d -> required %d", version, m_version);
				return false;
			}
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CActionMapManager::LoadRebindDataFromXML: Failed, obsolete action map format - version info is missing");
			return false;
		}
	}

	//	load action maps
	int nChildren = node->getChildCount();
	for (int i = 0; i < nChildren; ++i)
	{
		XmlNodeRef child = node->getChild(i);
		if (!strcmp(child->getTag(), "actionmap"))
		{
			const char* actionMapName = child->getAttr("name");
			IActionMap* pActionMap = GetActionMap(actionMapName);
			if (!pActionMap)
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CActionMapManager::LoadRebindDataFromXML: Failed to find actionmap: %s", actionMapName);
				continue;
			}

			bool bResult = pActionMap->LoadRebindingDataFromXML(child);
			if (!bResult)
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CActionMapManager::LoadRebindDataFromXML: Failed to load rebind data for actionmap: %s", actionMapName);
				continue;
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------
bool CActionMapManager::SaveRebindDataToXML(XmlNodeRef& node)
{
	node->setAttr("version", m_version);
	TActionMapMap::iterator iter = m_actionMaps.begin();
	TActionMapMap::iterator iterEnd = m_actionMaps.end();
	for (; iter != iterEnd; ++iter)
	{
		CActionMap* pActionMap = iter->second;
		CRY_ASSERT(pActionMap != NULL);

		if (pActionMap->GetNumRebindedInputs() == 0)
			continue;

		XmlNodeRef child = node->newChild("actionmap");
		bool bResult = pActionMap->SaveRebindingDataToXML(child);
		if (!bResult)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CActionMapManager::SaveRebindDataToXML: Failed to save rebind data for actionmap: %s", pActionMap->GetName());
		}
	}

	return true;
}

//------------------------------------------------------------------------
bool CActionMapManager::AddExtraActionListener(IActionListener* pExtraActionListener, const char* actionMap)
{
	if ((actionMap != NULL) && (actionMap[0] != '\0'))
	{
		TActionMapMap::iterator iActionMap = m_actionMaps.find(CONST_TEMP_STRING(actionMap));
		if (iActionMap != m_actionMaps.end())
		{
			iActionMap->second->AddExtraActionListener(pExtraActionListener);
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		stl::push_back_unique(m_ExtraActionListeners, pExtraActionListener);
		return true;
	}
}

//------------------------------------------------------------------------
bool CActionMapManager::RemoveExtraActionListener(IActionListener* pExtraActionListener, const char* actionMap)
{
	if ((actionMap != NULL) && (actionMap[0] != '\0'))
	{
		TActionMapMap::iterator iActionMap = m_actionMaps.find(CONST_TEMP_STRING(actionMap));
		if (iActionMap != m_actionMaps.end())
		{
			iActionMap->second->RemoveExtraActionListener(pExtraActionListener);
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		stl::find_and_erase(m_ExtraActionListeners, pExtraActionListener);
		return true;
	}
}

//------------------------------------------------------------------------
const TActionListeners& CActionMapManager::GetExtraActionListeners() const
{
	return m_ExtraActionListeners;
}

//------------------------------------------------------------------------
bool CActionMapManager::AddFlowgraphNodeActionListener(IActionListener* pFlowgraphNodeActionListener, const char* actionMap)
{
	if ((actionMap != NULL) && (actionMap[0] != '\0'))
	{
		TActionMapMap::iterator	iActionMap = m_actionMaps.find(CONST_TEMP_STRING(actionMap));
		if(iActionMap != m_actionMaps.end())
		{
			iActionMap->second->AddFlowNodeActionListener(pFlowgraphNodeActionListener);
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		stl::push_back_unique(m_ExtraActionListeners, pFlowgraphNodeActionListener);
		return true;
	}
}

//------------------------------------------------------------------------
bool CActionMapManager::RemoveFlowgraphNodeActionListener(IActionListener* pFlowgraphNodeActionListener, const char* actionMap)
{
	if ((actionMap != NULL) && (actionMap[0] != '\0'))
	{
		TActionMapMap::iterator	iActionMap = m_actionMaps.find(CONST_TEMP_STRING(actionMap));
		if(iActionMap != m_actionMaps.end())
		{
			iActionMap->second->RemoveFlowNodeActionListener(pFlowgraphNodeActionListener);
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		stl::find_and_erase(m_ExtraActionListeners, pFlowgraphNodeActionListener);
		return true;
	}
}

//------------------------------------------------------------------------
void CActionMapManager::AddAlwaysActionListener(TBlockingActionListener pActionListener)
{
	// Don't add if already exists
	TBlockingActionListeners::const_iterator iter = std::find(m_alwaysActionListeners.begin(), m_alwaysActionListeners.end(), pActionListener);
	if (iter != m_alwaysActionListeners.end())
		return;

	m_alwaysActionListeners.push_back(TBlockingActionListener(pActionListener));
}

//------------------------------------------------------------------------
void CActionMapManager::RemoveAlwaysActionListener(TBlockingActionListener pActionListener)
{
	TBlockingActionListeners::iterator iter = std::find(m_alwaysActionListeners.begin(), m_alwaysActionListeners.end(), pActionListener);
	if (iter == m_alwaysActionListeners.end())
		return;

	m_alwaysActionListeners.erase(iter);
}

//------------------------------------------------------------------------
void CActionMapManager::RemoveAllAlwaysActionListeners()
{
	m_alwaysActionListeners.clear();
}

//------------------------------------------------------------------------
IActionMap* CActionMapManager::CreateActionMap(const char* name)
{
	if (GetActionMap(name) != NULL)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CActionMapManager::CreateActionMap: Failed to create actionmap: %s, already exists", name);
		return NULL;
	}

	CActionMap* pActionMap = new CActionMap(this, name);
	if (pActionMap && (m_defaultEntityId != INVALID_ENTITYID))
	{
		pActionMap->SetActionListener(m_defaultEntityId);
	}

	m_actionMaps.insert(TActionMapMap::value_type(name, pActionMap));
	return pActionMap;
}

//------------------------------------------------------------------------
bool CActionMapManager::RemoveActionMap(const char* name)
{
	TActionMapMap::iterator it = m_actionMaps.find(CONST_TEMP_STRING(name));
	if (it == m_actionMaps.end())
		return false;

	CActionMap* pActionMap = it->second;
	CRY_ASSERT(pActionMap != NULL);

	RemoveBind(pActionMap);

	SAFE_DELETE(pActionMap);
	m_actionMaps.erase(it);

	return true;
}

//------------------------------------------------------------------------
void CActionMapManager::RemoveAllActionMaps()
{
	TActionMapMap::iterator actionMapit = m_actionMaps.begin();
	TActionMapMap::iterator actionMapitEnd = m_actionMaps.end();
	for (; actionMapit != actionMapitEnd; ++actionMapit)
	{
		CActionMap* pActionMap = actionMapit->second;
		CRY_ASSERT(pActionMap != NULL);
		SAFE_DELETE(pActionMap);
	}
	m_actionMaps.clear();
	m_inputCRCToBind.clear();

	RemoveAllRefireData();
}

//------------------------------------------------------------------------
void CActionMapManager::RemoveAllFilters()
{
	TActionFilterMap::iterator filtersIt = m_actionFilters.begin();
	TActionFilterMap::iterator filtersItEnd = m_actionFilters.end();
	for (; filtersIt != filtersItEnd; ++filtersIt)
	{
		CActionFilter* pFilter = filtersIt->second;
		CRY_ASSERT(pFilter != NULL);
		SAFE_DELETE(pFilter);
	}
	m_actionFilters.clear();
}

//------------------------------------------------------------------------
IActionMap* CActionMapManager::GetActionMap(const char* name)
{
	TActionMapMap::iterator it = m_actionMaps.find(CONST_TEMP_STRING(name));
	if (it == m_actionMaps.end())
	{
		return NULL;
	}
	else
	{
		return it->second;
	}
}

//------------------------------------------------------------------------
const IActionMap* CActionMapManager::GetActionMap(const char* name) const
{
	TActionMapMap::const_iterator it = m_actionMaps.find(CONST_TEMP_STRING(name));
	if (it == m_actionMaps.end())
		return NULL;
	else
		return it->second;
}

//------------------------------------------------------------------------
IActionFilter* CActionMapManager::CreateActionFilter(const char* name, EActionFilterType type)
{
	IActionFilter* pActionFilter = GetActionFilter(name);
	if (pActionFilter)
		return NULL;

	CActionFilter* pFilter = new CActionFilter(this, m_pInput, name, type);
	m_actionFilters.insert(TActionFilterMap::value_type(name, pFilter));

	return pFilter;
}

//------------------------------------------------------------------------
IActionFilter* CActionMapManager::GetActionFilter(const char* name)
{
	TActionFilterMap::iterator it = m_actionFilters.find(CONST_TEMP_STRING(name));
	if (it != m_actionFilters.end())
	{
		return it->second;
	}

	return NULL;
}

//------------------------------------------------------------------------
void CActionMapManager::Enable(const bool enable, const bool resetStateOnDisable)
{
	m_enabled = enable;

	if (!enable && resetStateOnDisable)
	{
		TActionMapMap::iterator it = m_actionMaps.begin();
		TActionMapMap::iterator itEnd = m_actionMaps.end();
		for (; it != itEnd; ++it)
		{
			CActionMap* pActionMap = it->second;
			CRY_ASSERT(pActionMap != NULL);
			pActionMap->ReleaseActionsIfActive(); // Releases pressed held state

			// Also need to clear repeat data
			RemoveAllRefireData();
		}
	}
}

//------------------------------------------------------------------------
void CActionMapManager::EnableActionMap(const char* name, bool enable)
{
	if (name == 0 || *name == 0)
	{
		TActionMapMap::iterator it = m_actionMaps.begin();
		TActionMapMap::iterator itEnd = m_actionMaps.end();
		while (it != itEnd)
		{
			it->second->Enable(enable);
			++it;
		}
	}
	else
	{
		TActionMapMap::iterator it = m_actionMaps.find(CONST_TEMP_STRING(name));

		if (it != m_actionMaps.end())
		{
			if (it->second->Enabled() != enable)
			{
				it->second->Enable(enable);
				ReleaseFilteredActions();
				BroadcastActionMapEvent(SActionMapEvent(SActionMapEvent::eActionMapManagerEvent_ActionMapStatusChanged, (UINT_PTR)it->second, (UINT_PTR)enable));
			}
		}
	}
}

//------------------------------------------------------------------------
void CActionMapManager::EnableFilter(const char* name, bool enable)
{
	if (name == 0 || *name == 0)
	{
		TActionFilterMap::iterator it = m_actionFilters.begin();
		TActionFilterMap::iterator itEnd = m_actionFilters.end();
		while (it != itEnd)
		{
			it->second->Enable(enable);
			++it;
		}
	}
	else
	{
		TActionFilterMap::iterator it = m_actionFilters.find(CONST_TEMP_STRING(name));

		if (it != m_actionFilters.end())
		{
			if (it->second->Enabled() != enable)
			{
				it->second->Enable(enable);
				ReleaseFilteredActions();
				BroadcastActionMapEvent(SActionMapEvent(SActionMapEvent::eActionMapManagerEvent_FilterStatusChanged, (UINT_PTR)name, (UINT_PTR)enable));
			}
		}
	}
}

//------------------------------------------------------------------------
void CActionMapManager::ReleaseFilteredActions()
{
	for (TActionMapMap::iterator it = m_actionMaps.begin(), eit = m_actionMaps.end(); it != eit; ++it)
	{
		if (it->second->Enabled())
		{
			it->second->ReleaseFilteredActions();
		}
	}
}

//------------------------------------------------------------------------
void CActionMapManager::ClearStoredCurrentInputData()
{
	m_currentInputKeyID = eKI_Unknown;
	m_bIncomingInputRepeated = false;
}

//------------------------------------------------------------------------
bool CActionMapManager::ReBindActionInput(const char* actionMapName, const ActionId& actionId, const char* szCurrentInput, const char* szNewInput)
{
	IActionMap* pActionMap = GetActionMap(actionMapName);
	if (pActionMap)
	{
		return pActionMap->ReBindActionInput(actionId, szCurrentInput, szNewInput);
	}

	GameWarning("CActionMapManager::ReBindActionInput: Failed to find actionmap: %s", actionMapName);
	return false;
}

//------------------------------------------------------------------------
const SActionInput* CActionMapManager::GetActionInput(const char* actionMapName, const ActionId& actionId, const EActionInputDevice device, const int iByDeviceIndex) const
{
	const IActionMap* pActionMap = GetActionMap(actionMapName);
	if (!pActionMap)
	{
		GameWarning("CActionMapManager::GetActionInput: Failed to find actionmap: %s", actionMapName);
		return NULL;
	}

	const IActionMapAction* pActionMapAction = pActionMap->GetAction(actionId);
	if (!pActionMapAction)
	{
		GameWarning("CActionMapManager::GetActionInput: Failed to find action:%s in actionmap: %s", actionId.c_str(), actionMapName);
		return NULL;
	}

	const SActionInput* pActionInput = pActionMapAction->GetActionInput(device, iByDeviceIndex);
	if (!pActionInput)
	{
		GameWarning("CActionMapManager::GetActionInput: Failed to find action input with deviceid: %d and index: %d, in action:%s in actionmap: %s", (int)device, iByDeviceIndex, actionId.c_str(), actionMapName);
		return NULL;
	}

	return pActionInput;
}

//------------------------------------------------------------------------
bool CActionMapManager::IsFilterEnabled(const char* name)
{
	TActionFilterMap::iterator it = m_actionFilters.find(CONST_TEMP_STRING(name));

	if (it != m_actionFilters.end())
	{
		return it->second->Enabled();
	}

	return false;
}

//------------------------------------------------------------------------
bool CActionMapManager::ActionFiltered(const ActionId& action)
{
	for (TActionFilterMap::iterator it = m_actionFilters.begin(); it != m_actionFilters.end(); ++it)
	{
		if (it->second->ActionFiltered(action))
		{
			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------
bool CActionMapManager::CreateEventPriorityList(const SInputEvent& inputEvent, TBindPriorityList& priorityList)
{
	bool bRes = false;

	// Look up the actions which are associated with this
	uint32 inputCRC = CCrc32::ComputeLowercase(inputEvent.keyName.c_str());
	TInputCRCToBind::iterator itCRCBound = m_inputCRCToBind.lower_bound(inputCRC);
	TInputCRCToBind::const_iterator itCRCBoundEnd = m_inputCRCToBind.end();

	// Create priority listing for which processes should be fired
	float fLongestHeldTime = 0.0f;
	for (; itCRCBound != itCRCBoundEnd && itCRCBound->first == inputCRC; ++itCRCBound)
	{
		SBindData& bindData = itCRCBound->second;
		SActionInput* pActionInput = bindData.m_pActionInput;
		CRY_ASSERT(pActionInput != NULL);
		CActionMapAction* pAction = bindData.m_pAction;
		CRY_ASSERT(pAction != NULL);
		CActionMap* pActionMap = bindData.m_pActionMap;
		CRY_ASSERT(pActionMap != NULL);

#ifdef _DEBUG
		CRY_ASSERT((pActionInput->inputCRC == inputCRC) && (strcmp(pActionInput->input.c_str(), inputEvent.keyName.c_str()) == 0));

		if ((pActionInput->inputCRC == inputCRC) && (strcmp(pActionInput->input.c_str(), inputEvent.keyName.c_str()) != 0))
		{
			GameWarning("Hash values are identical, but values are different! %s - %s", pActionInput->input.c_str(), inputEvent.keyName.c_str());
		}
		if ((pActionInput->inputCRC != inputCRC) && (strcmp(pActionInput->input.c_str(), inputEvent.keyName.c_str()) == 0))
		{
			GameWarning("Hash values are different, but values are identical! %s - %s", pActionInput->input.c_str(), inputEvent.keyName.c_str());
		}

#endif //_DEBUG

		bool bAdd = (!ActionFiltered(pAction->GetActionId()) && pActionMap->CanProcessInput(inputEvent, pActionMap, pAction, pActionInput));

		if (bAdd)
		{
			priorityList.push_back(&bindData);
			bRes = true;
		}
	}

	return bRes;
}

//------------------------------------------------------------------------
bool CActionMapManager::ProcessAlwaysListeners(const ActionId& action, int activationMode, float value, const SInputEvent& inputEvent)
{
	TBlockingActionListeners::iterator iter = m_alwaysActionListeners.begin();
	TBlockingActionListeners::iterator iterEnd = m_alwaysActionListeners.end();

	bool bHandled = false;
	for (; iter != iterEnd; ++iter)
	{
		TBlockingActionListener& listener = *iter;
		bHandled = listener->OnAction(action, activationMode, value, inputEvent);
		if (bHandled)
		{
			break;
		}
	}

	return bHandled;
}

//------------------------------------------------------------------------
void CActionMapManager::RemoveBind(CActionMap* pActionMap)
{
	for (TInputCRCToBind::iterator iter = m_inputCRCToBind.begin(); iter != m_inputCRCToBind.end(); )
	{
		SBindData* pBindData = &((*iter).second);
		if (pBindData->m_pActionMap == pActionMap)
		{
			TInputCRCToBind::iterator iterDelete = iter++;
			m_inputCRCToBind.erase(iterDelete);
		}
		else
		{
			++iter;
		}
	}

	RemoveRefireData(pActionMap);
}

//------------------------------------------------------------------------
void CActionMapManager::RemoveBind(CActionMapAction* pAction)
{
	for (TInputCRCToBind::iterator iter = m_inputCRCToBind.begin(); iter != m_inputCRCToBind.end(); )
	{
		SBindData* pBindData = &((*iter).second);
		if (pBindData->m_pAction == pAction)
		{
			TInputCRCToBind::iterator iterDelete = iter++;
			m_inputCRCToBind.erase(iterDelete);
		}
		else
		{
			++iter;
		}
	}

	RemoveRefireData(pAction);
}

//------------------------------------------------------------------------
void CActionMapManager::RemoveActionFilter(CActionFilter* pActionFilter)
{
	for (TActionFilterMap::iterator it = m_actionFilters.begin(); it != m_actionFilters.end(); ++it)
	{
		if (it->second == pActionFilter)
		{
			m_actionFilters.erase(it);

			return;
		}
	}
}

//------------------------------------------------------------------------
void CActionMapManager::ReleaseActionIfActive(const ActionId& actionId)
{
	TActionMapMap::iterator it = m_actionMaps.begin();
	TActionMapMap::const_iterator end = m_actionMaps.end();
	for (; it != end; ++it)
	{
		it->second->ReleaseActionIfActive(actionId);
	}
}
//------------------------------------------------------------------------
bool CActionMapManager::AddBind(CActionMap* pActionMap, CActionMapAction* pAction, SActionInput* pActionInput)
{
	if (pActionInput->inputCRC == 0)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CActionMapManager::AddBind: Failed to add, missing crc value");
		return false;
	}

	SBindData* pBindData = GetBindData(pActionMap, pAction, pActionInput);
	if (pBindData != NULL)
	{
		pActionInput->inputCRC = 0;
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CActionMapManager::AddBind: Failed to add, already exists");
		return false;
	}

	SBindData bindData(pActionMap, pAction, pActionInput);

	m_inputCRCToBind.insert(std::pair<uint32, SBindData>(pActionInput->inputCRC, bindData));

	return true;
}

//------------------------------------------------------------------------
bool CActionMapManager::RemoveBind(CActionMap* pActionMap, CActionMapAction* pAction, SActionInput* pActionInput)
{
	uint32 inputCRC = pActionInput->inputCRC;

	TInputCRCToBind::iterator iter = m_inputCRCToBind.lower_bound(inputCRC);
	TInputCRCToBind::const_iterator end = m_inputCRCToBind.end();

	for (; iter != end && iter->first == inputCRC; ++iter)
	{
		SBindData& bindData = iter->second;

		if (bindData.m_pActionMap == pActionMap && bindData.m_pAction == pAction && bindData.m_pActionInput->input == pActionInput->input)
		{
			m_inputCRCToBind.erase(iter);
			RemoveRefireData(pActionMap, pAction, pActionInput);
			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------
bool CActionMapManager::HasBind(CActionMap* pActionMap, CActionMapAction* pAction, SActionInput* pActionInput) const
{
	uint32 inputCRC = pActionInput->inputCRC;

	TInputCRCToBind::const_iterator iter = m_inputCRCToBind.lower_bound(inputCRC);
	TInputCRCToBind::const_iterator end = m_inputCRCToBind.end();

	for (; iter != end && iter->first == inputCRC; ++iter)
	{
		const SBindData& bindData = iter->second;

		if (bindData.m_pActionMap == pActionMap && bindData.m_pAction == pAction && bindData.m_pActionInput->input == pActionInput->input)
		{
			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------
bool CActionMapManager::UpdateRefireData(const SInputEvent& event, CActionMap* pActionMap, CActionMapAction* pAction, SActionInput* pActionInput)
{
	SRefireData* pRefireData = NULL;
	TInputCRCToRefireData::iterator iter = m_inputCRCToRefireData.find(pActionInput->inputCRC);
	if (iter != m_inputCRCToRefireData.end())
	{
		pRefireData = iter->second;
		CRY_ASSERT(pRefireData != NULL);

		// Search if match exists
		SRefireBindData* pFoundBindData = NULL;
		TRefireBindData& refireBindData = pRefireData->m_refireBindData;
		TRefireBindData::iterator refireBindDataIt = refireBindData.begin();
		TRefireBindData::const_iterator refireBindDataItEnd = refireBindData.end();
		for (; refireBindDataIt != refireBindDataItEnd; ++refireBindDataIt)
		{
			SRefireBindData& singleRefireBindData = *refireBindDataIt;
			SBindData& bindData = singleRefireBindData.m_bindData;
			if (bindData.m_pActionMap == pActionMap && bindData.m_pAction == pAction && bindData.m_pActionInput == pActionInput)
			{
				pFoundBindData = &singleRefireBindData;
			}
		}

		if (pFoundBindData) // Just update input since analog value could have changed or delayed press needing a release
		{
			pRefireData->m_inputEvent = event;
			pFoundBindData->m_bIgnoreNextUpdate = true;
		}
		else
		{
			refireBindData.push_back(SRefireBindData(pActionMap, pAction, pActionInput));
			refireBindData[refireBindData.size() - 1].m_bIgnoreNextUpdate = true;
		}
	}
	else
	{
		pRefireData = new SRefireData(event, pActionMap, pAction, pActionInput);
		CRY_ASSERT(pRefireData->m_refireBindData.empty() == false);
		pRefireData->m_refireBindData[0].m_bIgnoreNextUpdate = true;
		m_inputCRCToRefireData.insert(std::pair<uint32, SRefireData*>(pActionInput->inputCRC, pRefireData));
	}

	return true;
}

//------------------------------------------------------------------------
bool CActionMapManager::RemoveRefireData(CActionMap* pActionMap, CActionMapAction* pAction, SActionInput* pActionInput)
{
	CRY_ASSERT(m_bRefiringInputs == false); // This should never happen

	uint32 inputCRC = pActionInput->inputCRC;
	if (inputCRC == 0)
		return false;

	TInputCRCToRefireData::iterator iter = m_inputCRCToRefireData.find(inputCRC);
	if (iter == m_inputCRCToRefireData.end())
		return false;

	SRefireData* pRefireData = iter->second;
	CRY_ASSERT(pRefireData != NULL);

	// Try to find the actual match now, just finding the crc, doesn't mean this action input is here

	TRefireBindData& refireBindData = pRefireData->m_refireBindData;
	TRefireBindData::iterator refireBindDataIt = refireBindData.begin();
	TRefireBindData::const_iterator refireBindDataItEnd = refireBindData.end();
	for (; refireBindDataIt != refireBindDataItEnd; ++refireBindDataIt)
	{
		SRefireBindData& singleRefireBindData = *refireBindDataIt;
		SBindData& bindData = singleRefireBindData.m_bindData;
		if (bindData.m_pActionMap == pActionMap && bindData.m_pAction == pAction && bindData.m_pActionInput == pActionInput)
		{
			if (refireBindData.size() > 1) // Don't remove whole mapping, just this specific one
			{
				refireBindData.erase(refireBindDataIt);
			}
			else // No other inputs mapped, remove whole mapping
			{
				SAFE_DELETE(pRefireData);
				m_inputCRCToRefireData.erase(iter);
			}

			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------
void CActionMapManager::RemoveRefireData(CActionMap* pActionMap)
{
	CRY_ASSERT(m_bRefiringInputs == false); // This should never happen

	TInputCRCToRefireData::iterator iter = m_inputCRCToRefireData.begin();
	TInputCRCToRefireData::iterator iterEnd = m_inputCRCToRefireData.end();
	while (iter != iterEnd)
	{
		SRefireData* pRefireData = iter->second;
		CRY_ASSERT(pRefireData != NULL);

		TRefireBindData& refireBindData = pRefireData->m_refireBindData;
		TRefireBindData::iterator refireBindDataIt = refireBindData.begin();
		TRefireBindData::const_iterator refireBindDataItEnd = refireBindData.end();
		while (refireBindDataIt != refireBindDataItEnd)
		{
			SRefireBindData& singleRefireBindData = *refireBindDataIt;
			SBindData& bindData = singleRefireBindData.m_bindData;
			if (bindData.m_pActionMap == pActionMap)
			{
				TRefireBindData::iterator iterDelete = refireBindDataIt++;
				refireBindData.erase(iterDelete);
			}
			else
			{
				++refireBindDataIt;
			}
		}

		if (refireBindData.empty()) // Remove whole mapping if no more items exist for the input
		{
			SAFE_DELETE(pRefireData);
			TInputCRCToRefireData::iterator iterDelete = iter++;
			m_inputCRCToRefireData.erase(iterDelete);
		}
		else
		{
			++iter;
		}
	}
}

//------------------------------------------------------------------------
void CActionMapManager::RemoveRefireData(CActionMapAction* pAction)
{
	CRY_ASSERT(m_bRefiringInputs == false); // This should never happen

	TInputCRCToRefireData::iterator iter = m_inputCRCToRefireData.begin();
	TInputCRCToRefireData::iterator iterEnd = m_inputCRCToRefireData.end();
	while (iter != iterEnd)
	{
		SRefireData* pRefireData = iter->second;
		CRY_ASSERT(pRefireData != NULL);

		TRefireBindData& refireBindData = pRefireData->m_refireBindData;
		TRefireBindData::iterator refireBindDataIt = refireBindData.begin();
		TRefireBindData::const_iterator refireBindDataItEnd = refireBindData.end();
		while (refireBindDataIt != refireBindDataItEnd)
		{
			SRefireBindData& singleRefireBindData = *refireBindDataIt;
			SBindData& bindData = singleRefireBindData.m_bindData;
			if (bindData.m_pAction == pAction)
			{
				TRefireBindData::iterator iterDelete = refireBindDataIt++;
				refireBindData.erase(iterDelete);
			}
			else
			{
				++refireBindDataIt;
			}
		}

		if (refireBindData.empty()) // Remove whole mapping if no more items exist for the input
		{
			SAFE_DELETE(pRefireData);
			TInputCRCToRefireData::iterator iterDelete = iter++;
			m_inputCRCToRefireData.erase(iterDelete);
		}
		else
		{
			++iter;
		}
	}
}

//------------------------------------------------------------------------
void CActionMapManager::RemoveAllRefireData()
{
	if (!m_bRefiringInputs)
	{
		TInputCRCToRefireData::iterator it = m_inputCRCToRefireData.begin();
		TInputCRCToRefireData::iterator itEnd = m_inputCRCToRefireData.end();
		for (; it != itEnd; ++it)
		{
			SRefireData* pRefireData = it->second;
			CRY_ASSERT(pRefireData != NULL);
			SAFE_DELETE(pRefireData);
		}

		m_inputCRCToRefireData.clear();
	}
	else // This can happen if disabling an action filter from refired input
	{
		m_bDelayedRemoveAllRefiringData = true;
	}
}

//------------------------------------------------------------------------
void CActionMapManager::SetDefaultActionEntity(EntityId id, bool bUpdateAll)
{
	if (bUpdateAll)
	{
		IActionMapIteratorPtr pActionMapIter = CreateActionMapIterator();
		while (IActionMap* pActionMap = pActionMapIter->Next())
		{
			pActionMap->SetActionListener(id);
		}
	}

	m_defaultEntityId = id;

	BroadcastActionMapEvent(SActionMapEvent(SActionMapEvent::eActionMapManagerEvent_DefaultActionEntityChanged, (UINT_PTR)id));
}

//------------------------------------------------------------------------
void CActionMapManager::RegisterActionMapEventListener(IActionMapEventListener* pActionMapEventListener)
{
	stl::push_back_unique(m_actionMapEventListeners, pActionMapEventListener);
}

//------------------------------------------------------------------------
void CActionMapManager::UnregisterActionMapEventListener(IActionMapEventListener* pActionMapEventListener)
{
	stl::find_and_erase(m_actionMapEventListeners, pActionMapEventListener);
}

//------------------------------------------------------------------------
void CActionMapManager::BroadcastActionMapEvent(const SActionMapEvent& event)
{
	if (!m_actionMapEventListeners.empty())
	{
		CActionMapManager::TActionMapEventListeners::const_iterator iter = m_actionMapEventListeners.begin();
		CActionMapManager::TActionMapEventListeners::const_iterator cur;
		while (iter != m_actionMapEventListeners.end())
		{
			cur = iter++;
			(*cur)->OnActionMapEvent(event);
		}
	}
}

//------------------------------------------------------------------------
bool CActionMapManager::SetRefireDataDelayedPressNeedsRelease(const SInputEvent& event, CActionMap* pActionMap, CActionMapAction* pAction, SActionInput* pActionInput, const bool bDelayedPressNeedsRelease)
{
	TInputCRCToRefireData::iterator iter = m_inputCRCToRefireData.find(pActionInput->inputCRC);
	if (iter == m_inputCRCToRefireData.end())
		return false;

	SRefireData* pRefireData = iter->second;
	CRY_ASSERT(pRefireData != NULL);

	// Search if match exists
	TRefireBindData& refireBindData = pRefireData->m_refireBindData;
	TRefireBindData::iterator refireBindDataIt = refireBindData.begin();
	TRefireBindData::const_iterator refireBindDataItEnd = refireBindData.end();
	for (; refireBindDataIt != refireBindDataItEnd; ++refireBindDataIt)
	{
		SRefireBindData& singleRefireBindData = *refireBindDataIt;
		SBindData& bindData = singleRefireBindData.m_bindData;
		if (bindData.m_pActionMap == pActionMap && bindData.m_pAction == pAction && bindData.m_pActionInput == pActionInput)
		{
			singleRefireBindData.m_bDelayedPressNeedsRelease = bDelayedPressNeedsRelease;
			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------
void CActionMapManager::RemoveAllDelayedPressRefireData()
{
	CRY_ASSERT(m_bRefiringInputs == false); // This should never happen

	TInputCRCToRefireData::iterator iter = m_inputCRCToRefireData.begin();
	TInputCRCToRefireData::iterator iterEnd = m_inputCRCToRefireData.end();

	while (iter != iterEnd)
	{
		SRefireData* pRefireData = iter->second;
		CRY_ASSERT(pRefireData != NULL);
		PREFAST_ASSUME(pRefireData != NULL);
		if (pRefireData->m_inputEvent.state == eIS_Pressed)
		{
			TInputCRCToRefireData::iterator deleteIter = iter;
			++iter;
			SAFE_DELETE(pRefireData);
			m_inputCRCToRefireData.erase(deleteIter);
		}
		else
		{
			++iter;
		}
	}
}

//------------------------------------------------------------------------
int CActionMapManager::GetHighestPressDelayPriority() const
{
	int iHighestPressDelayPriority = -1;
	TInputCRCToRefireData::const_iterator iter = m_inputCRCToRefireData.begin();
	TInputCRCToRefireData::const_iterator iterEnd = m_inputCRCToRefireData.end();

	TRefireBindData::const_iterator refireBindIter;
	TRefireBindData::const_iterator refireBindIterEnd;

	for (; iter != iterEnd; ++iter)
	{
		const SRefireData* pRefireData = iter->second;
		CRY_ASSERT(pRefireData != NULL);

		refireBindIter = pRefireData->m_refireBindData.begin();
		refireBindIterEnd = pRefireData->m_refireBindData.end();
		for (; refireBindIter != refireBindIterEnd; ++refireBindIter)
		{
			const SRefireBindData& refireBindData = *refireBindIter;
			const SBindData& bindData = refireBindData.m_bindData;
			const SActionInput* pActionInput = bindData.m_pActionInput;
			CRY_ASSERT(pActionInput != NULL);
			if (pActionInput->iPressDelayPriority > iHighestPressDelayPriority)
			{
				iHighestPressDelayPriority = pActionInput->iPressDelayPriority;
			}
		}
	}

	return iHighestPressDelayPriority;
}

//------------------------------------------------------------------------
void CActionMapManager::ReloadActionMaps(IConsoleCmdArgs* pArgs)
{
	s_pThis->InitActionMaps(s_pThis->GetLoadFromXMLPath());
}

//------------------------------------------------------------------------
CActionMapManager::SBindData* CActionMapManager::GetBindData(CActionMap* pActionMap, CActionMapAction* pAction, SActionInput* pActionInput)
{
	SBindData* pFoundBindData = NULL;
	uint32 inputCRC = pActionInput->inputCRC;
	TInputCRCToBind::iterator iter = m_inputCRCToBind.lower_bound(inputCRC);
	TInputCRCToBind::const_iterator end = m_inputCRCToBind.end();

	for (; iter != end && iter->first == inputCRC; ++iter)
	{
		SBindData& bindData = iter->second;

		if (bindData.m_pActionMap == pActionMap && bindData.m_pAction == pAction && bindData.m_pActionInput->input == pActionInput->input)
		{
			pFoundBindData = &bindData;
			break;
		}
	}

	return pFoundBindData;
}

//------------------------------------------------------------------------

namespace
{
template<typename T, class Derived>
struct CGenericPtrMapIterator : public Derived
{
	typedef typename T::mapped_type MappedType;
	typedef typename T::iterator    IterType;
	CGenericPtrMapIterator(T& theMap)
		: m_nRefs(0)
	{
		m_cur = theMap.begin();
		m_end = theMap.end();
	}
	virtual MappedType Next()
	{
		if (m_cur == m_end)
			return 0;
		MappedType& dt = m_cur->second;
		++m_cur;
		return dt;
	}
	virtual void AddRef()
	{
		++m_nRefs;
	}
	virtual void Release()
	{
		if (--m_nRefs <= 0)
			delete this;
	}
	IterType m_cur;
	IterType m_end;
	int      m_nRefs;
};
};

//------------------------------------------------------------------------
IActionMapIteratorPtr CActionMapManager::CreateActionMapIterator()
{
	return new CGenericPtrMapIterator<TActionMapMap, IActionMapIterator>(m_actionMaps);
}

//------------------------------------------------------------------------
IActionFilterIteratorPtr CActionMapManager::CreateActionFilterIterator()
{
	return new CGenericPtrMapIterator<TActionFilterMap, IActionFilterIterator>(m_actionFilters);
}

//------------------------------------------------------------------------
void CActionMapManager::EnumerateActions(IActionMapPopulateCallBack* pCallBack) const
{
	for (TActionMapMap::const_iterator actionMapIt = m_actionMaps.begin(); actionMapIt != m_actionMaps.end(); ++actionMapIt)
	{
		actionMapIt->second->EnumerateActions(pCallBack);
	}
}

//------------------------------------------------------------------------
int CActionMapManager::GetActionsCount() const
{
	int actionCount = 0;
	for (TActionMapMap::const_iterator actionMapIt = m_actionMaps.begin(); actionMapIt != m_actionMaps.end(); ++actionMapIt)
	{
		actionCount += actionMapIt->second->GetActionsCount();
	}

	return actionCount;
}

//------------------------------------------------------------------------
bool CActionMapManager::AddInputDeviceMapping(const EActionInputDevice deviceType, const char* szDeviceTypeStr)
{
	TInputDeviceData::iterator iter = m_inputDeviceData.begin();
	TInputDeviceData::iterator iterEnd = m_inputDeviceData.end();
	for (; iter != iterEnd; ++iter)
	{
		SActionInputDeviceData& inputDeviceData = *iter;
		if (inputDeviceData.m_type == deviceType)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CActionMapManager::AddInputDeviceMapping: Device type already exists, failed to add device: %s", szDeviceTypeStr);
			return false;
		}
	}

	SActionInputDeviceData inputDeviceData(deviceType, szDeviceTypeStr);
	m_inputDeviceData.push_back(inputDeviceData);

	return true;
}

//------------------------------------------------------------------------
bool CActionMapManager::RemoveInputDeviceMapping(const EActionInputDevice deviceType)
{
	TInputDeviceData::iterator iter = m_inputDeviceData.begin();
	TInputDeviceData::iterator iterEnd = m_inputDeviceData.end();
	for (; iter != iterEnd; ++iter)
	{
		SActionInputDeviceData& inputDeviceData = *iter;
		if (inputDeviceData.m_type == deviceType)
		{
			m_inputDeviceData.erase(iter);
			return true;
		}
	}

	CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CActionMapManager::RemoveInputDeviceMapping: Failed to find device");
	return false;
}

//------------------------------------------------------------------------
void CActionMapManager::ClearInputDevicesMappings()
{
	m_inputDeviceData.clear();
}

//------------------------------------------------------------------------
int CActionMapManager::GetNumInputDeviceData() const
{
	return m_inputDeviceData.size();
}

//------------------------------------------------------------------------
const SActionInputDeviceData* CActionMapManager::GetInputDeviceDataByIndex(const int iIndex)
{
	CRY_ASSERT(iIndex >= 0 && iIndex < m_inputDeviceData.size());
	if (iIndex < 0 || iIndex >= m_inputDeviceData.size())
		return NULL;

	return &m_inputDeviceData[iIndex];
}

//------------------------------------------------------------------------
const SActionInputDeviceData* CActionMapManager::GetInputDeviceDataByType(const EActionInputDevice deviceType)
{
	SActionInputDeviceData* pFoundInputDeviceData = NULL;

	TInputDeviceData::iterator iter = m_inputDeviceData.begin();
	TInputDeviceData::iterator iterEnd = m_inputDeviceData.end();
	for (; iter != iterEnd; ++iter)
	{
		SActionInputDeviceData& inputDeviceData = *iter;
		if (inputDeviceData.m_type == deviceType)
		{
			pFoundInputDeviceData = &inputDeviceData;
			break;
		}
	}

	return pFoundInputDeviceData;
}

//------------------------------------------------------------------------
const SActionInputDeviceData* CActionMapManager::GetInputDeviceDataByType(const char* szDeviceType)
{
	SActionInputDeviceData* pFoundInputDeviceData = NULL;

	TInputDeviceData::iterator iter = m_inputDeviceData.begin();
	TInputDeviceData::iterator iterEnd = m_inputDeviceData.end();
	for (; iter != iterEnd; ++iter)
	{
		SActionInputDeviceData& inputDeviceData = *iter;
		if (strcmp(inputDeviceData.m_typeStr, szDeviceType) == 0)
		{
			pFoundInputDeviceData = &inputDeviceData;
			break;
		}
	}

	return pFoundInputDeviceData;
}

//------------------------------------------------------------------------
void CActionMapManager::GetMemoryStatistics(ICrySizer* pSizer)
{
	SIZER_SUBCOMPONENT_NAME(pSizer, "ActionMapManager");
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_actionMaps);
	pSizer->AddObject(m_actionFilters);
}

//------------------------------------------------------------------------
bool CActionMapManager::HandleAcceptedEvents(const SInputEvent& event, TBindPriorityList& priorityList)
{
	float fCurrTime = gEnv->pTimer->GetCurrTime();
	IGameFramework* pGameFramework = gEnv->pGameFramework;
	if (pGameFramework && pGameFramework->IsGamePaused())
	{
		fCurrTime = gEnv->pTimer->GetCurrTime(ITimer::ETIMER_UI);
	}

	TBindPriorityList::iterator itBind = priorityList.begin();
	TBindPriorityList::const_iterator itBindEnd = priorityList.end();
	for (; itBind != itBindEnd; ++itBind)
	{
		const SBindData* pBindData = *itBind;
		CRY_ASSERT(pBindData);

		CActionMap* pActionMap = pBindData->m_pActionMap;
		CRY_ASSERT(pActionMap != NULL);

		if (!pActionMap->Enabled())
			continue;

		SActionInput* pActionInput = pBindData->m_pActionInput;
		CRY_ASSERT(pActionInput != NULL);

		const CActionMapAction* pAction = pBindData->m_pAction;
		CRY_ASSERT(pAction != NULL);

		const ActionId& actionID = pAction->GetActionId();

		if (gEnv->pInput->Retriggering() && (!(pActionInput->activationMode & eAAM_Retriggerable)))
			continue;

		// Current action will fire below, check if actioninput blocks input and handle
		HandleInputBlocking(event, pActionInput, fCurrTime);

		EInputState currentState = pActionInput->currentState;

		// Process console
		if (pActionInput->activationMode & eAAM_ConsoleCmd)
		{
			CConsoleActionListener consoleActionListener;
			consoleActionListener.OnAction(actionID, currentState, event.value);

			const TActionListeners& extraActionListeners = GetExtraActionListeners();
			for (TActionListeners::const_iterator extraListener = extraActionListeners.begin(); extraListener != extraActionListeners.end(); ++extraListener)
				(*extraListener)->OnAction(actionID, currentState, event.value);

			return true;
		}

		pActionMap->NotifyFlowNodeActionListeners(actionID, currentState, event.value);

		// TODO: Remove always action listeners and integrate into 1 prioritized type
		// Process the always events first, then process normal if not handled
		bool bHandled = ProcessAlwaysListeners(actionID, currentState, event.value, event);
		if (bHandled)
			continue;

		const TActionListeners& extraActionListeners = GetExtraActionListeners();
		for (TActionListeners::const_iterator extraListener = extraActionListeners.begin(); extraListener != extraActionListeners.end(); ++extraListener)
			(*extraListener)->OnAction(actionID, currentState, event.value);

		pActionMap->NotifyExtraActionListeners(actionID, currentState, event.value);

		// Process normal events
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(pActionMap->GetActionListener());
		if (!pEntity)
			continue;

		CGameObject* pGameObject = (CGameObject*) pEntity->GetProxy(ENTITY_PROXY_USER);
		if (!pGameObject)
			continue;

		pGameObject->OnAction(actionID, currentState, event.value);
	}

	return false; // Not handled
}

//------------------------------------------------------------------------
void CActionMapManager::HandleInputBlocking(const SInputEvent& event, const SActionInput* pActionInput, const float fCurrTime)
{
	// Refired events will make it here even when input blocked, handle these here
	if (IsCurrentlyRefiringInput())
	{
		if (gEnv->pInput->ShouldBlockInputEventPosting(event.keyId, event.deviceType, event.deviceIndex))
		{
			return;
		}
	}

	const SActionInputBlockData& inputBlockData = pActionInput->inputBlockData;
	EActionInputBlockType blockType = inputBlockData.blockType;
	if (blockType == eAIBT_None)
		return;

	// Check if block condition fulfilled

	// This can be optimized once EInputState + EActionActivationMode are combined like it should be
	EInputState compareState;

	// Need to translate analog to press, down, release state for easy comparison
	if (event.state == eIS_Changed)
	{
		if (pActionInput->bAnalogConditionFulfilled)
		{
			if (fCurrTime - pActionInput->fPressedTime >= FLT_EPSILON) // Can't be pressed since time has elapsed, must be hold
			{
				compareState = eIS_Down;
			}
			else
			{
				compareState = eIS_Pressed;
			}
		}
		else // Must be release
		{
			compareState = eIS_Released;
		}
	}
	else
	{
		compareState = event.state;
	}

	if (compareState & inputBlockData.activationMode)
	{
		switch (inputBlockData.blockType)
		{
		case eAIBT_BlockInputs:
			{
				const TActionInputBlockers& inputs = inputBlockData.inputs;
				TActionInputBlockers::const_iterator iter = inputs.begin();
				TActionInputBlockers::const_iterator iterEnd = inputs.end();
				for (; iter != iterEnd; ++iter)
				{
					const SActionInputBlocker& inputToBlock = *iter;
					SInputBlockData inputDataToSend(inputToBlock.keyId, inputBlockData.fBlockDuration, inputBlockData.bAllDeviceIndices, inputBlockData.deviceIndex);
					m_pInput->SetBlockingInput(inputDataToSend);
				}
			}
			break;
		case eAIBT_Clear:
			{
				m_pInput->ClearBlockingInputs();
			}
			break;
		}
	}
}

//------------------------------------------------------------------------
bool CActionMapManager::CreateRefiredEventPriorityList(SRefireData* pRefireData,
                                                       TBindPriorityList& priorityList,
                                                       TBindPriorityList& removeList,
                                                       TBindPriorityList& delayPressNeedsReleaseList)
{
	bool bRes = false;

	uint32 inputCRC = pRefireData->m_inputCRC;
	SInputEvent& inputEvent = pRefireData->m_inputEvent;
	TRefireBindData& refireBindData = pRefireData->m_refireBindData;

	// Look up the actions which are associated with this
	TRefireBindData::iterator it = refireBindData.begin();
	TRefireBindData::const_iterator itEnd = refireBindData.end();

	float fCurrTime = gEnv->pTimer->GetCurrTime();
	IGameFramework* pGameFramework = gEnv->pGameFramework;
	if (pGameFramework && pGameFramework->IsGamePaused())
	{
		fCurrTime = gEnv->pTimer->GetCurrTime(ITimer::ETIMER_UI);
	}

	// Create priority listing for which processes should be fired
	float fLongestHeldTime = 0.0f;
	for (; it != itEnd; ++it)
	{
		SRefireBindData& singleRefireBindData = *it;
		SBindData& bindData = singleRefireBindData.m_bindData;

		if (singleRefireBindData.m_bIgnoreNextUpdate) // Ignoring is set true when an actual input event has just added or updated a refiring event
		{
			singleRefireBindData.m_bIgnoreNextUpdate = false;
			continue;
		}

		SActionInput* pActionInput = bindData.m_pActionInput;
		CRY_ASSERT(pActionInput != NULL);
		CActionMapAction* pAction = bindData.m_pAction;
		CRY_ASSERT(pAction != NULL);
		CActionMap* pActionMap = bindData.m_pActionMap;
		CRY_ASSERT(pActionMap != NULL);

		// Refire data could be either from analog repeating or delayed press
		// If its a delayed press event, only fire once when reach delay then remove
		if (inputEvent.state == eIS_Pressed)
		{
			const float fPressDelay = pActionInput->fPressTriggerDelay;
			if (fPressDelay >= FLT_EPSILON) // Must be delayed press event
			{
				if (fCurrTime - pActionInput->fPressedTime >= fPressDelay) // Meets delay so fire and mark for removal
				{
					removeList.push_back(&bindData);

					if (singleRefireBindData.m_bDelayedPressNeedsRelease) // Release must be fired after too
					{
						delayPressNeedsReleaseList.push_back(&bindData);
					}
				}
				else // Don't fire yet
				{
					continue;
				}
			}
		}

#ifdef _DEBUG

		CRY_ASSERT((pActionInput->inputCRC == inputCRC) && (strcmp(pActionInput->input.c_str(), inputEvent.keyName.c_str()) == 0)); \

		if ((pActionInput->inputCRC == inputCRC) && (strcmp(pActionInput->input.c_str(), inputEvent.keyName.c_str()) != 0))
		{
			GameWarning("Hash values are identical, but values are different! %s - %s", pActionInput->input.c_str(), inputEvent.keyName.c_str());
		}
		if ((pActionInput->inputCRC != inputCRC) && (strcmp(pActionInput->input.c_str(), inputEvent.keyName.c_str()) == 0))
		{
			GameWarning("Hash values are different, but values are identical! %s - %s", pActionInput->input.c_str(), inputEvent.keyName.c_str());
		}

#endif //_DEBUG
		bool bAdd = (!ActionFiltered(pAction->GetActionId()) && pActionMap->CanProcessInput(inputEvent, pActionMap, pAction, pActionInput));

		if (bAdd)
		{
			priorityList.push_back(&bindData);
			bRes = true;
		}
	}

	return bRes;
}

//------------------------------------------------------------------------
void CActionMapManager::UpdateRefiringInputs()
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	// no actions while console is open
	if (gEnv->pConsole->IsOpened())
		return;

	if (gEnv->IsEditor() && gEnv->pGameFramework->IsEditing())
		return;

	SetCurrentlyRefiringInput(true);

	TBindPriorityList removeList;

	std::vector<SRefireReleaseListData> releaseListRefireData;

	TInputCRCToRefireData::iterator iter = m_inputCRCToRefireData.begin();
	TInputCRCToRefireData::iterator iterEnd = m_inputCRCToRefireData.end();
	for (; iter != iterEnd; ++iter)
	{
		SRefireData* pRefireData = iter->second;
		const SInputEvent& event = pRefireData->m_inputEvent;

		TBindPriorityList priorityList;
		TBindPriorityList delayPressNeedsReleaseList;

		bool bHasItems = CreateRefiredEventPriorityList(pRefireData, priorityList, removeList, delayPressNeedsReleaseList);
		if (bHasItems)
		{
			HandleAcceptedEvents(event, priorityList);

			if (m_bDelayedRemoveAllRefiringData) // Probably an action filter just got disabled which wants to remove all refire data, do it now
			{
				m_bDelayedRemoveAllRefiringData = false;
				SetCurrentlyRefiringInput(false);
				RemoveAllRefireData();
				return;
			}
		}

		if (delayPressNeedsReleaseList.empty() == false)
		{
			SRefireReleaseListData singleReleaseListRefireData;
			singleReleaseListRefireData.m_inputEvent = event;
			// Since not going through normal path for release event (Doesn't need to be checked, already approved for fire, just delayed)
			// Need to manually change the current states to release before firing to ensure action is fired with a release
			singleReleaseListRefireData.m_inputEvent.state = eIS_Released;
			TBindPriorityList::iterator iterDelay = delayPressNeedsReleaseList.begin();
			TBindPriorityList::const_iterator iterDelayEnd = delayPressNeedsReleaseList.end();
			for (; iterDelay != iterDelayEnd; ++iterDelay)
			{
				const SBindData* pReleaseBindData = *iterDelay;
				CRY_ASSERT(pReleaseBindData != NULL);
				SActionInput* pReleaseActionInput = pReleaseBindData->m_pActionInput;
				CRY_ASSERT(pReleaseActionInput != NULL);
				pReleaseActionInput->currentState = eIS_Released;
			}

			singleReleaseListRefireData.m_inputsList = delayPressNeedsReleaseList;
			releaseListRefireData.push_back(singleReleaseListRefireData);
		}
	}

	// Now process delayed release events which must happen after the delayed press
	const size_t eventCount = releaseListRefireData.size();
	for (size_t i = 0; i < eventCount; i++)
	{
		SRefireReleaseListData& singleReleaseListRefireData = releaseListRefireData[i];
		HandleAcceptedEvents(singleReleaseListRefireData.m_inputEvent, singleReleaseListRefireData.m_inputsList);
	}
	releaseListRefireData.clear();
	SetCurrentlyRefiringInput(false);

	// Safe to remove now
	TBindPriorityList::iterator removeIt = removeList.begin();
	TBindPriorityList::const_iterator removeItEnd = removeList.end();
	for (; removeIt != removeItEnd; ++removeIt)
	{
		const SBindData* pBindData = *removeIt;
		CRY_ASSERT(pBindData);

		RemoveRefireData(pBindData->m_pActionMap, pBindData->m_pAction, pBindData->m_pActionInput);
	}
	removeList.clear();
}
