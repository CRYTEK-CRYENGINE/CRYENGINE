// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CQueryBlueprintLibrary
		//
		//===================================================================================

		class CQueryBlueprintLibrary : public IQueryBlueprintLibrary
		{
		public:
			explicit                                              CQueryBlueprintLibrary();

			// IQueryBlueprintLibrary
			virtual ELoadAndStoreResult                           LoadAndStoreQueryBlueprint(ELoadAndStoreOverwriteBehavior overwriteBehavior, DataSource::IQueryBlueprintLoader& loader, Shared::IUqsString& error) override;
			virtual bool                                          RemoveStoredQueryBlueprint(const char* szQueryBlueprintName, Shared::IUqsString& error) override;
			virtual CQueryBlueprintID                             FindQueryBlueprintIDByName(const char* szQueryBlueprintName) const override;
			virtual const IQueryBlueprint*                        GetQueryBlueprintByID(const CQueryBlueprintID& blueprintID) const override;
			virtual size_t                                        GetQueryBlueprintCount() const override;
			virtual CQueryBlueprintID                             GetQueryBlueprintID(size_t index) const override;
			// ~IQueryBlueprintLibrary

			std::shared_ptr<const CQueryBlueprint>                GetQueryBlueprintByIDInternal(const CQueryBlueprintID& blueprintID) const;
			void                                                  PrintToConsole(CLogger& logger) const;

		private:
			                                                      UQS_NON_COPYABLE(CQueryBlueprintLibrary);

		private:
			std::vector<std::shared_ptr<CQueryBlueprint>>                    m_queryBlueprintsVector;  // "master" list of blueprints; whenever a blueprints gets removed, its shared_ptr will simply get reset to nullptr
			std::map<string, CQueryBlueprintID, stl::less_stricmp<string>>   m_queryBlueprintsMap;     // key = blueprint name, value = index into m_queryBlueprints
		};

	}
}
