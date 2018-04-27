// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GeometryMaterialData.h"

void GeometryMaterialData::AddUsedMaterialIndex(int materialIndex)
{
	std::map<int, int>::iterator usedMaterialPos = m_usedMaterialIndexIndexMap.find(materialIndex);
	if (usedMaterialPos == m_usedMaterialIndexIndexMap.end())
	{
		int materialIndexIndex = int(m_usedMaterialIndices.size());
		m_usedMaterialIndices.push_back(materialIndex);
		m_usedMaterialIndexIndexMap.insert(std::make_pair(materialIndex, materialIndexIndex));
	}
}

int GeometryMaterialData::GetUsedMaterialCount() const
{
	return int(m_usedMaterialIndices.size());
}

int GeometryMaterialData::GetUsedMaterialIndex(int usedMaterialIndex) const
{
	return m_usedMaterialIndices[usedMaterialIndex];
}
