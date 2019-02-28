// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QueryBlueprintFileLibrary.h"
#include "QueryBlueprintSaver_XML.h"
#include <CrySystem/File/ICryPak.h>

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

//#pragma optimize("", off)
//#pragma inline_depth(0)

namespace UQS
{
	namespace DataSource_XML
	{
		CQueryBlueprintFileLibrary::CQueryBlueprintFileLibrary(const SLibraryConfig& config, Core::IQueryBlueprintLibrary& blueprintLibrary)
			: m_config(config)
			, m_blueprintLibrary(blueprintLibrary)
		{
			m_config.rootPath = PathUtil::AddSlash(m_config.rootPath);
			if (m_config.extension.empty())
			{
				m_config.extension = "uqs";
			}
		}

		void CQueryBlueprintFileLibrary::EnumerateAll()
		{
			EnumerateQueriesInFolder(m_config.rootPath.c_str());
		}

		void CQueryBlueprintFileLibrary::LoadAllEnumerated()
		{
			for (auto& nameRecordPair : m_queriesMap)
			{
				LoadRecord(nameRecordPair.first.c_str(), nameRecordPair.second, m_blueprintLibrary);
			}
		}

		void CQueryBlueprintFileLibrary::GetErrors(std::vector<string>& errors) const
		{
			for (const auto& nameQueryPair : m_queriesMap)
			{
				errors.insert(errors.end(), nameQueryPair.second.errors.cbegin(), nameQueryPair.second.errors.cend());
			}
		}

		void CQueryBlueprintFileLibrary::EnumerateQueriesInFolder(const char* szFolderPath)
		{
			// NOTE pavloi 2016.04.07: copy of CDatabase::LoadRecordsInFolder from descriptor database code

			const CryPathString folder = PathUtil::AddSlash(szFolderPath);
			const CryPathString search = PathUtil::Make(folder, CryPathString("*.*"));
			CryPathString subName;
			CryPathString fileName;

			ICryPak *pPak = gEnv->pCryPak;

			_finddata_t fd;
			intptr_t handle = pPak->FindFirst(search.c_str(), &fd);

			if (handle == -1)
				return;

			do
			{
				if (!strcmp(fd.name, ".") || !strcmp(fd.name, ".."))
					continue;

				const CryPathString fdName(fd.name);

				if (fd.attrib & _A_SUBDIR)
				{
					subName = PathUtil::Make(folder, PathUtil::AddSlash(fdName));
					EnumerateQueriesInFolder(subName.c_str());
					continue;
				}

				if (PathUtil::GetExt(fdName) != m_config.extension)
				{
					CryLog("[UQS] CQueryBlueprintFileLibrary: skipping file '%s%s': wrong extension",
						folder.c_str(), fdName.c_str());
					continue;
				}

				fileName = PathUtil::Make(folder, fdName);

				CreateRecordFromFile(fileName.c_str());

			} while (pPak->FindNext(handle, &fd) >= 0);
		}

		void CQueryBlueprintFileLibrary::CreateRecordFromFile(const CryPathString& fileName)
		{
			stack_string queryRecordName;
			GetQueryRecordNameFromFilePath(fileName, queryRecordName);

			SanitizeQueryName(queryRecordName);
			stack_string error;
			if (!ValidateQueryName(queryRecordName, error))
			{
				CryLog("[UQS] CQueryBlueprintFileLibrary: failed to load file '%s' for query name '%s' - name validation failed: %s",
					fileName.c_str(), queryRecordName.c_str(), error.c_str());
				return;
			}

			auto iter = m_queriesMap.find(queryRecordName);
			if (iter == m_queriesMap.end())
			{
				SQueryRecord record;
				record.filePath = string(fileName);
				record.bLoaded = false;

				m_queriesMap[queryRecordName] = std::move(record);

				CryLog("[UQS] CQueryBlueprintFileLibrary: loaded file '%s' for query name '%s'",
					fileName.c_str(), queryRecordName.c_str());
			}
		}

		void CQueryBlueprintFileLibrary::GetQueryRecordNameFromFilePath(const CryPathString& fileName, stack_string& outName) const
		{
			outName = fileName;
			outName.replace(m_config.rootPath.c_str(), "");
			PathUtil::RemoveExtension(outName);
		}

		void CQueryBlueprintFileLibrary::BuildFilePathFromQueryRecordName(const stack_string& queryName, string& outFilePath) const
		{
			outFilePath = PathUtil::Make(m_config.rootPath, string(queryName), m_config.extension);
		}

		void CQueryBlueprintFileLibrary::SanitizeQueryName(stack_string& queryName) const
		{
			queryName.Trim();
		}

		bool CQueryBlueprintFileLibrary::ValidateQueryName(const stack_string& queryName, stack_string& error) const
		{
			if (queryName.empty())
			{
				error.Format("query name is empty");
				return false;
			}

			// Query name doesn't end with empty "file name", for example "folder/"
			{
				stack_string lastPartOfName = PathUtil::GetFile(queryName.c_str());
				if (lastPartOfName.empty())
				{
					error.Format("last part of query name is empty");
					return false;
				}
			}

			// restrict allowed characters to prevent issues with actual file pathes
			{
				const char* szAllowedCharacters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-/";
				const stack_string::size_type disallowedCharacterPos = queryName.find_first_not_of(szAllowedCharacters);
				if (disallowedCharacterPos != stack_string::npos)
				{
					error.Format("query name contains disallowed character '%c'. Allowed characters are letters 'A-Z a-z', numbers '0-9', underscore '_', dash '-' and slash '/'.",
						queryName[disallowedCharacterPos]);
					return false;
				}
			}

			return true;
		}

		void CQueryBlueprintFileLibrary::LoadRecord(const char* szName, SQueryRecord& record, Core::IQueryBlueprintLibrary& outLibrary)
		{
			// clear errors from a previous call
			record.errors.clear();

			// Load query
			std::shared_ptr<CXMLDataErrorCollector> dataErrorCollector(new CXMLDataErrorCollector);
			CQueryBlueprintLoader_XML loader(szName, record.filePath.c_str(), dataErrorCollector);
			Shared::CUqsString error;

			const Core::IQueryBlueprintLibrary::ELoadAndStoreOverwriteBehavior overwriteBehavior =
				Core::IQueryBlueprintLibrary::ELoadAndStoreOverwriteBehavior::OverwriteExisting;
			const Core::IQueryBlueprintLibrary::ELoadAndStoreResult result =
				outLibrary.LoadAndStoreQueryBlueprint(overwriteBehavior, loader, error);

			switch (result)
			{
			case Core::IQueryBlueprintLibrary::ELoadAndStoreResult::StoredFromScratch:
			case Core::IQueryBlueprintLibrary::ELoadAndStoreResult::OverwrittenExistingOne:
			case Core::IQueryBlueprintLibrary::ELoadAndStoreResult::PreservedExistingOne:
				record.bLoaded = true;
				break;

			case Core::IQueryBlueprintLibrary::ELoadAndStoreResult::ExceptionOccurred:
				record.errors.push_back(error.c_str());
				for (size_t i = 0, numXMLErrors = dataErrorCollector->GetErrorCount(); i < numXMLErrors; ++i)
				{
					record.errors.push_back(dataErrorCollector->GetError(i));
				}
				// preserve the record.bLoaded flag because the query blueprint might reside in the library already from a previous successful load attempt
				break;

			default:
				CRY_ASSERT(false);
			}
		}

		void CQueryBlueprintFileLibrary::GetQueriesList(IListQueriesVisitor& visitor)
		{
			for (const auto& nameQueryPair : m_queriesMap)
			{
				visitor.Visit(nameQueryPair.first.c_str(), nameQueryPair.second.filePath.c_str());
			}
		}

		bool CQueryBlueprintFileLibrary::LoadTextualQueryBlueprint(const char* szQueryName, Core::ITextualQueryBlueprint& outBlueprint, Shared::IUqsString& error)
		{
			auto iter = m_queriesMap.find(szQueryName);
			if (iter == m_queriesMap.end())
			{
				error.Format("Query '%s' is not among the enumerated records.", szQueryName);
				return false;
			}

			const SQueryRecord& record = iter->second;

			std::shared_ptr<CXMLDataErrorCollector> dataErrorCollector(new CXMLDataErrorCollector);
			CQueryBlueprintLoader_XML loader(szQueryName, record.filePath.c_str(), dataErrorCollector);
			return loader.LoadTextualQueryBlueprint(outBlueprint, error);
		}

		bool CQueryBlueprintFileLibrary::SaveTextualQueryBlueprint(const char* szQueryName, const Core::ITextualQueryBlueprint& blueprintToSave, Shared::IUqsString& error)
		{
			auto iter = m_queriesMap.find(szQueryName);
			if (iter == m_queriesMap.end())
			{
				error.Format("Could not find the query blueprint '%s' among the enumerated records.", szQueryName);
				return false;
			}

			SQueryRecord& record = iter->second;

			CQueryBlueprintSaver_XML saver(record.filePath.c_str());
			const bool bResult = saver.SaveTextualQueryBlueprint(blueprintToSave, error);

			if (bResult)
			{
				// TODO pavloi 2016.04.08: we don't really need to load blueprint back from a file.
				// Write a way load new blueprint from passed textual blueprint.
				LoadRecord(szQueryName, record, m_blueprintLibrary);
			}

			return bResult;
		}

		bool CQueryBlueprintFileLibrary::CreateNewTextualQueryBlueprint(const char* szQueryName, Core::ITextualQueryBlueprint& outBlueprint, Shared::IUqsString& error)
		{
			stack_string name(szQueryName);
			SanitizeQueryName(name);

			stack_string nameValidationError;
			if (!ValidateQueryName(name, nameValidationError))
			{
				error.Format("Query with name '%s' cannot be created: %s", name.c_str(), nameValidationError.c_str());
				return false;
			}

			auto iter = m_queriesMap.find(name.c_str());
			if (iter != m_queriesMap.end())
			{
				error.Format("Query with name '%s' already exists.", name.c_str());
				return false;
			}

			outBlueprint.SetName(name.c_str());

			SQueryRecord queryRecord;
			queryRecord.bLoaded = false;
			BuildFilePathFromQueryRecordName(name, queryRecord.filePath);

			CryLog("[UQS] CQueryBlueprintFileLibrary: created record for query name '%s' (file '%s')",
				name.c_str(), queryRecord.filePath.c_str());

			m_queriesMap[name] = std::move(queryRecord);
			return true;
		}

		bool CQueryBlueprintFileLibrary::RemoveQueryBlueprint(const char* szQueryName, Shared::IUqsString& error)
		{
			auto iter = m_queriesMap.find(szQueryName);
			if (iter == m_queriesMap.end())
			{
				error.Format("Could not find the query blueprint '%s' among the enumerated records.", szQueryName);
				return false;
			}

			SQueryRecord& queryRecord = iter->second;

			// TODO pavloi 2016.07.04: remove blueprint only if it was loaded into m_blueprintLibrary.
			// queryRecord.bLoaded is not enough, because it is set to false after reloading error.
			{
				Shared::CUqsString err;
				if (m_blueprintLibrary.RemoveStoredQueryBlueprint(szQueryName, err))
				{
					CryLog("[UQS] CQueryBlueprintFileLibrary: removed query blueprint '%s' from core blueprints library", szQueryName);
				}
				else
				{
					CryLog("[UQS] CQueryBlueprintFileLibrary: unable to remove query blueprint '%s' from core blueprints library: %s",
						szQueryName, err.c_str());
				}
			}

			// TODO pavloi 2016.07.04: try to remove file only if we loaded from file or saved new blueprint to file.
			if (ICryPak* pPak = gEnv->pCryPak)
			{
				if (pPak->RemoveFile(queryRecord.filePath.c_str()))
				{
					CryLog("[UQS] CQueryBlueprintFileLibrary: removed query blueprint '%s' file at path '%s'",
						szQueryName, queryRecord.filePath.c_str());
				}
				else
				{
					CryLog("[UQS] CQueryBlueprintFileLibrary: unable to remove query blueprint '%s' file at path '%s'",
						szQueryName, queryRecord.filePath.c_str());
				}
			}

			m_queriesMap.erase(iter);
			return true;
		}

	} // namespace DataSource_XML
} // namespace UQS
