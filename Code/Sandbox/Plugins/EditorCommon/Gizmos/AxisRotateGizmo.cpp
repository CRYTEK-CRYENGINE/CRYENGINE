// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AxisRotateGizmo.h"
#include "IDisplayViewport.h"
#include "Grid.h"
#include "CryMath/Cry_Math.h"
#include "Objects/DisplayContext.h"
#include "Gizmos/AxisHelper.h"
#include "QtUtil.h"

////////////////////////////////////////////////////
// interaction modes for rotation gizmo
////////////////////////////////////////////////////

namespace Private_AxisRotateGizmo
{
	float GetDistanceBetweenLines(const Vec3& p1, const Vec3& dir1, const Vec3& p2, const Vec3& dir2)
	{
		const Vec3 cross = dir2.Cross(dir1);
		return cross.Dot(p2 - p1);
	}

	bool IsWithinRing(const Ray& ray, const Vec3& viewDir, const Vec3& pos, float outerRadius, float radius = 0)
	{
		const float rayAxisDot = ray.direction * viewDir;
		const float radiusSq = radius * radius;
		const float radiusOuterSq = outerRadius * outerRadius;

		const Vec3 dp = ray.origin - pos;
		const Vec3 isect = ray.origin - (dp * viewDir) * ray.direction / rayAxisDot;

		const float distSq = (isect - pos).len2();
		return distSq > radiusSq && distSq < radiusOuterSq;
	}
}

class CInteractionMode
{
public:
	CInteractionMode(const CAxisRotateGizmo& gizmo)
		: m_initPosition(gizmo.GetPosition())
		, m_initAxis(gizmo.GetAxis())
		, m_angle(0.0f)
		, m_bScreenAligned(gizmo.GetScreemAligned())
	{
	}

	virtual ~CInteractionMode() {};

	//! Initialize data for interaction. The reason this is done as separate step and not in constructor is that we need
	//! a boolean return value for cases where we can't interact (mouse too close to gizmo center, for instance)
	virtual bool Initialize(POINT point, IDisplayViewport* view) = 0;

	//! Calculates the angle value which can later be retrieved by the GetAngle method. Returns success or failure in case
	//! of division by zero errors or similar.
	virtual bool Interact(IDisplayViewport* view, POINT point, AngleAxis& rotation, bool bUseMultiplier) = 0;

	//! Get initial position of the gizmo
	Vec3& GetInitialPosition() { return m_initPosition; }

	//! Get cursor position in 3D world. This lies on the same plane as the gizmo center, aligned to the view direction
	Vec3& GetCursorPosition() { return m_cursorPosition; }

	//! Get the angle calculated during the Interact call
	float GetAngle() { return m_angle; }

	virtual void DrawCursor(DisplayContext& dc, float scale) = 0;

	//! Initialization utility function, stores 2D offset in screen space. Also useful for screen aligned rotation
	bool InitializeScreenSpaceOffset(POINT point, IDisplayViewport* view)
	{
		POINT initPoint = view->WorldToView(m_initPosition);

		// x/y projections on initial direction to determine atan2
		int x = point.x - initPoint.x;
		int y = point.y - initPoint.y;

		if (x * x + y * y == 0)
		{
			return false;
		}

		m_initScreenOffest.x = x;
		m_initScreenOffest.y = y;
		m_initScreenOffest.Normalize();

		return true;
	}

	//! Create a matrix whose y axis coincides with the initial direction where user started dragging from.
	//! This allows us to simply draw disks and arks as seen from a frame of reference local to the rotation
	Matrix34 CreateRotationFrameMatrix(IDisplayViewport* view)
	{
		if (m_bScreenAligned)
		{
			Vec3 zVec = view->CameraToWorld(m_initPosition);
			zVec.Normalize();
			Vec3 xVecTmp = zVec ^ view->UpViewDirection();
			xVecTmp.Normalize();
			Vec3 yVecTmp = zVec ^ xVecTmp;

			float cosangle = m_initScreenOffest.x;
			float sinangle = m_initScreenOffest.y;

			Vec3 yVec = cosangle * xVecTmp + sinangle * yVecTmp;
			Vec3 xVec = -sinangle * xVecTmp + cosangle * yVecTmp;

			return Matrix34::CreateFromVectors(xVec, yVec, zVec, m_initPosition);
		}
		else
		{
			return Matrix34::CreateFromVectors(m_initAxis ^ m_initDirection, m_initDirection, m_initAxis, m_initPosition);
		}
	}

protected:
	//! Initial gizmo position/axis. We need to store this and calculate interaction against these,
	//! in case the position of the gizmo changes during interaction
	//! (for instance, transforming a group of objects and the calculated gizmo position changes)
	Vec3  m_initPosition;
	Vec3  m_initAxis;

	//! Direction from which we start rotating, lies on the gizmo plane.
	Vec3  m_initDirection;

	//! offset of mouse from the initial position, projected on the view plane.
	Vec3  m_cursorPosition;

	//! computed interaction angle
	float m_angle;
	bool  m_bScreenAligned;

	//! pixel space offset from center of gizmo. Useful to calculate screen aligned gizmo interaction
	Vec2  m_initScreenOffest;
};

//! Dial style interaction, rotate mouse around the center of the gizmo in screen space to change the rotation value
class CDialInteraction : public CInteractionMode
{
public:
	CDialInteraction(const CAxisRotateGizmo& gizmo)
		: CInteractionMode(gizmo)
	{
		QtUtil::HideApplicationMouse();
	}

	~CDialInteraction()
	{
		QtUtil::ShowApplicationMouse();
	}

	virtual bool Initialize(POINT point, IDisplayViewport* view) override
	{
		if (!InitializeScreenSpaceOffset(point, view))
		{
			return false;
		}

		Ray ray;
		view->ViewToWorldRay(point, ray.origin, ray.direction);
		Plane p = Plane::CreatePlane(view->ViewDirection(), m_initPosition);
		Intersect::Ray_Plane(ray, p, m_cursorPosition, false);

		m_revolutions = 0;

		// find the world space vector on the gizmo plane
		if (!m_bScreenAligned)
		{
			Plane p = Plane::CreatePlane(m_initAxis, m_initPosition);
			if (!Intersect::Ray_Plane(ray, p, m_initDirection, false))
			{
				return false;
			}

			m_initDirection -= m_initPosition;
			m_initDirection.Normalize();
		}

		return true;
	}

	//! Finds intersection of ray on view aligned plane passing through gizmo position.
	//! helps with constant size drawing of mouse indicator
	void CalculateCursorPosition(IDisplayViewport* view, POINT point)
	{
		Vec3 vDir = view->ViewDirection();

		Ray ray;
		view->ViewToWorldRay(point, ray.origin, ray.direction);
		Plane p = Plane::CreatePlane(vDir, m_initPosition);
		Intersect::Ray_Plane(ray, p, m_cursorPosition, false);
	}

	virtual bool Interact(IDisplayViewport* view, POINT point, AngleAxis& rotation, bool bUseMultiplier) override
	{
		CalculateCursorPosition(view, point);

		// Ray in world space.
		Vec3 camToOrigin = view->CameraToWorld(m_initPosition);
		Vec3 axis = (m_bScreenAligned ? camToOrigin : m_initAxis);
		axis.Normalize();

		// x/y projections on initial direction to determine atan2
		POINT initPoint = view->WorldToView(m_initPosition);
		Vec2 screenVec(point.x - initPoint.x, point.y - initPoint.y);
		Vec2 perpendicularVec(-m_initScreenOffest.y, m_initScreenOffest.x);

		float x = screenVec * m_initScreenOffest;
		float y = screenVec * perpendicularVec;

		if (x * x + y * y == 0)
		{
			return false;
		}

		float prevLocalAngle = m_localAngle;
		m_localAngle = atan2(y, x);

		// check for change of sign. If previous angle was less than pi / 2, then we are doing a roundabout and should actually add a revolution
		if ((m_localAngle * prevLocalAngle < 0.0f) && (abs(m_localAngle) > g_PI / 2.0f))
		{
			if (abs(m_localAngle) > g_PI / 2.0f)
			{
				if (m_localAngle < 0.0f)
				{
					++m_revolutions;
				}
				else
				{
					--m_revolutions;
				}
			}
		}

		if (gSnappingPreferences.angleSnappingEnabled())
		{
			float angle = RAD2DEG(m_localAngle);
			m_localAngle = DEG2RAD(gSnappingPreferences.SnapAngle(angle));
		}

		m_angle = m_localAngle + m_revolutions * g_PI2;

		if (axis * camToOrigin < 0.0f)
		{
			m_angle = -m_angle;
		}

		rotation.angle = m_angle;
		rotation.axis = axis;
		return true;
	}

	virtual void DrawCursor(DisplayContext& dc, float scale) override
	{
		Vec3 cursorDir;
		cursorDir = ((m_cursorPosition - m_initPosition) ^ dc.view->CameraToWorld(m_cursorPosition)).GetNormalized() * 0.3 * scale;
		dc.SetColor(0.0f, 0.0f, 0.0f);
		dc.DrawArrow(m_cursorPosition, m_cursorPosition + cursorDir, 0.4f * scale);
		dc.DrawArrow(m_cursorPosition, m_cursorPosition - cursorDir, 0.4f * scale);
	}

private:
	float m_localAngle;
	int   m_revolutions;
};

//! Dial style interaction, rotate mouse around the center of the gizmo in screen space to change the rotation value
class CLinearInteraction : public CInteractionMode
{
public:
	CLinearInteraction(const CAxisRotateGizmo& gizmo, IDisplayViewport* view)
		: CInteractionMode(gizmo)
		, m_scale(gizmo.GetScale())
		, m_view(view)
	{
		QPixmap object_rotate_pixmap = QPixmap(":/icons/Viewport/object_rotate.png");
		QCursor cursor(object_rotate_pixmap, 15, 15);

		view->SetCursor(&cursor);
	}

	~CLinearInteraction()
	{
		m_view->SetCursor(nullptr);
	}

	virtual bool Initialize(POINT point, IDisplayViewport* view) override
	{
		if (!InitializeScreenSpaceOffset(point, view))
		{
			return false;
		}

		Ray ray;
		view->ViewToWorldRay(point, ray.origin, ray.direction);

		Plane p = Plane::CreatePlane(view->ViewDirection(), m_initPosition);
		Intersect::Ray_Plane(ray, p, m_cursorPosition, false);

		// find a point on the gizmo plane
		p = Plane::CreatePlane(m_initAxis, m_initPosition);
		Vec3 pointOnGizmoPlane;

		if (Intersect::Ray_Plane(ray, p, m_initDirection, false))
		{
			m_initDirection -= m_initPosition;
		}
		else
		{
			// intersection with plane failed which means the plane normal is close to perpendicular to ray
			// in that case, just use an approximate direction
			m_initDirection = -view->CameraToWorld(m_initPosition);
		}
		m_initDirection.Normalize();

		return true;
	}

	virtual bool Interact(IDisplayViewport* view, POINT point, AngleAxis& rotation, bool bUseMultiplier) override
	{
		float scale = view->GetScreenScaleFactor(m_initPosition) * gGizmoPreferences.axisGizmoSize * m_scale;

		Vec3 camToOrigin = view->CameraToWorld(m_initPosition);
		Vec3 axis = (m_bScreenAligned ? camToOrigin : m_initAxis);
		axis.Normalize();

		// point in 3D space where user started interacting with gizmo
		Vec3 offsetOnGizmoCircle;
		// vector from camera to that point
		Vec3 cameraToOffset;

		// after this part, m_interactionLine includes a tangent line to the gizmo in 3D space
		if (m_bScreenAligned)
		{
			// aligned gizmos can get a tangent vector just by using the generated rotation matrix
			Matrix34 m = CreateRotationFrameMatrix(view);
			offsetOnGizmoCircle = m_initPosition + scale * m.GetColumn1();
			cameraToOffset = view->CameraToWorld(offsetOnGizmoCircle);
			m_interactionLine = -m.GetColumn0();
		}
		else
		{
			offsetOnGizmoCircle = m_initPosition + scale * m_initDirection;
			cameraToOffset = view->CameraToWorld(offsetOnGizmoCircle);

			if (fabs(axis * cameraToOffset) < 0.00001f)
			{
				//if axis is almost perpendicular to view direction, use a line formed by axis cross view direction
				m_interactionLine = cameraToOffset ^ axis;
			}
			else
			{
				m_interactionLine = m_initDirection ^ axis;
			}
		}
		m_cursorPosition = offsetOnGizmoCircle;

		// the idea here is to find a plane that passes through camera origin and the tangent line in 3D space.
		// The normal of this plane is calculated by cameraToOffset ^ m_interactionLine. Finally, to get
		// a line aligned with the near plane, we get the cross product of the previous plane with the view direction.
		// So final result is (cameraToOffset ^ m_interactionLine) ^ vDir. We'll be using triple product to
		// reduce operations somewhat.
		Vec3 vDir = view->ViewDirection();
		m_interactionLine = (vDir | cameraToOffset) * m_interactionLine - (vDir | m_interactionLine) * cameraToOffset;
		m_interactionLine.Normalize();

		Vec3 offset = view->ViewToAxisConstraint(point, m_interactionLine, offsetOnGizmoCircle);

		m_angle = ((offset * m_interactionLine > 0.0f) ? -1.0f : 1.0f) * offset.len() / scale;

		if (gSnappingPreferences.angleSnappingEnabled())
		{
			float angle = RAD2DEG(m_angle);
			m_angle = DEG2RAD(gSnappingPreferences.SnapAngle(angle));
		}

		rotation.axis = axis;
		// express angle in radians
		rotation.angle = m_angle;

		return true;
	}

	virtual void DrawCursor(DisplayContext& dc, float scale) override
	{
		Vec3 cursorDir = m_interactionLine * 0.5 * scale;
		float activeSign = (m_angle < 0.0f) ? 1.0f : -1.0f;
		dc.SetColor(1.0f, 1.0f, 0.0f, 1.0f);
		dc.DrawArrow(m_cursorPosition, m_cursorPosition + activeSign * cursorDir, 0.4f * scale);
		dc.SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		dc.DrawArrow(m_cursorPosition, m_cursorPosition - activeSign * cursorDir, 0.4f * scale);
	}

private:
	//! Normalized original offset from the origin on the gizmo plane
	float m_scale;
	Vec3  m_interactionLine;
	//! viewport in which we are interacting
	IDisplayViewport* m_view;
};


CAxisRotateGizmo::CAxisRotateGizmo()
	: m_color(1.0f, 1.0f, 0.0f)
	, m_scale(1.0f)
	, m_bScreenAligned(false)
{
}

CAxisRotateGizmo::~CAxisRotateGizmo()
{
}

void CAxisRotateGizmo::SetPosition(Vec3 pos)
{
	m_position = pos;
}

void CAxisRotateGizmo::SetAxis(Vec3 axis)
{
	m_axis = axis;
	m_axis.Normalize();
}

void CAxisRotateGizmo::SetAlignmentAxis(Vec3 axis)
{
	m_alignment = axis;
	m_alignment.Normalize();
}

void CAxisRotateGizmo::SetColor(Vec3 color)
{
	m_color = color;
}

void CAxisRotateGizmo::SetScale(float scale)
{
	m_scale = scale;
}

void CAxisRotateGizmo::Display(DisplayContext& dc)
{
	IDisplayViewport* view = dc.view;

	// this could probably go in a "prepare" step, but for now keep it here
	float scale;

	uint32 curstate = dc.GetState();
	dc.DepthTestOff();

	float angle;

	if (GetFlag(EGIZMO_INTERACTING))
	{
		Vec3 initPosition = m_interaction->GetInitialPosition();
		scale = view->GetScreenScaleFactor(initPosition) * gGizmoPreferences.axisGizmoSize * m_scale;

		// if the ring is active, we draw differently
		angle = m_interaction->GetAngle();
		angle = fmod(angle, g_PI * 2);
		float angleVal = angle * 180.0f / g_PI;
		float textSize = 1.4f;

		Matrix34 m = m_interaction->CreateRotationFrameMatrix(view);
			// Draw the value of rotation within the circle
		string msg;
		msg.Format("%.1f degrees", angleVal);
		dc.SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		dc.DrawTextLabel(initPosition + m.GetColumn1() * scale, textSize, (LPCSTR)msg, true);

		m_interaction->DrawCursor(dc, scale);

		// Push interaction matrix. All coordinates below here are in space local to the rotation
		dc.PushMatrix(m);

		dc.SetColor(1.0f, 1.0f, 1.0f, 1.0f);

		float origLineWidth = dc.GetLineWidth();

		dc.SetLineWidth(4.0f);
		dc.DrawLine(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, scale, 0.0f));
		dc.SetColor(m_color);
		dc.DrawLine(Vec3(0.0f, 0.0f, 0.0f), Vec3(scale * sin(angle), scale * cos(angle), 0.0f));
		dc.SetLineWidth(origLineWidth);

		dc.SetColor(m_color, 0.5f);
		dc.CullOff();
		dc.DrawDisc(scale, angle);

		// draw snapping
		if (gSnappingPreferences.angleSnappingEnabled())
		{
			float snapAngle = gSnappingPreferences.angleSnap();
			int numLines = 360.0f / snapAngle + 0.5f;
			float step = g_PI2 / numLines;

			++numLines;
			for (int i = 1; i < numLines; ++i)
			{
				const float angle = i * step;
				const float cosangle = cos(angle);
				const float sinangle = sin(angle);

				Vec3 v1 = Vec3(-scale * cosangle, scale * sinangle, 0.0f);
				Vec3 v2 = 0.75f * v1;
				dc.DrawLine(v1, v2);
			}
		}

		// draw axis of wheel
		dc.SetColor(m_color);
		dc.DrawLine(Vec3(0.0f, 0.0f, 10000.0f), Vec3(0.0f, 0.0f, -10000.0f));
	}
	else
	{
		// find the half ring that points towards the camera and draw it
		// construct a view aligned matrix
		Vec3 camToOrigin = view->CameraToWorld(m_position);
		camToOrigin.Normalize();
		scale = view->GetScreenScaleFactor(m_position) * gGizmoPreferences.axisGizmoSize * m_scale;

		if (m_bScreenAligned)
		{
			Vec3 xVec = camToOrigin ^ view->UpViewDirection();
			xVec.Normalize();
			Vec3 yVec = camToOrigin ^ xVec;

			Matrix34 m = Matrix34::CreateFromVectors(xVec, yVec, camToOrigin, m_position);
			angle = g_PI2;
			dc.PushMatrix(m);
		}
		else
		{
			Vec3 yVec = camToOrigin ^ m_axis;
			yVec.Normalize();
			Vec3 xVec = yVec ^ m_axis;

			Matrix34 m = Matrix34::CreateFromVectors(xVec, yVec, m_axis, m_position);
			angle = g_PI;
			dc.PushMatrix(m);
		}

		if (GetFlag(EGIZMO_HIGHLIGHTED))
		{
			dc.SetColor(Vec3(1.0f, 1.0f, 1.0f));
		}
		else
		{
			dc.SetColor(m_color);
		}
	}

	dc.DrawCircle3D(scale, angle, 4.0f);
	dc.PopMatrix();

	dc.SetState(curstate);
}

bool CAxisRotateGizmo::MouseCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int nFlags)
{
	if (GetFlag(EGIZMO_INTERACTING))
	{
		switch (event)
		{
		case eMouseMove:
			{
				AngleAxis rotation;
				bool bUseMultiplier = (nFlags & MK_SHIFT) != 0;

				if (m_interaction->Interact(view, point, rotation, bUseMultiplier))
				{
					signalDragging(view, this, rotation, point, nFlags);
				}
			}
			break;

		case eMouseLUp:
			m_interaction.reset();
			signalEndDrag(view, this, point, nFlags);
			UnsetFlag(EGIZMO_INTERACTING);
			break;
		}
		return true;
	}
	else
	{
		if (event == eMouseLDown)
		{
			if (gGizmoPreferences.rotationInteraction == SGizmoPreferences::eRotationDial)
			{
				m_interaction.reset(new CDialInteraction(*this));
			}
			else if (gGizmoPreferences.rotationInteraction == SGizmoPreferences::eRotationLinear)
			{
				m_interaction.reset(new CLinearInteraction(*this, view));
			}

			if (!m_interaction->Initialize(point, view))
			{
				m_interaction.reset();
				return false;
			}

			SetFlag(EGIZMO_INTERACTING);
			signalBeginDrag(view, this, point, nFlags);
			return true;
		}
		else 
		{
			return false;
		}
	}
}

void CAxisRotateGizmo::GetWorldBounds(AABB& bbox)
{
	// this needs to be improved
	bbox.min = Vec3(-1000000, -1000000, -1000000);
	bbox.max = Vec3(1000000, 1000000, 1000000);
}

//! Custom, Ray - Ring / Cylinder intersection. Note: axis and ray direction should be normalized
inline bool Ray_RingCylinder(const Ray& ray, Vec3& origin, Vec3& axis, float height, float radius, float outerRadius, Vec3& ret)
{
	const float radiusSq = radius * radius;
	const float radiusOuterSq = outerRadius * outerRadius;
	const float rayAxisDot = ray.direction * axis;
	const Vec3 dp = ray.origin - origin;
	const Vec3 top = origin + height * axis;

	// part 1, check against caps of the ring
	if (abs(rayAxisDot) > 0.0001f)
	{
		Vec3 isect = ray.origin - (dp * axis) * ray.direction / rayAxisDot;
		float distSq = (isect - origin).len2();
		if (distSq > radiusSq && distSq < radiusOuterSq)
		{
			ret = isect;
			return true;
		}

		const Vec3 rayToPlane = top - ray.origin;
		isect = ray.origin + (rayToPlane * axis) * ray.direction / rayAxisDot;
		distSq = (isect - top).len2();
		if (distSq > radiusSq && distSq < radiusOuterSq)
		{
			ret = isect;
			return true;
		}
	}

	// part 2, intersect with the cylinder body itself - only do the outer part, since inner is hidden for gizmo
	const Vec3 v1 = ray.direction - rayAxisDot * axis;
	const Vec3 v2 = dp - (dp * axis) * axis;
	const float a = v1 * v1;
	const float b = 2.0f * (v1 * v2);
	const float c = (v2 * v2) - radiusOuterSq;

	float solutions[2];
	const int nsolutions = crymath::solve_quadratic(a, b, c, solutions);

	// solutions are sorted from biggest to lowest, therefore we test the lowest first
	for (int i = nsolutions - 1; i >= 0; --i)
	{
		// we are only interested in positive rays
		if (solutions[i] > 0.0f)
		{
			const Vec3 isect = ray.origin + solutions[i] * ray.direction;

			if ((isect - origin) * axis > 0.0f && (isect - top) * axis < 0.0f)
			{
				ret = isect;
				return true;
			}
		}
	}

	return false;
}

bool CAxisRotateGizmo::HitTest(HitContext& hc)
{
	using namespace Private_AxisRotateGizmo;
	IDisplayViewport* view = hc.view;

	const float scale = view->GetScreenScaleFactor(m_position) * gGizmoPreferences.axisGizmoSize * m_scale;

	// Ray in world space.
	view->ViewToWorldRay(hc.point2d, hc.raySrc, hc.rayDir);

	Vec3 viewDir = view->CameraToWorld(m_position);
	const Ray ray(hc.raySrc, hc.rayDir);

	const float radius = 0.9f * scale;
	const float outerRadius = 1.1f * scale;

	// aligned to view gizmos do not need the full intersection with the cylinder, just do one collision on ring plane
	viewDir.Normalize();
	auto viewAxisDot = m_axis * viewDir;
	if (m_bScreenAligned || viewAxisDot < -0.96)
	{
		if (IsWithinRing(ray, viewDir, m_position, outerRadius, radius))
		{
			hc.dist = 0;
			return true;
		}
	}
	// if we look at rotation gizmo straight form a side, than it's just a line
	else if (viewAxisDot > -0.04 && viewAxisDot < 0.04)
	{
		// line's got no ends so we need to check if it's still within the circle
		if (IsWithinRing(ray, viewDir, m_position, outerRadius))
		{
			const auto dist = GetDistanceBetweenLines(ray.origin, ray.direction, m_position, m_axis.Cross(viewDir));
			if (dist > -0.03 && dist < 0.03)
			{
				hc.dist = dist;
				return true;
			}
		}
	}
	else
	{
		const float height = 0.2f * scale;
		const float halfHeight = 0.5f * height;

		Vec3 isect;
		if (Ray_RingCylinder(ray, m_position - halfHeight * m_axis, m_axis, height, radius, outerRadius, isect))
		{
			// find difference between intersection and center of cylinder
			isect -= m_position;

			// check if intersection is in half ring part pointing towards view.
			if (viewDir * isect < 0.0f)
			{
				hc.dist = 0;
				return true;
			}
		}
	}
	return false;
}

