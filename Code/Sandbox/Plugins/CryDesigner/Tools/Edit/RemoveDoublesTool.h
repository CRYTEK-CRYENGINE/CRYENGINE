// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/Select/MoveTool.h"

namespace Designer
{
struct RemoveDoubleParameter
{
	float m_Distance;

	RemoveDoubleParameter() : m_Distance(0.01f)
	{
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(LENGTH_RANGE(m_Distance), "Distance", "Distance");
	}
};

class RemoveDoublesTool : public SelectTool
{
public:
	RemoveDoublesTool(EDesignerTool tool) : SelectTool(tool)
	{
		m_nPickFlag = ePF_Vertex;
	}

	void        Enter() override;
	void        Serialize(Serialization::IArchive& ar);

	bool        RemoveDoubles();

	static void RemoveDoubles(MainContext& mc, float fDistance);

private:

	static bool HasVertexInList(const std::vector<BrushVec3>& vList, const BrushVec3& vPos);

	RemoveDoubleParameter m_RemoveDoubleParameter;
};
}

