// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  CryEngine Source File.
//  Copyright (C), Crytek.
// -------------------------------------------------------------------------
//  File name:   ExcelReport.cpp
//  Created:     19/03/2008 by Timur.
//  Description: Implementation of the CryEngine Unit Testing framework
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "AssetFileInfo.h"
#include <CryString/CryPath.h>
#include "ExcelReport.h"
#include "IResCompiler.h"

//////////////////////////////////////////////////////////////////////////
struct FilesComparePredicate
{
	bool operator()(const CAssetFileInfo* p1,const CAssetFileInfo* p2)
	{
		if (p1->m_bSuccess != p2->m_bSuccess)
		{
			return p1->m_bSuccess < p2->m_bSuccess;
		}
		return p1->m_DstFileSize > p2->m_DstFileSize;
	}
};

//////////////////////////////////////////////////////////////////////////
static inline __int64 ComputeSizeInKB(__int64 sz)
{
	return (sz < 0) ? 0 : (sz + 512) / 1024;
}

//////////////////////////////////////////////////////////////////////////
bool CExcelReport::Export(IResourceCompiler* pRC,const char* filename, std::vector<CAssetFileInfo*>& files)
{
	m_pRC = pRC;
	XmlNodeRef WorkBook = NewWorkbook();

	std::sort(files.begin(), files.end(), FilesComparePredicate());

	ExportSummary(files);
	ExportTextures(files);
	ExportCGF(files);
	ExportCHR(files);
	ExportCAF(files);

	return SaveToFile(filename);
}

//////////////////////////////////////////////////////////////////////////
void CExcelReport::ExportSummary(const std::vector<CAssetFileInfo*>& files)
{
	NewWorksheet( "Summary" );

	const SFileVersion& fv = m_pRC->GetFileVersion();

	AddColumn( "",150 );
	AddColumn( "",150 );

	AddRow();
	AddCell( "RC Version",CELL_BOLD );
	char buffer[1024];
	_snprintf_s( buffer,sizeof(buffer),_TRUNCATE,"%d.%d.%d",fv.v[2],fv.v[1],fv.v[0] );
	AddCell( buffer );

	AddRow();
	AddCell( "RC Compile Time",CELL_BOLD );
	_snprintf_s( buffer,sizeof(buffer),_TRUNCATE,"%s %s", __DATE__, __TIME__ );
	AddCell( buffer );

	AddRow();
	AddRow();

	size_t unusedCount = 0;
	uint64 unusedSize = 0;
	for (size_t i = 0; i < files.size(); i++)
	{
		const CAssetFileInfo& afi = *files[i];
		if (!afi.m_bReferencedInLevels &&
		    (afi.m_type == CAssetFileInfo::eTexture ||
		     afi.m_type == CAssetFileInfo::eCGF ||
		     afi.m_type == CAssetFileInfo::eCHR))
		{
			++unusedCount;
			unusedSize += afi.m_DstFileSize;
		}
	}
	_snprintf_s( buffer,sizeof(buffer),_TRUNCATE,"%d files processed",(int)files.size() );
	AddCell( buffer );
	AddRow();
	const unsigned int bytesInMegabyte = 1024 * 1024;
	_snprintf_s( buffer,sizeof(buffer),_TRUNCATE,"%u unused assets found (%u MB)",(unsigned int)unusedCount,(unsigned int)(unusedSize / bytesInMegabyte) );
	AddCell( buffer );

}

//////////////////////////////////////////////////////////////////////////
void CExcelReport::ExportTextures(const std::vector<CAssetFileInfo*>& files)
{
	NewWorksheet( "Textures" );

	FreezeFirstRow();
	AutoFilter(1,10);

	BeginColumns();
	AddColumn( "Ok",40 );
	AddColumn( "File",400 );
	AddColumn( "In Levels", 60 );
	AddColumn( "User",80 );
	AddColumn( "Size (KB)",80 );
	AddColumn( "Width",50 );
	AddColumn( "Height",50 );
	AddColumn( "Mips",50 );
	AddColumn( "Format",80 );
	AddColumn( "Type",80 );
	AddColumn( "Alpha",50 );
	AddColumn( "Sides",50 );
	AddColumn( "Perforce",100 );
	AddColumn( "Error",100 );
	EndColumns();

	for (uint32 i = 0; i < files.size(); i++)
	{
		const CAssetFileInfo& afi = *files[i];
		if (afi.m_type != CAssetFileInfo::eTexture)
			continue;

		AddRow();

		AddCell( afi.m_bSuccess ? "OK" : "FAIL", (afi.m_bSuccess ? 0 : CELL_HIGHLIGHT) );
		AddCell( afi.m_sSourceFilename );
		AddCell( afi.m_bReferencedInLevels ? "USED" : "NOT USED", afi.m_bReferencedInLevels ? CELL_CENTERED : CELL_BOLD | CELL_CENTERED );

		AddCell( ComputeSizeInKB(afi.m_DstFileSize) );
		AddCell( afi.m_textureInfo.w );
		AddCell( afi.m_textureInfo.h );
		AddCell( afi.m_textureInfo.nNumMips );
		AddCell( afi.m_textureInfo.format,CELL_CENTERED  );

		const char *type = "2D";
		if (afi.m_textureInfo.nDepth > 1)
			type = "3D";
		else if (afi.m_textureInfo.nSides > 1)
			type = "Cubemap";
		AddCell( type,CELL_CENTERED );
		AddCell( (afi.m_textureInfo.bAlpha) ? "Yes" : "",CELL_CENTERED );
		AddCell( afi.m_textureInfo.nSides );

		AddCell( afi.m_sErrorLog );
	}
}

//////////////////////////////////////////////////////////////////////////
void CExcelReport::ExportCGF(const std::vector<CAssetFileInfo*>& files)
{
	NewWorksheet( "Geometry" );

	FreezeFirstRow();
	AutoFilter(1,15);

	BeginColumns();
	AddColumn( "Ok",40 );
	AddColumn( "File",400 );
	AddColumn( "In Levels", 60 );
	AddColumn( "User",80 );
	AddColumn( "File Size",80 );
	AddColumn( "Mesh Size (KB)",80 );
	AddColumn( "Mesh Size Lod0 (KB)",80 );
	AddColumn( "LODs",50 );
	AddColumn( "Sub Meshes",50 );
	AddColumn( "Vertices",50 );
	AddColumn( "Tris",50 );
	AddColumn( "Joints",50 );
	AddColumn( "Phys Tris",80 );
	AddColumn( "Phys Size (KB)",80 );
	AddColumn( "Phys Proxies",80 );
	AddColumn( "LODs Tris",80 );
	AddColumn( "Split LODs",80 );
	AddColumn( "Perforce",100 );
	AddColumn( "Error",100 );
	EndColumns();

	for (uint32 i = 0; i < files.size(); i++)
	{
		const CAssetFileInfo& afi = *files[i];
		if (afi.m_type != CAssetFileInfo::eCGF)
			continue;

		int nMeshSizeTotal = 0;
		for (int lod = 0; lod < CAssetFileInfo::kMaxCgfLods; lod++)
			nMeshSizeTotal += afi.m_geomInfo.nMeshSizePerLod[lod];

		AddRow();

		AddCell( afi.m_bSuccess ? "OK" : "FAIL", (afi.m_bSuccess ? 0 : CELL_HIGHLIGHT) );
		AddCell( afi.m_sSourceFilename );
		AddCell( afi.m_bReferencedInLevels ? "USED" : "NOT USED", afi.m_bReferencedInLevels ? CELL_CENTERED : CELL_BOLD | CELL_CENTERED );
		AddCell( ComputeSizeInKB(afi.m_DstFileSize) );
		AddCell( (nMeshSizeTotal+512)/1024 );
		AddCell( (afi.m_geomInfo.nMeshSize+512)/1024 );
		AddCell( afi.m_geomInfo.nLods );
		AddCell( afi.m_geomInfo.nSubMeshCount );
		AddCell( afi.m_geomInfo.nVertices );
		AddCell( afi.m_geomInfo.nIndices/3 );
		AddCell( afi.m_geomInfo.nJoints );
		AddCell( afi.m_geomInfo.nPhysTriCount );
		AddCell( (afi.m_geomInfo.nPhysProxySize+512)/1024 );
		AddCell( (afi.m_geomInfo.nPhysProxyCount) );

		if (afi.m_geomInfo.nLods > 1)
		{
			// Print lod1/lod2/lod3 ...
			char tempstr[256];
			char numstr[32];
			tempstr[0] = 0;
			int numlods = 0;
			for (int lod = 0; lod < CAssetFileInfo::kMaxCgfLods; lod++)
			{
				if (afi.m_geomInfo.nIndicesPerLod[lod] != 0)
				{
					_snprintf_s(numstr,sizeof(numstr),_TRUNCATE,"%d",(afi.m_geomInfo.nIndicesPerLod[lod]/3) );
					if (numlods > 0)
						cry_strcat(tempstr, " / ");
					cry_strcat(tempstr, numstr);
					numlods++;
				}
			}
			AddCell(tempstr,CELL_CENTERED);
		}
		else
		{
			AddCell("");
		}

		AddCell( (afi.m_geomInfo.bSplitLods) ? "Yes" : "" );
		
		AddCell( afi.m_sErrorLog );
	}
}


//////////////////////////////////////////////////////////////////////////
void CExcelReport::ExportCHR(const std::vector<CAssetFileInfo*>& files)
{
	NewWorksheet( "Characters" );

	FreezeFirstRow();
	AutoFilter(1,15);

	BeginColumns();
	AddColumn( "Ok",40 );
	AddColumn( "File",400 );
	AddColumn( "In Levels", 60 );
	AddColumn( "User",80 );
	AddColumn( "File Size",80 );
	AddColumn( "Mesh Size (KB)",80 );
	AddColumn( "Mesh Size Lod0 (KB)",80 );
	AddColumn( "LODs",50 );
	AddColumn( "Sub Meshes",50 );
	AddColumn( "Vertices",50 );
	AddColumn( "Tris",50 );
	AddColumn( "Phys Tris",80 );
	AddColumn( "Phys Size (KB)",80 );
	AddColumn( "Phys Proxies",80 );
	AddColumn( "LODs Tris",80 );
	AddColumn( "Perforce",100 );
	AddColumn( "Error",100 );
	EndColumns();

	for (uint32 i = 0; i < files.size(); i++)
	{
		const CAssetFileInfo& afi = *files[i];
		if (afi.m_type != CAssetFileInfo::eCHR)
			continue;

		int nMeshSizeTotal = 0;
		for (int lod = 0; lod < CAssetFileInfo::kMaxCgfLods; lod++)
			nMeshSizeTotal += afi.m_geomInfo.nMeshSizePerLod[lod];

		AddRow();

		AddCell( afi.m_bSuccess ? "OK" : "FAIL", (afi.m_bSuccess ? 0 : CELL_HIGHLIGHT) );
		AddCell( afi.m_sSourceFilename );
		AddCell( afi.m_bReferencedInLevels ? "USED" : "NOT USED", afi.m_bReferencedInLevels ? CELL_CENTERED : CELL_BOLD | CELL_CENTERED );
		AddCell( ComputeSizeInKB(afi.m_DstFileSize) );
		AddCell( (nMeshSizeTotal+512)/1024 );
		AddCell( (afi.m_geomInfo.nMeshSize+512)/1024 );
		AddCell( afi.m_geomInfo.nLods );
		AddCell( afi.m_geomInfo.nSubMeshCount );
		AddCell( afi.m_geomInfo.nVertices );
		AddCell( afi.m_geomInfo.nIndices/3 );
		AddCell( afi.m_geomInfo.nPhysTriCount );
		AddCell( (afi.m_geomInfo.nPhysProxySize+512)/1024 );
		AddCell( (afi.m_geomInfo.nPhysProxyCount) );

		if (afi.m_geomInfo.nLods > 1)
		{
			// Print lod1/lod2/lod3 ...
			char tempstr[256];
			char numstr[32];
			tempstr[0] = 0;
			int numlods = 0;
			for (int lod = 0; lod < CAssetFileInfo::kMaxCgfLods; lod++)
			{
				if (afi.m_geomInfo.nIndicesPerLod[lod] != 0)
				{
					sprintf_s(numstr,sizeof(numstr),"%d",(afi.m_geomInfo.nIndicesPerLod[lod]/3) );
					if (numlods > 0)
						cry_strcat(tempstr, " / ");
					cry_strcat(tempstr, numstr);
					numlods++;
				}
			}
			AddCell(tempstr,CELL_CENTERED);
		}
		else
		{
			AddCell("");
		}

		AddCell( afi.m_sErrorLog );
	}
}



//////////////////////////////////////////////////////////////////////////
void CExcelReport::ExportCAF(const std::vector<CAssetFileInfo*>& files)
{
	NewWorksheet( "Animations" );

	FreezeFirstRow();
	AutoFilter(1,5);

	BeginColumns();
	AddColumn( "Ok",40 );
	AddColumn( "File",400 );
	AddColumn( "User",80 );
	AddColumn( "File Size",80 );
	
	AddColumn( "Perforce",100 );
	AddColumn( "Error",100 );
	EndColumns();

	for (uint32 i = 0; i < files.size(); i++)
	{
		const CAssetFileInfo& afi = *files[i];
		if (afi.m_type != CAssetFileInfo::eCAF)
			continue;

		AddRow();

		AddCell( afi.m_bSuccess ? "OK" : "FAIL", (afi.m_bSuccess ? 0 : CELL_HIGHLIGHT) );
		AddCell( afi.m_sSourceFilename );
		AddCell( ComputeSizeInKB(afi.m_DstFileSize) );

		AddCell( afi.m_sErrorLog );
	}
}
