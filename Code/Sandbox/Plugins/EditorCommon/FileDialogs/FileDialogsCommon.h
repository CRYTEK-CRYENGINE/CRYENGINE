// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"
#include <CrySandbox/CrySignal.h>

#include <QWidget>
#include "QAdvancedTreeView.h"

namespace FileDialogs
{
//Old "tree view" approach similar to the Explorer and older CryEngine browser designs
struct EDITOR_COMMON_API FilesTreeView : public QAdvancedTreeView
{
	FilesTreeView(QAbstractItemModel* model, QWidget* parent = nullptr);

	CCrySignal<void(QModelIndexList&)> onDragFilter;

protected:
	virtual void startDrag(Qt::DropActions supportedActions) override;
	void         OnContextMenu(const QPoint& point);
};

}

