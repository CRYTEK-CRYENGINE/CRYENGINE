// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  CryEngine Source File.
//  Copyright (C), Crytek.
// -------------------------------------------------------------------------
//  File name:   ExcelExport.h
//  Created:     19/03/2008 by Timur.
//  Description: ExcelExporter
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ExcelExport_h__
#define __ExcelExport_h__
#pragma once

#include <CrySystem/XML/IXml.h>

// Base class for custom CryEngine excel exporters
class CExcelExportBase
{
public:
	enum CellFlags
	{
		CELL_BOLD = 0x0001,
		CELL_CENTERED = 0x0002,
		CELL_HIGHLIGHT = 0x0004,
	};

	bool SaveToFile( const char *filename ) const;

	XmlNodeRef NewWorkbook();
	void InitExcelWorkbook( XmlNodeRef Workbook );

	XmlNodeRef NewWorksheet( const char *name );

	XmlNodeRef AddColumn( const char *name,int nWidth );
	void BeginColumns();
	void EndColumns();

	void AddCell( float number );
	void AddCell( int number );
	void AddCell( uint32 number );
	void AddCell( uint64 number ) { AddCell( (uint32)number ); };
	void AddCell( int64 number ) { AddCell( (int)number ); };
	void AddCell( const char *str,int flags=0 );
	void AddCellAtIndex( int nIndex,const char *str,int flags=0 );
	void SetCellFlags( XmlNodeRef cell,int flags );
	void AddRow();
	void AddCell_SumOfRows( int nRows );
	string GetXmlHeader() const;

	void FreezeFirstRow();
	void AutoFilter( int nRow,int nNumColumns );

protected:
	XmlNodeRef m_Workbook;
	XmlNodeRef m_CurrTable;
	XmlNodeRef m_CurrWorksheet;
	XmlNodeRef m_CurrRow;
	XmlNodeRef m_CurrCell;
	std::vector<string> m_columns;
};

#endif //__ExcelExport_h__
