// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/STL.h>

template<class T, class I, class STORE>
struct DynArray;

template<class T, class I, class S>
bool Serialize(Serialization::IArchive& ar, DynArray<T, I, S>& container, const char* name, const char* label)
{
	Serialization::ContainerSTL<DynArray<T, I, S>, T> ser(&container);
	return ar(static_cast<Serialization::IContainer&>(ser), name, label);
}