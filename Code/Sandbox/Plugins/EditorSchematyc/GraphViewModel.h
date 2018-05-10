// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <NodeGraph/AbstractNodeGraphViewModel.h>

#include "NodeGraphRuntimeContext.h"

namespace Schematyc {

struct IScriptGraph;

}

namespace CrySchematycEditor {

class CNodeGraphRuntimeContext;
class CNodeItem;
class CConnectionItem;

class CNodeGraphViewModel : public CryGraphEditor::CNodeGraphViewModel
{
	typedef std::vector<CNodeItem*>                          NodesByIndex;
	typedef std::unordered_map<CryGUID, CNodeItem*> NodesByGuid;
	typedef std::vector<CConnectionItem*>                    ConnectionsByIndex;

public:
	CNodeGraphViewModel(Schematyc::IScriptGraph& scriptGraph /*, CNodeGraphRuntimeContext& context*/);
	virtual ~CNodeGraphViewModel();

	virtual CryGraphEditor::INodeGraphRuntimeContext& GetRuntimeContext() override { return m_runtimeContext; }
	virtual QString                                   GetGraphName() override      { return QString(); }

	virtual uint32                                    GetNodeItemCount() const override;
	virtual CryGraphEditor::CAbstractNodeItem*        GetNodeItemByIndex(uint32 index) const override;
	virtual CryGraphEditor::CAbstractNodeItem*        GetNodeItemById(QVariant id) const override;
	virtual CryGraphEditor::CAbstractNodeItem*        CreateNode(QVariant typeId, const QPointF& position = QPointF()) override;
	virtual bool                                      RemoveNode(CryGraphEditor::CAbstractNodeItem& node) override;

	virtual uint32                                    GetConnectionItemCount() const override;
	virtual CryGraphEditor::CAbstractConnectionItem*  GetConnectionItemByIndex(uint32 index) const override;
	virtual CryGraphEditor::CAbstractConnectionItem*  GetConnectionItemById(QVariant id) const override;
	virtual CryGraphEditor::CAbstractConnectionItem*  CreateConnection(CryGraphEditor::CAbstractPinItem& sourcePin, CryGraphEditor::CAbstractPinItem& targetPin) override;
	virtual bool                                      RemoveConnection(CryGraphEditor::CAbstractConnectionItem& connection) override;

	virtual CryGraphEditor::CItemCollection*          CreateClipboardItemsCollection() override;

	Schematyc::IScriptGraph&                          GetScriptGraph() const { return m_scriptGraph; }

	virtual CNodeItem*                                CreateNode(CryGUID typeGuid);
	void                                              Refresh();

private:
	Schematyc::IScriptGraph& m_scriptGraph;
	CNodeGraphRuntimeContext m_runtimeContext;

	NodesByIndex             m_nodesByIndex;
	NodesByGuid              m_nodesByGuid;
	ConnectionsByIndex       m_connectionsByIndex;
};

}

