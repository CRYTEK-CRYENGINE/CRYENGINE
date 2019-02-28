// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "aabbtree.h"

STRUCT_INFO_BEGIN(AABBnode)
	STRUCT_VAR_INFO(ichild, TYPE_INFO(unsigned int))
	STRUCT_VAR_INFO(minx, TYPE_INFO(unsigned char))
	STRUCT_VAR_INFO(maxx, TYPE_INFO(unsigned char))
	STRUCT_VAR_INFO(miny, TYPE_INFO(unsigned char))
	STRUCT_VAR_INFO(maxy, TYPE_INFO(unsigned char))
	STRUCT_VAR_INFO(minz, TYPE_INFO(unsigned char))
	STRUCT_VAR_INFO(maxz, TYPE_INFO(unsigned char))
	STRUCT_VAR_INFO(ntris, TYPE_INFO(unsigned char))
	STRUCT_VAR_INFO(bSingleColl, TYPE_INFO(unsigned char))
STRUCT_INFO_END(AABBnode)

STRUCT_INFO_BEGIN(AABBnodeV0)
	STRUCT_BITFIELD_INFO(minx, unsigned int, 7)
	STRUCT_BITFIELD_INFO(maxx, unsigned int, 7)
	STRUCT_BITFIELD_INFO(miny, unsigned int, 7)
	STRUCT_BITFIELD_INFO(maxy, unsigned int, 7)
	STRUCT_BITFIELD_INFO(minz, unsigned int, 7)
	STRUCT_BITFIELD_INFO(maxz, unsigned int, 7)
	STRUCT_BITFIELD_INFO(ichild, unsigned int, 15)
	STRUCT_BITFIELD_INFO(ntris, unsigned int, 6)
	STRUCT_BITFIELD_INFO(bSingleColl, unsigned int, 1)
STRUCT_INFO_END(AABBnodeV0)

