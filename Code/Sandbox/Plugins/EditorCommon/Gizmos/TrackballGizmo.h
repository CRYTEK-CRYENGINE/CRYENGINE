// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include "Gizmo.h"
#include <CrySandbox/CrySignal.h>

// Free Rotation (Trackball) gizmo
class EDITOR_COMMON_API CTrackballGizmo : public CGizmo
{
public:
	CTrackballGizmo();

	//! set position - should be world space
	void         SetPosition(Vec3 pos);
	//! set rgb color of the gizmo
	void         SetColor(Vec3 color);
	//! set unique scale of the gizmo
	void         SetScale(float scale);

	virtual void Display(SDisplayContext& dc) override;

	virtual bool MouseCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int nFlags) override;

	virtual void GetWorldBounds(AABB& bbox) override;

	virtual bool HitTest(HitContext& hc) override;

	// emitted when user starts dragging the gizmo
	CCrySignal<void(IDisplayViewport* view, CGizmo* gizmo, const CPoint& point, int nFlags)> signalBeginDrag;

	// emitted while dragging.
	CCrySignal<void(IDisplayViewport* view, CGizmo* gizmo, const AngleAxis& rotationAxis, const CPoint& point, int nFlags)> signalDragging;

	// emitted when finished dragging
	CCrySignal<void(IDisplayViewport* view, CGizmo* gizmo, const CPoint& point, int nFlags)> signalEndDrag;
private:
	// position and vectors defining the plane in world space
	Vec3 m_position;
	Vec3 m_color;

	//! custom scale of widget - final size is calculated from z-distance of widget to camera and global parameters
	float m_scale;

	Vec2  m_initOffset;
	float m_rotatedX;
	float m_rotatedY;
};
