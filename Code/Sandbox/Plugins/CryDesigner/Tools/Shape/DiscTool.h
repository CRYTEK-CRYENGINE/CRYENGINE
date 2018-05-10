// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ShapeTool.h"

namespace Designer
{
struct DiscParameter
{
	DiscParameter() :
		m_NumOfSubdivision(kDefaultSubdivisionNum),
		m_Radius(0),
		m_b90DegreeSnap(true)
	{
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(SUBDIVISION_RANGE(m_NumOfSubdivision), "SubdivisionCount", "Subdivision Count");
		if (ar.isEdit())
			ar(LENGTH_RANGE(m_Radius), "Radius", "Radius");
		ar(m_b90DegreeSnap, "90 Degrees Snap", "90 Degrees Snap");
	}

	int   m_NumOfSubdivision;
	float m_Radius;
	bool  m_b90DegreeSnap;
};

enum EDiscToolPhase
{
	eDiscPhase_SetCenter,
	eDiscPhase_Radius,
	eDiscPhase_RaiseHeight,
	eDiscPhase_Done,
};

class DiscTool : public ShapeTool
{
public:
	DiscTool(EDesignerTool tool) :
		ShapeTool(tool),
		m_fAngle(0),
		m_pBasePolygon(NULL),
		m_bSnapped(false)
	{
	}

	void         Enter() override;
	void         Leave() override;

	virtual bool OnLButtonDown(CViewport* view, UINT nFlags, CPoint point)  override;
	virtual bool OnMouseMove(CViewport* view, UINT nFlags, CPoint point) override;
	bool         OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags) override;
	void         OnEditorNotifyEvent(EEditorNotifyEvent event) override;
	virtual void Display(DisplayContext& dc) override;
	virtual void OnChangeParameter(bool continuous) override;
	bool         IsPhaseFirstStepOnPrimitiveCreation() const override;

	virtual void Serialize(Serialization::IArchive& ar) override;

protected:
	virtual void UpdateShape();
	virtual void Register();

	bool         SnapAngleBy90Degrees(float angle, float& outSnappedAngle);

protected:
	EDiscToolPhase m_Phase;
	PolygonPtr     m_pBasePolygon;
	BrushVec2      m_vCenterOnPlane;
	float          m_fAngle;
	bool           m_bSnapped;
	DiscParameter  m_DiscParameter;
};
}

