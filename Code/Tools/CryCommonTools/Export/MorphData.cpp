// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MorphData.h"

MorphData::MorphData()
: m_handle(0)
{
}

void MorphData::SetHandle(void* handle)
{
	m_handle = handle;
}

void MorphData::AddMorph(void* handle, const char* name, const char* fullname)
{
	m_morphs.push_back(Entry(handle, name, fullname ? fullname : ""));
}

void* MorphData::GetHandle() const
{
	return m_handle;
}

int MorphData::GetMorphCount() const
{
	return int(m_morphs.size());
}

std::string MorphData::GetMorphName(int morphIndex) const
{
	return m_morphs[morphIndex].name;
}

std::string MorphData::GetMorphFullName(int morphIndex) const
{
	return m_morphs[morphIndex].fullname.length() > 0 ? m_morphs[morphIndex].fullname : m_morphs[morphIndex].name;
}

void* MorphData::GetMorphHandle(int morphIndex) const
{
	return m_morphs[morphIndex].handle;
}
