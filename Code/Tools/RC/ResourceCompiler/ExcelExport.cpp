// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  CryEngine Source File.
//  Copyright (C), Crytek.
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
#include "ICryXML.h"
#include "IXmlSerializer.h"
#include "IRCLog.h"
#include <CryString/CryPath.h>

//////////////////////////////////////////////////////////////////////////
string CExcelExportBase::GetXmlHeader() const
{
	return "<?xml version=\"1.0\"?>\n<?mso-application progid=\"Excel.Sheet\"?>\n";
}

//////////////////////////////////////////////////////////////////////////
void CExcelExportBase::InitExcelWorkbook( XmlNodeRef Workbook )
{
	m_Workbook = Workbook;
	m_Workbook->setTag( "Workbook" );
	m_Workbook->setAttr( "xmlns","urn:schemas-microsoft-com:office:spreadsheet" );
	XmlNodeRef ExcelWorkbook = Workbook->newChild( "ExcelWorkbook" );
	ExcelWorkbook->setAttr( "xmlns","urn:schemas-microsoft-com:office:excel" );

	XmlNodeRef Styles = m_Workbook->newChild("Styles");
	{
		// Style s25
		// Bold header, With Background Color.
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID","s25");
		XmlNodeRef StyleFont = Style->newChild("Font");
		StyleFont->setAttr("x:CharSet","204");
		StyleFont->setAttr("x:Family","Swiss");
		StyleFont->setAttr("ss:Bold","1");
		XmlNodeRef StyleInterior = Style->newChild("Interior");
		StyleInterior->setAttr("ss:Color","#00FF00");
		StyleInterior->setAttr("ss:Pattern","Solid");
		XmlNodeRef NumberFormat = Style->newChild( "NumberFormat" );
		NumberFormat->setAttr( "ss:Format","#,##0" );
	}
	{
		// Style s26
		// Bold/Centered header.
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID","s26");
		XmlNodeRef StyleFont = Style->newChild("Font");
		StyleFont->setAttr("x:CharSet","204");
		StyleFont->setAttr("x:Family","Swiss");
		StyleFont->setAttr("ss:Bold","1");
		XmlNodeRef StyleInterior = Style->newChild("Interior");
		StyleInterior->setAttr("ss:Color","#FFFF99");
		StyleInterior->setAttr("ss:Pattern","Solid");
		XmlNodeRef Alignment = Style->newChild( "Alignment" );
		Alignment->setAttr( "ss:Horizontal","Center" );
		Alignment->setAttr( "ss:Vertical","Bottom" );
	}
	{
		// Style s27
		// Bold Highlighted Cell, With Red Background Color, white Text, Centered.
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID","s27");
		XmlNodeRef StyleFont = Style->newChild("Font");
		StyleFont->setAttr("x:CharSet","204");
		StyleFont->setAttr("x:Family","Swiss");
		StyleFont->setAttr("ss:Bold","1");
		StyleFont->setAttr("ss:Color","#FFFFFF");
		XmlNodeRef StyleInterior = Style->newChild("Interior");
		StyleInterior->setAttr("ss:Color","#FF0000");
		StyleInterior->setAttr("ss:Pattern","Solid");
		XmlNodeRef Alignment = Style->newChild( "Alignment" );
		Alignment->setAttr( "ss:Horizontal","Center" );
		Alignment->setAttr( "ss:Vertical","Bottom" );
	}
	{
		// Style s28
		// Bold Highlighted Cell, With Red Background Color, white Text.
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID","s28");
		XmlNodeRef StyleFont = Style->newChild("Font");
		StyleFont->setAttr("x:CharSet","204");
		StyleFont->setAttr("x:Family","Swiss");
		StyleFont->setAttr("ss:Bold","1");
		StyleFont->setAttr("ss:Color","#FFFFFF");
		XmlNodeRef StyleInterior = Style->newChild("Interior");
		StyleInterior->setAttr("ss:Color","#FF0000");
		StyleInterior->setAttr("ss:Pattern","Solid");
	}
	{
		// Style s20
		// Centered
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID","s20");
		XmlNodeRef Alignment = Style->newChild( "Alignment" );
		Alignment->setAttr( "ss:Horizontal","Center" );
		Alignment->setAttr( "ss:Vertical","Bottom" );
	}
	{
		// Style s21
		// Bold
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID","s21");
		XmlNodeRef StyleFont = Style->newChild("Font");
		StyleFont->setAttr("x:CharSet","204");
		StyleFont->setAttr("x:Family","Swiss");
		StyleFont->setAttr("ss:Bold","1");
	}
	{
		// Style s22
		// Centered, Integer Number format
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID","s22");
		XmlNodeRef Alignment = Style->newChild( "Alignment" );
		Alignment->setAttr( "ss:Horizontal","Center" );
		Alignment->setAttr( "ss:Vertical","Bottom" );
		XmlNodeRef NumberFormat = Style->newChild( "NumberFormat" );
		NumberFormat->setAttr( "ss:Format","#,##0" );
	}
	{
		// Style s23
		// Centered, Float Number format
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID","s23");
		XmlNodeRef Alignment = Style->newChild( "Alignment" );
		Alignment->setAttr( "ss:Horizontal","Center" );
		Alignment->setAttr( "ss:Vertical","Bottom" );
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
XmlNodeRef CExcelExportBase::NewWorksheet( const char *name )
{
	m_CurrWorksheet = m_Workbook->newChild( "Worksheet" );
	m_CurrWorksheet->setAttr( "ss:Name",name );
	m_CurrTable = m_CurrWorksheet->newChild( "Table" );
	return m_CurrWorksheet;
}

//////////////////////////////////////////////////////////////////////////
void CExcelExportBase::AddRow()
{
	m_CurrRow = m_CurrTable->newChild( "Row" );
}

//////////////////////////////////////////////////////////////////////////
void CExcelExportBase::AddCell_SumOfRows( int nRows )
{
	XmlNodeRef cell = m_CurrRow->newChild("Cell");
	XmlNodeRef data = cell->newChild( "Data" );
	data->setAttr( "ss:Type","Number" );
	data->setContent( "0" );
	m_CurrCell = cell;

	if (nRows > 0)
	{
		char buf[128];
		sprintf_s( buf,"=SUM(R[-%d]C:R[-1]C)",nRows );
		m_CurrCell->setAttr( "ss:Formula",buf );
	}
}

//////////////////////////////////////////////////////////////////////////
void CExcelExportBase::AddCell( float number )
{
	XmlNodeRef cell = m_CurrRow->newChild("Cell");
	cell->setAttr("ss:StyleID","s23"); // Centered
	XmlNodeRef data = cell->newChild( "Data" );
	data->setAttr( "ss:Type","Number" );
	char str[128];
	sprintf_s( str,"%.3f",number );
	data->setContent( str );
	m_CurrCell = cell;
}

//////////////////////////////////////////////////////////////////////////
void CExcelExportBase::AddCell( int number )
{
	XmlNodeRef cell = m_CurrRow->newChild("Cell");
	cell->setAttr("ss:StyleID","s22"); // Centered
	XmlNodeRef data = cell->newChild( "Data" );
	data->setAttr( "ss:Type","Number" );
	char str[128];
	sprintf_s( str,"%d",number );
	data->setContent( str );
	m_CurrCell = cell;
}

//////////////////////////////////////////////////////////////////////////
void CExcelExportBase::AddCell( uint32 number )
{
	XmlNodeRef cell = m_CurrRow->newChild("Cell");
	cell->setAttr("ss:StyleID","s22"); // Centered
	XmlNodeRef data = cell->newChild( "Data" );
	data->setAttr( "ss:Type","Number" );
	char str[128];
	sprintf_s( str,"%u",number );
	data->setContent( str );
	m_CurrCell = cell;
}

//////////////////////////////////////////////////////////////////////////
void CExcelExportBase::AddCell( const char *str,int nFlags )
{
	XmlNodeRef cell = m_CurrRow->newChild("Cell");
	XmlNodeRef data = cell->newChild( "Data" );
	data->setAttr( "ss:Type","String" );
	data->setContent( str );
	SetCellFlags( cell,nFlags );
	m_CurrCell = cell;
}

//////////////////////////////////////////////////////////////////////////
void CExcelExportBase::AddCellAtIndex( int nIndex,const char *str,int nFlags )
{
	XmlNodeRef cell = m_CurrRow->newChild("Cell");
	cell->setAttr( "ss:Index",nIndex );
	XmlNodeRef data = cell->newChild( "Data" );
	data->setAttr( "ss:Type","String" );
	data->setContent( str );
	SetCellFlags( cell,nFlags );
	m_CurrCell = cell;
}

//////////////////////////////////////////////////////////////////////////
void CExcelExportBase::SetCellFlags( XmlNodeRef cell,int flags )
{
	if (flags & CELL_BOLD)
	{
		if (flags & CELL_CENTERED)
			cell->setAttr("ss:StyleID","s26");
		else
			cell->setAttr("ss:StyleID","s21");
	}
	else if (flags & CELL_CENTERED)
	{
		cell->setAttr("ss:StyleID","s20");
	}
	else if (flags & CELL_HIGHLIGHT)
	{
		cell->setAttr("ss:StyleID","s27");
	}
}

//////////////////////////////////////////////////////////////////////////
bool CExcelExportBase::SaveToFile( const char *filename ) const
{
	string xml = m_Workbook->getXML();
	string header = GetXmlHeader();

	FILE *file = fopen( filename,"wb" );
	if (file)
	{
		fprintf( file,"%s",header.c_str() );
		fprintf( file,"%s",xml.c_str() );
		fclose(file);
		return true;
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
static ICryXML* LoadICryXML()
{
	HMODULE hXMLLibrary = LoadLibrary("CryXML.dll");
	if (NULL == hXMLLibrary)
	{
		RCLogError("Unable to load xml library (CryXML.dll)");
		return 0;
	}
	FnGetICryXML pfnGetICryXML = (FnGetICryXML)GetProcAddress(hXMLLibrary, "GetICryXML");
	if (pfnGetICryXML == 0)
	{
		RCLogError("Unable to load xml library (CryXML.dll) - cannot find exported function GetICryXML().");
		return 0;
	}
	return pfnGetICryXML();
}


//////////////////////////////////////////////////////////////////////////
XmlNodeRef CExcelExportBase::NewWorkbook()
{
	XmlNodeRef Workbook = LoadICryXML()->GetXMLSerializer()->CreateNode("Workbook");
	InitExcelWorkbook(Workbook);
	return Workbook;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CExcelExportBase::AddColumn( const char *name,int nWidth )
{
	XmlNodeRef Column = m_CurrTable->newChild("Column");
	Column->setAttr( "ss:Width",nWidth );

	m_columns.push_back(name);

	return Column;
}

//////////////////////////////////////////////////////////////////////////
void CExcelExportBase::BeginColumns()
{
	m_columns.clear();
}

//////////////////////////////////////////////////////////////////////////
void CExcelExportBase::EndColumns()
{
	AddRow();
	m_CurrRow->setAttr( "ss:StyleID","s25" );
	for (int i = 0; i < m_columns.size(); i++)
	{
		AddCell( m_columns[i] );
	}
}

//////////////////////////////////////////////////////////////////////////
void CExcelExportBase::FreezeFirstRow()
{
	XmlNodeRef options = m_CurrWorksheet->newChild( "WorksheetOptions" );
	options->setAttr( "xmlns","urn:schemas-microsoft-com:office:excel" );
	options->newChild( "FreezePanes" );
	options->newChild( "FrozenNoSplit" );
	options->newChild( "SplitHorizontal" )->setContent( "1" );
	options->newChild( "TopRowBottomPane" )->setContent( "1" );
	options->newChild( "ActivePane" )->setContent( "2" );
}

//////////////////////////////////////////////////////////////////////////
void CExcelExportBase::AutoFilter( int nRow,int nNumColumns )
{
	XmlNodeRef options = m_CurrWorksheet->newChild( "AutoFilter" );
	options->setAttr( "xmlns","urn:schemas-microsoft-com:office:excel" );
	string range;
	range.Format( "R%dC1:R%dC%d",nRow,nRow,nNumColumns );
	options->setAttr( "x:Range",range ); // x:Range="R1C1:R1C8"
}
