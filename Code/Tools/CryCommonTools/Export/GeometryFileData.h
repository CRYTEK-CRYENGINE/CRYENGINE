// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __GEOMETRYFILEDATA_H__
#define __GEOMETRYFILEDATA_H__

#include "IGeometryFileData.h"
#include "STLHelpers.h"

class GeometryFileData : public IGeometryFileData
{
public:
	// IGeometryFileData
	virtual int AddGeometryFile(void* handle, const char* name, const SProperties &properties);
	virtual const SProperties& GetProperties(int geometryFileIndex) const;
	virtual void SetProperties(int geometryFileIndex, const SProperties& properties);
	virtual int GetGeometryFileCount() const;
	virtual void* GetGeometryFileHandle(int geometryFileIndex) const;
	virtual const char* GetGeometryFileName(int geometryFileIndex) const;

private:
	struct GeometryFileEntry
	{
		GeometryFileEntry(void* a_handle, const char* a_name, const SProperties &a_properties)
			: handle(a_handle)
			, name(a_name)
			, properties(a_properties)
		{
		}

		void* handle;
		std::string name;
		SProperties properties;
	};

	std::vector<GeometryFileEntry> m_geometryFiles;
};

#endif //__GEOMETRYFILEDATA_H__
