// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <NodeGraph/ICryGraphEditor.h>
#include <NodeGraph/NodeGraphView.h>

#include <QWidget>

class QAdvancedPropertyTree;

namespace CryParticleEditor {

class CFeatureWidget;
class CNodeItem;
class CFeatureItem;

struct SFeatureMouseEventArgs : public CryGraphEditor::SMouseInputEventArgs
{
	SFeatureMouseEventArgs(CryGraphEditor::SMouseInputEventArgs& eventArgs)
		: CryGraphEditor::SMouseInputEventArgs(static_cast<CryGraphEditor::SMouseInputEventArgs &&>(eventArgs))
	{}
};

// TODO: Move this into a separate source file.
class CItemProperties : public QWidget
{
	Q_OBJECT

	struct SFeatureSerializer
	{
		CFeatureItem* pItem;
		string        name;

		SFeatureSerializer()
			: pItem(nullptr)
		{}

		void Serialize(Serialization::IArchive& archive);
	};

public:
	CItemProperties(CryGraphEditor::GraphItemSet& selectedItems);
	~CItemProperties();

	void Serialize(Serialization::IArchive& archive);

protected:
	virtual void showEvent(QShowEvent* pEvent) override;

	void         OnPushUndo();
	void         OnItemsChanged();
	void         OnItemsDeletion();

private:
	QAdvancedPropertyTree*          m_pPropertyTree;
	Serialization::SStructs         m_structs;

	CNodeItem*                      m_pNodeItem;
	std::vector<SFeatureSerializer> m_features;
	bool                            m_isPushingUndo;
};
// ~TODO

class CGraphView : public CryGraphEditor::CNodeGraphView
{
	enum ECustomAction : uint32
	{
		eCustomAction_MoveFeature = CNodeGraphView::eAction_UserAction,
	};

public:
	CGraphView();
	~CGraphView();

	void OnFeatureMouseEvent(QGraphicsItem* pSender, SFeatureMouseEventArgs& args);

private:
	// CryGraphEditor::CNodeGraphView
	virtual QWidget* CreatePropertiesWidget(CryGraphEditor::GraphItemSet& selectedItems) override;

	virtual bool     PopulateNodeContextMenu(CryGraphEditor::CAbstractNodeItem& node, QMenu& menu) override;

	virtual bool     DeleteCustomItem(CryGraphEditor::CAbstractNodeGraphViewModelItem& item) override;
	virtual void     OnRemoveCustomItem(CryGraphEditor::CAbstractNodeGraphViewModelItem& item) override;
	// ~CryGraphEditor::CNodeGraphView

	void ShowFeatureContextMenu(CFeatureWidget* pFeatureWidget, QPointF screenPos);
	bool PopulateMenuWithFeatures(const char* szTitle, CryGraphEditor::CAbstractNodeItem& node, QMenu& menu, uint32 index = ~0);

	bool MoveFeatureToIndex(CFeatureWidget& featureWidget, uint32 destIndex);

private:
	CFeatureWidget* m_pMovingFeatureWidget;
};

}

