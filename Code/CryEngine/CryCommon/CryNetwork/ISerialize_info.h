// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <CryNetwork/ISerialize.h> // <> required for Interfuscator

STRUCT_INFO_BEGIN(SNetObjectID)
STRUCT_VAR_INFO(id, TYPE_INFO(uint16))
STRUCT_VAR_INFO(salt, TYPE_INFO(uint16))
STRUCT_INFO_END(SNetObjectID)

STRUCT_INFO_EMPTY(SSerializeString)
