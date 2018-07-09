// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QAbstractItemModel>

#include "FileImportInfo.h"

#include <QHeaderView>
#include <CrySandbox/CrySignal.h>
#include <FileDialogs/ExtensionFilter.h>

namespace ACE
{
namespace Impl
{
using ColumnResizeModes = std::map<int, QHeaderView::ResizeMode>;

struct IItemModel : public QAbstractItemModel
{
	//! \cond INTERNAL
	virtual ~IItemModel() = default;
	//! \endcond

	//! Returns the logical index of the name column. Used for filtering.
	virtual int GetNameColumn() const = 0;

	//! Returns the logical indexes of columns and their resize modes.
	//! Columns that are not returned will be set to interactive resize mode.
	virtual ColumnResizeModes const& GetColumnResizeModes() const = 0;

	//! Returns the name of the current selected folder item, if supported by the implementation.
	//! Used for file import.
	//! \param index - Index of the current selected item.
	virtual QString const GetTargetFolderName(QModelIndex const& index) const = 0;

	//! Gets the supported file types.
	//! Used for file import.
	virtual QStringList const& GetSupportedFileTypes() const = 0;

	//! Gets the extensions and their descriptions for file types that are allowed to get imported.
	//! Used for file import.
	virtual ExtensionFilterVector const& GetExtensionFilters() const = 0;

	//! Sends a signal when files get dropped, if the middleware allows file import.
	//! \param SFileImportInfo - List of file infos of the dropped files.
	//! \param QString - Name of the target folder on which the files are dropped. If files are dropped on the root, this string has to be empty.
	CCrySignal<void(FileImportInfos const&, QString const&)> SignalDroppedFiles;
};
} // namespace Impl
} // namespace ACE

