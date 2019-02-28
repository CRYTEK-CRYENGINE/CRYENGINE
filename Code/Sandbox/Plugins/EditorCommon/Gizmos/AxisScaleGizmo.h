// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IEditor.h" // for AxisConstrains and RefCoordSys
#include "Gizmo.h"

struct DisplayContext;
struct HitContext;
struct IDisplayViewport;
class CViewport;

//////////////////////////////////////////////////////////////////////////
// CAxisScaleGizmo Gizmo. 
// 
// Interacts with an axis by squishing along the axis
//////////////////////////////////////////////////////////////////////////
class EDITOR_COMMON_API CAxisScaleGizmo : public CGizmo
{
public:
	CAxisScaleGizmo();
	~CAxisScaleGizmo();

	virtual void        SetName(const char* sName) override;
	virtual const char* GetName() override;

	//! set position - should be world space
	void SetPosition(Vec3 pos);
	//! set direction - should be world space
	void SetDirection(Vec3 dir);
	//! set up direction to orient the box at the end of the gizmo
	void SetUpAxis(Vec3 dir);
	//! set rgb color of the gizmo
	void SetColor(Vec3 color);
	//! set offset from central position as factor of the gizmo length
	void SetOffset(float offset);
	//! set unique scale of the gizmo
	void SetScale(float scale);

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
	void DrawSquishBox(DisplayContext& dc, Vec3 position);

	// position and direction in world space
	Vec3 m_position;
	Vec3 m_direction;
	Vec3 m_upAxis;
	Vec3 m_color;

	string m_name;

	//! custom scale of widget - final size is calculated from z-distance of widget to camera and global parameters
	float m_scale;
	//! offset from position along the axis (as widget length factor but will also be scaled before use)
	float m_offset;

	//! runtime data for the scale
	float m_scaleInteraction;
	float m_origLenInteraction;
	float m_origScale;
};


