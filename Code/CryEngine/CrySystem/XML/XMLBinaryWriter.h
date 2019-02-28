// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   XMLBinaryWriter.h
//  Created:     21/04/2006 by Michael Smith.
//  Description:
// -------------------------------------------------------------------------
//  History:
//     8/1/2008 - Modified by Timur
////////////////////////////////////////////////////////////////////////////

#ifndef __XMLBINARYWRITER_H__
#define __XMLBINARYWRITER_H__

#include <CrySystem/XML/IXml.h>
#include <CrySystem/XML/XMLBinaryHeaders.h>
#include <vector>
#include <map>

class IXMLDataSink;

namespace XMLBinary
{
class CXMLBinaryWriter
{
public:
	CXMLBinaryWriter();
	bool WriteNode(IDataWriter* pFile, XmlNodeRef node, bool bNeedSwapEndian, XMLBinary::IFilter* pFilter, string& error);

private:
	bool CompileTables(XmlNodeRef node, XMLBinary::IFilter* pFilter, string& error);

	bool CompileTablesForNode(XmlNodeRef node, int nParentIndex, XMLBinary::IFilter* pFilter, string& error);
	bool CompileChildTable(XmlNodeRef node, XMLBinary::IFilter* pFilter, string& error);
	int  AddString(const XmlString& sString);

private:
	// tables.
	typedef std::map<IXmlNode*, int> NodesMap;
	typedef std::map<string, uint>   StringMap;

	std::vector<Node>      m_nodes;
	NodesMap               m_nodesMap;
	std::vector<Attribute> m_attributes;
	std::vector<NodeIndex> m_childs;
	std::vector<string>    m_strings;
	StringMap              m_stringMap;

	uint                   m_nStringDataSize;
};
}

#endif //__XMLBINARYWRITER_H__
