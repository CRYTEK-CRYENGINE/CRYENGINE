// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace DataSource_XML
	{
		//===================================================================================
		//
		// SLibraryConfig
		//
		// Settings for CQueryBlueprintFileLibrary
		//
		//===================================================================================

		struct SLibraryConfig
		{
			string rootPath;
			string extension;
			// TODO pavloi 2016.04.08: maybe add a flag to remove records right after queries are loaded:
			// can be desired on release builds, where we don't need editor support.
		};

		//===================================================================================
		//
		// CQueryBlueprintFileLibrary
		//
		// A class to manage library of queries which are stored in xml files. Enforces some
		// assumptions on how queries are stored and used:
		// - all queries are located in SLibraryConfig::rootPath folder. There might be subfolders.
		// - one query per file.
		// - each query name is "subfolder/filename_without_extension".
		//
		// Instance of this class is owned by a game. UQS core doesn't create it, so particular
		// game can choose another storage schema.
		//
		// Pure game needs only a fraction of functionality of this class - enumerate files, read
		// textual query blueprints from files and load them into uqs core. Everything else is
		// to support the editor by providing an implementation of the IEditorLibraryProvider interface.
		//===================================================================================

		// TODO pavloi 2016.04.08: assumptions are not set in stone and should be discussed with designers
		// (like, should we store one or several queries per file, what would be naming convention in
		// such case; etc.).
		// TODO pavloi 2016.04.08: should we provide a way load/unload a set of queries (like level-specific
		// queries). Or should we keep this file as a basic implementation, which can be inherited or
		// completely replaced by a particular game?

		class CQueryBlueprintFileLibrary : public DataSource::IEditorLibraryProvider
		{
		private:
			struct SQueryRecord
			{
				string filePath;
				bool bLoaded;
				std::vector<string> errors;
			};

			typedef std::map<string, SQueryRecord, stl::less_stricmp<string>> QueryRecordsMap;

		public:
			explicit CQueryBlueprintFileLibrary(const SLibraryConfig& config, Core::IQueryBlueprintLibrary& blueprintLibrary);

			void EnumerateAll();
			void LoadAllEnumerated();
			void GetErrors(std::vector<string>& errors) const;   // returns errors of *all* records

		public:
			// IEditorLibraryProvider
			virtual void GetQueriesList(IListQueriesVisitor& callback) override;
			virtual bool LoadTextualQueryBlueprint(const char* szQueryName, Core::ITextualQueryBlueprint& outBlueprint, Shared::IUqsString& error) override;
			virtual bool SaveTextualQueryBlueprint(const char* szQueryName, const Core::ITextualQueryBlueprint& blueprintToSave, Shared::IUqsString& error) override;
			virtual bool CreateNewTextualQueryBlueprint(const char* szQueryName, Core::ITextualQueryBlueprint& outBlueprint, Shared::IUqsString& error) override;
			virtual bool RemoveQueryBlueprint(const char* szQueryName, Shared::IUqsString& error) override;
			// ~IEditorLibraryProvider

		private:

			void EnumerateQueriesInFolder(const char* szFolderPath);
			void CreateRecordFromFile(const CryPathString& fileName);

			void GetQueryRecordNameFromFilePath(const CryPathString& fileName, stack_string& outName) const;
			void BuildFilePathFromQueryRecordName(const stack_string& queryName, string& outFilePath) const;
			void SanitizeQueryName(stack_string& queryName) const;
			bool ValidateQueryName(const stack_string& queryName, stack_string& error) const;

			void LoadRecord(const char* szName, SQueryRecord& record, Core::IQueryBlueprintLibrary& outLibrary);

		private:

			QueryRecordsMap m_queriesMap;

			SLibraryConfig m_config;
			Core::IQueryBlueprintLibrary& m_blueprintLibrary;

			// TODO pavloi 2016.04.08: store loading errors?
		};

	} // namespace DataSource_XML
} // namespace UQS
