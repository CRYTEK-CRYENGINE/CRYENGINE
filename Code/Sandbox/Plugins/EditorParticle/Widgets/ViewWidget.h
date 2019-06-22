// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AsyncNodeGraphView.h"

#include <NodeGraph/ICryGraphEditor.h>

#include <QWidget>

class QAdvancedPropertyTreeLegacy;

namespace CryParticleEditor
{

class CFeatureItem;
class CFeatureWidget;
class CNodeItem;
class CParticleGraphModel;

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

	CCrySignal<void()> signalItemsChanged;

protected:
	virtual void showEvent(QShowEvent* pEvent) override;

	void         OnBeginUndo();
	void         OnEndUndo(bool undoAccepted);
	void         OnItemsChanged();
	void         OnItemsDeletion();

private:
	QAdvancedPropertyTreeLegacy*          m_pPropertyTree;
	Serialization::SStructs         m_structs;

	CNodeItem*                      m_pNodeItem;
	std::vector<SFeatureSerializer> m_features;
	bool                            m_isPushingUndo;
	string                          m_latestUndoDescription;
};
// ~TODO

class CGraphView : public CAsyncNodeGraphView
{
	enum ECustomAction : uint32
	{
		eCustomAction_MoveFeature = CNodeGraphView::eAction_UserAction,
	};

public:
	CGraphView();

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

	void OnItemsChanged();
private:
	CFeatureWidget* m_pMovingFeatureWidget;
};

}
