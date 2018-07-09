// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"

#include "Controls/EditorDialog.h"

class QAbstractItemModel;
class QAdvancedTreeView;
class QSearchBox;

class EDITOR_COMMON_API CTreeViewDialog : public CEditorDialog
{
public:
	CTreeViewDialog::CTreeViewDialog(QWidget* pParent)
		: CEditorDialog("TreeViewDialog", pParent)
		, m_pSearchBox(nullptr)
	{}

	void        Initialize(QAbstractItemModel* pModel, int32 filterColumn, const std::vector<int32>& visibleColumns = std::vector<int32>(0));
	void        SetSelectedValue(const QModelIndex& index, bool bExpand);

	QModelIndex GetSelected();

protected:
	void OnOk();

	virtual void showEvent(QShowEvent* event) override;
private:
	QSearchBox*		   m_pSearchBox;
	QAdvancedTreeView* m_pTreeView;
};

