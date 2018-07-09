// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

#include "StdAfx.h"
#include "XMLCPB_AttrWriter.h"
#include "XMLCPB_Writer.h"

#define CHECK_FOR_UNINITIALIZED_DATA 0

using namespace XMLCPB;

#ifdef XMLCPB_COLLECT_STATS
CAttrWriter::SStatistics CAttrWriter::m_statistics;
#endif

#if CHECK_FOR_UNINITIALIZED_DATA
bool IsInitialized(uint32 val)
{
	return (val != 0xCDCDCDCD) && (val != 0xCCCCCCCC) && (val != 0xBAADF00D);
}

bool IsInitialized(char val)
{
	return (val != 0xCD) && (val != 0xCC);
}

template<typename T>
bool IsInitialized(T val)
{
	return IsInitialized(*reinterpret_cast<uint32*>(&val));
}

	#define ASSERT_INITIALIZED(cond, name) CRY_ASSERT_TRACE(IsInitialized(cond), ("Saving uninitialized variable %s", name))
#else
	#define ASSERT_INITIALIZED(cond, name)
#endif

// Note: Without NO_INLINE gcc and clang would infer the input type range
//       and statically optimize away the condition
//       (e.g. for a bool value b, IsInRange(b, 0, 2) would be evaluated
//       as true at compile-time)
NO_INLINE bool IsInRange(int value, int lower, int upper)
{
	return (unsigned int)(value - lower) < upper - lower;
}

//////////////////////////////////////////////////////////////////////////

CAttrWriter::CAttrWriter(CWriter& Writer)
	: m_type(DT_INVALID)
	, m_Writer(Writer)
{
}

//////////////////////////////////////////////////////////////////////////

void CAttrWriter::SetName(const char* pName)
{
	m_nameID = m_Writer.GetAttrNamesTable().GetStringID(pName);
}

//////////////////////////////////////////////////////////////////////////

void CAttrWriter::Set(const char* pName, const char* pDataString)
{
	assert(pDataString);

	int size = strlen(pDataString);
	if (size > MAX_SIZE_FOR_A_DATASTRING)
	{
		Set(pName, (const uint8*)pDataString, size + 1, true); // store as raw data instead of pooled string  (+1 to include the ending 0). Uses immediateCopy to mimic the behavior of the normal strings, which are not guaranteed to be permanent (they can be local)
		return;
	}

	ASSERT_INITIALIZED(pDataString[0], pName);
	SetName(pName);

	StringID constantID = m_Writer.GetStrDataConstantsTable().GetStringID(pDataString, false);

	if (constantID == XMLCPB_INVALID_ID)
	{
		m_type = DT_STR;
		m_data.m_dataStringID = m_Writer.GetStrDataTable().GetStringID(pDataString);
	}
	else
	{
		m_type = eAttrDataType(DT_FIRST_CONST_STR + constantID);
	}
}

//////////////////////////////////////////////////////////////////////////

void CAttrWriter::Set(const char* pAttrName, int64 val)
{
	Set(pAttrName, (uint64)val);
}

//////////////////////////////////////////////////////////////////////////

void CAttrWriter::Set(const char* pAttrName, uint64 val)
{
	ASSERT_INITIALIZED(val, pAttrName);
	SetName(pAttrName);

	m_type = DT_INT64;
	m_data.m_uint64 = val;
}

//////////////////////////////////////////////////////////////////////////

void CAttrWriter::Set(const char* pAttrName, int val)
{
	ASSERT_INITIALIZED(val, pAttrName);
	SetName(pAttrName);

	if (val >= 0 && val <= 10)
	{
		m_type = eAttrDataType(DT_0 + val);
		return;
	}
	else if (val == 255)
	{
		m_type = DT_255;
		return;
	}
	else if (val > 0)
	{
		if (val < 256)
		{
			m_data.m_uint = val;
			m_type = DT_POS8;
			return;
		}
		if (val < 65536)
		{
			m_data.m_uint = val;
			m_type = DT_POS16;
			return;
		}
	}
	else
	{
		if (val > -256)
		{
			m_data.m_uint = -val;
			m_type = DT_NEG8;
			return;
		}
		if (val > -65536)
		{
			m_data.m_uint = -val;
			m_type = DT_NEG16;
			return;
		}
	}

	m_data.m_uint = val;
	m_type = DT_INT32;
}

void CAttrWriter::Set(const char* pAttrName, uint val)
{
	Set(pAttrName, int(val));
}

void CAttrWriter::Set(const char* pAttrName, bool val)
{
	assert(IsInRange((int)val, 0, 2));

	SetName(pAttrName);
	m_type = eAttrDataType(DT_0 + val);
}

//////////////////////////////////////////////////////////////////////////

void CAttrWriter::Set(const char* pAttrName, float val)
{
	ASSERT_INITIALIZED(val, pAttrName);
	if (val == 0)
	{
		SetName(pAttrName);
		m_type = DT_0;
		return;
	}

	if (fabsf(val) < 65536.0f) // fcmp but avoids FPE when val==MAX_FLOAT and abs(MIN_INT) problem if <-4Mb
	{
		float val10 = val * 10.0f;
		float intPart = floorf(val10);
		if (val10 == intPart)
		{
			int vali = (int)intPart; // LHS
			int avali = abs(vali);
			if ((vali % 10) == 0)
			{
				Set(pAttrName, vali / 10);
				return;
			}
			else if (avali < 128)
			{
				SetName(pAttrName);
				m_data.m_uint = int(vali);
				m_type = DT_F1_1DEC;
				return;
			}
		}
	}

	SetName(pAttrName);
	m_data.m_float.v0 = val;
	m_type = DT_F1;
}

void CAttrWriter::Set(const char* pAttrName, const Vec2& _val)
{
	Vec3 val(_val.x, _val.y, 0);
	Set(pAttrName, val);
}

void CAttrWriter::Set(const char* pAttrName, const Ang3& _val)
{
	Vec3 val(_val.x, _val.y, _val.z);
	Set(pAttrName, val);
}

void CAttrWriter::Set(const char* pAttrName, const Vec3& val)
{
	ASSERT_INITIALIZED(val.x, pAttrName);
	ASSERT_INITIALIZED(val.y, pAttrName);
	ASSERT_INITIALIZED(val.z, pAttrName);
	SetName(pAttrName);
	// TODO: this sequence of if's could be optimized, but it would get harder to understand. check if it is worth it.
	if (val.x == 0 && val.y == 0 && val.z == 0)
	{
		m_type = DT_0;
		return;
	}

	if (val.x == 1 && val.y == 0 && val.z == 0)
	{
		m_type = DT_F3_100;
		return;
	}

	if (val.x == 0 && val.y == 1 && val.z == 0)
	{
		m_type = DT_F3_010;
		return;
	}

	if (val.x == 0 && val.y == 0 && val.z == 1)
	{
		m_type = DT_F3_001;
		return;
	}

	{
		uint32 numVals = 0;
		m_data.m_floatSemiConstant.constMask = 0;
		PackFloatInSemiConstType(val.x, 0, numVals);
		PackFloatInSemiConstType(val.y, 1, numVals);
		PackFloatInSemiConstType(val.z, 2, numVals);
		if (numVals == 2)
		{
			m_type = DT_F3_1CONST;
			return;
		}
		if (numVals == 1)
		{
			m_type = DT_F3_2CONST;
			return;
		}
		if (numVals == 0)
		{
			m_type = DT_F3_3CONST;
			return;
		}
	}

	m_type = DT_F3;
}

void CAttrWriter::Set(const char* pAttrName, const Quat& val)
{
	ASSERT_INITIALIZED(val.v.x, pAttrName);
	ASSERT_INITIALIZED(val.v.y, pAttrName);
	ASSERT_INITIALIZED(val.v.z, pAttrName);
	ASSERT_INITIALIZED(val.w, pAttrName);

	SetName(pAttrName);
	if (val.v.x == 0 && val.v.y == 0 && val.v.z == 0 && val.w == 0)
	{
		m_type = DT_0;
		return;
	}

	{
		uint32 numVals = 0;
		m_data.m_floatSemiConstant.constMask = 0;
		PackFloatInSemiConstType(val.v.x, 0, numVals);
		PackFloatInSemiConstType(val.v.y, 1, numVals);
		PackFloatInSemiConstType(val.v.z, 2, numVals);
		PackFloatInSemiConstType(val.w, 3, numVals);
		if (numVals == 3)
		{
			m_type = DT_F4_1CONST;
			return;
		}
		if (numVals == 2)
		{
			m_type = DT_F4_2CONST;
			return;
		}
		if (numVals == 1)
		{
			m_type = DT_F4_3CONST;
			return;
		}
		if (numVals == 0)
		{
			m_type = DT_F4_4CONST;
			return;
		}
	}

	m_type = DT_F4;
}

void CAttrWriter::Set(const char* pAttrName, const uint8* data, uint32 len, bool needInmediateCopy)
{
	assert(data && len > 0);
	ASSERT_INITIALIZED(data[0], pAttrName);

	SetName(pAttrName);

	m_data.m_rawData.size = len;
	m_type = DT_RAWDATA;

	if (!needInmediateCopy)
	{
		m_data.m_rawData.data = data;
		m_data.m_rawData.allocatedData = NULL;
	}
	else
	{
		m_data.m_rawData.allocatedData = new uint8[m_data.m_rawData.size];
		memcpy(m_data.m_rawData.allocatedData, data, m_data.m_rawData.size);
		m_data.m_rawData.data = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////
// in a "multi float semi constant type" there is first a byte, separated in 4 2-bit-fields. Each field corresponds to one of the multifloat components, and it indicates if that
// if that float is a constant (and which constant) or if is a random value. If is a random value, it is stored as a full float.
// this function, called once for every member of the multifloat struct, builds the byte header and store the floats in the appropriate positions.

void CAttrWriter::PackFloatInSemiConstType(float val, uint32 ind, uint32& numVals)
{
	uint32 type = PFSC_VAL;

	if (val == 0.0f)
		type = PFSC_0;
	else if (val == 1.0f)
		type = PFSC_1;
	else if (val == -1.0f)
		type = PFSC_N1;
	else
	{
		assert(numVals < 4);
		m_data.m_floatSemiConstant.v[numVals] = val;
		++numVals;
	}

	type = type << (ind * 2);
	m_data.m_floatSemiConstant.constMask |= type;
}

//////////////////////////////////////////////////////////////////////////
// header refers to the 16 bits value that holds the type+tag

uint16 CAttrWriter::CalcHeader() const
{
	assert(m_type != DT_INVALID);
	assert(m_nameID <= MAX_NAMEID);
	uint16 header = (m_nameID << BITS_TYPEID) | m_type;
	return header;
}

//////////////////////////////////////////////////////////////////////////
// compacting is the process of converting the node information into final binary data, and store it into the writer main buffer.
// nameID : 10 bits  (1023 id max)
// typeID : 6 bits  (63 max )

void CAttrWriter::Compact()
{
	static_assert(uint32(DT_NUMTYPES) <= uint32(MAX_NUM_TYPES), "Too many types!");
	static_assert(BITS_TYPEID + BITS_NAMEID <= 16, "Wrong bit positions!");

#ifdef XMLCPB_COLLECT_STATS
	if (m_Writer.IsSavingIntoFile())
		AddDataToStatistics();
#endif

	if (GetDataTypeSize(m_type) == 0)
		return;

	CBufferWriter& buffer = m_Writer.GetMainBuffer();

	// data
	switch (m_type)
	{
	case DT_STR:
		{
			assert(m_data.m_dataStringID < MAX_NUM_STRDATA);
			uint16 dataStringID = m_data.m_dataStringID;
			buffer.AddDataEndianAware(dataStringID);
			break;
		}

	case DT_INT32:
		{
			buffer.AddDataEndianAware(m_data.m_uint);
			break;
		}

	case DT_INT64:
		{
			buffer.AddDataEndianAware(m_data.m_uint64);
			break;
		}

	case DT_POS16:
	case DT_NEG16:
		{
			uint16 val = m_data.m_uint;
			buffer.AddDataEndianAware(val);
			break;
		}

	case DT_POS8:
	case DT_NEG8:
		{
			uint8 val = m_data.m_uint;
			buffer.AddData(&val, sizeof(val));
			break;
		}

	case DT_F1:
		{
			buffer.AddData(&m_data.m_float.v0, sizeof(m_data.m_float.v0));
			break;
		}

	case DT_F1_1DEC:
		{
			uint8 val = m_data.m_uint;
			buffer.AddData(&val, sizeof(val));
			break;
		}

	case DT_F3:
		{
			buffer.AddData(&m_data.m_float.v0, sizeof(m_data.m_float.v0));
			buffer.AddData(&m_data.m_float.v1, sizeof(m_data.m_float.v1));
			buffer.AddData(&m_data.m_float.v2, sizeof(m_data.m_float.v2));
			break;
		}

	case DT_F3_1CONST:
		{
			buffer.AddData(&m_data.m_floatSemiConstant.constMask, sizeof(m_data.m_floatSemiConstant.constMask));
			buffer.AddData(&m_data.m_float.v0, sizeof(m_data.m_float.v0));
			buffer.AddData(&m_data.m_float.v1, sizeof(m_data.m_float.v1));
			break;
		}

	case DT_F3_2CONST:
		{
			buffer.AddData(&m_data.m_floatSemiConstant.constMask, sizeof(m_data.m_floatSemiConstant.constMask));
			buffer.AddData(&m_data.m_float.v0, sizeof(m_data.m_float.v0));
			break;
		}

	case DT_F3_3CONST:
		{
			buffer.AddData(&m_data.m_floatSemiConstant.constMask, sizeof(m_data.m_floatSemiConstant.constMask));
			break;
		}

	case DT_F4:
		{
			buffer.AddData(&m_data.m_float.v0, sizeof(m_data.m_float.v0));
			buffer.AddData(&m_data.m_float.v1, sizeof(m_data.m_float.v1));
			buffer.AddData(&m_data.m_float.v2, sizeof(m_data.m_float.v2));
			buffer.AddData(&m_data.m_float.v3, sizeof(m_data.m_float.v3));
			break;
		}

	case DT_F4_1CONST:
		{
			buffer.AddData(&m_data.m_floatSemiConstant.constMask, sizeof(m_data.m_floatSemiConstant.constMask));
			buffer.AddData(&m_data.m_float.v0, sizeof(m_data.m_float.v0));
			buffer.AddData(&m_data.m_float.v1, sizeof(m_data.m_float.v1));
			buffer.AddData(&m_data.m_float.v2, sizeof(m_data.m_float.v2));
			break;
		}
	case DT_F4_2CONST:
		{
			buffer.AddData(&m_data.m_floatSemiConstant.constMask, sizeof(m_data.m_floatSemiConstant.constMask));
			buffer.AddData(&m_data.m_float.v0, sizeof(m_data.m_float.v0));
			buffer.AddData(&m_data.m_float.v1, sizeof(m_data.m_float.v1));
			break;
		}
	case DT_F4_3CONST:
		{
			buffer.AddData(&m_data.m_floatSemiConstant.constMask, sizeof(m_data.m_floatSemiConstant.constMask));
			buffer.AddData(&m_data.m_float.v0, sizeof(m_data.m_float.v0));
			break;
		}
	case DT_F4_4CONST:
		{
			buffer.AddData(&m_data.m_floatSemiConstant.constMask, sizeof(m_data.m_floatSemiConstant.constMask));
			break;
		}

	case DT_RAWDATA:
		{
			uint32 size = m_data.m_rawData.size;
			buffer.AddData(&size, sizeof(size));
			buffer.AddData(m_data.m_rawData.data ? m_data.m_rawData.data : m_data.m_rawData.allocatedData, m_data.m_rawData.size);
			break;
		}

	default:
		assert(false);
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
// for debug purposes
const char* CAttrWriter::GetName() const
{
	return m_Writer.GetAttrNamesTable().GetString(m_nameID);
}

//////////////////////////////////////////////////////////////////////////
// for debug purposes
const char* CAttrWriter::GetStrData() const
{
	if (m_type == DT_STR)
	{
		return m_Writer.GetStrDataTable().GetString(m_data.m_dataStringID);
	}
	if (IsTypeStrConstant(m_type))
		return m_Writer.GetStrDataConstantsTable().GetString(GetStringConstantID(m_type));

	assert(false);

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
#ifdef XMLCPB_CHECK_HARDCODED_LIMITS
void CAttrWriter::CheckHardcodedLimits(int attrIndex)
{
	if (m_nameID > MAX_NAMEID)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "XMLCPB ERROR: IDAttrName overflow: %u / %d (attr index:%d)", m_nameID, MAX_NAMEID, attrIndex);
		m_Writer.NotifyInternalError();
	}
	else if (m_type == DT_STR && m_data.m_dataStringID > MAX_STRDATAID)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "XMLCPB ERROR: StrDataId overflow: %u / %d (attr index:%d)", m_data.m_dataStringID, MAX_STRDATAID, attrIndex);
		m_Writer.NotifyInternalError();
	}
}

#endif

//////////////////////////////////////////////////////////////////////////
// for debug/profiling purposes only
#ifdef XMLCPB_COLLECT_STATS
void CAttrWriter::AddDataToStatistics()
{
	m_statistics.m_typeCount[m_type]++;

	switch (m_type)
	{
	case DT_STR:
		{
			StringID stringId = /*IsTypeStrConstant(m_type) ? XMLCPB_INVALID_ID : */ m_data.m_dataStringID;
			std::map<StringID, uint>::iterator iter = m_statistics.m_stringIDMap.find(stringId);
			if (iter != m_statistics.m_stringIDMap.end())
				iter->second++;
			else
				m_statistics.m_stringIDMap[stringId] = 1;
			break;
		}

	case DT_INT32:
		{

			std::map<uint32, uint>::iterator iter = m_statistics.m_uint32Map.find(m_data.m_uint);
			if (iter != m_statistics.m_uint32Map.end())
				iter->second++;
			else
				m_statistics.m_uint32Map[m_data.m_uint] = 1;
			break;
		}

	case DT_INT64:
		{
			std::map<uint64, uint>::iterator iter = m_statistics.m_uint64Map.find(m_data.m_uint64);
			if (iter != m_statistics.m_uint64Map.end())
				iter->second++;
			else
				m_statistics.m_uint64Map[m_data.m_uint64] = 1;
			break;
		}

	case DT_F1:
		{
			std::map<float, uint>::iterator iter = m_statistics.m_floatMap.find(m_data.m_float.v0);
			if (iter != m_statistics.m_floatMap.end())
				iter->second++;
			else
				m_statistics.m_floatMap[m_data.m_float.v0] = 1;
			break;
		}

	case DT_F3:
		{
			SFloat3 val(m_data.m_float.v0, m_data.m_float.v1, m_data.m_float.v2);
			std::map<SFloat3, uint>::iterator iter = m_statistics.m_float3Map.find(val);
			if (iter != m_statistics.m_float3Map.end())
				iter->second++;
			else
				m_statistics.m_float3Map[val] = 1;
			break;
		}

	case DT_F4:
		{
			SFloat4 val(m_data.m_float.v0, m_data.m_float.v1, m_data.m_float.v2, m_data.m_float.v3);
			std::map<SFloat4, uint>::iterator iter = m_statistics.m_float4Map.find(val);
			if (iter != m_statistics.m_float4Map.end())
				iter->second++;
			else
				m_statistics.m_float4Map[val] = 1;
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// for debug/profiling purposes only
void CAttrWriter::WriteFileStatistics(const CStringTableWriter& stringTable)
{
	FILE* pFile = gEnv->pCryPak->FOpen("DataAttrStatistics.txt", "wb");
	assert(pFile);

	string strTmp;

	for (int i = 0; i < DT_NUMTYPES; i++)
	{
		strTmp.Format(" type: %s   times: %d \n", GetDataTypeName(eAttrDataType(i)), m_statistics.m_typeCount[i]);
		gEnv->pCryPak->FWrite(strTmp.c_str(), strTmp.size(), 1, pFile);
	}

	const int MIN_AMOUNT_FOR_FILE = 50;

	sortingMapType sortingMap;

	{
		sortingMap.clear();
		std::map<StringID, uint>::iterator iter = m_statistics.m_stringIDMap.begin();
		while (iter != m_statistics.m_stringIDMap.end())
		{
			if (iter->second > MIN_AMOUNT_FOR_FILE)
			{
				const char* pStr = iter->first == XMLCPB_INVALID_ID ? "someConstant" : stringTable.GetString(iter->first);

				strTmp.Format("%s : %d\n", pStr, iter->second);
				sortingMap.insert(std::pair<uint, string>(iter->second, strTmp));
			}
			++iter;
		}
		WriteDataTypeEntriesStatistics(pFile, sortingMap, "\n------------data strings-----------------\n");
	}

	{
		sortingMap.clear();
		std::map<uint32, uint>::iterator iter = m_statistics.m_uint32Map.begin();
		while (iter != m_statistics.m_uint32Map.end())
		{
			if (iter->second > MIN_AMOUNT_FOR_FILE)
			{
				strTmp.Format("%d (%x) : %d\n", iter->first, iter->first, iter->second);
				sortingMap.insert(std::pair<uint, string>(iter->second, strTmp));
			}
			++iter;
		}
		WriteDataTypeEntriesStatistics(pFile, sortingMap, "\n------------uint32-----------------\n");
	}

	{
		sortingMap.clear();
		std::map<float, uint>::iterator iter = m_statistics.m_floatMap.begin();
		while (iter != m_statistics.m_floatMap.end())
		{
			if (iter->second > MIN_AMOUNT_FOR_FILE)
			{
				strTmp.Format("%f : %d\n", iter->first, iter->second);
				sortingMap.insert(std::pair<uint, string>(iter->second, strTmp));
			}
			++iter;
		}
		WriteDataTypeEntriesStatistics(pFile, sortingMap, "\n------------F1-----------------\n");
	}

	{
		sortingMap.clear();
		std::map<SFloat3, uint>::iterator iter = m_statistics.m_float3Map.begin();
		while (iter != m_statistics.m_float3Map.end())
		{
			if (iter->second > MIN_AMOUNT_FOR_FILE)
			{
				strTmp.Format("%f,%f,%f: %d\n", iter->first.v0, iter->first.v1, iter->first.v2, iter->second);
				sortingMap.insert(std::pair<uint, string>(iter->second, strTmp));
			}
			++iter;
		}
		WriteDataTypeEntriesStatistics(pFile, sortingMap, "\n------------F3-----------------\n");
	}

	{
		sortingMap.clear();
		std::map<SFloat4, uint>::iterator iter = m_statistics.m_float4Map.begin();
		while (iter != m_statistics.m_float4Map.end())
		{
			if (iter->second > MIN_AMOUNT_FOR_FILE)
			{
				strTmp.Format("%f,%f,%f,%f: %d\n", iter->first.v0, iter->first.v1, iter->first.v2, iter->first.v3, iter->second);
				sortingMap.insert(std::pair<uint, string>(iter->second, strTmp));
			}
			++iter;
		}
		WriteDataTypeEntriesStatistics(pFile, sortingMap, "\n------------F4-----------------\n");
	}

	gEnv->pCryPak->FClose(pFile);
}

// for debug/profiling purposes only
void CAttrWriter::WriteDataTypeEntriesStatistics(FILE* pFile, const sortingMapType& sortingMap, const char* pHeaderStr)
{
	gEnv->pCryPak->FWrite(pHeaderStr, strlen(pHeaderStr), 1, pFile);
	sortingMapType::const_iterator iterSorted = sortingMap.begin();
	while (iterSorted != sortingMap.end())
	{
		const string& strTmp = iterSorted->second;
		gEnv->pCryPak->FWrite(strTmp.c_str(), strTmp.size(), 1, pFile);
		++iterSorted;
	}
}
#endif
