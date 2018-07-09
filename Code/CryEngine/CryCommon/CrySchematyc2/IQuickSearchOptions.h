// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/BasicTypes.h"

namespace Schematyc2
{
	struct IQuickSearchOptions
	{
		virtual ~IQuickSearchOptions() {}

		virtual const char* GetToken() const = 0;
		virtual size_t GetCount() const = 0;
		virtual const char* GetLabel(size_t optionIdx) const = 0;
		virtual const char* GetDescription(size_t optionIdx) const = 0;
		virtual const char* GetWikiLink(size_t optionIdx) const = 0;
	};

	namespace QuickSearchUtils
	{
		inline size_t FindOption(const IQuickSearchOptions& options, const char* szLabel)
		{
			CRY_ASSERT(szLabel);
			if(szLabel)
			{
				for(size_t optionIdx = 0, optionCount = options.GetCount(); optionIdx < optionCount; ++ optionIdx)
				{
					if(strcmp(szLabel, options.GetLabel(optionIdx)) == 0)
					{
						return optionIdx;
					}
				}
			}
			return INVALID_INDEX;
		}
	}

	struct SQuickSearchOption
	{
		inline SQuickSearchOption(const IQuickSearchOptions& _options, const size_t _optionIdx = INVALID_INDEX)
			: options(_options)
			, optionIdx(_optionIdx)
		{}

		inline const char* c_str() const
		{
			return optionIdx < options.GetCount() ? options.GetLabel(optionIdx) : "";
		}

		inline void operator = (const char* szLabel)
		{
			optionIdx = QuickSearchUtils::FindOption(options, szLabel);
		}

		inline void Serialize(Serialization::IArchive& archive)
		{
			archive(optionIdx, "optionIdx");
		}

		const IQuickSearchOptions& options;
		size_t                     optionIdx;
	};
}
