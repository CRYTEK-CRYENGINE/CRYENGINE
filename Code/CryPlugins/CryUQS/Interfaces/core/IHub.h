// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// EHubEvent
		//
		//===================================================================================

		enum class EHubEvent
		{
			RegisterYourFactoriesNow,
			LoadQueryBlueprintLibrary
		};

		//===================================================================================
		//
		// IHubEventListener
		//
		//===================================================================================

		struct IHubEventListener
		{
			virtual           ~IHubEventListener() {}
			virtual void      OnUQSHubEvent(EHubEvent ev) = 0;
		};

		//===================================================================================
		//
		// EHubOverrideFlags
		//
		// - specifies what parts of the IHub the game wants to customize, rather than having them run automatically through the CryPlugin system
		// - the according functionality/subsystem will then have to be called by the game (unless intending to bypass it)
		// - in other words: for each bit that is set, the game should then call the according functionality on its own
		//
		//===================================================================================

		enum class EHubOverrideFlags
		{
			InstantiateStdLibFactories       = BIT(0),
			InstallDatasourceAndLoadLibrary  = BIT(1),
			CallUpdate                       = BIT(2),
			CallUpdateDebugRendering3D       = BIT(3),
		};

		//===================================================================================
		//
		// IHub
		//
		//===================================================================================

		struct IHub
		{
			virtual                                                    ~IHub() {}
			virtual void                                               RegisterHubEventListener(IHubEventListener* pListener) = 0;
			virtual void                                               Update() = 0;
			virtual CEnumFlags<EHubOverrideFlags>&                     GetOverrideFlags() = 0;
			virtual IQueryFactoryDatabase&                             GetQueryFactoryDatabase() = 0;
			virtual IItemFactoryDatabase&                              GetItemFactoryDatabase() = 0;
			virtual IFunctionFactoryDatabase&                          GetFunctionFactoryDatabase() = 0;
			virtual IGeneratorFactoryDatabase&                         GetGeneratorFactoryDatabase() = 0;
			virtual IInstantEvaluatorFactoryDatabase&                  GetInstantEvaluatorFactoryDatabase() = 0;
			virtual IDeferredEvaluatorFactoryDatabase&                 GetDeferredEvaluatorFactoryDatabase() = 0;
			virtual IScoreTransformFactoryDatabase&                    GetScoreTransformFactoryDatabase() = 0;
			virtual IQueryBlueprintLibrary&                            GetQueryBlueprintLibrary() = 0;
			virtual IQueryManager&                                     GetQueryManager() = 0;
			virtual IQueryHistoryManager&                              GetQueryHistoryManager() = 0;
			virtual IUtils&                                            GetUtils() = 0;
			virtual IEditorService&                                    GetEditorService() = 0;
			virtual IItemSerializationSupport&                         GetItemSerializationSupport() = 0;
			virtual ISettingsManager&                                  GetSettingsManager() = 0;

			// TODO pavloi 2016.04.07: maybe editor library provider doesn't really belong here.
			// Instead, we should use other means to pass pointer from game to editor (like, register
			// provider as game extension CRYINTERFACE and query it by GUID - same as Hub).
			virtual DataSource::IEditorLibraryProvider*                GetEditorLibraryProvider() = 0;
			virtual void                                               SetEditorLibraryProvider(DataSource::IEditorLibraryProvider* pProvider) = 0;
		};

	}
}
