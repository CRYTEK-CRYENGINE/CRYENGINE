// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "terrain_sector.h"

STRUCT_INFO_BEGIN(STerrainNodeChunk)
STRUCT_VAR_INFO(nChunkVersion, TYPE_INFO(int16))
STRUCT_VAR_INFO(bHasHoles, TYPE_INFO(int16))
STRUCT_VAR_INFO(boxHeightmap, TYPE_INFO(AABB))
STRUCT_VAR_INFO(fOffset, TYPE_INFO(float))
STRUCT_VAR_INFO(fRange, TYPE_INFO(float))
STRUCT_VAR_INFO(nSize, TYPE_INFO(int))
STRUCT_VAR_INFO(nSurfaceTypesNum, TYPE_INFO(int))
STRUCT_INFO_END(STerrainNodeChunk)

STRUCT_INFO_BEGIN(SHeightMapItem)
STRUCT_BITFIELD_INFO(surface, TYPE_INFO(uint32), 20)
STRUCT_BITFIELD_INFO(height, TYPE_INFO(uint32), 12)
STRUCT_INFO_END(SHeightMapItem)
