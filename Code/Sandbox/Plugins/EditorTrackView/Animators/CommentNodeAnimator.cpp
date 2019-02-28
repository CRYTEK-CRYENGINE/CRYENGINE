// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Source File.
// Copyright (C), Crytek, 1999-2014.

#include "StdAfx.h"
#include "CommentNodeAnimator.h"
#include "Nodes/TrackViewAnimNode.h"
#include "Nodes/TrackViewTrack.h"

CCommentNodeAnimator::CCommentNodeAnimator(CTrackViewAnimNode* pCommentNode)
{
	assert(pCommentNode);
	m_pCommentNode = pCommentNode;
}

CCommentNodeAnimator::~CCommentNodeAnimator()
{
	m_pCommentNode = 0;
}

void CCommentNodeAnimator::Animate(CTrackViewAnimNode* pNode, const SAnimContext& ac)
{
	if (pNode != m_pCommentNode || pNode->IsDisabled())
	{
		return;
	}

	CTrackViewTrackBundle tracks = pNode->GetAllTracks();

	int trackCount = tracks.GetCount();
	Vec2 pos(0, 0);
	for (int i = 0; i < trackCount; ++i)
	{
		CTrackViewTrack* pTrack = tracks.GetTrack(i);

		if (pTrack->IsMasked(ac.trackMask))
			continue;

		switch (pTrack->GetParameterType().GetType())
		{
		case eAnimParamType_CommentText:
			{
				AnimateCommentTextTrack(pTrack, ac);
			}
			break;
		case eAnimParamType_PositionX:
			{
				pos.x = stl::get<float>(pTrack->GetValue(ac.time));
			}
			break;
		case eAnimParamType_PositionY:
			{
				pos.y = stl::get<float>(pTrack->GetValue(ac.time));
			}
			break;
		}
	}

	// Position mapping from [0,100] to [-1,1]
	pos = (pos - Vec2(50.0f, 50.0f)) / 50.0f;
	m_commentContext.m_unitPos = pos;
}

void CCommentNodeAnimator::AnimateCommentTextTrack(CTrackViewTrack* pTrack, const SAnimContext& animContext)
{
	if (pTrack->GetKeyCount() == 0)
		return;

	CTrackViewKeyHandle keyHandle = GetActiveKeyHandle(pTrack, animContext.time);

	if (keyHandle.IsValid())
	{
		SCommentKey commentKey;

		keyHandle.GetKey(&commentKey);

		if (commentKey.m_duration > SAnimTime(0.0f) && animContext.time < keyHandle.GetTime() + commentKey.m_duration)
		{
			m_commentContext.m_strComment = commentKey.m_comment.c_str();
			cry_strcpy(m_commentContext.m_strFont, commentKey.m_font);
			m_commentContext.m_color = commentKey.m_color;
			m_commentContext.m_align = commentKey.m_align;
			m_commentContext.m_size = commentKey.m_size;
		}
		else
		{
			m_commentContext.m_strComment = 0;
		}
	}
	else
	{
		m_commentContext.m_strComment = 0;
	}
}

CTrackViewKeyHandle CCommentNodeAnimator::GetActiveKeyHandle(CTrackViewTrack* pTrack, SAnimTime fTime)
{
	const int nkeys = pTrack->GetKeyCount();

	if (nkeys == 0)
	{
		return CTrackViewKeyHandle();
	}

	CTrackViewKeyHandle& firstKeyHandle = pTrack->GetKey(0);

	// Time is before first key.
	if (firstKeyHandle.GetTime() > fTime)
	{
		return CTrackViewKeyHandle();
	}

	for (int i = 0; i < nkeys; i++)
	{
		CTrackViewKeyHandle& keyHandle = pTrack->GetKey(i);

		if (fTime >= keyHandle.GetTime())
		{
			if ((i == nkeys - 1) || (fTime < pTrack->GetKey(i + 1).GetTime()))
			{
				return keyHandle;
			}
		}
		else
		{
			break;
		}
	}

	return CTrackViewKeyHandle();
}

void CCommentNodeAnimator::Render(CTrackViewAnimNode* pNode, const SAnimContext& ac)
{
	if (!pNode->IsDisabled())
	{
		CCommentContext* cc = &m_commentContext;

		if (cc->m_strComment)
		{
			DrawText(cc->m_strFont, cc->m_size, cc->m_unitPos, cc->m_color, cc->m_strComment, cc->m_align);
		}
	}
}

Vec2 CCommentNodeAnimator::GetScreenPosFromNormalizedPos(const Vec2& unitPos)
{
	const CCamera& cam = gEnv->pSystem->GetViewCamera();
	float width = (float)cam.GetViewSurfaceX();
	int height = cam.GetViewSurfaceZ();

#pragma message("TODO")
	float fAspectRatio = 1.0f; //gViewportPreferences.defaultAspectRatio;
	float camWidth = height * fAspectRatio;

	float x = 0.5f * width + 0.5f * camWidth * unitPos.x;
	float y = 0.5f * height * (1.f - unitPos.y);

	return Vec2(x, y);
}

void CCommentNodeAnimator::DrawText(const char* szFontName, float fSize, const Vec2& unitPos, const ColorF col, const char* szText, int align)
{
	IFFont* pFont = gEnv->pCryFont->GetFont(szFontName);
	if (!pFont)
		pFont = gEnv->pCryFont->GetFont("default");

	if (pFont)
	{
		STextDrawContext ctx;
		ctx.SetSizeIn800x600(false);
		ctx.SetSize(Vec2(UIDRAW_TEXTSIZEFACTOR * fSize, UIDRAW_TEXTSIZEFACTOR * fSize));
		ctx.SetCharWidthScale(0.5f);
		ctx.SetProportional(false);
		ctx.SetFlags(align);

		// alignment
		Vec2 pos = GetScreenPosFromNormalizedPos(unitPos);

		if (align & eDrawText_Center)
			pos.x -= pFont->GetTextSize(szText, true, ctx).x * 0.5f;
		else if (align & eDrawText_Right)
			pos.x -= pFont->GetTextSize(szText, true, ctx).x;

		// Color
		ctx.SetColor(col);

		pFont->DrawString(pos.x, pos.y, szText, true, ctx);
	}
}

