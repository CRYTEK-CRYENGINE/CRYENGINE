// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IEditor.h" // for AxisConstrains and RefCoordSys
#include "Gizmo.h"

struct DisplayContext;
struct HitContext;
struct IDisplayViewport;

//////////////////////////////////////////////////////////////////////////
// CViewTranslateGizmo Gizmo. 
// 
// Allows view space movement
//////////////////////////////////////////////////////////////////////////
class EDITOR_COMMON_API CViewTranslateGizmo : public CGizmo
{
public:
	CViewTranslateGizmo();
	~CViewTranslateGizmo();

	//! set position - should be world space
	void SetPosition(Vec3 pos);
	//! set rgb color of the gizmo
	void SetColor(Vec3 color);
	//! set unique scale of the gizmo
	void SetScale(float scale);

	virtual void Display(DisplayContext& dc) override;

	virtual bool MouseCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int nFlags) override;

	virtual void GetWorldBounds(AABB& bbox) override;

	virtual bool HitTest(HitContext& hc) override;

	// emitted when user starts dragging the gizmo
	CCrySignal <void(IDisplayViewport* view, CGizmo* gizmo, const Vec3& initialPosition, const CPoint& point, int nFlags)> signalBeginDrag;

	// emitted while dragging.
	CCrySignal <void(IDisplayViewport* view, CGizmo* gizmo, const Vec3& offset, const CPoint& point, int nFlags)> signalDragging;

	// emitted when finished dragging
	CCrySignal <void(IDisplayViewport* view, CGizmo* gizmo, const CPoint& point, int nFlags)> signalEndDrag;
private:
	// position and vectors defining the plane in world space
	Vec3 m_position;
	Vec3 m_color;

	//! custom scale of widget - final size is calculated from z-distance of widget to camera and global parameters
	float m_scale;

	Vec3 m_initOffset;
	Vec3 m_initPosition;
	Vec3 m_interactionOffset;
};


