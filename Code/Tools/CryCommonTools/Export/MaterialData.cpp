// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MaterialData.h"

int MaterialData::AddMaterial(const char* name, int id, void* handle, const char* properties)
{
	const int materialIndex = int(m_materials.size());
	m_materials.push_back(MaterialEntry(name, id, "submat", handle, properties));
	return materialIndex;
}

int MaterialData::AddMaterial(const char* name, int id, const char* subMatName, void* handle, const char* properties)
{
	const int materialIndex = int(m_materials.size());
	m_materials.push_back(MaterialEntry(name, id, subMatName, handle, properties));
	return materialIndex;
}

int MaterialData::GetMaterialCount() const
{
	return int(m_materials.size());
}

const char* MaterialData::GetName(int materialIndex) const
{
	assert(materialIndex >= 0);
	assert(materialIndex < int(m_materials.size()));
	return m_materials[materialIndex].name.c_str();
}

int MaterialData::GetID(int materialIndex) const
{
	assert(materialIndex >= 0);
	assert(materialIndex < int(m_materials.size()));
	return m_materials[materialIndex].id;
}

const char* MaterialData::GetSubMatName(int materialIndex) const
{
	assert(materialIndex >= 0);
	assert(materialIndex < int(m_materials.size()));
	return m_materials[materialIndex].subMatName.c_str();
}

void* MaterialData::GetHandle(int materialIndex) const
{
	assert(materialIndex >= 0);
	assert(materialIndex < int(m_materials.size()));
	return m_materials[materialIndex].handle;
}

const char* MaterialData::GetProperties(int materialIndex) const
{
	assert(materialIndex >= 0);
	assert(materialIndex < int(m_materials.size()));
	return m_materials[materialIndex].properties.c_str();
}
