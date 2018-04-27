// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/BaseTool.h"
#include "Util/SpotManager.h"

namespace Designer
{
enum ECubeEditorMode
{
	eCubeEditorMode_Add,
	eCubeEditorMode_Remove,
	eCubeEditorMode_Paint,
	eCubeEditorMode_Invalid
};

class CubeEditor : public BaseTool
{
public:
	CubeEditor(EDesignerTool tool)
		: BaseTool(tool)
		, m_mode(eCubeEditorMode_Add)
		, m_bSideMerged(false)
		, m_cubeSize(1.0f)
	{
	}

	void            Enter() override;
	void            Leave() override;

	bool            OnLButtonDown(CViewport* view, UINT nFlags, CPoint point) override;
	bool            OnLButtonUp(CViewport* view, UINT nFlags, CPoint point) override;
	bool            OnMouseMove(CViewport* view, UINT nFlags, CPoint point) override;
	bool            OnMouseWheel(CViewport* view, UINT nFlags, CPoint point) override;

	void            Display(DisplayContext& dc) override;

	ECubeEditorMode GetEditMode() const;
	void SetEditMode(ECubeEditorMode);
	bool            IsSideMerged() const;
	void            SetSideMerged(bool bsideMerged);
	BrushFloat      GetCubeSize() const;
	void            SetCubeSize(float csize);

private:

	bool                    IsAddMode() const;
	bool                    IsRemoveMode() const;
	bool                    IsPaintMode() const;
	void                    SelectPrevBrush();
	void                    SelectNextBrush();

	void                    DisplayBrush(DisplayContext& dc);
	std::vector<PolygonPtr> GetBrushPolygons(const AABB& aabb) const;
	void                    AddCube(const AABB& brushAABB);
	void                    RemoveCube(const AABB& brushAABB);
	void                    PaintCube(const AABB& brushAABB);
	AABB                    GetBrushBox(CViewport* view, CPoint point);
	AABB                    GetBrushBox(const BrushVec3& vSnappedPos, const BrushVec3& vPickedPos, const BrushVec3& vNormal);

	bool                    GetBrushPos(CViewport* view, CPoint point, BrushVec3& outSnappedPos, BrushVec3& outPickedPos, BrushVec3* pOutNormal);
	BrushVec3               Snap(const BrushVec3& vPos) const;

	void                    AddBrush(const AABB& aabb);

	AABB              m_BrushAABB;
	std::vector<AABB> m_BrushAABBs;
	CPoint            m_CurMousePos;
	ECubeEditorMode   m_mode;
	bool              m_bSideMerged;
	float             m_cubeSize;

	struct DrawStraight
	{
		DrawStraight() :
			m_bPressingShift(false),
			m_StraightDir(0, 0, 0),
			m_StartingPos(0, 0, 0),
			m_StartingNormal(0, 0, 0)
		{
		}
		BrushVec3 m_StraightDir;
		BrushVec3 m_StartingPos;
		BrushVec3 m_StartingNormal;
		bool      m_bPressingShift;
	};
	DrawStraight m_DS;
};
}

