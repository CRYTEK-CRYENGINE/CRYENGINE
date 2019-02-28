// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CStartupConsistencyChecker
		//
		// - should be called once on game startup after all factories have been registered
		//   to see if any obvious problem exists before starting queries
		// - tries to detect issues like missing item-factories that some generators or functions might rely on
		// - runtime issues (like missing/bad runtime-params) can NOT be detected by this facility
		// - also, syntax errors in query-blueprints will not be checked here (it's the responsibility of whoever fills the IQueryBlueprintLibrary to check for such errors)
		//
		//===================================================================================

		class CStartupConsistencyChecker
		{
		public:
			void                    CheckForConsistencyErrors();
			size_t                  GetErrorCount() const;
			const char*             GetError(size_t index) const;

		private:
			template <class TFactoryDB>
			void                    CheckFactoryDatabaseConsistency(const TFactoryDB& factoryDB, const char* szFactoryDatabaseNameForErrorMessages);

			void                    CheckInputParametersConsistency(const Client::IInputParameterRegistry& registry, const char* szErrorMessagePrefix);

#if UQS_SCHEMATYC_SUPPORT
			void                    CheckItemConvertersConsistency(const Client::IItemConverterCollection& itemConverters, const char* szItemFactoryNameForErrorMessages, std::set<CryGUID>& guidsInUse);
#endif

		private:
			std::vector<string>     m_errors;
		};

	}
}
