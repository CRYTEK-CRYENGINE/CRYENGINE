// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// IQueryFactory
		//
		// - this class is basically there for telling the editor how many children a specific query can have and such
		// - it's *not* there to actually create a query instance from outside the core!
		// - the core itself will create query instances
		//
		//===================================================================================

		struct IQueryFactory
		{
			static const size_t           kUnlimitedChildren = (size_t)-1;

			virtual                       ~IQueryFactory() {}
			virtual const char*           GetName() const = 0;
			virtual const CryGUID&        GetGUID() const = 0;
			virtual const char*           GetDescription() const = 0;
			virtual bool                  SupportsParameters() const = 0;
			virtual bool                  RequiresGenerator() const = 0;
			virtual bool                  SupportsEvaluators() const = 0;
			virtual size_t                GetMinRequiredChildren() const = 0;
			virtual size_t                GetMaxAllowedChildren() const = 0;
		};

		//===================================================================================
		//
		// IQueryFactoryDatabase
		//
		//===================================================================================

		typedef IFactoryDatabase<IQueryFactory> IQueryFactoryDatabase;

	}
}
