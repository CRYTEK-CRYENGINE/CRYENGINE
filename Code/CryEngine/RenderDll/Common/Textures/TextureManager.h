// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryString/CryName.h>

class CTexture;

class CTextureManager
{
public:

	CTextureManager() {}
	virtual ~CTextureManager();

	void            PreloadDefaultTextures();
	void            ReleaseDefaultTextures();

	const CTexture* GetDefaultTexture(const string& sTextureName) const;
	const CTexture* GetDefaultTexture(const CCryNameTSCRC& sTextureNameID) const;

private:

	typedef std::map<CCryNameTSCRC, CTexture*> TTextureMap;
	TTextureMap m_DefaultTextures;
};
