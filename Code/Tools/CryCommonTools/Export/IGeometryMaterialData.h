// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IGEOMETRYMATERIALDATA_H__
#define __IGEOMETRYMATERIALDATA_H__

class IGeometryMaterialData
{
public:
	virtual void AddUsedMaterialIndex(int materialIndex) = 0;
	virtual int GetUsedMaterialCount() const = 0;
	virtual int GetUsedMaterialIndex(int usedMaterialIndex) const = 0;
};

#endif //__IGEOMETRYMATERIALDATA_H__
