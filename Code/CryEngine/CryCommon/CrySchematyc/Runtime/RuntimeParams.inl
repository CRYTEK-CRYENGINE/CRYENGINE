// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Schematyc
{

inline CRuntimeParams::CRuntimeParams() {}

inline CRuntimeParams::CRuntimeParams(const CRuntimeParams& rhs)
	: CRuntimeParamMap(rhs)
	, m_scratchpad(rhs.m_scratchpad)
{}

inline CRuntimeParams::CRuntimeParams(const CRuntimeParamMap& rhs)
{
	*this = rhs;
}

inline CRuntimeParams::CRuntimeParams(const SInPlaceStorageParams& inputStorage, const SInPlaceStorageParams& outputStorage, const SInPlaceStorageParams& scratchpadStorage)
	: CRuntimeParamMap(inputStorage, outputStorage)
	, m_scratchpad(scratchpadStorage)
{}

inline CRuntimeParams::CRuntimeParams(const SInPlaceStorageParams& inputStorage, const SInPlaceStorageParams& outputStorage, const SInPlaceStorageParams& scratchpadStorage, const CRuntimeParams& rhs)
	: CRuntimeParamMap(inputStorage, outputStorage, rhs)
	, m_scratchpad(scratchpadStorage, rhs.m_scratchpad)
{}

inline CRuntimeParams::CRuntimeParams(const SInPlaceStorageParams& inputStorage, const SInPlaceStorageParams& outputStorage, const SInPlaceStorageParams& scratchpadStorage, const CRuntimeParamMap& rhs)
	: CRuntimeParamMap(inputStorage, outputStorage)
	, m_scratchpad(scratchpadStorage)
{
	*this = rhs;
}

inline void CRuntimeParams::ReserveScratchpad(uint32 capacity)
{
	m_scratchpad.Reserve(capacity);
}

inline bool CRuntimeParams::AddInput(const CUniqueId& id, const CAnyConstRef& value)
{
	const uint32 pos = m_scratchpad.Add(value);
	const CAnyPtr pValue = m_scratchpad.Get(pos);
	return CRuntimeParamMap::BindInput(id, pValue);
}

inline bool CRuntimeParams::AddOutput(const CUniqueId& id, const CAnyConstRef& value)
{
	const uint32 pos = m_scratchpad.Add(value);
	const CAnyPtr pValue = m_scratchpad.Get(pos);
	return CRuntimeParamMap::BindOutput(id, pValue);
}

inline CRuntimeParams& CRuntimeParams::operator=(const CRuntimeParams& rhs)
{
	CRuntimeParamMap::operator=(rhs);
	m_scratchpad = rhs.m_scratchpad;
	return *this;
}

inline CRuntimeParams& CRuntimeParams::operator=(const CRuntimeParamMap& rhs)
{
	auto visitInput = [this](const CUniqueId& id, const CAnyConstRef& value)
	{
		AddInput(id, value);
	};
	rhs.VisitInputs(visitInput);

	auto visitOutput = [this](const CUniqueId& id, const CAnyConstRef& value)
	{
		AddOutput(id, value);
	};
	rhs.VisitOutputs(visitOutput);

	return *this;
}

namespace RuntimeParams
{

inline bool FromInputClass(const CClassDesc& typeDesc, CRuntimeParams& params, const void* pInput)
{
	for (const CClassMemberDesc& memberDesc : typeDesc.GetMembers())
	{
		if (!params.AddInput(CUniqueId::FromUInt32(memberDesc.GetId()), CAnyConstRef(memberDesc.GetTypeDesc(), reinterpret_cast<const uint8*>(pInput) + memberDesc.GetOffset())))
		{
			return false;
		}
	}
	return true;
}

template<typename TYPE> inline bool FromInputClass(CRuntimeParams& params, const TYPE& input)
{
	SCHEMATYC_VERIFY_TYPE_IS_REFLECTED(TYPE);
	static_assert(std::is_class<TYPE>::value, "Type must be class!");

	return FromInputClass(GetTypeDesc<TYPE>(), params, &input);
}

inline bool ToInputClass(const CClassDesc& typeDesc, void* pOutput, const CRuntimeParamMap& params)
{
	return RuntimeParamMap::ToInputClass(typeDesc, pOutput, params);
}

template<typename TYPE> inline bool ToInputClass(TYPE& output, const CRuntimeParamMap& params)
{
	return RuntimeParamMap::ToInputClass(output, params);
}

} // RuntimeParams
} // Schematyc
