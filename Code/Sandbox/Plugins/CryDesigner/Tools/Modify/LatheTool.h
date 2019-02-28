// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "MagnetTool.h"

namespace Designer
{
enum ELatheErrorCode
{
	eLEC_Success,
	eLEC_NoPath,
	eLEC_InappropriateProfileShape,
	eLEC_ProfileShapeTooBig,
	eLEC_ShouldBeAtFirstPointInOpenPolygon
};

class LatheTool : public MagnetTool
{
public:
	LatheTool(EDesignerTool tool) :
		MagnetTool(tool),
		m_bChoosePathPolygonPhase(false)
	{
		m_bApplyUndoForMagnet = false;
	}

	void Enter() override;

	bool OnLButtonDown(
	  CViewport* view,
	  UINT nFlags,
	  CPoint point) override;

	bool OnMouseMove(
	  CViewport* view,
	  UINT nFlags,
	  CPoint point) override;

	void Display(DisplayContext& dc) override;

private:
	ELatheErrorCode CreateShapeAlongPath(
	  PolygonPtr pInitProfilePolygon,
	  PolygonPtr pPathPolygon);

	std::vector<BrushPlane> CreatePlanesAtEachPointOfPath(
	  const std::vector<Vertex>& vPath,
	  bool bPathClosed);

	std::vector<Vertex> ExtractPath(
	  PolygonPtr pPathPolygon,
	  bool& bOutClosed);

	bool GluePolygons(
	  PolygonPtr pPathPolygon,
	  const std::vector<PolygonPtr>& polygons);

	void AddPolygonToDesigner(
	  Model* pModel,
	  const std::vector<BrushVec3>& vList,
	  PolygonPtr pInitPolygon,
	  bool bFlip);

	bool QueryPolygonsOnPlane(
	  Model* pModel,
	  const BrushPlane& plane,
	  std::vector<PolygonPtr>& outPolygons);

	bool m_bChoosePathPolygonPhase;
};
}

