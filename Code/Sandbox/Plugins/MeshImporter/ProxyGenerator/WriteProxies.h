// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryString/CryString.h>

namespace FbxTool
{
class CScene;
}

class CProxyData;

struct phys_geometry;

// A proxy tree stores the hierarchy of visible FBX nodes together with all their proxy geometries.
// This data structure is used to pass a copy of all relevant data to asynchronous tasks that need
// proxies.
struct SProxyTree
{
	struct SNode
	{
		size_t m_firstChildIndex;
		size_t m_childCount;
		size_t m_firstPhysGeometryIndex;
		size_t m_physGeometryCount;
		string m_name;
	};

	SProxyTree(const CProxyData* proxyData, const FbxTool::CScene* pFbxScene);
	~SProxyTree();

	std::vector<SNode> m_nodes;
	std::vector<phys_geometry*> m_physGeometries;
};

void WriteAutoGenProxies(const string& fname, const SProxyTree* pProxyTree);

