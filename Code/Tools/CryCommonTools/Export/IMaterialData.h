// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IMATERIALDATA_H__
#define __IMATERIALDATA_H__

class IMaterialData
{
public:
	virtual int AddMaterial(const char* name, int id, void* handle, const char* properties) = 0;
	virtual int AddMaterial(const char* name, int id, const char* subMatName, void* handle, const char* properties) = 0;
	virtual int GetMaterialCount() const = 0;
	virtual const char* GetName(int materialIndex) const = 0;
	virtual int GetID(int materialIndex) const = 0;
	virtual const char* GetSubMatName(int materialIndex) const = 0;
	virtual void* GetHandle(int materialIndex) const = 0;
	virtual const char* GetProperties(int materialIndex) const = 0;
};

#endif //__IMATERIALDATA_H__
