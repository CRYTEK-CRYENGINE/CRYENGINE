// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef COMMON_TYPEINFO2_H
#define COMMON_TYPEINFO2_H

#include <CryMath/Cry_Vector3.h>

// IvoH:
// I'd like to have these structs in CommonTypeInfo.h
// But there seems to be no way without getting a linker error

STRUCT_INFO_T_BEGIN(Vec3_tpl, typename, F)
VAR_INFO(x)
VAR_INFO(y)
VAR_INFO(z)
STRUCT_INFO_T_END(Vec3_tpl, typename, F)

STRUCT_INFO_T_BEGIN(Vec4_tpl, typename, F)
VAR_INFO(x)
VAR_INFO(y)
VAR_INFO(z)
VAR_INFO(w)
STRUCT_INFO_T_END(Vec4_tpl, typename, F)

#endif
