// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

const float kSplinePointSelectionRadius = 0.8f;

// Spline Point
struct CSplinePoint
{
	Vec3  pos;
	Vec3  back;
	Vec3  forw;
	float angle;
	float width;
	bool  isDefaultWidth;
	CSplinePoint()
	{
		angle = 0;
		width = 0;
		isDefaultWidth = true;
	}
};

//////////////////////////////////////////////////////////////////////////
class CSplineObject : public CBaseObject
{
protected:
	CSplineObject();

public:
	virtual void  SetPoint(int index, const Vec3& pos);
	int           InsertPoint(int index, const Vec3& point);
	void          RemovePoint(int index);
	int           GetPointCount() const     { return m_points.size(); }
	const Vec3&   GetPoint(int index) const { return m_points[index].pos; }
	Vec3          GetBezierPos(int index, float t) const;
	float         GetBezierSegmentLength(int index, float t = 1.0f) const;
	Vec3          GetBezierNormal(int index, float t) const;
	Vec3          GetLocalBezierNormal(int index, float t) const;
	Vec3          GetBezierTangent(int index, float t) const;
	int           GetNearestPoint(const Vec3& raySrc, const Vec3& rayDir, float& distance);
	float         GetSplineLength() const;
	float         GetPosByDistance(float distance, int& outIndex) const;

	float         GetPointAngle() const;
	void          SetPointAngle(float angle);
	float         GetPointWidth() const;
	void          SetPointWidth(float width);
	bool          IsPointDefaultWidth() const;
	void          PointDafaultWidthIs(bool isDefault);

	void          SelectPoint(int index);
	int           GetSelectedPoint() const     { return m_selectedPoint; }

	void          SetEditMode(bool isEditMode) { m_isEditMode = isEditMode; }

	void          SetMergeIndex(int index)     { m_mergeIndex = index; }
	void          ReverseShape();
	void          Split(int index, const Vec3& point);
	void          Merge(CSplineObject* pSpline);

	void          CalcBBox();
	virtual float CreationZOffset() const { return 0.1f; }

	void          GetNearestEdge(const Vec3& raySrc, const Vec3& rayDir, int& p1, int& p2, float& distance, Vec3& intersectPoint);

	virtual void  OnUpdate()                  {}
	virtual void  SetLayerId(uint16 nLayerId) {}
	virtual void  SetPhysics(bool isPhysics)  {}
	virtual float GetAngleRange() const       { return 180.0f; }

	virtual void  OnContextMenu(CPopupMenuItem* menu);

protected:
	DECLARE_DYNAMIC(CSplineObject);
	void          OnUpdateUI();

	void          DrawJoints(DisplayContext& dc);
	bool          RayToLineDistance(const Vec3& rayLineP1, const Vec3& rayLineP2, const Vec3& pi, const Vec3& pj, float& distance, Vec3& intPnt);

	virtual int   GetMaxPoints() const { return 1000; }
	virtual int   GetMinPoints() const { return 2; }
	virtual float GetWidth() const     { return 0.0f; }
	virtual float GetStepSize() const  { return 1.0f; }

	void          BezierCorrection(int index);
	void          BezierAnglesCorrection(int index);

	// from CBaseObject
	bool         Init(CBaseObject* prev, const string& file) override;
	void         Done() override;

	void CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;

	void         GetBoundBox(AABB& box) override;
	void         GetLocalBounds(AABB& box) override;
	void         Display(CObjectRenderHelper& objRenderHelper) override;
	bool         HitTest(HitContext& hc) override;
	bool         HitTestRect(HitContext& hc) override;
	void         Serialize(CObjectArchive& ar) override;
	int          MouseCreateCallback(IDisplayViewport* pView, EMouseEvent event, CPoint& point, int flags) override;
	virtual bool IsScalable() const override  { return !m_isEditMode; }
	virtual bool IsRotatable() const override { return !m_isEditMode; }

	void         EditSpline();

protected:
	void         SerializeProperties(Serialization::IArchive& ar, bool bMultiEdit);

protected:
	std::vector<CSplinePoint>  m_points;
	AABB                       m_bbox;

	int                        m_selectedPoint;
	int                        m_mergeIndex;

	bool                       m_isEditMode;

	static class CSplinePanel* m_pSplinePanel;
	static int                 m_splineRollupID;
};

