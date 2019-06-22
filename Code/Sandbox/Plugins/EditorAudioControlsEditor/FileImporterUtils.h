// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/FileImportInfo.h"
#include <FileDialogs/ExtensionFilter.h>

namespace ACE
{
class CAsset;

extern ExtensionFilterVector g_extensionFilters;
extern QStringList g_supportedFileTypes;

enum class EImportTargetType : CryAudio::EnumFlagsType
{
	SystemControls,
	Connections,
	Middleware, };

void OpenFileSelectorFromImpl(QString const& targetFolderName, bool const isLocalized);
void OpenFileSelector(EImportTargetType const type, CAsset* const pAsset);
void OpenFileImporter(
	FileImportInfos const& fileImportInfos,
	QString const& targetFolderName,
	bool const isLocalized,
	EImportTargetType const type,
	CAsset* const pAsset);
} // namespace ACE
