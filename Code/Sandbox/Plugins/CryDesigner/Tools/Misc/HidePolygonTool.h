// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/Select/SelectTool.h"

namespace Designer
{
class HidePolygonTool : public SelectTool
{
public:

	HidePolygonTool(EDesignerTool tool) : SelectTool(tool)
	{
		m_nPickFlag = ePF_Polygon;
	}

	void Enter() override;

	bool HideSelectedPolygons();
	void UnhideAll();
	void Serialize(Serialization::IArchive& ar);
};
}

