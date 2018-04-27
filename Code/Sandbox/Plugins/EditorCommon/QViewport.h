// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <QWidget>

#include <CryMath/Cry_Math.h>
#include <CryMath/Cry_Matrix34.h>
#include <CryRenderer/IRenderer.h>
#include "QViewportEvents.h"
#include <CrySerialization/Forward.h>
#include "Gizmos/GizmoManager.h"
#include "DisplayViewportAdapter.h"

struct DisplayContext;
class CCamera;
struct SRenderingPassInfo;
struct SRendParams;
struct Ray;
struct IRenderer;
struct I3DEngine;
struct SSystemGlobalEnvironment;

using Serialization::IArchive;

struct SKeyEvent;
struct SMouseEvent;
struct SViewportSettings;
struct SViewportState;

class QViewport;
struct SRenderContext
{
	CCamera*            camera;
	QViewport*          viewport;
	SRendParams*        renderParams;
	SRenderingPassInfo* passInfo;
	IRenderAuxGeom*     pAuxGeom;
};

class QViewportConsumer;
class EDITOR_COMMON_API QViewport : public QWidget
{
	Q_OBJECT
public:
	QViewport(SSystemGlobalEnvironment* env, QWidget* parent, int supersamplingFactor = 1);
	~QViewport();

	void                     AddConsumer(QViewportConsumer* consumer);
	void                     RemoveConsumer(QViewportConsumer* consumer);

	void                     CaptureMouse();
	void                     ReleaseMouse();
	void                     SetForegroundUpdateMode(bool foregroundUpdate);
	CCamera*                 Camera() const { return m_camera.get(); }
	void                     ResetCamera();
	void                     Serialize(IArchive& ar);

	void                     SetSceneDimensions(const Vec3& size) { m_sceneDimensions = size; }
	void                     SetSettings(const SViewportSettings& settings);
	const SViewportSettings& GetSettings() const                  { return *m_settings; }
	void                     SetState(const SViewportState& state);
	const SViewportState&    GetState() const                     { return *m_state; }
	void                     SetOrbitMode(bool orbitMode);
	bool                     ScreenToWorldRay(Ray* ray, int x, int y);
	QPoint                   ProjectToScreen(const Vec3& point);
	void                     LookAt(const Vec3& target, float radius, bool snap);

	int                      Width() const;
	int                      Height() const;

	IGizmoManager*           GetGizmoManager() { return &m_gizmoManager; }

public slots:
	void Update();
signals:
	void SignalPreRender(const SRenderContext&);
	void SignalRender(const SRenderContext&);
	void SignalKey(const SKeyEvent&);
	void SignalMouse(const SMouseEvent&);
	void SignalUpdate();
	void SignalCameraMoved(const QuatT& qt);
protected:
	void mousePressEvent(QMouseEvent* ev) override;
	void mouseReleaseEvent(QMouseEvent* ev) override;
	void wheelEvent(QWheelEvent* ev) override;
	void mouseMoveEvent(QMouseEvent* ev) override;
	void keyPressEvent(QKeyEvent* ev) override;
	void keyReleaseEvent(QKeyEvent* ev) override;
	void resizeEvent(QResizeEvent* ev) override;
	void moveEvent(QMoveEvent* ev) override;
	void paintEvent(QPaintEvent* ev) override;
	bool event(QEvent* ev) override;
	bool winEvent(MSG* message, long* result);
private:
	struct SPrivate;

	void  IEditorEventFromQViewportEvent(const SMouseEvent& qEvt, CPoint& p, EMouseEvent& evt, int& flags);

	void  CameraMoved(QuatT qt, bool snap);
	bool  CreateRenderContext();
	void  DestroyRenderContext();
	void  SetCurrentContext();
	void  RestorePreviousContext();
	void  InitDisplayContext(HWND hWnd);
	void  UpdateBackgroundColor();

	void  CreateGridLine(ColorB col, const float alpha, const float alphaFalloff, const float slide, const float halfSlide, const float maxSlide, const Vec3& stepDir, const Vec3& orthoDir);
	void  CreateGridLines(const uint count, const uint interStepCount, const Vec3& stepDir, const float stepSize, const Vec3& orthoDir, const float offset);
	void  DrawGrid();
	void  DrawOrigin(const ColorB& col);
	void  DrawOrigin(const int left, const int top, const float scale, const Matrix34 cameraTM);

	void  ProcessMouse();
	void  ProcessKeys();
	void  PreRender();
	void  Render();
	void  RenderInternal();
	bool  OnMouseEvent(const SMouseEvent& ev);
	void  OnKeyEvent(const SKeyEvent& ev);
	float CalculateMoveSpeed(bool shiftPressed, bool ctrlPressed, bool bBackForth = false) const;
	void  CreateLookAt(const Vec3& target, float radius, QuatT& cameraTarget) const;
	struct SPreviousContext;
	std::vector<SPreviousContext>      m_previousContexts;
	std::unique_ptr<CCamera>           m_camera;
	DisplayContext                     m_displayContext;
	SDisplayContextKey                 m_displayContextKey;

	int                                m_width;
	int                                m_height;
	int                                m_supersamplingFactor;
	QPoint                             m_mousePressPos;
	int64                              m_lastTime;
	float                              m_lastFrameTime;
	float                              m_averageFrameTime;
	bool                               m_renderContextCreated;
	bool                               m_creatingRenderContext;
	bool                               m_updating;
	bool                               m_rotationMode;
	bool                               m_panMode;
	bool                               m_orbitModeEnabled;
	bool                               m_orbitMode;
	bool                               m_fastMode;
	bool                               m_slowMode;

	Vec3                               m_cameraSmoothPosRate;
	float                              m_cameraSmoothRotRate;
	int                                m_mouseMovementsSinceLastFrame;
	f32                                m_LightRotationRadian;
	SMouseEvent                        m_pendingMouseMoveEvent;

	Vec3                               m_sceneDimensions;
	std::unique_ptr<SPrivate>          m_private;
	std::unique_ptr<SViewportSettings> m_settings;
	std::unique_ptr<SViewportState>    m_state;
	std::vector<QViewportConsumer*>    m_consumers;
	std::vector<Vec3>                  m_gridLineVertices;
	std::vector<uint32>                m_gridLineVerticesColor;
	SSystemGlobalEnvironment*          m_env;
	IRenderAuxGeom*                    m_pAuxGeom;

	std::unique_ptr<CDisplayViewportAdapter> m_pViewportAdapter;
	CGizmoManager                            m_gizmoManager;
};

