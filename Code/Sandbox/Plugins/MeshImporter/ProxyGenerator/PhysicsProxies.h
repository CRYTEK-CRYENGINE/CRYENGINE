// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>

#include <CryPhysics/primitives.h>
#include <CryPhysics/physinterface.h>

#include <vector>
#include <memory.h>

struct SPhysSrcMesh
{
	std::unique_ptr<IGeometry>     pMesh;
	std::vector<Vec3_tpl<index_t>> faces;
	uint64                         closedIsles, restoredIsles;
	std::vector<int>               isleTriCount;
	std::vector<int>               isleTriClosed;
};

struct SPhysProxies
{
	std::vector<phys_geometry*>   proxyGeometries;

	std::shared_ptr<SPhysSrcMesh> pSrc;
	IGeometry::SProxifyParams     params;
	int                           nMeshes;
	bool                          ready;

	void                          Serialize(Serialization::IArchive& ar);
};

