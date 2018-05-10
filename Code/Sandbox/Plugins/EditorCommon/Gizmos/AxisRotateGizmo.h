// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IEditor.h" // for AxisConstrains and RefCoordSys
#include "Gizmo.h"
#include <memory>

struct DisplayContext;
struct HitContext;
struct IDisplayViewport;
class  CInteractionMode;

//////////////////////////////////////////////////////////////////////////
// Wheel Gizmo. 
// 
// Allows rotational movement around the axis of the gizmo
//////////////////////////////////////////////////////////////////////////
class EDITOR_COMMON_API CAxisRotateGizmo : public CGizmo
{
public:
	CAxisRotateGizmo();
	~CAxisRotateGizmo();

	//! set position - should be world space
	void SetPosition(Vec3 pos);
	const Vec3& GetPosition() const { return m_position; }
	//! set axis of the plane - should be world space
	void SetAxis(Vec3 axis);
	const Vec3& GetAxis() const { return m_axis; }
	//! set rgb color of the gizmo
	void SetColor(Vec3 color);
	//! set unique scale of the gizmo
	void SetScale(float scale);
	float GetScale() const { return m_scale; }
	//! set alignment axis for the gizmo - only relevant if interaction style is eQuarterCircle.
	//! It should be perpendicular to the axis!
	void SetAlignmentAxis(Vec3 axis);
	void SetScreenAligned(bool aligned) { m_bScreenAligned = aligned; }
	bool GetScreemAligned() const { return m_bScreenAligned; }

	virtual void Display(DisplayContext& dc) override;

	virtual bool MouseCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int nFlags) override;

	virtual void GetWorldBounds(AABB& bbox) override;

	virtual bool HitTest(HitContext& hc) override;

	// emitted when user starts dragging the gizmo
	CCrySignal <void(IDisplayViewport* view, CGizmo* gizmo, const CPoint& point, int nFlags)> signalBeginDrag;

	// emitted while dragging.
	CCrySignal <void(IDisplayViewport* view, CGizmo* gizmo, const AngleAxis& rotationAxis, const CPoint& point, int nFlags)> signalDragging;

	// emitted when finished dragging
	CCrySignal <void(IDisplayViewport* view, CGizmo* gizmo, const CPoint& point, int nFlags)> signalEndDrag;

private:
	// position and vectors defining the plane in world space
	Vec3 m_position;
	Vec3 m_axis;
	Vec3 m_alignment;
	Vec3 m_color;
	bool m_bScreenAligned;

	//! custom scale of widget - final size is calculated from z-distance of widget to camera and global parameters
	float m_scale;

	std::unique_ptr<CInteractionMode> m_interaction;
};


