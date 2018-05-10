// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Core/Polygon.h"

namespace Designer
{
class ElementSet;

struct EdgeSharpness
{
	string                   name;
	std::vector<BrushEdge3D> edges;
	float                    sharpness;
	CryGUID                  guid;
};

struct SharpEdgeInfo
{
	SharpEdgeInfo() : sharpnessindex(-1), edgeindex(-1) {}
	int sharpnessindex;
	int edgeindex;
};

class EdgeSharpnessManager : public _i_reference_target_t
{
public:
	EdgeSharpnessManager& operator=(const EdgeSharpnessManager& edgeSharpnessMgr);

	void                  Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bUndo);

	bool                  AddEdges(const char* name, ElementSet* pElements, float sharpness = 0);
	bool                  AddEdges(const char* name, const std::vector<BrushEdge3D>& edges, float sharpness = 0);
	void                  RemoveEdgeSharpness(const char* name);
	void                  RemoveEdgeSharpness(const BrushEdge3D& edge);

	void                  SetSharpness(const char* name, float sharpness);
	void                  Rename(const char* oldName, const char* newName);

	bool                  HasName(const char* name) const;
	string                GenerateValidName(const char* baseName = "EdgeGroup") const;

	int                   GetCount() const { return m_EdgeSharpnessList.size(); }
	const EdgeSharpness& Get(int n) const { return m_EdgeSharpnessList[n]; }

	float                FindSharpness(const BrushEdge3D& edge) const;
	void                 Clear() { m_EdgeSharpnessList.clear(); }

	EdgeSharpness*       FindEdgeSharpness(const char* name);
private:

	SharpEdgeInfo GetEdgeInfo(const BrushEdge3D& edge);
	void          DeleteEdge(const SharpEdgeInfo& edgeInfo);

	std::vector<EdgeSharpness> m_EdgeSharpnessList;
};
}

