// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ToolFactory.h"
#include "Core/UVIsland.h"
#include <CrySerialization/Enum.h>
#include "Core/Polygon.h"

class QViewport;

namespace Designer {
namespace UVMapping
{

class BaseTool;

enum EUVMappingToolGroup
{
	eUVMappingToolGroup_Unwrapping,
	eUVMappingToolGroup_Manipulation
};

enum EUVMappingTool
{
	eUVMappingTool_Plane,
	eUVMappingTool_Cube,
	eUVMappingTool_View,
	eUVMappingTool_Sphere,
	eUVMappingTool_Cylinder,

	eUVMappingTool_Island,
	eUVMappingTool_Polygon,
	eUVMappingTool_Edge,
	eUVMappingTool_Vertex,
	eUVMappingTool_SharedSelect,
	eUVMappingTool_LoopSelect,
	eUVMappingTool_Unmap,
	eUVMappingTool_Sew,
	eUVMappingTool_MoveAndSew,
	eUVMappingTool_SmartSew,
	eUVMappingTool_Separate,
	eUVMappingTool_FlipHori,
	eUVMappingTool_FlipVert,
	eUVMappingTool_Alignment
};

enum EPrincipleAxis
{
	ePrincipleAxis_X,
	ePrincipleAxis_Y
};

struct UVVertex
{
	UVVertex() : vIndex(-1) {}
	UVVertex(UVIslandPtr _pUVIsland, PolygonPtr _pPolygon, int _vIndex) : pUVIsland(_pUVIsland), pPolygon(_pPolygon), vIndex(_vIndex) {}
	const Vec2&      GetUV() const         { return pPolygon->GetUV(vIndex); }
	void             SetUV(const Vec2& uv) { pPolygon->SetUV(vIndex, uv); }

	const BrushVec3& GetPos() const        { return pPolygon->GetPos(vIndex); }

	bool             operator==(const UVVertex& uv) const
	{
		return pUVIsland == uv.pUVIsland && pPolygon == uv.pPolygon && vIndex == uv.vIndex;
	}

	bool IsEquivalent(const UVVertex& uv) const
	{
		return Comparison::IsEquivalent(GetUV(), uv.GetUV());
	}

	bool operator<(const UVVertex& uv) const
	{
		return Comparison::less_UV()(GetUV(), uv.GetUV());
	}

	UVIslandPtr pUVIsland;
	PolygonPtr  pPolygon;
	int         vIndex;
};

struct UVEdge
{
	UVEdge() {}
	UVEdge(UVIslandPtr _pUVIsland, const UVVertex& _uv0, const UVVertex& _uv1) :
		pUVIsland(_pUVIsland),
		uv0(_uv0),
		uv1(_uv1)
	{
	}
	UVIslandPtr pUVIsland;
	UVVertex    uv0;
	UVVertex    uv1;
};

struct UVPolygon
{
	UVPolygon() {}
	UVPolygon(UVIslandPtr _pUVIsland, const std::vector<UVVertex>& _uvs) :
		pUVIsland(_pUVIsland),
		uvs(_uvs)
	{
	}
	UVIslandPtr           pUVIsland;
	std::vector<UVVertex> uvs;
};

class SearchUtil
{
public:
	static Vec3 FindHitPointToUVPlane(QViewport* viewport, int x, int y);

	static bool FindUVIslands(const Ray& ray, std::vector<UVIslandPtr>& outFoundIslands);
	static bool FindUVIslands(const AABB& aabb, std::vector<UVIslandPtr>& outFoundIslands);

	static bool FindPolygons(const Ray& ray, std::vector<UVPolygon>& outFoundPolygons);
	static bool FindPolygons(const AABB& aabb, std::vector<UVPolygon>& outFoundPolygons);
	static bool FindPolygons(const Ray& ray, UVIslandPtr pUVIsland, std::vector<UVPolygon>& outFoundPolygons);
	static bool FindPolygons(const AABB& aabb, UVIslandPtr pUVIsland, std::vector<UVPolygon>& outFoundPolygons);

	static bool FindEdges(const Ray& ray, std::vector<UVEdge>& outFoundEdges);
	static bool FindEdges(const AABB& aabb, std::vector<UVEdge>& outFoundEdges);
	static bool FindEdges(const Ray& ray, UVIslandPtr pUVIsland, PolygonPtr pPolygon, std::map<float, UVEdge>& outFoundEdges);
	static bool FindEdges(const AABB& aabb, UVIslandPtr pUVIsland, PolygonPtr pPolygon, std::vector<UVEdge>& outFoundEdges);

	static bool FindVertices(const Ray& ray, std::vector<UVVertex>& outFoundVertices);
	static bool FindVertices(const AABB& aabb, std::vector<UVVertex>& outFoundVertices);
	static bool FindVertices(const Ray& ray, UVIslandPtr pUVIsland, PolygonPtr pPolygon, std::map<float, UVVertex>& outFoundVertices);
	static bool FindVertices(const AABB& aabb, UVIslandPtr pUVIsland, PolygonPtr pPolygon, std::vector<UVVertex>& outFoundVertices);

	static bool FindUVVerticesFromPos(const BrushVec3& pos, int subMatId, std::vector<UVVertex>& outUVVertices);
	static bool FindUVEdgesFromEdge(const BrushEdge3D& edge, int subMatId, std::vector<UVEdge>& outUVEdges);
	static bool FindUVPolygonsFromPolygon(PolygonPtr inPolygon, int subMatId, std::vector<UVPolygon>& outUVPolygons);

	static bool FindPolygonsSharingEdge(const Vec2& uv0, const Vec2& uv1, UVIslandPtr pUVIsland, std::set<PolygonPtr>& outFoundPolygons);
	static bool FindConnectedEdges(
	  const Vec2& uv0,
	  const Vec2& uv1,
	  UVIslandPtr pUVIsland,
	  std::vector<UVEdge>& outPrevConnectedUVEdges,
	  std::vector<UVEdge>& outNextConnectedUVEdges);
	static PolygonPtr FindPolygonContainingEdge(const Vec2& uv0, const Vec2& uv1, UVIslandPtr pUVIsland);
};

typedef std::map<EUVMappingToolGroup, std::set<EUVMappingTool>> UVMAPPINGTOOLGROUP_MAP;
class UVMappingToolGroupMapper
{
public:
	static UVMappingToolGroupMapper& the()
	{
		static UVMappingToolGroupMapper toolGroupMapper;
		return toolGroupMapper;
	}

	void AddTool(EUVMappingToolGroup group, EUVMappingTool tool)
	{
		m_ToolGroupMap[group].insert(tool);
	}

	std::vector<EUVMappingTool> GetToolList(EUVMappingToolGroup group)
	{
		auto iter = m_ToolGroupMap.find(group);
		std::vector<EUVMappingTool> toolList;
		toolList.insert(toolList.begin(), iter->second.begin(), iter->second.end());
		return toolList;
	}

private:

	UVMappingToolGroupMapper(){}
	UVMAPPINGTOOLGROUP_MAP m_ToolGroupMap;
};

class UVMappingToolRegister
{
public:
	UVMappingToolRegister(EUVMappingTool tool, EUVMappingToolGroup group, const char* name, const char* classname)
	{
		Serialization::getEnumDescription<EUVMappingTool>().add(tool, name, classname);
		UVMappingToolGroupMapper::the().AddTool(group, tool);
	}
};

static Serialization::EnumDescription& GetUVMapptingToolDesc()
{
	return Serialization::getEnumDescription<EUVMappingTool>();
}

static bool IsUVCCW(PolygonPtr polygon)
{
	std::vector<Vertex> vertices;
	if (!polygon->GetLinkedVertices(vertices))
		return false;

	int idxWithMinimumX = -1;
	float minimumX = 3e10f;

	for (int i = 0, iCount(vertices.size()); i < iCount; ++i)
	{
		if (vertices[i].uv.x < minimumX)
		{
			minimumX = vertices[i].uv.x;
			idxWithMinimumX = i;
		}
	}

	if (idxWithMinimumX == -1)
		return false;

	int previdx = ((idxWithMinimumX - 1) + vertices.size()) % vertices.size();
	int nextidx = (idxWithMinimumX + 1) % vertices.size();

	Vec3 v10 = (vertices[previdx].uv - vertices[idxWithMinimumX].uv).GetNormalized();
	Vec3 v20 = (vertices[nextidx].uv - vertices[idxWithMinimumX].uv).GetNormalized();

	return v10.Cross(v20).z > kDesignerEpsilon;
}

void SyncSelection();
void GetAllUVsFromUVIsland(UVIslandPtr pUVIsland, std::set<UVVertex>& outUVs);
}
}

#define PYTHON_DESIGNER_UVMAPPING_COMMAND(_key, _class)                        \
	namespace DesignerUVMappingCommand                                           \
	{                                                                            \
	static void Py ## _class()                                                   \
	{                                                                            \
	if (Designer::UVMapping::GetUVEditor())                                    \
	    Designer::UVMapping::GetUVEditor()->SetTool(Designer::UVMapping::_key);  \
	}                                                                            \
	}

typedef Designer::DesignerFactory<Designer::UVMapping::EUVMappingTool, Designer::UVMapping::BaseTool, int> UVMappingToolFactory;
#define REGISTER_UVMAPPING_TOOL(_key, _group, _name, _class)                                                                                 \
  static UVMappingToolFactory::ToolCreator<Designer::UVMapping::_class> uvMappingToolFactory ## _group ## _class(Designer::UVMapping::_key); \
  static Designer::UVMapping::UVMappingToolRegister uvToolRegister ## _class(Designer::UVMapping::_key, Designer::UVMapping::_group, _name, # _class);

#define REGISTER_UVMAPPING_TOOL_AND_COMMAND(_key, _group, _name, _class, _cmd_functionName, _cmd_description, _cmd_example) \
	REGISTER_UVMAPPING_TOOL(_key, _group, _name, _class)                                                                      \
	PYTHON_DESIGNER_UVMAPPING_COMMAND(_key, _class)                                                                           \
	REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(DesignerUVMappingCommand::Py ## _class, uvmapping, _cmd_functionName, _cmd_description, _cmd_example)

