// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>

// Sandbox
#include "Objects/ObjectLayer.h"

// EditorCommon
#include <EditorFramework/Editor.h>
#include <LevelEditor/ILevelExplorerContext.h>

class QAdvancedTreeView;
class QFilteringPanel;
class QAttributeFilterProxyModel;
class QAbstractItemModel;
class CLevelExplorer;
class QBoxLayout;
class QItemSelection;
class CBaseObject;
class CObjectLayer;
class QToolButton;

struct CLayerChangeEvent;
struct CObjectEvent;

class CLevelExplorer final : public CDockableEditor, public ILevelExplorerContext
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

	void                       SetModelType(ModelType aModelType);
	void                       SetSyncSelection(bool syncSelection);
	void                       FocusActiveLayer();
	void                       GrabFocusSearchBar() { OnFind(); }

	std::vector<CObjectLayer*> GetSelectedObjectLayers() const;

	virtual void GetSelection(std::vector<IObjectLayer*>& layers, std::vector<IObjectLayer*>& layerFolders) const override;

	virtual std::vector<IObjectLayer*> GetSelectedIObjectLayers() const override;

protected:
	virtual const IEditorContext* GetContextObject() const override { return this; };

	// Adaptive layouts enables editor owners to make better use of space
	bool            SupportsAdaptiveLayout() const override { return true; }
	// Used for determining what layout direction to use if adaptive layout is turned off
	Qt::Orientation GetDefaultOrientation() const override  { return Qt::Vertical; }

private:
	QToolButton*  CreateToolButton(QCommandAction* pAction);
	void          InitMenuBar();
	void          InitActions();
	CObjectLayer* GetParentLayerForIndexList(const QModelIndexList& indexList) const;
	CObjectLayer* CreateLayer(EObjectLayerType layerType);

	virtual void  OnContextMenu(const QPoint& pos) const;

	void          CreateContextMenuForLayers(CAbstractMenu& abstractMenu, const std::vector<CObjectLayer*>& layers) const;
	void          PopulateExistingSectionsForLayers(CAbstractMenu& abstractMenu) const;

	void          OnClick(const QModelIndex& index);
	void          OnDoubleClick(const QModelIndex& index);

	void          SetActionsEnabled(bool areEnabled);
	void          UpdateSelectionActionState();

	void          CopySelectedLayersInfo(std::function<string(const CObjectLayer*)> retrieveInfoFunc) const;

	bool  OnNew();
	bool  OnNewFolder();
	bool  OnImport();
	bool  OnReload();

	virtual bool  OnFind() override;
	bool  OnDelete();

	bool  OnRename();
	bool  OnLock();
	bool  OnUnlock();
	bool  OnToggleLock();
	void          OnLockAll();
	void          OnUnlockAll();
	bool  OnIsolateLocked();
	bool  OnHide();
	bool  OnUnhide();
	bool  OnToggleHide();
	void          OnHideAll();
	void          OnUnhideAll();
	bool  OnIsolateVisibility();

	// Context sensitive hierarchical operations
	bool OnCollapseAll();
	bool OnExpandAll();
	bool OnLockChildren();
	bool OnUnlockChildren();
	bool OnToggleLockChildren();
	bool OnHideChildren();
	bool OnUnhideChildren();
	bool OnToggleHideChildren();

	bool         OnLockReadOnlyLayers();
	bool         OnMakeLayerActive();
	bool         IsFirstChildLocked(const std::vector<CObjectLayer*>& layers) const;
	bool         DoChildrenMatchLockedState(const std::vector<CObjectLayer*>& layers, bool isLocked) const;
	bool         IsFirstChildHidden(const std::vector<CObjectLayer*>& layers) const;
	bool         DoChildrenMatchHiddenState(const std::vector<CObjectLayer*>& layers, bool isHidden) const;

	bool         IsolateLocked(const QModelIndex& index);
	bool         IsolateVisibility(const QModelIndex& index);

	void         OnLayerChange(const CLayerChangeEvent& event);
	void         OnObjectsChanged(const std::vector<CBaseObject*>& objects, const CObjectEvent& event);
	void         OnViewportSelectionChanged(const std::vector<CBaseObject*>& selected, const std::vector<CBaseObject*>& deselected);
	void         OnLayerModelsUpdated();
	void         OnModelDestroyed();
	void         OnDeleteLayers(const std::vector<CObjectLayer*>& layers) const;
	void         OnUnlockAllInLayer(CObjectLayer* layer) const;
	void         OnLockAllInLayer(CObjectLayer* layer) const;
	void         OnHideAllInLayer(CObjectLayer* layer) const;
	void         OnUnhideAllInLayer(CObjectLayer* layer) const;
	bool         AllFrozenInLayer(CObjectLayer* layer) const;
	bool         AllHiddenInLayer(CObjectLayer* layer) const;
	void         OnSelectColor(const std::vector<CObjectLayer*>& layers) const;
	void         OnHeaderSectionCountChanged();
	void         OnSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
	//Register to any Reset event being called on a CLevelLayerModel
	void         OnLayerModelResetBegin();
	void         OnLayerModelResetEnd();
	void         EditLayer(CObjectLayer* pLayer) const;

	void         SetSourceModel(QAbstractItemModel* model);
	void         SyncSelection();

	void         UpdateCurrentIndex();
	void         CreateItemSelectionFrom(const std::vector<CBaseObject*>& objects, QItemSelection& selection) const;

	QModelIndex  FindObjectIndex(const CBaseObject* object) const;
	QModelIndex  FindLayerIndex(const CObjectLayer* layer) const;
	QModelIndex  FindIndexByInternalId(intptr_t id) const;
	QModelIndex  FindLayerIndexInModel(const CObjectLayer* layer) const;
	QModelIndex  FindObjectInHierarchy(const QModelIndex& parentIndex, const CBaseObject* object) const;

	ModelType                   m_modelType;
	QAdvancedTreeView*          m_treeView;
	QFilteringPanel*            m_filterPanel;
	QAttributeFilterProxyModel* m_pAttributeFilterProxyModel;
	QBoxLayout*                 m_pMainLayout;
	bool                        m_syncSelection;
	bool                        m_ignoreSelectionEvents;
};
