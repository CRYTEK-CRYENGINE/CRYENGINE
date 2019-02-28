// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IDisplayViewport.h"

class QViewport;

class EDITOR_COMMON_API CDisplayViewportAdapter : public ::IDisplayViewport
{
public:

	CDisplayViewportAdapter(QViewport* viewport);

	void               Update() override {}
	float              GetScreenScaleFactor(const Vec3& position) const override;
	bool               HitTestLine(const Vec3& lineP1, const Vec3& lineP2, const CPoint& hitpoint, int pixelRadius, float* pToCameraDistance = 0) const override;
	CBaseObjectsCache* GetVisibleObjectsCache() override               { return 0; }
	bool               IsBoundsVisible(const AABB& box) const override { return false; }
	void               GetPerpendicularAxis(EAxis* axis, bool* is2D) const override;
	const Matrix34& GetViewTM() const override;
	POINT           WorldToView(const Vec3& worldPoint) const override;
	Vec3            WorldToView3D(const Vec3& worldPoint, int flags = 0) const override                                                                                      { return Vec3(0.0f, 0.0f, 0.0f); }
	Vec3            ViewToWorld(POINT vp, bool* collideWithTerrain = 0, bool onlyTerrain = false, bool bSkipVegetation = false, bool bTestRenderMesh = false) const override { return Vec3(0.0f, 0.0f, 0.0f); }
	void            ViewToWorldRay(POINT vp, Vec3& raySrc, Vec3& rayDir) const;
	Vec3            ViewToAxisConstraint(POINT& point, Vec3 axis, Vec3 origin) const;
	Vec3            ViewDirection() const;
	Vec3            UpViewDirection() const;
	Vec3            CameraToWorld(Vec3 worldPoint) const;

	float           GetGridStep() const override                                                                                                                             { return 1.0f; }
	float           GetAspectRatio() const override;
	void            ScreenToClient(POINT* pt) const override                                                                                                                 {}
	void            GetDimensions(int* width, int* height) const override;

	void            EnableXYViewport(bool bEnable) { m_bXYViewport = bEnable; }
	virtual Vec3	MapViewToCP(CPoint point, int axis, bool aSnapToTerrain = false, float aTerrainOffset = 0.f) { return Vec3(0, 0, 0); }
	virtual Vec3    SnapToGrid(Vec3 vec) { return vec; }
	virtual void    SetConstructionMatrix(const Matrix34& xform) {}
	virtual void    SetCursor(QCursor* hCursor) override;

private:
	mutable Matrix34 m_viewMatrix;
	QViewport*       m_viewport;
	bool             m_bXYViewport;
};

