// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"
#include <Controls/EditorDialog.h>

class QAttributeFilterProxyModel;
class QSearchBox;
class QDialogButtonBox;

namespace ACE
{
class CListenerSelectorModel;
class CTreeView;

class CListenerSelectorDialog final : public CEditorDialog
{
	Q_OBJECT

public:

	struct SResourceSelectionDialogResult final
	{
		string selectedListener;
		bool   selectionAccepted;
	};

	CListenerSelectorDialog() = delete;
	CListenerSelectorDialog(CListenerSelectorDialog const&) = delete;
	CListenerSelectorDialog(CListenerSelectorDialog&&) = delete;
	CListenerSelectorDialog& operator=(CListenerSelectorDialog const&) = delete;
	CListenerSelectorDialog& operator=(CListenerSelectorDialog&&) = delete;

	explicit CListenerSelectorDialog(QWidget* const pParent);
	virtual ~CListenerSelectorDialog() override;

	//! Returns if the operation was accepted or not. If the operation was accepted the newly selected item is in selectedListener. If the operation was canceled selectedListener will be set to szCurrentValue
	SResourceSelectionDialogResult ChooseListener(char const* szCurrentValue);

	virtual QSize                  sizeHint() const override;

private slots:

	void OnUpdateSelectedListener();
	void OnItemDoubleClicked(QModelIndex const& index);

private:

	QModelIndex FindListenerIndex(string const& listenerName);

	bool                              m_selectionIsValid;

	CListenerSelectorModel* const     m_pModel;
	QAttributeFilterProxyModel* const m_pAttributeFilterProxyModel;
	QSearchBox* const                 m_pSearchBox;
	CTreeView* const                  m_pTreeView;
	QDialogButtonBox* const           m_pDialogButtons;

	static string                     s_previousListenerName;
};
} // namespace ACE
