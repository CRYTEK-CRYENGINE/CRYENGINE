// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryIcon.h>

struct IStatObj;
struct IMaterial;
class CBitmap;

// Note: values are used as array indices
enum EStatObject
{
	eStatObject_Arrow = 0,
	eStatObject_Axis,
	eStatObject_Sphere,
	eStatObject_Anchor,
	eStatObject_Entrance,
	eStatObject_HidePoint,
	eStatObject_HidePointSecondary,
	eStatObject_ReinforcementSpot,

	eStatObject_COUNT
};

// Note: values are used as array indices
enum EIcon
{
	eIcon_ScaleWarning = 0,
	eIcon_RotationWarning,

	eIcon_COUNT
};

// Note: image effects to apply to image
enum EIconEffect
{
	eIconEffect_Dim           = 1 << 0,
	eIconEffect_HalfAlpha     = 1 << 1,
	eIconEffect_TintRed       = 1 << 2,
	eIconEffect_TintGreen     = 1 << 3,
	eIconEffect_TintYellow    = 1 << 4,
	eIconEffect_ColorEnabled  = 1 << 5,
	eIconEffect_ColorDisabled = 1 << 6,
};

struct IIconManager
{
	virtual IStatObj*  GetObject(EStatObject object) = 0;
	virtual int        GetIconTexture(EIcon icon) = 0;
	virtual int        GetIconTexture(const char* szIconName) = 0;
	virtual int        GetIconTexture(const char* szIconName, CryIcon& icon) = 0;
	virtual IMaterial* GetHelperMaterial() = 0;
};

