// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QAdvancedTreeView.h>

namespace ACE
{
class CAudioAdvancedTreeView final : public QAdvancedTreeView
{
public:
	CAudioAdvancedTreeView() = default;

	QModelIndexList GetSelectedIndexes() const { return selectedIndexes(); }
	bool            IsEditing() const;

	void            ExpandSelection(QModelIndexList const& indexList);
	void            CollapseSelection(QModelIndexList const& indexList);
};
} // namespace ACE