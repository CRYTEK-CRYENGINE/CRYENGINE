// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DiscTool.h"

namespace Designer
{
class SphereTool : public DiscTool
{
public:
	SphereTool(EDesignerTool tool) :
		DiscTool(tool),
		m_MatTo001(BrushMatrix34::CreateIdentity())
	{
	}
	~SphereTool(){}

protected:
	void UpdateShape() override;
	void UpdateDesignerBasedOnSpherePolygons(const BrushMatrix34& tm);
	void Register() override;

private:
	BrushMatrix34           m_MatTo001;
	std::vector<PolygonPtr> m_SpherePolygons;
};
}

