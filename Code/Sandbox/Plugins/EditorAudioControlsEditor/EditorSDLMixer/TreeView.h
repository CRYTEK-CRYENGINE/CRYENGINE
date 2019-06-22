// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ItemModel.h"
#include "../Common/SharedData.h"
#include <QAdvancedTreeView.h>

namespace ACE
{
namespace Impl
{
namespace SDLMixer
{
constexpr int g_nameColumn = static_cast<int>(CItemModel::EColumns::Name);

class CTreeView final : public QAdvancedTreeView
{
public:

	CTreeView() = delete;
	CTreeView(CTreeView const&) = delete;
	CTreeView(CTreeView&&) = delete;
	CTreeView& operator=(CTreeView const&) = delete;
	CTreeView& operator=(CTreeView&&) = delete;

	explicit CTreeView(QWidget* const pParent);
	virtual ~CTreeView() override = default;

	void ExpandSelection();
	void CollapseSelection();

	void BackupExpanded();
	void RestoreExpanded();

	void BackupSelection();
	void RestoreSelection();

private:

	ControlId GetItemId(QModelIndex const& index) const;

	void      ExpandSelectionRecursively(QModelIndexList const& indexList);
	void      CollapseSelectionRecursively(QModelIndexList const& indexList);

	void      BackupExpandedRecursively(QModelIndex const& index);
	void      RestoreExpandedRecursively(QModelIndex const& index);
	void      RestoreSelectionRecursively(QModelIndex const& index);

	QSet<ControlId> m_expandedBackup;
	QSet<ControlId> m_selectionBackup;
};
} // namespace SDLMixer
} // namespace Impl
} // namespace ACE
