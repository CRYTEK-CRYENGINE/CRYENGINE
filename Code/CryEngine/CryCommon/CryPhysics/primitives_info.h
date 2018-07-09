// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "primitives.h"

STRUCT_INFO_TYPE_EMPTY(primitives::primitive)

STRUCT_INFO_BEGIN(primitives::box)
STRUCT_BASE_INFO(primitives::primitive)
STRUCT_VAR_INFO(Basis, TYPE_INFO(Matrix33))
STRUCT_VAR_INFO(bOriented, TYPE_INFO(int))
STRUCT_VAR_INFO(center, TYPE_INFO(Vec3))
STRUCT_VAR_INFO(size, TYPE_INFO(Vec3))
STRUCT_INFO_END(primitives::box)

STRUCT_INFO_BEGIN(primitives::sphere)
STRUCT_BASE_INFO(primitives::primitive)
STRUCT_VAR_INFO(center, TYPE_INFO(Vec3))
STRUCT_VAR_INFO(r, TYPE_INFO(float))
STRUCT_INFO_END(primitives::sphere)

STRUCT_INFO_BEGIN(primitives::cylinder)
STRUCT_BASE_INFO(primitives::primitive)
STRUCT_VAR_INFO(center, TYPE_INFO(Vec3))
STRUCT_VAR_INFO(axis, TYPE_INFO(Vec3))
STRUCT_VAR_INFO(r, TYPE_INFO(float))
STRUCT_VAR_INFO(hh, TYPE_INFO(float))
STRUCT_INFO_END(primitives::cylinder)

STRUCT_INFO_BEGIN(primitives::plane)
STRUCT_BASE_INFO(primitives::primitive)
STRUCT_VAR_INFO(n, TYPE_INFO(Vec3))
STRUCT_VAR_INFO(origin, TYPE_INFO(Vec3))
STRUCT_INFO_END(primitives::plane)
