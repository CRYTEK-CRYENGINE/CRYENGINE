// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "AssetSystem/AssetType.h"

//This is only for loose sounds (ex: mp3, ogg, wav files)
//Sounds from soundbanks are not handled though this asset type
class CSoundType : public CAssetType
{
public:
	DECLARE_ASSET_TYPE_DESC(CSoundType);

	virtual const char* GetTypeName() const override		{ return "Sound"; }
	virtual const char* GetUiTypeName() const override		{ return QT_TR_NOOP("Sound"); }
	virtual const char* GetFileExtension() const override	{ return "wav"; } // TODO: At the moment, the RC produces .ogg.cryasset and .wav.cryasset files.
	virtual bool        IsImported() const override			{ return false; }
	virtual bool        CanBeEdited() const override		{ return false; }

	//This asset type almost could use the generic pickers 
	//but because it is allowing more extensions than wav to be picked with file picker
	//and we know that this asset type has problems handling various formats still
	virtual bool IsUsingGenericPropertyTreePicker() const override { return false; }

private:
	virtual CryIcon GetIconInternal() const override
	{
		return CryIcon("icons:common/assets_sound.ico");
	}
};

