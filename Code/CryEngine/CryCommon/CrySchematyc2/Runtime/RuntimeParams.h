// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Make sure we can allocate on stack! Can we use CScratchPad to do this?
// #SchematycTODO : Do we need to be able to pass IAny data?
// #SchematycTODO : If we're going to be using this in other dlls we need to replace std::vector!

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/Any.h"
#include "CrySchematyc2/BasicTypes.h"

namespace Schematyc2
{
	class CRuntimeParams
	{
	private:

		struct SParamHeader
		{
			inline SParamHeader(uint32 _id = s_invalidId, uint32 _pos = s_invalidIdx)
				: id(_id)
				, pos(_pos)
			{}

			uint32 id;
			uint32 pos;
		};

		typedef std::vector<SParamHeader> ParamHeaders;
		typedef std::vector<IAnyPtr>      Data;

	public:

		inline CRuntimeParams() {}

		template <typename TYPE> inline bool Set(uint32 paramId, const TYPE& value)
		{
			const uint32 paramIdx = ParamIdToIdx(paramId);
			if(ParamIdToIdx(paramId) != s_invalidIdx)
			{
				const IAnyPtr& pValue = m_data[m_paramHeaders[paramIdx].pos];
				if(pValue->GetTypeInfo() != GetTypeInfo<TYPE>())
				{
					// Error : Type mismatch!!!
					return false;
				}
				*static_cast<TYPE*>(pValue->ToVoidPtr()) = value;
				return true;
			}
			else if(paramId != s_invalidId)
			{
				const uint32 pos = m_data.size();
				m_paramHeaders.push_back(SParamHeader(paramId, pos));
				m_data.push_back(MakeAnyShared(value));
				return true;
			}
			return false;
		}

		template <typename TYPE> inline bool Get(uint32 paramId, TYPE& value) const
		{
			const uint32 paramIdx = ParamIdToIdx(paramId);
			if(ParamIdToIdx(paramId) != s_invalidIdx)
			{
				const IAnyPtr& pValue = m_data[m_paramHeaders[paramIdx].pos];
				if(pValue->GetTypeInfo() != GetTypeInfo<TYPE>())
				{
					// Error : Type mismatch!!!
					return false;
				}
				value = *static_cast<const TYPE*>(pValue->ToVoidPtr());
				return true;
			}
			return false;
		}

	private:

		inline uint32 ParamIdToIdx(uint32 paramId) const
		{
			for(uint32 paramIdx = 0, paramCount = m_paramHeaders.size(); paramIdx < paramCount; ++ paramIdx)
			{
				if(m_paramHeaders[paramIdx].id == paramId)
				{
					return paramIdx;
				}
			}
			return s_invalidIdx;
		}

	private:

		ParamHeaders m_paramHeaders;
		Data         m_data;
	};
}
