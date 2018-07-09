// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

#pragma once
#ifndef XMLCPB_STRING_TABLE_WRITER_H
	#define XMLCPB_STRING_TABLE_WRITER_H

	#include "../XMLCPB_Common.h"
	#include "XMLCPB_BufferWriter.h"
	#include <CryCore/StlUtils.h>

namespace XMLCPB {

class CWriter;

class CStringTableWriter
{
public:

	CStringTableWriter(int maxNumStrings, int bufferSize);
	~CStringTableWriter();
	void        Init(CWriter* pWriter);

	StringID    GetStringID(const char* pString, bool addIfCantFind = true);
	void        LogStrings();
	int         GetNumStrings() const { return (int)m_stringAddrs.size(); }
	void        FillFileHeaderInfo(SFileHeader::SStringTable& info);
	void        WriteToFile();
	void        WriteToMemory(uint8*& rpData, uint32& outWriteLoc);
	uint32      GetDataSize();
	const char* GetString(const StringID stringId) const { const SAddr* addr; return GetStringRealPointer(stringId, addr); }
	void        CreateStringsFromConstants();

	#ifdef XMLCPB_CHECK_HARDCODED_LIMITS
	const char* GetStringSafe(StringID stringId) const;
	#endif

private:

	struct SAddr : CBufferWriter::SAddr
	{
		size_t hash;
	};

	CStringTableWriter&       operator=(const CStringTableWriter& other); // just to prevent assignments and copy constructors
	CStringTableWriter(const CStringTableWriter&);                        //
	const CStringTableWriter& GetThis() const { return *this; }

	static const StringID SEARCHING_ID;

	struct StringHasher
	{
		const CStringTableWriter& m_owner;

		StringHasher(const CStringTableWriter& owner) : m_owner(owner) {}

		ILINE static size_t HashString(const char* __restrict str)
		{
			size_t hash = 5381;
			size_t c;
			while (c = *str++)
				hash = ((hash << 5) + hash) + c;   /* hash * 33 + c */
			return hash;
		}

		size_t operator()(const StringID& _Keyval) const
		{
			return _Keyval == CStringTableWriter::SEARCHING_ID ? m_owner.m_checkAddr.hash : m_owner.m_stringAddrs[_Keyval].hash;
		}

		bool operator()(const StringID& _Keyval1, const StringID& _Keyval2) const
		{
			const SAddr* addr0, * addr1;
			const char* __restrict str0 = m_owner.GetStringRealPointer(_Keyval1, addr0);
			const char* __restrict str1 = m_owner.GetStringRealPointer(_Keyval2, addr1);
			if (addr0->hash != addr1->hash)
				return false;
			for (; * str0 && * str0 == * str1; ++str0, ++str1) {}
			return *str0 == *str1;
		}
	};

private:

	ILINE const char* GetStringRealPointer(StringID ID, const SAddr*& addr) const
	{
		assert(ID != XMLCPB_INVALID_ID);

		if (ID == SEARCHING_ID)
		{
			addr = &m_checkAddr;
			return m_pCheckString;
		}

		addr = &m_stringAddrs[ID];
		return (const char*)m_buffer.GetPointerFromAddr(*addr);
	}

	bool HasStringID(const char* pString) const
	{
		//Prefer HasStringID() for use in asserts to avoid cppcheck believeing GetStringID() is causing unwanted side effects
		CStringTableWriter* self = const_cast<CStringTableWriter*> (this);
		return self->GetStringID(pString, false) != XMLCPB_INVALID_ID;
	}

	StringID AddString(const char* pString, size_t hash);

	typedef std::vector<FlatAddr> FlatAddrVec;
	void CalculateFlatAddrs(FlatAddrVec& outFlatAddrs);

private:

	CBufferWriter      m_buffer;                  // holds all strings, including the end '0's.
	std::vector<SAddr> m_stringAddrs;             // pointers into m_buffer
	typedef std::unordered_set<StringID, StringHasher, StringHasher, AttrStringAllocator<StringID>> StringHasherType;
	StringHasherType   m_sortedIndexes;
	CWriter*           m_pWriter;
	int                m_maxNumStrings;                  // used for checks. we need to control it because in the final data we use a limited amount of bits to store the ID. This amount of bits can be diferent for diferent types of string (tags, attrnames, strdata)
	const char*        m_pCheckString;                   // only used when comparing strings inside the GetStringID function. it should be NULL otherwise
	SAddr              m_checkAddr;                      // ditto, used for the hash
};

}  // end namespace

#endif
