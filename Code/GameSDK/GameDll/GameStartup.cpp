// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameStartup.h"
#include "Game.h"
#include "GameCVars.h"

#include <CryString/StringUtils.h>
#include <CryString/CryFixedString.h>
#include <CryCore/Platform/CryLibrary.h>
#include <CryCore/Platform/platform_impl.inl>
#include <CryNetwork/INetworkService.h>

#include <CryInput/IHardwareMouse.h>
#include <CrySystem/File/ICryPak.h>
#include <CrySystem/ILocalizationManager.h>
#include "Editor/GameRealtimeRemoteUpdate.h"
#include "Utility/StringUtils.h"

#include "Testing/AutoTester.h"
#include <CryThreading/IJobManager.h>

#include <CryThreading/IThreadManager.h>
#include <CryThreading/IThreadConfigManager.h>

#if ENABLE_AUTO_TESTER 
static CAutoTester s_autoTesterSingleton;
#endif

#ifdef __LINK_GCOV__
extern "C" void __gcov_flush(void);
#define GCOV_FLUSH __gcov_flush()
namespace
{
	static void gcovFlushUpdate()
	{
		static unsigned sCounter = 0;
		static const sInterval = 1000;

		if (++sCounter == sInterval)
		{
			__gcov_flush();
			sCounter = 0;
		}
	}
}
#define GCOV_FLUSH_UPDATE gcovFlushUpdate()
#else
#define GCOV_FLUSH ((void)0)
#define GCOV_FLUSH_UPDATE ((void)0)
#endif

#if CRY_PLATFORM_WINDOWS
bool g_StickyKeysStatusSaved = false;
STICKYKEYS g_StartupStickyKeys = {sizeof(STICKYKEYS), 0};
TOGGLEKEYS g_StartupToggleKeys = {sizeof(TOGGLEKEYS), 0};
FILTERKEYS g_StartupFilterKeys = {sizeof(FILTERKEYS), 0};

#endif

static void RestoreStickyKeys();

static void AllowAccessibilityShortcutKeys(bool bAllowKeys)
{
// disabling sticky keys for now since they are not being restored, atexit is not being called because the game process is killed by the system
//#if CRY_PLATFORM_WINDOWS
#if 0
	if(!g_StickyKeysStatusSaved)
	{
		SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &g_StartupStickyKeys, 0);
		SystemParametersInfo(SPI_GETTOGGLEKEYS, sizeof(TOGGLEKEYS), &g_StartupToggleKeys, 0);
		SystemParametersInfo(SPI_GETFILTERKEYS, sizeof(FILTERKEYS), &g_StartupFilterKeys, 0);
		g_StickyKeysStatusSaved = true;
		atexit(RestoreStickyKeys);
	}

	if(bAllowKeys)
	{
		// Restore StickyKeys/etc to original state and enable Windows key      
		SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &g_StartupStickyKeys, 0);
		SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &g_StartupToggleKeys, 0);
		SystemParametersInfo(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &g_StartupFilterKeys, 0);
	}
	else
	{
		STICKYKEYS skOff = g_StartupStickyKeys;
		skOff.dwFlags &= ~SKF_HOTKEYACTIVE;
		skOff.dwFlags &= ~SKF_CONFIRMHOTKEY; 
		SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &skOff, 0);

		TOGGLEKEYS tkOff = g_StartupToggleKeys;
		tkOff.dwFlags &= ~TKF_HOTKEYACTIVE;
		tkOff.dwFlags &= ~TKF_CONFIRMHOTKEY;
		SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &tkOff, 0);

		FILTERKEYS fkOff = g_StartupFilterKeys;
		fkOff.dwFlags &= ~FKF_HOTKEYACTIVE;
		fkOff.dwFlags &= ~FKF_CONFIRMHOTKEY;
		SystemParametersInfo(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &fkOff, 0);
	}
#endif
}
static void RestoreStickyKeys()
{
	AllowAccessibilityShortcutKeys(true);
}

#define EYEADAPTIONBASEDEFAULT		0.25f					// only needed for Crysis

#if CRY_PLATFORM_WINDOWS
void debugLogCallStack()
{
	// Print call stack for each find.
	const char *funcs[32];
	int nCount = 32;

	CryLogAlways( "    ----- CallStack () -----");
	gEnv->pSystem->debug_GetCallStack( funcs, nCount);
	for (int i = 1; i < nCount; i++) // start from 1 to skip this function.
	{
		CryLogAlways( "    %02d) %s",i,funcs[i] );
	}
}
#endif

void GameStartupErrorObserver::OnAssert(const char* condition, const char* message, const char* fileName, unsigned int fileLineNumber)
{
	if(!g_pGameCVars)
		return;

	if (g_pGameCVars->cl_logAsserts != 0)
		CryLogAlways("---ASSERT: condition:%s; message:%s; fileName:%s; line %d", condition, message, fileName, fileLineNumber);

#if CRY_PLATFORM_WINDOWS
	if (g_pGameCVars->cl_logAsserts > 1)
	{
		debugLogCallStack();
		CryLogAlways("----------------------------------------");
	}
#endif
}

void GameStartupErrorObserver::OnFatalError(const char* message)
{
	CryLogAlways("---FATAL ERROR: message:%s", message);

#if CRY_PLATFORM_WINDOWS
	gEnv->pSystem->debug_LogCallStack();
	CryLogAlways("----------------------------------------");
#endif
}

//////////////////////////////////////////////////////////////////////////

CGameStartup* CGameStartup::Create()
{
	static char buff[sizeof(CGameStartup)];
	return new (buff) CGameStartup();
}

CGameStartup::CGameStartup()
	:	m_pGame(nullptr),
		m_GameRef(&m_pGame),
		m_quit(false),
		m_pFramework(nullptr),
		m_fullScreenCVarSetup(false),
		m_nVOIPWasActive(-1)
{}

#define EngineStartProfiler(x)
#define InitTerminationCheck(x)

static inline void InlineInitializationProcessing(const char *sDescription)
{
	EngineStartProfiler( sDescription );
	InitTerminationCheck( sDescription );
	gEnv->pLog->UpdateLoadingScreen(0);
}

IGameRef CGameStartup::Init(SSystemInitParams &startupParams)
{
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "Game startup initialisation");

	IGameRef pOut = Reset(startupParams.pSystem);
	CRY_ASSERT(gEnv && GetISystem());

	m_pFramework = gEnv->pGameFramework;

	InlineInitializationProcessing("CGameStartup::Init");

	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	// Load thread config
	gEnv->pThreadManager->GetThreadConfigManager()->LoadConfig("%engine%/config/game.thread_config");

	const ICmdLineArg* pSvBind = GetISystem()->GetICmdLine()->FindArg(eCLAT_Pre, "sv_bind");
	IConsole* pConsole = GetISystem()->GetIConsole();
	if ((pSvBind != NULL) && (pConsole != NULL))
	{
		string command = pSvBind->GetName() + string(" ") + pSvBind->GetValue();
		pConsole->ExecuteString(command.c_str(), true, false);
	}

	GetISystem()->GetISystemEventDispatcher()->RegisterListener(this, "CGameStartup");

	// Creates and starts the realtime update system listener.
	if (GetISystem()->IsDevMode())
	{
		CGameRealtimeRemoteUpdateListener::GetGameRealtimeRemoteUpdateListener().Enable(true);
	}

	GCOV_FLUSH;

	GetISystem()->RegisterErrorObserver(&m_errorObsever);
	GetISystem()->RegisterWindowMessageHandler(this);

	InlineInitializationProcessing("CGameStartup::Init End");

	CryRegisterFlowNodes();

	assert(gEnv);
	PREFAST_ASSUME(gEnv);
	
	return pOut;
}

IGameRef CGameStartup::Reset(ISystem* pSystem)
{
	if (m_pGame)
	{
		m_pGame->Shutdown();
	}

	ModuleInitISystem(pSystem, "CryGame");
	const int wordSizeCGame = (sizeof(CGame) + sizeof(int) - 1) / sizeof(int); // Round up to next word
	static int pGameBuffer[wordSizeCGame];
	m_pGame = new ((void*)pGameBuffer) CGame();

	if (m_pGame->Init())
	{
		return m_GameRef;
	}

	return nullptr;
}

void CGameStartup::Shutdown()
{
	CryUnregisterFlowNodes();

#if CRY_PLATFORM_WINDOWS
	AllowAccessibilityShortcutKeys(true);
#endif

	GetISystem()->UnregisterErrorObserver(&m_errorObsever);
	GetISystem()->UnregisterWindowMessageHandler(this);

	if (m_pGame)
	{
		m_pGame->Shutdown();
		m_pGame = nullptr;
	}

#ifndef _LIB
	ICryFactoryRegistryImpl* pCryFactoryImpl = static_cast<ICryFactoryRegistryImpl*>(GetISystem()->GetCryFactoryRegistry());
	pCryFactoryImpl->UnregisterFactories(g_pHeadToRegFactories);
#endif

	GetISystem()->GetISystemEventDispatcher()->RemoveListener(this);

	/*delete this;*/
	this->~CGameStartup();
}

void CGameStartup::FullScreenCVarChanged( ICVar *pVar )
{
	if(gEnv->pSystem->GetISystemEventDispatcher())
	{
		gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_TOGGLE_FULLSCREEN, pVar->GetIVal(), 0);
	}
}

#if defined(RELEASE_SERVER_SECURITY)

#define TMP_CONFIG_LINE_BUFFER			(2048)
#define FILE_CHECK_BUFFER_SIZE			(32768)

#include <CrySystem/ZLib/IZLibCompressor.h>

bool ValidateFile(string path,string md5)
{
	bool bOk;
	// Try to open file on disk and hash it.
	FILE *file = fopen( path.c_str(),"rb" );
	if (file)
	{
		fseek( file,0,SEEK_END );
    unsigned int nFileSize = ftell(file);
		fseek( file,0,SEEK_SET );

		unsigned char *pBuf = (unsigned char*)malloc( FILE_CHECK_BUFFER_SIZE );
		if (!pBuf)
		{
			fclose(file);
			return false;
		}
																
		IZLibCompressor			*pZLib=GetISystem()->GetIZLibCompressor();

		char digest[16];
		SMD5Context context;
		string digestAsString;
		
		pZLib->MD5Init(&context);

		while (nFileSize)
		{
			unsigned int fetchLength=min(nFileSize,(unsigned int)FILE_CHECK_BUFFER_SIZE);

			if (fread( pBuf,fetchLength,1,file ) != 1)
			{
				free( pBuf );
				fclose(file);
				return false;
			}
			
			pZLib->MD5Update(&context,(const char*)pBuf,fetchLength);

			nFileSize-=fetchLength;
		}

		pZLib->MD5Final(&context,digest);

		digestAsString="";
		for (int a=0;a<16;a++)
		{
			string hex;
			hex.Format("%02x",(unsigned char)digest[a]);
			digestAsString.append(hex);
		}

		bOk = (digestAsString.compare(md5) == 0 );

		free( pBuf );
		fclose(file);

		return bOk;
	}

	return false;
}

bool PerformDedicatedInstallationSanityCheck()
{
	char tmpLineBuffer[TMP_CONFIG_LINE_BUFFER];
	string checksumPath = PathUtil::GetGameFolder() + "/Scripts/DedicatedConfigs/" + "check.txt";
	bool bOk=true;
	
	FILE *checksumFile = fopen(checksumPath.c_str(),"r");

	if (!checksumFile)
	{
		CryLogAlways("Failed to open validation configuration - Please check your install!");
		return false;
	}
	else
	{
		while (fgets(tmpLineBuffer,TMP_CONFIG_LINE_BUFFER-1,checksumFile))
		{
			string temp = tmpLineBuffer;
				
			string::size_type posEq = temp.find( "*", 0 );
			
			if (string::npos!=posEq)
			{
					string md5( temp, 0, posEq );
					string path( temp,posEq+1,temp.length());

					path.TrimRight(" \r\n");
					md5.TrimRight(" \r\n");

					if (!ValidateFile(path,md5))
					{
						CryLogAlways(path + " failed validation check - Please check your install! ("+md5+")");
						bOk=false;
					}
			}
		}

		fclose(checksumFile);
	}

	return bOk;
}
#endif

// If you have a valid RSA key set it here and set USE_RSA_KEY to 1
#define USE_RSA_KEY 0
#if USE_RSA_KEY
static unsigned char g_rsa_public_key_data[] =
{
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
#endif

const uint8* CGameStartup::GetRSAKey(uint32 *pKeySize) const
{
#if USE_RSA_KEY
	*pKeySize = sizeof(g_rsa_public_key_data);
	return g_rsa_public_key_data;
#else
	*pKeySize = 0;
	return NULL;
#endif
}

void CGameStartup::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_CHANGE_FOCUS:
		{
			#if CRY_PLATFORM_WINDOWS

			AllowAccessibilityShortcutKeys(wparam==0);
			
			#endif
		}
		break;
	case ESYSTEM_EVENT_LEVEL_LOAD_START:
		{			
			// For MP gamemodes set the correct sound parameter
			// Default to SP
			float sp_coop_mp = 0.0f;
			if ( gEnv->bMultiplayer )
			{
				sp_coop_mp = 2.0f;
			}
			//gEnv->pSoundSystem->SetGlobalParameter( "sp_coop_mp", sp_coop_mp );
			CryLog("sp_coop_mp set to %f", sp_coop_mp);
		}
		break;

	case ESYSTEM_EVENT_FAST_SHUTDOWN:
		m_quit = true;
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
#if CRY_PLATFORM_WINDOWS
bool CGameStartup::HandleMessage(CRY_HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *pResult)
{
	switch(msg)
	{
	case WM_SYSCHAR: // Prevent ALT + key combinations from creating 'ding' sounds
		{
			const bool bAlt = (lParam & (1 << 29)) != 0;
			if (bAlt && wParam == VK_F4)
			{
				return false; // Pass though ALT+F4
			}

			*pResult = 0;
			return true;
		}
		break;

	case WM_SIZE:
		HandleResizeForVOIP(wParam);
		break;

	case WM_SETFOCUS:
		if (g_pGameCVars)
		{
			g_pGameCVars->g_hasWindowFocus = true;
		}
		break;

	case WM_KILLFOCUS:
		if (g_pGameCVars)
		{
			g_pGameCVars->g_hasWindowFocus = false;
		}
		break;

	case WM_SETCURSOR:
		{
			// This is sample code to change the displayed cursor for Windows applications.
			// Note that this sample loads a texture (ie, .TIF or .DDS), not a .ICO or .CUR resource.
			IHardwareMouse* const pMouse = gEnv ? gEnv->pHardwareMouse : NULL;
			assert(pMouse && "HWMouse should be initialized before window is shown, check engine initialization order");
			const char* currentCursorPath = gEnv->pConsole->GetCVar("r_MouseCursorTexture")->GetString();
			const bool bResult = pMouse ? pMouse->SetCursor(currentCursorPath) : false;
			if (!bResult)
			{
				GameWarning("Unable to load cursor %s, does this file exist?", currentCursorPath);
			}
			*pResult = bResult ? TRUE : FALSE;
			return bResult;
		}
		break;
	}
	return false;
}


void CGameStartup::HandleResizeForVOIP(WPARAM wparam)
{
	if(gEnv->pConsole)
	{
		ICVar * pVOIPCvar = gEnv->pConsole->GetCVar("net_blaze_voip_enable");

		if(pVOIPCvar)
		{
			if(wparam == SIZE_RESTORED || wparam == SIZE_MAXIMIZED)
			{
				int currentVal = pVOIPCvar->GetIVal();
				if(m_nVOIPWasActive == -1)
				{
					m_nVOIPWasActive = currentVal;
				}
				if(m_nVOIPWasActive != currentVal)
				{
					pVOIPCvar->Set(m_nVOIPWasActive);
				}
				CryLog("[VOIP] Game maximized or restored, VOIP was set to %d, saved value %d - now restored", currentVal, m_nVOIPWasActive);
			}
			else if(wparam == SIZE_MINIMIZED)
			{
				m_nVOIPWasActive = pVOIPCvar->GetIVal();
				pVOIPCvar->Set(0);
				CryLog("[VOIP] Game minimized, VOIP was set to %d, setting to 0 while minimized", m_nVOIPWasActive);
			}
		}
	}	
}
#endif // CRY_PLATFORM_WINDOWS