// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{
		namespace Internal
		{

			//===================================================================================
			//
			// SContainedTypeRetriever
			//
			// - this is a helper for CFunctionFactory<>::GetContainedType()
			// - it's used to retrieve the underlying type of items that have been shuttled from one query to another
			// - also see Client::Internal::CFunc_ShuttledItems<> and observe that it's making use of CItemListProxy_Readable<>, hence the template specialization for it
			//
			//===================================================================================

			template <class TItem>
			struct SContainedTypeRetriever
			{
				static const Shared::CTypeInfo* GetTypeInfo()
				{
					return nullptr;
				}
			};

			template <class TItem>
			struct SContainedTypeRetriever<CItemListProxy_Readable<TItem>>
			{
				static const Shared::CTypeInfo* GetTypeInfo()
				{
					static const Shared::CTypeInfo& type = Shared::SDataTypeHelper<TItem>::GetTypeInfo();
					return &type;
				}
			};

		}
	}
}
