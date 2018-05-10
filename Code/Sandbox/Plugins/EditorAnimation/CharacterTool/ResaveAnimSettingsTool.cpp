// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "BatchFileDialog.h"
#include <CrySerialization/StringList.h>
#include <QApplication>
#include "AnimationList.h"
#include <CryCore/Platform/CryWindows.h>
#include "Controls/QuestionDialog.h"

namespace CharacterTool
{

void ShowResaveAnimSettingsTool(AnimationList* animationList, QWidget* parent)
{
	Serialization::StringList filenames;
	SBatchFileSettings settings;
	settings.useCryPak = true;
	settings.scanExtension = "animsettings";
	settings.title = "Resave AnimSettings";
	settings.stateFilename = "resaveAnimSettings.state";
	settings.listLabel = "AnimSettings files";
	settings.descriptionText = "Files marked below will be automatically resaved (converted to new format if needed).";

	if (ShowBatchFileDialog(&filenames, settings, parent))
	{
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
		QApplication::processEvents();

		int numFailed = 0;
		for (size_t i = 0; i < filenames.size(); ++i)
		{
			if (!animationList->ResaveAnimSettings(filenames[i].c_str()))
			{
				CryLogAlways("Failed to resave animsettings: \"%s\"", filenames[i].c_str());
				++numFailed;
			}
		}
		if (numFailed > 0)
		{
			QString message;
			message.sprintf("Failed to resave %d files. See Sandbox log for details.", numFailed);
			CQuestionDialog::SWarning("Error", message);
		}
		QApplication::restoreOverrideCursor();
	}
}

}

