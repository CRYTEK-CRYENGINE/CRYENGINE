// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  CryEngine Source File.
//  Copyright (C), Crytek.
// -------------------------------------------------------------------------
//  File name:   ExcelReport.h
//  Created:     19/03/2008 by Timur.
//  Description: ExcelReporter
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ExcelReport_h__
#define __ExcelReport_h__
#pragma once

#include "ExcelExport.h"
#include "ConvertContext.h"

// Base class for custom CryEngine excel exporters
class CExcelReport : public CExcelExportBase
{
public:
	bool Export(IResourceCompiler* pRC, const char* filename, std::vector<CAssetFileInfo*>& files);

	void ExportSummary(const std::vector<CAssetFileInfo*>& files);
	void ExportTextures(const std::vector<CAssetFileInfo*>& files);
	void ExportCGF(const std::vector<CAssetFileInfo*>& files);
	void ExportCHR(const std::vector<CAssetFileInfo*>& files);
	void ExportCAF(const std::vector<CAssetFileInfo*>& files);

protected:
	IResourceCompiler *m_pRC;
};

#endif //__ExcelReport_h__