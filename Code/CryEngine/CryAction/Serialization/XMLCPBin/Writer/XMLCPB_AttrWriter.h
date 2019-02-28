// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

#pragma once
#ifndef XMLCPB_ATTRWRITER_H
	#define XMLCPB_ATTRWRITER_H

	#include "../XMLCPB_Common.h"

// holds an attribute of a live node

namespace XMLCPB {

class CWriter;
class CStringTableWriter;

class CAttrWriter
{
public:

	CAttrWriter(CWriter& Writer);
	CAttrWriter(const CAttrWriter& other) : m_type(DT_INVALID), m_Writer(other.m_Writer) { *this = other; }
	~CAttrWriter()
	{
		if (m_type == DT_RAWDATA)
			SAFE_DELETE_ARRAY(m_data.m_rawData.allocatedData);
	}

	CAttrWriter& operator=(const CAttrWriter& other)
	{
		if (this != &other)
		{
			if (m_type == DT_RAWDATA)
				SAFE_DELETE_ARRAY(m_data.m_rawData.allocatedData);
			m_data = other.m_data;
			if (other.m_type == DT_RAWDATA && other.m_data.m_rawData.allocatedData)
			{
				m_data.m_rawData.allocatedData = new uint8[other.m_data.m_rawData.size];
				memcpy(m_data.m_rawData.allocatedData, other.m_data.m_rawData.allocatedData, other.m_data.m_rawData.size);
			}

			m_type = other.m_type;
			m_nameID = other.m_nameID;
		}
		return *this;
	}

	void        Set(const char* pAttrName, int val);
	void        Set(const char* pAttrName, uint val);
	void        Set(const char* pAttrName, int64 val);
	void        Set(const char* pAttrName, uint64 val);
	void        Set(const char* pAttrName, float val);
	void        Set(const char* pAttrName, bool val);
	void        Set(const char* pAttrName, const Vec2& val);
	void        Set(const char* pAttrName, const Ang3& val);
	void        Set(const char* pAttrName, const Vec3& val);
	void        Set(const char* pAttrName, const Quat& val);
	void        Set(const char* pAttrName, const char* pStr);
	void        Set(const char* pAttrName, const uint8* data, uint32 len, bool needInmediateCopy);
	void        Compact();
	const char* GetName() const;
	const char* GetStrData() const;
	uint16      CalcHeader() const;

	#ifdef XMLCPB_CHECK_HARDCODED_LIMITS
	void CheckHardcodedLimits(int attrIndex);
	#endif

private:

	void SetName(const char* pName);
	void PackFloatInSemiConstType(float val, uint32 ind, uint32& numVals);

	#ifdef XMLCPB_COLLECT_STATS
	void AddDataToStatistics();
	#endif

	eAttrDataType m_type;
	StringID      m_nameID;
	CWriter&      m_Writer;

	union
	{
		StringID m_dataStringID;
		uint32   m_uint;
		uint64   m_uint64;
		struct floatType
		{
			float v0;
			float v1;
			float v2;
			float v3;
		} m_float;             // used for all float, vec2, vec3, quat, etc
		struct floatSemiConstantType
		{
			float v[4];      // the position of those has to be compatible with the ones in "floattype"
			uint8 constMask; // look into PackFloatInSemiConstType() for explanation
		} m_floatSemiConstant;
		struct rawDataType
		{
			uint32       size;
			const uint8* data;      // one of them has to be always NULL when this struct is used.
			uint8*       allocatedData;
		} m_rawData;             // used for raw byte chunk
	} m_data;

	#ifdef XMLCPB_COLLECT_STATS
	struct SFloat3
	{
		SFloat3(float _v0, float _v1, float _v2) : v0(_v0), v1(_v1), v2(_v2) {}
		float v0, v1, v2;
		bool operator<(const SFloat3& other) const
		{
			if (v0 < other.v0) return true;
			else if (v1 < other.v1) return true;
			else return v2 < other.v2;
		}
	};

	struct SFloat4
	{
		SFloat4(float _v0, float _v1, float _v2, float _v3) : v0(_v0), v1(_v1), v2(_v2), v3(_v3){}
		float v0, v1, v2, v3;
		bool operator<(const SFloat4& other) const
		{
			if (v0 < other.v0) return true;
			else if (v1 < other.v1) return true;
			else if (v2 < other.v2) return true;
			else return v3 < other.v3;
		}
	};

public:
	struct SStatistics
	{
		std::unordered_map<StringID, uint> m_stringIDMap;
		std::unordered_map<uint32, uint>   m_uint32Map;
		std::unordered_map<uint64, uint>   m_uint64Map;
		std::unordered_map<float, uint>    m_floatMap;
		std::unordered_map<SFloat3, uint>  m_float3Map;
		std::unordered_map<SFloat4, uint>  m_float4Map;
		int                                m_typeCount[DT_NUMTYPES];

		SStatistics() { Reset(); }

		void Reset()
		{
			for (int i = 0; i < DT_NUMTYPES; i++)
				m_typeCount[i] = 0;
			m_stringIDMap.clear();
			m_uint32Map.clear();
			m_uint64Map.clear();
			m_floatMap.clear();
			m_float3Map.clear();
			m_float4Map.clear();
		}
	};
	static SStatistics m_statistics;

	typedef std::multimap<uint, string, std::greater<int>> sortingMapType;
	static void WriteFileStatistics(const CStringTableWriter& stringTable);
private:
	static void WriteDataTypeEntriesStatistics(FILE* pFile, const sortingMapType& sortingMap, const char* pHeaderStr);

	#endif
};

} // end namespace
#endif
