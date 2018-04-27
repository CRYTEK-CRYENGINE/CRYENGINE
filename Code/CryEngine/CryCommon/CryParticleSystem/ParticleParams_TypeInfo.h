// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ParticleParamsTypeInfo.h
// -------------------------------------------------------------------------
// Implements TypeInfo for ParticleParams.
// Include only once per executable.
//
////////////////////////////////////////////////////////////////////////////

#ifdef ENABLE_TYPE_INFO_NAMES
	#if !ENABLE_TYPE_INFO_NAMES
		#error ENABLE_TYPE_INFO_NAMES previously defined to 0
	#endif
#else
	#define ENABLE_TYPE_INFO_NAMES 1
#endif

#include <CryCore/TypeInfo_impl.h>
#include <CryRenderer/IShader_info.h>
#include <Cry3DEngine/I3DEngine_info.h>
#include <CryCore/Common_TypeInfo2.h>
#include "ParticleParams_info.h"
#include <CryString/Name_TypeInfo.h>
#include <CryCore/CryTypeInfo.h>

///////////////////////////////////////////////////////////////////////
// Implementation of TCurve<> functions.

//! \cond INTERNAL
//! Helper class for serialization.
template<class T, class V>
struct SplineElem
{
	T   t;
	V   v;
	int flags;

	STRUCT_INFO_BEGIN_INTERNAL(SplineElem)
	VAR_INFO(t)
	VAR_INFO(v)
	VAR_INFO(flags)
	STRUCT_INFO_END(SplineElem)
};
//! \endcond

template<class S>
string TCurve<S >::ToString(FToString flags) const
{
	string str;
	for (int i = 0; i < num_keys(); i++)
	{
		if (i > 0)
			str += ";";
		key_type k = key(i);
		SplineElem<UnitFloat8, TMod> elem = { k.time, k.value, k.flags };
		str += ::TypeInfo(&elem).ToString(&elem, flags);
	}
	return str;
}

template<class S>
bool TCurve<S >::FromString(cstr str_in, FFromString flags)
{
	CryStackStringT<char, 256> strTemp;

	source_spline source;

	cstr str = str_in;
	while (*str)
	{
		// Extract element string.
		while (*str == ' ')
			str++;
		cstr strElem = str;
		if (cstr strEnd = strchr(str, ';'))
		{
			strTemp.assign(str, strEnd);
			strElem = strTemp;
			str = strEnd + 1;
		}
		else
			str = "";

		// Parse element.
		SplineElem<float, typename source_spline::value_type> elem = { 0, 0, 0 };
		if (!::TypeInfo(&elem).FromString(&elem, strElem, FFromString().SkipEmpty(1)))
			return false;

		typename source_spline::key_type key;
		key.time = elem.t;
		key.value = elem.v;
		key.flags = elem.flags;

		source.insert_key(key);
	}
	;

	source.SetModified(true);
	super_type::from_source(source);

	return true;
}

///////////////////////////////////////////////////////////////////////
template<class S>
struct TCurve<S >::CCustomInfo : CTypeInfo
{
	typedef spline::FinalizingSpline<typename TCurve<S>::source_spline, TCurve<S>> TIndirectSpline;

	CCustomInfo()
		: CTypeInfo("TCurve<>", sizeof(TThis), alignof(TThis))
	{}
	virtual string ToString(const void* data, FToString flags = {}, const void* def_data = 0) const
	{
		if (!HasString(*(const TThis*)data, flags, def_data))
			return string();
		return ((const TThis*)data)->ToString(flags);
	}
	virtual bool FromString(void* data, cstr str, FFromString flags = {}) const
	{
		return ((TThis*)data)->FromString(str, flags);
	}
	virtual bool ToValue(const void* data, void* value, const CTypeInfo& typeVal) const
	{
		if (&typeVal == this)
			return *(TThis*)value = *(const TThis*)data, true;
		if (typeVal.IsType<ISplineInterpolator*>())
		{
			TCurve<S>* pSource = (TCurve<S>*)data;
			TIndirectSpline*& pDest = *(TIndirectSpline**)value;
			if (!pDest)
				pDest = new TIndirectSpline;
			pDest->SetFinal(pSource);
			return true;
		}
		return false;
	}
	virtual bool FromValue(void* data, const void* value, const CTypeInfo& typeVal) const
	{
		if (&typeVal == this)
			return *(TThis*)data = *(const TThis*)value, true;
		return false;
	}
	virtual bool ValueEqual(const void* data, const void* def_data = 0) const
	{
		if (!def_data)
			return ((const TThis*)data)->empty();
		return *(const TThis*)data == *(const TThis*)def_data;
	}
	virtual void GetMemoryUsage(ICrySizer* pSizer, const void* data) const
	{
		((TThis*)data)->GetMemoryUsage(pSizer);
	}
};

const CTypeInfo& CSurfaceTypeIndex::TypeInfo() const
{
	struct SurfaceEnums : DynArray<CEnumInfo<uint16>::SElem>
	{
		//! Enumerate game surface types.
		SurfaceEnums()
		{
			CEnumInfo<uint16>::SElem elem = { 0, "" };

			// Empty elem for 0 value.
			push_back(elem);

			// Trigger surface types loading.
			gEnv->p3DEngine->GetMaterialManager()->GetDefaultLayersMaterial();

			// Get surface types, duplicate names, store in enum array.
			ISurfaceTypeEnumerator* pSurfaceTypeEnum = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager()->GetEnumerator();
			for (ISurfaceType* pSurfaceType = pSurfaceTypeEnum->GetFirst(); pSurfaceType; pSurfaceType = pSurfaceTypeEnum->GetNext())
			{
				elem.Value = pSurfaceType->GetId();
				elem.Name = pSurfaceType->GetName();
				if (size_t nLen = strlen(elem.Name))
					elem.Name = (char*)memcpy(new char[nLen + 1], elem.Name, nLen + 1);
				push_back(elem);
			}
			pSurfaceTypeEnum->Release();
		}

		~SurfaceEnums()
		{
			// Free names.
			for (int i = size() - 1; i >= 0; i--)
			{
				if (*at(i).Name)
					delete[] at(i).Name;
			}
		}
	};

#pragma warning(push)
#pragma warning(disable:4355) // 'this': used in base member initializer list
	struct CCustomInfo : SurfaceEnums, CEnumInfo<uint16>
	{
		CCustomInfo()
			: CEnumInfo<uint16>("CSurfaceTypeIndex", *this)
		{}
	};
#pragma warning(pop)

	static CCustomInfo Info;
	return Info;
}

#if defined(TEST_TYPEINFO) && defined(_DEBUG)

struct STypeInfoTest
{
	STypeInfoTest()
	{
		TestTypes<UnitFloat8>(1.f);
		TestTypes<UnitFloat8>(0.5f);
		TestTypes<UnitFloat8>(37.f / 255);
		TestTypes<UnitFloat8>(80.f / 240);
		TestTypes<UnitFloat8>(1.001f);
		TestTypes<UnitFloat8>(-78.f);

		TestTypes<SFloat16>(0.f);
		TestTypes<SFloat16>(1.f);
		TestTypes<SFloat16>(0.999f);
		TestTypes<SFloat16>(0.9999f);
		TestTypes<SFloat16>(-123.4f);
		TestTypes<SFloat16>(-123.5f);
		TestTypes<SFloat16>(-123.6f);
		TestTypes<SFloat16>(-123.456f);
		TestTypes<SFloat16>(-0.00012345f);
		TestTypes<SFloat16>(-1e-8f);
		TestTypes<SFloat16>(1e27f);

		TestTypes<UFloat16>(1.f);
		TestTypes<UFloat16>(9.87654321f);
		TestTypes<UFloat16>(0.00012345f);
		TestTypes<UFloat16>(45678.9012f);
		TestTypes<UFloat16>(1e16f);

		TestTypes<Vec3U>(Vec3(2, 3, 4));
		TestTypes<Color3B>(Color3F(1, 0.5, 0.25));
	}
};
static STypeInfoTest _ParticleTypeInfoTest;

#endif
