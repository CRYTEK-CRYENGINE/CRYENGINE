// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/BaseTool.h"
#include "Util/SpotManager.h"

namespace Designer
{
class ShapeTool : public BaseTool, public SpotManager
{
public:
	ShapeTool(EDesignerTool tool) :
		BaseTool(tool),
		SpotManager(),
		m_bEnableMagnet(true)
	{
	}

	virtual ~ShapeTool(){}

	virtual void Enter() override;
	virtual void Leave() override;

	void         DisplayCurrentSpot(DisplayContext& dc);
	virtual void Serialize(Serialization::IArchive& ar);

protected:
	bool UpdateCurrentSpotPosition(
	  CViewport* view,
	  UINT nFlags,
	  CPoint point,
	  bool bKeepInitialPlane,
	  bool bSearchAllShelves = false);

	void         UpdateDrawnPolygon(const BrushVec2& p0, const BrushVec2& p1);

	TexInfo      GetTexInfo() const;
	int          GetMatID() const;

	virtual void RegisterShape(PolygonPtr pFloorPolygon);
	void         Register2DShape(PolygonPtr polygon);
	void         CancelCreation();

	bool         IsSeparateStatus() const;
	void         StoreSeparateStatus();
	void         Separate1stStep();
	void         Separate2ndStep();

	void         SetPlane(const BrushPlane& plane) { m_Plane = plane; }
	BrushPlane   GetPlane() const                  { return m_Plane; }

	BrushPlane m_Plane;
	bool       m_bSeparatedNewShape;
	bool       m_bEnableMagnet;
};
}

