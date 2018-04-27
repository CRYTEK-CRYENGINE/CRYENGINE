// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlashUIBaseNode.h
//  Version:     v1.00
//  Created:     10/9/2010 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#pragma once

#include "FlashUI.h"
#include <CryFlowGraph/IFlowBaseNode.h>

#ifndef RELEASE
	#define ENABLE_UISTACK_DEBUGGING
#endif

// ---------------------------------------------------------------
// ---------------------- ui base nodes ---------------------------
// ---------------------------------------------------------------
class CFlashUIBaseNode : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlashUIBaseNode();
	virtual ~CFlashUIBaseNode() {};

	virtual void            GetMemoryUsage(ICrySizer* s) const { s->Add(*this); }
	virtual IFlowNodePtr    Clone(SActivationInfo* pActInfo) = 0;

	static SInputPortConfig CreateInstanceIdPort();
	static SInputPortConfig CreateElementsPort();
	static SInputPortConfig CreateVariablesPort();
	static SInputPortConfig CreateArraysPort();
	static SInputPortConfig CreateMovieClipsPort();
	static SInputPortConfig CreateMovieClipsParentPort();
	static SInputPortConfig CreateMovieClipTmplPort();
	static SInputPortConfig CreateVariablesForTmplPort();
	static SInputPortConfig CreateArraysForTmplPort();
	static SInputPortConfig CreateMovieClipsForTmplPort();
	static SInputPortConfig CreateTmplInstanceNamePort();

	void                    UpdateUIElement(const string& sName, SActivationInfo* pActInfo);

	inline IUIElement*      GetElement() const { return m_pElement; }
	inline IUIElement*      GetInstance(int instanceID);

private:
	IUIElement* m_pElement;
};

// ---------------------------------------------------------------
template<EUIObjectType Type>
class CFlashUIBaseDescNode : public CFlashUIBaseNode
{
public:
	CFlashUIBaseDescNode() : m_pObjectDesc(NULL), m_pTmplDesc(NULL) {}

	virtual void         GetConfiguration(SFlowNodeConfig&)               { assert(false); };
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo*) { assert(false); };
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)                 { assert(false); return new CFlashUIBaseDescNode(); }

	void                 UpdateObjectDesc(const string& sInputStr, SActivationInfo* pActInfo, bool isTemplate)
	{
		m_pObjectDesc = NULL;

		SUIArguments path;
		path.SetDelimiter(":");
		path.SetArguments(sInputStr.c_str());

		if (path.GetArgCount() > 1)
		{
			string sUIElement;
			path.GetArg(0, sUIElement);
			UpdateUIElement(sUIElement, pActInfo);

			if (GetElement())
			{
				if (isTemplate)
					m_pObjectDesc = UpdateObjectDescInt<eUOT_MovieClipTmpl>(GetElement(), path, 1);
				else
					m_pObjectDesc = UpdateObjectDescInt<eUOT_MovieClip>(GetElement(), path, 1);
				if (!m_pObjectDesc && !isTemplate)
				{
					UIACTION_WARNING("FG: UIElement \"%s\" does not have %s \"%s\", referenced at node \"%s\"", GetElement()->GetName(), SUIGetTypeStr<Type>::GetTypeName(), sInputStr.c_str(), pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
				}
				return;
			}
			UIACTION_WARNING("FG: Path To object \"%s\" does not exist, referenced at node \"%s\"", sInputStr.c_str(), pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
			return;
		}
		UIACTION_WARNING("FG: Invalid object path \"%s\", referenced at node \"%s\"", sInputStr.c_str(), pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
	}

	bool UpdateTmplDesc(const string& sInputStr, SActivationInfo* pActInfo)
	{
		m_pTmplDesc = NULL;
		if (GetElement())
		{
			m_pTmplDesc = GetElement()->GetMovieClipDesc(sInputStr.c_str());
			if (!m_pTmplDesc)
			{
				UIACTION_WARNING("FG: UIElement \"%s\" does not have template \"%s\", referenced at node \"%s\"", GetElement()->GetName(), sInputStr.c_str(), pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
				return false;
			}
			return true;
		}
		UIACTION_WARNING("FG: Invalid UIElement, referenced at node \"%s\"", pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
		return false;
	}

	void Reset()
	{
		m_pObjectDesc = NULL;
		m_pTmplDesc = NULL;
	}

	inline const typename SUIDescTypeOf<Type>::TType * GetObjectDesc() const { return m_pObjectDesc ? m_pObjectDesc : m_pTmplDesc; }
	inline const SUIMovieClipDesc* GetTmplDesc(bool bNoObjectDescNeeded = false) const { return m_pObjectDesc || bNoObjectDescNeeded ? m_pTmplDesc : NULL; }

private:
	template<EUIObjectType ParentType, class Item>
	inline const typename SUIDescTypeOf<Type>::TType * UpdateObjectDescInt(Item * parent, const SUIArguments &path, int depth)
	{
		if (!parent)
			return NULL;

		string name;
		path.GetArg(depth, name);
		if (depth < path.GetArgCount() - 1)
			return UpdateObjectDescInt<eUOT_MovieClip>(SUIGetDesc<ParentType, Item, const char*>::GetDesc(parent, name.c_str()), path, depth + 1);
		return SUIGetDesc<Type, Item, const char*>::GetDesc(parent, name.c_str());
	}

private:
	const typename SUIDescTypeOf<Type>::TType * m_pObjectDesc;
	const SUIMovieClipDesc* m_pTmplDesc;
};

// ---------------------------------------------------------------
class CFlashUIBaseNodeCategory : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlashUIBaseNodeCategory(string sCategory) : m_category(sCategory) {}

	const char*          GetCategory() const                { return m_category.c_str(); }
	virtual void         GetMemoryUsage(ICrySizer* s) const { s->Add(*this); }
	virtual void         GetConfiguration(SFlowNodeConfig& config) = 0;
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) = 0;
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) = 0;

private:
	string m_category;
};

// ---------------------------------------------------------------
class CFlashUIBaseNodeDynPorts : public CFlashUIBaseNodeCategory
{
public:
	CFlashUIBaseNodeDynPorts(string sCategory) : CFlashUIBaseNodeCategory(sCategory) {}

protected:
	void AddParamInputPorts(const SUIEventDesc::SEvtParams& eventDesc, std::vector<SInputPortConfig>& ports);
	void AddParamOutputPorts(const SUIEventDesc::SEvtParams& eventDesc, std::vector<SOutputPortConfig>& ports);
	void AddCheckPorts(const SUIEventDesc::SEvtParams& eventDesc, std::vector<SInputPortConfig>& ports);

	void GetDynInput(SUIArguments& args, const SUIParameterDesc& desc, SActivationInfo* pActInfo, int port);
	void ActivateDynOutput(const TUIData& arg, const SUIParameterDesc& desc, SActivationInfo* pActInfo, int port);

private:
	string m_enumStr;
};

// ---------------------------------------------------------------
class CFlashUIBaseElementNode : public CFlashUIBaseNodeDynPorts
{
public:
	CFlashUIBaseElementNode(IUIElement* pUIElement, string sCategory) : CFlashUIBaseNodeDynPorts(sCategory), m_pElement(pUIElement) { assert(pUIElement); }
	virtual ~CFlashUIBaseElementNode() {};

	virtual void GetConfiguration(SFlowNodeConfig& config) = 0;
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) = 0;

protected:
	IUIElement* m_pElement;
};

// FlowNode factory class for Flash UI nodes
class CFlashUiFlowNodeFactory : public IFlowNodeFactory
{
public:
	CFlashUiFlowNodeFactory(const char* sClassName, IFlowNodePtr pFlowNode) : m_sClassName(sClassName), m_pFlowNode(pFlowNode) {}
	IFlowNodePtr Create(IFlowNode::SActivationInfo* pActInfo) { return m_pFlowNode->Clone(pActInfo); }
	void         GetMemoryUsage(ICrySizer* s) const           { SIZER_SUBCOMPONENT_NAME(s, "CFlashUiFlowNodeFactory"); }
	void         Reset()                                      {}

	const char*  GetNodeTypeName() const { return m_sClassName; }

private:
	const char*  m_sClassName;
	IFlowNodePtr m_pFlowNode;
};

TYPEDEF_AUTOPTR(CFlashUiFlowNodeFactory);
typedef CFlashUiFlowNodeFactory_AutoPtr CFlashUiFlowNodeFactoryPtr;

// ---------------------------------------------------------------
// ------------ UI Stack for UI Action nodes ---------------------
// ---------------------------------------------------------------
#ifdef ENABLE_UISTACK_DEBUGGING
	#define UI_STACK_PUSH(stack, val, fmt, ...) stack.push(val, fmt, __VA_ARGS__);
#else
	#define UI_STACK_PUSH(stack, val, fmt, ...) stack.push(val);
#endif

class CUIFGStackMan
{
public:
	static int  GetTotalStackSize() { return m_Size; }
#ifdef ENABLE_UISTACK_DEBUGGING
	static void Add(int count, const char* dgbInfo, int id)
	{
		m_Size += count;
		for (int i = 0; i < count; ++i)
			m_debugInfo[id] = dgbInfo;
	}
	static void Remove(int count, int id)
	{
		m_Size -= count;
		for (int i = 0; i < count; ++i)
		{
			const bool ok = stl::member_find_and_erase(m_debugInfo, id);
			assert(ok);
		}
		assert(m_Size >= 0);
	}
	static const std::map<int, const char*>& GetStack()                { return m_debugInfo; }
	static int                               GetNextId()               { static int id = 0; return id++; }
#else
	static void                              Add(int count = 1)        { m_Size += count; }
	static void                              Remove(int count = 1)     { m_Size -= count; assert(m_Size >= 0); }
#endif
	static bool                              IsEnabled()               { return m_bEnabled; }
	static void                              SetEnabled(bool bEnabled) { m_bEnabled = bEnabled; }

private:
	static int                        m_Size;
	static bool                       m_bEnabled;
#ifdef ENABLE_UISTACK_DEBUGGING
	static std::map<int, const char*> m_debugInfo;
#endif
};

// ---------------------------------------------------------------
template<class T>
class CUIStack
{
public:
	CUIStack()
		: m_isUIAction(-1)
		, m_pGraph(nullptr)
		, m_pAction(nullptr)
	{}

	inline void init(IFlowGraph* pGraph) { InitInt(pGraph); }
#ifdef ENABLE_UISTACK_DEBUGGING
	inline void push(const T& val, const char* fmt, ...)
	{
		if (gEnv->IsEditor() && !CUIFGStackMan::IsEnabled())
			return;
		if (!m_pGraph || !m_pGraph->IsEnabled())
			return;

		va_list args;
		va_start(args, fmt);
		char tmp[1024];
		cry_vsprintf(tmp, fmt, args);
		va_end(args);

		m_Impl.push_front(val);
		if (IsUIAction())
		{
			int id = CUIFGStackMan::GetNextId();
			m_debugInfo.push_back(std::make_pair(tmp, id));
			CUIFGStackMan::Add(1, m_debugInfo.rbegin()->first.c_str(), id);
		}
		CRY_ASSERT_MESSAGE(size() < 256, "Too many items on stack!");
	}

	inline void pop()
	{
		m_Impl.pop_back();
		if (IsUIAction())
		{
			CUIFGStackMan::Remove(1, m_debugInfo.rbegin()->second);
			m_debugInfo.pop_back();
		}
	}

	inline void clear()
	{
		while (!m_Impl.empty())
			pop();
	}

#else
	inline void push(const T& val)
	{
		if (gEnv->IsEditor() && !CUIFGStackMan::IsEnabled())
			return;
		if (!m_pGraph || !m_pGraph->IsEnabled())
			return;
		m_Impl.push_front(val);
		if (IsUIAction())
			CUIFGStackMan::Add();
		CRY_ASSERT_MESSAGE(size() < 256, "Too many items on stack!");
	}

	inline void pop()   { m_Impl.pop_back(); if (IsUIAction()) CUIFGStackMan::Remove(); }
	inline void clear() { if (!m_Impl.empty() && IsUIAction()) CUIFGStackMan::Remove(size()); m_Impl.clear(); }
#endif

	inline int      size() const { return m_Impl.size(); }
	inline const T& get() const  { return *m_Impl.rbegin(); }

private:
	inline void InitInt(IFlowGraph* pGraph)
	{
		m_pGraph = pGraph;
		int i = 0;
		while (const IUIAction* pAction = gEnv->pFlashUI->GetUIAction(i++))
		{
			if (pAction->GetType() == IUIAction::eUIAT_FlowGraph && pAction->GetFlowGraph().get() == pGraph)
			{
				m_pAction = pAction;
				m_isUIAction = 1;
				return;
			}
		}
		m_isUIAction = 0;
	}

	inline bool IsUIAction() const
	{
		CRY_ASSERT_MESSAGE(m_isUIAction != -1, "Stack was not intialized");
		return m_isUIAction == 1 && m_pAction->IsEnabled();
	};

private:
	std::deque<T>                     m_Impl;
#ifdef ENABLE_UISTACK_DEBUGGING
	std::list<std::pair<string, int>> m_debugInfo;
#endif
	int                               m_isUIAction;
	IFlowGraph*                       m_pGraph;
	const IUIAction*                  m_pAction;
};
