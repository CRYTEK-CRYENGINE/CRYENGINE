// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>

template<class T> class _smart_ptr;

template<class T>
bool Serialize(Serialization::IArchive& ar, _smart_ptr<T>& ptr, const char* name, const char* label);

#include "SmartPtrImpl.h"
