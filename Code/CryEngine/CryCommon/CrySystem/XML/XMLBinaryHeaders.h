// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Created by: Michael Smith
//---------------------------------------------------------------------------
#ifndef __XMLBINARYHEADERS_H__
#define __XMLBINARYHEADERS_H__

namespace XMLBinary
{
class IDataWriter
{
public:
	virtual ~IDataWriter() {}
	virtual void Write(const void* pData, size_t size) = 0;
};

class IFilter
{
public:
	enum EType
	{
		eType_ElementName,
		eType_AttributeName
	};
	virtual ~IFilter() {}
	virtual bool IsAccepted(EType type, const char* pName) const = 0;
};

//////////////////////////////////////////////////////////////////////////
typedef uint32 NodeIndex;              //!< Only uint32 or uint16 are supported.

template<int size> struct Pad
{
	char pad[size];
};
template<> struct Pad<0> {};

struct Node
{
	uint32    nTagStringOffset;        //!< Offset in CBinaryXmlData::pStringData.
	uint32    nContentStringOffset;    //!< Offset in CBinaryXmlData::pStringData.
	uint16    nAttributeCount;
	uint16    nChildCount;
	NodeIndex nParentIndex;
	NodeIndex nFirstAttributeIndex;
	NodeIndex nFirstChildIndex;
	Pad<sizeof(uint32) - sizeof(NodeIndex)> reserved_for_alignment;
};

struct Attribute
{
	uint32 nKeyStringOffset;           //!< Offset in CBinaryXmlData::pStringData.
	uint32 nValueStringOffset;         //!< Offset in CBinaryXmlData::pStringData.
};

struct BinaryFileHeader
{
	char   szSignature[8];
	uint32 nXMLSize;
	uint32 nNodeTablePosition;
	uint32 nNodeCount;
	uint32 nAttributeTablePosition;
	uint32 nAttributeCount;
	uint32 nChildTablePosition;
	uint32 nChildCount;
	uint32 nStringDataPosition;
	uint32 nStringDataSize;
};
}

#endif //__XMLBINARYHEADERS_H__
