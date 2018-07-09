// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Serialization.h"

namespace Serialization {

void EDITOR_COMMON_API SerializeToMemory(std::vector<char>* buffer, const SStruct& obj)
{
	MemoryOArchive oa;
	oa(obj);
	buffer->assign(oa.buffer(), oa.buffer() + oa.length());
}

void EDITOR_COMMON_API SerializeToMemory(DynArray<char>* buffer, const SStruct& obj)
{
	MemoryOArchive oa;
	oa(obj);
	buffer->assign(oa.buffer(), oa.buffer() + oa.length());
}

void EDITOR_COMMON_API SerializeFromMemory(const SStruct& obj, const std::vector<char>& buffer)
{
	MemoryIArchive ia;
	if (!ia.open(buffer.data(), buffer.size()))
		return;
	ia(obj);
}

void EDITOR_COMMON_API SerializeFromMemory(const SStruct& obj, const DynArray<char>& buffer)
{
	MemoryIArchive ia;
	if (!ia.open(buffer.data(), buffer.size()))
		return;
	ia(obj);
}

}

