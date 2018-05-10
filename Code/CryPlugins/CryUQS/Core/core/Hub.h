// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace DataSource_XML
	{
		class CXMLDatasource;
	}

	namespace Core
	{

		//===================================================================================
		//
		// CHub
		//
		//===================================================================================

		class CHub : public IHub, public ISystemEventListener
		{
		public:
			explicit                                                   CHub();
			                                                           ~CHub();

			// IHub
			virtual void                                               RegisterHubEventListener(IHubEventListener* pListener) override;
			virtual void                                               Update() override;
			virtual CEnumFlags<EHubOverrideFlags>&                     GetOverrideFlags() override;
			// the co-variant return types are intended to help getting around casting down along the inheritance hierarchy when double-dispatching is involved
			virtual QueryFactoryDatabase&                              GetQueryFactoryDatabase() override;
			virtual ItemFactoryDatabase&                               GetItemFactoryDatabase() override;
			virtual FunctionFactoryDatabase&                           GetFunctionFactoryDatabase() override;
			virtual GeneratorFactoryDatabase&                          GetGeneratorFactoryDatabase() override;
			virtual InstantEvaluatorFactoryDatabase&                   GetInstantEvaluatorFactoryDatabase() override;
			virtual DeferredEvaluatorFactoryDatabase&                  GetDeferredEvaluatorFactoryDatabase() override;
			virtual ScoreTransformFactoryDatabase&                     GetScoreTransformFactoryDatabase() override;
			virtual CQueryBlueprintLibrary&                            GetQueryBlueprintLibrary() override;
			virtual CQueryManager&                                     GetQueryManager() override;
			virtual CQueryHistoryManager&                              GetQueryHistoryManager() override;
			virtual CUtils&                                            GetUtils() override;
			virtual CEditorService&                                    GetEditorService() override;
			virtual CItemSerializationSupport&                         GetItemSerializationSupport() override;
			virtual CSettingsManager&                                  GetSettingsManager() override;
			virtual DataSource::IEditorLibraryProvider*                GetEditorLibraryProvider() override;
			virtual void                                               SetEditorLibraryProvider(DataSource::IEditorLibraryProvider* pProvider) override;
			// ~IHub

			bool                                                       HaveConsistencyChecksBeenDoneAlready() const;
			void                                                       AutomaticUpdateBegin();
			void                                                       AutomaticUpdateEnd();

#if UQS_SCHEMATYC_SUPPORT
			static void                                                OnRegisterSchematycEnvPackage(Schematyc::IEnvRegistrar& registrar);  // gcc-4.9 requires this method to be public when registering as a callback
			static CryGUID                                             GetSchematycPackageGUID() { return "5ee1079d-1b49-41c0-856d-6521d8758bd6"_cry_guid; }
#endif

		private:
			                                                           UQS_NON_COPYABLE(CHub);

			virtual void                                               OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;

			void                                                       SendHubEventToAllListeners(EHubEvent ev);

			static void                                                CmdListFactoryDatabases(IConsoleCmdArgs* pArgs);
			static void                                                CmdListQueryBlueprintLibrary(IConsoleCmdArgs* pArgs);
			static void                                                CmdListRunningQueries(IConsoleCmdArgs* pArgs);
			static void                                                CmdDumpQueryHistory(IConsoleCmdArgs* pArgs);
			static void                                                CmdLoadQueryHistory(IConsoleCmdArgs* pArgs);
			static void                                                CmdClearLiveQueryHistory(IConsoleCmdArgs* pArgs);
			static void                                                CmdClearDeserializedQueryHistory(IConsoleCmdArgs* pArgs);

		private:
			CEnumFlags<EHubOverrideFlags>                              m_overrideFlags;
			std::list<IHubEventListener*>                              m_eventListeners;
			bool                                                       m_bConsistencyChecksDoneAlready;
			std::unique_ptr<DataSource_XML::CXMLDatasource>            m_pXmlDatasource;
			bool                                                       m_bAutomaticUpdateInProgress;
			QueryFactoryDatabase                                       m_queryFactoryDatabase;
			ItemFactoryDatabase                                        m_itemFactoryDatabase;
			FunctionFactoryDatabase                                    m_functionFactoryDatabase;
			GeneratorFactoryDatabase                                   m_generatorFactoryDatabase;
			InstantEvaluatorFactoryDatabase                            m_instantEvaluatorFactoryDatabase;
			DeferredEvaluatorFactoryDatabase                           m_deferredEvaluatorFactoryDatabase;
			ScoreTransformFactoryDatabase                              m_scoreTransformFactoryDatabase;
			CQueryBlueprintLibrary                                     m_queryBlueprintLibrary;
			CQueryHistoryManager                                       m_queryHistoryManager;
			CQueryHistoryInGameGUI                                     m_queryHistoryInGameGUI;
			CQueryManager                                              m_queryManager;
			CEditorService                                             m_editorService;
			CItemSerializationSupport                                  m_itemSerializationSupport;
			CSettingsManager                                           m_settingsManager;
			DataSource::IEditorLibraryProvider*                        m_pEditorLibraryProvider;
			CUtils                                                     m_utils;
		};

		// - this gets set to a valid instance in CHub::CHub() and reset in CHub::~CHub()
		// - only one instance can exist at a time (or an assert() will fail)
		extern CHub*   g_pHub;

	}
}
