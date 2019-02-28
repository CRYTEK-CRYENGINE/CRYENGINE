// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <EditorFramework/Preferences.h>

// Preferences
struct EDITOR_COMMON_API SSnappingPreferences : public SPreferencePage
{
	SSnappingPreferences()
		: SPreferencePage("Snapping", "General/Snapping")
		, m_gridSize(1)
		, m_angleSnap(5)
		, m_scaleSnap(1)
		, m_gridScale(1)
		, m_gridMajorLine(16)
		, m_markerSize(1.0f)
		, m_markerColor(RGB(0, 200, 200))
		, m_gridSnappingEnabled(true)
		, m_angleSnappingEnabled(true)
		, m_scaleSnappingEnabled(true)
		, m_markerDisplay(false)
	{
	}

	virtual bool Serialize(yasli::Archive& ar) override;

	double       SnapLength(const double length) const;
	float        SnapScale(const float length) const;
	Vec3         Snap(const Vec3& vec, bool bForceSnap = false) const;
	Vec3         Snap(const Vec3& vec, double fZoom) const;
	double       SnapAngle(double angle) const;
	Ang3         SnapAngle(const Ang3& vec) const;
	Matrix34     GetMatrix() const;

	ADD_PREFERENCE_PAGE_PROPERTY(double, gridSize, setGridSize)
	ADD_PREFERENCE_PAGE_PROPERTY(double, angleSnap, setAngleSnap)
	ADD_PREFERENCE_PAGE_PROPERTY(float, scaleSnap, setScaleSnap)
	ADD_PREFERENCE_PAGE_PROPERTY(double, gridScale, setGridScale)
	ADD_PREFERENCE_PAGE_PROPERTY(int, gridMajorLine, setGridMajorLine)
	ADD_PREFERENCE_PAGE_PROPERTY(float, markerSize, setMarkerSize);
	ADD_PREFERENCE_PAGE_PROPERTY(COLORREF, markerColor, setMarkerColor);

	ADD_PREFERENCE_PAGE_PROPERTY(bool, gridSnappingEnabled, setGridSnappingEnabled)
	ADD_PREFERENCE_PAGE_PROPERTY(bool, angleSnappingEnabled, setAngleSnappingEnabled)
	ADD_PREFERENCE_PAGE_PROPERTY(bool, scaleSnappingEnabled, setScaleSnappingEnabled)
	ADD_PREFERENCE_PAGE_PROPERTY(bool, markerDisplay, setMarkerDisplay)
};

EDITOR_COMMON_API extern SSnappingPreferences gSnappingPreferences;

