// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>
#include "Controls/EditorDialog.h"
#include "QTreeWidgetFilter.h"
#include <ACETypes.h>

class QAudioControlsTreeView;
class QDialogButtonBox;
class QAudioControlSortProxy;
class CMountingProxyModel;
class QAbstractItemModel;

namespace ACE
{

class CAudioAssetsManager;
class CAudioAssetsExplorerModel;
class CAudioLibraryModel;

class ATLControlsDialog : public CEditorDialog
{
	Q_OBJECT
public:
	ATLControlsDialog(QWidget* pParent, EItemType eType);
	~ATLControlsDialog();

private slots:
	void UpdateSelectedControl();
	void SetTextFilter(QString filter);
	void StopTrigger();
	void ItemDoubleClicked(const QModelIndex& modelIndex);

public:
	void        SetScope(Scope scope);
	const char* ChooseItem(const char* currentValue);
	QSize       sizeHint() const override;

private:

	QModelIndex         FindItem(const string& sControlName);
	void                ApplyFilter();
	bool                ApplyFilter(const QModelIndex parent);
	bool                IsValid(const QModelIndex index);
	QAbstractItemModel* CreateLibraryModelFromIndex(const QModelIndex& sourceIndex);

	// ------------------ QWidget ----------------------------
	bool eventFilter(QObject* pObject, QEvent* pEvent);
	// -------------------------------------------------------

	// Filtering
	QString                          m_sFilter;
	EItemType                        m_eType;
	Scope                            m_scope;

	static string                    ms_controlName;
	QAudioControlsTreeView*          m_pControlTree;
	QDialogButtonBox*                m_pDialogButtons;

	QAudioControlSortProxy*          m_pProxyModel;
	CAudioAssetsManager*             m_pAssetsManager;
	CAudioAssetsExplorerModel*       m_pAssetsModel;
	CMountingProxyModel*             m_pMountedModel;
	std::vector<CAudioLibraryModel*> m_libraryModels;

};
}
