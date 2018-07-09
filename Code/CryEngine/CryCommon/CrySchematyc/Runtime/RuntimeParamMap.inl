// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Schematyc
{

inline CRuntimeParamMap::SInput::SInput(const CUniqueId& _id, const CAnyConstPtr& _pValue)
	: id(_id)
	, pValue(_pValue)
{}

inline CRuntimeParamMap::SOutput::SOutput(const CUniqueId& _id, const CAnyPtr& _pValue)
	: id(_id)
	, pValue(_pValue)
{}

inline CRuntimeParamMap::CRuntimeParamMap() {}

inline CRuntimeParamMap::CRuntimeParamMap(const CRuntimeParamMap& rhs)
	: m_inputs(rhs.m_inputs)
	, m_outputs(rhs.m_outputs)
{}

inline CRuntimeParamMap::CRuntimeParamMap(const SInPlaceStorageParams& inputStorage, const SInPlaceStorageParams& outputStorage)
	: m_inputs(inputStorage)
	, m_outputs(outputStorage)
{}

inline CRuntimeParamMap::CRuntimeParamMap(const SInPlaceStorageParams& inputStorage, const SInPlaceStorageParams& outputStorage, const CRuntimeParamMap& rhs)
	: m_inputs(inputStorage, rhs.m_inputs)
	, m_outputs(outputStorage, rhs.m_outputs)
{}

inline void CRuntimeParamMap::ReserveInputs(uint32 capacity)
{
	m_inputs.reserve(capacity);
}

inline bool CRuntimeParamMap::BindInput(const CUniqueId& id, const CAnyConstPtr& pValue)
{
	if (pValue && !GetInput(id))
	{
		m_inputs.emplace_back(id, pValue);
		return true;
	}
	return false;
}

inline CAnyConstPtr CRuntimeParamMap::GetInput(const CUniqueId& id) const
{
	for (const SInput& input : m_inputs)
	{
		if (input.id == id)
		{
			return input.pValue;
		}
	}
	return CAnyConstPtr();
}

inline bool CRuntimeParamMap::GetInput(const CUniqueId& id, const CAnyRef& value) const
{
	for (const SInput& input : m_inputs)
	{
		if (input.id == id)
		{
			Any::CopyAssign(value, *input.pValue);
			return true;
		}
	}
	return false;
}

template <typename VISITOR> inline void CRuntimeParamMap::VisitInputs(VISITOR visitor) const
{
	for (const SInput& input : m_inputs)
	{
		visitor(input.id, *input.pValue);
	}
}

inline void CRuntimeParamMap::ReserveOutputs(uint32 capacity)
{
	m_outputs.reserve(capacity);
}

inline bool CRuntimeParamMap::BindOutput(const CUniqueId& id, const CAnyPtr& pValue)
{
	if (pValue && !GetOutput(id))
	{
		m_outputs.emplace_back(id, pValue);
		return true;
	}
	return false;
}

inline bool CRuntimeParamMap::SetOutput(const CUniqueId& id, const CAnyConstRef& value) const
{
	for (const SOutput& output : m_outputs)
	{
		if (output.id == id)
		{
			return Any::CopyAssign(*output.pValue, value);
		}
	}
	return false;
}

inline CAnyPtr CRuntimeParamMap::GetOutput(const CUniqueId& id)
{
	for (SOutput& output : m_outputs)
	{
		if (output.id == id)
		{
			return output.pValue;
		}
	}
	return CAnyPtr();
}

inline CAnyConstPtr CRuntimeParamMap::GetOutput(const CUniqueId& id) const
{
	for (const SOutput& output : m_outputs)
	{
		if (output.id == id)
		{
			return output.pValue;
		}
	}
	return CAnyConstPtr();
}

inline bool CRuntimeParamMap::GetOutput(const CUniqueId& id, const CAnyRef& value) const
{
	for (const SOutput& output : m_outputs)
	{
		if (output.id == id)
		{
			return Any::CopyAssign(value, *output.pValue);
		}
	}
	return false;
}

template <typename VISITOR> inline void CRuntimeParamMap::VisitOutputs(VISITOR visitor)
{
	for (SOutput& output : m_outputs)
	{
		visitor(output.id, *output.pValue);
	}
}

template <typename VISITOR> inline void CRuntimeParamMap::VisitOutputs(VISITOR visitor) const
{
	for (const SOutput& output : m_outputs)
	{
		visitor(output.id, *output.pValue);
	}
}

inline CRuntimeParamMap& CRuntimeParamMap::operator=(const CRuntimeParamMap& rhs)
{
	m_inputs = rhs.m_inputs;
	m_outputs = rhs.m_outputs;
	return *this;
}

namespace RuntimeParamMap
{

inline bool FromInputClass(const CClassDesc& typeDesc, CRuntimeParamMap& params, const void* pInput)
{
	for (const CClassMemberDesc& memberDesc : typeDesc.GetMembers())
	{
		if (!params.BindInput(CUniqueId::FromUInt32(memberDesc.GetId()), CAnyConstPtr(memberDesc.GetTypeDesc(), reinterpret_cast<const uint8*>(pInput) + memberDesc.GetOffset())))
		{
			return false;
		}
	}
	return true;
}

template<typename TYPE> inline bool FromInputClass(CRuntimeParamMap& params, const TYPE& input)
{
	SCHEMATYC_VERIFY_TYPE_IS_REFLECTED(TYPE);
	static_assert(std::is_class<TYPE>::value, "Type must be class!");

	return FromInputClass(GetTypeDesc<TYPE>(), params, &input);
}

inline bool ToInputClass(const CClassDesc& typeDesc, void* pOutput, const CRuntimeParamMap& params)
{
	for (const CClassMemberDesc& memberDesc : typeDesc.GetMembers())
	{
		if (!params.GetInput(CUniqueId::FromUInt32(memberDesc.GetId()), CAnyRef(memberDesc.GetTypeDesc(), reinterpret_cast<uint8*>(pOutput) + memberDesc.GetOffset())))
		{
			return false;
		}
	}
	return true;
}

template<typename TYPE> inline bool ToInputClass(TYPE& output, const CRuntimeParamMap& params)
{
	SCHEMATYC_VERIFY_TYPE_IS_REFLECTED(TYPE);
	static_assert(std::is_class<TYPE>::value, "Type must be class!");

	return ToInputClass(GetTypeDesc<TYPE>(), &output, params);
}

} // RuntimeParamMap
} // Schematyc
