// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <NodeGraph/AbstractNodeItem.h>
#include <NodeGraph/NodeWidget.h>
#include <NodeGraph/NodeHeaderIconWidget.h>
#include <NodeGraph/NodeGraphViewStyle.h>

#include "GraphPinItem.h"
#include "NodeGraphRuntimeContext.h"

#include <unordered_map>

class QPixmap;
class QIcon;

namespace Schematyc {

struct IScriptGraphNode;

}

namespace CrySchematycEditor {

class CNodeItem : public CryGraphEditor::CAbstractNodeItem
{
public:
	CNodeItem(Schematyc::IScriptGraphNode& scriptNode, CryGraphEditor::CNodeGraphViewModel& model);
	virtual ~CNodeItem();

	// CryGraphEditor::CAbstractNodeItem
	virtual CryGraphEditor::CNodeWidget*        CreateWidget(CryGraphEditor::CNodeGraphView& view) override;
	virtual const char*                         GetStyleId() const override;
	virtual void                                Serialize(Serialization::IArchive& archive) override;

	virtual QPointF                             GetPosition() const override;
	virtual void                                SetPosition(QPointF position) override;

	virtual QVariant                            GetId() const override;
	virtual bool                                HasId(QVariant id) const override;

	virtual QVariant                            GetTypeId() const override;

	virtual const CryGraphEditor::PinItemArray& GetPinItems() const override    { return m_pins; };
	virtual QString                             GetName() const override        { return m_shortName; }

	virtual QString                             GetToolTipText() const override { return m_fullQualifiedName; }

	virtual CryGraphEditor::CAbstractPinItem*   GetPinItemById(QVariant id) const;

	// CryGraphEditor::CAbstractNodeItem

	CPinItem*                    GetPinItemById(CPinId id) const;

	Schematyc::IScriptGraphNode& GetScriptElement() const { return m_scriptNode; }
	CryGUID                      GetGUID() const;

	bool                         IsRemovable() const;
	bool                         IsCopyAllowed() const { return true; }
	bool                         IsPasteAllowed() const;

	void                         Refresh(bool forceRefresh = false);

protected:
	void LoadFromScriptElement();
	void RefreshName();
	void Validate();

private:
	QString                      m_shortName;
	QString                      m_fullQualifiedName;
	CryGraphEditor::PinItemArray m_pins;

	Schematyc::IScriptGraphNode& m_scriptNode;
	QColor                       m_headerTextColor;
	bool                         m_isDirty;
};

}

