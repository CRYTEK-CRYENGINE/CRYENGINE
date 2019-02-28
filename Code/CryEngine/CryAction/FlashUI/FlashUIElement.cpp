// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlashUIElement.cpp
//  Version:     v1.00
//  Created:     10/9/2010 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "FlashUIElement.h"
#include <CryString/UnicodeFunctions.h>
#include <CryInput/IHardwareMouse.h>
#include <CrySystem/Scaleform/IFlashUI.h>
#include <CrySystem/Scaleform/IScaleformHelper.h>
#include <CrySystem/File/ICryPak.h>
#include "FlashUI.h"

#define FLASH_PLATFORM_PC      0
#define FLASH_PLATFORM_DURANGO 1
#define FLASH_PLATFORM_ORBIS   2
#define FLASH_PLATFORM_ANDROID 1

#if !defined (_RELEASE)
CFlashUIElement::TElementInstanceList CFlashUIElement::s_ElementDebugList;
#endif

//------------------------------------------------------------------------------------
struct SUIElementSerializer
{
	static bool Serialize(CFlashUIElement* pElement, XmlNodeRef& xmlNode, bool bIsLoading)
	{
		CRY_ASSERT_MESSAGE(pElement, "NULL pointer passed!");
		CRY_ASSERT_MESSAGE(xmlNode != 0, "XmlNode is invalid");

		if (!pElement || !xmlNode) return false;

		if (bIsLoading)
		{
			pElement->m_baseInfo = xmlNode;

			DeleteContainer(pElement->m_variables);
			DeleteContainer(pElement->m_arrays);
			DeleteContainer(pElement->m_displayObjects);
			DeleteContainer(pElement->m_displayObjectsTmpl);
			DeleteContainer(pElement->m_events);
			DeleteContainer(pElement->m_functions);
			pElement->m_StringBufferSet.clear();
			pElement->m_sName = xmlNode->getAttr("name");

			SUIMovieClipDesc* pRoot = new SUIMovieClipDesc("_root", "Root", "Root MC");
			pElement->m_displayObjects.push_back(pRoot);
			pElement->m_pRoot = pRoot;

			ReadElementFlag(xmlNode, pElement, "mouseevents", IUIElement::eFUI_MOUSEEVENTS);
			ReadElementFlag(xmlNode, pElement, "keyevents", IUIElement::eFUI_KEYEVENTS);
			ReadElementFlag(xmlNode, pElement, "cursor", IUIElement::eFUI_HARDWARECURSOR);
			ReadElementFlag(xmlNode, pElement, "console_mouse", IUIElement::eFUI_CONSOLE_MOUSE);
			ReadElementFlag(xmlNode, pElement, "console_cursor", IUIElement::eFUI_CONSOLE_CURSOR);
			ReadElementFlag(xmlNode, pElement, "controller_input", IUIElement::eFUI_CONTROLLER_INPUT);
			ReadElementFlag(xmlNode, pElement, "events_exclusive", IUIElement::eFUI_EVENTS_EXCLUSIVE);
			ReadElementFlag(xmlNode, pElement, "render_lockless", IUIElement::eFUI_RENDER_LOCKLESS);
			ReadElementFlag(xmlNode, pElement, "force_no_unload", IUIElement::eFUI_FORCE_NO_UNLOAD);
			ReadElementFlag(xmlNode, pElement, "fixed_proj_depth", IUIElement::eFUI_FIXED_PROJ_DEPTH);
			ReadElementFlag(xmlNode, pElement, "lazy_update", IUIElement::eFUI_LAZY_UPDATE);
			ReadElementFlag(xmlNode, pElement, "is_Hud", IUIElement::eFUI_IS_HUD);
			ReadElementFlag(xmlNode, pElement, "shared_rt", IUIElement::eFUI_SHARED_RT);

			ReadGFxNode(xmlNode->findChild("GFx"), pElement);
			ReadParamNodes<SUIParameterDesc>(xmlNode->findChild("variables"), pElement->m_variables, "varname", true, pElement->m_pRoot);
			ReadParamNodes<SUIParameterDesc>(xmlNode->findChild("arrays"), pElement->m_arrays, "varname", true, pElement->m_pRoot);
			ReadParamNodes<SUIMovieClipDesc>(xmlNode->findChild("movieclips"), pElement->m_displayObjects, "instancename", true, pElement->m_pRoot);
			ReadParamNodes<SUIMovieClipDesc>(xmlNode->findChild("templates"), pElement->m_displayObjectsTmpl, "template", false);
			ReadEventNodes(xmlNode->findChild("events"), pElement->m_events, "fscommand");
			ReadEventNodes(xmlNode->findChild("functions"), pElement->m_functions, "funcname", pElement->m_pRoot);

			pElement->m_firstDynamicDisplObjIndex = pElement->m_displayObjects.size();
		}
		else
		{
			// save - not needed atm
			// maybe we need it to serialize dynamic created stuff?
			assert(false);
		}
		return true;
	}

	static void ReadElementFlag(const XmlNodeRef& node, CFlashUIElement* pElement, const char* sFlag, IUIElement::EFlashUIFlags eFlag)
	{
		bool xmlflag = false;
		node->getAttr(sFlag, xmlflag);
		pElement->SetFlagInt(eFlag, xmlflag);
	}

	static void ReadGFxNode(const XmlNodeRef& node, CFlashUIElement* pElement)
	{
		if (node)
		{
			node->getAttr("alpha", pElement->m_fAlpha);
			node->getAttr("layer", pElement->m_iLayer);
			pElement->m_sFlashFile = node->getAttr("file");

			const XmlNodeRef& constraintsnode = node->findChild("Constraints");
			if (constraintsnode)
			{
				const XmlNodeRef& alignnode = constraintsnode->findChild("Align");
				if (alignnode)
				{
					pElement->m_constraints.eType = GetPositionType(alignnode->getAttr("mode"));
					pElement->m_constraints.eHAlign = GetAlignType(alignnode->getAttr("halign"));
					pElement->m_constraints.eVAlign = GetAlignType(alignnode->getAttr("valign"));
					pElement->m_constraints.bScale = false;
					alignnode->getAttr("scale", pElement->m_constraints.bScale);
					pElement->m_constraints.bMax = false;
					alignnode->getAttr("max", pElement->m_constraints.bMax);
				}

				const XmlNodeRef& posnode = constraintsnode->findChild("Position");
				if (posnode)
				{
					posnode->getAttr("top", pElement->m_constraints.iTop);
					posnode->getAttr("left", pElement->m_constraints.iLeft);
					posnode->getAttr("width", pElement->m_constraints.iWidth);
					posnode->getAttr("height", pElement->m_constraints.iHeight);
				}
			}
		}
	}

	static IUIElement::SUIConstraints::EPositionType GetPositionType(const char* mode)
	{
		if (mode != NULL)
		{
			if (!strcmpi("fixed", mode))
				return IUIElement::SUIConstraints::ePT_Fixed;
			else if (!strcmpi("dynamic", mode))
				return IUIElement::SUIConstraints::ePT_Dynamic;
			else if (!strcmpi("fullscreen", mode))
				return IUIElement::SUIConstraints::ePT_Fullscreen;
			else if (!strcmpi("fixdyntexsize", mode))
				return IUIElement::SUIConstraints::ePT_FixedDynTexSize;
		}
		return IUIElement::SUIConstraints::ePT_Dynamic;
	}

	static IUIElement::SUIConstraints::EPositionAlign GetAlignType(const char* type)
	{
		if (type != NULL)
		{
			if (!strcmpi("left", type) || !strcmpi("top", type))
				return IUIElement::SUIConstraints::ePA_Lower;
			else if (!strcmpi("mid", type))
				return IUIElement::SUIConstraints::ePA_Mid;
			else if (!strcmpi("right", type) || !strcmpi("bottom", type))
				return IUIElement::SUIConstraints::ePA_Upper;
		}
		return IUIElement::SUIConstraints::ePA_Mid;
	}

	static SUIParameterDesc::EUIParameterType GetParamType(const char* type)
	{
		if (type != NULL)
		{
			if (!strcmpi("bool", type))
				return SUIParameterDesc::eUIPT_Bool;
			else if (!strcmpi("int", type))
				return SUIParameterDesc::eUIPT_Int;
			else if (!strcmpi("float", type))
				return SUIParameterDesc::eUIPT_Float;
			else if (!strcmpi("string", type))
				return SUIParameterDesc::eUIPT_String;
		}
		return SUIParameterDesc::eUIPT_Any;
	}

	template<class T>
	static void DeleteContainer(T& container)
	{
		for (typename T::iterator it = container.begin(); it != container.end(); ++it)
			delete *it;
		container.clear();
	}

	static void DeleteContainer(TUIMovieClipLookup& container)
	{
		for (TUIMovieClipLookup::iterator it = container.begin(); it != container.end(); ++it)
		{
			DeleteContainer((*it)->m_variables);
			DeleteContainer((*it)->m_arrays);
			DeleteContainer((*it)->m_displayObjects);
			DeleteContainer((*it)->m_functions);
			delete *it;
		}
		container.clear();
	}

	template<class D>
	static void ReadSubnodes(const XmlNodeRef& node, D* item, bool bSetParent)
	{
	}

	template<class D, class T>
	static D* AddToList(T& list, const char* name, const char* displname, const char* desc, SUIParameterDesc::EUIParameterType eType, const SUIParameterDesc* pParent)
	{
		D* newItem = new D(name, displname, desc, eType, pParent);
		list.push_back(newItem);
		return newItem;
	}

	template<class D, class T>
	static void ReadParamNodes(const XmlNodeRef& node, T& list, const char* sParamName, bool bSetParent = true, const SUIParameterDesc* pParent = NULL, const char* nodename = NULL)
	{
		if (node)
		{
			for (int i = 0; i < node->getChildCount(); ++i)
			{
				const XmlNodeRef& paramNode = node->getChild(i);

				if (nodename && strcmpi(paramNode->getTag(), nodename) != 0) continue;

				const char* displName = paramNode->getAttr("name");
				const char* varName = paramNode->getAttr(sParamName);
				varName = strcmp(varName, "") ? varName : displName;
				const char* root = strstr(varName, "_root.");
				if (root) varName = root + 6;
				const char* desc = paramNode->getAttr("desc");
				const char* type = paramNode->getAttr("type");
				SUIParameterDesc::EUIParameterType eType = GetParamType(type);

				if (IsUnique(list, displName, varName))
				{
					ReadSubnodes(paramNode, AddToList<D>(list, varName, displName, desc, eType, pParent), bSetParent);
				}
				else
					gEnv->pLog->LogError("%s (%s) is not unique!", varName, displName);
			}
		}
	}

	template<class T>
	static void ReadEventNodes(const XmlNodeRef& node, T& list, const char* sEventName, const SUIParameterDesc* pParent = NULL, const char* nodename = NULL)
	{
		if (node)
		{
			for (int i = 0; i < node->getChildCount(); ++i)
			{
				const XmlNodeRef& eventNode = node->getChild(i);
				if (nodename && strcmpi(eventNode->getTag(), nodename) != 0) continue;

				const char* displName = eventNode->getAttr("name");
				const char* eventName = eventNode->getAttr(sEventName);
				eventName = strcmp(eventName, "") ? eventName : displName;
				const char* root = strstr(eventName, "_root.");
				if (root) eventName = root + 6;
				const char* desc = eventNode->getAttr("desc");
				bool IsDyn = false;
				eventNode->getAttr("dynamic", IsDyn);
				const char* dynName = eventNode->getAttr("dynName");
				const char* dynDesc = eventNode->getAttr("dynDesc");
				dynName = strcmp(dynName, "") ? dynName : "Array";

				if (IsUnique(list, displName, eventName))
				{
					SUIEventDesc* pNewEvent = new SUIEventDesc(eventName, displName, desc, SUIEventDesc::SEvtParamsDesc(IsDyn, dynName, dynDesc), SUIEventDesc::SEvtParamsDesc(), pParent);
					list.push_back(pNewEvent);
					ReadParamNodes<SUIParameterDesc>(eventNode, pNewEvent->InputParams.Params, "name", true, pParent);
				}
				else
					gEnv->pLog->LogError("%s (%s) is not unique!", displName, eventName);
			}
		}
	}

	template<class T>
	static bool IsUnique(const T& list, const char* displayName, const char* flashName)
	{
		for (typename T::const_iterator it = list.begin(); it != list.end(); ++it)
		{
			if (strcmpi((*it)->sDisplayName, displayName) == 0 || strcmpi((*it)->sName, flashName) == 0)
				return false;
		}
		return true;
	}

	template<class T>
	static size_t GetMemUsage(const T& container)
	{
		return container.get_alloc_size();
	}

	static size_t GetMemUsage(TUIMovieClipLookup& container)
	{
		size_t res = container.get_alloc_size();
		for (TUIMovieClipLookup::iterator it = container.begin(); it != container.end(); ++it)
		{
			res += GetMemUsage((*it)->m_variables);
			res += GetMemUsage((*it)->m_arrays);
			res += GetMemUsage((*it)->m_displayObjects);
			res += GetMemUsage((*it)->m_functions);
		}
		return res;
	}
};

template<>
SUIParameterDesc* SUIElementSerializer::AddToList<SUIParameterDesc, TUIParams>(TUIParams& list, const char* name, const char* displname, const char* desc, SUIParameterDesc::EUIParameterType eType, const SUIParameterDesc* pParent)
{
	list.push_back(SUIParameterDesc(name, displname, desc, eType, pParent));
	return &(*list.rbegin());
}

template<>
bool SUIElementSerializer::IsUnique<TUIParams>(const TUIParams& list, const char* displayName, const char* flashName)
{
	for (TUIParams::const_iterator it = list.begin(); it != list.end(); ++it)
	{
		if (strcmpi(it->sDisplayName, displayName) == 0 || strcmpi(it->sName, flashName) == 0)
			return false;
	}
	return true;
}

template<>
void SUIElementSerializer::ReadSubnodes<SUIMovieClipDesc>(const XmlNodeRef& node, SUIMovieClipDesc* item, bool bSetParent)
{
	ReadParamNodes<SUIParameterDesc>(node, item->m_variables, "varname", true, bSetParent ? item : NULL, "Variable");
	ReadParamNodes<SUIParameterDesc>(node, item->m_arrays, "varname", true, bSetParent ? item : NULL, "Array");
	ReadParamNodes<SUIMovieClipDesc>(node, item->m_displayObjects, "instancename", true, bSetParent ? item : NULL, "MovieClip");
	ReadEventNodes(node, item->m_functions, "funcname", bSetParent ? true, item : NULL, "Function");
}

//------------------------------------------------------------------------------------
CFlashUIElement::CFlashUIElement(CFlashUI* pFlashUI, CFlashUIElement* pBaseInstance, uint instanceId)
	: m_refCount(1)
	, m_pFlashUI(pFlashUI)
	, m_bVisible(false)
	, m_bCursorVisible(false)
	, m_iFlags(0)
	, m_fAlpha(0)
	, m_iLayer(0)
	, m_bIsValid(false)
	, m_pBaseInstance(pBaseInstance)
	, m_iInstanceID(instanceId)
	, m_pBootStrapper(NULL)
	, m_eventListener(4)
	, m_bNeedLazyUpdate(false)
	, m_bNeedLazyRender(false)
	, m_firstDynamicDisplObjIndex(0)
	, m_bIsHideRequest(false)
	, m_bUnloadRequest(false)
	, m_bUnloadAll(false)
{
	if (pBaseInstance == NULL)
		m_instances.push_back(this);

#if !defined (_RELEASE)
	s_ElementDebugList.push_back(this);
#endif
}

//------------------------------------------------------------------------------------
CFlashUIElement::~CFlashUIElement()
{
	if (m_pBaseInstance == NULL)
		DestroyBootStrapper();
	else
		Unload();

	TUIElements instances = m_instances;
	m_instances.clear();
	for (TUIElements::iterator it = instances.begin(); it != instances.end(); ++it)
	{
		if (*it != this)
		{
			SAFE_RELEASE(*it);
		}
	}

	CRY_ASSERT_MESSAGE(m_variableObjects.empty(), "Variable objects not cleared!");
	CRY_ASSERT_MESSAGE(!m_pFlashplayer, "Flash player not correct unloaded!");
	CRY_ASSERT_MESSAGE(m_pBootStrapper == NULL, "Flash bootstrapper not correct unloaded!");

#if !defined (_RELEASE)
	bool ok = stl::find_and_erase(s_ElementDebugList, this);
	CRY_ASSERT_MESSAGE(ok, "UIElement was not registered to debug list!!");
#endif

	SUIElementSerializer::DeleteContainer(m_variables);
	SUIElementSerializer::DeleteContainer(m_arrays);
	SUIElementSerializer::DeleteContainer(m_displayObjects);
	SUIElementSerializer::DeleteContainer(m_displayObjectsTmpl);
	SUIElementSerializer::DeleteContainer(m_events);
	SUIElementSerializer::DeleteContainer(m_functions);
}

void CFlashUIElement::AddRef()
{
	CryInterlockedIncrement(&m_refCount);
}

void CFlashUIElement::Release()
{
	long refCnt = CryInterlockedDecrement(&m_refCount);
	if (refCnt <= 0)
		delete this;
}

//------------------------------------------------------------------------------------
IUIElement* CFlashUIElement::GetInstance(uint instanceID)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (m_pBaseInstance)
		return m_pBaseInstance->GetInstance(instanceID);

	for (TUIElements::iterator it = m_instances.begin(); it != m_instances.end(); ++it)
	{
		if ((*it)->GetInstanceID() == instanceID)
			return *it;
	}

	CFlashUIElement* pNewElement = new CFlashUIElement(m_pFlashUI, this, instanceID);

	if (!SUIElementSerializer::Serialize(pNewElement, m_baseInfo, true))
	{
		SAFE_RELEASE(pNewElement);
		return NULL;
	}

	m_instances.push_back(pNewElement);

	TUIEventListenerUnique listeners;
	GetAllListeners(listeners);
	for (TUIEventListenerUnique::iterator listener = listeners.begin(); listener != listeners.end(); ++listener)
		(*listener)->OnInstanceCreated(this, pNewElement);

	UIACTION_LOG("%s (%i): Created UIElement instance", GetName(), instanceID);

	return pNewElement;
}

//------------------------------------------------------------------------------------
bool CFlashUIElement::DestroyInstance(uint instanceID)
{
	if (m_pBaseInstance)
		return m_pBaseInstance->DestroyInstance(instanceID);

	if (instanceID > 0)
	{
		TUIEventListenerUnique listeners;
		TUIElements::iterator instanceToDelete = GetAllListeners(listeners, instanceID);

		if (instanceToDelete != m_instances.end())
		{
			for (TUIEventListenerUnique::iterator listener = listeners.begin(); listener != listeners.end(); ++listener)
				(*listener)->OnInstanceDestroyed(this, *instanceToDelete);

			SAFE_RELEASE(*instanceToDelete);
			m_instances.erase(instanceToDelete);

			UIACTION_LOG("%s (%i): UIElement instance destroyed", GetName(), instanceID);
			return true;
		}

		UIACTION_WARNING("%s (%i): UIElement can't destroy instance, instance not found", GetName(), instanceID);
		return false;
	}

	UIACTION_WARNING("%s (%i): UIElement can't destroy base instance", GetName(), instanceID);
	return false;
}

//------------------------------------------------------------------------------------
CFlashUIElement::TUIElements::iterator CFlashUIElement::GetAllListeners(TUIEventListenerUnique& listeners, uint instanceID)
{
	TUIElements::iterator result = m_instances.end();
	for (TUIElements::iterator instance = m_instances.begin(); instance != m_instances.end(); ++instance)
	{
		CFlashUIElement* pInstance = (CFlashUIElement*)*instance;
		if (pInstance->m_iInstanceID == instanceID)
			result = instance;
		for (TUIEventListener::Notifier notifier(pInstance->m_eventListener); notifier.IsValid(); notifier.Next())
			listeners.insert(*notifier);
	}
	return result;
}

//------------------------------------------------------------------------------------
IUIElementIteratorPtr CFlashUIElement::GetInstances() const
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (m_pBaseInstance)
		return m_pBaseInstance->GetInstances();

	CUIElementIterator* pIter = new CUIElementIterator(this);
	IUIElementIteratorPtr iter = pIter;
	iter->Release();
	return iter;
}

//------------------------------------------------------------------------------------
bool CFlashUIElement::LazyInit()
{
	if (!m_pFlashplayer)
		Init();

	return !!m_pFlashplayer;
}

//------------------------------------------------------------------------------------
void CFlashUIElement::ShowCursor()
{
	if (!m_bCursorVisible)
	{
		m_bCursorVisible = true;
		gEnv->pHardwareMouse->IncrementCounter();
	}
}

//------------------------------------------------------------------------------------
void CFlashUIElement::HideCursor()
{
	if (m_bCursorVisible)
	{
		m_bCursorVisible = false;
		gEnv->pHardwareMouse->DecrementCounter();
	}
}

//------------------------------------------------------------------------------------
const char* CFlashUIElement::GetStringBuffer(const char* str)
{
	if (m_pBaseInstance)
		return m_pBaseInstance->GetStringBuffer(str);
	return m_StringBufferSet.insert(string(str)).first->c_str();
}

//------------------------------------------------------------------------------------
void CFlashUIElement::SetFlashFile(const char* sFlashFile)
{
	if (m_sFlashFile != sFlashFile)
	{
		m_sFlashFile = sFlashFile;
		UIACTION_LOG("%s (%i): UIElement set new flash file", GetName(), m_iInstanceID);
		ReloadBootStrapper();
	}
}

//------------------------------------------------------------------------------------
bool CFlashUIElement::Init(bool bLoadAsset)
{
	if (!IsValid())
	{
		UIACTION_WARNING("%s: Element is marked as not valid! Instance was not created!", GetName());
		return false;
	}

	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	IFlashPlayerBootStrapper* pBootStrapper = InitBootStrapper();

	if (!bLoadAsset || !pBootStrapper)
		return pBootStrapper != NULL;

	// UIElements that needs to be advanced & rendered manually must
	// have CE_NoAutoUpdate in the metadata section of their flash assets. (e.g. 3DHUD)
	bool bNoAutoUpdate = pBootStrapper->HasMetadata("CE_NoAutoUpdate");
	SetFlagInt(IUIElement::eFUI_NO_AUTO_UPDATE, bNoAutoUpdate);

	if (!CFlashUI::CV_gfx_uiaction_enable)
		return false;

	if (!m_pFlashplayer)
	{
		m_pFlashplayer = pBootStrapper->CreatePlayerInstance();
		m_pFlashplayer->SetViewport(-1, -1, 1, 1);
		UpdateFlags();
	}
	else
	{
		return true;
	}

	if (!m_pFlashplayer)
	{
		UIACTION_ERROR("%s (%i): Failed to created flash instance: \"%s\"", GetName(), m_iInstanceID, m_sFlashFile.c_str());
		return false;
	}

	UIACTION_LOG("%s (%i): Created flash instance: \"%s\"", GetName(), m_iInstanceID, m_sFlashFile.c_str());

	m_pFlashplayer->SetBackgroundAlpha(m_fAlpha);
	m_pFlashplayer->SetFSCommandHandler(this);
	IFlashVariableObject* pRoot = NULL;
	m_pFlashplayer->GetVariable(m_pRoot->sName, pRoot);
	assert(pRoot);
	m_variableObjects[CCryName(m_pRoot->sName)].pObj = pRoot;
	m_variableObjects[CCryName(m_pRoot->sName)].pParent = NULL;
	m_variableObjectsLookup[(const SUIParameterDesc*)NULL][m_pRoot] = &m_variableObjects[CCryName(m_pRoot->sName)];
	if (!HasExtTexture())
	{
		UpdateViewPort();
	}

	UIACTION_LOG("%s (%i): UIElement invoke \"cry_onSetup\"", GetName(), m_iInstanceID);
	if (m_pFlashplayer->IsAvailable("cry_onSetup"))
	{
		if (gEnv->IsEditor())
		{
			switch (m_pFlashUI->GetCurrentPlatform())
			{
			case IFlashUI::ePUI_Durango:
				m_pFlashplayer->Invoke1("cry_onSetup", FLASH_PLATFORM_DURANGO);
				break;
			case IFlashUI::ePUI_Orbis:
				m_pFlashplayer->Invoke1("cry_onSetup", FLASH_PLATFORM_ORBIS);
				break;
			case IFlashUI::ePUI_Android:
				m_pFlashplayer->Invoke1("cry_onSetup", FLASH_PLATFORM_ANDROID);
				break;
			default:
				m_pFlashplayer->Invoke1("cry_onSetup", FLASH_PLATFORM_PC);
			}
		}
		else
		{
#if CRY_PLATFORM_DURANGO
			m_pFlashplayer->Invoke1("cry_onSetup", FLASH_PLATFORM_DURANGO);
#elif CRY_PLATFORM_ORBIS
			m_pFlashplayer->Invoke1("cry_onSetup", FLASH_PLATFORM_ORBIS);
#elif CRY_PLATFORM_ANDROID
			m_pFlashplayer->Invoke1("cry_onSetup", FLASH_PLATFORM_ANDROID);
#else
			m_pFlashplayer->Invoke1("cry_onSetup", FLASH_PLATFORM_PC);
#endif
		}
	}

	for (TUIEventListener::Notifier notifier(m_eventListener); notifier.IsValid(); notifier.Next())
		notifier->OnInit(this, m_pFlashplayer.get());

	if (HasExtTexture())
	{
		m_pFlashplayer->SetViewScaleMode(IFlashPlayer::eSM_ExactFit);
		m_pFlashplayer->SetViewport(0, 0, m_pFlashplayer->GetWidth(), m_pFlashplayer->GetHeight(), 1.f);
		m_pFlashplayer->SetScissorRect(0, 0, m_pFlashplayer->GetWidth(), m_pFlashplayer->GetHeight());
		SetVisible(true);
	}

	return true;
}

void CFlashUIElement::Unload(bool bAllInstances)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (!CFlashUI::CV_gfx_uiaction_enable)
		return;

	if (bAllInstances)
	{
		if (m_pBaseInstance)
		{
			m_pBaseInstance->Unload(true);
		}
		else
		{
			TUIElements instances = m_instances;
			for (TUIElements::iterator iter = instances.begin(); iter != instances.end(); ++iter)
			{
				uint instanceId = (*iter)->GetInstanceID();
				if (instanceId == 0)
					(*iter)->Unload();
				else
					DestroyInstance(instanceId);
			}
		}
		return;
	}

	if (m_pFlashplayer)
	{
		SetVisible(false);
		FreeVarObjects();
		// reset all pParent pointers on dynamically created MCs
		for (int i = m_firstDynamicDisplObjIndex; i < m_displayObjects.size(); ++i)
			m_displayObjects[i]->pParent = NULL;

		UIACTION_LOG("%s (%i): Unload flash instance", GetName(), m_iInstanceID);

		for (TUIEventListener::Notifier notifier(m_eventListener); notifier.IsValid(); notifier.Next())
			notifier->OnUnload(this);

		m_pFlashplayer = nullptr;
	}
}

void CFlashUIElement::RequestUnload(bool bAllInstances)
{
	m_bUnloadRequest = true;
	m_bUnloadAll = bAllInstances;
}

void CFlashUIElement::Reload(bool bAllInstances)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (!CFlashUI::CV_gfx_uiaction_enable)
		return;

	if (bAllInstances)
	{
		if (m_pBaseInstance)
		{
			m_pBaseInstance->Reload(true);
		}
		else
		{
			for (TUIElements::iterator iter = m_instances.begin(); iter != m_instances.end(); ++iter)
				(*iter)->Reload(false);
		}
		return;
	}

	const bool bVisible = IsVisible();
	Unload();
	Init();
	SetVisible(bVisible);
}

void CFlashUIElement::UnloadBootStrapper()
{
	DestroyBootStrapper();
}

void CFlashUIElement::ReloadBootStrapper()
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (!CFlashUI::CV_gfx_uiaction_enable)
		return;

	if (m_pBaseInstance)
	{
		m_pBaseInstance->ReloadBootStrapper();
		return;
	}

	std::map<IUIElement*, std::pair<bool, bool>> visMap;
	for (TUIElements::iterator iter = m_instances.begin(); iter != m_instances.end(); ++iter)
	{
		visMap[*iter].first = (*iter)->IsInit();
		visMap[*iter].second = (*iter)->IsVisible();
		(*iter)->UnloadBootStrapper();
	}

	for (TUIElements::iterator iter = m_instances.begin(); iter != m_instances.end(); ++iter)
	{
		(*iter)->Init(visMap[*iter].first);
		(*iter)->SetVisible(visMap[*iter].second);
	}
}

IFlashPlayerBootStrapper* CFlashUIElement::InitBootStrapper()
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (!CFlashUI::CV_gfx_uiaction_enable)
		return NULL;

	if (!m_pBaseInstance)
	{
		if (!m_pBootStrapper)
		{
			m_pBootStrapper = gEnv->pScaleformHelper ? gEnv->pScaleformHelper->CreateFlashPlayerBootStrapper() : nullptr;
			if (m_pBootStrapper)
			{
				string filename;
				filename.Format("%s%s", CFlashUI::CV_gfx_uiaction_folder->GetString(), m_sFlashFile.c_str());
				if (!gEnv->pCryPak->IsFileExist(filename.c_str()))
					filename = m_sFlashFile;

				if (!m_pBootStrapper->Load(filename.c_str()))
				{
					UIACTION_ERROR("%s: Can't find flash file: \"%s\"", GetName(), m_sFlashFile.c_str());
					SAFE_RELEASE(m_pBootStrapper);
				}
				else
				{
					UIACTION_LOG("%s: Created BootStrapper for flash file: \"%s\"", GetName(), filename.c_str());
				}
			}
		}
		return m_pBootStrapper;
	}
	return m_pBaseInstance->InitBootStrapper();
}

void CFlashUIElement::DestroyBootStrapper()
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (!CFlashUI::CV_gfx_uiaction_enable)
		return;

	if (m_pBaseInstance)
	{
		m_pBaseInstance->DestroyBootStrapper();
		return;
	}

	// deactivate dyn textures as they have to release all IFlashPlayer instances to be able to delete the bootstrapper
	TDynTextures::TVec texVex = m_textures.GetCopy();
	for (TDynTextures::TVec::iterator it = texVex.begin(); it != texVex.end(); ++it)
		(*it)->Activate(false);

	Unload(true);

	// reactivate dyn textures!
	for (TDynTextures::TVec::iterator it = texVex.begin(); it != texVex.end(); ++it)
		(*it)->Activate(true);

	CRY_ASSERT_MESSAGE(!HasExtTexture(), "Can't destory Bootstrapper while still used by dynamic textures");
	SAFE_RELEASE(m_pBootStrapper);
	UIACTION_LOG("%s: BootStrapper destroyed for flash file: \"%s\"", GetName(), m_sFlashFile.c_str());
}
//------------------------------------------------------------------------------------
bool CFlashUIElement::Serialize(XmlNodeRef& xmlNode, bool bIsLoading)
{
	CRY_ASSERT_MESSAGE(bIsLoading, "only loading supported!");

	bool res = true;
	for (TUIElements::iterator iter = m_instances.begin(); iter != m_instances.end(); ++iter)
		res &= SUIElementSerializer::Serialize((CFlashUIElement*)*iter, xmlNode, bIsLoading);
	SetValid(res);
	return res;
}

//------------------------------------------------------------------------------------
void CFlashUIElement::Update(float fDeltaTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);
	if (!m_pFlashplayer || ((m_iFlags & (uint64) eFUI_LAZY_UPDATE) != 0 && !m_bNeedLazyUpdate)) return;

	m_pFlashplayer->Advance(fDeltaTime);
	m_bNeedLazyUpdate = false;

	if (m_bUnloadRequest)
	{
		Unload(m_bUnloadAll);
		m_bUnloadRequest = false;
	}
	else
	{
		m_bNeedLazyRender = true;
	}
}

//------------------------------------------------------------------------------------
void CFlashUIElement::Render()
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);
	if (!m_pFlashplayer) return;

	if (!HasExtTexture())
	{
		m_pFlashplayer->Render();
	}
}

//------------------------------------------------------------------------------------
void CFlashUIElement::RenderLockless()
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);
	if (!m_pFlashplayer) return;

	if (!HasExtTexture())
	{
		m_pFlashplayer->Render();
	}
}

//------------------------------------------------------------------------------------
void CFlashUIElement::RequestHide()
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (!CFlashUI::CV_gfx_uiaction_enable)
		return;

	UIACTION_LOG("%s (%i): Hide requested", GetName(), m_iInstanceID);

	if (!LazyInit())
		return;

	if (m_pFlashplayer && m_pFlashplayer->IsAvailable("cry_requestHide"))
	{
		UIACTION_LOG("%s (%i): UIElement invoke \"cry_requestHide\"", GetName(), m_iInstanceID);
		m_pFlashplayer->Invoke0("cry_requestHide");
	}

	m_bIsHideRequest = true;
	UpdateFlags();
}

//------------------------------------------------------------------------------------
void CFlashUIElement::SetVisible(bool bVisible)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (!CFlashUI::CV_gfx_uiaction_enable)
		return;

	if (m_bVisible == bVisible && !m_bIsHideRequest)
		return;

	if (bVisible && !LazyInit())
		return;

	m_bVisible = bVisible;
	m_bIsHideRequest = false;

	UIACTION_LOG("%s (%i): Set visible: %s", GetName(), m_iInstanceID, bVisible ? "true" : "false");

	if (m_pFlashplayer)
	{
		if (m_bVisible && m_pFlashplayer->IsAvailable("cry_onShow"))
		{
			UIACTION_LOG("%s (%i): UIElement invoke \"cry_onShow\"", GetName(), m_iInstanceID);
			m_pFlashplayer->Invoke0("cry_onShow");
		}
		else if (m_pFlashplayer->IsAvailable("cry_onHide"))
		{
			UIACTION_LOG("%s (%i): UIElement invoke \"cry_onHide\"", GetName(), m_iInstanceID);
			m_pFlashplayer->Invoke0("cry_onHide");
		}
		m_pFlashplayer->Advance(0);
	}

	for (TUIEventListener::Notifier notifier(m_eventListener); notifier.IsValid(); notifier.Next())
		notifier->OnSetVisible(this, m_bVisible);

	m_pFlashUI->InvalidateSortedElements();

	UpdateFlags();
	if (m_bVisible)
		UpdateViewPort();
}

//------------------------------------------------------------------------------------
std::shared_ptr<IFlashPlayer> CFlashUIElement::GetFlashPlayer()
{
	LazyInit();
	return m_pFlashplayer;
}

//------------------------------------------------------------------------------------
void CFlashUIElement::SetLayer(int iLayer)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (!CFlashUI::CV_gfx_uiaction_enable)
		return;

	UIACTION_LOG("%s (%i): UIElement set layer %i", GetName(), m_iInstanceID, iLayer);

	if (m_iLayer != iLayer)
		m_pFlashUI->InvalidateSortedElements();
	m_iLayer = iLayer;
}

//------------------------------------------------------------------------------------
void CFlashUIElement::SetConstraints(const SUIConstraints& newConstraints)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (!CFlashUI::CV_gfx_uiaction_enable)
		return;

	UIACTION_LOG("%s (%i): UIElement set new constraints", GetName(), m_iInstanceID);
	m_constraints = newConstraints;
	UpdateViewPort();
}

//------------------------------------------------------------------------------------
void CFlashUIElement::SetAlpha(float fAlpha)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (!CFlashUI::CV_gfx_uiaction_enable)
		return;

	if (!LazyInit())
		return;

	UIACTION_LOG("%s (%i): Set background alpha: %f", GetName(), m_iInstanceID, m_fAlpha);

	m_fAlpha = clamp_tpl(fAlpha, 0.0f, 1.f);
	if (m_pFlashplayer)
		m_pFlashplayer->SetBackgroundAlpha(m_fAlpha);
	ForceLazyUpdateInl();
}

//------------------------------------------------------------------------------------
void CFlashUIElement::UpdateFlags()
{
#if CRY_PLATFORM_DESKTOP
	if (HasFlag(eFUI_HARDWARECURSOR) && m_bVisible && !m_bIsHideRequest)
#else
	if (HasFlag(eFUI_HARDWARECURSOR) && HasFlag(eFUI_CONSOLE_CURSOR) && m_bVisible && !m_bIsHideRequest)
#endif
		ShowCursor();
	else
		HideCursor();

	if (m_pFlashplayer)
		m_pFlashplayer->StereoEnforceFixedProjectionDepth(HasFlag(eFUI_FIXED_PROJ_DEPTH));
	ForceLazyUpdateInl();
}

//------------------------------------------------------------------------------------
void CFlashUIElement::SetFlag(EFlashUIFlags flag, bool bSet)
{
	CRY_ASSERT_MESSAGE((flag & eFUI_NOT_CHANGEABLE) == 0, "Not allowed to set non changeable flag during runtime!");
	if ((flag & eFUI_NOT_CHANGEABLE) == 0)
		SetFlagInt(flag, bSet);
}

void CFlashUIElement::SetFlagInt(EFlashUIFlags flag, bool bSet)
{
	if (HasFlag(flag) == bSet) return;

	if ((flag & eFUI_MASK_PER_ELEMENT) && m_pBaseInstance)
	{
		m_pBaseInstance->SetFlag(flag, bSet);
	}
	else
	{
		UIACTION_LOG("%s (%i): UIElement set flag %i to %s", GetName(), m_iInstanceID, flag, bSet ? "1" : "0");
		if (bSet)
			m_iFlags |= (uint64) flag;
		else
			m_iFlags &= ~(uint64) flag;
	}

	UpdateFlags();
}

//------------------------------------------------------------------------------------
bool CFlashUIElement::HasFlag(EFlashUIFlags flag) const
{
	if ((flag & eFUI_MASK_PER_ELEMENT) && m_pBaseInstance)
		return m_pBaseInstance->HasFlag(flag);

	return (m_iFlags & (uint64) flag) != 0;
}

//------------------------------------------------------------------------------------
bool CFlashUIElement::DefaultInfoCheck(SFlashObjectInfo*& pInfo, const SUIParameterDesc* pDesc, const SUIMovieClipDesc* pTmplDesc)
{
	if (!CFlashUI::CV_gfx_uiaction_enable)
		return false;

	if (!LazyInit())
		return false;

	CRY_ASSERT_MESSAGE(pDesc, "NULL pointer passed!");
	if (!pDesc)
		return false;

	const SUIMovieClipDesc* pParentDesc = pTmplDesc ? pTmplDesc : m_pRoot;
	CRY_ASSERT_MESSAGE(pParentDesc, "Invalid parent passed!");
	if (!pParentDesc)
		return false;

	pInfo = GetFlashVarObj(pDesc, pParentDesc);
	if (!pInfo)
	{
#if defined(UIACTION_LOGGING)
		string sInstance;
		pDesc->GetInstanceString(sInstance, pParentDesc);
		UIACTION_ERROR("%s (%i): UIElement does not have object %s", GetName(), m_iInstanceID, sInstance.c_str());
#endif
		return false;
	}

	return true;
}

//------------------------------------------------------------------------------------
bool CFlashUIElement::CallFunction(const char* pFctName, const SUIArguments& args, TUIData* pDataRes, const char* pTmplName)
{
	const SUIEventDesc* pEventDesc = GetOrCreateFunctionDesc(pFctName);
	const SUIMovieClipDesc* pTmplDesc = pTmplName ? GetMovieClipDesc(pTmplName) : NULL;
	return CallFunction(pEventDesc, args, pDataRes, pTmplDesc);
}

//------------------------------------------------------------------------------------
bool CFlashUIElement::CallFunction(const SUIEventDesc* pFctDesc, const SUIArguments& args, TUIData* pDataRes, const SUIMovieClipDesc* pTmplDesc)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	SFlashObjectInfo* pInfo = NULL;
	if (!DefaultInfoCheck(pInfo, pFctDesc, pTmplDesc))
		return false;

#if defined(UIACTION_LOGGING)
	string sInstance;
	pFctDesc->GetInstanceString(sInstance, pTmplDesc);

	if (args.GetArgCount() != pFctDesc->InputParams.Params.size() && !pFctDesc->InputParams.IsDynamic)
	{
		UIACTION_WARNING("%s (%i): Called function %s (%s) with wrong number of arguments. Expected %i args, got %i args", GetName(), m_iInstanceID, pFctDesc->sDisplayName, sInstance.c_str(), pFctDesc->InputParams.Params.size(), args.GetArgCount());
	}
#endif

	if (!pInfo->pParent)
	{
		UIACTION_WARNING("%s (%i): Root movie clip missing", GetName(), m_iInstanceID);
		return false;
	}

	SFlashVarValue pRes(0);
	if (pInfo->pParent->Invoke(pInfo->sMember.c_str(), args.GetAsList(), args.GetArgCount(), &pRes))
	{
		if (pDataRes)
		{
			SUIArguments tmp(&pRes, 1);
			*pDataRes = tmp.GetArg(0);
		}

#if defined(UIACTION_LOGGING)
		{
			string res;
			if (pDataRes)
			{
				pDataRes->GetValueWithConversion(res);
				res = string(" - Res: ") + res;
			}
			UIACTION_LOG("%s (%i): Function invoked: %s (%s) - Args: %s%s", GetName(), m_iInstanceID, pFctDesc->sDisplayName, sInstance.c_str(), args.GetAsString(), res.c_str());
		}
#endif

		ForceLazyUpdateInl();
		RemoveFlashVarObj(pFctDesc);
		return true;
	}
#if defined(UIACTION_LOGGING)
	UIACTION_ERROR("%s (%i): Try to call undefined function %s (%s)", GetName(), m_iInstanceID, pFctDesc->sDisplayName, sInstance.c_str());
#endif
	RemoveFlashVarObj(pFctDesc);
	return false;
}

//------------------------------------------------------------------------------------
IFlashVariableObject* CFlashUIElement::GetMovieClip(const char* movieClipName, const char* pTmplName)
{
	const SUIMovieClipDesc* pMCDesc = GetOrCreateMovieClipDesc(movieClipName);
	const SUIMovieClipDesc* pTmplDesc = pTmplName ? GetMovieClipDesc(pTmplName) : NULL;
	return GetMovieClip(pMCDesc, pTmplDesc);
}

//------------------------------------------------------------------------------------
IFlashVariableObject* CFlashUIElement::GetMovieClip(const SUIMovieClipDesc* pMovieClipDesc, const SUIMovieClipDesc* pTmplDesc)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	SFlashObjectInfo* pInfo = NULL;
	if (!DefaultInfoCheck(pInfo, pMovieClipDesc, pTmplDesc))
		return NULL;

	if (pInfo->pObj->IsDisplayObject())
	{
		return pInfo->pObj;
	}
	return NULL;
}

//------------------------------------------------------------------------------------
IFlashVariableObject* CFlashUIElement::CreateMovieClip(const SUIMovieClipDesc*& pNewInstanceDesc, const char* movieClipTemplate, const char* mcParentName, const char* mcInstanceName)
{
	const SUIMovieClipDesc* pMCTmplDesc = GetOrCreateMovieClipTmplDesc(movieClipTemplate);
	const SUIMovieClipDesc* pParentDesc = mcParentName ? GetMovieClipDesc(mcParentName) : NULL;
	return CreateMovieClip(pNewInstanceDesc, pMCTmplDesc, pParentDesc, mcInstanceName);
}

//------------------------------------------------------------------------------------
IFlashVariableObject* CFlashUIElement::CreateMovieClip(const SUIMovieClipDesc*& pNewInstanceDesc, const SUIMovieClipDesc* pMovieClipTemplateDesc, const SUIMovieClipDesc* pParentMC, const char* mcInstanceName)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (!CFlashUI::CV_gfx_uiaction_enable)
		return NULL;

	if (!LazyInit())
		return NULL;

	const SUIMovieClipDesc* pParentDesc = pParentMC ? pParentMC : m_pRoot;
	CRY_ASSERT_MESSAGE(pParentDesc, "Invalid parent passed!");
	if (!pParentDesc)
		return NULL;

	string newName;
	if (mcInstanceName)
	{
		if (const SUIMovieClipDesc* pDesc = GetMovieClipDesc(mcInstanceName))
		{
			if (pDesc->pParent)
			{
				IFlashVariableObject* pVar = GetMovieClip(pDesc, pParentDesc);
				UIACTION_WARNING("%s (%i): Try to create new movieclip %s that already exist!", GetName(), m_iInstanceID, mcInstanceName);
				pNewInstanceDesc = pDesc;
				return pVar;
			}
		}
		newName = mcInstanceName;
	}
	else
	{
		int i = 0;
		while (true)
		{
			newName.Format("%s_%i", pMovieClipTemplateDesc->sDisplayName, i++);
			if (const SUIMovieClipDesc* pDesc = GetMovieClipDesc(newName.c_str()))
			{
				if (pDesc->pParent == NULL)
					break;
			}
			else
				break;
		}
	}

	IFlashVariableObject* pNewMc = NULL;
	SFlashObjectInfo* pParent = GetFlashVarObj(pParentDesc);
	if (pParent)
	{
		string instanceName;
		pParentDesc->GetInstanceString(instanceName);
		instanceName += ".";
		instanceName += newName.c_str();
		pParent->pObj->AttachMovie(pNewMc, pMovieClipTemplateDesc->sName, instanceName.c_str());
		if (pNewMc)
		{
			pParent->pObj->SetMember(newName.c_str(), pNewMc); // we have to set this member otherwise GetMember will return NULL even if it exists!
			SUIMovieClipDesc* pMCDesc = const_cast<SUIMovieClipDesc*>(GetOrCreateMovieClipDesc(newName.c_str()));
			pMCDesc->pParent = pParentDesc;
			SFlashObjectInfo* pNewInfo = &m_variableObjects[CCryName(instanceName.c_str())];
			pNewInfo->pObj = pNewMc;
			pNewInfo->pParent = pParent->pObj;
			pNewInfo->sMember = newName;
			m_variableObjectsLookup[pParentDesc][pMCDesc] = pNewInfo;
			pNewInstanceDesc = pMCDesc;
			m_pFlashplayer->Advance(0);
			UIACTION_LOG("%s (%i): Created Movieclip from template %s - Instancename %s", GetName(), m_iInstanceID, pMovieClipTemplateDesc->sDisplayName, instanceName.c_str());
		}
	}
	ForceLazyUpdateInl();
	return pNewMc;
}

//------------------------------------------------------------------------------------
void CFlashUIElement::RemoveMovieClip(const char* movieClipName)
{
	RemoveMovieClip(GetMovieClipDesc(movieClipName));
}

//------------------------------------------------------------------------------------
void CFlashUIElement::RemoveMovieClip(const SUIParameterDesc* pMovieClipDesc)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	SFlashObjectInfo* pInfo = NULL;
	if (!DefaultInfoCheck(pInfo, pMovieClipDesc, NULL))
		return;

	if (pInfo->pParent == NULL)
	{
		UIACTION_ERROR("%s (%i): Not allowed to remove _root MC!", GetName(), m_iInstanceID);
		return;
	}
	SUIParameterDesc* pMC = const_cast<SUIParameterDesc*>(pMovieClipDesc);
	pInfo->pObj->Invoke0("removeMovieClip");
	pInfo->pParent->SetMember(pInfo->sMember.c_str(), NULL);
	RemoveFlashVarObj(pMC);
	UIACTION_LOG("%s (%i): Removed Movieclip %s", GetName(), m_iInstanceID, pMovieClipDesc->sDisplayName);
	pMC->pParent = NULL;
}

//------------------------------------------------------------------------------------
void CFlashUIElement::RemoveMovieClip(IFlashVariableObject* flashVarObj)
{
	for (TVarMap::iterator it = m_variableObjects.begin(); it != m_variableObjects.end(); ++it)
	{
		if (flashVarObj == it->second.pObj)
		{
			if (const SUIParameterDesc* pDesc = GetDescForInfo(&it->second))
			{
				RemoveMovieClip(pDesc);
				return;
			}
		}
	}
	CRY_ASSERT_MESSAGE(false, "The given IFlashVariableObject was not created thru the UISystem!");
}

//------------------------------------------------------------------------------------
bool CFlashUIElement::SetVariable(const char* pVarName, const TUIData& value, const char* pTmplName)
{
	const SUIParameterDesc* pVarDesc = GetOrCreateVariableDesc(pVarName);
	const SUIMovieClipDesc* pTmplDesc = pTmplName ? GetMovieClipDesc(pTmplName) : NULL;
	return SetVariableInt(pVarDesc, value, pTmplDesc);
}

//------------------------------------------------------------------------------------
bool CFlashUIElement::SetVariable(const SUIParameterDesc* pVarDesc, const TUIData& value, const SUIMovieClipDesc* pTmplDesc)
{
	return SetVariableInt(pVarDesc, value, pTmplDesc);
}

//------------------------------------------------------------------------------------
bool CFlashUIElement::SetVariableInt(const SUIParameterDesc* pVarDesc, const TUIData& value, const SUIMovieClipDesc* pTmplDesc, bool bCreate /*= false*/)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	SFlashObjectInfo* pInfo = NULL;
	if (!DefaultInfoCheck(pInfo, pVarDesc, pTmplDesc))
		return false;

	SUIArguments tmp;
	tmp.AddArgument(value);
	if (pInfo->pParent->SetMember(pInfo->sMember.c_str(), *tmp.GetAsList()))
	{

#if defined(UIACTION_LOGGING)
		string sInstance;
		pVarDesc->GetInstanceString(sInstance, pTmplDesc);
		{
			string sVal;
			value.GetValueWithConversion(sVal);
			UIACTION_LOG("%s (%i): Variable set %s (%s) - Value: %s", GetName(), m_iInstanceID, pVarDesc->sDisplayName, sInstance.c_str(), sVal.c_str());
		}
#endif

		ForceLazyUpdateInl();
		return true;
	}
#if defined(UIACTION_LOGGING)
	string sInstance;
	pVarDesc->GetInstanceString(sInstance, pTmplDesc);
	UIACTION_ERROR("%s (%i): Try to access undefined variable %s (%s)", GetName(), m_iInstanceID, pVarDesc->sDisplayName, sInstance.c_str());
#endif
	return false;
}

//------------------------------------------------------------------------------------
bool CFlashUIElement::GetVariable(const char* pVarName, TUIData& valueOut, const char* pTmplName)
{
	const SUIParameterDesc* pVarDesc = GetOrCreateVariableDesc(pVarName);
	const SUIMovieClipDesc* pTmplDesc = pTmplName ? GetMovieClipDesc(pTmplName) : NULL;
	return GetVariable(pVarDesc, valueOut, pTmplDesc);
}

//------------------------------------------------------------------------------------
bool CFlashUIElement::GetVariable(const SUIParameterDesc* pVarDesc, TUIData& valueOut, const SUIMovieClipDesc* pTmplDesc)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	SFlashObjectInfo* pInfo = NULL;
	if (!DefaultInfoCheck(pInfo, pVarDesc, pTmplDesc))
		return false;

	SFlashVarValue val = SFlashVarValue::CreateUndefined();
	if (pInfo->pParent->GetMember(pInfo->sMember.c_str(), val))
	{
		SUIArguments tmp(&val, 1);
		valueOut = tmp.GetArg(0);

#if defined(UIACTION_LOGGING)
		string sInstance;
		pVarDesc->GetInstanceString(sInstance, pTmplDesc);
		UIACTION_LOG("%s (%i): Variable get %s (%s) - Value: %s", GetName(), m_iInstanceID, pVarDesc->sDisplayName, sInstance.c_str(), tmp.GetAsString());
#endif
		return true;
	}
#if defined(UIACTION_LOGGING)
	string sInstance;
	pVarDesc->GetInstanceString(sInstance, pTmplDesc);
	UIACTION_ERROR("%s (%i): Try to access undefined variable %s (%s)", GetName(), m_iInstanceID, pVarDesc->sDisplayName, sInstance.c_str());
#endif
	return false;
}

//------------------------------------------------------------------------------------
bool CFlashUIElement::CreateVariable(const SUIParameterDesc*& pNewDesc, const char* varName, const TUIData& value, const char* pTmplName)
{
	const SUIMovieClipDesc* pTmplDesc = pTmplName ? GetMovieClipDesc(pTmplName) : NULL;
	return CreateVariable(pNewDesc, varName, value, pTmplDesc);
}

//------------------------------------------------------------------------------------
bool CFlashUIElement::CreateVariable(const SUIParameterDesc*& pNewDesc, const char* varName, const TUIData& value, const SUIMovieClipDesc* pTmplDesc)
{
	bool bExist;
	pNewDesc = GetOrCreateVariableDesc(varName, &bExist);
	if (bExist)
	{
		UIACTION_ERROR("%s (%i): Try to create variable %s that already exists!", GetName(), m_iInstanceID, pNewDesc->sDisplayName);
	}
	ForceLazyUpdateInl();
	return SetVariableInt(pNewDesc, value, pTmplDesc, true);
}

//------------------------------------------------------------------------------------
bool CFlashUIElement::SetArray(const char* pArrayName, const SUIArguments& values, const char* pTmplName)
{
	const SUIParameterDesc* pArrayDesc = GetOrCreateArrayDesc(pArrayName);
	const SUIMovieClipDesc* pTmplDesc = pTmplName ? GetMovieClipDesc(pTmplName) : NULL;
	return SetArray(pArrayDesc, values, pTmplDesc);
}

//------------------------------------------------------------------------------------
bool CFlashUIElement::SetArray(const SUIParameterDesc* pArrayDesc, const SUIArguments& values, const SUIMovieClipDesc* pTmplDesc)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	SFlashObjectInfo* pInfo = NULL;
	if (!DefaultInfoCheck(pInfo, pArrayDesc, pTmplDesc))
		return false;

	if (pInfo->pObj->IsArray())
	{
		const int valueCount = values.GetArgCount();
		pInfo->pObj->SetArraySize(valueCount);
		const SFlashVarValue* pValueList = values.GetAsList();
		for (int i = 0; i < valueCount; ++i)
		{
			pInfo->pObj->SetElement(i, pValueList[i]);
		}

		ForceLazyUpdateInl();
#if defined(UIACTION_LOGGING)
		string sInstance;
		pArrayDesc->GetInstanceString(sInstance, pTmplDesc);
		UIACTION_LOG("%s (%i): Array set %s (%s) - Values: %s", GetName(), m_iInstanceID, pArrayDesc->sDisplayName, sInstance.c_str(), values.GetAsString());
#endif
		return true;
	}
#if defined(UIACTION_LOGGING)
	string sInstance;
	pArrayDesc->GetInstanceString(sInstance, pTmplDesc);
	UIACTION_ERROR("%s (%i): Try to access undefined array %s (%s)", GetName(), m_iInstanceID, pArrayDesc->sDisplayName, sInstance.c_str());
#endif
	return false;
}

//------------------------------------------------------------------------------------
bool CFlashUIElement::GetArray(const char* pArrayName, SUIArguments& valuesOut, const char* pTmplName)
{
	const SUIParameterDesc* pArrayDesc = GetOrCreateArrayDesc(pArrayName);
	const SUIMovieClipDesc* pTmplDesc = pTmplName ? GetMovieClipDesc(pTmplName) : NULL;
	return GetArray(pArrayDesc, valuesOut, pTmplDesc);
}

//------------------------------------------------------------------------------------
bool CFlashUIElement::GetArray(const SUIParameterDesc* pArrayDesc, SUIArguments& valuesOut, const SUIMovieClipDesc* pTmplDesc)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	SFlashObjectInfo* pInfo = NULL;
	if (!DefaultInfoCheck(pInfo, pArrayDesc, pTmplDesc))
		return false;

	if (pInfo->pObj->IsArray())
	{
		const int size = pInfo->pObj->GetArraySize();
		SFlashVarValue val = SFlashVarValue::CreateUndefined();

		for (int i = 0; i < size; ++i)
		{
			pInfo->pObj->GetElement(i, val);
			valuesOut.AddArguments(&val, 1);
		}

#if defined(UIACTION_LOGGING)
		string sInstance;
		pArrayDesc->GetInstanceString(sInstance, pTmplDesc);
		UIACTION_LOG("%s (%i): Array get %s (%s) - Values: %s", GetName(), m_iInstanceID, pArrayDesc->sDisplayName, sInstance.c_str(), valuesOut.GetAsString());
#endif
		return true;
	}
#if defined(UIACTION_LOGGING)
	string sInstance;
	pArrayDesc->GetInstanceString(sInstance, pTmplDesc);
	UIACTION_ERROR("%s (%i): Try to access undefined array %s (%s)", GetName(), m_iInstanceID, pArrayDesc->sDisplayName, sInstance.c_str());
#endif
	return false;
}

//------------------------------------------------------------------------------------
bool CFlashUIElement::CreateArray(const SUIParameterDesc*& pNewDesc, const char* arrayName, const SUIArguments& values, const char* pTmplName)
{
	const SUIMovieClipDesc* pTmplDesc = pTmplName ? GetMovieClipDesc(pTmplName) : NULL;
	return CreateArray(pNewDesc, arrayName, values, pTmplDesc);
}

//------------------------------------------------------------------------------------
bool CFlashUIElement::CreateArray(const SUIParameterDesc*& pNewDesc, const char* arrayName, const SUIArguments& values, const SUIMovieClipDesc* pTmplDesc)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (!CFlashUI::CV_gfx_uiaction_enable)
		return false;

	if (!LazyInit())
		return false;

	const SUIMovieClipDesc* pParentDesc = pTmplDesc ? pTmplDesc : m_pRoot;
	CRY_ASSERT_MESSAGE(pParentDesc && arrayName, "Invalid parent passed!");
	if (!pParentDesc)
		return false;

	const SUIParameterDesc* pDesc = GetOrCreateArrayDesc(arrayName);
	if (pDesc->pParent)
	{
		SFlashObjectInfo* pInfo = GetFlashVarObj(pDesc, pParentDesc);
		if (pInfo)
		{
			UIACTION_WARNING("%s (%i): Try to create new array %s that already exist!", GetName(), m_iInstanceID, arrayName);
			return SetArray(pDesc, values);
			;
		}
	}

	IFlashVariableObject* pNewArray = NULL;
	SFlashObjectInfo* pParent = GetFlashVarObj(pParentDesc);
	if (pParent)
	{
		bool ok = m_pFlashplayer->CreateArray(pNewArray);
		CRY_ASSERT_MESSAGE(ok, "Failed to create new flash array!");
		pParent->pObj->SetMember(arrayName, pNewArray);

		const_cast<SUIParameterDesc*>(pDesc)->pParent = pParentDesc;
		string instanceName;
		pDesc->GetInstanceString(instanceName, pParentDesc);
		SFlashObjectInfo* pNewInfo = &m_variableObjects[CCryName(instanceName.c_str())];
		pNewInfo->pObj = pNewArray;
		pNewInfo->pParent = pParent->pObj;
		pNewInfo->sMember = arrayName;
		m_variableObjectsLookup[pParentDesc][pDesc] = pNewInfo;
		m_pFlashplayer->Advance(0);
		pNewDesc = pDesc;
		ForceLazyUpdateInl();
		return SetArray(pDesc, values);
	}
	UIACTION_ERROR("%s (%i): Try to create new array %s on invalid parent!", GetName(), m_iInstanceID, arrayName);
	return false;
}

//------------------------------------------------------------------------------------
void CFlashUIElement::LoadTexIntoMc(const char* movieClip, ITexture* pTexture, const char* pTmplName)
{
	const SUIParameterDesc* pMovieClipDesc = GetOrCreateMovieClipDesc(movieClip);
	const SUIMovieClipDesc* pTmplDesc = pTmplName ? GetMovieClipDesc(pTmplName) : NULL;
	LoadTexIntoMc(pMovieClipDesc, pTexture, pTmplDesc);
}

//------------------------------------------------------------------------------------
void CFlashUIElement::LoadTexIntoMc(const SUIParameterDesc* pMovieClipDesc, ITexture* pTexture, const SUIMovieClipDesc* pTmplDesc)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	SFlashObjectInfo* pInfo = NULL;
	if (!DefaultInfoCheck(pInfo, pMovieClipDesc, pTmplDesc))
		return;

	if (pInfo->pObj->IsDisplayObject())
	{
		string texStr;
		texStr.Format("img://TEXID:%i", pTexture->GetTextureID());
		pInfo->pObj->Invoke1("loadMovie", texStr.c_str());
		ForceLazyUpdateInl();
#if defined(UIACTION_LOGGING)
		string sInstance;
		pMovieClipDesc->GetInstanceString(sInstance, pTmplDesc);
		UIACTION_LOG("%s (%i): Loaded texture %s into movieclip %s (%s)", GetName(), m_iInstanceID, pTexture->GetName(), pMovieClipDesc->sDisplayName, sInstance.c_str());
#endif
	}
	else
	{
#if defined(UIACTION_LOGGING)
		string sInstance;
		pMovieClipDesc->GetInstanceString(sInstance, pTmplDesc);
		UIACTION_ERROR("%s (%i): Try to access non-existing movieclip %s", GetName(), m_iInstanceID, pMovieClipDesc->sDisplayName);
#endif
	}
}

//------------------------------------------------------------------------------------
void CFlashUIElement::UnloadTexFromMc(const char* movieClip, ITexture* pTexture, const char* pTmplName)
{
	const SUIParameterDesc* pMovieClipDesc = GetOrCreateMovieClipDesc(movieClip);
	const SUIMovieClipDesc* pTmplDesc = pTmplName ? GetMovieClipDesc(pTmplName) : NULL;
	UnloadTexFromMc(pMovieClipDesc, pTexture, pTmplDesc);
}

//------------------------------------------------------------------------------------
void CFlashUIElement::UnloadTexFromMc(const SUIParameterDesc* pMovieClipDesc, ITexture* pTexture, const SUIMovieClipDesc* pTmplDesc)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	SFlashObjectInfo* pInfo = NULL;
	if (!DefaultInfoCheck(pInfo, pMovieClipDesc, pTmplDesc))
		return;

	if (pInfo->pObj->IsDisplayObject())
	{
		string texStr;
		texStr.Format("img://TEXID:%i", pTexture->GetTextureID());
		pInfo->pObj->Invoke1("unloadMovie", texStr.c_str());
		ForceLazyUpdateInl();
#if defined(UIACTION_LOGGING)
		string sInstance;
		pMovieClipDesc->GetInstanceString(sInstance, pTmplDesc);
		UIACTION_LOG("%s (%i): Unloaded texture %s from movieclip %s (%s)", GetName(), m_iInstanceID, pTexture->GetName(), pMovieClipDesc->sDisplayName, sInstance.c_str());
#endif
	}
	else
	{
#if defined(UIACTION_LOGGING)
		string sInstance;
		pMovieClipDesc->GetInstanceString(sInstance, pTmplDesc);
		UIACTION_ERROR("%s (%i): Try to access non-existing movieclip %s", GetName(), m_iInstanceID, pMovieClipDesc->sDisplayName);
#endif
	}
}

//------------------------------------------------------------------------------------
void CFlashUIElement::ScreenToFlash(const float& px, const float& py, float& rx, float& ry, bool bStageScaleMode) const
{
	if (m_pFlashplayer)
	{
		const float flashWidth  = (float) m_pFlashplayer->GetWidth();
		const float flashHeigth = (float) m_pFlashplayer->GetHeight();
		float flashVPX, flashVPY, flashVPWidth, flashVPHeight;
		{
			int x, y, width, height;
			float aspect;
			m_pFlashplayer->GetViewport(x, y, width, height, aspect);
			flashVPX = (float) x;
			flashVPY = (float) y;
			flashVPWidth = (float) width;
			flashVPHeight = (float) height;
		}

		if (!m_constraints.bScale && !bStageScaleMode)
		{
			const float oAspect = flashWidth / flashHeigth;
			const float nAspect = flashVPWidth / flashVPHeight;
			if (oAspect < nAspect) flashVPWidth = flashVPHeight * oAspect;
			else flashVPHeight = flashVPWidth / oAspect;
		}

		const float screenWidth  = (float)gEnv->pRenderer->GetOverlayWidth();
		const float screenHeigth = (float)gEnv->pRenderer->GetOverlayHeight();

		const float screenX = px * screenWidth;
		const float screenY = py * screenHeigth;

		const float flashX = (screenX - flashVPX) / flashVPWidth;
		const float flashY = (screenY - flashVPY) / flashVPHeight;

		rx = flashX * flashWidth;
		ry = flashY * flashHeigth;

		if (bStageScaleMode)
		{
			float factorX = screenWidth / flashWidth;
			float factorY = screenHeigth / flashHeigth;

			rx *= factorX;
			ry *= factorY;

			rx -= (screenWidth - flashWidth) / 2.f;
			ry -= (screenHeigth - flashHeigth) / 2.f;
		}
	}
}

//------------------------------------------------------------------------------------
void CFlashUIElement::WorldToFlash(const Matrix34& camMat, const Vec3& worldpos, Vec3& flashpos, Vec2& borders, float& scale, bool bStageScaleMode) const
{
	// calculate scale
	const float distance = (camMat.GetTranslation() - worldpos).GetLength();
	
	// calculate screen x,y coordinates
	CCamera cam = GetISystem()->GetViewCamera();
	cam.SetMatrix(camMat);
	cam.Project(worldpos, flashpos);
	flashpos.x = flashpos.x / (f32)gEnv->pRenderer->GetOverlayWidth();
	flashpos.y = flashpos.y / (f32)gEnv->pRenderer->GetOverlayHeight();

	cam.CalculateRenderMatrices();
	Matrix44 projMat = cam.GetRenderProjectionMatrix();
	scale = MatMulVec3(projMat, Vec3(-1.f, -1.f, distance)).x;

	// overflow
	borders.x = flashpos.x<0 ? -1.f : flashpos.x> 1.f ? 1.f : flashpos.z < 1.f ? 0 : -1.f;
	borders.y = flashpos.y<0 ? -1.f : flashpos.y> 1.f ? 1.f : flashpos.z < 1.f ? 0 : -1.f;

	// store screen pos in x/y and depth in z (clamp to borders if not on screen)
	flashpos.x = borders.x == 0 ? flashpos.x : borders.x < 0 ? 0 : 1.f;
	flashpos.y = borders.y == 0 ? flashpos.y : borders.y < 0 ? 0 : 1.f;
	;
	flashpos.z = distance;

	// finally transform into flash coordinates
	ScreenToFlash(flashpos.x, flashpos.y, flashpos.x, flashpos.y, bStageScaleMode);
}

//------------------------------------------------------------------------------------
Vec3 CFlashUIElement::MatMulVec3(const Matrix44& m, const Vec3& v) const
{
	Vec3 r;
	const float fInvW = 1.0f / (m.m30 * v.x + m.m31 * v.y + m.m32 * v.z + m.m33);
	r.x = (m.m00 * v.x + m.m01 * v.y + m.m02 * v.z + m.m03) * fInvW;
	r.y = (m.m10 * v.x + m.m11 * v.y + m.m12 * v.z + m.m13) * fInvW;
	r.z = (m.m20 * v.x + m.m21 * v.y + m.m22 * v.z + m.m23) * fInvW;
	return r;
}

//------------------------------------------------------------------------------------
void CFlashUIElement::AddTexture(IDynTextureSource* pDynTexture)
{
	CRY_ASSERT_MESSAGE(pDynTexture, "NULL pointer passed!");
	if (pDynTexture)
	{
		UIACTION_LOG("%s (%i): UIElement registered by texture \"%p\"", GetName(), m_iInstanceID, pDynTexture);
		const bool ok = m_textures.PushBackUnique(pDynTexture);
		CRY_ASSERT_MESSAGE(ok, "Texture already registered!");

		SetVisible(true);

		if (m_pFlashplayer)
		{
			m_pFlashplayer->SetViewScaleMode(IFlashPlayer::eSM_ExactFit);
			m_pFlashplayer->SetViewport   (0, 0, m_pFlashplayer->GetWidth(), m_pFlashplayer->GetHeight(), 1.f);
			m_pFlashplayer->SetScissorRect(0, 0, m_pFlashplayer->GetWidth(), m_pFlashplayer->GetHeight());
		}
	}
}

//------------------------------------------------------------------------------------
void CFlashUIElement::RemoveTexture(IDynTextureSource* pDynTexture)
{
	CRY_ASSERT_MESSAGE(pDynTexture, "NULL pointer passed!");
	if (pDynTexture)
	{
		UIACTION_LOG("%s (%i): UIElement unregistered by texture \"%p\"", GetName(), m_iInstanceID, pDynTexture);
		const bool ok = m_textures.FindAndErase(pDynTexture);
		CRY_ASSERT_MESSAGE(ok, "Texture was never registered or already unregistered!");
	}
}

//------------------------------------------------------------------------------------
void CFlashUIElement::SendCursorEvent(SFlashCursorEvent::ECursorState evt, int iX, int iY, int iButton /*= 0*/, float fWheel /*= 0.f*/)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (!CFlashUI::CV_gfx_uiaction_enable)
		return;

	if (!m_pFlashplayer || m_bIsHideRequest)
		return;

	UpdateFlags();

	if (HasExtTexture() && gEnv->pRenderer)
	{
		int x = 0;
		int y = 0;
		int width  = gEnv->pRenderer->GetOverlayWidth();
		int height = gEnv->pRenderer->GetOverlayHeight();

		float fX = (float) iX / (float) width;
		float fY = (float) iY / (float) height;
		float aspect;
		m_pFlashplayer->GetViewport(x, y, width, height, aspect);
		iX = (int) (fX * (float)width);
		iY = (int) (fY * (float)height);
	}

	m_pFlashplayer->ScreenToClient(iX, iY);
	m_pFlashplayer->SendCursorEvent(SFlashCursorEvent(evt, iX, iY, iButton, fWheel));
	m_pFlashplayer->Advance(0);
	ForceLazyUpdateInl();
}

void CFlashUIElement::SendKeyEvent(const SFlashKeyEvent& evt)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (!CFlashUI::CV_gfx_uiaction_enable)
		return;

	if (!m_pFlashplayer || m_bIsHideRequest)
		return;

	m_pFlashplayer->SendKeyEvent(evt);
	m_pFlashplayer->Advance(0);
	ForceLazyUpdateInl();
}

void CFlashUIElement::SendCharEvent(const SFlashCharEvent& charEvent)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (!CFlashUI::CV_gfx_uiaction_enable)
		return;

	if (!m_pFlashplayer || m_bIsHideRequest)
		return;

	m_pFlashplayer->SendCharEvent(charEvent);
	m_pFlashplayer->Advance(0);
	ForceLazyUpdateInl();
}

void CFlashUIElement::SendControllerEvent(EControllerInputEvent event, EControllerInputState state, float value)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (!CFlashUI::CV_gfx_uiaction_enable)
		return;

	if (!m_pFlashplayer || m_bIsHideRequest)
		return;

	if (m_pFlashplayer->IsAvailable("cry_onController"))
	{
		HideCursor();

		SFlashVarValue args[] = { SFlashVarValue((int) event), SFlashVarValue(state == eCIS_OnRelease), SFlashVarValue(value) };
		m_pFlashplayer->Invoke("cry_onController", args, 3);
		m_pFlashplayer->Advance(0);
	}
	ForceLazyUpdateInl();
}

//-------------------------------------------------------------------
void CFlashUIElement::GetMemoryUsage(ICrySizer* s) const
{
	SIZER_SUBCOMPONENT_NAME(s, "UIElement");
	s->AddObject(this, sizeof(*this));
	s->Add(&m_variables, SUIElementSerializer::GetMemUsage(m_variables));
	s->Add(&m_arrays, SUIElementSerializer::GetMemUsage(m_arrays));
	s->Add(&m_displayObjects, SUIElementSerializer::GetMemUsage(m_displayObjects));
	s->Add(&m_displayObjectsTmpl, SUIElementSerializer::GetMemUsage(m_displayObjectsTmpl));
	s->Add(&m_functions, SUIElementSerializer::GetMemUsage(m_functions));
	s->Add(&m_events, SUIElementSerializer::GetMemUsage(m_events));

	s->AddContainer(m_variableObjects);
	s->Add(&m_eventListener, m_eventListener.MemSize());
	TDynTextures::TVec dynTex = m_textures.GetCopy();
	s->AddContainer(dynTex);

	{
		SIZER_SUBCOMPONENT_NAME(s, "Instances");
		s->AddContainer(m_instances);
		for (TUIElements::const_iterator iter = m_instances.begin(); iter != m_instances.end(); ++iter)
		{
			if (*iter != this)
				(*iter)->GetMemoryUsage(s);
		}
	}
}

//------------------------------------------------------------------------------------
void CFlashUIElement::HandleFSCommand(const char* sCommand, const char* sArgs, void* pUserData)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (!CFlashUI::CV_gfx_uiaction_enable)
		return;

	SUIArguments args;
	args.SetDelimiter(",");
	args.SetArguments(sArgs);
	args.SetDelimiter(UIARGS_DEFAULT_DELIMITER);

	if (HandleInternalCommand(sCommand, args))
	{
		UIACTION_LOG("%s (%i): Internal command received %s (args: %s)", GetName(), m_iInstanceID, sCommand, sArgs);
	}
	else
	{
		SUIEventDesc* pEventDesc = NULL;
		for (TUIEventsLookup::iterator iter = m_events.begin(); iter != m_events.end(); ++iter)
		{
			if (!strcmp((*iter)->sName, sCommand))
			{
				pEventDesc = *iter;
				break;
			}
		}

		if (pEventDesc)
		{
			UIACTION_LOG("%s (%i): Event %s (%s) sended (args: %s)", GetName(), m_iInstanceID, pEventDesc->sDisplayName, pEventDesc->sName, args.GetAsString());

			for (TUIEventListener::Notifier notifier(m_eventListener); notifier.IsValid(); notifier.Next())
				notifier->OnUIEvent(this, *pEventDesc, args);
		}
		else
		{
			UIACTION_LOG("%s (%i): Fscommand %s sended (args: %s)", GetName(), m_iInstanceID, sCommand, args.GetAsString());

			for (TUIEventListener::Notifier notifier(m_eventListener); notifier.IsValid(); notifier.Next())
				notifier->OnUIEventEx(this, sCommand, args, pUserData);
		}
	}
}

//------------------------------------------------------------------------------------
bool CFlashUIElement::HandleInternalCommand(const char* sCommand, const SUIArguments& args)
{
	if (strcmp(sCommand, "cry_hideElement") == 0)
	{
		UIACTION_LOG("%s (%i): UIElement received \"cry_hideElement\" from ActionScript", GetName(), m_iInstanceID);
		SetVisible(false);
		return true;
	}
	if (strcmp(sCommand, "cry_virtualKeyboard") == 0)
	{
		UIACTION_LOG("%s (%i): UIElement received \"cry_virtualKeyboard\" from ActionScript with args %s", GetName(), m_iInstanceID, args.GetAsString());
		string mode;
		string utf8Title;
		string utf8InitialInput;
		int maxchars;

		bool ok = args.GetArg(0, mode);
		ok &= args.GetArg(1, utf8Title);
		ok &= args.GetArg(2, utf8InitialInput);
		ok &= args.GetArg(3, maxchars);

		if (ok && utf8Title[0] == '@') // this is a localization label (label name stored in utf8Title)
		{
			string utf8Localized;
			if (gEnv->pSystem->GetLocalizationManager()->LocalizeLabel(utf8Title.c_str(), utf8Localized))
			{
				utf8Title.swap(utf8Localized);
			}
		}

		CRY_ASSERT_MESSAGE(ok, "cry_virtualKeyboard received with wrong arguments!");
		if (ok)
		{
			unsigned int flags = IPlatformOS::KbdFlag_Default;
			if (strcmpi(mode.c_str(), "GamerTag") == 0)
				flags &= IPlatformOS::KbdFlag_GamerTag;
			else if (strcmpi(mode.c_str(), "Password") == 0)
				flags &= IPlatformOS::KbdFlag_Password;
			else if (strcmpi(mode.c_str(), "Email") == 0)
				flags &= IPlatformOS::KbdFlag_Email;

			m_pFlashUI->DisplayVirtualKeyboard(flags, utf8Title, utf8InitialInput, maxchars, this);
		}
		else
		{
			UIACTION_ERROR("%s (%i): cry_virtualKeyboard received with wrong arguments! Args: %s", GetName(), m_iInstanceID, args.GetAsString());
		}
		return true;
	}
	if (strcmp(sCommand, "cry_imeFocus") == 0)
	{
		UIACTION_LOG("%s (%i): UIElement received \"cry_imeFocus\" from ActionScript", GetName(), m_iInstanceID);
		const auto& pPlayer = GetFlashPlayer();
		if (pPlayer) pPlayer->SetImeFocus();
		return true;
	}
	return false;
}

//------------------------------------------------------------------------------------
void CFlashUIElement::KeyboardCancelled()
{
	if (!m_pFlashplayer)
		return;

	if (!CFlashUI::CV_gfx_uiaction_enable)
		return;

	if (m_pFlashplayer->IsAvailable("cry_onVirtualKeyboard"))
	{
		SUIArguments args;
		args.AddArgument(false);
		args.AddArgument("");
		m_pFlashplayer->Invoke("cry_onVirtualKeyboard", args.GetAsList(), args.GetArgCount());
	}
}

//------------------------------------------------------------------------------------
void CFlashUIElement::KeyboardFinished(const char* pInString)
{
	if (!m_pFlashplayer)
		return;

	if (!CFlashUI::CV_gfx_uiaction_enable)
		return;

	if (m_pFlashplayer->IsAvailable("cry_onVirtualKeyboard"))
	{
		SUIArguments args;
		args.AddArgument(true);
		args.AddArgument(pInString);
		m_pFlashplayer->Invoke("cry_onVirtualKeyboard", args.GetAsList(), args.GetArgCount());
	}
}

//------------------------------------------------------------------------------------
const SUIParameterDesc* CFlashUIElement::GetOrCreateVariableDesc(const char* pVarName, bool* bExist /*= NULL*/)
{
	const SUIParameterDesc* pVarDesc = GetVariableDesc(pVarName);
	if (bExist) *bExist = pVarDesc != NULL;
	if (!pVarDesc)
	{
		pVarName = GetStringBuffer(pVarName);
		SUIParameterDesc* pNewVarDesc = new SUIParameterDesc(pVarName, pVarName, "New dynamic created var");
		m_variables.push_back(pNewVarDesc);
		pVarDesc = pNewVarDesc;
		UIACTION_LOG("%s (%i): Created new dynamic variable definition %s", GetName(), m_iInstanceID, pVarName);
	}
	return pVarDesc;
}

//------------------------------------------------------------------------------------
const SUIParameterDesc* CFlashUIElement::GetOrCreateArrayDesc(const char* pArrayName, bool* bExist /*= NULL*/)
{
	const SUIParameterDesc* pArrayDesc = GetArrayDesc(pArrayName);
	if (bExist) *bExist = pArrayDesc != NULL;
	if (!pArrayDesc)
	{
		pArrayName = GetStringBuffer(pArrayName);
		SUIParameterDesc* pNewArrayDesc = new SUIParameterDesc(pArrayName, pArrayName, "New dynamic created array");
		m_arrays.push_back(pNewArrayDesc);
		pArrayDesc = pNewArrayDesc;
		UIACTION_LOG("%s (%i): Created new dynamic array definition %s", GetName(), m_iInstanceID, pArrayName);
	}
	return pArrayDesc;
}

//------------------------------------------------------------------------------------
const SUIMovieClipDesc* CFlashUIElement::GetOrCreateMovieClipDesc(const char* pMovieClipName, bool* bExist /*= NULL*/)
{
	const SUIMovieClipDesc* pMCDesc = GetMovieClipDesc(pMovieClipName);
	if (bExist) *bExist = pMCDesc != NULL;
	if (!pMCDesc)
	{
		pMovieClipName = GetStringBuffer(pMovieClipName);
		SUIMovieClipDesc* pNewMCDesc = new SUIMovieClipDesc(pMovieClipName, pMovieClipName, "New dynamic created MC");
		m_displayObjects.push_back(pNewMCDesc);
		pMCDesc = pNewMCDesc;
		UIACTION_LOG("%s (%i): Created new dynamic movieclip definition %s", GetName(), m_iInstanceID, pMovieClipName);
	}
	return pMCDesc;
}

//------------------------------------------------------------------------------------
const SUIMovieClipDesc* CFlashUIElement::GetOrCreateMovieClipTmplDesc(const char* pMovieClipTmplName)
{
	const SUIMovieClipDesc* pMCTmplDesc = GetMovieClipTmplDesc(pMovieClipTmplName);
	if (!pMCTmplDesc)
	{
		pMovieClipTmplName = GetStringBuffer(pMovieClipTmplName);
		SUIMovieClipDesc* pNewMCTmplDesc = new SUIMovieClipDesc(pMovieClipTmplName, pMovieClipTmplName, "New dynamic created MC Template");
		m_displayObjectsTmpl.push_back(pNewMCTmplDesc);
		pMCTmplDesc = pNewMCTmplDesc;
		UIACTION_LOG("%s (%i): Created new dynamic movieclip template %s", GetName(), m_iInstanceID, pMovieClipTmplName);
	}
	return pMCTmplDesc;
}

//------------------------------------------------------------------------------------
const SUIEventDesc* CFlashUIElement::GetOrCreateFunctionDesc(const char* pFunctionName)
{
	const SUIEventDesc* pEventDesc = GetFunctionDesc(pFunctionName);
	if (!pEventDesc)
	{
		pFunctionName = GetStringBuffer(pFunctionName);
		SUIEventDesc* pNewEventDesc = new SUIEventDesc(pFunctionName, pFunctionName, "Dynamic Created Fct", true);
		m_functions.push_back(pNewEventDesc);
		pEventDesc = pNewEventDesc;
		UIACTION_LOG("%s (%i): Created new dynamic function definition %s", GetName(), m_iInstanceID, pFunctionName);
	}
	return pEventDesc;
}

//------------------------------------------------------------------------------------
CFlashUIElement::SFlashObjectInfo* CFlashUIElement::GetFlashVarObj(const SUIParameterDesc* pDesc, const SUIMovieClipDesc* pTmplDesc)
{
	CRY_ASSERT_MESSAGE(pDesc, "NULL pointer passed!");
	SFlashObjectInfo* pVarInfo = NULL;
	const SUIMovieClipDesc* pRootDesc = pDesc != m_pRoot ? pTmplDesc ? pTmplDesc : m_pRoot : NULL;

	TVarMapLookup& map = m_variableObjectsLookup[pRootDesc];
	TVarMapLookup::iterator iter = map.find(pDesc);
	if (iter != map.end())
		pVarInfo = iter->second;

	if (pDesc && !pVarInfo)
	{
		string sInstance;
		pDesc->GetInstanceString(sInstance, pRootDesc);
		TVarMap::iterator it = m_variableObjects.find(CCryName(sInstance.c_str()));
		if (it != m_variableObjects.end())
		{
			pVarInfo = &it->second;
			map[pDesc] = pVarInfo;
		}
		else
		{
			// since accessing thru path foo.bar.mc does not work in dynamic created MCs we have to go thru the path manually
			// we will collect all members on the way and store them in the m_variableObjects list for easy release on unload
			string currPathToObj = "_root";
			SUIArguments path;
			path.SetDelimiter(".");
			path.SetArguments(sInstance.c_str());
			const int count = path.GetArgCount();
			IFlashVariableObject* pParent = m_variableObjects[CCryName(m_pRoot->sName)].pObj;
			IFlashVariableObject* pVar = NULL;
			for (int i = 1; i < count && pParent; ++i)
			{
				string member;
				path.GetArg(i, member);
				currPathToObj += ".";
				currPathToObj += member;
				SFlashObjectInfo* pCurrVarInfo = NULL;
				it = m_variableObjects.find(CCryName(currPathToObj.c_str()));
				if (it != m_variableObjects.end())
				{
					pCurrVarInfo = &it->second;
					pVar = pCurrVarInfo->pObj;
				}
				else
				{
					pParent->GetMember(member.c_str(), pVar);
					if (pVar)
					{
						pCurrVarInfo = &m_variableObjects[CCryName(currPathToObj.c_str())];
						pCurrVarInfo->pObj = pVar;
						pCurrVarInfo->pParent = pParent;
						pCurrVarInfo->sMember = member;
					}
				}
				if (i == count - 1)
				{
					pVarInfo = pCurrVarInfo;
					map[pDesc] = pVarInfo;
				}
				pParent = pVar;
			}
		}
	}
	return pVarInfo;
}

//------------------------------------------------------------------------------------
void CFlashUIElement::RemoveFlashVarObj(const SUIParameterDesc* pDesc)
{
	typedef std::vector<std::pair<TVarMap::iterator, std::pair<const SUIParameterDesc*, const SUIParameterDesc*>>> TDelList;
	TDelList iterToDelete;
	string instance;
	pDesc->GetInstanceString(instance);
	const char* sInstanceStr = instance.c_str();
	int lenInstance = instance.length();
	for (TVarMap::iterator it = m_variableObjects.begin(); it != m_variableObjects.end(); ++it)
	{
		const char* instStr = it->first.c_str();
		int len = it->first.length();
		if (strstr(instStr, sInstanceStr) && (len == lenInstance || (len > lenInstance && instStr[lenInstance] == '.')))
		{
			const SUIParameterDesc* pParentDescToDel = NULL;
			const SUIParameterDesc* pDescToDel = GetDescForInfo(&it->second, &pParentDescToDel);
			UIACTION_LOG("%s (%i): Release FlashVarObject %s (%s)", GetName(), m_iInstanceID, pDescToDel ? pDescToDel->sDisplayName : "", it->first.c_str());
			iterToDelete.push_back(std::make_pair(it, std::make_pair(pDescToDel, pParentDescToDel)));
		}
	}
	CRY_ASSERT_MESSAGE(!iterToDelete.empty(), "The given ParameterDescription is not valid!");
	for (TDelList::iterator it = iterToDelete.begin(); it != iterToDelete.end(); ++it)
	{
		it->first->second.pObj->Release();
		if (it->second.first)
		{
			TVarMapLookup& map = m_variableObjectsLookup[it->second.second];
			const bool ok = stl::member_find_and_erase(map, it->second.first);
			CRY_ASSERT_MESSAGE(ok, "no lookup for this object!");
		}
		m_variableObjects.erase(it->first);
	}
}

const SUIParameterDesc* CFlashUIElement::GetDescForInfo(SFlashObjectInfo* pInfo, const SUIParameterDesc** pParent) const
{
	for (TTmplMapLookup::const_iterator it = m_variableObjectsLookup.begin(); it != m_variableObjectsLookup.end(); ++it)
	{
		for (TVarMapLookup::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2)
			if (it2->second == pInfo)
			{
				if (pParent) *pParent = it->first;
				return it2->first;
			}
	}
	return NULL;
}

//------------------------------------------------------------------------------------
void CFlashUIElement::FreeVarObjects()
{
	for (TVarMap::iterator it = m_variableObjects.begin(); it != m_variableObjects.end(); ++it)
	{
		SAFE_RELEASE(it->second.pObj);
	}
	m_variableObjects.clear();
	m_variableObjectsLookup.clear();
}

//------------------------------------------------------------------------------------
void CFlashUIElement::AddEventListener(IUIElementEventListener* pListener, const char* name)
{
	const bool ok = m_eventListener.Add(pListener, name);
	CRY_ASSERT_MESSAGE(ok, "Listener already registered!");
}

//------------------------------------------------------------------------------------
void CFlashUIElement::RemoveEventListener(IUIElementEventListener* pListener)
{
	// Since flow nodes will unregister at all living instances to make sure they only re-register at elements of interest
	// this check is disabled
	// assert( m_eventListener.Contains(pListener) );
	m_eventListener.Remove(pListener);
}

//------------------------------------------------------------------------------------
void CFlashUIElement::UpdateViewPort()
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (!CFlashUI::CV_gfx_uiaction_enable)
		return;

	if (!m_pFlashplayer || HasExtTexture() || m_constraints.eType == SUIConstraints::ePT_FixedDynTexSize)
		return;

	int posX, posY, width, height, dx, dy;

	if (!gEnv->IsEditor())
	{
		dx = gEnv->pRenderer->GetOverlayWidth();
		dy = gEnv->pRenderer->GetOverlayHeight();
	}
	else
	{
		m_pFlashUI->GetScreenSize(dx, dy);
	}

	if (m_constraints.eType == SUIConstraints::ePT_Fixed)
	{
		posX = m_constraints.iLeft;
		posY = m_constraints.iTop;
		width = m_constraints.iWidth;
		height = m_constraints.iHeight;
		if (m_constraints.bScale)
		{
			m_pFlashplayer->SetViewScaleMode(IFlashPlayer::eSM_ExactFit);
		}
	}
	else if (m_constraints.eType == SUIConstraints::ePT_Dynamic)
	{
		if (m_constraints.bScale)
		{
			float screenAspectRatio = (float) dx / dy;
			float hudAspectRatio = (float) m_pFlashplayer->GetWidth() / m_pFlashplayer->GetHeight();

			const bool fitToHeight = m_constraints.bMax ? screenAspectRatio<hudAspectRatio : screenAspectRatio> hudAspectRatio;

			width = fitToHeight ? (int) (dy * hudAspectRatio) : dx;
			height = !fitToHeight ? (int) (dx / hudAspectRatio) : dy;
			;

			switch (m_constraints.eHAlign)
			{
			default:
			case SUIConstraints::ePA_Lower:
				posX = 0;
				break;
			case SUIConstraints::ePA_Mid:
				posX = fitToHeight ? (int) ((dx - dy * hudAspectRatio) / 2.f) : 0;
				break;
			case SUIConstraints::ePA_Upper:
				posX = fitToHeight ? (int) (dx - dy * hudAspectRatio) : 0;
				break;
			}
			switch (m_constraints.eVAlign)
			{
			default:
			case SUIConstraints::ePA_Lower:
				posY = 0;
				break;
			case SUIConstraints::ePA_Mid:
				posY = !fitToHeight ? (int) ((dy - dx / hudAspectRatio) / 2.f) : 0;
				break;
			case SUIConstraints::ePA_Upper:
				posY = !fitToHeight ? (int) (dy - dx / hudAspectRatio) : 0;
				break;
			}

		}
		else
		{
			width = m_pFlashplayer->GetWidth();
			height = m_pFlashplayer->GetHeight();

			switch (m_constraints.eHAlign)
			{
			default:
			case SUIConstraints::ePA_Lower:
				posX = 0;
				break;
			case SUIConstraints::ePA_Mid:
				posX = (dx - width) / 2;
				break;
			case SUIConstraints::ePA_Upper:
				posX = dx - width;
				break;
			}
			switch (m_constraints.eVAlign)
			{
			default:
			case SUIConstraints::ePA_Lower:
				posY = 0;
				break;
			case SUIConstraints::ePA_Mid:
				posY = (dy - height) / 2;
				break;
			case SUIConstraints::ePA_Upper:
				posY = dy - height;
				break;
			}

		}
	}
	else /*if ( m_constraints.eType == SUIConstraints::ePT_Fullscreen )*/
	{
		posX = 0;
		posY = 0;
		width = dx;
		height = dy;

		if (m_constraints.bScale)
		{
			m_pFlashplayer->SetViewScaleMode(IFlashPlayer::eSM_ExactFit);
		}
		m_pFlashplayer->SetViewAlignment(IFlashPlayer::eAT_TopLeft);
	}

	// set viewport
	m_pFlashplayer->SetViewport(posX, posY, width, height, 1.f);
	if (m_pFlashplayer->IsAvailable("cry_onResize"))
	{
		SFlashVarValue args[] = { SFlashVarValue(width), SFlashVarValue(height), SFlashVarValue(dx), SFlashVarValue(dy) };
		m_pFlashplayer->Invoke("cry_onResize", args, 4);
		m_pFlashplayer->Advance(0);
	}
	ForceLazyUpdateInl();
	UIACTION_LOG("%s (%i): Changed Viewport: %d, %d, %d, %d", GetName(), m_iInstanceID, posX, posY, width, height);
}

//------------------------------------------------------------------------------------
void CFlashUIElement::GetViewPort(int& x, int& y, int& width, int& height, float& aspectRatio)
{
	if (m_pFlashplayer)
	{
		m_pFlashplayer->GetViewport(x, y, width, height, aspectRatio);
	}
}

//------------------------------------------------------------------------------------

CUIElementIterator::CUIElementIterator(const CFlashUIElement* pElement)
	: m_pElement(pElement)
{
	m_iRefs = 1;
	m_currIter = pElement->m_instances.begin();
}

//------------------------------------------------------------------------------------
void CUIElementIterator::AddRef()
{
	m_iRefs++;
}

//------------------------------------------------------------------------------------
void CUIElementIterator::Release()
{
	if (--m_iRefs == 0)
		delete this;
}

//------------------------------------------------------------------------------------
IUIElement* CUIElementIterator::Next()
{
	IUIElement* pNext = NULL;
	if (m_currIter != m_pElement->m_instances.end())
	{
		pNext = *m_currIter;
		++m_currIter;
	}
	return pNext;
}

//------------------------------------------------------------------------------------
#undef SHOW_CURSOR
#undef HIDE_CURSOR
