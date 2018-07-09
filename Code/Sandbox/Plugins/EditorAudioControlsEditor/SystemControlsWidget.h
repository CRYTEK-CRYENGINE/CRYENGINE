// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>

#include <SharedData.h>

class QAction;
class QVBoxLayout;
class QAbstractItemModel;
class CMountingProxyModel;
class QFilteringPanel;

namespace ACE
{
class CAsset;
class CControl;
class CSystemSourceModel;
class CSystemLibraryModel;
class CSystemFilterProxyModel;
class CTreeView;

class CSystemControlsWidget final : public QWidget
{
	Q_OBJECT

public:

	CSystemControlsWidget(QWidget* const pParent);
	virtual ~CSystemControlsWidget() override;

	bool   IsEditing() const;
	Assets GetSelectedAssets() const;
	void   SelectConnectedSystemControl(ControlId const systemControlId, ControlId const implItemId);
	void   Reset();
	void   OnAboutToReload();
	void   OnReloaded();

signals:

	void SignalSelectedControlChanged();

private slots:

	void OnRenameSelectedControls(string const& name);
	void OnDeleteSelectedControls();
	void OnContextMenu(QPoint const& pos);
	void OnUpdateCreateButtons();

	void ExecuteControl();
	void StopControlExecution();

private:

	// QObject
	virtual bool eventFilter(QObject* pObject, QEvent* pEvent) override;
	// ~QObject

	void                InitAddControlWidget(QVBoxLayout* const pLayout);
	void                SelectNewAsset(QModelIndex const& parent, int const row);
	void                ClearFilters();
	void                DeleteModels();

	CControl*           CreateControl(string const& name, EAssetType const type, CAsset* const pParent);
	CAsset*             CreateFolder(CAsset* const pParent);
	void                CreateParentFolder();
	bool                IsParentFolderAllowed() const;
	bool                IsDefaultControlSelected() const;

	QAbstractItemModel* CreateLibraryModelFromIndex(QModelIndex const& sourceIndex);
	CAsset*             GetSelectedAsset() const;

	QFilteringPanel*                  m_pFilteringPanel;
	CTreeView* const                  m_pTreeView;
	CSystemFilterProxyModel* const    m_pSystemFilterProxyModel;
	CMountingProxyModel*              m_pMountingProxyModel;
	CSystemSourceModel* const         m_pSourceModel;
	std::vector<CSystemLibraryModel*> m_libraryModels;

	QAction*                          m_pCreateParentFolderAction;
	QAction*                          m_pCreateFolderAction;
	QAction*                          m_pCreateTriggerAction;
	QAction*                          m_pCreateParameterAction;
	QAction*                          m_pCreateSwitchAction;
	QAction*                          m_pCreateStateAction;
	QAction*                          m_pCreateEnvironmentAction;
	QAction*                          m_pCreatePreloadAction;

	bool                              m_isReloading;
	bool                              m_isCreatedFromMenu;
	bool                              m_suppressRenaming;
	int const                         m_nameColumn;
};
} // namespace ACE

