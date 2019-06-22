// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TransformManipulator.h"

#include "Gizmos/AxisHelper.h"
#include "Gizmos/AxisHelperExtended.h"
#include "Preferences/SnappingPreferences.h"
#include "IDisplayViewport.h"
#include "QtUtil.h"

#include <IEditor.h>
#include <IUndoManager.h>

CTransformManipulator::CTransformManipulator(ITransformManipulatorOwner* owner)
	: m_bDragging(false)
	, m_bUseCustomTransform(false)
	, m_bSkipConstraintColor(false)
	, m_owner(owner)
	, m_xRotateAxis(false)
	, m_yRotateAxis(false)
	, m_zRotateAxis(false)
{
	m_matrix.SetIdentity();

	SetUpGizmos();

	GetIEditor()->GetLevelEditorSharedState()->signalAxisConstraintChanged.Connect(this, &CTransformManipulator::UpdateGizmoColors);
	GetIEditor()->GetLevelEditorSharedState()->signalCoordSystemChanged.Connect(this, &CTransformManipulator::OnUpdateState);
	GetIEditor()->GetLevelEditorSharedState()->signalEditModeChanged.Connect(this, &CTransformManipulator::OnUpdateState);
}

CTransformManipulator::~CTransformManipulator()
{
	GetIEditor()->GetLevelEditorSharedState()->signalAxisConstraintChanged.DisconnectObject(this);
	GetIEditor()->GetLevelEditorSharedState()->signalCoordSystemChanged.DisconnectObject(this);
	GetIEditor()->GetLevelEditorSharedState()->signalEditModeChanged.DisconnectObject(this);
}

void CTransformManipulator::SetUpGizmos()
{
	// Arrow gizmos
	float arrowOffset = 0.08f;

	m_xMoveAxis.SetOffset(arrowOffset);
	m_xMoveAxis.signalBeginDrag.Connect(this, &CTransformManipulator::BeginMoveDrag);
	m_xMoveAxis.signalDragging.Connect(this, &CTransformManipulator::MoveDragging);
	m_xMoveAxis.signalEndDrag.Connect(this, &CTransformManipulator::EndDrag);

	m_yMoveAxis.SetOffset(arrowOffset);
	m_yMoveAxis.signalBeginDrag.Connect(this, &CTransformManipulator::BeginMoveDrag);
	m_yMoveAxis.signalDragging.Connect(this, &CTransformManipulator::MoveDragging);
	m_yMoveAxis.signalEndDrag.Connect(this, &CTransformManipulator::EndDrag);

	m_zMoveAxis.SetOffset(arrowOffset);
	m_zMoveAxis.signalBeginDrag.Connect(this, &CTransformManipulator::BeginMoveDrag);
	m_zMoveAxis.signalDragging.Connect(this, &CTransformManipulator::MoveDragging);
	m_zMoveAxis.signalEndDrag.Connect(this, &CTransformManipulator::EndDrag);

	// Plane gizmos
	float planeOffset = 0.25f;
	float planeScale = 0.2f;

	m_xyMovePlane.SetXOffset(planeOffset);
	m_xyMovePlane.SetYOffset(planeOffset);
	m_xyMovePlane.SetScale(planeScale);
	m_xyMovePlane.signalBeginDrag.Connect(this, &CTransformManipulator::BeginMoveDrag);
	m_xyMovePlane.signalDragging.Connect(this, &CTransformManipulator::MoveDragging);
	m_xyMovePlane.signalEndDrag.Connect(this, &CTransformManipulator::EndDrag);

	m_yzMovePlane.SetXOffset(planeOffset);
	m_yzMovePlane.SetYOffset(planeOffset);
	m_yzMovePlane.SetScale(planeScale);
	m_yzMovePlane.signalBeginDrag.Connect(this, &CTransformManipulator::BeginMoveDrag);
	m_yzMovePlane.signalDragging.Connect(this, &CTransformManipulator::MoveDragging);
	m_yzMovePlane.signalEndDrag.Connect(this, &CTransformManipulator::EndDrag);

	m_zxMovePlane.SetXOffset(planeOffset);
	m_zxMovePlane.SetYOffset(planeOffset);
	m_zxMovePlane.SetScale(planeScale);
	m_zxMovePlane.signalBeginDrag.Connect(this, &CTransformManipulator::BeginMoveDrag);
	m_zxMovePlane.signalDragging.Connect(this, &CTransformManipulator::MoveDragging);
	m_zxMovePlane.signalEndDrag.Connect(this, &CTransformManipulator::EndDrag);

	m_viewMoveGizmo.SetScale(arrowOffset);
	m_viewMoveGizmo.signalBeginDrag.Connect(this, &CTransformManipulator::BeginMoveDrag);
	m_viewMoveGizmo.signalDragging.Connect(this, &CTransformManipulator::MoveDragging);
	m_viewMoveGizmo.signalEndDrag.Connect(this, &CTransformManipulator::EndDrag);

	// rotation wheel gizmos
	float wheelScale = 0.75f;
	float outerWheelScale = 0.85f;
	float trackballScale = outerWheelScale;
	float rotateAxisScale = wheelScale * 0.5f;

	m_xWheelGizmo.SetScale(wheelScale);
	m_xWheelGizmo.signalBeginDrag.Connect(this, &CTransformManipulator::BeginRotateDrag);
	m_xWheelGizmo.signalDragging.Connect(this, &CTransformManipulator::RotateDragging);
	m_xWheelGizmo.signalEndDrag.Connect(this, &CTransformManipulator::EndDrag);

	m_yWheelGizmo.SetScale(wheelScale);
	m_yWheelGizmo.signalBeginDrag.Connect(this, &CTransformManipulator::BeginRotateDrag);
	m_yWheelGizmo.signalDragging.Connect(this, &CTransformManipulator::RotateDragging);
	m_yWheelGizmo.signalEndDrag.Connect(this, &CTransformManipulator::EndDrag);

	m_zWheelGizmo.SetScale(wheelScale);
	m_zWheelGizmo.signalBeginDrag.Connect(this, &CTransformManipulator::BeginRotateDrag);
	m_zWheelGizmo.signalDragging.Connect(this, &CTransformManipulator::RotateDragging);
	m_zWheelGizmo.signalEndDrag.Connect(this, &CTransformManipulator::EndDrag);

	m_trackballGizmo.SetScale(trackballScale);
	m_trackballGizmo.signalBeginDrag.Connect(this, &CTransformManipulator::BeginRotateDrag);
	m_trackballGizmo.signalDragging.Connect(this, &CTransformManipulator::RotateDragging);
	m_trackballGizmo.signalEndDrag.Connect(this, &CTransformManipulator::EndDrag);

	m_viewRotationGizmo.SetScale(outerWheelScale);
	m_viewRotationGizmo.SetScreenAligned(true);
	m_viewRotationGizmo.signalBeginDrag.Connect(this, &CTransformManipulator::BeginRotateDrag);
	m_viewRotationGizmo.signalDragging.Connect(this, &CTransformManipulator::RotateDragging);
	m_viewRotationGizmo.signalEndDrag.Connect(this, &CTransformManipulator::EndDrag);

	m_xRotateAxis.SetScale(rotateAxisScale);
	m_yRotateAxis.SetScale(rotateAxisScale);
	m_zRotateAxis.SetScale(rotateAxisScale);

	// scale gizmos
	m_xScaleAxis.SetOffset(arrowOffset);
	m_xScaleAxis.signalBeginDrag.Connect(this, &CTransformManipulator::BeginScaleDrag);
	m_xScaleAxis.signalDragging.Connect(this, &CTransformManipulator::ScaleDragging);
	m_xScaleAxis.signalEndDrag.Connect(this, &CTransformManipulator::EndDrag);

	m_yScaleAxis.SetOffset(arrowOffset);
	m_yScaleAxis.signalBeginDrag.Connect(this, &CTransformManipulator::BeginScaleDrag);
	m_yScaleAxis.signalDragging.Connect(this, &CTransformManipulator::ScaleDragging);
	m_yScaleAxis.signalEndDrag.Connect(this, &CTransformManipulator::EndDrag);

	m_zScaleAxis.SetOffset(arrowOffset);
	m_zScaleAxis.signalBeginDrag.Connect(this, &CTransformManipulator::BeginScaleDrag);
	m_zScaleAxis.signalDragging.Connect(this, &CTransformManipulator::ScaleDragging);
	m_zScaleAxis.signalEndDrag.Connect(this, &CTransformManipulator::EndDrag);

	float planeOffsetInner = 0.5f;
	float planeStripScale = 0.3f;

	m_xyScalePlane.SetScale(planeStripScale);
	m_xyScalePlane.SetOffsetInner(planeOffsetInner);
	m_xyScalePlane.SetOffsetFromCenter(planeOffset);
	m_xyScalePlane.signalBeginDrag.Connect(this, &CTransformManipulator::BeginScaleDrag);
	m_xyScalePlane.signalDragging.Connect(this, &CTransformManipulator::ScaleDragging);
	m_xyScalePlane.signalEndDrag.Connect(this, &CTransformManipulator::EndDrag);

	m_yzScalePlane.SetScale(planeStripScale);
	m_yzScalePlane.SetOffsetInner(planeOffsetInner);
	m_yzScalePlane.SetOffsetFromCenter(planeOffset);
	m_yzScalePlane.signalBeginDrag.Connect(this, &CTransformManipulator::BeginScaleDrag);
	m_yzScalePlane.signalDragging.Connect(this, &CTransformManipulator::ScaleDragging);
	m_yzScalePlane.signalEndDrag.Connect(this, &CTransformManipulator::EndDrag);

	m_zxScalePlane.SetScale(planeStripScale);
	m_zxScalePlane.SetOffsetInner(planeOffsetInner);
	m_zxScalePlane.SetOffsetFromCenter(planeOffset);
	m_zxScalePlane.signalBeginDrag.Connect(this, &CTransformManipulator::BeginScaleDrag);
	m_zxScalePlane.signalDragging.Connect(this, &CTransformManipulator::ScaleDragging);
	m_zxScalePlane.signalEndDrag.Connect(this, &CTransformManipulator::EndDrag);

	m_xyzScaleGizmo.SetScale(arrowOffset);
	m_xyzScaleGizmo.signalBeginDrag.Connect(this, &CTransformManipulator::BeginScaleDrag);
	m_xyzScaleGizmo.signalDragging.Connect(this, &CTransformManipulator::ScaleDragging);
	m_xyzScaleGizmo.signalEndDrag.Connect(this, &CTransformManipulator::EndDrag);

	m_highlightedGizmo = nullptr;
	m_lastHitGizmo = nullptr;

	UpdateGizmos();
	UpdateGizmoColors();
}

void CTransformManipulator::UpdateGizmoColors()
{
	Vec3 red = Vec3(1.0f, 0.0f, 0.0f);
	Vec3 green = Vec3(0.0f, 1.0f, 0.0f);
	Vec3 blue = Vec3(0.0f, 0.0f, 1.0f);
	Vec3 yellow = Vec3(1.0f, 1.0f, 0.0f);
	Vec3 white = Vec3(1.0f, 1.0f, 1.0f);

	m_xMoveAxis.SetColor(red);
	m_yMoveAxis.SetColor(green);
	m_zMoveAxis.SetColor(blue);

	m_xyMovePlane.SetColor(blue);
	m_yzMovePlane.SetColor(red);
	m_zxMovePlane.SetColor(green);

	m_viewMoveGizmo.SetColor(yellow);

	m_viewRotationGizmo.SetColor(yellow);

	m_xWheelGizmo.SetColor(red);
	m_yWheelGizmo.SetColor(green);
	m_zWheelGizmo.SetColor(blue);

	m_xRotateAxis.SetColor(red);
	m_yRotateAxis.SetColor(green);
	m_zRotateAxis.SetColor(blue);

	m_trackballGizmo.SetColor(yellow);

	m_xScaleAxis.SetColor(red);
	m_yScaleAxis.SetColor(green);
	m_zScaleAxis.SetColor(blue);
	m_xyScalePlane.SetColor(blue);
	m_yzScalePlane.SetColor(red);
	m_zxScalePlane.SetColor(green);
	m_xyzScaleGizmo.SetColor(yellow);

	if (!m_bSkipConstraintColor)
	{
		switch (GetIEditor()->GetLevelEditorSharedState()->GetAxisConstraint())
		{
		case CLevelEditorSharedState::Axis::X:
			m_xMoveAxis.SetColor(white);
			m_xWheelGizmo.SetColor(white);
			m_xScaleAxis.SetColor(white);
			break;
		case CLevelEditorSharedState::Axis::Y:
			m_yMoveAxis.SetColor(white);
			m_yScaleAxis.SetColor(white);
			m_yWheelGizmo.SetColor(white);
			break;
		case CLevelEditorSharedState::Axis::Z:
			m_zMoveAxis.SetColor(white);
			m_zWheelGizmo.SetColor(white);
			m_zScaleAxis.SetColor(white);
			break;
		case CLevelEditorSharedState::Axis::XY:
			m_xyMovePlane.SetColor(white);
			m_xyScalePlane.SetColor(white);
			break;
		case CLevelEditorSharedState::Axis::YZ:
			m_yzMovePlane.SetColor(white);
			m_yzScalePlane.SetColor(white);
			break;
		case CLevelEditorSharedState::Axis::XZ:
			m_zxMovePlane.SetColor(white);
			m_zxScalePlane.SetColor(white);
			break;
		case CLevelEditorSharedState::Axis::XYZ:
			m_xyzScaleGizmo.SetColor(white);
			break;
		case CLevelEditorSharedState::Axis::View:
			m_viewMoveGizmo.SetColor(white);
			m_viewRotationGizmo.SetColor(white);
			break;
		}
	}
}

void CTransformManipulator::UpdateGizmos()
{
	Vec3 position = m_matrix.GetTranslation();
	CLevelEditorSharedState::EditMode editMode = GetIEditor()->GetLevelEditorSharedState()->GetEditMode();

	if (editMode == CLevelEditorSharedState::EditMode::Move)
	{
		m_xMoveAxis.SetPosition(position);
		m_xMoveAxis.SetDirection(m_matrix.GetColumn0());

		m_yMoveAxis.SetPosition(position);
		m_yMoveAxis.SetDirection(m_matrix.GetColumn1());

		m_zMoveAxis.SetPosition(position);
		m_zMoveAxis.SetDirection(m_matrix.GetColumn2());

		m_xyMovePlane.SetPosition(position);
		m_xyMovePlane.SetXVector(m_matrix.GetColumn0());
		m_xyMovePlane.SetYVector(m_matrix.GetColumn1());

		m_yzMovePlane.SetPosition(position);
		m_yzMovePlane.SetXVector(m_matrix.GetColumn1());
		m_yzMovePlane.SetYVector(m_matrix.GetColumn2());

		m_zxMovePlane.SetPosition(position);
		m_zxMovePlane.SetXVector(m_matrix.GetColumn2());
		m_zxMovePlane.SetYVector(m_matrix.GetColumn0());

		m_viewMoveGizmo.SetPosition(position);
		m_viewMoveGizmo.SetRotationAxis(m_matrix.GetColumn0(), m_matrix.GetColumn1());
	}
	else if (editMode == CLevelEditorSharedState::EditMode::Rotate)
	{
		m_xWheelGizmo.SetPosition(position);
		m_xWheelGizmo.SetAxis(m_matrix.GetColumn0());
		m_xWheelGizmo.SetAlignmentAxis(m_matrix.GetColumn1());

		m_yWheelGizmo.SetPosition(position);
		m_yWheelGizmo.SetAxis(m_matrix.GetColumn1());
		m_yWheelGizmo.SetAlignmentAxis(m_matrix.GetColumn2());

		m_zWheelGizmo.SetPosition(position);
		m_zWheelGizmo.SetAxis(m_matrix.GetColumn2());
		m_zWheelGizmo.SetAlignmentAxis(m_matrix.GetColumn0());

		m_viewRotationGizmo.SetPosition(position);
		m_trackballGizmo.SetPosition(position);

		Matrix34 manipulatorMatrix;
		if (!m_owner->GetManipulatorMatrix(manipulatorMatrix))
			manipulatorMatrix = m_matrix;

		m_xRotateAxis.SetPosition(position);
		m_xRotateAxis.SetDirection(manipulatorMatrix.GetColumn0());

		m_yRotateAxis.SetPosition(position);
		m_yRotateAxis.SetDirection(manipulatorMatrix.GetColumn1());

		m_zRotateAxis.SetPosition(position);
		m_zRotateAxis.SetDirection(manipulatorMatrix.GetColumn2());
	}
	else if (editMode == CLevelEditorSharedState::EditMode::Scale)
	{
		m_xScaleAxis.SetPosition(position);
		m_xScaleAxis.SetDirection(m_matrix.GetColumn0());
		m_xScaleAxis.SetUpAxis(m_matrix.GetColumn1());

		m_yScaleAxis.SetPosition(position);
		m_yScaleAxis.SetDirection(m_matrix.GetColumn1());
		m_yScaleAxis.SetUpAxis(m_matrix.GetColumn2());

		m_zScaleAxis.SetPosition(position);
		m_zScaleAxis.SetDirection(m_matrix.GetColumn2());
		m_zScaleAxis.SetUpAxis(m_matrix.GetColumn0());

		m_xyScalePlane.SetPosition(position);
		m_xyScalePlane.SetXVector(m_matrix.GetColumn0());
		m_xyScalePlane.SetYVector(m_matrix.GetColumn1());

		m_yzScalePlane.SetPosition(position);
		m_yzScalePlane.SetXVector(m_matrix.GetColumn1());
		m_yzScalePlane.SetYVector(m_matrix.GetColumn2());

		m_zxScalePlane.SetPosition(position);
		m_zxScalePlane.SetXVector(m_matrix.GetColumn2());
		m_zxScalePlane.SetYVector(m_matrix.GetColumn0());

		m_xyzScaleGizmo.SetPosition(position);
		m_xyzScaleGizmo.SetXVector(m_matrix.GetColumn2());
		m_xyzScaleGizmo.SetYVector(m_matrix.GetColumn0());
	}
}

void CTransformManipulator::UpdateTransform()
{
	// if we have a custom transform our matrix is already as it should be
	if (m_bUseCustomTransform)
	{
		return;
	}

	CLevelEditorSharedState::CoordSystem coordSystem = GetIEditor()->GetLevelEditorSharedState()->GetCoordSystem();

	Vec3 position;
	// TODO: This should not be necessary, but many tools have a separate step for getting a transform and also getting
	// the real position of the transform manipulator and then expecting the original matrix to be modified here.
	// GetManipulatorMatrix() should already return a matrix with the correct position data.
	m_owner->GetManipulatorPosition(position);
	if (coordSystem == CLevelEditorSharedState::CoordSystem::Local || coordSystem == CLevelEditorSharedState::CoordSystem::Parent)
	{
		// attempt to get local or parent matrix from gizmo owner. If none is provided, just use world matrix instead
		if (!m_owner->GetManipulatorMatrix(m_matrix))
		{
			m_matrix.SetIdentity();
		}
	}
	else if (coordSystem == CLevelEditorSharedState::CoordSystem::World)
	{
		m_matrix.SetIdentity();
	}
	m_matrix.SetTranslation(position);
}

void CTransformManipulator::Display(SDisplayContext& dc)
{
	if (!m_owner->IsManipulatorVisible())
	{
		return;
	}

	bool bUpdateGizmos = false;

	if (GetFlag(EGIZMO_INVALID))
	{
		UnsetFlag(EGIZMO_INVALID);
		UpdateTransform();
		bUpdateGizmos = true;
	}

	// view coordinates need to be calculated each frame
	if (GetIEditor()->GetLevelEditorSharedState()->GetCoordSystem() == CLevelEditorSharedState::CoordSystem::View && !m_bUseCustomTransform)
	{
		Vec3 position = m_matrix.GetTranslation();
		Vec3 direction = -dc.view->ViewDirection();
		Vec3 upVec = dc.view->UpViewDirection();
		Vec3 xVec = upVec ^ direction;
		direction = -dc.view->CameraToWorld(position);

		m_matrix.SetFromVectors(xVec, upVec, direction, position);
		bUpdateGizmos = true;
	}

	if (bUpdateGizmos)
	{
		UpdateGizmos();
	}

	// Only enable axis planes when editor is in Move mode.
	CLevelEditorSharedState::EditMode editMode = GetIEditor()->GetLevelEditorSharedState()->GetEditMode();
	int nModeFlags = 0;
	switch (editMode)
	{
	case CLevelEditorSharedState::EditMode::Move:
		nModeFlags |= CAxisHelper::MOVE_FLAG;
		break;
	case CLevelEditorSharedState::EditMode::Rotate:
		nModeFlags |= CAxisHelper::ROTATE_FLAG;
		break;
	case CLevelEditorSharedState::EditMode::Scale:
		nModeFlags |= CAxisHelper::SCALE_FLAG;
		break;
	case CLevelEditorSharedState::EditMode::Select:
		nModeFlags |= CAxisHelper::SELECT_FLAG;
		break;
	case CLevelEditorSharedState::EditMode::SelectArea:
		nModeFlags |= CAxisHelper::SELECT_FLAG;
		break;
	}

	if (m_highlightedGizmo && m_highlightedGizmo->GetFlag(EGIZMO_INTERACTING))
	{
		m_highlightedGizmo->Display(dc);

		if (editMode == CLevelEditorSharedState::EditMode::Move)
		{
			DisplayPivotPoint(dc);
		}
		else if (editMode == CLevelEditorSharedState::EditMode::Rotate)
		{
			m_xRotateAxis.Display(dc);
			m_yRotateAxis.Display(dc);
			m_zRotateAxis.Display(dc);
		}

		if (editMode == CLevelEditorSharedState::EditMode::Move && !dc.display2D)
		{
			bool bClickedShift = QtUtil::IsModifierKeyDown(Qt::ShiftModifier);
			if (bClickedShift)
			{
				m_pAxisHelperExtended.DrawAxes(dc, m_matrix);
			}
		}
		return;
	}

	// first try for new gizmo code!
	if (editMode == CLevelEditorSharedState::EditMode::Move)
	{
		m_viewMoveGizmo.Display(dc);

		m_xMoveAxis.Display(dc);
		m_yMoveAxis.Display(dc);
		m_zMoveAxis.Display(dc);

		m_xyMovePlane.Display(dc);
		m_yzMovePlane.Display(dc);
		m_zxMovePlane.Display(dc);

		if (!dc.display2D)
		{
			bool bClickedShift = QtUtil::IsModifierKeyDown(Qt::ShiftModifier);
			if (bClickedShift)
			{
				m_pAxisHelperExtended.DrawAxes(dc, m_matrix);
			}
		}
	}
	else if (editMode == CLevelEditorSharedState::EditMode::Rotate)
	{
		m_trackballGizmo.Display(dc);

		m_xRotateAxis.Display(dc);
		m_yRotateAxis.Display(dc);
		m_zRotateAxis.Display(dc);

		m_xWheelGizmo.Display(dc);
		m_yWheelGizmo.Display(dc);
		m_zWheelGizmo.Display(dc);

		m_viewRotationGizmo.Display(dc);
	}
	else if (editMode == CLevelEditorSharedState::EditMode::Scale)
	{
		m_xScaleAxis.Display(dc);
		m_yScaleAxis.Display(dc);
		m_zScaleAxis.Display(dc);

		m_xyScalePlane.Display(dc);
		m_yzScalePlane.Display(dc);
		m_zxScalePlane.Display(dc);

		m_xyzScaleGizmo.Display(dc);
	}
}

void CTransformManipulator::DisplayPivotPoint(SDisplayContext& dc)
{
	uint32 curflags = dc.GetState();
	dc.DepthTestOff();

	Vec3 position = m_matrix.GetTranslation();
	Vec3 direction = -dc.view->ViewDirection();
	Vec3 upVec = dc.view->UpViewDirection();
	Vec3 xVec = upVec ^ direction;
	direction = -dc.view->CameraToWorld(position);
	float scale = dc.view->GetScreenScaleFactor(position) * gGizmoPreferences.axisGizmoSize;

	Matrix34 tm;
	tm.SetFromVectors(xVec, upVec, direction, position);
	dc.PushMatrix(tm);
	float lineWidth = dc.GetLineWidth();
	dc.SetColor(Vec3(0.f, 0.f, 0.f));
	dc.SetLineWidth(0.02 * scale);
	dc.DrawDisc(0.03 * scale);
	dc.SetColor(Vec3(1.f, 1.f, 1.f));
	dc.DrawDisc(0.02 * scale);
	dc.SetLineWidth(lineWidth);
	dc.PopMatrix();

	dc.SetStateFlag(curflags);
}

void CTransformManipulator::SetWorldBounds(const AABB& bbox)
{
	m_bbox = bbox;
}

void CTransformManipulator::GetWorldBounds(AABB& bbox)
{
	// TODO: substitute with scaled bounding box (will need very careful evaluation order)
	{
		bbox.min = Vec3(-1000000, -1000000, -1000000);
		bbox.max = Vec3(1000000, 1000000, 1000000);
	}
}

void CTransformManipulator::SetMatrix(const Matrix34& m)
{
	m_matrix = m;
}

bool CTransformManipulator::HitTest(HitContext& hc, EManipulatorMode& manipulatorMode)
{
	CLevelEditorSharedState::EditMode editMode = GetIEditor()->GetLevelEditorSharedState()->GetEditMode();

	m_lastHitGizmo = nullptr;

	if (editMode == CLevelEditorSharedState::EditMode::Move)
	{
		if (m_viewMoveGizmo.HitTest(hc))
		{
			m_lastHitGizmo = &m_viewMoveGizmo;
			manipulatorMode = MOVE_MODE;
			hc.axis = CLevelEditorSharedState::Axis::View;
			return true;
		}

		if (m_xMoveAxis.HitTest(hc))
		{
			m_lastHitGizmo = &m_xMoveAxis;
			manipulatorMode = MOVE_MODE;
			hc.axis = CLevelEditorSharedState::Axis::X;
			return true;
		}
		if (m_yMoveAxis.HitTest(hc))
		{
			m_lastHitGizmo = &m_yMoveAxis;
			manipulatorMode = MOVE_MODE;
			hc.axis = CLevelEditorSharedState::Axis::Y;
			return true;
		}
		if (m_zMoveAxis.HitTest(hc))
		{
			m_lastHitGizmo = &m_zMoveAxis;
			manipulatorMode = MOVE_MODE;
			hc.axis = CLevelEditorSharedState::Axis::Z;
			return true;
		}

		if (m_xyMovePlane.HitTest(hc))
		{
			m_lastHitGizmo = &m_xyMovePlane;
			manipulatorMode = MOVE_MODE;
			hc.axis = CLevelEditorSharedState::Axis::XY;
			return true;
		}
		if (m_yzMovePlane.HitTest(hc))
		{
			m_lastHitGizmo = &m_yzMovePlane;
			manipulatorMode = MOVE_MODE;
			hc.axis = CLevelEditorSharedState::Axis::YZ;
			return true;
		}
		if (m_zxMovePlane.HitTest(hc))
		{
			m_lastHitGizmo = &m_zxMovePlane;
			manipulatorMode = MOVE_MODE;
			hc.axis = CLevelEditorSharedState::Axis::XZ;
			return true;
		}

		return false;
	}
	else if (editMode == CLevelEditorSharedState::EditMode::Rotate)
	{
		if (m_xWheelGizmo.HitTest(hc))
		{
			m_lastHitGizmo = &m_xWheelGizmo;
			manipulatorMode = ROTATE_MODE;
			hc.axis = CLevelEditorSharedState::Axis::X;
			return true;
		}
		if (m_yWheelGizmo.HitTest(hc))
		{
			m_lastHitGizmo = &m_yWheelGizmo;
			manipulatorMode = ROTATE_MODE;
			hc.axis = CLevelEditorSharedState::Axis::Y;
			return true;
		}
		if (m_zWheelGizmo.HitTest(hc))
		{
			m_lastHitGizmo = &m_zWheelGizmo;
			manipulatorMode = ROTATE_MODE;
			hc.axis = CLevelEditorSharedState::Axis::Z;
			return true;
		}
		if (m_viewRotationGizmo.HitTest(hc))
		{
			m_lastHitGizmo = &m_viewRotationGizmo;
			manipulatorMode = ROTATE_MODE;
			hc.axis = CLevelEditorSharedState::Axis::View;
			return true;
		}
		if (m_trackballGizmo.HitTest(hc))
		{
			m_lastHitGizmo = &m_trackballGizmo;
			manipulatorMode = ROTATE_MODE;
			hc.axis = CLevelEditorSharedState::Axis::XYZ;
			return true;
		}
		return false;
	}
	else if (editMode == CLevelEditorSharedState::EditMode::Scale)
	{
		if (m_xyzScaleGizmo.HitTest(hc))
		{
			m_lastHitGizmo = &m_xyzScaleGizmo;
			manipulatorMode = SCALE_MODE;
			hc.axis = CLevelEditorSharedState::Axis::XYZ;
			return true;
		}

		if (m_xScaleAxis.HitTest(hc))
		{
			m_lastHitGizmo = &m_xScaleAxis;
			manipulatorMode = SCALE_MODE;
			hc.axis = CLevelEditorSharedState::Axis::X;
			return true;
		}
		if (m_yScaleAxis.HitTest(hc))
		{
			m_lastHitGizmo = &m_yScaleAxis;
			manipulatorMode = SCALE_MODE;
			hc.axis = CLevelEditorSharedState::Axis::Y;
			return true;
		}
		if (m_zScaleAxis.HitTest(hc))
		{
			m_lastHitGizmo = &m_zScaleAxis;
			manipulatorMode = SCALE_MODE;
			hc.axis = CLevelEditorSharedState::Axis::Z;
			return true;
		}
		if (m_xyScalePlane.HitTest(hc))
		{
			m_lastHitGizmo = &m_xyScalePlane;
			manipulatorMode = SCALE_MODE;
			hc.axis = CLevelEditorSharedState::Axis::XY;
			return true;
		}
		if (m_yzScalePlane.HitTest(hc))
		{
			m_lastHitGizmo = &m_yzScalePlane;
			manipulatorMode = SCALE_MODE;
			hc.axis = CLevelEditorSharedState::Axis::YZ;
			return true;
		}
		if (m_zxScalePlane.HitTest(hc))
		{
			m_lastHitGizmo = &m_zxScalePlane;
			manipulatorMode = SCALE_MODE;
			hc.axis = CLevelEditorSharedState::Axis::XZ;
			return true;
		}
	}
	return false;
}

bool CTransformManipulator::HitTest(HitContext& hc)
{
	if (IsDelete() || !m_owner->IsManipulatorVisible())
	{
		return false;
	}

	EManipulatorMode manipulatorMode;
	if (HitTest(hc, manipulatorMode))
	{
		return true;
	}

	return false;
}

Matrix34 CTransformManipulator::GetTransform() const
{
	return m_matrix;
}

void CTransformManipulator::SetCustomTransform(bool on, const Matrix34& m)
{
	m_matrix = m;
	m_bUseCustomTransform = on;

	UpdateGizmos();
}

bool CTransformManipulator::NeedsSnappingGrid() const
{
	return m_bDragging && GetIEditor()->GetLevelEditorSharedState()->GetEditMode() == CLevelEditorSharedState::EditMode::Move;
}

bool CTransformManipulator::MouseCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int nFlags)
{
	if (m_highlightedGizmo)
	{
		return m_highlightedGizmo->MouseCallback(view, event, point, nFlags);
	}
	else if (event == eMouseMove)
	{
		// Hit test current transform manipulator, to highlight when mouse over.
		HitContext hc(view);
		hc.point2d = point;
		view->ViewToWorldRay(point, hc.raySrc, hc.rayDir);

		EManipulatorMode manipulatorMode;
		HitTest(hc, manipulatorMode);
	}

	return false;
}

void CTransformManipulator::BeginMoveDrag(IDisplayViewport* view, CGizmo* gizmo, const Vec3& initialPosition, const CPoint& point, int nFlags)
{
	GetIEditor()->GetIUndoManager()->Begin();
	m_bDragging = true;
	m_cMouseDownPos = point;
	m_initPos = initialPosition;

	// remove constraint colors from display during interaction
	m_bSkipConstraintColor = true;
	if (gizmo == &m_xMoveAxis)
	{
		GetIEditor()->GetLevelEditorSharedState()->SetAxisConstraint(CLevelEditorSharedState::Axis::X);
	}
	else if (gizmo == &m_yMoveAxis)
	{
		GetIEditor()->GetLevelEditorSharedState()->SetAxisConstraint(CLevelEditorSharedState::Axis::Y);
	}
	else if (gizmo == &m_zMoveAxis)
	{
		GetIEditor()->GetLevelEditorSharedState()->SetAxisConstraint(CLevelEditorSharedState::Axis::Z);
	}
	else if (gizmo == &m_xyMovePlane)
	{
		GetIEditor()->GetLevelEditorSharedState()->SetAxisConstraint(CLevelEditorSharedState::Axis::XY);
	}
	else if (gizmo == &m_yzMovePlane)
	{
		GetIEditor()->GetLevelEditorSharedState()->SetAxisConstraint(CLevelEditorSharedState::Axis::YZ);
	}
	else if (gizmo == &m_zxMovePlane)
	{
		GetIEditor()->GetLevelEditorSharedState()->SetAxisConstraint(CLevelEditorSharedState::Axis::XZ);
	}
	else if (gizmo == &m_viewMoveGizmo)
	{
		GetIEditor()->GetLevelEditorSharedState()->SetAxisConstraint(CLevelEditorSharedState::Axis::View);
	}

	view->SetConstructionMatrix(m_matrix);

	Vec2i ipoint(point.x, point.y);
	signalBeginDrag(view, this, ipoint, nFlags);

	SetFlag(EGIZMO_INTERACTING);
}

void CTransformManipulator::MoveDragging(IDisplayViewport* view, CGizmo* gizmo, const Vec3& offset, const CPoint& point, int nFlags)
{
	Vec2i ipoint(point.x, point.y);
	signalDragging(view, this, ipoint, offset, nFlags);
}

void CTransformManipulator::BeginRotateDrag(IDisplayViewport* view, CGizmo* gizmo, const CPoint& point, int nFlags)
{
	GetIEditor()->GetIUndoManager()->Begin();
	m_bDragging = true;
	m_cMouseDownPos = point;
	m_initMatrix = Matrix33(m_matrix);
	m_initMatrixInverse = m_initMatrix.GetInverted();
	// remove constraint colors from display during interaction
	m_bSkipConstraintColor = true;
	if (gizmo == &m_xWheelGizmo)
	{
		GetIEditor()->GetLevelEditorSharedState()->SetAxisConstraint(CLevelEditorSharedState::Axis::X);
	}
	else if (gizmo == &m_yWheelGizmo)
	{
		GetIEditor()->GetLevelEditorSharedState()->SetAxisConstraint(CLevelEditorSharedState::Axis::Y);
	}
	else if (gizmo == &m_zWheelGizmo)
	{
		GetIEditor()->GetLevelEditorSharedState()->SetAxisConstraint(CLevelEditorSharedState::Axis::Z);
	}
	else if (gizmo == &m_viewRotationGizmo)
	{
		GetIEditor()->GetLevelEditorSharedState()->SetAxisConstraint(CLevelEditorSharedState::Axis::View);
	}
	else if (gizmo == &m_trackballGizmo)
	{
		GetIEditor()->GetLevelEditorSharedState()->SetAxisConstraint(CLevelEditorSharedState::Axis::XYZ);
	}

	Vec2i ipoint(point.x, point.y);
	signalBeginDrag(view, this, ipoint, nFlags);

	SetFlag(EGIZMO_INTERACTING);
}

void CTransformManipulator::RotateDragging(IDisplayViewport* view, CGizmo* gizmo, const AngleAxis& rotationAxis, const CPoint& point, int nFlags)
{
	Vec3 vDragValue(0.0f, 0.0f, 0.0f);

	// in view space coordinates, we create a rotation in world space
	if (GetIEditor()->GetLevelEditorSharedState()->GetCoordSystem() == CLevelEditorSharedState::CoordSystem::View)
	{
		Matrix33 customSpaceRot = Matrix33::CreateRotationAA(rotationAxis.angle, rotationAxis.axis);
		Ang3 ang(customSpaceRot);
		vDragValue = ang;
	}
	else if (gizmo == &m_viewRotationGizmo || gizmo == &m_trackballGizmo)
	{
		// custom transforms need more care, because users will expect that the vectors will describe an euler decomposition in
		// their own custom space
		if (m_bUseCustomTransform)
		{
			// rotationAxis is in world space, so what we do is apply the matrix transform first, to bring the transform space to global,
			// then apply the rotation itself which is in global space, then invert back to the custom space
			Matrix33 customSpaceRot = m_initMatrixInverse * Matrix33::CreateRotationAA(rotationAxis.angle, rotationAxis.axis) * m_initMatrix;
			Ang3 ang(customSpaceRot);
			vDragValue = ang;
		}
		else
		{
			Quat q = Quat::CreateRotationAA(rotationAxis.angle, rotationAxis.axis);
			Ang3 ang(q);
			vDragValue = ang;
		}
	}
	else if (gizmo == &m_xWheelGizmo)
	{
		vDragValue.x = rotationAxis.angle;
	}
	else if (gizmo == &m_yWheelGizmo)
	{
		vDragValue.y = rotationAxis.angle;
	}
	else if (gizmo == &m_zWheelGizmo)
	{
		vDragValue.z = rotationAxis.angle;
	}

	Vec2i ipoint(point.x, point.y);
	signalDragging(view, this, ipoint, vDragValue, nFlags);
}

void CTransformManipulator::BeginScaleDrag(IDisplayViewport* view, CGizmo* gizmo, const CPoint& point, int nFlags)
{
	GetIEditor()->GetIUndoManager()->Begin();
	m_bDragging = true;
	m_cMouseDownPos = point;

	// remove constraint colors from display during interaction
	m_bSkipConstraintColor = true;
	if (gizmo == &m_xScaleAxis)
	{
		GetIEditor()->GetLevelEditorSharedState()->SetAxisConstraint(CLevelEditorSharedState::Axis::X);
	}
	else if (gizmo == &m_yScaleAxis)
	{
		GetIEditor()->GetLevelEditorSharedState()->SetAxisConstraint(CLevelEditorSharedState::Axis::Y);
	}
	else if (gizmo == &m_zScaleAxis)
	{
		GetIEditor()->GetLevelEditorSharedState()->SetAxisConstraint(CLevelEditorSharedState::Axis::Z);
	}
	if (gizmo == &m_xyScalePlane)
	{
		GetIEditor()->GetLevelEditorSharedState()->SetAxisConstraint(CLevelEditorSharedState::Axis::XY);
	}
	else if (gizmo == &m_yzScalePlane)
	{
		GetIEditor()->GetLevelEditorSharedState()->SetAxisConstraint(CLevelEditorSharedState::Axis::YZ);
	}
	else if (gizmo == &m_zxScalePlane)
	{
		GetIEditor()->GetLevelEditorSharedState()->SetAxisConstraint(CLevelEditorSharedState::Axis::XZ);
	}
	else if (gizmo == &m_xyzScaleGizmo)
	{
		GetIEditor()->GetLevelEditorSharedState()->SetAxisConstraint(CLevelEditorSharedState::Axis::XYZ);
	}

	Vec2i ipoint(point.x, point.y);
	signalBeginDrag(view, this, ipoint, nFlags);

	SetFlag(EGIZMO_INTERACTING);
}

void CTransformManipulator::ScaleDragging(IDisplayViewport* view, CGizmo* gizmo, float scale, const CPoint& point, int nFlags)
{
	Vec3 vDragValue(1.0f, 1.0f, 1.0f);

	if (gizmo == &m_xScaleAxis)
	{
		vDragValue.x = scale;
	}
	else if (gizmo == &m_yScaleAxis)
	{
		vDragValue.y = scale;
	}
	else if (gizmo == &m_zScaleAxis)
	{
		vDragValue.z = scale;
	}
	else if (gizmo == &m_xyScalePlane)
	{
		vDragValue.x = scale;
		vDragValue.y = scale;
	}
	else if (gizmo == &m_yzScalePlane)
	{
		vDragValue.y = scale;
		vDragValue.z = scale;
	}
	else if (gizmo == &m_zxScalePlane)
	{
		vDragValue.z = scale;
		vDragValue.x = scale;
	}
	else if (gizmo == &m_xyzScaleGizmo)
	{
		vDragValue.x = scale;
		vDragValue.y = scale;
		vDragValue.z = scale;
	}

	Vec2i ipoint(point.x, point.y);
	signalDragging(view, this, ipoint, vDragValue, nFlags);
}

void CTransformManipulator::EndDrag(IDisplayViewport* view, CGizmo* gizmo, const CPoint& point, int nFlags)
{
	if (m_bDragging)
	{
		m_bDragging = false;
		UnsetFlag(EGIZMO_INTERACTING);
		// request gizmo position again after transform ends (transformed object may be far from gizmo position)
		Invalidate();

		signalEndDrag(view, this);

		m_bSkipConstraintColor = false;

		GetIEditor()->GetIUndoManager()->Accept("Manipulator Drag");

		// update colors to take constraint color into account
		UpdateGizmoColors();
	}
}

void CTransformManipulator::OnUpdateState()
{
	// if we are interacting skip update; it will happen after we finish dragging anyway.
	if (!m_bDragging)
	{
		SetHighlighted(false);
		Invalidate();
	}
}

void CTransformManipulator::SetHighlighted(bool highlighted)
{
	if (!highlighted)
	{
		if (m_highlightedGizmo)
		{
			m_highlightedGizmo->SetHighlighted(false);
		}
		m_highlightedGizmo = nullptr;
	}
	else
	{
		m_highlightedGizmo = m_lastHitGizmo;
		if (m_highlightedGizmo)
		{
			m_highlightedGizmo->SetHighlighted(true);
		}
	}

	CGizmo::SetHighlighted(highlighted);
}
