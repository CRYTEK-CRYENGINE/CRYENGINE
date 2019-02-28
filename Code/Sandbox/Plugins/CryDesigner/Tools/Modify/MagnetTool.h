// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/Select/SelectTool.h"

namespace Designer
{
enum EMagnetToolPhase
{
	eMTP_ChoosePolygon,
	eMTP_ChooseFirstPoint,
	eMTP_ChooseUpPoint,
	eMTP_ChooseMoveToTargetPoint,
};

class MagnetTool : public SelectTool
{
public:
	MagnetTool(EDesignerTool tool) :
		SelectTool(tool),
		m_bApplyUndoForMagnet(true)
	{
		m_nPickFlag = ePF_Polygon;
	}

	void         Enter() override;
	void         Leave() override;

	virtual bool OnLButtonDown(CViewport* view, UINT nFlags, CPoint point) override;
	virtual bool OnMouseMove(CViewport* view, UINT nFlags, CPoint point) override;
	virtual void Display(DisplayContext& dc) override;

	bool         OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags) override;

protected:
	struct SourceVertex
	{
		SourceVertex(const BrushVec3& v) : position(v), color(kElementBoxColor){}
		SourceVertex(const BrushVec3& v, const ColorB c) : position(v), color(c){}
		BrushVec3 position;
		ColorB    color;
	};

	void PrepareChooseFirstPointStep();
	void InitializeSelectedPolygonBeforeTransform();
	void AddVertexToList(const BrushVec3& vertex, ColorB color, std::vector<SourceVertex>& vertices);
	void SwitchSides();
	void AlignSelectedPolygon();

	EMagnetToolPhase          m_Phase;
	std::vector<SourceVertex> m_SourceVertices;
	int                       m_nSelectedSourceVertex;
	int                       m_nSelectedUpVertex;
	PolygonPtr                m_pInitPolygon;
	PolygonPtr                m_pSelectedPolygon;
	PolygonPtr                m_pTargetPolygon;
	BrushVec3                 m_TargetPos;
	BrushVec3                 m_PickedPos;
	BrushVec3                 m_vTargetUpDir;
	bool                      m_bPickedTargetPos;
	bool                      m_bSwitchedSides;
	bool                      m_bApplyUndoForMagnet;
};
}

