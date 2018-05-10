// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BoxTool.h"

namespace Designer
{
struct StairParameter
{
	float m_StepRise;
	bool  m_bMirror;
	bool  m_bRotation90Degree;

	StairParameter() :
		m_bRotation90Degree(false),
		m_bMirror(false),
		m_StepRise(kDefaultStepRise)
	{
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(STEPRISE_RANGE(m_StepRise), "StepRise", "Step Rise");
		ar(m_bMirror, "Mirror", "Mirror");
		ar(m_bRotation90Degree, "RotationBy90Degrees", "Rotation by 90 Degrees");
	}
};

struct OutputStairParameter
{
	OutputStairParameter() : pCapPolygon(NULL){}
	std::vector<PolygonPtr> polygons;
	std::vector<PolygonPtr> polygonsNeedPostProcess;
	PolygonPtr              pCapPolygon;
};

class StairTool : public BoxTool
{
public:
	StairTool(EDesignerTool tool) :
		BoxTool(tool),
		m_bWidthIsLonger(false)
	{
	}

	void        Serialize(Serialization::IArchive& ar);

	static void CreateStair(
	  const BrushVec3& vStartPos,
	  const BrushVec3& vEndPos,
	  BrushFloat fBoxWidth,
	  BrushFloat fBoxDepth,
	  BrushFloat fBoxHeight,
	  const BrushPlane& floorPlane,
	  float fStepRise,
	  bool bXDirection,
	  bool bMirrored,
	  bool bRotationBy90Degree,
	  PolygonPtr pBasePolygon,
	  OutputStairParameter& out);

private:
	void              UpdateShape(bool bUpdateUIs = true) override;
	void              UpdateShapeWithBoundaryCheck(bool bUpdateUIs = true) override;
	void              AcceptUndo();
	void              RegisterShape(PolygonPtr pFloorPolygon) override;

	static PolygonPtr CreatePolygon(const std::vector<BrushVec3>& vList, bool bFlip, PolygonPtr pBasePolygon);

	PolygonPtr              m_pCapPolygon;
	std::vector<PolygonPtr> m_PolygonsNeedPostProcess;
	StairParameter          m_StairParameter;
	bool                    m_bWidthIsLonger;
};
}

