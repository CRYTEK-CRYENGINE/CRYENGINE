// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IGEOMETRYFILE_H__
#define __IGEOMETRYFILE_H__

#include "ExportFileType.h"
#include <string>

class IGeometryFileData
{
public:
	struct SProperties
	{
		int  filetypeInt; // combination of flags from CryFileType
		bool bDoNotMerge;
		bool bUseCustomNormals;
		bool bUseF32VertexFormat;
		bool b8WeightsPerVertex;
		bool bVClothPreProcess;
		std::string customExportPath;
		
		SProperties() 			
			: filetypeInt(CRY_FILE_TYPE_NONE)
			, bDoNotMerge(false)
			, bUseCustomNormals(false)
			, bUseF32VertexFormat(false)
			, b8WeightsPerVertex(false)
			, bVClothPreProcess(false)
		{
		}
	};
public:
	virtual int AddGeometryFile(void* handle, const char* name, const SProperties &properties) = 0;
	virtual const SProperties& GetProperties(int geometryFileIndex) const = 0;
	virtual int GetGeometryFileCount() const = 0;
	virtual void* GetGeometryFileHandle(int geometryFileIndex) const = 0;
	virtual const char* GetGeometryFileName(int geometryFileIndex) const = 0;
};

#endif //__IGEOMETRYFILE_H__
