// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>

#include "EditorFramework/Editor.h"

class QAdvancedTreeView;
class QFilteringPanel;
class QAttributeFilterProxyModel;
class QAbstractItemModel;
class QLabel;
struct CLayerChangeEvent;
class CLevelExplorer;
class QItemSelection;

class CActiveLayerWidget : public QWidget
{
	Q_OBJECT
public:
	CActiveLayerWidget(CLevelExplorer* parent);
	void UpdateLabel();

	QLabel* m_label;
};

class CLevelExplorer final : public CDockableEditor
{
	Q_OBJECT

public:
	CLevelExplorer(QWidget* pParent = nullptr);
	~CLevelExplorer();

	//////////////////////////////////////////////////////////////////////////
	// CEditor implementation
	virtual const char* GetEditorName() const override { return "Level Explorer"; }
	virtual void        SetLayout(const QVariantMap& state) override;
	virtual QVariantMap GetLayout() const override;
	virtual void        customEvent(QEvent* event) override;
	//////////////////////////////////////////////////////////////////////////

	enum ModelType
	{
		Objects,
		Layers,
		FullHierarchy,
		ActiveLayer
	};

	bool IsModelTypeShowingLayers() const
	{
		return m_modelType == Layers || m_modelType == FullHierarchy;
	}

	void ShowActiveLayerWidget(bool show);
	void SetModelType(ModelType aModelType);
	void SetSyncSelection(bool syncSelection);
	void FocusActiveLayer();
	void GrabFocusSearchBar() { OnFind(); }

private:

	void         InitMenuBar();

	virtual void OnContextMenu(const QPoint& pos) const;
	void         OnContextMenuForSingleLayer(QMenu *menu, CObjectLayer* layer) const;
	void         OnClick(const QModelIndex& index);
	void         OnDoubleClick(const QModelIndex& index);

	//////////////////////////////////////////////////////////////////////////
	// CEditor impl
	virtual bool OnFind() override;
	virtual bool OnDelete() override;
	//////////////////////////////////////////////////////////////////////////

	void        OnLayerChange(const CLayerChangeEvent& event);
	void        OnObjectChange(CObjectEvent& event);
	void        OnViewportSelectionChanged(const std::vector<CBaseObject*>& selected, const std::vector<CBaseObject*>& deselected);
	void        OnLayerModelsUpdated();
	void        OnModelDestroyed();
	void        OnNewLayer(CObjectLayer* parent) const;
	void        OnNewFolder(CObjectLayer* parent) const;
	void        OnReloadLayers(const std::vector<CObjectLayer*>& layers) const;
	void        OnDeleteLayers(const std::vector<CObjectLayer*>& layers) const;
	void        OnImportLayer(CObjectLayer* pTargetLayer = nullptr) const;
	void        OnFreezeAllBut(CObjectLayer* layer) const;
	void        OnHideAllBut(CObjectLayer* layer) const;
	void        OnFreezeAllInLayer(CObjectLayer* layer) const;
	void        OnUnfreezeAllInLayer(CObjectLayer* layer) const;
	void        OnHideAllInLayer(CObjectLayer* layer) const;
	void        OnUnhideAllInLayer(CObjectLayer* layer) const;
	bool        AllFrozenInLayer(CObjectLayer* layer) const;
	bool        AllHiddenInLayer(CObjectLayer* layer) const;
	void        OnExpandAllLayers() const;
	void        OnCollapseAllLayers() const;
	void        OnSelectColor(const std::vector<CObjectLayer*>& layers) const;
	void        OnHeaderSectionCountChanged();
	void        OnSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
	void        OnRename(const QModelIndex& index) const;
	void        EditLayer(CObjectLayer* pLayer) const;

	void        SetSourceModel(QAbstractItemModel* model);
	void        SyncSelection();

	void        UpdateCurrentIndex();
	void        CreateItemSelectionFrom(const std::vector<CBaseObject*>& objects, QItemSelection& selection) const;

	QModelIndex FindObjectIndex(const CBaseObject* object) const;
	QModelIndex FindLayerIndex(const CObjectLayer* layer) const;
	QModelIndex FindIndexByInternalId(intptr_t id) const;
	QModelIndex FindLayerIndexInModel(const CObjectLayer* layer) const;
	QModelIndex FindObjectInHierarchy(const QModelIndex& parentIndex, const CBaseObject* object) const;

	// returns nullptr if none or more than one layer/folder was selected or if selected object was not a layer/folder
	CObjectLayer* GetSingleLayerInCaseOfSingleSelection() const;

	ModelType                   m_modelType;
	QAdvancedTreeView*          m_treeView;
	CActiveLayerWidget*         m_activeLayerWidget;
	QFilteringPanel*            m_filterPanel;
	QAttributeFilterProxyModel* m_pAttributeFilterProxyModel;
	bool                        m_syncSelection;
	bool                        m_ignoreSelectionEvents;
};

