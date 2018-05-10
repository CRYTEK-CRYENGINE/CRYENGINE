// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "BatchFileDialog.h"
#include <CrySerialization/StringList.h>
#include <CryCore/Platform/CryWindows.h>
#include <Controls/QuestionDialog.h>
namespace CharacterTool
{

void ShowCleanCompiledAnimationsTool(QWidget* parent)
{
	Serialization::StringList filenames;
	SBatchFileSettings settings;
	settings.useCryPak = false;
	settings.scanExtension = "caf";
	settings.title = "Clean Compiled Animations";
	settings.stateFilename = "cleanAnimations.state";
	settings.listLabel = "Compiled Animation Files";
	settings.descriptionText = "Files marked below will be deleted and recompiled if needed:";

	if (ShowBatchFileDialog(&filenames, settings, parent))
	{
		int numFailed = 0;
		for (size_t i = 0; i < filenames.size(); ++i)
		{
			const char* path = filenames[i].c_str();
			DWORD attribs = GetFileAttributesA(path);
			if (attribs == INVALID_FILE_ATTRIBUTES)
			{
				++numFailed;
			}
			else
			{
				if ((attribs & FILE_ATTRIBUTE_READONLY) != 0)
					SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);

				if (!DeleteFileA(path))
					++numFailed;
			}
		}
		if (numFailed > 0)
		{
			QString message;
			message.sprintf("Failed to remove %d files.", numFailed);
			CQuestionDialog::SWarning("Error", message);
		}
	}
}

}

