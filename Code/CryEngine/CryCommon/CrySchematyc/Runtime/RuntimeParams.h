// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/FundamentalTypes.h"
#include "CrySchematyc/Runtime/RuntimeParamMap.h"
#include "CrySchematyc/Utils/Any.h"
#include "CrySchematyc/Utils/Scratchpad.h"
#include "CrySchematyc/Utils/UniqueId.h"

namespace Schematyc
{

class CRuntimeParams : public CRuntimeParamMap
{
public:

	CRuntimeParams();
	CRuntimeParams(const CRuntimeParams& rhs);
	explicit CRuntimeParams(const CRuntimeParamMap& rhs);
	CRuntimeParams(const SInPlaceStorageParams& inputStorage, const SInPlaceStorageParams& outputStorage, const SInPlaceStorageParams& scratchpadStorage);
	CRuntimeParams(const SInPlaceStorageParams& inputStorage, const SInPlaceStorageParams& outputStorage, const SInPlaceStorageParams& scratchpadStorage, const CRuntimeParams& rhs);
	explicit CRuntimeParams(const SInPlaceStorageParams& inputStorage, const SInPlaceStorageParams& outputStorage, const SInPlaceStorageParams& scratchpadStorage, const CRuntimeParamMap& rhs);

	void ReserveScratchpad(uint32 capacity);

	bool AddInput(const CUniqueId& id, const CAnyConstRef& value);
	bool AddOutput(const CUniqueId& id, const CAnyConstRef& value);

	CRuntimeParams& operator=(const CRuntimeParams& rhs);
	CRuntimeParams& operator=(const CRuntimeParamMap& rhs);

private:

	CScratchpad m_scratchpad;
};

template<uint32 INPUT_CAPACITY, uint32 OUTPUT_CAPACITY, uint32 SCRATCHPAD_CAPACITY> class CInPlaceRuntimeParams : public CRuntimeParams
{
public:

	inline CInPlaceRuntimeParams()
		: CRuntimeParams(SInPlaceStorageParams(INPUT_CAPACITY, m_inputStorage), SInPlaceStorageParams(OUTPUT_CAPACITY, m_outputStorage), SInPlaceStorageParams(SCRATCHPAD_CAPACITY, m_scratchpadStorage))
	{}

	inline CInPlaceRuntimeParams(const CRuntimeParams& rhs)
		: CRuntimeParams(SInPlaceStorageParams(INPUT_CAPACITY, m_inputStorage), SInPlaceStorageParams(OUTPUT_CAPACITY, m_outputStorage), SInPlaceStorageParams(SCRATCHPAD_CAPACITY, m_scratchpadStorage), rhs)
	{}

	explicit inline CInPlaceRuntimeParams(const CRuntimeParamMap& rhs)
		: CRuntimeParams(SInPlaceStorageParams(INPUT_CAPACITY, m_inputStorage), SInPlaceStorageParams(OUTPUT_CAPACITY, m_outputStorage), SInPlaceStorageParams(SCRATCHPAD_CAPACITY, m_scratchpadStorage), rhs)
	{}

	inline CInPlaceRuntimeParams(const CInPlaceRuntimeParams& rhs)
		: CRuntimeParams(SInPlaceStorageParams(INPUT_CAPACITY, m_inputStorage), SInPlaceStorageParams(OUTPUT_CAPACITY, m_outputStorage), SInPlaceStorageParams(SCRATCHPAD_CAPACITY, m_scratchpadStorage), rhs)
	{}

	template <uint32 RHS_INPUT_CAPACITY, uint32 RHS_OUTPUT_CAPACITY, uint32 RHS_SCRATCHPAD_CAPACITY> inline CInPlaceRuntimeParams(const CInPlaceRuntimeParams<RHS_INPUT_CAPACITY, RHS_OUTPUT_CAPACITY, RHS_SCRATCHPAD_CAPACITY>& rhs)
		: CRuntimeParams(SInPlaceStorageParams(INPUT_CAPACITY, m_inputStorage), SInPlaceStorageParams(OUTPUT_CAPACITY, m_outputStorage), SInPlaceStorageParams(SCRATCHPAD_CAPACITY, m_scratchpadStorage), rhs)
	{}

private:

	uint8 m_inputStorage[INPUT_CAPACITY];
	uint8 m_outputStorage[OUTPUT_CAPACITY];
	uint8 m_scratchpadStorage[SCRATCHPAD_CAPACITY];
};

typedef CRuntimeParams HeapRuntimeParams;
#if SCHEMATYC_HASH_UNIQUE_IDS
typedef CInPlaceRuntimeParams<64, 64, 256> StackRuntimeParams;
#else
typedef CInPlaceRuntimeParams<256, 256, 256> StackRuntimeParams;
#endif

namespace RuntimeParams
{

bool FromInputClass(const CClassDesc& typeDesc, CRuntimeParams& params, const void* pInput);
template<typename TYPE> inline bool FromInputClass(CRuntimeParams& params, const TYPE& input);

bool ToInputClass(const CClassDesc& typeDesc, void* pOutput, const CRuntimeParams& params);
template<typename TYPE> inline bool ToInputClass(TYPE& output, const CRuntimeParams& params);

} // RuntimeParams
} // Schematyc

#include "CrySchematyc/Runtime/RuntimeParams.inl"
