// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryExtension/CryGUID.h>

class CViewport;

// This interface is used to allow external plugins (e.g. TrackView) to control
// the viewport camera if the user selects it in the viewport menu.
struct ICameraDelegate
{
	virtual CryGUID     GetActiveCameraObjectGUID() const = 0;
	virtual const char* GetName() const = 0;
};

//! Class that manages state that must be shared to all viewports
//! This is somewhat of a legacy concept, and there shouldn't be too many 
//! shared things between viewports nowadays
struct IViewportManager
{
public:

	//! Ensures the game engine processes focus in this viewport
	//! For example this may influence audio
	virtual void SelectViewport(CViewport* pViewport) = 0;

	//! Get Viewport in focus
	virtual CViewport* GetSelectedViewport() const = 0;

	//! Assign zoom factor for 2d viewports.
	virtual void SetZoom2D(float zoom) = 0;

	//! Get zoom factor of 2d viewports.
	virtual float GetZoom2D() const = 0;

	//! Register a new instance of CViewport on construct
	virtual void RegisterViewport(CViewport* vp) = 0;

	//! Unregister an instance of CViewport on destruct
	virtual void UnregisterViewport(CViewport* vp) = 0;

	virtual int GetNumberOfGameViewports() = 0;

	virtual CryGUID GetCameraObjectId() const = 0;
	virtual void    SetCameraObjectId(CryGUID cameraObjectId) = 0;

	//! Retrieve main game viewport, where the full game is rendered in 3D.
	virtual CViewport* GetGameViewport() const = 0;
};
