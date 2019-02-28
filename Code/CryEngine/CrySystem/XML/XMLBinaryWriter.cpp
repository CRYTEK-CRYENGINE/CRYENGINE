// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   XMLBinaryWriter.cpp
//  Created:     21/04/2006 by Michael Smith.
//  Description:
// -------------------------------------------------------------------------
//  History:
//     8/01/2008 - Modified by Timur
//     3/12/2014 - Modified by Sergey
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "XMLBinaryWriter.h"
#include <CryCore/CryEndian.h>
#include <string.h>  // memcpy()

//////////////////////////////////////////////////////////////////////////
namespace XMLBinary
{
void SwapEndianness_Node(Node& t)
{
	SwapEndian(t.nTagStringOffset, true);
	SwapEndian(t.nContentStringOffset, true);
	SwapEndian(t.nAttributeCount, true);
	SwapEndian(t.nChildCount, true);
	SwapEndian(t.nParentIndex, true);
	SwapEndian(t.nFirstAttributeIndex, true);
	SwapEndian(t.nFirstChildIndex, true);
}

void SwapEndianness_Attribute(Attribute& t)
{
	SwapEndian(t.nKeyStringOffset, true);
	SwapEndian(t.nValueStringOffset, true);
}

void SwapEndianness_Header(BinaryFileHeader& t)
{
	SwapEndian(t.nXMLSize, true);
	SwapEndian(t.nNodeTablePosition, true);
	SwapEndian(t.nNodeCount, true);
	SwapEndian(t.nAttributeTablePosition, true);
	SwapEndian(t.nAttributeCount, true);
	SwapEndian(t.nChildTablePosition, true);
	SwapEndian(t.nChildCount, true);
	SwapEndian(t.nStringDataPosition, true);
	SwapEndian(t.nStringDataSize, true);
}
}

//////////////////////////////////////////////////////////////////////////
XMLBinary::CXMLBinaryWriter::CXMLBinaryWriter()
{
	m_nStringDataSize = 0;
}

static void align(size_t& nPosition, const size_t nAlignment)
{
	const size_t nPadSize = ((nPosition + (nAlignment - 1)) & ~(nAlignment - 1)) - nPosition;
	nPosition += nPadSize;
}

static void alignWrite(XMLBinary::IDataWriter* const pFile, size_t& nPosition, const size_t nAlignment)
{
	size_t nPadSize = ((nPosition + (nAlignment - 1)) & ~(nAlignment - 1)) - nPosition;

	if (nPadSize > 0)
	{
		nPosition += nPadSize;

		static const char zeroes[32] = { 0 };

		while (nPadSize > 0)
		{
			const size_t n = (nPadSize <= sizeof(zeroes)) ? nPadSize : sizeof(zeroes);
			nPadSize -= n;
			pFile->Write(zeroes, n);
		}
	}
}

static void write(XMLBinary::IDataWriter* const pFile, size_t& nPosition, const void* const pData, const size_t nDataSize)
{
	pFile->Write(pData, nDataSize);
	nPosition += nDataSize;
}

//////////////////////////////////////////////////////////////////////////
bool XMLBinary::CXMLBinaryWriter::WriteNode(IDataWriter* pFile, XmlNodeRef node, bool bNeedSwapEndian, XMLBinary::IFilter* pFilter, string& error)
{
	error = "";

	// Scan the node tree, building a flat node list, attribute list and string table.
	m_nStringDataSize = 0;

	if (!CompileTables(node, pFilter, error))
	{
		return false;
	}

	static const uint nMaxNodeCount = (NodeIndex) ~0;
	if (m_nodes.size() > nMaxNodeCount)
	{
		error.Format("XMLBinary: Too many nodes: %" PRISIZE_T " (max is %u)", m_nodes.size(), nMaxNodeCount);
		return false;
	}

	// Initialize the file header.
	size_t nTheoreticalPosition = 0;
	static const size_t nAlignment = sizeof(uint32);

	BinaryFileHeader header;
	static const char signature[] = "CryXmlB";
	static_assert(sizeof(signature) == sizeof(header.szSignature), "Wrong signature size!");
	memcpy(header.szSignature, signature, sizeof(header.szSignature));
	nTheoreticalPosition += sizeof(header);
	align(nTheoreticalPosition, nAlignment);

	header.nNodeTablePosition = nTheoreticalPosition;
	header.nNodeCount = int(m_nodes.size());
	nTheoreticalPosition += header.nNodeCount * sizeof(Node);
	align(nTheoreticalPosition, nAlignment);

	header.nChildTablePosition = nTheoreticalPosition;
	header.nChildCount = int(m_childs.size());
	nTheoreticalPosition += header.nChildCount * sizeof(NodeIndex);
	align(nTheoreticalPosition, nAlignment);

	header.nAttributeTablePosition = nTheoreticalPosition;
	header.nAttributeCount = int(m_attributes.size());
	nTheoreticalPosition += header.nAttributeCount * sizeof(Attribute);
	align(nTheoreticalPosition, nAlignment);

	header.nStringDataPosition = nTheoreticalPosition;
	header.nStringDataSize = m_nStringDataSize;
	nTheoreticalPosition += header.nStringDataSize;

	header.nXMLSize = nTheoreticalPosition;

	// Swap endianness of the data structures
	if (bNeedSwapEndian)
	{
		SwapEndianness_Header(header);
		for (size_t i = 0, iCount = m_nodes.size(); i < iCount; ++i)
		{
			SwapEndianness_Node(m_nodes[i]);
		}
		for (size_t i = 0, iCount = m_attributes.size(); i < iCount; ++i)
		{
			SwapEndianness_Attribute(m_attributes[i]);
		}
		for (size_t i = 0, iCount = m_childs.size(); i < iCount; ++i)
		{
			SwapEndian(m_childs[i], true);
		}
	}

	// Write file
	{
		nTheoreticalPosition = 0;

		// Write out the file header.
		write(pFile, nTheoreticalPosition, &header, sizeof(header));
		alignWrite(pFile, nTheoreticalPosition, nAlignment);

		// Write out the node table.
		if (!m_nodes.empty())
		{
			write(pFile, nTheoreticalPosition, &m_nodes[0], sizeof(m_nodes[0]) * m_nodes.size());
			alignWrite(pFile, nTheoreticalPosition, nAlignment);
		}

		// Write out the children table.
		if (!m_childs.empty())
		{
			write(pFile, nTheoreticalPosition, &m_childs[0], sizeof(m_childs[0]) * m_childs.size());
			alignWrite(pFile, nTheoreticalPosition, nAlignment);
		}

		// Write out the attribute table.
		if (!m_attributes.empty())
		{
			write(pFile, nTheoreticalPosition, &m_attributes[0], sizeof(m_attributes[0]) * m_attributes.size());
			alignWrite(pFile, nTheoreticalPosition, nAlignment);
		}

		// Write out the data of all the m_strings.
		for (size_t nString = 0; nString < m_strings.size(); ++nString)
		{
			pFile->Write(m_strings[nString].c_str(), m_strings[nString].size() + 1);
		}
	}

	return true;
}

bool XMLBinary::CXMLBinaryWriter::CompileTables(XmlNodeRef node, XMLBinary::IFilter* pFilter, string& error)
{
	bool ok = CompileTablesForNode(node, -1, pFilter, error);
	ok = ok && CompileChildTable(node, pFilter, error);
	return ok;
}

//////////////////////////////////////////////////////////////////////////
bool XMLBinary::CXMLBinaryWriter::CompileTablesForNode(XmlNodeRef node, int nParentIndex, XMLBinary::IFilter* pFilter, string& error)
{
	// Add the tag to the string table.
	int nTagStringOffset = AddString(node->getTag());

	// Add the content string to the string table.
	int nContentStringOffset = AddString(node->getContent());

	// Add all the attributes to the attributes table.
	const char* szKey;
	const char* szValue;
	const int nFirstAttributeIndex = int(m_attributes.size());
	for (int i = 0, attrCount = node->getNumAttributes(); i < attrCount; ++i)
	{
		if (node->getAttributeByIndex(i, &szKey, &szValue) &&
		    (!pFilter || pFilter->IsAccepted(IFilter::eType_AttributeName, szKey)))
		{
			// Add the key and the value to the string table.
			Attribute attribute;
			attribute.nKeyStringOffset = AddString(szKey);
			attribute.nValueStringOffset = AddString(szValue);

			// Add the attribute to the attribute table.
			m_attributes.push_back(attribute);
		}
	}
	const int nAttributeCount = int(m_attributes.size()) - nFirstAttributeIndex;

	static const int nMaxAttributeCount = (uint16) ~0;
	if (nAttributeCount > nMaxAttributeCount)
	{
		error.Format("XMLBinary: Too many attributes in a node: %d (max is %i)", nAttributeCount, nMaxAttributeCount);
		return false;
	}

	// Add ourselves to the node list.
	const int nIndex = int(m_nodes.size());
	{
		Node nd;
		memset(&nd, 0, sizeof(nd));
		nd.nTagStringOffset = nTagStringOffset;
		nd.nContentStringOffset = nContentStringOffset;
		nd.nParentIndex = nParentIndex;
		nd.nFirstAttributeIndex = nFirstAttributeIndex;
		nd.nAttributeCount = nAttributeCount;

		m_nodes.push_back(nd);
	}

	m_nodesMap.insert(NodesMap::value_type(node, nIndex));

	// Recurse to the child nodes.
	int nChildCount = 0;
	static const int nMaxChildCount = (uint16) ~0;
	for (int nChild = 0, numChilds = node->getChildCount(); nChild < numChilds; ++nChild)
	{
		XmlNodeRef childNode = node->getChild(nChild);
		if (!pFilter || pFilter->IsAccepted(IFilter::eType_ElementName, childNode->getTag()))
		{
			if (++nChildCount > nMaxChildCount)
			{
				error.Format("XMLBinary: Too many children in node '%s': %d (max is %i)", childNode->getTag(), nChildCount, nMaxChildCount);
				return false;
			}
			if (!CompileTablesForNode(childNode, nIndex, pFilter, error))
			{
				return false;
			}
		}
	}

	m_nodes[nIndex].nChildCount = nChildCount;

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool XMLBinary::CXMLBinaryWriter::CompileChildTable(XmlNodeRef node, XMLBinary::IFilter* pFilter, string& error)
{
	const int nIndex = m_nodesMap.find(node)->second; // Assume node always exist in map.
	const int nFirstChildIndex = (int)m_childs.size();

	Node& nd = m_nodes[nIndex];
	nd.nFirstChildIndex = nFirstChildIndex;

	int nChildCount = 0;
	for (int nChild = 0, numChilds = node->getChildCount(); nChild < numChilds; ++nChild)
	{
		XmlNodeRef childNode = node->getChild(nChild);
		if (!pFilter || pFilter->IsAccepted(IFilter::eType_ElementName, childNode->getTag()))
		{
			++nChildCount;
			const int nChildIndex = m_nodesMap.find(childNode)->second; // Assume node always exist in map.
			m_childs.push_back(nChildIndex);
		}
	}
	if (nChildCount != nd.nChildCount)
	{
		error.Format("XMLBinary: Internal error in CompileChildTable()");
		return false;
	}

	// Recurse to the child nodes.
	for (int nChild = 0, numChilds = node->getChildCount(); nChild < numChilds; ++nChild)
	{
		XmlNodeRef childNode = node->getChild(nChild);
		if (!pFilter || pFilter->IsAccepted(IFilter::eType_ElementName, childNode->getTag()))
		{
			if (!CompileChildTable(childNode, pFilter, error))
			{
				return false;
			}
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
int XMLBinary::CXMLBinaryWriter::AddString(const XmlString& sString)
{
	// If we have such string already, then we will re-use its data.
	StringMap::const_iterator itStringEntry = m_stringMap.find(sString);
	if (itStringEntry == m_stringMap.end())
	{
		// We don't have such string yet, so we should add it to the tables.
		m_strings.push_back(sString);
		itStringEntry = m_stringMap.insert(StringMap::value_type(sString, m_nStringDataSize)).first;
		m_nStringDataSize += sString.length() + 1;
	}

	// Return offset of the string in the string data buffer.
	return (*itStringEntry).second;
}
