// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Core/Polygon.h"

class CBrushObject;
class CPakFile;
class CCryMemFile;

namespace Designer
{
class AreaSolidObject;
class ClipVolumeObject;
struct ExportedBrushGeom
{
	enum EFlags
	{
		SUPPORT_LIGHTMAP = 0x01,
		NO_PHYSICS       = 0x02,
	};
	int  size;
	char filename[128];
	int  flags;
	Vec3 m_minBBox;
	Vec3 m_maxBBox;
};

struct ExportedBrushMaterial
{
	int  size;
	char material[64];
};

class Exporter
{
public:
	void ExportBrushes(const string& path, CPakFile& pakFile);

private:
	bool ExportAreaSolid(const string& path, AreaSolidObject* pAreaSolid, CPakFile& pakFile) const;
	bool ExportClipVolume(const string& path, ClipVolumeObject* pClipVolume, CPakFile& pakFile);
	void ExportStatObj(const string& path, IStatObj* pStatObj, CBaseObject* pObj, int renderFlag, const string& sGeomFileName, CPakFile& pakFile);

	struct AreaSolidStatistic
	{
		int numOfClosedPolygons;
		int numOfOpenPolygons;
		int totalSize;
	};

	static void ComputeAreaSolidMemoryStatistic(AreaSolidObject* pAreaSolid, AreaSolidStatistic& outStatistic, std::vector<PolygonPtr>& optimizedPolygons);

	//////////////////////////////////////////////////////////////////////////
	std::map<CMaterial*, int>          m_mtlMap;
	std::vector<ExportedBrushGeom>     m_geoms;
	std::vector<ExportedBrushMaterial> m_materials;
};
}

