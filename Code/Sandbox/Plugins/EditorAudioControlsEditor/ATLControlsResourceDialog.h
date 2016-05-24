// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>
#include "Controls/EditorDialog.h"
#include "QTreeWidgetFilter.h"
#include <ACETypes.h>

class QAudioControlsTreeView;
class QDialogButtonBox;
class QAudioControlSortProxy;
class QStandardItemModel;

namespace ACE
{
class CATLControlsModel;

class ATLControlsDialog : public CEditorDialog
{
	Q_OBJECT
public:
	ATLControlsDialog(QWidget* pParent, EACEControlType eType);
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

	QModelIndex FindItem(const string& sControlName);
	void        ApplyFilter();
	bool        ApplyFilter(const QModelIndex parent);
	bool        IsValid(const QModelIndex index);

	// ------------------ QWidget ----------------------------
	bool eventFilter(QObject* pObject, QEvent* pEvent);
	// -------------------------------------------------------

	// Filtering
	QString                 m_sFilter;
	EACEControlType         m_eType;
	Scope                   m_scope;

	static string           ms_controlName;
	QAudioControlsTreeView* m_pControlTree;
	QDialogButtonBox*       m_pDialogButtons;

	QStandardItemModel*     m_pTreeModel;
	QAudioControlSortProxy* m_pProxyModel;
	CATLControlsModel*      m_pATLModel;
};
}
