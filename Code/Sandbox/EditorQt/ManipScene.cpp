// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ManipScene.h"

#include "Serialization.h"
#include "QViewportEvents.h"

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CryPhysics/IPhysics.h>
#include <CryPhysics/IPhysicsDebugRenderer.h>
#include <Cry3DEngine/IStatObj.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryAnimation/ICryAnimation.h>

#include "QViewport.h"

#include "IDisplayViewport.h"
#include <atltypes.h>
#include "HitContext.h"
#include "Objects/DisplayContext.h"
#include "Gizmos/AxisHelper.h"
#include "Util/EditorUtils.h"
#include "Util/Math.h"

#include "DisplayViewportAdapter.h"

namespace Manip
{

static unsigned int GetLayerBits(const SElements& elements)
{
	unsigned int result = 0;
	for (size_t i = 0; i < elements.size(); ++i)
		result |= 1 << elements[i].layer;
	return result;
}

static unsigned int GetLayerBits(const SElements& elements, const SSelectionSet& selection)
{
	unsigned int result = 0;
	for (size_t i = 0; i < elements.size(); ++i)
		if (selection.Contains(elements[i].id))
			result |= 1 << elements[i].layer;
	return result;
}

static QuatT GetGizmoOrientation(const QuatT& transform, const CCamera* camera, ETransformationSpace space)
{
	if (space == SPACE_LOCAL)
		return transform;
	else
		return QuatT(IDENTITY, transform.t);
}
// ---------------------------------------------------------------------------

SERIALIZATION_ENUM_BEGIN(ETransformationMode, "Transformation Mode")
SERIALIZATION_ENUM(MODE_TRANSLATE, "translate", "Translate")
SERIALIZATION_ENUM(MODE_ROTATE, "rotate", "Rotate")
SERIALIZATION_ENUM(MODE_SCALE, "scale", "Scale")
SERIALIZATION_ENUM_END()

// ---------------------------------------------------------------------------

CScene::CScene()
	: m_axisHelper(new CAxisHelper())
	, m_showGizmo(true)
	, m_gizmoVisibilityFlags(PoseModifierGizmo | AttachmentGizmo)
	, m_transformationMode(MODE_TRANSLATE)
	, m_transformationSpace(SPACE_LOCAL)
	, m_customDrawer(0)
	, m_customTracer(0)
	, m_spaceProvider(0)
	, m_temporaryLocalDelta(IDENTITY)
	, m_visibleLayerMask(0xffffffff)
{
	m_axisHelper->SetMode(CAxisHelper::MOVE_FLAG);
}

CScene::~CScene()
{
}

static void DrawElement(const SRenderContext& rc, const SElement& element, ElementId highlightedItem, bool isSelected, bool xRay, IElementDrawer* customDrawer, CScene* spaceProvider, const SLookSettings& lookSettings)
{
	if (element.hidden)
		return;

	IRenderer* renderer = GetIEditorImpl()->GetRenderer();
	IPhysicsDebugRenderer* physicsDebugRenderer = GetIEditorImpl()->GetSystem()->GetIPhysicsDebugRenderer();
	IRenderAuxGeom* aux = rc.pAuxGeom;

	SAuxGeomRenderFlags defaultFlags(e_Mode3D | e_AlphaBlended | e_DrawInFrontOff | e_FillModeSolid | e_CullModeNone | e_DepthWriteOn | e_DepthTestOn);
	SAuxGeomRenderFlags xRayFlags(e_Mode3D | e_AlphaBlended | e_DrawInFrontOn | e_FillModeSolid | e_CullModeNone | e_DepthWriteOff | e_DepthTestOff);
	aux->SetRenderFlags(xRay ? xRayFlags : defaultFlags);

	QuatT transform = spaceProvider->ElementToWorldSpace(element);
	Matrix34 transformM(transform);

	bool isHighlighted = element.id == highlightedItem;

	if (element.shape == SHAPE_BOX)
	{
		OBB obb;
		obb.m33 = Matrix33(IDENTITY);
		obb.c = element.placement.center;
		obb.h = element.placement.size * 0.5f;

		bool isHighlighted = highlightedItem == element.id;
		ColorB color;
		if (isHighlighted)
			color = lookSettings.proxyHighlightColor;
		else if (isSelected)
			color = lookSettings.proxySelectionColor;
		else if (element.poseModifier)
			color = lookSettings.poseModifierColor;
		else if (element.colorGroup == ELEMENT_COLOR_CLOTH)
			color = lookSettings.clothProxyColor;
		else
			color = lookSettings.proxyColor;

		ColorB innerColor = color;

		ColorB edgeColor(innerColor.r, innerColor.g, innerColor.b, 255);

		bool drawBB = false;
		if (!customDrawer || !customDrawer->Draw(element))
			drawBB = true;

		if (isSelected || isHighlighted)
			drawBB = true;

		if (drawBB)
			aux->DrawOBB(obb, transformM, false, edgeColor, eBBD_Faceted);
	}
	else if (element.shape == SHAPE_AXES)
	{
		OBB obb;
		obb.m33 = Matrix33(IDENTITY);
		obb.c = element.placement.center;
		obb.h = Vec3(0.01f, 0.01f, 0.01f);

		bool isHighlighted = highlightedItem == element.id;
		ColorB color;
		if (isHighlighted)
			color = lookSettings.proxyHighlightColor;
		else if (isSelected)
			color = lookSettings.proxySelectionColor;
		else if (element.poseModifier)
			color = lookSettings.poseModifierColor;
		else
			color = lookSettings.proxyColor;

		ColorB innerColor = color;

		ColorB edgeColor(innerColor.r, innerColor.g, innerColor.b, 255);

		bool drawBB = false;
		if (!customDrawer || !customDrawer->Draw(element))
			drawBB = true;
		if (isSelected || isHighlighted)
			drawBB = true;

		if (drawBB)
			aux->DrawOBB(obb, transformM, false, edgeColor, eBBD_Faceted);
	}
}

void CScene::OnViewportRender(const SRenderContext& rc)
{
	IRenderer* renderer = GetIEditorImpl()->GetRenderer();
	IRenderAuxGeom* aux = rc.pAuxGeom;

	IPhysicsDebugRenderer* physicsDebugRenderer = GetIEditorImpl()->GetSystem()->GetIPhysicsDebugRenderer();
	physicsDebugRenderer->UpdateCamera(*rc.viewport->Camera());

	SignalRenderElements(m_elements, rc);

	for (int xRayPass = 0; xRayPass < 2; ++xRayPass)
	{
		size_t numElements = m_elements.size();
		for (size_t i = 0; i < numElements; ++i)
		{
			SElement& element = m_elements[i];
			bool isSelected = m_selection.Contains(element.id);
			if (!isSelected)
			{
				if (!IsLayerVisible(element.layer))
					continue;
				if (element.poseModifier && !(m_gizmoVisibilityFlags & PoseModifierGizmo))
					continue;
				if (!element.poseModifier && !(m_gizmoVisibilityFlags & AttachmentGizmo))
					continue;
			}

			bool xRay = isSelected || element.id == m_highlightedItem || element.alwaysXRay;

			if (int(xRay) != xRayPass)
				continue;

			DrawElement(rc, element, m_highlightedItem, m_selection.Contains(element.id), xRay, m_customDrawer, this, m_lookSettings);
		}
	}

	if (m_mouseDragHandler.get())
		m_mouseDragHandler->Render(rc);

	bool hasSelection = !m_selection.IsEmpty();

	if (m_showGizmo && hasSelection)
	{
		int selectionCaps = GetSelectionCaps();
		Matrix34 m = Matrix34(GetGizmoOrientation(GetSelectionTransform(SPACE_WORLD), rc.viewport->Camera(), m_transformationSpace));
		DisplayContext dc;
		CDisplayViewportAdapter view(rc.viewport);
		dc.SetView(&view);

		SGizmoPreferences gizmoParameters;
		gizmoParameters.axisGizmoSize = 0.2f;

		if (m_axisHelper.get())
		{
			switch (m_transformationMode)
			{
			case MODE_TRANSLATE:
				m_axisHelper->SetMode(CAxisHelper::MOVE_FLAG);
				gizmoParameters.enabled = (selectionCaps & CAP_MOVE) != 0;
				break;
			case MODE_ROTATE:
				m_axisHelper->SetMode(CAxisHelper::ROTATE_FLAG);
				gizmoParameters.enabled = (selectionCaps & CAP_ROTATE) != 0;
				break;
			case MODE_SCALE:
				m_axisHelper->SetMode(CAxisHelper::SCALE_FLAG);
				gizmoParameters.enabled = (selectionCaps & CAP_SCALE) != 0;
				break;
			}
			m_axisHelper->DrawAxis(m, gizmoParameters, dc);
		}
	}
}

void CScene::SetTransformationMode(ETransformationMode mode)
{
	m_transformationMode = mode;
}

void CScene::SetTransformationSpace(ETransformationSpace space)
{
	m_transformationSpace = space;
}

ETransformationMode CScene::TransformationMode() const
{
	return m_transformationMode;
}

void CScene::SetSelection(const SSelectionSet& selection)
{
	if (m_selection != selection)
	{
		m_selection = selection;
		SignalSelectionChanged();
	}
}

void CScene::AddToSelection(ElementId elementId)
{
	m_selection.Add(elementId);
	SignalSelectionChanged();
}

void CScene::ApplyToAll(EElementAction action)
{
	size_t count = m_elements.size();
	for (size_t i = 0; i < count; ++i)
	{
		SElement& m = m_elements[i];
		if (m.hidden)
		{
			m.action = action;
			//m.hidden = false;
		}
	}
	SignalElementsChanged(GetLayerBits(m_elements));
}

void CScene::ApplyToSelection(EElementAction action)
{
	size_t count = m_elements.size();
	for (size_t i = 0; i < count; ++i)
	{
		SElement& m = m_elements[i];
		if (m_selection.Contains(m.id))
		{
			m.action = action;
			//m.hidden = true;
		}
	}
	SignalElementsChanged(GetLayerBits(m_elements, m_selection));
}

void CScene::OnViewportKey(const SKeyEvent& ev)
{
	QKeySequence key(ev.key);
	if (ev.type == SKeyEvent::TYPE_PRESS)
	{
		if (key == QKeySequence(Qt::Key_U) || key == QKeySequence(Qt::Key_Z | Qt::CTRL))
		{
			SignalUndo();
		}
		else if (key == QKeySequence(Qt::Key_Y | Qt::CTRL) || key == QKeySequence(Qt::Key_Z | Qt::CTRL | Qt::SHIFT))
		{
			SignalRedo();
		}
		else if (key == QKeySequence(Qt::ALT | Qt::Key_H) || key == QKeySequence(Qt::SHIFT | Qt::Key_H))
		{
			ApplyToAll(ACTION_UNHIDE);
		}
		else if (key == QKeySequence(Qt::Key_H))
		{
			ApplyToSelection(ACTION_HIDE);
		}
		else if (key == QKeySequence(Qt::Key_Delete))
		{
			ApplyToSelection(ACTION_DELETE);
		}
		else if (key == QKeySequence(Qt::Key_1))
		{
			m_transformationMode = MODE_TRANSLATE;
			SignalManipulationModeChanged();
			SignalPropertiesChanged();
		}
		else if (key == QKeySequence(Qt::Key_2))
		{
			m_transformationMode = MODE_ROTATE;
			SignalManipulationModeChanged();
			SignalPropertiesChanged();
		}
		else if (key == QKeySequence(Qt::Key_3))
		{
			m_transformationMode = MODE_SCALE;
			SignalManipulationModeChanged();
			SignalPropertiesChanged();
		}
	}
}

bool RayHitsElement(Vec3* intersectionPoint, const Ray& ray, const SElement& element, const IElementTracer* customTracer, const CScene* spaceProvider)
{
	QuatT parentSpace(IDENTITY);
	if (spaceProvider)
		parentSpace = spaceProvider->GetParentSpace(element);

	OBB obb;
	if (element.shape == SHAPE_BOX)
	{
		obb.m33 = Matrix33((parentSpace * element.placement.transform).q);
		obb.c = element.placement.center;
		obb.h = element.placement.size * 0.5f;
	}
	else
	{
		obb.m33 = IDENTITY;
		obb.c = Vec3(ZERO);
		obb.h = element.placement.size * 0.5f;
	}

	if (Intersect::Ray_OBB(ray, (parentSpace * element.placement.transform).t, obb, *intersectionPoint) != 0)
	{
		if (customTracer)
			return customTracer->HitRay(intersectionPoint, ray, element);

		return true;
	}
	return false;
}

static SElement* HitElementWithRay(Vec3* hitPoint, SElements& elements, const Ray& ray, const IElementTracer* customTracer, const CScene* spaceProvider, const unsigned int gizmoVisibilityFlags)
{
	const float BIG_VALUE = 1e20f;
	float closestDistanceSquare = BIG_VALUE;
	int hitPickPriority = INT_MIN;
	SElement* closestElement = 0;

	size_t numElements = elements.size();
	for (size_t i = 0; i < numElements; ++i)
	{
		SElement& element = elements[i];
		if (element.hidden)
			continue;
		if (!spaceProvider->IsLayerVisible(element.layer) &&
		    !spaceProvider->Selection().Contains(element.id))
			continue;

		if (element.poseModifier && !(gizmoVisibilityFlags & CScene::PoseModifierGizmo))
			continue;
		if (!element.poseModifier && !(gizmoVisibilityFlags & CScene::AttachmentGizmo))
			continue;

		Vec3 intersectionPoint;
		if (RayHitsElement(&intersectionPoint, ray, element, customTracer, spaceProvider))
		{
			float distanceSquare = (intersectionPoint - ray.origin).GetLengthSquared();
			if (distanceSquare < closestDistanceSquare || int(element.mousePickPriority) > hitPickPriority)
			{
				closestDistanceSquare = distanceSquare;
				closestElement = &element;
				hitPickPriority = element.mousePickPriority;
				*hitPoint = intersectionPoint;
			}
		}
	}
	return closestElement;
}

static ElementId HitSelectionWithRay(Vec3* hitPoint, SElements& elements, const Ray& ray, const IElementTracer* customTracer, const CScene* spaceProvider, const unsigned int gizmoVisibilityFlags)
{
	SElement* element = HitElementWithRay(hitPoint, elements, ray, customTracer, spaceProvider, gizmoVisibilityFlags);
	if (element)
		return element->id;

	return 0;
}

struct STransformConstraint
{
	enum EType
	{
		NONE,
		AXIS,
		PLANE
	};

	EType type;
	Plane plane;
	Vec3  axis;
	Vec3  localAxis;

	Vec3  GetAxis(ETransformationSpace space)
	{
		switch (space)
		{
		case SPACE_LOCAL:
			return localAxis;
		default:
			return axis;
		}
	}

	STransformConstraint()
		: type(NONE)
	{
	}
};

struct CScene::SMoveHandler : IMouseDragHandler
{
	CScene*              m_scene;
	Vec3                 m_lastPoint;
	Vec3                 m_startPoint;
	Vec2i                m_startMousePosition;
	STransformConstraint m_constraint;
	SElements            m_elements;
	SElements            m_transformedElements;
	int                  m_layerBits;

	SMoveHandler(CScene* scene, const STransformConstraint& constraint)
		: m_scene(scene)
		, m_constraint(constraint)
		, m_layerBits(0)
		, m_lastPoint(ZERO)
		, m_startPoint(ZERO)
	{
	}

	bool Begin(const SMouseEvent& ev, Vec3 hitPoint) override
	{
		m_startMousePosition = Vec2i(ev.x, ev.y);
		m_startPoint = hitPoint;

		m_scene->GetSelectedElements(&m_elements);
		m_layerBits = GetLayerBits(m_elements);

		if (!ReprojectStartPoint(ev.viewport))
			return false;
		return true;
	}

	bool ReprojectStartPoint(QViewport* viewport)
	{
		Ray ray;
		MAKE_SURE(viewport->ScreenToWorldRay(&ray, m_startMousePosition.x, m_startMousePosition.y), return false);

		if (m_constraint.type == STransformConstraint::PLANE)
		{
			Intersect::Ray_Plane(ray, m_constraint.plane, m_startPoint, false);
		}
		else if (m_constraint.type == STransformConstraint::AXIS)
		{
			Vec3 point;
			Vec3 axis = m_constraint.GetAxis(m_scene->TransformationSpace());
			RayToLineDistance(ray.origin, ray.origin + ray.direction,
			                  m_startPoint - axis * 10000.0f, m_startPoint + axis * 10000.0f, point);
			m_startPoint = m_startPoint + axis * axis.dot(point - m_startPoint);
		}
		return true;
	}

	void Update(const SMouseEvent& ev) override
	{
		ReprojectStartPoint(ev.viewport);

		Ray ray;
		MAKE_SURE(ev.viewport->ScreenToWorldRay(&ray, ev.x, ev.y), return );

		if (m_constraint.type == STransformConstraint::PLANE)
		{
			Vec3 point(0.0f, 0.0f, 0.0f);
			Intersect::Ray_Plane(ray, m_constraint.plane, point, false);

			Vec3 delta = point - m_startPoint;
			Move(delta);
		}
		else if (m_constraint.type == STransformConstraint::AXIS)
		{
			Vec3 point = m_startPoint;
			Vec3 axis = m_constraint.GetAxis(m_scene->TransformationSpace());
			RayToLineDistance(ray.origin, ray.origin + ray.direction,
			                  m_startPoint - axis * 10000.0f, m_startPoint + axis * 10000.0f, point);
			Vec3 newPoint = m_startPoint + axis * axis.dot(point - m_startPoint);
			Vec3 delta = newPoint - m_startPoint;
			Move(delta);
		}
	}

	void Move(Vec3 delta)
	{
		m_transformedElements = m_elements;
		for (size_t i = 0; i < m_transformedElements.size(); ++i)
		{
			SElement& e = m_transformedElements[i];
			QuatT parentSpace = m_scene->GetParentSpace(e) * QuatT(e.placement.transform.q, ZERO);
			QuatT localDelta = parentSpace.GetInverted() * (QuatT(IDENTITY, delta) * parentSpace);
			m_scene->m_temporaryLocalDelta = localDelta;
			e.placement.transform = e.placement.transform * localDelta;
			e.placement.transform.q.Normalize();
		}
		m_scene->UpdateElements(m_transformedElements);
		m_scene->SignalElementContinuousChange(m_layerBits);
	}

	void End(const SMouseEvent& ev) override
	{
		m_scene->m_temporaryLocalDelta = QuatT(IDENTITY);
		m_scene->SignalElementsChanged(m_layerBits);
	}
};

struct CScene::SRotationHandler : public IMouseDragHandler
{
	CScene*              m_scene;
	STransformConstraint m_constraint;
	SElements            m_elements;
	SElements            m_transformedElements;
	int                  m_layerBits;
	Vec3                 m_origin;
	Vec2i                m_startPoint;

	SRotationHandler(CScene* scene, const STransformConstraint& constraint)
		: m_scene(scene)
		, m_constraint(constraint)
		, m_layerBits(0)
		, m_origin(ZERO)
		, m_startPoint(ZERO)
	{
		if (m_constraint.type == STransformConstraint::PLANE)
		{
			// TODO: Apart from having rotation around axis of gizmo (as Sandbox does) it
			// is possible here to rotate around viewer axis. This part can be considerably
			// improved by making sure that picked point on the proxy keeps touching the
			// mouse pointer.

			m_constraint.localAxis = m_constraint.plane.n;
			m_constraint.axis = Vec3(0.0f, 0.0f, 1.0f);
			m_constraint.type = STransformConstraint::AXIS;
		}

		m_scene->GetSelectedElements(&m_elements);
		m_layerBits = GetLayerBits(m_elements);

		m_origin = m_scene->GetSelectionTransform(SPACE_WORLD).t;
	}

	bool Begin(const SMouseEvent& ev, Vec3 hitPoint) override
	{
		m_startPoint = Vec2i(ev.x, ev.y);
		return true;
	}

	void Update(const SMouseEvent& ev) override
	{
		if (ev.y != m_startPoint.y)
		{
			float angle = ((ev.y - m_startPoint.y) * 0.01f) * 3.1415926f;
			QuatT rotation;
			float cos, sin;
			sin = sinf(angle);
			cos = cosf(angle);
			Vec3 axis = m_constraint.GetAxis(m_scene->TransformationSpace());
			rotation.SetRotationAA(cos, sin, axis);

			m_transformedElements = m_elements;
			for (size_t i = 0; i < m_elements.size(); ++i)
			{
				SElement& e = m_transformedElements[i];
				QuatT t = m_scene->ElementToWorldSpace(e);
				t.t -= m_origin;
				t = rotation * t;
				t.t += m_origin;
				m_scene->m_temporaryLocalDelta = m_scene->ElementToWorldSpace(m_elements[i]).GetInverted() * t;
				m_scene->WorldSpaceToElement(&e, t);
			}
			m_scene->UpdateElements(m_transformedElements);
			m_scene->SignalElementContinuousChange(m_layerBits);
		}
	}

	void End(const SMouseEvent& ev) override
	{
		m_scene->m_temporaryLocalDelta = QuatT(IDENTITY);
		m_scene->SignalElementsChanged(m_layerBits);
	}
};

static Vec3 ScaleAround(Vec3 p, Vec3 origin, float scale)
{
	return (p - origin) * scale + origin;
}

struct CScene::SScalingHandler : public IMouseDragHandler
{
	CScene*              m_scene;
	STransformConstraint m_constraint;
	SElements            m_elements;
	SElements            m_transformedElements;
	Vec3                 m_origin;
	Vec3                 m_size;
	Vec3                 m_hitPoint;
	Vec2i                m_startPoint;
	unsigned int         m_layerBits;

	SScalingHandler(CScene* scene, const STransformConstraint& constraint)
		: m_scene(scene)
		, m_constraint(constraint)
		, m_layerBits(0)
		, m_origin(ZERO)
		, m_size(ZERO)
		, m_hitPoint(ZERO)
		, m_startPoint(ZERO)
	{
	}

	bool Begin(const SMouseEvent& ev, Vec3 hitPoint) override
	{
		m_hitPoint = hitPoint;
		m_startPoint = Vec2i(ev.x, ev.y);
		m_scene->GetSelectedElements(&m_elements);
		m_layerBits = GetLayerBits(m_elements);
		m_origin = m_scene->GetSelectionTransform(SPACE_WORLD).t;
		m_size = m_scene->GetSelectionSize();
		return true;
	}

	void Update(const SMouseEvent& ev) override
	{
		float screenScaleFactor = 0.0f;
		if (const CCamera* camera = ev.viewport->Camera())
		{
			screenScaleFactor = camera->GetPosition().GetDistance(m_hitPoint);
			if (screenScaleFactor < camera->GetNearPlane())
				screenScaleFactor = camera->GetNearPlane();
		}

		m_transformedElements = m_elements;

		if (m_transformedElements.size() == 1)
		{
			Vec3& size = m_transformedElements[0].placement.size;
			float difference = -(ev.y - m_startPoint.y) * 0.01f * screenScaleFactor;
			Vec3 axis = m_constraint.GetAxis(m_scene->TransformationSpace());
			size = (size + size.CompMul(axis * difference)).abs();
		}
		else
		{
			float difference = -(ev.y - m_startPoint.y) * 0.01f * screenScaleFactor;
			float sizeDifference = max(0.01f, fabsf(1.0f + difference));

			for (size_t i = 0; i < m_transformedElements.size(); ++i)
			{
				SElement& g = m_transformedElements[i];
				g.placement.transform.t = ScaleAround(g.placement.transform.t, m_origin, sizeDifference);
				g.placement.size = g.placement.size * sizeDifference;
			}
		}

		m_scene->UpdateElements(m_transformedElements);
		m_scene->SignalElementContinuousChange(m_layerBits);
	}

	void End(const SMouseEvent& ev) override
	{
		m_scene->SignalElementsChanged(m_layerBits);
	}
};

template<class T>
struct SRange
{
	T low;
	T high;

	SRange(T low, T high)
		: low(low), high(high)
	{
	}

	SRange()
		: low(T(0))
		, high(T(0))
	{
	}

	void Add(T value)
	{
		if (value < low)
			low = value;
		if (value > high)
			high = value;
	}

	bool Intersects(const SRange& r) const
	{
		if (high < r.low)
			return false;
		if (low > r.high)
			return false;
		return true;
	}
};

static bool RectsIntersect(Vec2i amin, Vec2i amax, Vec2i bmin, Vec2i bmax)
{
	if (!SRange<int>(amin.x, amax.x).Intersects(SRange<int>(bmin.x, bmax.x)))
		return false;

	if (!SRange<int>(amin.y, amax.y).Intersects(SRange<int>(bmin.y, bmax.y)))
		return false;

	return true;
}

static bool OBBOverlapsSelectionFrustum(const OBB& box, Vec2i rectMin, Vec2i rectMax, const Plane (&frustum)[4], IDisplayViewport* viewport)
{
	Vec3 s = box.h;
	Vec3 points[8] = {
		box.m33 * Vec3(-s.x, -s.y, -s.z) + box.c,
		box.m33 * Vec3(s.x,  -s.y, -s.z) + box.c,
		box.m33 * Vec3(s.x,  s.y,  -s.z) + box.c,
		box.m33 * Vec3(-s.x, s.y,  -s.z) + box.c,
		box.m33 * Vec3(-s.x, -s.y, s.z) + box.c,
		box.m33 * Vec3(s.x,  -s.y, s.z) + box.c,
		box.m33 * Vec3(s.x,  s.y,  s.z) + box.c,
		box.m33 * Vec3(-s.x, s.y,  s.z) + box.c,
	};

	Vec2i projectedPoints[8];
	Vec2i boundMin(INT_MAX, INT_MAX);
	Vec2i boundMax(INT_MIN, INT_MIN);
	for (int i = 0; i < 8; ++i)
	{
		Vec2i& p = projectedPoints[i];
		POINT pt = viewport->WorldToView(points[i]);
		p.x = pt.x;
		p.y = pt.y;
		if (p.x < boundMin.x)
			boundMin.x = p.x;
		if (p.y < boundMin.y)
			boundMin.y = p.y;
		if (p.x > boundMax.x)
			boundMax.x = p.x;
		if (p.y > boundMax.y)
			boundMax.y = p.y;
	}

	if (boundMin.x > rectMin.x &&
	    boundMax.x < rectMax.x &&
	    boundMin.y > rectMin.y &&
	    boundMax.y < rectMax.y)
		return true;

	if (!RectsIntersect(boundMin, boundMax, rectMin, rectMax))
		return false;

	struct Edge
	{
		Vec2 a;
		Vec2 b;
	};

	Edge boxEdges[12] = {
		{ projectedPoints[0], projectedPoints[1] },
		{ projectedPoints[1], projectedPoints[2] },
		{ projectedPoints[2], projectedPoints[3] },
		{ projectedPoints[3], projectedPoints[0] },
		{ projectedPoints[4], projectedPoints[5] },
		{ projectedPoints[5], projectedPoints[6] },
		{ projectedPoints[6], projectedPoints[7] },
		{ projectedPoints[7], projectedPoints[4] },
		{ projectedPoints[0], projectedPoints[4] },
		{ projectedPoints[1], projectedPoints[5] },
		{ projectedPoints[2], projectedPoints[6] },
		{ projectedPoints[3], projectedPoints[7] }
	};

	Vec2 rectPoints[4] = {
		Vec2(float(rectMin.x), float(rectMin.y)),
		Vec2(float(rectMax.x), float(rectMin.y)),
		Vec2(float(rectMin.x), float(rectMax.y)),
		Vec2(float(rectMax.x), float(rectMax.y))
	};

	for (size_t i = 0; i < 12; ++i)
	{
		const Edge& e = boxEdges[i];
		Vec2 dir = e.b - e.a;
		if (dir.GetLength2() < 1.0f)
			continue;
		dir.Normalize();
		Vec2 ort(dir.y, -dir.x);

		SRange<float> range1;
		range1.low = ort.Dot(projectedPoints[0]);
		range1.high = range1.low;
		for (size_t j = 1; j < 8; ++j)
		{
			float pos = ort.Dot(projectedPoints[j]);
			range1.Add(pos);
		}

		SRange<float> range2;
		range2.low = ort.Dot(rectPoints[0]);
		range2.high = range2.low;
		range2.Add(ort.Dot(rectPoints[1]));
		range2.Add(ort.Dot(rectPoints[2]));
		range2.Add(ort.Dot(rectPoints[3]));

		if (!range1.Intersects(range2))
			return false;
	}

	return true;
}

static bool ElementInFrustum(const SElement& element, Vec2i rectMin, Vec2i rectMax, const Plane (&frustum)[4], IDisplayViewport* viewport, CScene* scene)
{
	OBB obb;
	QuatT parentSpace = scene->GetParentSpace(element);
	obb.m33 = Matrix33((parentSpace * element.placement.transform).q);
	obb.c = Vec3((parentSpace * element.placement.transform).t);
	obb.h = element.placement.size * 0.5f;

	return OBBOverlapsSelectionFrustum(obb, rectMin, rectMax, frustum, viewport);
}

static void FindElementsInRect(vector<const SElement*>* out, Vec2i point1, Vec2i point2, QViewport* viewport,
                               const vector<SElement>& elements, ISpaceProvider* spaceProvider, CScene* scene)
{
	int minX = min(point1.x, point2.x);
	int minY = min(point1.y, point2.y);
	int maxX = max(point1.x, point2.x);
	int maxY = max(point1.y, point2.y);
	Ray ray1;
	if (!viewport->ScreenToWorldRay(&ray1, minX, minY))
		return;
	Ray ray2;
	if (!viewport->ScreenToWorldRay(&ray2, maxX, maxY))
		return;

	Matrix34 m = viewport->Camera()->GetMatrix();
	Vec3 xdir = m.GetColumn0().GetNormalized();
	Vec3 zdir = m.GetColumn2().GetNormalized();
	Vec3 pos = m.GetTranslation();

	Vec3 normals[4] = {
		ray1.direction.Cross(xdir).GetNormalized(),
		(zdir).Cross(ray2.direction).GetNormalized(),
		ray2.direction.Cross(-xdir).GetNormalized(),
		(-zdir).Cross(ray1.direction).GetNormalized(),
	};
	Plane frustum[4];
	for (int i = 0; i < 4; ++i)
		frustum[i].SetPlane(-normals[i], pos);

	CDisplayViewportAdapter view(viewport);

	for (int j = 0; j < elements.size(); ++j)
	{
		const SElement& m = elements[j];
		if (m.hidden)
			continue;
		if (!scene->IsLayerVisible(m.layer) && !scene->Selection().Contains(m.id))
			continue;
		if (ElementInFrustum(m, Vec2i(minX, minY), Vec2i(maxX, maxY), frustum, &view, scene))
			out->push_back(&m);
	}
}

static void GetSelectionInRect(SSelectionSet* selection, Vec2i p1, Vec2i p2, QViewport* viewport, const vector<SElement>& elements, ISpaceProvider* spaceProvider, CScene* scene)
{
	vector<const SElement*> elementsIn;
	FindElementsInRect(&elementsIn, p1, p2, viewport, elements, spaceProvider, scene);

	for (size_t i = 0; i < elementsIn.size(); ++i)
		selection->Add(elementsIn[i]->id);
}

static void DrawLine2D(const SRenderContext& rc, Vec2i p1, Vec2i p2, ColorB color)
{
	IRenderer* renderer = GetIEditorImpl()->GetRenderer();
	IRenderAuxGeom* aux = renderer->GetIRenderAuxGeom();
	int w = rc.viewport->Width();
	int h = rc.viewport->Height();
	if (w == 0 || h == 0)
		return;

	int renderFlags = aux->GetRenderFlags().m_renderFlags;
	aux->SetRenderFlags((renderFlags | e_Mode2D) & ~e_Mode3D);

	Vec3 start(float(p1.x) / w, float(p1.y) / h, 0.0f);
	Vec3 end(float(p2.x) / w, float(p2.y) / h, 0.0f);
	aux->DrawLine(start, color, end, color);

	aux->SetRenderFlags(renderFlags);
}

struct CScene::SBlockSelectHandler : public IMouseDragHandler
{
	CScene*       m_scene;
	Vec2i         m_startPoint;
	Vec2i         m_endPoint;
	SSelectionSet m_lastSelection;

	SBlockSelectHandler(CScene* scene)
		: m_scene(scene)
	{
	}

	bool Begin(const SMouseEvent& ev, Vec3 hitPoint) override
	{
		m_startPoint = Vec2i(ev.x, ev.y);
		m_endPoint = m_startPoint;
		m_scene->SignalPushUndo("Selection change", 0);
		m_scene->SetSelection(SSelectionSet());
		return true;
	}

	void Update(const SMouseEvent& ev) override
	{
		m_endPoint = Vec2i(ev.x, ev.y);

		SSelectionSet selection;
		GetSelectionInRect(&selection, m_startPoint, m_endPoint, ev.viewport, m_scene->Elements(), m_scene->SpaceProvider(), m_scene);

		if (selection != m_lastSelection || (selection.IsEmpty() && !m_scene->Selection().IsEmpty()))
			m_scene->SetSelection(selection);
	}

	void Render(const SRenderContext& rc) override
	{
		ColorB color(255, 255, 255, 255);
		Vec2i points[4] = { m_startPoint, Vec2i(m_startPoint.x, m_endPoint.y), m_endPoint, Vec2i(m_endPoint.x, m_startPoint.y) };
		DrawLine2D(rc, points[0], points[1], color);
		DrawLine2D(rc, points[1], points[2], color);
		DrawLine2D(rc, points[2], points[3], color);
		DrawLine2D(rc, points[3], points[0], color);
	}

	void End(const SMouseEvent& ev) override
	{
	}
};

void CScene::OnMouseMove(const SMouseEvent& ev)
{
	SGizmoPreferences gizmoParameters;
	gizmoParameters.axisGizmoSize = 0.2f;
	CDisplayViewportAdapter displayView(ev.viewport);

	QuatT selectionTransform = GetSelectionTransform(SPACE_WORLD);
	int selectionCaps = GetSelectionCaps();
	QuatT axesTransform = GetGizmoOrientation(selectionTransform, ev.viewport->Camera(), m_transformationSpace);

	if (m_mouseDragHandler.get())
		m_mouseDragHandler->Update(ev);
	else if (m_showGizmo)
	{
		bool gizmoEnabled = true;

		switch (m_transformationMode)
		{
		case MODE_TRANSLATE:
			gizmoEnabled = (selectionCaps & CAP_MOVE) != 0;
			break;
		case MODE_ROTATE:
			gizmoEnabled = (selectionCaps & CAP_ROTATE) != 0;
			break;
		case MODE_SCALE:
			gizmoEnabled = (selectionCaps & CAP_SCALE) != 0;
			break;
		}

		if (gizmoEnabled)
		{
			HitContext hc;
			hc.point2d.x = ev.x;
			hc.point2d.y = ev.y;
			hc.view = &displayView;
			m_axisHelper->HitTest(Matrix34(axesTransform), gizmoParameters, hc);
			m_axisHelper->SetHighlightAxis(hc.axis);
		}
		else
		{
			m_axisHelper->SetHighlightAxis(0);
		}

		Ray ray;
		if (ev.viewport->ScreenToWorldRay(&ray, ev.x, ev.y))
		{
			Vec3 hitPoint;
			m_highlightedItem = HitSelectionWithRay(&hitPoint, m_elements, ray, m_customTracer, this, m_gizmoVisibilityFlags);
		}
	}
}

void CScene::OnViewportMouse(const SMouseEvent& ev)
{
	if (!ev.viewport)
		return;

	SGizmoPreferences gizmoParameters;
	gizmoParameters.axisGizmoSize = 0.2f;
	CDisplayViewportAdapter displayView(ev.viewport);

	QuatT selectionTransform = GetSelectionTransform(SPACE_WORLD);
	int selectionCaps = GetSelectionCaps();
	QuatT axesTransform = GetGizmoOrientation(selectionTransform, ev.viewport->Camera(), m_transformationSpace);

	if (ev.type == SMouseEvent::TYPE_PRESS)
	{
		int button = ev.button;
		if (button == SMouseEvent::BUTTON_LEFT)
		{
			Ray ray;
			if (ev.viewport->ScreenToWorldRay(&ray, ev.x, ev.y))
			{
				STransformConstraint constraint;

				Vec3 hitPoint = selectionTransform.t;
				if (m_showGizmo)
				{
					HitContext hc;
					hc.point2d.x = ev.x;
					hc.point2d.y = ev.y;
					hc.view = &displayView;
					if (!Selection().IsEmpty() && m_axisHelper->HitTest(Matrix34(axesTransform), gizmoParameters, hc))
					{
						Quat localRot(selectionTransform.q);
						Quat planeRot(GetGizmoOrientation(QuatT(selectionTransform.q, ZERO), ev.viewport->Camera(), m_transformationSpace).q);
						switch (hc.axis)
						{
						case AXIS_X:
							constraint.type = STransformConstraint::AXIS;
							constraint.axis = Vec3(1.0f, 0.0f, 0.0f);
							constraint.localAxis = localRot * constraint.axis;
							break;
						case AXIS_Y:
							constraint.type = STransformConstraint::AXIS;
							constraint.axis = Vec3(0.0f, 1.0f, 0.0f);
							constraint.localAxis = localRot * constraint.axis;
							break;
						case AXIS_Z:
							constraint.type = STransformConstraint::AXIS;
							constraint.axis = Vec3(0.0f, 0.0f, 1.0f);
							constraint.localAxis = localRot * constraint.axis;
							break;
						case AXIS_XY:
							constraint.type = STransformConstraint::PLANE;
							constraint.plane.SetPlane(planeRot * Vec3(0.0f, 0.0f, 1.0f), hitPoint);
							constraint.axis = Vec3(1.0f, 1.0f, 0.0f);
							break;
						case AXIS_XZ:
							constraint.type = STransformConstraint::PLANE;
							constraint.plane.SetPlane(planeRot * Vec3(0.0f, 1.0f, 0.0f), hitPoint);
							constraint.axis = Vec3(1.0f, 0.0f, 1.0f);
							break;
						case AXIS_YZ:
							constraint.type = STransformConstraint::PLANE;
							constraint.plane.SetPlane(planeRot * Vec3(1.0f, 0.0f, 0.0f), hitPoint);
							constraint.axis = Vec3(0.0f, 1.0f, 1.0f);
							break;
						case AXIS_XYZ:
							constraint.type = STransformConstraint::AXIS;
							constraint.localAxis = Vec3(1.0f, 1.0f, 1.0f);
							constraint.axis = Vec3(1.0f, 1.0f, 1.0f);
							break;
						}
					}
				}

				if (constraint.type == STransformConstraint::NONE)
				{
					ElementId selected_id = HitSelectionWithRay(&hitPoint, m_elements, ray, m_customTracer, this, m_gizmoVisibilityFlags);
					if (selected_id != 0)
					{
						if (!m_selection.Contains(selected_id) || m_selection.Size() > 1)
						{
							if (ev.control)
								AddToSelection(selected_id);
							else
								SetSelection(SSelectionSet(selected_id));
						}

						if (m_transformationMode == MODE_SCALE)
						{
							constraint.type = STransformConstraint::AXIS;
							constraint.axis = Vec3(1.0f, 1.0f, 1.0f);
							constraint.localAxis = Vec3(1.0f, 1.0f, 1.0f);
						}
						else
						{
							Matrix34 m = ev.viewport->Camera()->GetMatrix();
							Vec3 xdir = m.GetColumn0().GetNormalized();
							Vec3 ydir = m.GetColumn1().GetNormalized();
							Vec3 zdir = m.GetColumn2().GetNormalized();
							Vec3 pos = m.GetTranslation();

							Vec3 fromScreenToSelection = pos - hitPoint;
							float distance = ydir.Dot(fromScreenToSelection);
							Vec3 planeCenter = pos + -ydir * distance;

							constraint.type = STransformConstraint::PLANE;
							constraint.plane.SetPlane(-ydir, planeCenter);
							constraint.axis = Vec3(1.0f, 0.0f, 1.0f);
						}
					}
					else
					{
						m_mouseDragHandler.reset(new SBlockSelectHandler(this));
					}
				}

				if (constraint.type != STransformConstraint::NONE)
				{
					switch (m_transformationMode)
					{
					case MODE_TRANSLATE:
						if (selectionCaps & CAP_MOVE)
							m_mouseDragHandler.reset(new SMoveHandler(this, constraint));
						break;
					case MODE_ROTATE:
						if (selectionCaps & CAP_ROTATE)
							m_mouseDragHandler.reset(new SRotationHandler(this, constraint));
						break;
					case MODE_SCALE:
						if (selectionCaps & CAP_SCALE)
							m_mouseDragHandler.reset(new SScalingHandler(this, constraint));
						break;
					default:
						break;
					}
				}

				if (m_mouseDragHandler.get())
				{
					if (!m_mouseDragHandler->Begin(ev, hitPoint))
						m_mouseDragHandler.reset();
				}
			}
		}
	}
	else if (ev.type == SMouseEvent::TYPE_RELEASE)
	{
		if (m_mouseDragHandler.get())
		{
			m_mouseDragHandler->End(ev);
			m_mouseDragHandler.reset();
			ev.viewport->ReleaseMouse();
		}
	}
	else if (ev.type == SMouseEvent::TYPE_MOVE)
	{
		OnMouseMove(ev);
	}
}

struct CScene::STransformBox
{
	CScene* m_scene;

	STransformBox(CScene* scene)
		: m_scene(scene)
	{
	}

	void Serialize(IArchive& ar)
	{
		QuatT transform = m_scene->GetSelectionTransform(SPACE_WORLD);
		int caps = m_scene->GetSelectionCaps();

		Vec3 position = transform.t;
		if ((caps & CAP_MOVE))
			ar(position, "position", "<P");

		Ang3 rotation(Ang3::GetAnglesXYZ(transform.q));
		Ang3 rotationOld = rotation;
		if ((caps & CAP_ROTATE))
			ar(Serialization::RadiansAsDeg(rotation), "rotation", "<R");

		Vec3 size = m_scene->GetSelectionSize();
		Vec3 sizeOld = size;
		if ((caps & CAP_SCALE))
			ar(size, "size", "<S");

		if (ar.isInput())
		{
			bool transformChanged = false;
			bool sizeChanged = false;

			Vec3 oldPosition = transform.t;
			if (position.IsValid() && oldPosition.IsValid() && position != oldPosition)
			{
				transform.SetTranslation(position);
				transformChanged = true;
			}

			if (rotation.IsValid() && rotationOld.IsValid() && rotation != rotationOld)
			{
				transform = QuatT(Quat(rotation), transform.t);
				transformChanged = true;
			}

			if (size.IsValid() && sizeOld.IsValid() && size != sizeOld)
				sizeChanged = true;

			if (transformChanged)
				m_scene->SetSelectionTransform(SPACE_WORLD, transform);

			if (sizeChanged)
				m_scene->SetSelectionSize(size);
		}
	}
};

struct SSpaceSelector
{
	ETransformationSpace& space;

	SSpaceSelector(ETransformationSpace& space)
		: space(space)
	{
	}

	void Serialize(Serialization::IArchive& ar)
	{
		using Serialization::RadioButton;
		ar(RadioButton((int&)space, SPACE_WORLD), "transformWorld", "^Global");
		ar(RadioButton((int&)space, SPACE_LOCAL), "transformLocal", "^Local");
	}
};

void CScene::Serialize(IArchive& ar)
{
	ar(m_transformationMode, "transformationMode", 0);
	if (ar.isEdit())
		ar(SSpaceSelector(m_transformationSpace), "transformationSpace", "<Manipulator Space");
	else
		ar(SSpaceSelector(m_transformationSpace), "transformationSpace");
	ar(m_showGizmo, "showGizmo", "Show Manipulation Gizmo");
	if (!m_selection.IsEmpty())
	{
		STransformBox transform(this);
		ar(transform, "transform", "Global Transform");
	}
}

void CScene::GetSelectedElements(SElements* elements) const
{
	elements->clear();
	size_t numElements = m_elements.size();
	for (size_t i = 0; i < numElements; ++i)
	{
		const SElement& element = m_elements[i];
		if (m_selection.Contains(element.id))
			elements->push_back(element);
	}
}

void CScene::UpdateElements(const SElements& elements)
{
	// TODO: remove quadratic complexity here
	size_t numElements = m_elements.size();
	for (size_t i = 0; i < numElements; ++i)
	{
		for (size_t j = 0; j < elements.size(); ++j)
		{
			if (m_elements[i].id == elements[j].id)
			{
				m_elements[i].placement = elements[j].placement;
				m_elements[i].changed = true;
			}
		}
	}
}

void CScene::RefreshAllElements()
{
	int layerBits = GetLayerBits(m_elements);
	SignalElementContinuousChange(layerBits); // Use continuous change to update the entire property tree
}

void CScene::Clear()
{
	m_elements.clear();
	m_lastIdByLayer.clear();
}

void CScene::ClearLayer(int layer)
{
	auto newEnd = std::remove_if(m_elements.begin(), m_elements.end(), [=](const SElement& e){ return e.layer == layer; });
	m_elements.erase(newEnd, m_elements.end());
	if (layer < m_lastIdByLayer.size())
		m_lastIdByLayer[layer] = layer << 24;
}

void CScene::AddElement(const SElement& element)
{
	ElementId& lastId = m_lastIdByLayer[element.layer];
	AddElement(element, lastId);
	++lastId;

}

void CScene::AddElement(const SElement& element, ElementId id)
{
	m_elements.push_back(element);
	if (element.layer >= m_lastIdByLayer.size())
	{
		while (m_lastIdByLayer.size() <= element.layer)
			m_lastIdByLayer.push_back(m_lastIdByLayer.size() << 24);
	}
	m_elements.back().id = id;

	ElementId& lastId = m_lastIdByLayer[element.layer];
	lastId = max(lastId, id) + 1;
}

QuatT CScene::GetSelectionTransform(ETransformationSpace space) const
{
	if (space == SPACE_WORLD)
	{
		QuatT r(IDENTITY);

		vector<SElement> selectedElements;
		GetSelectedElements(&selectedElements);

		if (selectedElements.size() == 1)
		{
			r = ElementToWorldSpace(selectedElements[0]);
		}
		else if (selectedElements.size() > 1)
		{
			r = GetParentSpace(selectedElements[0]) * selectedElements[0].placement.transform;
			for (size_t i = 1; i < selectedElements.size(); ++i)
			{
				SElement& e = selectedElements[i];
				QuatT parentSpace = GetParentSpace(e);
				r.t = r.t + ElementToWorldSpace(e).t;
			}
			r.SetTranslation(r.t / float(selectedElements.size()));
		}

		if (!_finite(r.t.x) ||
		    !_finite(r.t.y) ||
		    !_finite(r.t.z))
		{
			if (::IsDebuggerPresent())
				__debugbreak();
			r.SetIdentity();
		}
		return r;
	}
	else if (space == SPACE_LOCAL)
	{
		return m_temporaryLocalDelta;
	}

	return IDENTITY;
}

bool CScene::SelectionCanBeMoved() const
{
	return (GetSelectionCaps() & CAP_MOVE) != 0;
}

bool CScene::SelectionCanBeRotated() const
{
	return (GetSelectionCaps() & CAP_ROTATE) != 0;
}

int CScene::GetSelectionCaps() const
{
	int caps = 0;
	vector<SElement> selectedElements;
	GetSelectedElements(&selectedElements);
	for (size_t i = 0; i < selectedElements.size(); ++i)
	{
		if (selectedElements[i].caps & CAP_MOVE && (caps & CAP_MOVE) != 0)
		{
			// enable rotation and scale of two or more positions
			caps |= CAP_ROTATE | CAP_SCALE;
		}

		caps |= selectedElements[i].caps;
	}
	;

	return caps;
}

bool CScene::SetSelectionTransform(ETransformationSpace space, const QuatT& newTransform)
{
	QuatT transform = GetSelectionTransform(SPACE_WORLD);
	QuatT deltaWorld;
	if (space == SPACE_WORLD)
		deltaWorld = transform.GetInverted() * newTransform;
	else
		deltaWorld = newTransform;

	bool hasElementsChanged = false;

	size_t numElements = m_elements.size();
	for (size_t i = 0; i < numElements; ++i)
	{
		SElement& element = m_elements[i];
		if (m_selection.Contains(element.id))
		{
			QuatT parentSpace = GetParentSpace(element);
			QuatT newWorldTransform = parentSpace * element.placement.transform * deltaWorld;
			element.placement.transform = parentSpace.GetInverted() * newWorldTransform;
			element.changed = true;
			hasElementsChanged = true;
		}
	}

	return hasElementsChanged;
}

Vec3 CScene::GetSelectionSize() const
{
	Vec3 size(1.0f, 1.0f, 1.0f);

	vector<SElement> selectedElements;
	GetSelectedElements(&selectedElements);

	if (selectedElements.size() == 1)
		size = selectedElements[0].placement.size;
	else if (selectedElements.size() > 1)
	{
		AABB combinedSize(AABB::RESET);

		for (size_t i = 0; i < selectedElements.size(); ++i)
		{
			const SElement& element = m_elements[i];
			AABB box(element.placement.size * -0.5f, element.placement.size * 0.5f);
			AABB transformedBox = AABB::CreateTransformedAABB(Matrix34(element.placement.transform), box);
			combinedSize.Add(transformedBox);
		}
		size = combinedSize.IsReset() ? Vec3(1.0f, 1.0f, 1.0f) : combinedSize.GetSize();
	}

	return size;
}

bool CScene::SetSelectionSize(const Vec3& size)
{
	bool hasElementsChanged = false;

	vector<SElement> selectedElements;
	GetSelectedElements(&selectedElements);

	if (selectedElements.size() == 1)
	{
		selectedElements[0].placement.size = size;
		selectedElements[0].changed = true;
		hasElementsChanged = true;
	}

	return hasElementsChanged;
}

void CScene::SetSpaceProvider(ISpaceProvider* provider)
{
	m_spaceProvider = provider;
}

QuatT CScene::GetParentSpace(const SElement& e) const
{
	if (!m_spaceProvider)
		return QuatT(IDENTITY);
	QuatT result = m_spaceProvider->GetTransform(e.parentSpaceIndex);
	return result;
}

QuatT CScene::ElementToWorldSpace(const SElement& e) const
{
	if (!m_spaceProvider)
		return e.placement.transform;
	QuatT parent = m_spaceProvider->GetTransform(e.parentSpaceIndex);
	if (e.parentOrientationSpaceIndex.m_attachmentCRC32 == -1 && e.parentOrientationSpaceIndex.m_jointCRC32 == -1)
		return parent * e.placement.transform;

	QuatT parentOrientation = m_spaceProvider->GetTransform(e.parentOrientationSpaceIndex);
	QuatT result = parent * e.placement.transform;
	result.q = parentOrientation.q * e.placement.transform.q;
	return result;
}

void CScene::WorldSpaceToElement(SElement* e, const QuatT& worldSpaceTransform)
{
	if (!e)
		return;
	if (!m_spaceProvider)
	{
		e->placement.transform = worldSpaceTransform;
		return;
	}

	QuatT parent = m_spaceProvider->GetTransform(e->parentSpaceIndex);

	if (e->parentOrientationSpaceIndex.m_attachmentCRC32 == -1 && e->parentOrientationSpaceIndex.m_jointCRC32 == -1)
	{
		e->placement.transform = parent.GetInverted() * worldSpaceTransform;
		return;
	}
	QuatT parentOrientation = m_spaceProvider->GetTransform(e->parentOrientationSpaceIndex);
	e->placement.transform = parent.GetInverted() * worldSpaceTransform;
	e->placement.transform.q = parentOrientation.GetInverted().q * worldSpaceTransform.q;
}

bool CScene::IsLayerVisible(int layer) const
{
	return (m_visibleLayerMask & (1 << layer)) != 0;

}

void CScene::SetVisibleLayerMask(unsigned int layerMask)
{
	m_visibleLayerMask = layerMask;
}

}

