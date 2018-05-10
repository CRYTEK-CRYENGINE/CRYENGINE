// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <NodeGraph/AbstractNodeGraphViewModel.h>

#include "ParticleNodeTreeModel.h"

#include <CryExtension/CryGUID.h>
#include <CryParticleSystem/IParticlesPfx2.h>

namespace CryParticleEditor {

class CNodeItem;
class CBasePinItem;
class CConnectionItem;
class CFeatureItem;

class CParticleGraphModel;

class CParticleGraphRuntimeContext : public CryGraphEditor::INodeGraphRuntimeContext
{
public:
	CParticleGraphRuntimeContext();
	~CParticleGraphRuntimeContext();

	// CryGraphEditor::INodeGraphRuntimeContext
	virtual const char*                                GetTypeName() const override           { return "ParticleFX2"; }
	virtual CAbstractDictionary*                       GetAvailableNodesDictionary() override { return &m_nodeTreeModel; }
	virtual const CryGraphEditor::CNodeGraphViewStyle* GetStyle() const                       { return m_pStyle; }
	// ~CryGraphEditor::INodeGraphRuntimeContext

private:
	CNodesDictionary                     m_nodeTreeModel;
	CryGraphEditor::CNodeGraphViewStyle* m_pStyle;
};

class CParticleGraphModel : public CryGraphEditor::CNodeGraphViewModel
{
	Q_OBJECT

public:
	CParticleGraphModel(pfx2::IParticleEffectPfx2& effect);
	virtual ~CParticleGraphModel();

	// NodeGraph::CNodeGraphViewModel
	virtual CryGraphEditor::INodeGraphRuntimeContext& GetRuntimeContext() { return m_runtimeContext; }
	virtual QString                                   GetGraphName() override;

	virtual uint32                                    GetNodeItemCount() const override;
	virtual CryGraphEditor::CAbstractNodeItem*        GetNodeItemByIndex(uint32 index) const override;
	virtual CryGraphEditor::CAbstractNodeItem*        GetNodeItemById(QVariant id) const override;
	virtual CryGraphEditor::CAbstractNodeItem*        CreateNode(QVariant identifier, const QPointF& position = QPointF()) override;
	virtual bool                                      RemoveNode(CryGraphEditor::CAbstractNodeItem& node) override;

	virtual uint32                                    GetConnectionItemCount() const override;
	virtual CryGraphEditor::CAbstractConnectionItem*  GetConnectionItemByIndex(uint32 index) const override;
	virtual CryGraphEditor::CAbstractConnectionItem*  GetConnectionItemById(QVariant id) const override;
	virtual CryGraphEditor::CAbstractConnectionItem*  CreateConnection(CryGraphEditor::CAbstractPinItem& sourcePin, CryGraphEditor::CAbstractPinItem& targetPin) override;
	virtual bool                                      RemoveConnection(CryGraphEditor::CAbstractConnectionItem& connection) override;

	virtual CryGraphEditor::CItemCollection*          CreateClipboardItemsCollection() override;
	// ~NodeGraph::CNodeGraphViewModel

	CNodeItem*                       CreateNode(const string& data, const QPointF& position);
	CNodeItem*                       CreateNode(const char* szTemplateName, const QPointF& position);
	CConnectionItem*                 CreateConnection(CBasePinItem& sourcePin, CBasePinItem& targetPin);

	const pfx2::IParticleEffectPfx2& GetEffectInterface() const { return m_effect; }

	CNodeItem*                       GetNodeItemById(string identifier) const;

	void                             ToggleSoloNode(CNodeItem& node);
	CNodeItem*                       GetSoloNode() const { return m_pSolorNode; }

protected:
	void       ExtractConnectionsFromNodes();

	CNodeItem* CreateNodeItem(pfx2::IParticleComponent& component);

private:
	std::vector<CNodeItem*>       m_nodes;
	std::vector<CConnectionItem*> m_connections;

	pfx2::IParticleEffectPfx2&    m_effect;

	bool                          m_isValid;
	CNodeItem*                    m_pSolorNode;
	CParticleGraphRuntimeContext  m_runtimeContext; // XXX Share between all models.
};

}

