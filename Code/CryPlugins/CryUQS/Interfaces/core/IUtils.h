// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// IUtils
		//
		//===================================================================================

		struct IUtils
		{
			virtual                          ~IUtils() {}
			virtual Client::IItemFactory*    FindItemFactoryByType(const Shared::CTypeInfo& type) const = 0;
			virtual const IQueryFactory&     GetDefaultQueryFactory() const = 0;
		};

	}
}
