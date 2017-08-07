// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QFrame>
#include <QMenu>

#include "AudioAssets.h"
#include "QTreeWidgetFilter.h"
#include <CryAudio/IAudioInterfacesCommonData.h>

// Forward declarations
class QSortFilterProxyModel;
class QLineEdit;
class QAudioControlsTreeView;
class QAbstractItemModel;
class QAdvancedTreeView;
class CMountingProxyModel;
class QCheckableMenu;

namespace ACE
{
class CAudioAssetsManager;
class QFilterButton;
class CAudioAssetsExplorerModel;
class QControlsProxyFilter;
class CAudioLibraryModel;
class CAudioAdvancedTreeView;

class CAudioAssetsExplorer final : public QFrame
{
	Q_OBJECT

public:

	CAudioAssetsExplorer(CAudioAssetsManager* pAssetsManager);
	virtual ~CAudioAssetsExplorer() override;

	std::vector<CAudioControl*> GetSelectedControls();
	void                        Reload();

private:

	// QObject
	virtual bool eventFilter(QObject* pObject, QEvent* pEvent) override;
	// ~QObject

	// Filtering
	void                ResetFilters();
	void                ShowControlType(EItemType type, bool bShow);

	CAudioControl*      CreateControl(string const& name, EItemType type, IAudioAsset* pParent);
	IAudioAsset*        CreateFolder(IAudioAsset* pParent);

	QAbstractItemModel* CreateLibraryModelFromIndex(QModelIndex const& sourceIndex);
	IAudioAsset*        GetSelectedAsset() const;

	void                SelectNewAsset(QModelIndex const& parent, int const row);

private slots:

	void DeleteSelectedControl();
	void ShowControlsContextMenu(QPoint const& pos);

	// Audio Preview
	void ExecuteControl();
	void StopControlExecution();

signals:

	void SelectedControlChanged();
	void ControlTypeFiltered(EItemType type, bool bShow);

private:

	CAudioAssetsManager* const m_pAssetsManager;

	// Context Menu
	QMenu m_addItemMenu;

	// Filtering
	QString                          m_filter;
	QCheckableMenu*                  m_pFilterMenu;

	QLineEdit*                       m_pTextFilter;
	CAudioAdvancedTreeView*          m_pControlsTree;
	CAudioAssetsExplorerModel*       m_pAssetsModel;
	QControlsProxyFilter*            m_pProxyModel;

	CMountingProxyModel*             m_pMountedModel;
	std::vector<CAudioLibraryModel*> m_libraryModels;
	bool                             m_bReloading = false;
	bool                             m_bCreatedFromMenu = false;
};
} // namespace ACE
