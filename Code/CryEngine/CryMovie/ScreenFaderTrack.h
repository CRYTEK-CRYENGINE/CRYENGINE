// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SCREENFADERTRACK_H__
#define __SCREENFADERTRACK_H__

#pragma once

#include <CryMovie/IMovieSystem.h>
#include "AnimTrack.h"

class CScreenFaderTrack : public TAnimTrack<SScreenFaderKey>
{
public:
	CScreenFaderTrack();
	~CScreenFaderTrack();

	virtual void           SerializeKey(SScreenFaderKey& key, XmlNodeRef& keyNode, bool bLoading) override;

	virtual CAnimParamType GetParameterType() const override { return eAnimParamType_ScreenFader; }

public:
	void      PreloadTextures();
	ITexture* GetActiveTexture() const;
	void      SetScreenFaderTrackDefaults();

	bool      IsTextureVisible() const         { return m_bTextureVisible; }
	void      SetTextureVisible(bool bVisible) { m_bTextureVisible = bVisible; }
	Vec4      GetDrawColor() const             { return m_drawColor; }
	void      SetDrawColor(Vec4 vDrawColor)    { m_drawColor = vDrawColor; }
	int       GetLastTextureID() const         { return m_lastTextureID; }
	void      SetLastTextureID(int nTextureID) { m_lastTextureID = nTextureID; }
	bool      SetActiveTexture(int index);

private:
	void ReleasePreloadedTextures();

	std::vector<ITexture*> m_preloadedTextures;
	bool                   m_bTextureVisible;
	Vec4                   m_drawColor;
	int                    m_lastTextureID;
};

#endif//__SCREENFADERTRACK_H__
