// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MATERIALDATA_H__
#define __MATERIALDATA_H__

#include "IMaterialData.h"

class MaterialData : public IMaterialData
{
public:
	virtual int AddMaterial(const char* name, int id, void* handle, const char* properties);
	virtual int AddMaterial(const char* name, int id, const char* subMatName, void* handle, const char* properties);
	virtual int GetMaterialCount() const;
	virtual const char* GetName(int materialIndex) const;
	virtual int GetID(int materialIndex) const;
	virtual const char* GetSubMatName(int materialIndex) const;
	virtual void* GetHandle(int materialIndex) const;
	virtual const char* GetProperties(int materialIndex) const;

private:
	struct MaterialEntry
	{
		MaterialEntry(const char* a_name, int a_id, const char* a_subMatName, void* a_handle, const char* a_properties)
			: name(a_name)
			, id(a_id)
			, subMatName(a_subMatName)
			, handle(a_handle) 
			, properties(a_properties ? a_properties : "")
		{
		}

		string name;
		int id;
		string subMatName;
		void* handle;
		string properties;
	};

	std::vector<MaterialEntry> m_materials;
};

#endif //__MATERIALDATA_H__
