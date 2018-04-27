// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include <QObject>
#include <CrySerialization/StringList.h>

// Private class, should not be used directly
class QWidget;
class QPropertyDialog;
class CBatchFileDialog : public QObject
{
	Q_OBJECT
public slots:
	void OnSelectAll();
	void OnSelectNone();
	void OnLoadList();
public:
	QPropertyDialog* m_dialog;
	struct SContent;
	SContent*        m_content;
};
// ^^^

struct SBatchFileSettings
{
	const char*               scanExtension;
	const char*               scanFolder;
	const char*               title;
	const char*               descriptionText;
	const char*               listLabel;
	const char*               stateFilename;
	bool                      useCryPak;
	bool                      allowListLoading;
	Serialization::StringList explicitFileList;
	int                       defaultWidth;
	int                       defaultHeight;

	SBatchFileSettings()
		: useCryPak(true)
		, allowListLoading(true)
		, descriptionText("Batch Selected Files")
		, listLabel("Files")
		, stateFilename("batchFileDialog.state")
		, title("Batch Files")
		, scanFolder("")
		, scanExtension("")
	{
	}
};

bool EDITOR_COMMON_API ShowBatchFileDialog(Serialization::StringList* filenames, const SBatchFileSettings& settings, QWidget* parent);

