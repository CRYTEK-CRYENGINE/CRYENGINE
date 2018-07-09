// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

#include "StdAfx.h"
#include "XMLCPB_StringTableWriter.h"
#include "XMLCPB_Writer.h"
#include "CryActionCVars.h"

using namespace XMLCPB;

const StringID CStringTableWriter::SEARCHING_ID = 0Xfffffffe;

//////////////////////////////////////////////////////////////////////////

CStringTableWriter::CStringTableWriter(int maxNumStrings, int bufferSize)
	: m_buffer(bufferSize)
	, m_sortedIndexes(256, StringHasher(GetThis()), StringHasher(GetThis()))
	, m_pWriter(nullptr)
	, m_maxNumStrings(maxNumStrings)
	, m_pCheckString(nullptr)
{
	m_checkAddr.hash = 0;
}

CStringTableWriter::~CStringTableWriter()
{
#ifdef _DEBUG
	if (CCryActionCVars::Get().g_saveLoadExtendedLog != 0)
	{
		// Check hash function for collisions and examine hash_map bucket sizes
		size_t size = m_sortedIndexes.size();
		size_t bucket_count = m_sortedIndexes.bucket_count();
		size_t max_bucket_count = m_sortedIndexes.max_bucket_count();
		CryLog("CStringTableWriter size: %" PRISIZE_T ", bucket_count: %" PRISIZE_T ", max_bucket_count: %" PRISIZE_T, size, bucket_count, max_bucket_count);
		float load_factor = m_sortedIndexes.load_factor();
		CryLog("load_factor %f", load_factor);
	}
#endif
}

void CStringTableWriter::Init(CWriter* pWriter)
{
	m_pWriter = pWriter;
	m_buffer.Init(pWriter, false);    // we never want a stringTableWriter buffer to be streamed, because we need it along the whole process.
}

void CStringTableWriter::CreateStringsFromConstants()
{
	for (int i = 0; i < DT_NUM_CONST_STR; i++)
	{
		const char* pString = GetConstantString(i);
		StringID id = AddString(pString, StringHasher::HashString(pString));
		assert(id == i);
	}
}

//////////////////////////////////////////////////////////////////////////
// AddString could be used directly instead, with some changes. But then it would be very inefficient because it would need to copy every to-compare string first into the buffer.
StringID CStringTableWriter::GetStringID(const char* pString, bool addIfCantFind)
{
	m_pCheckString = pString;  // the internal std::set comparison code will use this string when trying to find SEARCHING_ID
	m_checkAddr.hash = StringHasher::HashString(pString);
	StringHasherType::const_iterator iter = m_sortedIndexes.find(SEARCHING_ID);
	m_pCheckString = NULL;

	// found the string
	if (iter != m_sortedIndexes.end())
		//return *iter;
		return *iter;

	// didn't find
	if (addIfCantFind)
	{
		StringID ID = AddString(pString, m_checkAddr.hash);
		return ID;
	}
	else
		return XMLCPB_INVALID_ID;
}

//////////////////////////////////////////////////////////////////////////

StringID CStringTableWriter::AddString(const char* pString, size_t hash)
{
	assert(m_stringAddrs.size() < m_maxNumStrings);
	assert(!HasStringID(pString));

	int size = strlen(pString) + 1;

	StringID addStringId = m_stringAddrs.size();
	m_stringAddrs.push_back(SAddr());
	SAddr& stringAddr = m_stringAddrs.back();
	m_buffer.AddDataNoSplit(stringAddr, pString, size);    // no split = can not be internally split in different internal buffers. Because we want to be able to read it in one go with the translated pointer

	stringAddr.hash = hash;

	m_sortedIndexes.insert(addStringId);

	return addStringId;
}

//////////////////////////////////////////////////////////////////////////
// debug and statistics purposes
void CStringTableWriter::LogStrings()
{
	/*	StringHasherType::const_iterator iter = m_sortedIndexes.begin();

	   while (iter!=m_sortedIndexes.end())
	   {
	    const StringID& stringId = *iter;
	    string str;
	    str.Format( "---string: %s  offset: %d\n", GetStringRealPointer(stringId), m_buffer.ConvertSegmentedAddrToFlatAddr( m_stringAddrs[i] ) );
	    gEnv->pCryPak->FWrite( str.c_str(), str.size()+1. pFile )
	   ++iter;
	   }
	 */
	/*	for (uint32 i=0; i<GetNumStrings(); i++)
	   {
	    string str;
	   //		str.Format( "%s %d\n", GetStringRealPointer(i), m_buffer.ConvertSegmentedAddrToFlatAddr( m_stringAddrs[i] ) );
	    str.Format( "%d\n", m_buffer.ConvertSegmentedAddrToFlatAddr( m_stringAddrs[i] ) );
	    gEnv->pCryPak->FWrite( str.c_str(), str.size(), pFile );
	   } */
}

//////////////////////////////////////////////////////////////////////////

void CStringTableWriter::FillFileHeaderInfo(SFileHeader::SStringTable& info)
{
	info.m_numStrings = m_stringAddrs.size();
	info.m_sizeStringData = m_buffer.GetUsedMemory();
}

//////////////////////////////////////////////////////////////////////////

void CStringTableWriter::CalculateFlatAddrs(FlatAddrVec& outFlatAddrs)
{
	outFlatAddrs.resize(m_stringAddrs.size());

	for (int i = 0; i < outFlatAddrs.size(); ++i)
		outFlatAddrs[i] = m_buffer.ConvertSegmentedAddrToFlatAddr(m_stringAddrs[i]);
}

//////////////////////////////////////////////////////////////////////////

void CStringTableWriter::WriteToFile()
{
	if (m_stringAddrs.empty())
		return;

	// TODO: could reuse the same memory space than the segmented addrs. the table is not going to be used after it anyway. that could be about 32k less in writing
	FlatAddrVec flatAddrs;
	CalculateFlatAddrs(flatAddrs);
	for (size_t i = 0, size = flatAddrs.size(); i < size; ++i)
		SwapIntegerValue(flatAddrs[i]);

	assert(!flatAddrs.empty());

	FlatAddr* p = &(flatAddrs[0]);

	m_pWriter->WriteDataIntoFile(p, sizeof(FlatAddr) * flatAddrs.size());
	m_buffer.WriteToFile();
}

//////////////////////////////////////////////////////////////////////////

void CStringTableWriter::WriteToMemory(uint8*& rpData, uint32& outWriteLoc)
{
	if (m_stringAddrs.empty())
		return;

	// TODO: could reuse the same memory space than the segmented addrs. the table is not going to be used after it anyway. that could be about 32k less in writing
	FlatAddrVec flatAddrs;
	CalculateFlatAddrs(flatAddrs);

	assert(!flatAddrs.empty());

	FlatAddr* p = &(flatAddrs[0]);

	m_pWriter->WriteDataIntoMemory(rpData, p, sizeof(FlatAddr) * flatAddrs.size(), outWriteLoc);
	m_buffer.WriteToMemory(rpData, outWriteLoc);
}

//////////////////////////////////////////////////////////////////////////

uint32 CStringTableWriter::GetDataSize()
{
	if (m_stringAddrs.empty())
		return 0;

	uint32 uSize = sizeof(FlatAddr) * GetNumStrings();
	uSize += m_buffer.GetDataSize();

	return uSize;
}

//////////////////////////////////////////////////////////////////////////
#ifdef XMLCPB_CHECK_HARDCODED_LIMITS
const char* CStringTableWriter::GetStringSafe(StringID stringId) const
{
	if (stringId < m_stringAddrs.size())
		return GetString(stringId);
	else
		return "<Invalid String>";
}
#endif
