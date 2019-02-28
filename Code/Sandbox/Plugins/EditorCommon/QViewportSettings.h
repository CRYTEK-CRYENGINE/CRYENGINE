// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Cry_Math.h>
#include <CryMath/Cry_Color.h>
#include "Serialization.h"

enum ECameraTransformRestraint
{
	eCameraTransformRestraint_Rotation = 0x01,
	eCameraTransformRestraint_Panning  = 0x02,
	eCameraTransformRestraint_Zoom     = 0x04
};

struct SViewportState
{
	QuatT cameraTarget;
	QuatT cameraParentFrame;
	QuatT gridOrigin;
	Vec3  gridCellOffset;
	QuatT lastCameraTarget;
	QuatT lastCameraParentFrame;

	Vec3  orbitTarget;
	float orbitRadius;

	SViewportState()
		: cameraParentFrame(IDENTITY)
		, gridOrigin(IDENTITY)
		, gridCellOffset(0)
		, lastCameraTarget(IDENTITY)
		, lastCameraParentFrame(IDENTITY)
		, orbitTarget(ZERO)
		, orbitRadius(1.0f)
	{
		cameraTarget = QuatT(Matrix34(-0.92322f, 0.37598f, 0.07935f, -0.88123f,
		                              -0.38426f, -0.90333f, -0.19065f, 2.19696f,
		                              0.0f, -0.2065f, 0.97845f, 1.58522f));

		lastCameraTarget = cameraTarget;
	}
};

struct SViewportRenderingSettings
{
	bool wireframe;
	bool fps;

	SViewportRenderingSettings()
		: wireframe(false)
		, fps(true)
	{
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(wireframe, "wireframe", "Wireframe");
		ar(fps, "fps", "Framerate");
	}
};

struct SViewportCameraSettings
{
	bool  showViewportOrientation;

	float fov;
	float nearClip;
	float smoothPos;
	float smoothRot;

	float moveSpeed;
	float moveBackForthSpeed;
	float rotationSpeed;
	float zoomSpeed;
	float fastMoveMultiplier;
	float slowMoveMultiplier;

	int   transformRestraint;

	// If true, camera movement speed is moveSpeed in x-direction and moveBackForthSpeed in y-direction.
	// Otherwise, it's moveSpeed in both directions.
	// TODO: Remove concept of different movement speeds. This is a hack for CryDesigner's UVMappingEditor.
	bool bIndependentBackForthSpeed;

	SViewportCameraSettings()
		: showViewportOrientation(true)
		, fov(60)
		, nearClip(0.01f)
		, smoothPos(0.07f)
		, smoothRot(0.05f)
		, moveSpeed(1.0f)
		, moveBackForthSpeed(1.0f)
		, rotationSpeed(2.0f)
		, zoomSpeed(0.1f)
		, fastMoveMultiplier(3.0f)
		, slowMoveMultiplier(0.1f)
		, transformRestraint(0)
		, bIndependentBackForthSpeed(false)
	{
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(showViewportOrientation, "showViewportOrientation", "Show Viewport Orientation");
		ar(Serialization::Range(fov, 20.0f, 120.0f), "fov", "FOV");
		ar(Serialization::Range(nearClip, 0.01f, 0.5f), "nearClip", "Near Clip");
		ar(Serialization::Range(moveSpeed, 0.1f, 10.0f), "moveSpeed", "Move Speed");
		ar(transformRestraint, "transformRestraint", 0);
		ar.doc("Relative to the scene size");
		ar(Serialization::Range(rotationSpeed, 0.1f, 4.0f), "rotationSpeed", "Rotation Speed");
		ar.doc("Degrees per 1000 px");
		if (ar.openBlock("movementSmoothing", "+Movement Smoothing"))
		{
			ar(smoothPos, "smoothPos", "Position");
			ar(smoothRot, "smoothRot", "Rotation");
			ar.closeBlock();
		}
	}
};

struct SViewportGridSettings
{
	bool   showGrid;
	bool   circular;
	ColorB mainColor;
	ColorB middleColor;
	int    alphaFalloff;
	float  spacing;
	uint16 count;
	uint16 interCount;
	bool   origin;
	ColorB originColor;

	SViewportGridSettings()
		: showGrid(true)
		, circular(true)
		, mainColor(255, 255, 255, 50)
		, middleColor(255, 255, 255, 10)
		, alphaFalloff(100)
		, spacing(1.0f)
		, count(10)
		, interCount(10)
		, origin(false)
		, originColor(10, 10, 10, 255)
	{
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(showGrid, "showGrid", "Show Grid");
		ar(circular, "circular", 0);
		ar(mainColor, "mainColor", "Main Color");
		ar(middleColor, "middleColor", "Middle Color");
		ar(Serialization::Range(alphaFalloff, 0, 100), "alphaFalloff", 0);
		ar(spacing, "spacing", "Spacing");
		ar(count, "count", "Main Lines");
		ar(interCount, "interCount", "Middle Lines");
		ar(origin, "origin", "Origin");
		ar(originColor, "originColor", origin ? "Origin Color" : 0);
	}
};

struct SViewportLightingSettings
{
	f32    m_brightness;
	ColorB m_ambientColor;

	bool   m_useLightRotation;
	f32    m_lightMultiplier;
	f32    m_lightSpecMultiplier;

	ColorB m_directionalLightColor;

	SViewportLightingSettings()
	{
		m_brightness = 1.0f;
		m_ambientColor = ColorB(128, 128, 128, 255);

		m_useLightRotation = 0;
		m_lightMultiplier = 3.0;
		m_lightSpecMultiplier = 2.0f;

		m_directionalLightColor = ColorB(255, 255, 255, 255);
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(Serialization::Range(m_brightness, 0.0f, 200.0f), "brightness", "Brightness");
		ar(m_ambientColor, "ambientColor", "Ambient Color");

		ar(m_useLightRotation, "rotatelight", "Rotate Light");
		ar(m_lightMultiplier, "lightMultiplier", "Light Multiplier");
		ar(m_lightSpecMultiplier, "lightSpecMultiplier", "Light Spec Multiplier");

		ar(m_directionalLightColor, "directionalLightColor", "Directional Light Color");
	}
};

struct SViewportBackgroundSettings
{
	bool   useGradient;
	ColorB topColor;
	ColorB bottomColor;

	SViewportBackgroundSettings()
		: useGradient(true)
		, topColor(128, 128, 128, 255)
		, bottomColor(32, 32, 32, 255)
	{
	}

	void Serialize(Serialization::IArchive& ar)
	{
		const bool bOldUseGradient = useGradient;
		ar(useGradient, "useGradient", "Use Gradient");
		if ((bOldUseGradient && ar.isInput()) || (useGradient && ar.isOutput()))
		{
			ar(topColor, "topColor", "Top Color");
			ar(bottomColor, "bottomColor", "Bottom Color");
		}
		else
		{
			ar(topColor, "topColor", "Color");
		}
	}
};

struct SViewportSettings
{
	SViewportRenderingSettings  rendering;
	SViewportCameraSettings     camera;
	SViewportGridSettings       grid;
	SViewportLightingSettings   lighting;
	SViewportBackgroundSettings background;

	void                        Serialize(Serialization::IArchive& ar)
	{
		ar(rendering, "debug", "Debug");
		ar(camera, "camera", "Camera");
		ar(grid, "grid", "Grid");
		ar(lighting, "lighting", "Lighting");
		ar(background, "background", "Background");
	}
};

