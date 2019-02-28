// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PhysicsProxies.h"

#include <CrySerialization/yasli/decorators/Range.h>

void SPhysProxies::Serialize(Serialization::IArchive& ar)
{
	bool axisAligned = !is_unused(params.qForced), mergeIslands = params.mergeIslands, forceBBox = params.forceBBox, convexHull = params.convexHull;
	bool findPrimSurfaces = params.findPrimSurfaces, findPrimLines = params.findPrimLines, findMeshes = params.findMeshes;
	ar(mergeIslands, "mergeIslands", "Merge islands");
	ar(axisAligned, "axisAligned", "Axis-aligned voxels");
	ar(forceBBox, "forceBBox", "Force single box");
	ar(convexHull, "convexHull", "Convex hull");
	ar(findPrimSurfaces, "findPrimSurfaces", "Detect primitive surfaces");
	ar(findPrimLines, "findPrimLines", "Detect primitive rods");
	ar(params.ncells, "ncells", ">Voxel cell number");
	ar(params.surfMeshIters, "surfMeshIters", ">Mesh smoothing passes");
	MARK_UNUSED params.qForced;
	axisAligned ? params.qForced.SetIdentity(), 0 : 0;
	params.mergeIslands = mergeIslands ? 1 : 0;
	params.forceBBox = forceBBox ? 1 : 0;
	params.convexHull = convexHull ? 1 : 0;
	params.findPrimSurfaces = findPrimSurfaces ? 1 : 0;
	params.findPrimLines = findPrimLines ? 1 : 0;
	params.findMeshes = findMeshes ? 1 : 0;

	struct
	{
		IGeometry::SProxifyParams& params;
		void                       Serialize(Serialization::IArchive& ar)
		{
			bool surfMaxAndMinNorms = params.surfMaxAndMinNorms, surfRefineWithMesh = params.surfRefineWithMesh;
			ar(surfMaxAndMinNorms, "surfMinAndMaxNorms", "Use min and max normals as mesh vertices");
			ar(params.inflatePrims, "inflatePrims", "Inflate primitives");
			ar(params.inflateMeshes, "inflateMeshes", "Inflate meshes");
			ar(params.primVfillLine, "primVfillLine", "Minimal rod primitive volume efficiency");
			ar(params.primVfillSurf, "primVfillSurf", "Minimal surface primitive volume efficiency");
			ar(params.capsHRratio, "capsHratio", "Height/radius threshold for capsule/cylinder rods");
			ar(params.surfMergeDist, "surfMergeDist", "Merge primitive surfaces threshold (in voxels)");
			ar(params.surfPrimIters, "surfPrimIters", "Surface detection smooting iterations");
			ar(yasli::Range(params.surfMinNormLen, 0.0f, 1.0f), "surfMinNormLen", "Surface flatness threshold");
			ar(yasli::Range(params.surfNormRefineThresh, 0.0f, 1.0f), "surfNormRefineThresh", "Surface alignment with mesh normal threshold");
			ar(params.maxLineDist, "maxLineDist", "Rod candidates merge threshold (in voxels)");
			ar(yasli::Range(params.maxLineDot, 0.0f, 1.0f), "maxLineDot", "Rod candidates curvature threshold");
			ar(params.primVoxInflate, "primVoxInflate", "Inflate primitives for voxel removal (in cells)");
			ar(params.primRefillThresh, "primRefillThresh", "Primitives holes refill ratio");
			ar(yasli::Range(params.primSurfOutside, 0.0f, 1.0f), "primSurfOutside", "Required primitive exposed area");
			ar(params.minSurfCells, "minSurfCells", "Minimal surface primitive voxels number");
			ar(params.minLineCells, "minLineCells", "Minimal rod primitive voxels number");
			ar(params.surfMeshMinCells, "surfMeshMinCells", "Minimal mesh voxels number");
			ar(surfRefineWithMesh, "surfRefineWithMesh", "Refine voxels with source mesh");
			params.surfMaxAndMinNorms = surfMaxAndMinNorms ? 1 : 0;
			params.surfRefineWithMesh = surfRefineWithMesh ? 1 : 0;
		}
	} advanced = { params };
	ar(advanced, "advanced", "Advanced settings");
}

