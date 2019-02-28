// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QFileInfo>

namespace ACE
{
struct SFileImportInfo final
{
	enum class EActionType
	{
		None,
		New,
		Replace,
		Ignore,
		SameFile,
	};

	explicit SFileImportInfo(QFileInfo const& sourceInfo_, bool const isTypeSupported_, QString const& parentFolderName_ = "")
		: sourceInfo(sourceInfo_)
		, isTypeSupported(isTypeSupported_)
		, parentFolderName(parentFolderName_)
		, actionType(EActionType::None)
	{}

	QFileInfo const sourceInfo;
	QFileInfo       targetInfo;
	QString const   parentFolderName;
	EActionType     actionType;
	bool const      isTypeSupported;
};

using FileImportInfos = std::vector<SFileImportInfo>;
} // namespace ACE
