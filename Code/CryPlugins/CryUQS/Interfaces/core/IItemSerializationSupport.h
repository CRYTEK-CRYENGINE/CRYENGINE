// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// IItemSerializationSupport
		//
		// - provides a support to serialize items to textual representation and deserialize them back
		//
		//===================================================================================

		struct IItemSerializationSupport
		{
			virtual                          ~IItemSerializationSupport() {}
			
			virtual bool                     DeserializeItemFromCStringLiteral(void* pOutItem, const Client::IItemFactory& itemFactory, const char* szItemLiteral, Shared::IUqsString* pErrorMessage) const = 0;
			virtual bool                     DeserializeItemIntoDictFromCStringLiteral(Shared::IVariantDict& out, const char* szKey, Client::IItemFactory& itemFactory, const char* szItemLiteral, Shared::IUqsString* pErrorMessage) const = 0;

			virtual bool                     SerializeItemToStringLiteral(const void* pItem, const Client::IItemFactory& itemFactory, Shared::IUqsString& outString) const = 0;
		};

	}
}
