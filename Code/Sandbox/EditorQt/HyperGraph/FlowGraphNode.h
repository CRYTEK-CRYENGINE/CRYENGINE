// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryEntitySystem/IEntitySystem.h>
#include <CryFlowGraph/IFlowSystem.h>

#include "HyperGraphNode.h"

class CFlowNode;
class CEntityObject;

//////////////////////////////////////////////////////////////////////////
class CFlowNode : public CHyperNode
{
	friend class CFlowGraphManager;

public:
	CFlowNode();
	virtual ~CFlowNode();

	//////////////////////////////////////////////////////////////////////////
	// CHyperNode implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void           Init() override;
	virtual void           Done() override;
	virtual CHyperNode*    Clone() override;
	virtual const char*    GetDescription() const override;
	virtual void           Serialize(XmlNodeRef& node, bool bLoading, CObjectArchive* ar) override;
	virtual void           SetName(const char* sName) override;
	virtual void           OnInputsChanged() override;
	virtual void           OnEnteringGameMode() override;
	virtual void           Unlinked(bool bInput) override;
	virtual CString        GetPortName(const CHyperNodePort& port) override;
	virtual void           PostClone(CBaseObject* pFromObject, CObjectCloneContext& ctx) override;
	virtual Gdiplus::Color GetCategoryColor() const override;
	virtual void           DebugPortActivation(TFlowPortId port, const char* value, bool bIsInitializationStep) override;
	virtual bool           IsPortActivationModified(const CHyperNodePort* port = nullptr) const override;
	virtual void           ClearDebugPortActivation() override;
	virtual CString        GetDebugPortValue(const CHyperNodePort& pp) override;
	virtual void           ResetDebugPortActivation(CHyperNodePort* port) override;
	virtual bool           GetAdditionalDebugPortInformation(const CHyperNodePort& pp, bool& bOutIsInitialization) override;
	virtual bool           IsDebugPortActivated(CHyperNodePort* port) const override;
	virtual bool           IsObsolete() const override { return m_flowSystemNodeFlags & EFLN_OBSOLETE; }
	virtual TFlowNodeId    GetTypeId() const override;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// FlowSystem Flags. We don't want to replicate those again....
	//////////////////////////////////////////////////////////////////////////
	uint32         GetCoreFlags() const  { return m_flowSystemNodeFlags & EFLN_CORE_MASK; }
	uint32         GetCategory() const   { return m_flowSystemNodeFlags & EFLN_CATEGORY_MASK; }
	uint32         GetUsageFlags() const { return m_flowSystemNodeFlags & EFLN_USAGE_MASK; }
	const char*    GetCategoryName() const;
	const char*    GetUIClassName() const;

	void           SetFromNodeId(TFlowNodeId flowNodeId);

	void           SetEntity(CEntityObject* pEntity);
	CEntityObject* GetEntity() const;

	// Takes selected entity as target entity.
	void           SetSelectedEntity();
	// Takes graph default entity as target entity.
	void           SetDefaultEntity();
	CEntityObject* GetDefaultEntity() const;

	// Returns IFlowNode.
	IFlowGraph* GetIFlowGraph() const;

	// Return ID of the flow node.
	TFlowNodeId          GetFlowNodeId() const { return m_flowNodeId; }

	void                 SetInputs(bool bActivate, bool bForceResetEntities = false);
	virtual CVarBlock*   GetInputsVarBlock();

	virtual CString      GetTitle() const;

	virtual IUndoObject* CreateUndo();
protected:
	virtual bool         IsEntityValid() const;
	virtual CString      GetEntityTitle() const;

protected:
	CryGUID                       m_entityGuid;
	_smart_ptr<CEntityObject>     m_pEntity;
	uint32                        m_flowSystemNodeFlags;
	TFlowNodeId                   m_flowNodeId;
	const char*                   m_szUIClassName;
	const char*                   m_szDescription;
	std::map<TFlowPortId, string> m_portActivationMap;
	std::map<TFlowPortId, bool>   m_portActivationAdditionalDebugInformationMap;
	std::vector<TFlowPortId>      m_debugPortActivations;
};

