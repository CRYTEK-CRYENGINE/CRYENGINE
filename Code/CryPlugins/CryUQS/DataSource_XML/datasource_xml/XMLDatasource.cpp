// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace DataSource_XML
	{

		CXMLDatasource* CXMLDatasource::s_pInstance;

		CXMLDatasource::CXMLDatasource()
		{
			assert(!s_pInstance);
			s_pInstance = this;
		}

		CXMLDatasource::~CXMLDatasource()
		{
			s_pInstance = nullptr;
		}

		void CXMLDatasource::SetupAndInstallInHub(Core::IHub& hub, const char* szLibraryRootPath, const char* szFileExtension /*= "uqs"*/)
		{
			REGISTER_COMMAND("UQS_XML_LoadQueryBlueprintLibrary", CmdLoadQueryBlueprintLibrary, 0, "Loads all query-blueprints from disk into memory.");

			//
			// install in hub
			//

			SLibraryConfig config;
			config.rootPath = szLibraryRootPath;

			Core::IQueryBlueprintLibrary& queryBlueprintLibrary = hub.GetQueryBlueprintLibrary();
			m_pLibraryLoader.reset(new CQueryBlueprintFileLibrary(config, queryBlueprintLibrary));

			hub.SetEditorLibraryProvider(m_pLibraryLoader.get());

			//
			// load all query-blueprints
			//

			EnumerateAndLoadAllQueryBlueprints();
		}

		void CXMLDatasource::EnumerateAndLoadAllQueryBlueprints()
		{
			assert(m_pLibraryLoader);

			m_pLibraryLoader->EnumerateAll();
			m_pLibraryLoader->LoadAllEnumerated();

			std::vector<string> errors;
			m_pLibraryLoader->GetErrors(errors);

			if (errors.empty())
			{
				CryLog("UQS CXMLDatasource: Successfully loaded all query-blueprints.");
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "UQS CXMLDatasource: %" PRISIZE_T " errors while loading all query-blueprints:", errors.size());
				for (const string& err : errors)
				{
					CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "UQS CXMLDatasource:   %s", err.c_str());
				}
			}
		}

		void CXMLDatasource::CmdLoadQueryBlueprintLibrary(IConsoleCmdArgs* pArgs)
		{
			if (s_pInstance && s_pInstance->m_pLibraryLoader)
			{
				// TODO: would be nice if individual query-blueprints could be reloaded as well (not just the whole library)
				s_pInstance->EnumerateAndLoadAllQueryBlueprints();
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "UQS CXMLDatasource::CmdLoadQueryBlueprintLibrary: the XML datasource has not been set up and installed yet");
			}

		}

	}
}
