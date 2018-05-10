// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "obbtree.h"

STRUCT_INFO_BEGIN(OBBnode)
	STRUCT_VAR_INFO(axes, TYPE_ARRAY(3, TYPE_INFO(Vec3)))
	STRUCT_VAR_INFO(center, TYPE_INFO(Vec3))
	STRUCT_VAR_INFO(size, TYPE_INFO(Vec3))
	STRUCT_VAR_INFO(iparent, TYPE_INFO(int))
	STRUCT_VAR_INFO(ichild, TYPE_INFO(int))
	STRUCT_VAR_INFO(ntris, TYPE_INFO(int))
STRUCT_INFO_END(OBBnode)

