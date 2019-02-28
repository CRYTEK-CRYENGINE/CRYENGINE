// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Gizmos/AxisTranslateGizmo.h"
#include "Gizmos/AxisScaleGizmo.h"
#include "Gizmos/UniformScaleGizmo.h"
#include "Gizmos/ViewTranslateGizmo.h"
#include "Gizmos/AxisRotateGizmo.h"
#include "Gizmos/AxisHelper.h"
#include "QViewportConsumer.h"

class QViewport;
class UVGizmo;

namespace Designer {
namespace UVMapping
{

class IGizmoTransform
{
public:
	virtual void OnTransformGizmo(int mode, const Vec3& offset) = 0;
	virtual void OnGizmoLMBDown(int mode) = 0;
	virtual void OnGizmoLMBUp(int mode) = 0;
};

struct GizmoDraggingContext
{
	int   axis;
	int   mode;
	Vec3  pos;
	int   mouse_x, mouse_y;
	int   mouse_down_x, mouse_down_y;
	float radian;
};

enum EGizmoMode
{
	eGizmoMode_Translation,
	eGizmoMode_Rotation,
	eGizmoMode_Scale,
	eGizmoMode_Invalid
};

class UVGizmo : public _i_reference_target_t, public QViewportConsumer
{
public:
	UVGizmo();

	void        AddCallback(IGizmoTransform* pGizmoTransform);
	void        RemoveCallback(IGizmoTransform* pGizmoTransform);

	void        SetPos(const Vec3& pos);
	void        MovePos(const Vec3& offset);
	const Vec3& GetPos() const   { return m_Pos;    }
	void        Hide(bool bHide) { m_bVisible = !bHide; }
	void        SetMode(EGizmoMode mode);
	void        Draw(DisplayContext& dc);
	bool        HitTest(QViewport* viewport, int x, int y, int& axis);

	void        OnViewportMouse(const SMouseEvent& ev) override;

private:
	int                           m_HighlightedAxis;
	Vec3                          m_Pos;
	std::unique_ptr<CAxisHelper>  m_pAxisHelper;
	bool                          m_bStartedDragging;
	bool                          m_bVisible;
	GizmoDraggingContext          m_Context;

	std::vector<IGizmoTransform*> m_GizmoTransformCallbacks;

	CAxisTranslateGizmo m_xTranslateGizmo;
	CAxisTranslateGizmo m_yTranslateGizmo;
	CViewTranslateGizmo m_viewTranslateGizmo;

	CAxisScaleGizmo     m_xScaleGizmo;
	CAxisScaleGizmo     m_yScaleGizmo;
	CUniformScaleGizmo  m_uniformScaleGizmo;

	CAxisRotateGizmo    m_rotateGizmo;

	CGizmo*             m_highlightedGizmo;
	CGizmo*             m_lastHitGizmo;

	EGizmoMode          m_mode;
};

typedef _smart_ptr<UVGizmo> UVGizmoPtr;

}
}

