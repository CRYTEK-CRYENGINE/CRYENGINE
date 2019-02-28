// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __GravityVolumeObject_h__
#define __GravityVolumeObject_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "EntityObject.h"
#include "SafeObjectsArray.h"

#define GravityVolume_CLOSE_DISTANCE 0.8f
#define GravityVolume_Z_OFFSET       0.1f

// GravityVolume Point
struct CGravityVolumePoint
{
	Vec3  pos;
	Vec3  back;
	Vec3  forw;
	float angle;
	float width;
	bool  isDefaultWidth;
	CGravityVolumePoint()
	{
		angle = 0;
		width = 0;
		isDefaultWidth = true;
	}
};

typedef std::vector<CGravityVolumePoint> CGravityVolumePointVector;

/*!
 *	CGravityVolumeObject is an object that represent named 3d position in world.
 *
 */
class CGravityVolumeObject : public CEntityObject
{
public:
	DECLARE_DYNCREATE(CGravityVolumeObject)

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool Init(CBaseObject* prev, const string& file);
	void InitVariables();
	void Done();
	bool HasMeasurementAxis() const { return true;  }

	void Display(CObjectRenderHelper& objRenderHelper) override;
	void DrawBezierSpline(DisplayContext& dc, CGravityVolumePointVector& points, COLORREF col, bool isDrawJoints, bool isDrawGravityVolume);

	bool CreateGameObject();

	//////////////////////////////////////////////////////////////////////////
	string GetUniqueName() const;

	void CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;

	//! Called when object is being created.
	int        MouseCreateCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int flags);

	void       GetBoundBox(AABB& box);
	void       GetLocalBounds(AABB& box);

	bool       HitTest(HitContext& hc);
	bool       HitTestRect(HitContext& hc);

	void       Serialize(CObjectArchive& ar);
	XmlNodeRef Export(const string& levelPath, XmlNodeRef& xmlNode);

	void       SetSelected(bool bSelect);

	void       OnEvent(ObjectEvent event);
	//////////////////////////////////////////////////////////////////////////

	//void SetClosed( bool bClosed );
	//bool IsClosed() { return mv_closed; };

	//! Insert new point to GravityVolume at given index.
	//! @return index of newly inserted point.
	int  InsertPoint(int index, const Vec3& point);
	//! Remove point from GravityVolume at given index.
	void RemovePoint(int index);

	//! Get number of points in GravityVolume.
	int         GetPointCount()           { return m_points.size(); };
	//! Get point at index.
	const Vec3& GetPoint(int index) const { return m_points[index].pos; };
	//! Set point position at specified index.
	void        SetPoint(int index, const Vec3& pos);

	void        SelectPoint(int index);
	int         GetSelectedPoint() const { return m_selectedPoint; };

	//! Find GravityVolume point nearest to given point.
	int GetNearestPoint(const Vec3& raySrc, const Vec3& rayDir, float& distance);

	//! Find GravityVolume edge nearest to given point.
	void GetNearestEdge(const Vec3& pos, int& p1, int& p2, float& distance, Vec3& intersectPoint);

	//! Find GravityVolume edge nearest to given ray.
	void GetNearestEdge(const Vec3& raySrc, const Vec3& rayDir, int& p1, int& p2, float& distance, Vec3& intersectPoint);
	//////////////////////////////////////////////////////////////////////////

	void  CalcBBox();

	Vec3  GetBezierPos(CGravityVolumePointVector& points, int index, float t);
	Vec3  GetSplinePos(CGravityVolumePointVector& points, int index, float t);
	Vec3  GetBezierNormal(int index, float t);
	float GetBezierSegmentLength(int index, float t = 1.0f);
	void  BezierAnglesCorrection(CGravityVolumePointVector& points, int index);
	void  SplinePointsCorrection(CGravityVolumePointVector& points, int index);
	void  BezierCorrection(int index);
	void  SplineCorrection(int index);

	float GetPointAngle();
	void  SetPointAngle(float angle);

	float GetPointWidth();
	void  SetPointWidth(float width);

	bool  IsPointDefaultWidth();
	void  PointDafaultWidthIs(bool IsDefault);

	void  UpdateGameArea();

protected:
	bool        RayToLineDistance(const Vec3& rayLineP1, const Vec3& rayLineP2, const Vec3& pi, const Vec3& pj, float& distance, Vec3& intPnt);
	virtual int GetMaxPoints() const { return 1000; };
	virtual int GetMinPoints() const { return 2; };

	// Ignore default draw highlight.
	void DrawHighlight(DisplayContext& dc) {};

	//overrided from CBaseObject.
	void InvalidateTM(int nWhyFlags);

	//! Called when GravityVolume variable changes.
	void OnParamChange(IVariable* var);

	//! Dtor must be protected.
	CGravityVolumeObject();

	void DeleteThis() { delete this; };

protected:
	AABB                      m_bbox;

	CGravityVolumePointVector m_points;
	std::vector<Vec3>         m_bezierPoints;

	string                   m_lastGameArea;

	//////////////////////////////////////////////////////////////////////////
	// GravityVolume parameters.
	//////////////////////////////////////////////////////////////////////////
	CVariable<float> mv_radius;
	CVariable<float> mv_gravity;
	CVariable<float> mv_falloff;
	CVariable<float> mv_damping;
	CVariable<float> mv_step;
	CVariable<bool>  mv_dontDisableInvisible;

	int              m_selectedPoint;
	//! Forces GravityVolume to be always 2D. (all vertices lie on XY plane).
	bool             m_bForce2D;

	bool             m_bIgnoreParamUpdate;
};

/*!
 * Class Description of GravityVolumeObject.
 */
class CGravityVolumeObjectClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()     { return OBJTYPE_OTHER; };
	const char*    ClassName()         { return "GravityVolume"; };
	const char*    Category()          { return "Misc"; };
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(CGravityVolumeObject); };
	virtual bool   IsCreatable() const override { return gEnv->pEntitySystem->GetClassRegistry()->FindClass("AreaBezierVolume") != nullptr; }
};

#endif // __GravityVolumeObject_h__

