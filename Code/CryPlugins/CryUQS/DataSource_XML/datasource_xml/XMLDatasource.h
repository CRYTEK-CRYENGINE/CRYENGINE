// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace DataSource_XML
	{

		//===================================================================================
		//
		// CXMLDatasource
		//
		// - high-level class for the game to be used out-of-the-box
		// - it's responsible for setting up the XML datasource and providing the library loader for the editor
		//
		//===================================================================================

		class CXMLDatasource
		{
		public:
			explicit                                      CXMLDatasource();
			                                              ~CXMLDatasource();
			void                                          SetupAndInstallInHub(Core::IHub& hub, const char* szLibraryRootPath, const char* szFileExtension = "uqs");

		private:
			                                              UQS_NON_COPYABLE(CXMLDatasource);

			void                                          EnumerateAndLoadAllQueryBlueprints();
			static void                                   CmdLoadQueryBlueprintLibrary(IConsoleCmdArgs* pArgs);

		private:
			std::unique_ptr<CQueryBlueprintFileLibrary>   m_pLibraryLoader;
			static CXMLDatasource*                        s_pInstance;
		};

	}
}
