// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ExcelExport.cpp
//  Created:     19/03/2008 by Timur.
//  Description: Implementation of the CryEngine Unit Testing framework
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ExcelExport.h"
#include <CryString/CryPath.h>

//////////////////////////////////////////////////////////////////////////
string CExcelExportBase::GetXmlHeader()
{
	return "<?xml version=\"1.0\"?>\n<?mso-application progid=\"Excel.Sheet\"?>\n";
}

//////////////////////////////////////////////////////////////////////////
void CExcelExportBase::InitExcelWorkbook(XmlNodeRef Workbook)
{
	m_Workbook = Workbook;
	m_Workbook->setTag("Workbook");
	m_Workbook->setAttr("xmlns", "urn:schemas-microsoft-com:office:spreadsheet");
	m_Workbook->setAttr("xmlns:o", "urn:schemas-microsoft-com:office:office");
	m_Workbook->setAttr("xmlns:x", "urn:schemas-microsoft-com:office:excel");
	m_Workbook->setAttr("xmlns:o", "urn:schemas-microsoft-com:office:office");
	m_Workbook->setAttr("xmlns:ss", "urn:schemas-microsoft-com:office:spreadsheet");
	m_Workbook->setAttr("xmlns:html", "http://www.w3.org/TR/REC-html40");

	XmlNodeRef ExcelWorkbook = Workbook->newChild("ExcelWorkbook");
	ExcelWorkbook->setAttr("xmlns", "urn:schemas-microsoft-com:office:excel");

	XmlNodeRef Styles = m_Workbook->newChild("Styles");
	{
		// Style s25
		// Bold header, With Background Color.
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID", "s25");
		XmlNodeRef StyleFont = Style->newChild("Font");
		StyleFont->setAttr("x:CharSet", "204");
		StyleFont->setAttr("x:Family", "Swiss");
		StyleFont->setAttr("ss:Bold", "1");
		XmlNodeRef StyleInterior = Style->newChild("Interior");
		StyleInterior->setAttr("ss:Color", "#00FF00");
		StyleInterior->setAttr("ss:Pattern", "Solid");
		XmlNodeRef NumberFormat = Style->newChild("NumberFormat");
		NumberFormat->setAttr("ss:Format", "#,##0");
	}
	{
		// Style s26
		// Bold/Centered header.
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID", "s26");
		XmlNodeRef StyleFont = Style->newChild("Font");
		StyleFont->setAttr("x:CharSet", "204");
		StyleFont->setAttr("x:Family", "Swiss");
		StyleFont->setAttr("ss:Bold", "1");
		XmlNodeRef StyleInterior = Style->newChild("Interior");
		StyleInterior->setAttr("ss:Color", "#FFFF99");
		StyleInterior->setAttr("ss:Pattern", "Solid");
		XmlNodeRef Alignment = Style->newChild("Alignment");
		Alignment->setAttr("ss:Horizontal", "Center");
		Alignment->setAttr("ss:Vertical", "Bottom");
	}
	{
		// Style s20
		// Centered
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID", "s20");
		XmlNodeRef Alignment = Style->newChild("Alignment");
		Alignment->setAttr("ss:Horizontal", "Center");
		Alignment->setAttr("ss:Vertical", "Bottom");
	}
	{
		// Style s21
		// Bold
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID", "s21");
		XmlNodeRef StyleFont = Style->newChild("Font");
		StyleFont->setAttr("x:CharSet", "204");
		StyleFont->setAttr("x:Family", "Swiss");
		StyleFont->setAttr("ss:Bold", "1");
	}
	{
		// Style s22
		// Centered, Integer Number format
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID", "s22");
		XmlNodeRef Alignment = Style->newChild("Alignment");
		Alignment->setAttr("ss:Horizontal", "Center");
		Alignment->setAttr("ss:Vertical", "Bottom");
		XmlNodeRef NumberFormat = Style->newChild("NumberFormat");
		NumberFormat->setAttr("ss:Format", "#,##0");
	}
	{
		// Style s23
		// Centered, Float Number format
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID", "s23");
		XmlNodeRef Alignment = Style->newChild("Alignment");
		Alignment->setAttr("ss:Horizontal", "Center");
		Alignment->setAttr("ss:Vertical", "Bottom");
		//XmlNodeRef NumberFormat = Style->newChild( "NumberFormat" );
		//NumberFormat->setAttr( "ss:Format","#,##0" );
	}

	/*
	    <Style ss:ID="s25">
	    <Font x:CharSet="204" x:Family="Swiss" ss:Bold="1"/>
	    <Interior ss:Color="#FFFF99" ss:Pattern="Solid"/>
	    </Style>
	 */
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CExcelExportBase::NewWorksheet(const char* name)
{
	m_CurrWorksheet = m_Workbook->newChild("Worksheet");
	m_CurrWorksheet->setAttr("ss:Name", name);
	m_CurrTable = m_CurrWorksheet->newChild("Table");
	return m_CurrWorksheet;
}

//////////////////////////////////////////////////////////////////////////
void CExcelExportBase::AddRow()
{
	m_CurrRow = m_CurrTable->newChild("Row");
}

//////////////////////////////////////////////////////////////////////////
void CExcelExportBase::AddCell_SumOfRows(int nRows)
{
	XmlNodeRef cell = m_CurrRow->newChild("Cell");
	XmlNodeRef data = cell->newChild("Data");
	data->setAttr("ss:Type", "Number");
	data->setContent("0");
	m_CurrCell = cell;

	if (nRows > 0)
	{
		char buf[128];
		cry_sprintf(buf, "=SUM(R[-%d]C:R[-1]C)", nRows);
		m_CurrCell->setAttr("ss:Formula", buf);
	}
}

//////////////////////////////////////////////////////////////////////////
void CExcelExportBase::AddCell(float number)
{
	XmlNodeRef cell = m_CurrRow->newChild("Cell");
	cell->setAttr("ss:StyleID", "s23"); // Centered
	XmlNodeRef data = cell->newChild("Data");
	data->setAttr("ss:Type", "Number");
	char str[128];
	cry_sprintf(str, "%.3f", number);
	data->setContent(str);
	m_CurrCell = cell;
}

//////////////////////////////////////////////////////////////////////////
void CExcelExportBase::AddCell(int number)
{
	XmlNodeRef cell = m_CurrRow->newChild("Cell");
	cell->setAttr("ss:StyleID", "s22"); // Centered
	XmlNodeRef data = cell->newChild("Data");
	data->setAttr("ss:Type", "Number");
	char str[128];
	cry_sprintf(str, "%d", number);
	data->setContent(str);
	m_CurrCell = cell;
}

//////////////////////////////////////////////////////////////////////////
void CExcelExportBase::AddCell(uint32 number)
{
	XmlNodeRef cell = m_CurrRow->newChild("Cell");
	cell->setAttr("ss:StyleID", "s22"); // Centered
	XmlNodeRef data = cell->newChild("Data");
	data->setAttr("ss:Type", "Number");
	char str[128];
	cry_sprintf(str, "%u", number);
	data->setContent(str);
	m_CurrCell = cell;
}

//////////////////////////////////////////////////////////////////////////
void CExcelExportBase::AddCell(const char* str, int nFlags)
{
	XmlNodeRef cell = m_CurrRow->newChild("Cell");
	XmlNodeRef data = cell->newChild("Data");
	data->setAttr("ss:Type", "String");
	data->setContent(str);
	SetCellFlags(cell, nFlags);
	m_CurrCell = cell;
}

//////////////////////////////////////////////////////////////////////////
void CExcelExportBase::AddCellAtIndex(int nIndex, const char* str, int nFlags)
{
	XmlNodeRef cell = m_CurrRow->newChild("Cell");
	cell->setAttr("ss:Index", nIndex);
	XmlNodeRef data = cell->newChild("Data");
	data->setAttr("ss:Type", "String");
	data->setContent(str);
	SetCellFlags(cell, nFlags);
	m_CurrCell = cell;
}

//////////////////////////////////////////////////////////////////////////
void CExcelExportBase::SetCellFlags(XmlNodeRef cell, int flags)
{
	if (flags & CELL_BOLD)
	{
		if (flags & CELL_CENTERED)
			cell->setAttr("ss:StyleID", "s26");
		else
			cell->setAttr("ss:StyleID", "s21");
	}
	else
	{
		if (flags & CELL_CENTERED)
			cell->setAttr("ss:StyleID", "s20");
	}
}

//////////////////////////////////////////////////////////////////////////
bool CExcelExportBase::SaveToFile(const char* filename)
{
	string xml = GetXmlHeader();

	string sDir = PathUtil::GetParentDirectory(filename);
	gEnv->pCryPak->MakeDir(sDir.c_str());

	FILE* file = fxopen(filename, "wb");
	if (file)
	{
		fprintf(file, "%s", xml.c_str());
		m_Workbook->saveToFile(filename, 8 * 1024 /*chunksize*/, file);
		fclose(file);
		return true;
	}
	return false;
}
