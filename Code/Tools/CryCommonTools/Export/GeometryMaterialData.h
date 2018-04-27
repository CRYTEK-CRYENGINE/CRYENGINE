// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __GEOMETRYMATERIALDATA_H__
#define __GEOMETRYMATERIALDATA_H__

#include "IGeometryMaterialData.h"

class GeometryMaterialData : public IGeometryMaterialData
{
public:
	// IGeometryMaterialData
	virtual void AddUsedMaterialIndex(int materialIndex);
	virtual int GetUsedMaterialCount() const;
	virtual int GetUsedMaterialIndex(int usedMaterialIndex) const;

private:
	std::vector<int> m_usedMaterialIndices;
	std::map<int, int> m_usedMaterialIndexIndexMap;
};

#endif //__GEOMETRYMATERIALDATA_H__
