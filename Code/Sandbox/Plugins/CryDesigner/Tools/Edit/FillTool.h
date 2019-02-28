// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/BaseTool.h"

namespace Designer
{
struct FillParam
{
	FillParam() :
		bMergeAdjacent(false)
	{
	}
	bool bMergeAdjacent;
};

class FillTool : public BaseTool
{
public:

	FillTool(EDesignerTool tool) : BaseTool(tool)
	{
	}

	void Enter() override;
	void Leave() override;

	bool OnMouseMove(CViewport* view, UINT nFlags, CPoint point) override;
	bool OnLButtonDown(CViewport* view, UINT nFlags, CPoint point) override;

	void OnEditorNotifyEvent(EEditorNotifyEvent event) override;
	void Serialize(Serialization::IArchive& ar);

private:

	void              CompileHoles();

	bool              FillHoleBasedOnSelectedElements();

	static bool       ContainPolygon(PolygonPtr pPolygon, std::vector<PolygonPtr>& polygonList);
	static PolygonPtr QueryAdjacentPolygon(PolygonPtr pPolygon, std::vector<PolygonPtr>& polygonList);

	_smart_ptr<Model> m_pHoleContainer;
	PolygonPtr        m_PickedHolePolygon;
	FillParam         m_Params;
};
}

