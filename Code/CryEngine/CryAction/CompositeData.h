// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __COMPOSITEDATA_H__
#define __COMPOSITEDATA_H__

#pragma once

#include <CryCore/BoostHelpers.h>

// basic types are CONCRETE types which does NOT contain references like
// pointers or references, or more specifically dynamically allocated strings
// their value sizes can be determined by simply doing sizeof(T); also
// {&T, sizeof(T)} should be enough to retrieve the value bytes or set it; AND
// the condition must be satisfied that sizeof(T) <= 255.
enum EBasicTypes4CC
{
	eBT_i08  = '_i08',
	eBT_i16  = '_i16',
	eBT_i32  = '_i32',
	eBT_i64  = '_i64',
	eBT_u08  = '_u08',
	eBT_u16  = '_u16',
	eBT_u32  = '_u32',
	eBT_u64  = '_u64',
	eBT_f32  = '_f32',
	eBT_f64  = '_f64',
	eBT_str  = '_str', // 255 characters maximum, one byte prefix for length

	eBT_vec3 = 'vec3', // Vec3<f32>
	eBT_ang3 = 'ang3', // Ang3<f32>
	eBT_mtrx = 'mtrx', // Matrix34<f32>
	eBT_quat = 'quat', // Quat<f32>
	eBT_qutt = 'qutt', // QuatT<f32>
};

typedef boost::mpl::vector<int8, uint8, int16, uint16, int32, uint32, int64, uint64, f32, f64, string, Vec3, Matrix34, Quat, QuatT> TBasicTypes;
typedef boost::make_variant_over<TBasicTypes>::type                                                                                 TBasicType;

template<typename T>
static inline T ComposeValue(const uint8* data, size_t size)
{
	CRY_ASSERT(sizeof(T) == size);
	T t = T();
	memcpy(&t, data, size);
	return t;
}

template<>
inline string ComposeValue<string>(const uint8* data, size_t size)
{
	return string((const char*)data, size);
}

static inline TBasicType ComposeValue(const IMetadata* metadata)
{
	TBasicType v;
	uint8 dt[255];
	uint8 sz = sizeof(dt);
	if (!metadata->GetValue(dt, &sz))
		return v;
	switch (metadata->GetValueType())
	{
	case eBT_i08:
		v = TBasicType(ComposeValue<int8>(dt, sz));
		break;
	case eBT_u08:
		v = TBasicType(ComposeValue<uint8>(dt, sz));
		break;
	case eBT_i16:
		v = TBasicType(ComposeValue<int16>(dt, sz));
		break;
	case eBT_u16:
		v = TBasicType(ComposeValue<uint16>(dt, sz));
		break;
	case eBT_i32:
		v = TBasicType(ComposeValue<int32>(dt, sz));
		break;
	case eBT_u32:
		v = TBasicType(ComposeValue<uint32>(dt, sz));
		break;
	case eBT_i64:
		v = TBasicType(ComposeValue<int64>(dt, sz));
		break;
	case eBT_u64:
		v = TBasicType(ComposeValue<uint64>(dt, sz));
		break;
	case eBT_f32:
		v = TBasicType(ComposeValue<f32>(dt, sz));
		break;
	case eBT_f64:
		v = TBasicType(ComposeValue<f64>(dt, sz));
		break;
	case eBT_str:
		v = TBasicType(ComposeValue<string>(dt, sz));
		break;
	case eBT_vec3:
		v = TBasicType(ComposeValue<Vec3>(dt, sz));
		break;
	case eBT_ang3:
		v = TBasicType(ComposeValue<Ang3>(dt, sz));
		break;
	case eBT_mtrx:
		v = TBasicType(ComposeValue<Matrix34>(dt, sz));
		break;
	case eBT_quat:
		v = TBasicType(ComposeValue<Quat>(dt, sz));
		break;
	case eBT_qutt:
		v = TBasicType(ComposeValue<QuatT>(dt, sz));
		break;
	default:
		break;
	}
	return v;
}

/*
   CCompositeData data;
   data.Compose(metadata);
   data.GetField(eDT_name).GetValue().GetPtr<string>()->c_str();
   data.GetField(eDT_item).GetField(eDT_name).GetPtr<string>()->c_str();
 */

class CCompositeData
{
public:
	CCompositeData() : m_tag(0) {}
	~CCompositeData() {}

	void Compose(const IMetadata* metadata)
	{
		m_tag = metadata->GetTag();
		m_value = ::ComposeValue(metadata);

		for (size_t i = 0; i < metadata->GetNumFields(); ++i)
		{
			const IMetadata* pMetadata = metadata->GetFieldByIndex(i);
			CCompositeData data;
			data.Compose(pMetadata);
			m_fields.push_back(data);
		}
	}

	const TBasicType& GetValue() const
	{
		return m_value;
	}

	const CCompositeData& GetField(uint32 tag, size_t count = 1) const
	{
		size_t n = 0;
		for (size_t i = 0; i < m_fields.size(); ++i)
			if (m_fields[i].m_tag == tag && ++n == count)
				return m_fields[i];

		static const CCompositeData dummy;
		return dummy;
	}

private:
	uint32                      m_tag;
	TBasicType                  m_value;

	std::vector<CCompositeData> m_fields;
};

#endif
