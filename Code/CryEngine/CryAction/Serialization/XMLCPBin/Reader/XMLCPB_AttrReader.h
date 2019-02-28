// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

#pragma once
#ifndef XMLCPB_ATTRREADER_H
	#define XMLCPB_ATTRREADER_H

	#include "../XMLCPB_Common.h"

namespace XMLCPB {

class CReader;

class CAttrReader
{
	friend class CReader;
public:

	CAttrReader(CReader& Reader);
	CAttrReader(const CAttrReader& other) : m_Reader(other.m_Reader) { *this = other; }
	~CAttrReader() {}

	CAttrReader& operator=(const CAttrReader& other)
	{
		if (this != &other)
		{
			CRY_ASSERT (&m_Reader == &other.m_Reader);
			m_type = other.m_type;
			m_nameId = other.m_nameId;
			m_addr = other.m_addr;
		}
		return *this;
	}

	void          InitFromCompact(FlatAddr offset, uint16 header);

	FlatAddr      GetAddrNextAttr() const;
	const char*   GetName() const;
	eAttrDataType GetBasicDataType() const { return XMLCPB::GetBasicDataType(m_type); }

	void          Get(int32& val) const;
	void          Get(int64& val) const;
	void          Get(uint64& val) const;
	void          Get(int16& val) const;
	void          Get(int8& val) const;
	void          Get(uint8& val) const;
	void          Get(uint16& val) const;
	void          Get(uint& val) const;
	void          Get(bool& val) const;
	void          Get(float& val) const;
	void          Get(Vec2& val) const;
	void          Get(Ang3& val) const;
	void          Get(Vec3& val) const;
	void          Get(Quat& val) const;
	void          Get(const char*& pStr) const;
	void          Get(uint8*& rdata, uint32& outSize) const;

	void          GetValueAsString(string& str) const;

	#ifndef _RELEASE
	StringID GetNameId() const { return m_nameId; }    // debug help
	#endif

private:

	FlatAddr GetDataAddr() const { return m_addr;  }
	float    UnpackFloatInSemiConstType(uint8 mask, uint32 ind, FlatAddr& addr) const;

	template<class T>
	void ValueToString(string& str, const char* formatString) const;

	eAttrDataType m_type;
	StringID      m_nameId;
	FlatAddr      m_addr;
	CReader&      m_Reader;

};

template<class T>
void CAttrReader::ValueToString(string& str, const char* formatString) const
{
	T val;
	Get(val);
	str.Format(formatString, val);
}

} // end namespace
#endif
