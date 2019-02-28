// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

#if UQS_SCHEMATYC_SUPPORT

namespace UQS
{
	namespace Client
	{

		//===================================================================================
		//
		// IItemConverter
		//
		//===================================================================================

		struct IItemConverter
		{
			virtual                             ~IItemConverter() {}
			virtual const char*                 GetFromName() const = 0;   // just a bit more readable type name since CTypeInfo::name() may come up with funky names when C++ templates are involved
			virtual const char*                 GetToName() const = 0;     // ditto
			virtual const Shared::CTypeInfo&    GetFromItemType() const = 0;
			virtual const Shared::CTypeInfo&    GetToItemType() const = 0;
			virtual void                        ConvertItem(const void* pFromItem, void* pToItem) const = 0;
			virtual const CryGUID&              GetGUID() const = 0;
		};

		//===================================================================================
		//
		// IItemConverterCollection
		//
		//===================================================================================

		struct IItemConverterCollection
		{
			virtual                             ~IItemConverterCollection() {}
			virtual size_t                      GetItemConverterCount() const = 0;
			virtual const IItemConverter&       GetItemConverter(size_t index) const = 0;
		};

	}
}

#endif // UQS_SCHEMATYC_SUPPORT
