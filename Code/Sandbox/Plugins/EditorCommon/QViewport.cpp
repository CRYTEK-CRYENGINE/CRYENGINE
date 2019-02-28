// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <CryMath/Cry_Camera.h>
#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CrySystem/ITimer.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryPhysics/IPhysicsDebugRenderer.h>
#include <IEditor.h>

#include <QMouseEvent>

#include "QtUtil.h"
#include "QViewport.h"
#include "QViewportEvents.h"
#include "QViewportConsumer.h"
#include "QViewportSettings.h"
#include "Serialization.h"
#include <QApplication>

#include "ViewportInteraction.h"
#include "RenderLock.h"

SERIALIZATION_ENUM_BEGIN(ECameraTransformRestraint, "CameraTransformRestraint")
SERIALIZATION_ENUM(eCameraTransformRestraint_Rotation, "Rotation", "Rotation")
SERIALIZATION_ENUM(eCameraTransformRestraint_Panning, "Panning", "Panning")
SERIALIZATION_ENUM(eCameraTransformRestraint_Zoom, "Zoom", "Zoom")
SERIALIZATION_ENUM_END()

struct QViewport::SPreviousContext
{
	CCamera systemCamera;
	int     width;
	int     height;
	HWND    window;
	bool    mainViewport;
};

#pragma warning(disable: 4355) // 'this' : used in base member initializer list)

void QViewport::CreateGridLine(ColorB col, const float alpha, const float alphaFalloff, const float slide, const float halfSlide, const float maxSlide, const Vec3& stepDir, const Vec3& orthoDir)
{
	const SViewportState& state = *m_state;
	const SViewportGridSettings& gridSettings = m_settings->grid;

	ColorB colEnd = col;

	float weight = 1.0f - (slide / halfSlide);
	if (slide > halfSlide)
		weight = (slide - halfSlide) / halfSlide;

	float orthoWeight = 1.0f;

	if (gridSettings.circular)
	{
		float invWeight = 1.0f - weight;
		orthoWeight = sqrtf((invWeight * 2) - (invWeight * invWeight));
	}
	else
		orthoWeight = 1.0f;

	col.a = (1.0f - (weight * (1.0f - alphaFalloff))) * alpha;
	colEnd.a = alphaFalloff * alpha;

	Vec3 orthoStep = state.gridOrigin.q * (orthoDir * halfSlide * orthoWeight);

	Vec3 point = state.gridOrigin * (-(stepDir * halfSlide) + (stepDir * slide));
	Vec3 points[3] = {
		point,
		point - orthoStep,
		point + orthoStep
	};

	const uint32 cPacked = col.pack_argb8888();
	const uint32 cEndPacked = colEnd.pack_argb8888();
	m_gridLineVertices.push_back(points[0]); m_gridLineVerticesColor.push_back(cPacked);
	m_gridLineVertices.push_back(points[1]); m_gridLineVerticesColor.push_back(cEndPacked);
	m_gridLineVertices.push_back(points[0]); m_gridLineVerticesColor.push_back(cPacked);
	m_gridLineVertices.push_back(points[2]); m_gridLineVerticesColor.push_back(cEndPacked);
}

void QViewport::CreateGridLines(const uint count, const uint interStepCount, const Vec3& stepDir, const float stepSize, const Vec3& orthoDir, const float offset)
{
	const SViewportGridSettings& gridSettings = m_settings->grid;

	const uint countHalf = count / 2;
	Vec3 step = stepDir * stepSize;
	Vec3 orthoStep = orthoDir * countHalf;
	Vec3 maxStep = step * countHalf;// + stepDir*fabs(offset);
	const float maxStepLen = count * stepSize;
	const float halfStepLen = countHalf * stepSize;

	float interStepSize = interStepCount > 0 ? (stepSize / interStepCount) : stepSize;
	const float alphaMulMain = (float)gridSettings.mainColor.a;
	const float alphaMulInter = (float)gridSettings.middleColor.a;
	const float alphaFalloff = 1.0f - (gridSettings.alphaFalloff / 100.0f);
	float orthoWeight = 1.0f;

	for (int i = 0; i < count + 2; i++)
	{
		float pointSlide = i * stepSize + offset;
		if (pointSlide > 0.0f && pointSlide < maxStepLen)
			CreateGridLine(gridSettings.mainColor, alphaMulMain, alphaFalloff, pointSlide, halfStepLen, maxStepLen, stepDir, orthoDir);

		for (int d = 1; d < interStepCount; d++)
		{
			float interSlide = ((i - 1) * stepSize) + offset + (d * interStepSize);
			if (interSlide > 0.0f && interSlide < maxStepLen)
				CreateGridLine(gridSettings.middleColor, alphaMulInter, alphaFalloff, interSlide, halfStepLen, maxStepLen, stepDir, orthoDir);
		}
	}
}

void QViewport::DrawGrid()
{
	const SViewportGridSettings& gridSettings = m_settings->grid;

	const uint count = gridSettings.count * 2;
	const float gridSize = gridSettings.spacing * gridSettings.count * 2.0f;
	const float stepSize = gridSize / count;
	
	m_gridLineVertices.clear();
	m_gridLineVerticesColor.clear();
	CreateGridLines(count, gridSettings.interCount, Vec3(1.0f, 0.0f, 0.0f), stepSize, Vec3(0.0f, 1.0f, 0.0f), m_state->gridCellOffset.x);
	CreateGridLines(count, gridSettings.interCount, Vec3(0.0f, 1.0f, 0.0f), stepSize, Vec3(1.0f, 0.0f, 0.0f), m_state->gridCellOffset.y);
	
	m_pAuxGeom->DrawLines( &m_gridLineVertices[0], &m_gridLineVerticesColor[0], (uint32)m_gridLineVertices.size() );
}

void QViewport::DrawOrigin(const ColorB& col)
{
	IRenderAuxGeom* aux = m_pAuxGeom;

	const float scale = 0.3f;
	const float lineWidth = 4.0f;
	aux->DrawLine(Vec3(-scale, 0, 0), col, Vec3(0, 0, 0), col, lineWidth);
	aux->DrawLine(Vec3(0, -scale, 0), col, Vec3(0, 0, 0), col, lineWidth);
	aux->DrawLine(Vec3(0, 0, -scale), col, Vec3(0, 0, 0), col, lineWidth);
	const ColorB cx(255, 0, 0, 255);
	const ColorB cy(0, 255, 0, 255);
	const ColorB cz(0, 0, 255, 255);
	aux->DrawLine(Vec3(0, 0, 0), cx, Vec3(scale, 0, 0), cx, lineWidth);
	aux->DrawLine(Vec3(0, 0, 0), cy, Vec3(0, scale, 0), cy, lineWidth);
	aux->DrawLine(Vec3(0, 0, 0), cz, Vec3(0, 0, scale), cz, lineWidth);
}

void QViewport::DrawOrigin(const int left, const int top, const float scale, const Matrix34 cameraTM)
{
	IRenderAuxGeom* aux = m_pAuxGeom;

	Vec3 originPos = Vec3(left, top, 0);
	Quat originRot = Quat(0.707107f, 0.707107f, 0, 0) * Quat(cameraTM).GetInverted();
	Vec3 x = originPos + originRot * Vec3(1, 0, 0) * scale;
	Vec3 y = originPos + originRot * Vec3(0, 1, 0) * scale;
	Vec3 z = originPos + originRot * Vec3(0, 0, 1) * scale;
	ColorF xCol(1, 0, 0);
	ColorF yCol(0, 1, 0);
	ColorF zCol(0, 0, 1);
	const float lineWidth = 2.0f;

	aux->DrawLine(originPos, xCol, x, xCol, lineWidth);
	aux->DrawLine(originPos, yCol, y, yCol, lineWidth);
	aux->DrawLine(originPos, zCol, z, zCol, lineWidth);
}

struct QViewport::SPrivate
{
	SRenderLight m_VPLight0;

};

QViewport::QViewport(SSystemGlobalEnvironment* env, QWidget* parent, int supersamplingFactor)
	: QWidget(parent)
	, m_renderContextCreated(false)
	, m_updating(false)
	, m_width(0)
	, m_height(0)
	, m_supersamplingFactor(supersamplingFactor)
	, m_rotationMode(false)
	, m_panMode(false)
	, m_orbitModeEnabled(false)
	, m_orbitMode(false)
	, m_fastMode(false)
	, m_slowMode(false)
	, m_lastTime(0)
	, m_lastFrameTime(0.0f)
	, m_averageFrameTime(0.0f)
	, m_sceneDimensions(1.0f, 1.0f, 1.0f)
	, m_creatingRenderContext(false)
	, m_env(env)
	, m_cameraSmoothPosRate(0)
	, m_cameraSmoothRotRate(0)
	, m_settings(new SViewportSettings())
	, m_state(new SViewportState())
	, m_mouseMovementsSinceLastFrame(0)
	, m_private(new SPrivate())
{
	if (!gEnv)
		gEnv = m_env; // Shhh!

	m_camera.reset(new CCamera());
	ResetCamera();

	m_mousePressPos = QCursor::pos();

	UpdateBackgroundColor();

	setUpdatesEnabled(false);
	setAttribute(Qt::WA_PaintOnScreen);
	setMouseTracking(true);
	m_LightRotationRadian = 0;

	m_pViewportAdapter.reset(new CDisplayViewportAdapter(this));
}

QViewport::~QViewport()
{
	DestroyRenderContext();
}

void QViewport::UpdateBackgroundColor()
{
	QPalette pal(palette());
	pal.setColor(QPalette::Background, QColor(m_settings->background.topColor.r,
	                                          m_settings->background.topColor.g,
	                                          m_settings->background.topColor.b,
	                                          m_settings->background.topColor.a));
	setPalette(pal);
	setAutoFillBackground(true);
}

void QViewport::SetOrbitMode(bool orbitMode)
{
	m_orbitModeEnabled = orbitMode;
}

bool QViewport::ScreenToWorldRay(Ray* ray, int x, int y)
{
	if (!m_env->pRenderer)
		return false;

	Vec3 pos0(0,0,0), pos1(0,0,0);
	if (!Camera()->Unproject(Vec3(float(x), float(m_height - y), 0), pos0))
	{
		return false;
	}
	if (!Camera()->Unproject(Vec3(float(x), float(m_height - y), 1), pos1))
	{
		return false;
	}
	
	Vec3 v = (pos1 - pos0);
	v = v.GetNormalized();

	ray->origin = pos0;
	ray->direction = v;
	return true;
}

QPoint QViewport::ProjectToScreen(const Vec3& wp)
{
	Vec3 out(0, 0, 0);
	if (Camera()->Project(wp, out))
	{
		return QPoint((int)out.x,(int)out.y);
	}
	RestorePreviousContext();

	return QPoint(0, 0);
}

void QViewport::LookAt(const Vec3& target, float radius, bool snap)
{
	QuatT cameraTarget = m_state->cameraTarget;
	CreateLookAt(target, radius, cameraTarget);
	CameraMoved(cameraTarget, snap);
}

int QViewport::Width() const
{
	return QtUtil::PixelScale(this, rect().width());
}

int QViewport::Height() const
{
	return QtUtil::PixelScale(this, rect().height());
}

bool QViewport::CreateRenderContext()
{
	if (m_creatingRenderContext)
		return false;
	m_creatingRenderContext = true;
	DestroyRenderContext();
	HWND window = (HWND)QWidget::winId();

	// Create context.
	if (window && m_env->pRenderer && !m_renderContextCreated)
	{
		IRenderer::SDisplayContextDescription desc;

		desc.handle = window;
		desc.type = IRenderer::eViewportType_Secondary;
		desc.clearColor = ColorF(m_settings->background.topColor);
		desc.renderFlags = FRT_CLEAR | FRT_OVERLAY_DEPTH;
		desc.superSamplingFactor.x = m_supersamplingFactor;
		desc.superSamplingFactor.y = m_supersamplingFactor;
		desc.screenResolution.x = m_width;
		desc.screenResolution.y = m_height;

		m_displayContextKey = gEnv->pRenderer->CreateSwapChainBackedContext(desc);

		m_renderContextCreated = true;
		m_creatingRenderContext = false;

		SetCurrentContext();
		return true;
	}

	m_creatingRenderContext = false;
	return false;
}

void QViewport::DestroyRenderContext()
{
	// Destroy render context.
	if (m_env->pRenderer && m_renderContextCreated)
	{
		// Do not delete primary context.
		if (m_displayContextKey != static_cast<HWND>(m_env->pRenderer->GetHWND()))
			m_env->pRenderer->DeleteContext(m_displayContextKey);

		m_renderContextCreated = false;
	}
}

void QViewport::SetCurrentContext()
{
	if (m_camera.get() == 0)
		return;

	SPreviousContext previous;
	previous.systemCamera = m_env->pSystem->GetViewCamera();
	m_env->pSystem->SetViewCamera(*m_camera);
	m_previousContexts.push_back(previous);
}

void QViewport::RestorePreviousContext()
{
	if (!m_camera.get())
		return;
	if (m_previousContexts.empty())
	{
		ASSERT(0);
		return;
	}
	SPreviousContext x = m_previousContexts.back();
	m_previousContexts.pop_back();
	m_env->pSystem->SetViewCamera(x.systemCamera);
}

void QViewport::InitDisplayContext(HWND hWnd)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	// Draw all objects.
	SDisplayContextKey displayContextKey;
	displayContextKey.key.emplace<HWND>(hWnd);
	DisplayContext& dctx = m_displayContext;
	dctx.SetDisplayContext(displayContextKey);
	dctx.SetView(m_pViewportAdapter.get());
	dctx.SetCamera(m_camera.get());
	dctx.renderer = m_env->pRenderer;
	dctx.engine = m_env->p3DEngine;
	dctx.box.min = Vec3(-100000, -100000, -100000);
	dctx.box.max = Vec3(100000, 100000, 100000);
	dctx.flags = 0;
}

void QViewport::Serialize(IArchive& ar)
{
	if (!ar.isEdit())
	{
		ar(m_state->cameraTarget, "cameraTarget", "Camera Target");
	}
}

struct AutoBool
{
	AutoBool(bool* value)
		: m_value(value)
	{
		* m_value = true;
	}

	~AutoBool()
	{
		* m_value = false;
	}

	bool* m_value;
};

void QViewport::Update()
{
	int64 time = m_env->pSystem->GetITimer()->GetAsyncTime().GetMilliSecondsAsInt64();
	if (m_lastTime == 0)
		m_lastTime = time;
	m_lastFrameTime = (time - m_lastTime) * 0.001f;
	m_lastTime = time;
	if (m_averageFrameTime == 0.0f)
		m_averageFrameTime = m_lastFrameTime;
	else
		m_averageFrameTime = 0.01f * m_lastFrameTime + 0.99f * m_averageFrameTime;

	if (m_env->pRenderer == 0 ||
	    m_env->p3DEngine == 0)
		return;

	if (!isVisible())
		return;

	if (!m_width || !m_height)
		return;

	if (!m_renderContextCreated && !m_creatingRenderContext)
		CreateRenderContext();

	if (m_updating)
		return;

	AutoBool updating(&m_updating);

	if (hasFocus())
	{
		ProcessMouse();
		ProcessKeys();
	}

	RenderInternal();
}

void QViewport::CaptureMouse()
{
	grabMouse();
}

void QViewport::ReleaseMouse()
{
	releaseMouse();
}

void QViewport::SetForegroundUpdateMode(bool foregroundUpdate)
{
	//m_timer->setInterval(foregroundUpdate ? 2 : 50);
}

void QViewport::ProcessMouse()
{
	QPoint point = mapFromGlobal(QCursor::pos());

	if (point == m_mousePressPos)
	{
		return;
	}

	float speedScale = CalculateMoveSpeed(m_fastMode, m_slowMode);

	if ((m_rotationMode && m_panMode) /* || m_bInZoomMode*/)
	{
		if (!(m_settings->camera.transformRestraint & eCameraTransformRestraint_Zoom))
		{
			// Zoom.
			QuatT qt = m_state->cameraTarget;
			Vec3 xdir(0, 0, 0);

			Vec3 ydir = qt.GetColumn1().GetNormalized();
			Vec3 pos = qt.t;
			pos = pos - 0.2f * ydir * (m_mousePressPos.y() - point.y()) * speedScale;
			qt.t = pos;
			CameraMoved(qt, false);

			QCursor::setPos(mapToGlobal(m_mousePressPos));
		}
	}
	else if (m_rotationMode)
	{
		if (!(m_settings->camera.transformRestraint & eCameraTransformRestraint_Rotation))
		{
			Ang3 angles(-point.y() + m_mousePressPos.y(), 0, -point.x() + m_mousePressPos.x());
			angles = angles * 0.001f * m_settings->camera.rotationSpeed;

			QuatT qt = m_state->cameraTarget;
			Ang3 ypr = CCamera::CreateAnglesYPR(Matrix33(qt.q));
			ypr.x += angles.z;
			ypr.y += angles.x;
			ypr.y = clamp_tpl(ypr.y, -1.5f, 1.5f);

			qt.q = Quat(CCamera::CreateOrientationYPR(ypr));
			CameraMoved(qt, false);

			QCursor::setPos(mapToGlobal(m_mousePressPos));
		}
	}
	else if (m_panMode)
	{
		if (!(m_settings->camera.transformRestraint & eCameraTransformRestraint_Panning))
		{
			// Slide.
			QuatT qt = m_state->cameraTarget;
			Vec3 xdir = qt.GetColumn0().GetNormalized();
			Vec3 zdir = qt.GetColumn2().GetNormalized();

			Vec3 pos = qt.t;
			pos += 0.0025f * xdir * (point.x() - m_mousePressPos.x()) * speedScale + 0.0025f * zdir * (m_mousePressPos.y() - point.y()) * speedScale;
			qt.t = pos;
			CameraMoved(qt, false);

			QCursor::setPos(mapToGlobal(m_mousePressPos));
		}
	}
	else if (m_orbitMode)
	{
		// Rotate around orbit target.
		QuatT cameraTarget = m_state->cameraTarget;
		Vec3 at = cameraTarget.t - m_state->orbitTarget;
		float distanceFromTarget = at.GetLength();
		if (distanceFromTarget > 0.001f)
		{
			at /= distanceFromTarget;
		}
		else
		{
			at = Vec3(0.0f, m_state->orbitRadius, 0.0f);
			distanceFromTarget = m_state->orbitRadius;
		}

		Vec3 up = Vec3(0.0f, 0.0f, 1.0f);
		const Vec3 right = at.Cross(up).GetNormalized();
		up = right.Cross(at).GetNormalized();

		Ang3 angles = CCamera::CreateAnglesYPR(Matrix33::CreateFromVectors(right, at, up));
		const Ang3 delta = Ang3(-point.y() + m_mousePressPos.y(), 0.0f, -point.x() + m_mousePressPos.x()) * 0.002f * m_settings->camera.rotationSpeed;
		angles.x += delta.z;
		angles.y -= delta.x;
		angles.y = clamp_tpl(angles.y, -1.5f, 1.5f);

		cameraTarget.t = m_state->orbitTarget + CCamera::CreateOrientationYPR(angles).TransformVector(Vec3(0.0f, distanceFromTarget, 0.0f));

		CameraMoved(cameraTarget, true);

		QCursor::setPos(mapToGlobal(m_mousePressPos));
	}
}

void QViewport::ProcessKeys()
{
	if (!m_renderContextCreated)
		return;

	float deltaTime = m_lastFrameTime;

	if (deltaTime > 0.1f)
		deltaTime = 0.1f;

	QuatT qt = m_state->cameraTarget;
	Vec3 ydir = qt.GetColumn1().GetNormalized();
	Vec3 xdir = qt.GetColumn0().GetNormalized();
	Vec3 pos = qt.t;

	float moveSpeed = CalculateMoveSpeed(m_fastMode, m_slowMode);
	float moveBackForthSpeed = CalculateMoveSpeed(m_fastMode, m_slowMode, true);
	bool hasPressedKey = false;

	if (ViewportInteraction::CheckPolledKey(ViewportInteraction::eKey_Forward))
	{
		hasPressedKey = true;
		qt.t = qt.t + deltaTime * moveBackForthSpeed * ydir;
		CameraMoved(qt, false);
	}

	if (ViewportInteraction::CheckPolledKey(ViewportInteraction::eKey_Backward))
	{
		hasPressedKey = true;
		qt.t = qt.t - deltaTime * moveBackForthSpeed * ydir;
		CameraMoved(qt, false);
	}

	if (!m_orbitMode && ViewportInteraction::CheckPolledKey(ViewportInteraction::eKey_Left))
	{
		hasPressedKey = true;
		qt.t = qt.t - deltaTime * moveSpeed * xdir;
		CameraMoved(qt, false);
	}

	if (!m_orbitMode && ViewportInteraction::CheckPolledKey(ViewportInteraction::eKey_Right))
	{
		hasPressedKey = true;
		qt.t = qt.t + deltaTime * moveSpeed * xdir;
		CameraMoved(qt, false);
	}

	if (QGuiApplication::mouseButtons() & (Qt::LeftButton | Qt::RightButton))
	{
		hasPressedKey = true;
	}
}

void QViewport::CameraMoved(QuatT qt, bool snap)
{
	if (m_orbitMode)
	{
		CreateLookAt(m_state->orbitTarget, m_state->orbitRadius, qt);
	}
	m_state->cameraTarget = qt;
	if (snap)
	{
		m_state->lastCameraTarget = qt;
	}
	SignalCameraMoved(qt);
}

void QViewport::OnKeyEvent(const SKeyEvent& ev)
{
	for (size_t i = 0; i < m_consumers.size(); ++i)
		m_consumers[i]->OnViewportKey(ev);
	SignalKey(ev);
}

bool QViewport::OnMouseEvent(const SMouseEvent& ev)
{
	if (ev.type == SMouseEvent::TYPE_MOVE)
	{
		// Make sure we don't process more than one mouse event per frame, so we don't
		// end up consuming all the "idle" time
		++m_mouseMovementsSinceLastFrame;

		if (m_mouseMovementsSinceLastFrame > 1)
		{
			// we can't discard all movement events, the last one should be delivered.
			m_pendingMouseMoveEvent = ev;
			return false;
		}
	}

	CPoint p;
	EMouseEvent evt;
	int flags;
	IEditorEventFromQViewportEvent(ev, p, evt, flags);

	if (m_gizmoManager.HandleMouseInput(m_pViewportAdapter.get(), evt, p, flags))
	{
		return true;
	}

	for (size_t i = 0; i < m_consumers.size(); ++i)
	{
		m_consumers[i]->OnViewportMouse(ev);
	}
	SignalMouse(ev);

	return false;
}

void QViewport::PreRender()
{
	SRenderContext rc;
	rc.camera = m_camera.get();
	rc.viewport = this;
	rc.pAuxGeom = m_pAuxGeom;

	SignalPreRender(rc);

	const float fov = DEG2RAD(m_settings->camera.fov);
	const float fTime = m_env->pTimer->GetFrameTime();
	float lastRotWeight = 0.0f;

	QuatT targetTM = m_state->cameraTarget;
	QuatT currentTM = m_state->lastCameraTarget;

	if ((targetTM.t - currentTM.t).len() > 0.0001f)
		SmoothCD(currentTM.t, m_cameraSmoothPosRate, fTime, targetTM.t, m_settings->camera.smoothPos);
	else
		m_cameraSmoothPosRate = Vec3(0);

	SmoothCD(lastRotWeight, m_cameraSmoothRotRate, fTime, 1.0f, m_settings->camera.smoothRot);

	if (lastRotWeight >= 1.0f)
		m_cameraSmoothRotRate = 0.0f;

	currentTM = QuatT(Quat::CreateNlerp(currentTM.q, targetTM.q, lastRotWeight), currentTM.t);

	m_state->lastCameraParentFrame = m_state->cameraParentFrame;
	m_state->lastCameraTarget = currentTM;

	m_camera->SetFrustum(m_width, m_height, fov, m_settings->camera.nearClip, m_env->p3DEngine->GetMaxViewDistance());
	m_camera->SetMatrix(Matrix34(m_state->cameraParentFrame * currentTM));
}

// TODO: Either move it somewhere reusable or completely remove IEditor or QViewport style events
void QViewport::IEditorEventFromQViewportEvent(const SMouseEvent& qEvt, CPoint& p, EMouseEvent& evt, int& flags)
{
	p.x = qEvt.x;
	p.y = qEvt.y;
	flags = 0;

	if (qEvt.control)
	{
		flags |= MK_CONTROL;
	}
	if (qEvt.shift)
	{
		flags |= MK_SHIFT;
	}

	if (qEvt.type == SMouseEvent::TYPE_MOVE)
	{
		evt = eMouseMove;
	}
	else if (qEvt.type == SMouseEvent::TYPE_PRESS)
	{
		if (qEvt.button == SMouseEvent::BUTTON_LEFT)
		{
			evt = eMouseLDown;
		}
		else if (qEvt.button == SMouseEvent::BUTTON_RIGHT)
		{
			evt = eMouseRDown;
		}
		else if (qEvt.button == SMouseEvent::BUTTON_MIDDLE)
		{
			evt = eMouseMDown;
		}
	}
	else if (qEvt.type == SMouseEvent::TYPE_RELEASE)
	{
		if (qEvt.button == SMouseEvent::BUTTON_LEFT)
		{
			evt = eMouseLUp;
		}
		else if (qEvt.button == SMouseEvent::BUTTON_RIGHT)
		{
			evt = eMouseRUp;
		}
		else if (qEvt.button == SMouseEvent::BUTTON_MIDDLE)
		{
			evt = eMouseMUp;
		}
	}
}

struct ScopedBackup
{
	ScopedBackup(IRenderAuxGeom** pDCAuxGeomPtr, IRenderAuxGeom* pNewAuxGeom)
	{
		ppDCAuxGeom = pDCAuxGeomPtr;
		oldAuxGeom = *pDCAuxGeomPtr;
		*pDCAuxGeomPtr = pNewAuxGeom;
	}
	~ScopedBackup()
	{
		*ppDCAuxGeom = oldAuxGeom;
	}

	IRenderAuxGeom** ppDCAuxGeom;
	IRenderAuxGeom* oldAuxGeom;
};

void QViewport::Render()
{
	DisplayContext& dc = m_displayContext;
	ScopedBackup(&m_displayContext.pRenderAuxGeom, m_pAuxGeom);

	IRenderAuxGeom* aux = m_pAuxGeom;
	SAuxGeomRenderFlags oldFlags = aux->GetRenderFlags();

	dc.SetState(e_Mode3D | e_AlphaBlended | e_FillModeSolid | e_CullModeBack | e_DepthWriteOff | e_DepthTestOn);

	// wireframe mode
	CScopedWireFrameMode scopedWireFrame(m_env->pRenderer, m_settings->rendering.wireframe ? R_WIREFRAME_MODE : R_SOLID_MODE);

	SRenderingPassInfo passInfo = SRenderingPassInfo::CreateGeneralPassRenderingInfo(*m_camera, SRenderingPassInfo::DEFAULT_FLAGS, true, dc.GetDisplayContextKey());

	if (m_settings->background.useGradient)
	{
		Vec3 frustumVertices[8];
		aux->GetCamera().GetFrustumVertices(frustumVertices);
		Vec3 lt = Vec3::CreateLerp(frustumVertices[0], frustumVertices[4], 0.10f);
		Vec3 lb = Vec3::CreateLerp(frustumVertices[1], frustumVertices[5], 0.10f);
		Vec3 rb = Vec3::CreateLerp(frustumVertices[2], frustumVertices[6], 0.10f);
		Vec3 rt = Vec3::CreateLerp(frustumVertices[3], frustumVertices[7], 0.10f);
		aux->SetRenderFlags(e_Mode3D | e_AlphaNone | e_FillModeSolid | e_CullModeNone | e_DepthWriteOff | e_DepthTestOn);
		ColorB topColor = m_settings->background.topColor;
		ColorB bottomColor = m_settings->background.bottomColor;

		// convert colors from sRGB to linear to render the gradation to HDR render target instead of back buffer.
		ColorF tempColor = ColorF(topColor.r, topColor.g, topColor.b) / 255.0f;
		tempColor.srgb2rgb();
		topColor = ColorB(tempColor);
		tempColor = ColorF(bottomColor.r, bottomColor.g, bottomColor.b) / 255.0f;
		tempColor.srgb2rgb();
		bottomColor = ColorB(tempColor);

		aux->DrawTriangle(lt, topColor, rt, topColor, rb, bottomColor);
		aux->DrawTriangle(lb, bottomColor, rb, bottomColor, lt, topColor);
	}

	m_env->pRenderer->EF_StartEf(passInfo);

	SRendParams rp;
	rp.AmbientColor.r = m_settings->lighting.m_ambientColor.r / 255.0f * m_settings->lighting.m_brightness;
	rp.AmbientColor.g = m_settings->lighting.m_ambientColor.g / 255.0f * m_settings->lighting.m_brightness;
	rp.AmbientColor.b = m_settings->lighting.m_ambientColor.b / 255.0f * m_settings->lighting.m_brightness;

	//---------------------------------------------------------------------------------------
	//---- directional light    -------------------------------------------------------------
	//---------------------------------------------------------------------------------------

	if (m_settings->lighting.m_useLightRotation)
		m_LightRotationRadian += m_averageFrameTime;
	if (m_LightRotationRadian > gf_PI)
		m_LightRotationRadian = -gf_PI;

	Matrix33 LightRot33 = Matrix33::CreateRotationZ(m_LightRotationRadian);

	f32 lightMultiplier = m_settings->lighting.m_lightMultiplier;
	f32 lightSpecMultiplier = m_settings->lighting.m_lightSpecMultiplier;
	f32 lightRadius = 400;

	f32 lightOrbit = 15.0f;
	Vec3 LPos0 = Vec3(-lightOrbit, lightOrbit, lightOrbit / 2);
	m_private->m_VPLight0.SetPosition(LightRot33 * LPos0);

	Vec3 d0;
	d0.x = f32(m_settings->lighting.m_directionalLightColor.r) / 255.0f;
	d0.y = f32(m_settings->lighting.m_directionalLightColor.g) / 255.0f;
	d0.z = f32(m_settings->lighting.m_directionalLightColor.b) / 255.0f;
	m_private->m_VPLight0.m_Flags |= DLF_POINT;
	m_private->m_VPLight0.SetLightColor(ColorF(d0.x * lightMultiplier, d0.y * lightMultiplier, d0.z * lightMultiplier, 0));
	m_private->m_VPLight0.SetSpecularMult(lightSpecMultiplier);
	m_private->m_VPLight0.SetRadius(lightRadius);

	ColorB col;
	col.r = uint8(d0.x * 255);
	col.g = uint8(d0.y * 255);
	col.b = uint8(d0.z * 255);
	col.a = 255;
	aux->DrawSphere(m_private->m_VPLight0.m_Origin, 0.4f, col);

	m_private->m_VPLight0.m_Flags = DLF_SUN | DLF_DIRECTIONAL;
	m_env->pRenderer->EF_ADDDlight(&m_private->m_VPLight0, passInfo);

	//---------------------------------------------------------------------------------------

	Matrix34 tm(IDENTITY);
	rp.pMatrix = &tm;
	rp.pPrevMatrix = &tm;

	rp.dwFObjFlags = 0;
	rp.dwFObjFlags |= FOB_TRANS_MASK;

	SRenderContext rc;
	rc.camera = m_camera.get();
	rc.viewport = this;
	rc.passInfo = &passInfo;
	rc.renderParams = &rp;
	rc.pAuxGeom = aux;

	SignalRender(rc);

	m_gizmoManager.Display(dc);

	m_env->pSystem->GetIPhysicsDebugRenderer()->Flush(m_lastFrameTime);
	m_env->pRenderer->EF_EndEf3D(SHDF_ALLOWHDR | SHDF_SECONDARY_VIEWPORT, -1, -1, passInfo);

	if (m_settings->grid.showGrid)
	{
		aux->SetRenderFlags(e_Mode3D | e_AlphaBlended | e_FillModeSolid | e_CullModeNone | e_DepthWriteOff | e_DepthTestOn);
		DrawGrid();
	}

	if (m_settings->grid.origin)
	{
		aux->SetRenderFlags(e_Mode3D | e_AlphaBlended | e_FillModeSolid | e_CullModeNone | e_DepthWriteOff | e_DepthTestOn);
		DrawOrigin(m_settings->grid.originColor);
	}

	if (m_settings->camera.showViewportOrientation)
	{
		aux->SetRenderFlags(e_Mode3D | e_AlphaBlended | e_FillModeSolid | e_CullModeNone | e_DepthWriteOn | e_DepthTestOn);
		aux->SetOrthographicProjection(true, 0.0f, m_width, m_height, 0.0f);
		DrawOrigin(50, m_height - 50, 20.0f, m_camera->GetMatrix());
		aux->SetOrthographicProjection(false);
	}

	for (size_t i = 0; i < m_consumers.size(); ++i)
		m_consumers[i]->OnViewportRender(rc);

	aux->Submit();
	aux->SetRenderFlags(oldFlags);

	if ((m_settings->rendering.fps == true) && (m_averageFrameTime != 0.0f))
	{
		IRenderAuxText::Draw2dLabel(12.0f, 12.0f, 1.25f, ColorF(1, 1, 1, 1), false, "FPS: %.2f", 1.0f / m_averageFrameTime);
	}

	if (m_mouseMovementsSinceLastFrame > 1)
	{
		m_mouseMovementsSinceLastFrame = 0;

		// Make sure we deliver at least last mouse movement event
		OnMouseEvent(m_pendingMouseMoveEvent);
	}
}

void QViewport::RenderInternal()
{
	// lock while we are rendering to prevent any recursive rendering across the application
	if (CScopedRenderLock lock = CScopedRenderLock())
	{
		const HWND hWnd = reinterpret_cast<HWND>(QWidget::winId());
		SDisplayContextKey displayContextKey;
		displayContextKey.key.emplace<HWND>(hWnd);

		SetCurrentContext();

		// Configures Aux to draw to the current display-context
		InitDisplayContext(hWnd);

		// Request for a new aux geometry to capture aux commands in the current viewport
		CCamera camera = *m_camera;
		m_pAuxGeom = gEnv->pRenderer->GetOrCreateIRenderAuxGeom(&camera);

		m_env->pSystem->RenderBegin(displayContextKey);

		// Sets the current viewport's aux geometry display context
		m_pAuxGeom->SetCurrentDisplayContext(displayContextKey);

		// Do the pre-rendering. This call updates the member camera (applying transformation to the camera).
		PreRender();
		camera = *m_camera;

		// PreRender touches the camera, so we need to submit the aux geometries drawn with the old camera
		gEnv->pRenderer->SubmitAuxGeom(m_pAuxGeom, false);

		// Get an Aux geometry using newly updated camera
		// In addition set the default aux camera
		m_pAuxGeom = gEnv->pRenderer->GetOrCreateIRenderAuxGeom(&camera);
		gEnv->pRenderer->UpdateAuxDefaultCamera(*m_camera);

		// Do the actual render call for the viewport.
		Render();
		
		// Submit the current viewport's aux geometry and make it null to completely pass the ownership to renderer
		gEnv->pRenderer->SubmitAuxGeom(m_pAuxGeom, false);
		m_pAuxGeom = nullptr;

		bool renderStats = false;
		m_env->pSystem->RenderEnd(renderStats);

		RestorePreviousContext();
	}
}

void QViewport::ResetCamera()
{
	*m_state = SViewportState();
	m_camera->SetMatrix(Matrix34(m_state->cameraTarget));
}

void QViewport::SetSettings(const SViewportSettings& settings)
{
	*m_settings = settings;
}

void QViewport::SetState(const SViewportState& state)
{
	*m_state = state;
}

float QViewport::CalculateMoveSpeed(bool shiftPressed, bool ctrlPressed, bool bBackForth) const
{
	float maxDimension = max(0.1f, max(m_sceneDimensions.x, max(m_sceneDimensions.y, m_sceneDimensions.z)));
	float moveSpeed = max(0.01f, bBackForth && m_settings->camera.bIndependentBackForthSpeed ? m_settings->camera.moveBackForthSpeed : m_settings->camera.moveSpeed) * maxDimension;

	if (shiftPressed)
		moveSpeed *= m_settings->camera.fastMoveMultiplier;
	if (ctrlPressed)
		moveSpeed *= m_settings->camera.slowMoveMultiplier;

	return moveSpeed;
}

void QViewport::CreateLookAt(const Vec3& target, float radius, QuatT& cameraTarget) const
{
	Vec3 at = target - cameraTarget.t;
	float distanceFromTarget = at.GetLength();
	if (distanceFromTarget > 0.001f)
	{
		at /= distanceFromTarget;
	}
	else
	{
		at = Vec3(0.0f, 1.0f, 0.0f);
		distanceFromTarget = 0.0f;
	}

	if (distanceFromTarget < radius)
	{
		cameraTarget.t = target - (at * radius);
	}

	Vec3 up = Vec3(0.0f, 0.0f, 1.0f);
	const Vec3 right = at.Cross(up).GetNormalized();
	up = right.Cross(at).GetNormalized();
	cameraTarget.q = Quat(Matrix33::CreateFromVectors(right, at, up));
}

void QViewport::mousePressEvent(QMouseEvent* ev)
{
	SMouseEvent me;
	me.type = SMouseEvent::TYPE_PRESS;
	me.button = SMouseEvent::EButton(ev->button());
	me.x = QtUtil::PixelScale(this, ev->x());
	me.y = QtUtil::PixelScale(this, ev->y());
	me.viewport = this;
	me.shift = (ev->modifiers() & Qt::SHIFT) != 0;
	me.control = (ev->modifiers() & Qt::CTRL) != 0;
	bool bAccepted = OnMouseEvent(me);

	QWidget::mousePressEvent(ev);
	setFocus();

	m_mousePressPos = ev->pos();

	// return prematurely if the viewport has processed the event
	if (bAccepted)
	{
		return;
	}

	if (m_orbitModeEnabled && (ev->button() == Qt::LeftButton))
	{
		m_rotationMode = false;
		m_panMode = false;
		m_orbitMode = true;
		QApplication::setOverrideCursor(Qt::BlankCursor);
	}
	else
	{
		if (ev->button() == Qt::MiddleButton)
		{
			m_panMode = true;
			QApplication::setOverrideCursor(Qt::BlankCursor);
		}
		if (ev->button() == Qt::RightButton)
		{
			m_rotationMode = true;
			QApplication::setOverrideCursor(Qt::BlankCursor);
		}
	}
}

void QViewport::mouseReleaseEvent(QMouseEvent* ev)
{

	SMouseEvent me;
	me.type = SMouseEvent::TYPE_RELEASE;
	me.button = SMouseEvent::EButton(ev->button());
	me.x = QtUtil::PixelScale(this, ev->x());
	me.y = QtUtil::PixelScale(this, ev->y());
	me.viewport = this;
	OnMouseEvent(me);

	QWidget::mouseReleaseEvent(ev);

	if (ev->button() == Qt::LeftButton)
	{
		m_orbitMode = false;
	}
	if (ev->button() == Qt::MiddleButton)
	{
		m_panMode = false;
	}
	if (ev->button() == Qt::RightButton)
	{
		m_rotationMode = false;
	}
	QApplication::restoreOverrideCursor();
}

void QViewport::wheelEvent(QWheelEvent* ev)
{
	QuatT qt = m_state->cameraTarget;
	Vec3 ydir = qt.GetColumn1().GetNormalized();
	Vec3 pos = qt.t;
	const float wheelSpeed = m_settings->camera.zoomSpeed * (m_fastMode ? m_settings->camera.fastMoveMultiplier : 1.0f) * (m_slowMode ? m_settings->camera.slowMoveMultiplier : 1.0f);
	pos += 0.01f * ydir * ev->delta() * wheelSpeed;
	qt.t = pos;
	CameraMoved(qt, false);
}

void QViewport::mouseMoveEvent(QMouseEvent* ev)
{
	SMouseEvent me;
	me.type = SMouseEvent::TYPE_MOVE;
	me.button = SMouseEvent::EButton(ev->button());
	me.x = QtUtil::PixelScale(this, ev->x());
	me.y = QtUtil::PixelScale(this, ev->y());
	me.viewport = this;
	m_fastMode = (ev->modifiers() & Qt::SHIFT) != 0;
	m_slowMode = (ev->modifiers() & Qt::CTRL) != 0;
	OnMouseEvent(me);

	QWidget::mouseMoveEvent(ev);
}

void QViewport::keyPressEvent(QKeyEvent* ev)
{
	SKeyEvent event;
	event.type = SKeyEvent::TYPE_PRESS;
	event.key = ev->key() | ev->modifiers();
	m_fastMode = (ev->modifiers() & Qt::SHIFT) != 0;
	m_slowMode = (ev->modifiers() & Qt::CTRL) != 0;
	OnKeyEvent(event);

	QWidget::keyPressEvent(ev);
}

void QViewport::keyReleaseEvent(QKeyEvent* ev)
{
	SKeyEvent event;
	event.type = SKeyEvent::TYPE_RELEASE;
	event.key = ev->key() | ev->modifiers();
	m_fastMode = (ev->modifiers() & Qt::SHIFT) != 0;
	m_slowMode = (ev->modifiers() & Qt::CTRL) != 0;
	OnKeyEvent(event);
	QWidget::keyReleaseEvent(ev);
}

void QViewport::resizeEvent(QResizeEvent* ev)
{
	QWidget::resizeEvent(ev);

	int cx = ev->size().width() * devicePixelRatioF();
	int cy = ev->size().height() * devicePixelRatioF();
	if (cx == 0 || cy == 0)
		return;

	m_width = cx;
	m_height = cy;

	m_env->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_RESIZE, cx, cy);
	gEnv->pRenderer->ResizeContext(m_displayContextKey, m_width, m_height);

	SignalUpdate();
}

void QViewport::moveEvent(QMoveEvent* ev)
{
	QWidget::moveEvent(ev);

	m_env->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_MOVE, QtUtil::PixelScale(this, ev->pos().x()), QtUtil::PixelScale(this, ev->pos().y()));
}

bool QViewport::event(QEvent* ev)
{
	bool result = QWidget::event(ev);

	if (ev->type() == QEvent::WinIdChange)
	{
		CreateRenderContext();
	}

	return result;
}

void QViewport::paintEvent(QPaintEvent* ev)
{
	QWidget::paintEvent(ev);
}

bool QViewport::winEvent(MSG* message, long* result)
{
	return QWidget::nativeEvent("windows_generic_MSG", message, result);
}

void QViewport::AddConsumer(QViewportConsumer* consumer)
{
	RemoveConsumer(consumer);
	m_consumers.push_back(consumer);
}

void QViewport::RemoveConsumer(QViewportConsumer* consumer)
{
	m_consumers.erase(std::remove(m_consumers.begin(), m_consumers.end(), consumer), m_consumers.end());
}

