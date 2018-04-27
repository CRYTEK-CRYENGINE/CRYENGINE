// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// IQueryBlueprintLibrary
		//
		//===================================================================================

		struct IQueryBlueprintLibrary
		{
			enum class ELoadAndStoreOverwriteBehavior
			{
				OverwriteExisting,
				KeepExisting
			};

			enum class ELoadAndStoreResult
			{
				StoredFromScratch,              // there was no query blueprint with the same name in the library before
				OverwrittenExistingOne,         // there was a query blueprint with the same name already, but it got overwritten now
				PreservedExistingOne,           // there was a query blueprint with the same name already, and it was preserved
				ExceptionOccurred,              // the query blueprint could not be loaded from the data source at all, and hasn't therefore stored in the library
			};

			virtual                             ~IQueryBlueprintLibrary() {}
			virtual ELoadAndStoreResult         LoadAndStoreQueryBlueprint(ELoadAndStoreOverwriteBehavior overwriteBehavior, DataSource::IQueryBlueprintLoader& loader, Shared::IUqsString& error) = 0;
			virtual bool                        RemoveStoredQueryBlueprint(const char* szQueryBlueprintName, Shared::IUqsString& error) = 0;
			virtual CQueryBlueprintID           FindQueryBlueprintIDByName(const char* szQueryBlueprintName) const = 0;
			virtual const IQueryBlueprint*      GetQueryBlueprintByID(const CQueryBlueprintID& blueprintID) const = 0; // careful: the returned pointer might start to dangle once the library's content changes in any way
			virtual size_t                      GetQueryBlueprintCount() const = 0;
			virtual CQueryBlueprintID           GetQueryBlueprintID(size_t index) const = 0;
		};

	}
}
