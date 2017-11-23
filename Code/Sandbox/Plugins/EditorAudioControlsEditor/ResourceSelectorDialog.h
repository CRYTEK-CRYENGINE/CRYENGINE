// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Controls/EditorDialog.h>
#include <SystemTypes.h>

class QAdvancedTreeView;
class QDeepFilterProxyModel;
class QDialogButtonBox;
class CMountingProxyModel;
class QAbstractItemModel;
class QModelIndex;

namespace ACE
{
class CSystemAssetsManager;
class CResourceLibraryModel;
class CResourceControlsModel;

class CResourceSelectorDialog final : public CEditorDialog
{
	Q_OBJECT

public:

	CResourceSelectorDialog(QWidget* pParent, ESystemItemType const eType);
	~CResourceSelectorDialog();

private slots:

	void UpdateSelectedControl();
	void SetTextFilter(QString const&filter);
	void StopTrigger();
	void ItemDoubleClicked(QModelIndex const& modelIndex);
	void OnContextMenu(QPoint const& pos);

public:

	void        SetScope(Scope const scope);
	char const* ChooseItem(char const* currentValue);
	QSize       sizeHint() const override;

private:

	QModelIndex         FindItem(string const& sControlName);
	void                ApplyFilter();
	bool                ApplyFilter(QModelIndex const& parent);
	bool                IsValid(QModelIndex const& index);
	QAbstractItemModel* CreateControlsModelFromIndex(QModelIndex const& sourceIndex);

	// QDialog
	virtual bool eventFilter(QObject* pObject, QEvent* pEvent) override;
	// ~QDialog

	// Filtering
	QString                              m_sFilter;
	ESystemItemType                      m_eType;
	Scope                                m_scope;
	bool                                 m_selectionIsValid = false;

	static string                        s_previousControlName;
	static ESystemItemType               s_previousControlType;
	QAdvancedTreeView* const             m_pTreeView;
	QDialogButtonBox* const              m_pDialogButtons;

	CSystemAssetsManager*                m_pAssetsManager;
	QDeepFilterProxyModel*               m_pFilterProxyModel;
	CResourceLibraryModel*               m_pLibraryModel;
	CMountingProxyModel*                 m_pMountingProxyModel;
	std::vector<CResourceControlsModel*> m_controlsModels;
};
} // namespace ACE
