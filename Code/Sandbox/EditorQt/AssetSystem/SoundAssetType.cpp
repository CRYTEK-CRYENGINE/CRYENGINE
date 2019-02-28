// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "SoundAssetType.h"

#include "AssetSystem/AssetResourceSelector.h"

REGISTER_ASSET_TYPE(CSoundType)

// ---------------------------------------------------------------------------

dll_string SoundFileSelector(const SResourceSelectorContext& x, const char* previousValue)
{
	// start in Sounds folder if no sound is selected
	string startPath = PathUtil::GetPathWithoutFilename(previousValue);
	if (previousValue[0] == '\0')
		startPath = AUDIO_SYSTEM_DATA_ROOT CRY_NATIVE_PATH_SEPSTR;

	string relativeFilename = previousValue;
	if (CFileUtil::SelectSingleFile(x.parentWidget, EFILE_TYPE_SOUND, relativeFilename, "", startPath))
		return relativeFilename.GetBuffer();
	else
		return previousValue;
}
dll_string SoundAssetSelector(const SResourceSelectorContext& x, const char* previousValue)
{
	return SStaticAssetSelectorEntry::SelectFromAsset(x, { "Sound" }, previousValue);
}
dll_string SoundSelector(const SResourceSelectorContext& x, const char* previousValue)
{
	EAssetResourcePickerState state = (EAssetResourcePickerState)GetIEditorImpl()->GetSystem()->GetIConsole()->GetCVar("ed_enableAssetPickers")->GetIVal();
	if (state == EAssetResourcePickerState::EnableAll)
	{
		return SoundAssetSelector(x, previousValue);
	}
	else
	{
		return SoundFileSelector(x, previousValue);
	}
}
REGISTER_RESOURCE_SELECTOR("Sound", SoundSelector, "")

