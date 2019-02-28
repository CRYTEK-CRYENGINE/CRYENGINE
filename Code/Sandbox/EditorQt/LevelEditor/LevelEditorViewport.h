// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "RenderViewport.h"

class QViewportHeader;

//! LevelEditorViewport is a specialization of RenderViewport for the Level editor
//! In the past RenderViewport was the level editor viewport and also the base class for all other viewports
//! Which caused anti-patterns and a ton of issues. This was started as an attempt to sanitize viewport code
//! By progressively moving all level editor specific code here. See RenderViewport.h for other refactoring notes.
class CLevelEditorViewport : public CRenderViewport
{
public:
	CLevelEditorViewport();
	~CLevelEditorViewport();

	bool        CreateRenderContext(HWND hWnd, IRenderer::EViewportType viewportType = IRenderer::eViewportType_Default) override;

	void		SetHeaderWidget(QViewportHeader* headerWidget) { m_headerWidget = headerWidget; }

	bool		DragEvent(EDragEvent eventId, QEvent* event, int flags) override;
	void		PopulateMenu(CPopupMenuItem& menu);

	void        SetFOV(float fov);
	void        SetFOVDeg(float fov);
	float       GetFOV() const;

	void        SetSelectedCamera();
	bool        IsSelectedCamera() const;

	//! This switches the active camera to the next one in the list of (default, all custom cams).
	void        CycleCamera();

	void        CenterOnSelection() override;
	void		CenterOnAABB(AABB* const aabb) override;

	void		OnEditorNotifyEvent(EEditorNotifyEvent event) override;

	Vec3		ViewToWorld(POINT vp, bool* collideWithTerrain = 0, bool onlyTerrain = false, bool bSkipVegetation = false, bool bTestRenderMesh = false) const override;
	Vec3        ViewToWorldNormal(POINT vp, bool onlyTerrain, bool bTestRenderMesh = false) override;
	bool		MouseCallback(EMouseEvent event, CPoint& point, int flags) override;
private:


	void		OnCameraSpeedChanged() override;
	void        OnMenuCreateCameraFromCurrentView();
	void        OnMenuSelectCurrentCamera();
	void		OnRender() override;
	void        RenderAll(const SRenderingPassInfo& passInfo);
	void        RenderSnappingGrid();
	void        AddCameraMenuItems(CPopupMenuItem& menu);

	bool AssetDragCreate(EDragEvent eventId, QEvent* event, int flags);

	float                m_camFOV;
	QViewportHeader* m_headerWidget;
};
