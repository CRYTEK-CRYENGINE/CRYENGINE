// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Header File.
// Copyright (C), Crytek, 1999-2014.

#pragma once

#include "Gizmos/Gizmo.h"

struct DisplayContext;
class CTrackViewAnimNode;
class CTrackViewTrack;

// Gizmo of Objects animation track.
class CTrackGizmo : public CGizmo
{
public:
	CTrackGizmo();
	~CTrackGizmo();

	// Overrides from CGizmo
	virtual void GetWorldBounds(AABB& bbox);
	virtual void Display(DisplayContext& dc);
	virtual bool HitTest(HitContext& hc);
	// TODO implement this
	virtual bool MouseCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int nFlags) { return false; }

	void         SetMatrix(const Matrix34& tm);
	void         SetAnimNode(CTrackViewAnimNode* pNode);
	void         DrawAxis(DisplayContext& dc, const Vec3& pos);
	void         DrawKeys(DisplayContext& dc, CTrackViewTrack* pTrack, CTrackViewTrack* pKeysTrack);

private:
	mutable Matrix34 m_matrix;
	CTrackViewAnimNode* m_pAnimNode;
	AABB                m_worldBbox;
};

