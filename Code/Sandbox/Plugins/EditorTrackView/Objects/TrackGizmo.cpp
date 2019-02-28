// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Source File.
// Copyright (C), Crytek, 1999-2014.

#include "StdAfx.h"
#include "TrackGizmo.h"
#include "Objects/DisplayContext.h"
#include "Nodes/TrackViewAnimNode.h"
#include "AnimationContext.h"
#include "TrackViewPlugin.h"

#include "Viewport.h"

#include <CryMovie/IMovieSystem.h>

namespace
{
const int kHighlightAxis = 0;
const float kTrackDrawZOffset = 0.01f;
const float kAxisSize = 0.1f;
}

CTrackGizmo::CTrackGizmo()
{
	m_pAnimNode = 0;

	m_worldBbox.min = Vec3(-10000, -10000, -10000);
	m_worldBbox.max = Vec3(10000, 10000, 10000);
	m_matrix.SetIdentity();
}

CTrackGizmo::~CTrackGizmo()
{
}

void CTrackGizmo::SetMatrix(const Matrix34& tm)
{
	m_matrix = tm;
	m_worldBbox.min = Vec3(-10000, -10000, -10000);
	m_worldBbox.max = Vec3(10000, 10000, 10000);
}

void CTrackGizmo::Display(DisplayContext& dc)
{
	if (!(dc.flags & DISPLAY_TRACKS))
	{
		return;
	}

	if (!m_pAnimNode)
	{
		return;
	}

#pragma message("TODO")
	uint32 hideMask = 0; //gSettings.objectHideMask;

	CAnimationContext* ac = CTrackViewPlugin::GetAnimationContext();

	// Should have animation sequence.
	if (!ac->GetSequence())
	{
		return;
	}

	// Must have non empty position track.
	CTrackViewTrack* pTrack = m_pAnimNode->GetTrackForParameter(eAnimParamType_Position);

	if (!pTrack)
	{
		return;
	}

	int nkeys = pTrack->GetKeyCount();

	if (nkeys < 2)
	{
		return;
	}

	TRange<SAnimTime> range = ac->GetTimeRange();
	SAnimTime step(0.1f);

	bool bTicks = (dc.flags & DISPLAY_TRACKTICKS) == DISPLAY_TRACKTICKS;

	// Get Spline color.
	ColorF splineCol(0.5f, 0.3f, 1, 1);
	ColorF timeCol(0, 1, 0, 1);

	m_worldBbox.Reset();

	float zOffset = kTrackDrawZOffset;
	Vec3 p0(0, 0, 0), p1(0, 0, 0);
	Vec3 tick(0, 0, 0.05f);
	p0 = stl::get<Vec3>(pTrack->GetValue(range.start));
	p0 = m_matrix * p0;
	p0.z += zOffset;

	// Update bounding box.
	m_worldBbox.Add(p0);

	for (SAnimTime t = range.start + step; t < range.end; t += step)
	{
		p1 = stl::get<Vec3>(pTrack->GetValue(t));
		p1 = m_matrix * p1;
		p1.z += zOffset;

		// Update bounding box.
		m_worldBbox.Add(p1);

		if (bTicks)
		{
			dc.DrawLine(p0 - tick, p0 + tick, timeCol, timeCol);
		}

		dc.DrawLine(p0, p1, splineCol, splineCol);
		p0 = p1;
	}

	int nSubTracks = pTrack->GetChildCount();

	if (nSubTracks > 0)
	{
		for (int i = 0; i < nSubTracks; i++)
		{
			DrawKeys(dc, pTrack, static_cast<CTrackViewTrack*>(pTrack->GetChild(i)));
		}
	}
	else
	{
		DrawKeys(dc, pTrack, pTrack);
	}
}

void CTrackGizmo::SetAnimNode(CTrackViewAnimNode* pNode)
{
	m_pAnimNode = pNode;
}

void CTrackGizmo::GetWorldBounds(AABB& bbox)
{
	bbox = m_worldBbox;
}

void CTrackGizmo::DrawAxis(DisplayContext& dc, const Vec3& org)
{
	float size = kAxisSize;

	dc.DepthTestOff();

	Vec3 x(size, 0, 0);
	Vec3 y(0, size, 0);
	Vec3 z(0, 0, size);

	float fScreenScale = dc.view->GetScreenScaleFactor(org);
	x = x * fScreenScale;
	y = y * fScreenScale;
	z = z * fScreenScale;

	float col[4] = { 1, 1, 1, 1 };
	float hcol[4] = { 1, 0, 0, 1 };
	IRenderAuxText::DrawLabelEx(org + x, 1.2f, col, true, true, "X");
	IRenderAuxText::DrawLabelEx(org + y, 1.2f, col, true, true, "Y");
	IRenderAuxText::DrawLabelEx(org + z, 1.2f, col, true, true, "Z");

	Vec3 colX(1, 0, 0), colY(0, 1, 0), colZ(0, 0, 1);

	if (kHighlightAxis)
	{
		float col[4] = { 1, 0, 0, 1 };

		if (kHighlightAxis == 1)
		{
			colX(1, 1, 0);
			IRenderAuxText::DrawLabelEx(org + x, 1.2f, col, true, true, "X");
		}

		if (kHighlightAxis == 2)
		{
			colY(1, 1, 0);
			IRenderAuxText::DrawLabelEx(org + y, 1.2f, col, true, true, "Y");
		}

		if (kHighlightAxis == 3)
		{
			colZ(1, 1, 0);
			IRenderAuxText::DrawLabelEx(org + z, 1.2f, col, true, true, "Z");
		}
	}

	x = x * 0.8f;
	y = y * 0.8f;
	z = z * 0.8f;
	float fArrowScale = fScreenScale * 0.07f;
	dc.SetColor(colX);
	dc.DrawArrow(org, org + x, fArrowScale);
	dc.SetColor(colY);
	dc.DrawArrow(org, org + y, fArrowScale);
	dc.SetColor(colZ);
	dc.DrawArrow(org, org + z, fArrowScale);

	dc.DepthTestOn();
}

bool CTrackGizmo::HitTest(HitContext& hc)
{
	return false;
}

void CTrackGizmo::DrawKeys(DisplayContext& dc, CTrackViewTrack* pTrack, CTrackViewTrack* pKeysTrack)
{
	// Get Key color.
	dc.SetColor(1, 0, 0, 1);

	float zOffset = kTrackDrawZOffset;

	float sz = 0.2f;
	int nkeys = pKeysTrack->GetKeyCount();

	for (int i = 0; i < nkeys; i++)
	{
		CTrackViewKeyHandle& keyHandle = pKeysTrack->GetKey(i);

		const SAnimTime t = keyHandle.GetTime();
		Vec3 p0 = stl::get<Vec3>(pTrack->GetValue(t));
		p0 = m_matrix * p0;
		p0.z += zOffset;

		float sz = 0.005f * dc.view->GetScreenScaleFactor(p0);

		// Draw quad.
		dc.DrawWireBox(p0 - Vec3(sz, sz, sz), p0 + Vec3(sz, sz, sz));

		if (keyHandle.IsSelected())
		{
			DrawAxis(dc, p0);
			dc.SetColor(1, 0, 0, 1);
		}
	}
}

