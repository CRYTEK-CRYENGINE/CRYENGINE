// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client { struct IItemFactory; }

	namespace Shared
	{

		struct IVariantDict
		{
			virtual                             ~IVariantDict() {}
			virtual void                        AddOrReplace(const char* szKey, Client::IItemFactory& itemFactory, const void* pItemToClone) = 0;
			virtual void                        AddSelfToOtherAndReplace(IVariantDict& out) const = 0;
			virtual bool                        Exists(const char* szKey) const = 0;
			virtual Client::IItemFactory*       FindItemFactory(const char* szKey) const = 0;
			virtual bool                        FindItemFactoryAndObject(const char* szKey, Client::IItemFactory* &outItemItemFactory, void* &pOutObject) const = 0;
		};

	}
}
