// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

template<uint32 StoreStrings, typename THash>
struct SCRCRef;

#include <CrySerialization/Forward.h>

template<uint32 StoreStrings, typename THash>
bool Serialize(Serialization::IArchive& ar, SCRCRef<StoreStrings, THash>& crcRef, const char* name, const char* label);

#include "CRCRefImpl.h"
