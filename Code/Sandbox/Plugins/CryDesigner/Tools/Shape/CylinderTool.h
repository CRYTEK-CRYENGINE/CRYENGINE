// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DiscTool.h"
#include "BoxTool.h"

namespace Designer
{
typedef BoxParameter CylinderParameter;

class CylinderTool : public DiscTool
{
public:
	CylinderTool(EDesignerTool tool) : DiscTool(tool)
	{
	}

	~CylinderTool(){}

	bool OnLButtonDown(CViewport* view, UINT nFlags, CPoint point) override;
	bool OnMouseMove(CViewport* view, UINT nFlags, CPoint point) override;
	void Display(DisplayContext& dc) override;
	bool EnabledSeamlessSelection() const;
	void OnChangeParameter(bool continuous) override;
	void Serialize(Serialization::IArchive& ar) override;
	void UpdateHeightWithBoundaryCheck(BrushFloat fHeight);
	void Update();

protected:
	virtual void UpdateShape(float fHeight);
	virtual void Register() override;

private:
	bool              m_bIsOverOpposite;
	PolygonPtr        m_pCapPolygon;
	CylinderParameter m_CylinderParameter;
};
}

