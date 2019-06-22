// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "CryAssert.h"
#include <CrySystem/ISystem.h>
#include <CryThreading/CryAtomics.h>

#if defined(USE_CRY_ASSERT)

// buffer to place the formatted assert message in, allows easy reuse
// is filled by CryAssertFormat() before calling into the platform's CryAssert() implementation
static thread_local char tls_szAssertMessage[256] = { 0 };

// platform-specific CryAssert() implementations
// return value says whether the system should do a debug break
bool CryAssert(const char* szCondition, const char* szFile, unsigned int line, bool* pIgnore);
#if CRY_PLATFORM_DURANGO
#	include <CryAssert/CryAssert_Durango.h>
#elif CRY_PLATFORM_MAC
#	include <CryAssert/CryAssert_Mac.h>
#elif CRY_PLATFORM_IOS
#	include <CryAssert/CryAssert_iOS.h>
#elif CRY_PLATFORM_ANDROID
#	include <CryAssert/CryAssert_Android.h>
#elif CRY_PLATFORM_LINUX
#	include <CryAssert/CryAssert_Linux.h>
#elif CRY_PLATFORM_WINDOWS
#	include <CryAssert/CryAssert_Windows.h>
#elif CRY_PLATFORM_ORBIS
#	include <CryAssert/CryAssert_Orbis.h>
#else
	// Pull in system assert macro
#	include <assert.h>

bool CryAssert(const char*, const char*, unsigned int, bool*)
{
	assert(false && "CryAssert not implemented");
	return true;
}

// Restore previous assert definition
#	include "CryAssert.h"
#endif

const char* GetCryModuleName(uint cryModuleId)
{
	switch (cryModuleId)
	{
	case eCryM_3DEngine:                 return "Cry3DEngine";
	case eCryM_GameFramework:            return "CryAction";
	case eCryM_AISystem:                 return "CryAISystem";
	case eCryM_Animation:                return "CryAnimation";
	case eCryM_DynamicResponseSystem:    return "CryDynamicResponseSystem";
	case eCryM_EntitySystem:             return "CryEntitySystem";
	case eCryM_Font:                     return "CryFont";
	case eCryM_Input:                    return "CryInput";
	case eCryM_Movie:                    return "CryMovie";
	case eCryM_Network:                  return "CryNetwork";
	case eCryM_Physics:                  return "CryPhysics";
	case eCryM_ScriptSystem:             return "CryScriptSystem";
	case eCryM_AudioSystem:              return "CryAudioSystem";
	case eCryM_System:                   return "CrySystem";
	case eCryM_LegacyGameDLL:            return "CryGame";
	case eCryM_Render:                   return "CryRenderer";
	case eCryM_Launcher:                 return "Launcher";
	case eCryM_Editor:                   return "Sandbox";
	case eCryM_LiveCreate:               return "CryLiveCreate";
	case eCryM_AudioImplementation:      return "CryAudioImplementation";
	case eCryM_MonoBridge:               return "CryMonoBridge";
	case eCryM_ScaleformHelper:          return "CryScaleformHelper";
	case eCryM_FlowGraph:                return "CryFlowGraph";
	case eCryM_Legacy:                   return "Legacy";
	case eCryM_EnginePlugin:             return "Engine Plugin";
	case eCryM_EditorPlugin:             return "Editor Plugin";
	case eCryM_Schematyc2:               return "Schematyc2";
	case eCryM_UniversalDebugRecordings: return "UniversalDebugRecordings";
	default:                             return "Unknown Module";
	}
}

namespace Cry {
	namespace Assert {

		bool AreAssertsEnabledForModule(uint32 moduleId)
		{
			return gEnv ? gEnv->assertSettings.disabledAssertModules[moduleId] == 0 : true;
		}

		void DisableAssertionsForModule(uint32 moduleId)
		{
			if (gEnv)
			{
				gEnv->assertSettings.disabledAssertModules.set(moduleId, true);
			}
		}

		bool IsInAssert()
		{
			return gEnv ? gEnv->assertSettings.isInAssertHandler : false;
		}
		
		bool IgnoreAllAsserts()
		{
			return gEnv ? gEnv->assertSettings.ignoreAllAsserts : false;
		}

		void IgnoreAllAsserts(bool ignoreAsserts)
		{
			if (gEnv)
			{
				gEnv->assertSettings.ignoreAllAsserts = ignoreAsserts;
			}
		}

		bool ShowDialogOnAssert()
		{
			return gEnv ? (!gEnv->bUnattendedMode && !gEnv->bTesting && gEnv->assertSettings.showAssertDialog) : true;
		}

		void ShowDialogOnAssert(bool showAssertDialogue)
		{
			if (gEnv)
			{
				gEnv->assertSettings.showAssertDialog = showAssertDialogue;
			}
		}

		bool LogAssertsAlways()
		{
			return gEnv ? gEnv->assertSettings.logAlways : false;
		}

		void LogAssertsAlways(bool logAlways)
		{
			if (gEnv)
			{
				gEnv->assertSettings.logAlways = logAlways;
			}
		}

		ELevel GetAssertLevel()
		{
			return gEnv ? gEnv->assertSettings.assertLevel : ELevel::Enabled;
		}

		bool IsAssertLevel(ELevel assertLevel)
		{
			return GetAssertLevel() == assertLevel;
		}

		void SetAssertLevel(ELevel assertLevel)
		{
			if (gEnv)
			{
				gEnv->assertSettings.assertLevel = assertLevel;
			}
		}

		CustomPreAssertCallback GetCustomPreAssertCallback()
		{
			return gEnv ? gEnv->assertSettings.customPreAssertCallback : nullptr;
		}

		bool SetCustomAssertCallback(CustomPreAssertCallback customPreAssertCallback)
		{
			if (gEnv)
			{
				gEnv->assertSettings.customPreAssertCallback = customPreAssertCallback;
				return true;
			}
			return false;
		}

		namespace Detail
		{
			void IsInAssert(bool bIsInAssertHandler)
			{
				if (gEnv)
				{
					gEnv->assertSettings.isInAssertHandler = bIsInAssertHandler;
				}
			}

			bool CryAssertIsEnabled()
			{
				const bool suppressGlobally = Cry::Assert::IsAssertLevel(ELevel::Disabled);
				const bool suppressedByUser = Cry::Assert::IgnoreAllAsserts();
#ifdef eCryModule
				const bool suppressedCurrentModule = !Cry::Assert::AreAssertsEnabledForModule(eCryModule);
#else
				const bool suppressedCurrentModule = false;
#endif

				return !(suppressGlobally || suppressedByUser || suppressedCurrentModule);
			}

			void CryAssertFormat(const char* szFormat, ...)
			{
				if (szFormat == NULL || szFormat[0] == '\0')
				{
					cry_fixed_size_strcpy(tls_szAssertMessage, "<no message was provided for this assert>");
				}
				else
				{
					va_list args;
					va_start(args, szFormat);
					cry_vsprintf(tls_szAssertMessage, szFormat, args);
					va_end(args);
				}
			}

			void CryLogAssert(const char* _pszCondition, const char* _pszFile, unsigned int _uiLine)
			{
				ISystem* const pSystem = GetISystem();
				if (!pSystem)
					return;

			#ifdef _RELEASE
				pSystem->WarningV(VALIDATOR_MODULE_UNKNOWN, VALIDATOR_ASSERT, 0, _pszFile, gs_szMessage, va_list());
			#else
				if (tls_szAssertMessage[0] == '\0')
				{
					pSystem->Warning(VALIDATOR_MODULE_UNKNOWN, VALIDATOR_ASSERT, 0, _pszFile, "Condition: %s [line %u]", _pszCondition, _uiLine);
				}
				else
				{
					pSystem->Warning(VALIDATOR_MODULE_UNKNOWN, VALIDATOR_ASSERT, 0, _pszFile, "%s (Condition: %s)[line %u]", tls_szAssertMessage, _pszCondition, _uiLine);
				}
			#endif
			}

			//////////////////////////////////////////////////////////////////////////
			NO_INLINE bool CryAssertHandler(SAssertData const& data, SAssertCond& cond)
			{
				bool retShouldDebugBreak = false;
				char simplifiedPathBuf[MAX_PATH];
				const char* finalSimplifiedPath = PathUtil::SimplifyFilePath(data.szFile, simplifiedPathBuf, MAX_PATH, PathUtil::ePathStyle_Native)
													? simplifiedPathBuf
													: data.szFile;

				if (Cry::Assert::LogAssertsAlways() || cond.bLogAssert || !cond.bIgnoreAssert)
				{
					// Lock - Ensure only one thread is asserting
					volatile int dummyLock = 0;
					volatile int* lock = gEnv ? &gEnv->assertSettings.lock : &dummyLock;
					CryWriteLock(lock);
				
					if (cond.bLogAssert || !cond.bIgnoreAssert)
					{
						if (CustomPreAssertCallback cb_preCustomAssert = Cry::Assert::GetCustomPreAssertCallback())
						{
							cb_preCustomAssert(cond.bLogAssert, cond.bIgnoreAssert, tls_szAssertMessage, finalSimplifiedPath, data.line);
						}
					}

					if (Cry::Assert::LogAssertsAlways() || cond.bLogAssert)
					{
						CryLogAssert(data.szExpression, finalSimplifiedPath, data.line);
						cond.bLogAssert = false;  // Just log assert the first time
					}

					if (!cond.bIgnoreAssert)
					{
						Cry::Assert::Detail::IsInAssert(true);
						retShouldDebugBreak = CryAssert(data.szExpression, finalSimplifiedPath, data.line, &cond.bIgnoreAssert);
						Cry::Assert::Detail::IsInAssert(false);
					}
					CryReleaseWriteLock(lock);
				}

				if (gEnv && gEnv->pSystem)
					gEnv->pSystem->OnAssert(data.szExpression, tls_szAssertMessage, finalSimplifiedPath, data.line);

				// When stepping out of the function we loose our assert lock.
				// Furthermore we are also outside of Cry::Assert::IsInAssert(true) when the CryDebugBreak() is triggered.
				// This is a penalty to take if we want the CryDebugBreak() to break on the line where the CRY_ASSERT macro resides.
				return retShouldDebugBreak;
			}
		} // namespace Detail
	}
}

#endif // defined(USE_CRY_ASSERT)
