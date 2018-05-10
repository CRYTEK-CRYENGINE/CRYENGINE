// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlashUIElementNodes.h
//  Version:     v1.00
//  Created:     10/9/2010 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __FlashUIElementNodes_H__
#define __FlashUIElementNodes_H__

#include "FlashUIBaseNode.h"

////////////////////////////////////////////////////////////////////////////
#define IMPL_BASE_NODE(name, base, istmpl)                                                \
  class name : public base                                                                \
  {                                                                                       \
  public:                                                                                 \
    name(SActivationInfo * pActInfo) : base(pActInfo, istmpl) {}                          \
    virtual IFlowNodePtr Clone(SActivationInfo * pActInfo) { return new name(pActInfo); } \
  };
#define IMPL_DEFAULT_NODE(name, base)  IMPL_BASE_NODE(name, base, false)
#define IMPL_TEMPLATE_NODE(name, base) IMPL_BASE_NODE(name, base, true)

////////////////////////////////////////////////////////////////////////////
class CFlashUIVariableBaseNode : public CFlashUIBaseDescNode<eUOT_Variable>
{
public:
	CFlashUIVariableBaseNode(SActivationInfo* pActInfo, bool isTemplate) : m_isTemplate(isTemplate) {}
	virtual ~CFlashUIVariableBaseNode() {}

	virtual void GetConfiguration(SFlowNodeConfig& config);
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);

private:
	void SetVariable(IUIElement* pInstance, const TUIData& value) { pInstance->SetVariable(GetObjectDesc(), value, GetTmplDesc()); }
	void GetVariable(IUIElement* pInstance, TUIData& value)       { pInstance->GetVariable(GetObjectDesc(), value, GetTmplDesc()); }

	void ConvertToUIData(const TFlowInputData& in, TUIData& out, SActivationInfo* pActInfo);

private:
	enum EInputs
	{
		eI_UIVariable = 0,
		eI_InstanceID,
		eI_TemplateInstanceName,
		eI_Set,
		eI_Get,
		eI_Value,
	};

	enum EOutputs
	{
		eO_OnSet = 0,
		eO_Value,
	};

	inline bool IsTemplate() const { return m_isTemplate; }
	inline int  GetInputPort(EInputs input)
	{
		if (input <= eI_TemplateInstanceName)
			return (int) input;
		return m_isTemplate ? (int) input : (int) input - 1;
	}
	bool m_isTemplate;
};
////////////////////////////////////////////////////////////////////////////
IMPL_DEFAULT_NODE(CFlashUIVariableNode, CFlashUIVariableBaseNode);
IMPL_TEMPLATE_NODE(CFlashUITmplVariableNode, CFlashUIVariableBaseNode);

////////////////////////////////////////////////////////////////////////////
class CFlashUIArrayBaseNode : public CFlashUIBaseDescNode<eUOT_Array>
{
public:
	CFlashUIArrayBaseNode(SActivationInfo* pActInfo, bool isTemplate) : m_isTemplate(isTemplate) {}
	virtual ~CFlashUIArrayBaseNode() {}

	virtual void GetConfiguration(SFlowNodeConfig& config);
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);

private:
	void SetArray(IUIElement* pInstance, const SUIArguments& array) { pInstance->SetArray(GetObjectDesc(), array, GetTmplDesc()); }
	void GetArray(IUIElement* pInstance, SUIArguments& array)       { pInstance->GetArray(GetObjectDesc(), array, GetTmplDesc()); }

private:
	enum EInputs
	{
		eI_UIArray = 0,
		eI_InstanceID,
		eI_TemplateInstanceName,
		eI_Set,
		eI_Get,
		eI_Value,
	};

	enum EOutputs
	{
		eO_OnSet = 0,
		eO_Value,
	};

	inline bool IsTemplate() const { return m_isTemplate; }
	inline int  GetInputPort(EInputs input)
	{
		if (input <= eI_TemplateInstanceName)
			return (int) input;
		return m_isTemplate ? (int) input : (int) input - 1;
	}
	bool m_isTemplate;
};

////////////////////////////////////////////////////////////////////////////
IMPL_DEFAULT_NODE(CFlashUIArrayNode, CFlashUIArrayBaseNode);
IMPL_TEMPLATE_NODE(CFlashUITmplArrayNode, CFlashUIArrayBaseNode);

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
class CFlashUIGotoAndPlayBaseNode : public CFlashUIBaseDescNode<eUOT_MovieClip>
{
public:
	CFlashUIGotoAndPlayBaseNode(SActivationInfo* pActInfo, bool isTemplate) : m_isTemplate(isTemplate) {}
	virtual ~CFlashUIGotoAndPlayBaseNode() {}

	virtual void GetConfiguration(SFlowNodeConfig& config);
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);

private:
	void GotoAndPlay(IUIElement* pElement, int frameId, const char* frameName);
	void GotoAndStop(IUIElement* pElement, int frameId, const char* frameName);

private:
	enum EInputs
	{
		eI_UIMovieClip = 0,
		eI_InstanceID,
		eI_TemplateInstanceName,
		eI_GotoAndPlay,
		eI_GotoAndStop,
		eI_FrameNum,
		eI_FrameName,
	};

	enum EOutputs
	{
		eO_OnGotoAndPlay = 0,
		eO_OnGotoAndStop,
	};

	inline bool IsTemplate() const { return m_isTemplate; }
	inline int  GetInputPort(EInputs input)
	{
		if (input <= eI_TemplateInstanceName)
			return (int) input;
		return m_isTemplate ? (int) input : (int) input - 1;
	}
	bool m_isTemplate;
};
////////////////////////////////////////////////////////////////////////////
IMPL_DEFAULT_NODE(CFlashUIGotoAndPlayNode, CFlashUIGotoAndPlayBaseNode);
IMPL_TEMPLATE_NODE(CFlashUIGotoAndPlayTmplNode, CFlashUIGotoAndPlayBaseNode);

////////////////////////////////////////////////////////////////////////////
class CFlashUIMCVisibleBaseNode : public CFlashUIBaseDescNode<eUOT_MovieClip>
{
public:
	CFlashUIMCVisibleBaseNode(SActivationInfo* pActInfo, bool isTemplate) : m_isTemplate(isTemplate) {}
	virtual ~CFlashUIMCVisibleBaseNode() {}

	virtual void GetConfiguration(SFlowNodeConfig& config);
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);

private:
	void SetValues(IUIElement* pElement, bool bVisible, float alpha);
	void GetValues(IUIElement* pElement, bool& bVisible, float& alpha);

private:
	enum EInputs
	{
		eI_UIMovieClip = 0,
		eI_InstanceID,
		eI_TemplateInstanceName,
		eI_Set,
		eI_Get,
		eI_Visible,
		eI_Alpha,
	};

	enum EOutputs
	{
		eO_OnSet = 0,
		eO_Visible,
		eO_Alpha,
	};

	inline bool IsTemplate() const { return m_isTemplate; }
	inline int  GetInputPort(EInputs input)
	{
		if (input <= eI_TemplateInstanceName)
			return (int) input;
		return m_isTemplate ? (int) input : (int) input - 1;
	}
	bool m_isTemplate;
};
////////////////////////////////////////////////////////////////////////////
IMPL_DEFAULT_NODE(CFlashUIMCVisibleNode, CFlashUIMCVisibleBaseNode);
IMPL_TEMPLATE_NODE(CFlashUIMCVisibleTmplNode, CFlashUIMCVisibleBaseNode);

////////////////////////////////////////////////////////////////////////////
class CFlashUIMCPosRotScaleBaseNode : public CFlashUIBaseDescNode<eUOT_MovieClip>
{
public:
	CFlashUIMCPosRotScaleBaseNode(SActivationInfo* pActInfo, bool isTemplate) : m_isTemplate(isTemplate) {}
	virtual ~CFlashUIMCPosRotScaleBaseNode() {}

	virtual void GetConfiguration(SFlowNodeConfig& config);
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);

private:
	void SetValues(IUIElement* pElement, const Vec3& vPos, const Vec3& vRot, const Vec3& vScale);
	void GetValues(IUIElement* pElement, Vec3& vPos, Vec3& vRot, Vec3& vScale);

private:
	enum EInputs
	{
		eI_UIMovieClip = 0,
		eI_InstanceID,
		eI_TemplateInstanceName,
		eI_Set,
		eI_Get,
		eI_Pos,
		eI_Rot,
		eI_Scale,
	};

	enum EOutputs
	{
		eO_OnSet = 0,
		eO_Pos,
		eO_Rot,
		eO_Scale,
	};

	inline bool IsTemplate() const { return m_isTemplate; }
	inline int  GetInputPort(EInputs input)
	{
		if (input <= eI_TemplateInstanceName)
			return (int) input;
		return m_isTemplate ? (int) input : (int) input - 1;
	}
	bool m_isTemplate;
};
////////////////////////////////////////////////////////////////////////////
IMPL_DEFAULT_NODE(CFlashUIMCPosRotScaleNode, CFlashUIMCPosRotScaleBaseNode);
IMPL_TEMPLATE_NODE(CFlashUIMCPosRotScaleTmplNode, CFlashUIMCPosRotScaleBaseNode);

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
class CFlashUIMCTemplateCreateNode : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlashUIMCTemplateCreateNode(SActivationInfo* pActInfo) {}
	virtual ~CFlashUIMCTemplateCreateNode() {}

	virtual void         GetMemoryUsage(ICrySizer* s) const { s->Add(*this); }
	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlashUIMCTemplateCreateNode(pActInfo); }

private:
	void CreateMoviclip(IUIElement* pElement, const SUIMovieClipDesc* pTemplate, const SUIMovieClipDesc* pParent, string& newname);

private:
	enum EInputs
	{
		eI_UIMovieClipTmpl = 0,
		eI_UIParentMovieClip,
		eI_InstanceID,
		eI_NewInstanceName,
		eI_Create,
	};

	enum EOutputs
	{
		eO_OnCreate = 0,
		eO_InstanceName,
	};

	CFlashUIBaseDescNode<eUOT_MovieClipTmpl> m_TmplDescHelper;
	CFlashUIBaseDescNode<eUOT_MovieClip>     m_ParentDescHelper;

	inline IUIElement*             GetElement() const      { return m_TmplDescHelper.GetElement(); }
	inline const SUIMovieClipDesc* GetTemplateDesc() const { return m_TmplDescHelper.GetObjectDesc(); }
	inline const SUIMovieClipDesc* GetParentDesc() const   { return m_ParentDescHelper.GetObjectDesc(); }
};

////////////////////////////////////////////////////////////////////////////
class CFlashUIMCTemplateRemoveNode : public CFlashUIBaseDescNode<eUOT_MovieClipTmpl>
{
public:
	CFlashUIMCTemplateRemoveNode(SActivationInfo* pActInfo) {}
	virtual ~CFlashUIMCTemplateRemoveNode() {}

	virtual void GetConfiguration(SFlowNodeConfig& config);
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);

private:
	void RemoveMoviclip(IUIElement* pElement, const SUIMovieClipDesc* pMc) { pElement->RemoveMovieClip(pMc); }

private:
	enum EInputs
	{
		eI_UIElement = 0,
		eI_InstanceID,
		eI_TemplateInstanceName,
		eI_Remove,
	};

	enum EOutputs
	{
		eO_OnRemove = 0,
	};
};

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
class CFlashUIEventNode
	: public CFlashUIBaseElementNode
	  , public IUIElementEventListener
{
public:
	CFlashUIEventNode(IUIElement* pUIElement, string sCategory, const SUIEventDesc* pEventDesc);
	virtual ~CFlashUIEventNode();

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo);
	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);

	virtual void         OnUIEvent(IUIElement* pSender, const SUIEventDesc& event, const SUIArguments& args);
	virtual void         OnInstanceCreated(IUIElement* pSender, IUIElement* pNewInstance);

private:
	void RegisterAsListener(IUIElement* pInstance) { pInstance->AddEventListener(this, "CFlashUIEventNode"); }
	void ClearListener();
	void FlushNextEvent(SActivationInfo* pActInfo);

private:
	enum EInputs
	{
		eI_InstanceID = 0,
		eI_CheckPort,
		eI_CheckValue,
	};

	enum EOutputs
	{
		eO_OnEvent = 0,
		eO_OnInstanceId,
		eO_DynamicPorts,
	};

	std::vector<SOutputPortConfig> m_outPorts;
	std::vector<SInputPortConfig>  m_inPorts;
	SUIEventDesc                   m_eventDesc;

	int                            m_iCurrInstanceId;

	typedef CUIStack<std::pair<SUIArguments, int>> TEventStack;
	TEventStack m_events;
};

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
class CFlashUIFunctionNode
	: public CFlashUIBaseElementNode
{
public:
	CFlashUIFunctionNode(IUIElement* pUIElement, string sCategory, const SUIEventDesc* pFuncDesc, bool isTemplate);
	virtual ~CFlashUIFunctionNode();

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo);
	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);

private:
	void CallFunction(IUIElement* pInstance, const SUIArguments& args, TUIData& res) { pInstance->CallFunction(&m_funcDesc, args, &res, m_pTmplDesc); }

	bool UpdateTmplDesc(const string& sInputStr, SActivationInfo* pActInfo)
	{
		m_pTmplDesc = NULL;
		if (m_pElement)
		{
			m_pTmplDesc = m_pElement->GetMovieClipDesc(sInputStr.c_str());
			if (!m_pTmplDesc)
			{
				UIACTION_WARNING("FG: UIElement \"%s\" does not have template \"%s\", referenced at node \"%s\"", m_pElement->GetName(), sInputStr.c_str(), pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
			}
			return true;
		}
		UIACTION_WARNING("FG: Invalid UIElement, referenced at node \"%s\"", pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
		return false;
	}

private:
	enum EInputs
	{
		eI_Call = 0,
		eI_InstanceID,
		eI_TemplateInstanceName,
		eI_DynamicPorts,
	};

	enum EOutputs
	{
		eO_OnCall = 0,
		eO_RetVal,
	};

	inline bool IsTemplate() const { return m_isTemplate; }
	inline int  GetDynStartPort()  { return m_isTemplate ? (int) eI_DynamicPorts : (int) eI_DynamicPorts - 1; }

	bool                          m_isTemplate;
	SUIEventDesc                  m_funcDesc;
	std::vector<SInputPortConfig> m_inPorts;
	const SUIMovieClipDesc*       m_pTmplDesc;
};

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
class CFlashUIElementListenerNode
	: public CFlashUIBaseNode
	  , public IUIElementEventListener
{
public:
	CFlashUIElementListenerNode(SActivationInfo* pActInfo);
	virtual ~CFlashUIElementListenerNode();

	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlashUIElementListenerNode(pActInfo); }

	virtual void         OnInit(IUIElement* pSender, IFlashPlayer* pFlashPlayer);
	virtual void         OnUnload(IUIElement* pSender);
	virtual void         OnSetVisible(IUIElement* pSender, bool bVisible);
	virtual void         OnInstanceCreated(IUIElement* pSender, IUIElement* pNewInstance);

private:
	void RegisterAsListener(IUIElement* pInstance) { pInstance->AddEventListener(this, "CFlashUIElementListenerNode"); }
	void ClearListener();
	void FlushNextEvent(SActivationInfo* pActInfo);

private:
	enum EInputs
	{
		eI_UIElement = 0,
		eI_InstanceID,
	};
	enum EOutputs
	{
		eO_InstanceId = 0,
		eO_OnInit,
		eO_OnUnload,
		eO_OnShow,
		eO_OnHide,
	};

	typedef CUIStack<std::pair<EOutputs, int>> TEventStack;
	TEventStack m_events;

	int         m_iCurrInstanceId;
};

////////////////////////////////////////////////////////////////////////////
class CFlashUIElementInstanceNode
	: public CFlashUIBaseNode
	  , public IUIElementEventListener
{
public:
	CFlashUIElementInstanceNode(SActivationInfo* pActInfo) {}
	virtual ~CFlashUIElementInstanceNode();

	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlashUIElementInstanceNode(pActInfo); }

	virtual void         OnInstanceCreated(IUIElement* pSender, IUIElement* pNewInstance);
	virtual void         OnInstanceDestroyed(IUIElement* pSender, IUIElement* pDeletedInstance);

private:
	void DestroyInstance(IUIElement* pInstance) { pInstance->DestroyThis(); }
	void FlushNextEvent(SActivationInfo* pActInfo);

private:
	enum EInputs
	{
		eI_UIElement = 0,
		eI_InstanceID,
		eI_DestroyInstance,
	};
	enum EOutputs
	{
		eO_InstanceId = 0,
		eO_OnNewInstance,
		eO_OnInstanceDestroyed,
	};

	typedef CUIStack<std::pair<EOutputs, int>> TEventStack;
	TEventStack m_events;
};
////////////////////////////////////////////////////////////////////////////

#endif //#ifndef __FlashUIElementNodes_H__
