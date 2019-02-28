// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
////////////////////////////////////////////////////////////////////////////
//
//  CryEngine Source File.
//  Copyright (C), Crytek
// -------------------------------------------------------------------------
//  File name: CommentNodeAnimator.h
//  Created:   09-04-2010 by Dongjoon Kim
//  Description: Comment node animator class
//
////////////////////////////////////////////////////////////////////////////

/** CCommentContext stores information about comment track.
   The Comment Track is activated only in the editor.
 */

#include "Nodes/TrackViewAnimNode.h"

class CTrackViewTrack;

struct CCommentContext
{
	CCommentContext() : m_nLastActiveKeyIndex(-1), m_strComment(0), m_size(1.0f), m_align(0)
	{
		cry_sprintf(m_strFont, "default");
		m_unitPos = Vec2(0.f, 0.f);
		m_color = Vec3(0.f, 0.f, 0.f);
	}

	int         m_nLastActiveKeyIndex;

	const char* m_strComment;
	char        m_strFont[64];
	Vec2        m_unitPos;
	Vec3        m_color;
	float       m_size;
	int         m_align;
};

class CCommentNodeAnimator : public IAnimNodeAnimator
{
public:
	CCommentNodeAnimator(CTrackViewAnimNode* pCommentNode);
	virtual void Animate(CTrackViewAnimNode* pNode, const SAnimContext& ac);
	virtual void Render(CTrackViewAnimNode* pNode, const SAnimContext& ac);

private:
	virtual ~CCommentNodeAnimator();

	void                AnimateCommentTextTrack(CTrackViewTrack* pTrack, const SAnimContext& ac);
	CTrackViewKeyHandle GetActiveKeyHandle(CTrackViewTrack* pTrack, SAnimTime fTime);
	Vec2                GetScreenPosFromNormalizedPos(const Vec2& unitPos);
	void                DrawText(const char* szFontName, float fSize, const Vec2& unitPos, const ColorF col, const char* szText, int align);

	CTrackViewAnimNode* m_pCommentNode;
	CCommentContext     m_commentContext;
};

