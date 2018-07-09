// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ExcelExport.h
//  Created:     19/03/2008 by Timur.
//  Description: Implementation of the CryEngine Unit Testing framework
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once
#include <CrySystem/XML/IXml.h>

// Base class for custom CryEngine excel exporers
class CExcelExportBase
{
public:
	enum CellFlags
	{
		CELL_BOLD     = 0x0001,
		CELL_CENTERED = 0x0002,
	};

	bool       SaveToFile(const char* filename);

	void       InitExcelWorkbook(XmlNodeRef Workbook);

	XmlNodeRef NewWorksheet(const char* name);
	void       AddCell(float number);
	void       AddCell(int number);
	void       AddCell(uint32 number);
	void       AddCell(uint64 number) { AddCell((uint32)number); };
	void       AddCell(int64 number)  { AddCell((int)number); };
	void       AddCell(const char* str, int flags = 0);
	void       AddCellAtIndex(int nIndex, const char* str, int flags = 0);
	void       SetCellFlags(XmlNodeRef cell, int flags);
	void       AddRow();
	void       AddCell_SumOfRows(int nRows);
	string     GetXmlHeader();

protected:
	XmlNodeRef m_Workbook;
	XmlNodeRef m_CurrTable;
	XmlNodeRef m_CurrWorksheet;
	XmlNodeRef m_CurrRow;
	XmlNodeRef m_CurrCell;
};
