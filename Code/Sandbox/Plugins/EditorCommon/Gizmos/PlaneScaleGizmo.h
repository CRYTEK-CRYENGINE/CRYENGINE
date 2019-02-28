// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IEditor.h" // for AxisConstrains and RefCoordSys
#include "Gizmo.h"

struct DisplayContext;
struct HitContext;
struct IDisplayViewport;

//////////////////////////////////////////////////////////////////////////
// CPlaneScaleGizmo Gizmo. 
// 
// Allows constrained movement on the plane of the gizmo
//////////////////////////////////////////////////////////////////////////
class EDITOR_COMMON_API CPlaneScaleGizmo : public CGizmo
{
public:
	CPlaneScaleGizmo();
	~CPlaneScaleGizmo();

	//! set position - should be world space
	void SetPosition(Vec3 pos);
	//! set x vector of the plane - should be world space
	void SetXVector(Vec3 dir);
	//! set y vector of the plane - should be world space
	void SetYVector(Vec3 dir);
	//! set rgb color of the gizmo
	void SetColor(Vec3 color);
	//! set unique scale of the gizmo
	void SetScale(float scale);
	//! set x offset from position along the length of the x vector
	void SetOffsetInner(float offset);
	//! set y offset from position along the length of the x vector
	void SetOffsetFromCenter(float offset);

	virtual void Display(DisplayContext& dc) override;

	virtual bool MouseCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int nFlags) override;

	virtual void GetWorldBounds(AABB& bbox) override;

	virtual bool HitTest(HitContext& hc) override;

	// emitted when user starts dragging the gizmo
	CCrySignal <void(IDisplayViewport* view, CGizmo* gizmo, const CPoint& point, int nFlags)> signalBeginDrag;

	// emitted while dragging.
	CCrySignal <void(IDisplayViewport* view, CGizmo* gizmo, float scale, const CPoint& point, int nFlags)> signalDragging;

	// emitted when finished dragging
	CCrySignal <void(IDisplayViewport* view, CGizmo* gizmo, const CPoint& point, int nFlags)> signalEndDrag;

private:
	void DrawPlane(DisplayContext& dc, Vec3 position);

	// position and vectors defining the plane in world space
	Vec3 m_position;
	Vec3 m_xVector;
	Vec3 m_yVector;
	Vec3 m_color;

	//! custom scale of widget - final size is calculated from z-distance of widget to camera and global parameters
	float m_scale;
	float m_offsetInner;
	float m_offsetFromCenter;

	//interaction data;
	float m_interactionScale;
	float m_initLen;
};


