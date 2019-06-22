// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"
#include <Controls/EditorDialog.h>

class QSearchBox;
class QDialogButtonBox;
class CMountingProxyModel;
class QAbstractItemModel;
class QModelIndex;

namespace ACE
{
class CTreeView;
class CAsset;
class CResourceSourceModel;
class CResourceLibraryModel;
class CResourceFilterProxyModel;

class CResourceSelectorDialog final : public CEditorDialog
{
	Q_OBJECT

public:

	struct SResourceSelectionDialogResult final
	{
		string selectedItem;
		bool   selectionAccepted;
	};

	CResourceSelectorDialog() = delete;
	CResourceSelectorDialog(CResourceSelectorDialog const&) = delete;
	CResourceSelectorDialog(CResourceSelectorDialog&&) = delete;
	CResourceSelectorDialog& operator=(CResourceSelectorDialog const&) = delete;
	CResourceSelectorDialog& operator=(CResourceSelectorDialog&&) = delete;

	explicit CResourceSelectorDialog(EAssetType const type, bool const isSwitchAndState, QWidget* const pParent);
	virtual ~CResourceSelectorDialog() override;

	//! Returns if the operation was accepted or not. If the operation was accepted the newly selected item is in selectedItem. If the operation was canceled selectedItem will be set to szCurrentValue
	SResourceSelectionDialogResult ChooseItem(char const* szCurrentValue);

	virtual QSize                  sizeHint() const override;

private slots:

	void OnUpdateSelectedControl();
	void OnStopTrigger();
	void OnItemDoubleClicked(QModelIndex const& modelIndex);
	void OnContextMenu(QPoint const& pos);

private:

	QModelIndex         FindItem(string const& controlName);
	QAbstractItemModel* CreateLibraryModelFromIndex(QModelIndex const& sourceIndex);
	void                DeleteModels();
	string              GetSwitchStateNameFromState(string const& stateName);

	// QDialog
	virtual bool eventFilter(QObject* pObject, QEvent* pEvent) override;
	// ~QDialog

	EAssetType const                    m_type;
	bool                                m_selectionIsValid;

	QSearchBox* const                   m_pSearchBox;
	CTreeView* const                    m_pTreeView;
	QDialogButtonBox* const             m_pDialogButtons;
	CResourceFilterProxyModel* const    m_pFilterProxyModel;
	CMountingProxyModel*                m_pMountingProxyModel;
	CResourceSourceModel* const         m_pSourceModel;
	std::vector<CResourceLibraryModel*> m_libraryModels;

	static string                       s_previousControlName;
	static EAssetType                   s_previousControlType;
	static bool                         s_isSwitchAndState;
};
} // namespace ACE
