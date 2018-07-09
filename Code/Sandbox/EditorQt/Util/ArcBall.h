// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <CryMath/Cry_Math.h>
#include <CryMath/Cry_Color.h>

#define CrossDist (0.05f)
#define AxisDist  (0.05f)

class SANDBOX_API CArcBall3D
{
public:
	uint32 RotControl;
	Sphere sphere;
	uint32 Mouse_CutFlag;
	uint32 Mouse_CutFlagStart;
	uint32 AxisSnap;
	Vec3   LineStart3D;
	Vec3   Mouse_CutOnUnitSphere;
	Quat   DragRotation;
	Quat   ObjectRotation;

	CArcBall3D()
	{
		InitArcBall();
	};

	void InitArcBall()
	{
		RotControl = 0;
		sphere(Vec3(ZERO), 0.25f);
		Mouse_CutFlag = 0;
		Mouse_CutOnUnitSphere(0, 0, 0);
		LineStart3D(0, -1, 0);
		AxisSnap = 0;
		DragRotation.SetIdentity();
		ObjectRotation.SetIdentity();
	}

	//---------------------------------------------------------------
	// ArcControl
	// Returns true if the rotation has changed
	//---------------------------------------------------------------
	bool          ArcControl(const Matrix34& reference, const Ray& ray, uint32 mouseleft);
	void          ArcRotation();
	void          DrawSphere(const Matrix34& reference, const CCamera& cam, struct IRenderAuxGeom* pRenderer);
	static uint32 IntersectSphereLineSegment(const Sphere& s, const Vec3& LineStart, const Vec3& LineEnd, Vec3& I);
};

