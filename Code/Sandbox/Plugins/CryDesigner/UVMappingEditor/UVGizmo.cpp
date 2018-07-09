// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UVGizmo.h"
#include "QViewport.h"
#include "UVMappingEditorCommon.h"
#include "DisplayViewportAdapter.h"
#include "Grid.h"

namespace Designer {
namespace UVMapping
{

UVGizmo::UVGizmo() :
	m_pAxisHelper(new CAxisHelper),
	m_Pos(Vec3(0, 0, 0)),
	m_HighlightedAxis(0),
	m_bVisible(true)
{
	Vec3 red(1.0f, 0.0f, 0.0f);
	Vec3 green(0.0f, 1.0f, 0.0f);
	float scale = 0.02f;
	float arrowOffset = 0.08f * scale;

	m_mode = eGizmoMode_Translation;
	m_highlightedGizmo = nullptr;
	m_lastHitGizmo = nullptr;

	m_xTranslateGizmo.SetColor(red);
	m_xTranslateGizmo.SetScale(scale);
	m_xTranslateGizmo.SetOffset(arrowOffset);
	m_xTranslateGizmo.SetDirection(Vec3(-1.0f, 0.0f, 0.0f));

	m_yTranslateGizmo.SetColor(green);
	m_yTranslateGizmo.SetOffset(arrowOffset);
	m_yTranslateGizmo.SetScale(scale);
	m_yTranslateGizmo.SetDirection(Vec3(0.0f, -1.0f, 0.0f));

	m_xScaleGizmo.SetColor(red);
	m_xScaleGizmo.SetOffset(arrowOffset);
	m_xScaleGizmo.SetScale(scale);
	m_xScaleGizmo.SetDirection(Vec3(-1.0f, 0.0f, 0.0f));
	m_xScaleGizmo.SetUpAxis(Vec3(0.0f, 0.0f, 1.0f));

	m_yScaleGizmo.SetColor(green);
	m_yScaleGizmo.SetOffset(arrowOffset);
	m_yScaleGizmo.SetScale(scale);
	m_yScaleGizmo.SetDirection(Vec3(0.0f, -1.0f, 0.0f));
	m_yScaleGizmo.SetUpAxis(Vec3(0.0f, 0.0f, 1.0f));

	m_viewTranslateGizmo.SetScale(arrowOffset);
	m_uniformScaleGizmo.SetScale(arrowOffset);
	m_uniformScaleGizmo.SetXVector(Vec3(1.0f, 0.0f, 0.0f));
	m_uniformScaleGizmo.SetYVector(Vec3(0.0f, 1.0f, 0.0f));

	m_rotateGizmo.SetScale(scale);
	m_rotateGizmo.SetScreenAligned(true);
}

void UVGizmo::SetMode(EGizmoMode mode)
{
	m_mode = mode;
}

void UVGizmo::SetPos(const Vec3& pos) 
{ 
	m_Pos = pos;
	m_xTranslateGizmo.SetPosition(m_Pos);
	m_yTranslateGizmo.SetPosition(m_Pos);
	m_viewTranslateGizmo.SetPosition(m_Pos);

	m_xScaleGizmo.SetPosition(m_Pos);
	m_yScaleGizmo.SetPosition(m_Pos);
	m_uniformScaleGizmo.SetPosition(m_Pos);

	m_rotateGizmo.SetPosition(m_Pos);
}

void UVGizmo::MovePos(const Vec3& offset)
{ 
	SetPos(m_Pos + offset);
}

void UVGizmo::Draw(DisplayContext& dc)
{
	if (!m_bVisible)
		return;

	if (m_mode == eGizmoMode_Translation)
	{
		m_xTranslateGizmo.Display(dc);
		m_yTranslateGizmo.Display(dc);
		m_viewTranslateGizmo.Display(dc);
	}
	else if (m_mode == eGizmoMode_Scale)
	{
		m_xScaleGizmo.Display(dc);
		m_yScaleGizmo.Display(dc);
		m_uniformScaleGizmo.Display(dc);
	}
	else if (m_mode == eGizmoMode_Rotation)
	{
		m_rotateGizmo.Display(dc);
	}
}

bool UVGizmo::HitTest(QViewport* viewport, int x, int y, int& axis)
{
	if (!m_bVisible)
		return false;

	HitContext hc;
	CDisplayViewportAdapter viewAdapter(viewport);
	hc.view = &viewAdapter;
	hc.camera = viewport->Camera();
	hc.point2d = CPoint(x, y);
	viewAdapter.ViewToWorldRay(hc.point2d, hc.raySrc, hc.rayDir);

	Matrix34 tm = Matrix34::CreateIdentity();
	tm.SetTranslation(m_Pos);

	if (m_mode == eGizmoMode_Translation)
	{
		if (m_viewTranslateGizmo.HitTest(hc))
		{
			axis = AXIS_XY;
			m_lastHitGizmo = &m_viewTranslateGizmo;
			return true;
		}
		if (m_xTranslateGizmo.HitTest(hc))
		{
			axis = AXIS_X;
			m_lastHitGizmo = &m_xTranslateGizmo;
			return true;
		}
		if (m_yTranslateGizmo.HitTest(hc))
		{
			axis = AXIS_Y;
			m_lastHitGizmo = &m_yTranslateGizmo;
			return true;
		}
	}
	else if (m_mode == eGizmoMode_Scale)
	{
		if (m_uniformScaleGizmo.HitTest(hc))
		{
			axis = AXIS_XY;
			m_lastHitGizmo = &m_uniformScaleGizmo;
			return true;
		}
		if (m_xScaleGizmo.HitTest(hc))
		{
			axis = AXIS_X;
			m_lastHitGizmo = &m_xScaleGizmo;
			return true;
		}
		if (m_yScaleGizmo.HitTest(hc))
		{
			axis = AXIS_Y;
			m_lastHitGizmo = &m_yScaleGizmo;
			return true;
		}
	}
	else if (m_mode == eGizmoMode_Rotation)
	{
		if (m_rotateGizmo.HitTest(hc))
		{
			axis = AXIS_XY;
			m_lastHitGizmo = &m_rotateGizmo;
			return true;
		}
	}

	return false;
}

void UVGizmo::OnViewportMouse(const SMouseEvent& ev_)
{
	SMouseEvent ev = ev_;
	if (!ev.viewport || !m_bVisible)
		return;

	int axis;
	bool bHit = HitTest(ev.viewport, ev.x, ev.y, axis);

	if (ev.type == SMouseEvent::TYPE_PRESS && ev.button == SMouseEvent::BUTTON_LEFT)
	{
		if (bHit)
		{
			m_Context.pos = SearchUtil::FindHitPointToUVPlane(ev.viewport, ev.x, ev.y);

			// hack to convert from flags to mode
			if (m_mode == eGizmoMode_Translation)
			{
				m_Context.mode = CAxisHelper::MOVE_MODE;
			}
			else if (m_mode == eGizmoMode_Rotation)
			{
				m_Context.mode = CAxisHelper::ROTATE_CIRCLE_MODE;
			}
			else if (m_mode == eGizmoMode_Scale)
			{
				m_Context.mode = CAxisHelper::SCALE_MODE;
			}

			m_Context.axis = axis;
			m_Context.mouse_x = m_Context.mouse_down_x = ev.x;
			m_Context.mouse_y = m_Context.mouse_down_y = ev.y;
			m_Context.radian = 0;
			m_bStartedDragging = true;

			for (auto ii = m_GizmoTransformCallbacks.begin(); ii != m_GizmoTransformCallbacks.end(); ++ii)
				(*ii)->OnGizmoLMBDown(m_Context.mode);
		}
	}

	if (ev.type == SMouseEvent::TYPE_RELEASE && ev.button == SMouseEvent::BUTTON_LEFT)
	{
		m_bStartedDragging = false;

		for (auto ii = m_GizmoTransformCallbacks.begin(); ii != m_GizmoTransformCallbacks.end(); ++ii)
			(*ii)->OnGizmoLMBUp(m_Context.mode);
	}

	if (ev.type == SMouseEvent::TYPE_MOVE)
	{
		if (m_bStartedDragging)
		{
			Vec3 pos = SearchUtil::FindHitPointToUVPlane(ev.viewport, ev.x, ev.y);
			Vec3 offset(0, 0, 0);

			if (m_Context.mode == CAxisHelper::MOVE_MODE)
			{
				offset = pos - m_Context.pos;
				if (m_Context.axis == AXIS_X)
					offset.y = 0;
				else if (m_Context.axis == AXIS_Y)
					offset.x = 0;
			}
			else if (m_Context.mode == CAxisHelper::ROTATE_CIRCLE_MODE)
			{
				float radian = 0;
				Vec3 vDir0 = Vec3(0, 0, 0);
				Vec3 vDir1 = Vec3(0, 0, 0);
				
				if (gSnappingPreferences.angleSnappingEnabled())
				{
					Vec3 v0 = SearchUtil::FindHitPointToUVPlane(ev.viewport, m_Context.mouse_down_x, m_Context.mouse_down_y);
					Vec3 v1 = SearchUtil::FindHitPointToUVPlane(ev.viewport, ev.x, ev.y);
					vDir0 = (v0 - m_Pos).GetNormalized();
					vDir1 = (v1 - m_Pos).GetNormalized();
					radian = std::acos(vDir0.Dot(vDir1));
					radian = DEG2RAD(gSnappingPreferences.SnapAngle(RAD2DEG(radian)));
					float prev_radian = m_Context.radian;
					m_Context.radian = radian;
					radian = radian - prev_radian;
				}
				else if (m_Context.mouse_x != ev.x || m_Context.mouse_y != ev.y)
				{
					Vec3 v0 = SearchUtil::FindHitPointToUVPlane(ev.viewport, m_Context.mouse_x, m_Context.mouse_y);
					Vec3 v1 = SearchUtil::FindHitPointToUVPlane(ev.viewport, ev.x, ev.y);
					vDir0 = (v0 - m_Pos).GetNormalized();
					vDir1 = (v1 - m_Pos).GetNormalized();
					if (!Comparison::IsEquivalent(vDir0, vDir1))
						radian = std::acos(vDir0.Dot(vDir1));
				}

				if (radian != 0)
				{
					Vec3 vCurlDir = vDir0.Cross(vDir1).GetNormalized();
					if (vCurlDir.Dot(Vec3(0, 0, 1)) > 0)
						offset = Vec3(radian, 0, 0);
					else
						offset = Vec3(-radian, 0, 0);
				}
			}
			else if (m_Context.mode == CAxisHelper::SCALE_MODE)
			{
				offset = Vec3(1.0f + 0.01f * (ev.x - m_Context.mouse_x), 1.0f - 0.01f * (ev.y - m_Context.mouse_y), 1.0f);
				if (m_Context.axis == AXIS_X)
					offset.y = 1.0f;
				else if (m_Context.axis == AXIS_Y)
					offset.x = 1.0f;
				else
					offset.x = offset.y;
			}

			for (auto ii = m_GizmoTransformCallbacks.begin(); ii != m_GizmoTransformCallbacks.end(); ++ii)
				(*ii)->OnTransformGizmo(m_Context.mode, offset);

			m_Context.pos = pos;
			m_Context.mouse_x = ev.x;
			m_Context.mouse_y = ev.y;
		}
		else
		{
			if (m_highlightedGizmo && m_highlightedGizmo != m_lastHitGizmo)
			{
				m_highlightedGizmo->SetHighlighted(false);
				m_highlightedGizmo = nullptr;
			}

			if (bHit)
			{
				m_highlightedGizmo = m_lastHitGizmo;
				m_highlightedGizmo->SetHighlighted(true);
			}
		}
	}
}

void UVGizmo::AddCallback(IGizmoTransform* pGizmoTransform)
{
	m_GizmoTransformCallbacks.push_back(pGizmoTransform);
}

void UVGizmo::RemoveCallback(IGizmoTransform* pGizmoTransform)
{
	auto ii = m_GizmoTransformCallbacks.begin();
	for (; ii != m_GizmoTransformCallbacks.end(); )
	{
		if (pGizmoTransform == *ii)
			ii = m_GizmoTransformCallbacks.erase(ii);
		else
			++ii;
	}
}

}
}

