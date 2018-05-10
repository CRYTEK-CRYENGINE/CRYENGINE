// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#ifdef INCLUDE_SCALEFORM_SDK

#include <CrySystem/Scaleform/ConfigScaleform.h>
	#include "SharedStates.h"

	#include "../PakVars.h"
	#include "FlashPlayerInstance.h"
	#include "GFileCryPak.h"
	#include "GImage.h"
	#include "GFxVideoWrapper.h"
	#include "GImageInfo_Impl.h"
	#include "GTexture_Impl.h"

	#include <CrySystem/IConsole.h>
	#include <CryString/StringUtils.h>
	#include <CryString/CryPath.h>
	#include <CrySystem/ILocalizationManager.h>
	#include <CryRenderer/ITexture.h>
	#include <CrySystem/IStreamEngine.h>
	#include <CryString/UnicodeFunctions.h>

	#ifndef GFC_NO_THREADSUPPORT
//////////////////////////////////////////////////////////////////////////
// Convert custom CryEngine thread priority to a GFx acceptable thread priority value

GThread::ThreadPriority ConvertToGFxThreadPriority(int32 nPriority)
{
	if (nPriority > THREAD_PRIORITY_HIGHEST)
		return GThread::CriticalPriority;
	else if (nPriority > THREAD_PRIORITY_ABOVE_NORMAL)
		return GThread::HighestPriority;
	else if (nPriority > THREAD_PRIORITY_NORMAL)
		return GThread::AboveNormalPriority;
	else if (nPriority > THREAD_PRIORITY_BELOW_NORMAL)
		return GThread::NormalPriority;
	else if (nPriority > THREAD_PRIORITY_LOWEST)
		return GThread::BelowNormalPriority;
	else
		return GThread::LowestPriority;
}
	#endif

//////////////////////////////////////////////////////////////////////////
// CryGFxFileOpener

CryGFxFileOpener::CryGFxFileOpener()
{
}

CryGFxFileOpener& CryGFxFileOpener::GetAccess()
{
	static auto s_pInst = new CryGFxFileOpener(); // This pointer is released in ~GFxLoader2()
	return *s_pInst;
}

CryGFxFileOpener::~CryGFxFileOpener()
{
}

GFile* CryGFxFileOpener::OpenFile(const char* pUrl, SInt flags, SInt /*mode*/)
{
	assert(pUrl);
	if (flags & ~(GFileConstants::Open_Read | GFileConstants::Open_Buffered))
		return 0;
	const char* pExt = PathUtil::GetExt(pUrl);
	#if defined(USE_GFX_VIDEO)
	if (!strcmp(pUrl, internal_video_player))
		return new GMemoryFile(pUrl, fxvideoplayer_swf, sizeof(fxvideoplayer_swf));
	if (!stricmp(pExt, "usm"))
		return new GFileCryStream(pUrl, eStreamTaskTypeVideo);
	#endif
	// delegate all file I/O to ICryPak by returning a GFileCryPak object
	return new GFileCryPak(pUrl);
}

SInt64 CryGFxFileOpener::GetFileModifyTime(const char* pUrl)
{
	SInt64 fileModTime = 0;
	if (CFlashPlayer::CheckFileModTimeEnabled())
	{
		ICryPak* pPak = gEnv->pCryPak;
		if (pPak)
		{
			FILE* f = pPak->FOpen(pUrl, "rb");
			if (f)
			{
				fileModTime = (SInt64) pPak->GetModificationTime(f);
				pPak->FClose(f);
			}
		}
	}
	return fileModTime;
}

//////////////////////////////////////////////////////////////////////////
// CryGFxURLBuilder

CryGFxURLBuilder::CryGFxURLBuilder()
{
}

CryGFxURLBuilder& CryGFxURLBuilder::GetAccess()
{
	static auto s_pInst = new CryGFxURLBuilder(); // This pointer is released in ~GFxLoader2()
	return *s_pInst;
}

CryGFxURLBuilder::~CryGFxURLBuilder()
{
}

void CryGFxURLBuilder::BuildURL(GString* pPath, const LocationInfo& loc)
{
	assert(pPath); // GFx always passes a valid dst ptr

	const char* pOrigFilePath = loc.FileName.ToCStr();
	if (pOrigFilePath && *pOrigFilePath)
	{
		// if pOrigFilePath ends with "_locfont.[swf|gfx]", we search for the font in the localization folder
		const char locFontPostFixSwf[] = "_locfont.swf";
		const char locFontPostFixGfx[] = "_locfont.gfx";
		assert(sizeof(locFontPostFixSwf) == sizeof(locFontPostFixGfx)); // both postfixes must be of same length
		const size_t locFontPostFixLen = sizeof(locFontPostFixSwf) - 1;

		size_t filePathLength = strlen(pOrigFilePath);
		if (filePathLength > locFontPostFixLen)
		{
			size_t offset = filePathLength - locFontPostFixLen;
			if (!strnicmp(locFontPostFixGfx, pOrigFilePath + offset, locFontPostFixLen) || !strnicmp(locFontPostFixSwf, pOrigFilePath + offset, locFontPostFixLen))
			{
				char const* const szLanguage = gEnv->pSystem->GetLocalizationManager()->GetLanguage();
				*pPath = PathUtil::GetLocalizationFolder() + CRY_NATIVE_PATH_SEPSTR + szLanguage + "_xml" + CRY_NATIVE_PATH_SEPSTR;
				*pPath += PathUtil::GetFile(pOrigFilePath);
				return;
			}
		}

		if (*pOrigFilePath != '/')
			*pPath = loc.ParentPath + loc.FileName;
		else
			*pPath = GString(&pOrigFilePath[1], filePathLength - 1);
	}
	else
	{
		assert(0 && "CryGFxURLBuilder::BuildURL() - no filename passed!");
		*pPath = "";
	}
}

//////////////////////////////////////////////////////////////////////////
// CryGFxTextClipboard

CryGFxTextClipboard::CryGFxTextClipboard()
#if CRY_PLATFORM_WINDOWS
	: m_bSyncingClipboardFromWindows(false)
#endif // CRY_PLATFORM_WINDOWS
{
	if (auto pSystem = gEnv->pSystem)
	{
#if CRY_PLATFORM_WINDOWS
		const HWND hWnd = reinterpret_cast<HWND>(pSystem->GetHWND());
		AddClipboardFormatListener(hWnd);
		HandleMessage(hWnd, WM_CLIPBOARDUPDATE, 0, 0, nullptr); // Sync current clipboard content with Scaleform
#endif // CRY_PLATFORM_WINDOWS
		pSystem->RegisterWindowMessageHandler(this);
	}
}

CryGFxTextClipboard::~CryGFxTextClipboard()
{
	if (gEnv && gEnv->pSystem)
	{
		gEnv->pSystem->UnregisterWindowMessageHandler(this);
#if CRY_PLATFORM_WINDOWS
		RemoveClipboardFormatListener(reinterpret_cast<HWND>(gEnv->pSystem->GetHWND()));
#endif // CRY_PLATFORM_WINDOWS
	}
}

CryGFxTextClipboard& CryGFxTextClipboard::GetAccess()
{
	static auto s_pInst = new CryGFxTextClipboard(); // This pointer is released in ~GFxLoader2()
	return *s_pInst;
}

void CryGFxTextClipboard::OnTextStore(const wchar_t* szText, UPInt length)
{
#if CRY_PLATFORM_WINDOWS
	// Copy to windows clipboard
	if (!m_bSyncingClipboardFromWindows && OpenClipboard(nullptr) != 0)
	{
		const HWND hWnd = reinterpret_cast<HWND>(gEnv->pSystem->GetHWND());

		// Avoid endless notification loop
		RemoveClipboardFormatListener(hWnd);

		static_assert(sizeof(wchar_t) == 2, "sizeof(wchar_t) needs to be 2 to be compatible with Scaleform.");
		const HGLOBAL clipboardData = GlobalAlloc(GMEM_DDESHARE, sizeof(wchar_t) * (length + 1));

		wchar_t* const pData = reinterpret_cast<wchar_t*>(GlobalLock(clipboardData));
		G_wcscpy(pData, length + 1, szText);
		GlobalUnlock(clipboardData);

		SetClipboardData(CF_UNICODETEXT, clipboardData);

		CloseClipboard();

		AddClipboardFormatListener(hWnd);
	}
#endif // CRY_PLATFORM_WINDOWS
}

#if CRY_PLATFORM_WINDOWS
bool CryGFxTextClipboard::HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	if (uMsg == WM_CLIPBOARDUPDATE)
	{
		if (OpenClipboard(nullptr) != 0)
		{
			wstring data;
			const HANDLE wideData = GetClipboardData(CF_UNICODETEXT);
			if (wideData)
			{
				const LPCWSTR pWideData = reinterpret_cast<LPCWSTR>(GlobalLock(wideData));
				if (pWideData)
				{
					// Note: This conversion is just to make sure we discard malicious or malformed data
					Unicode::ConvertSafe<Unicode::eErrorRecovery_Discard>(data, pWideData);
					GlobalUnlock(wideData);
				}
			}
			CloseClipboard();

			m_bSyncingClipboardFromWindows = true;
			SetText(data.c_str(), data.size());
			m_bSyncingClipboardFromWindows = false;
		}
	}

	return false;
}
#endif // CRY_PLATFORM_WINDOWS

//////////////////////////////////////////////////////////////////////////
// CryGFxTranslator

CryGFxTranslator::CryGFxTranslator()
	: m_pILocMan(0)
{
}

CryGFxTranslator& CryGFxTranslator::GetAccess()
{
	static auto s_pInst = new CryGFxTranslator(); // This pointer is released in ~GFxLoader2()
	return *s_pInst;
}

void CryGFxTranslator::SetWordWrappingMode(const char* pLanguage)
{
	if (!stricmp(pLanguage, "japanese"))
	{
		WWMode = WWT_Prohibition;
	}
	else if (!stricmp(pLanguage, "chineset"))
	{
		WWMode = WWT_Asian;
	}
	else
	{
		WWMode = WWT_Default;
	}
}

CryGFxTranslator::~CryGFxTranslator()
{
}

UInt CryGFxTranslator::GetCaps() const
{
	return Cap_ReceiveHtml | Cap_StripTrailingNewLines;
}

void CryGFxTranslator::Translate(TranslateInfo* pTranslateInfo)
{
	IF(m_pILocMan == 0, 0)
	m_pILocMan = gEnv->pSystem->GetLocalizationManager();

	if (!m_pILocMan || !pTranslateInfo)
		return;

	const wchar_t* pKey = pTranslateInfo->GetKey();

	if (!pKey) // No string -> not localizable
		return;

	string utf8Key, localizedString;
	Unicode::Convert(utf8Key, pKey);

	if (m_pILocMan->LocalizeString(utf8Key, localizedString))
	{
		if (pTranslateInfo->IsKeyHtml())
			pTranslateInfo->SetResultHtml(localizedString.c_str());
		else
			pTranslateInfo->SetResult(localizedString.c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
// CryGFxLog

ICVar* CryGFxLog::CV_sys_flash_debuglog(0);
int CryGFxLog::ms_sys_flash_debuglog(0);

	#if defined(CRYGFXLOG_USE_IMPLICT_TLS)
		#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO
static __declspec(thread) const char* g_pCurFlashContext = 0;
		#elif CRY_PLATFORM_ORBIS
static __thread const char* g_pCurFlashContext = 0;
		#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
static __thread const char* g_pCurFlashContext = 0;
		#else
			#error No TLS storage defined for CryGFxLog on this platform config
		#endif
	#else
static unsigned int g_curFlashContextTlsIdx = TLS_OUT_OF_INDEXES;
	#endif

CryGFxLog::CryGFxLog()
	: m_pLog(0)
{
	m_pLog = gEnv->pLog;

	if (!CV_sys_flash_debuglog)
		CV_sys_flash_debuglog = REGISTER_CVAR2("sys_flash_debuglog", &ms_sys_flash_debuglog, 0, VF_NULL, "");

	#if !defined(CRYGFXLOG_USE_IMPLICT_TLS)
	g_curFlashContextTlsIdx = TlsAlloc();
	assert(g_curFlashContextTlsIdx != TLS_OUT_OF_INDEXES);
	#endif
}

CryGFxLog& CryGFxLog::GetAccess()
{
	static auto s_pInst = new CryGFxLog(); // This pointer is released in ~GFxLoader2()
	return *s_pInst;
}

CryGFxLog::~CryGFxLog()
{
	#if !defined(CRYGFXLOG_USE_IMPLICT_TLS)
	if (g_curFlashContextTlsIdx != TLS_OUT_OF_INDEXES)
		TlsFree(g_curFlashContextTlsIdx);
	#endif
}

void CryGFxLog::LogMessageVarg(LogMessageType messageType, const char* pFmt, va_list argList)
{
	char logBuf[1024];
	{
		const char prefix[] = "<Flash> ";

		static_assert(sizeof(prefix) + 128 <= sizeof(logBuf), "Invalid array size!");

		// prefix
		{
			cry_strcpy(logBuf, prefix);
		}

		// msg
		size_t len = sizeof(prefix) - 1;
		{
			cry_vsprintf(&logBuf[len], sizeof(logBuf) - len, pFmt, argList);
			len = strlen(logBuf);
			if (logBuf[len - 1] == '\n')
			{
				logBuf[--len] = 0;
			}
		}

		// context
		{
	#if defined(CRYGFXLOG_USE_IMPLICT_TLS)
			const char* pCurFlashContext = g_pCurFlashContext;
	#else
			const char* pCurFlashContext = g_curFlashContextTlsIdx != TLS_OUT_OF_INDEXES ? (const char*) TlsGetValue(g_curFlashContextTlsIdx) : 0;
	#endif
			cry_sprintf(&logBuf[len], sizeof(logBuf) - len, " [%s]", pCurFlashContext ? pCurFlashContext : "#!NO_CONTEXT!#");
		}
	}

	if (ms_sys_flash_debuglog)
	{
		PREFAST_SUPPRESS_WARNING(6053);
		OutputDebugString(logBuf);
	}

	switch (messageType & (~Log_Channel_Mask))
	{
	case Log_MessageType_Error:
		{
			m_pLog->LogError("%s", logBuf);
			break;
		}
	case Log_MessageType_Warning:
		{
			m_pLog->LogWarning("%s", logBuf);
			break;
		}
	case Log_MessageType_Message:
	default:
		{
			m_pLog->Log("%s", logBuf);
			break;
		}
	}
}

void CryGFxLog::SetContext(const char* pFlashContext)
{
	#if defined(CRYGFXLOG_USE_IMPLICT_TLS)
	g_pCurFlashContext = pFlashContext;
	#else
	if (g_curFlashContextTlsIdx != TLS_OUT_OF_INDEXES)
		TlsSetValue(g_curFlashContextTlsIdx, (void*) pFlashContext);
	#endif
}

const char* CryGFxLog::GetContext() const
{
	#if defined(CRYGFXLOG_USE_IMPLICT_TLS)
	return g_pCurFlashContext;
	#else
	const char* pCurFlashContext = g_curFlashContextTlsIdx != TLS_OUT_OF_INDEXES ? (const char*) TlsGetValue(g_curFlashContextTlsIdx) : 0;
	return pCurFlashContext;
	#endif
}

//////////////////////////////////////////////////////////////////////////
// CryGFxFSCommandHandler

CryGFxFSCommandHandler::CryGFxFSCommandHandler()
{
}

CryGFxFSCommandHandler& CryGFxFSCommandHandler::GetAccess()
{
	static auto s_pInst = new CryGFxFSCommandHandler(); // This pointer is released in ~GFxLoader2()
	return *s_pInst;
}

CryGFxFSCommandHandler::~CryGFxFSCommandHandler()
{
}

void CryGFxFSCommandHandler::Callback(GFxMovieView* pMovieView, const char* pCommand, const char* pArgs)
{
	// get flash player instance this action script command belongs to and delegate it to client
	CFlashPlayer* pFlashPlayer(static_cast<CFlashPlayer*>(pMovieView->GetUserData()));
	if (pFlashPlayer)
		pFlashPlayer->DelegateFSCommandCallback(pCommand, pArgs);
}

//////////////////////////////////////////////////////////////////////////
// CryGFxExternalInterface

CryGFxExternalInterface::CryGFxExternalInterface()
{
}

CryGFxExternalInterface& CryGFxExternalInterface::GetAccess()
{
	static auto s_pInst = new CryGFxExternalInterface(); // This pointer is released in ~GFxLoader2()
	return *s_pInst;
}

CryGFxExternalInterface::~CryGFxExternalInterface()
{
}

void CryGFxExternalInterface::Callback(GFxMovieView* pMovieView, const char* pMethodName, const GFxValue* pArgs, UInt numArgs)
{
	CFlashPlayer* pFlashPlayer(static_cast<CFlashPlayer*>(pMovieView->GetUserData()));
	if (pFlashPlayer)
		pFlashPlayer->DelegateExternalInterfaceCallback(pMethodName, pArgs, numArgs);
	else
		pMovieView->SetExternalInterfaceRetVal(GFxValue(GFxValue::VT_Undefined));
}

//////////////////////////////////////////////////////////////////////////
// CryGFxUserEventHandler

CryGFxUserEventHandler::CryGFxUserEventHandler()
{
}

CryGFxUserEventHandler& CryGFxUserEventHandler::GetAccess()
{
	static auto s_pInst = new CryGFxUserEventHandler(); // This pointer is released in ~GFxLoader2()
	return *s_pInst;
}

CryGFxUserEventHandler::~CryGFxUserEventHandler()
{
}

void CryGFxUserEventHandler::HandleEvent(class GFxMovieView* /*pMovieView*/, const class GFxEvent& /*event*/)
{
	// not implemented since it's not needed yet... would need to translate GFx event to independent representation
}

//////////////////////////////////////////////////////////////////////////
// CryGFxImageCreator

CryGFxImageCreator::CryGFxImageCreator()
{
}

CryGFxImageCreator& CryGFxImageCreator::GetAccess()
{
	static auto s_pInst = new CryGFxImageCreator(); // This pointer is released in ~GFxLoader2()
	return *s_pInst;
}

CryGFxImageCreator::~CryGFxImageCreator()
{
}

GImageInfoBase* CryGFxImageCreator::CreateImage(const GFxImageCreateInfo& info)
{
	GImageInfoBase* pImageInfo(0);
	switch (info.Type)
	{
	case GFxImageCreateInfo::Input_Image:
	case GFxImageCreateInfo::Input_File:
		{
			switch (info.Type)
			{
			case GFxImageCreateInfo::Input_Image:
				{
					// create image info and texture for internal image
					pImageInfo = new GImageInfoXRender(info.pImage);
					break;
				}
			case GFxImageCreateInfo::Input_File:
				{
					// create image info and texture for external image
					pImageInfo = new GImageInfoFileXRender(info.pFileInfo->FileName.ToCStr(), info.pFileInfo->TargetWidth, info.pFileInfo->TargetHeight);
					break;
				}
			}
			break;
		}
	default:
		{
			assert(0);
			break;
		}
	}
	return pImageInfo;
}

//////////////////////////////////////////////////////////////////////////
// CryGFxImageLoader

CryGFxImageLoader::CryGFxImageLoader()
{
}

CryGFxImageLoader& CryGFxImageLoader::GetAccess()
{
	static auto s_pInst = new CryGFxImageLoader(); // This pointer is released in ~GFxLoader2()
	return *s_pInst;
}

CryGFxImageLoader::~CryGFxImageLoader()
{
}

static bool LookupDDS(const char* pFilePath, uint32& width, uint32& height)
{
	ITexture* pTex = gEnv->pRenderer->EF_GetTextureByName(pFilePath);
	if (pTex)
	{
		height = pTex->GetHeight();
		width = pTex->GetWidth();

		return true;
	}
	else
	{
		CryStackStringT<char, 256> splitFilePath(pFilePath);
		splitFilePath += ".0";

		ICryPak* pPak(gEnv->pCryPak);

		FILE* f = pPak->FOpen(splitFilePath.c_str(), "rb");
		if (!f)
			f = pPak->FOpen(pFilePath, "rb");

		if (f)
		{
			pPak->FSeek(f, 12, SEEK_SET);
			pPak->FRead(&height, 1, f);
			pPak->FRead(&width, 1, f);
			pPak->FClose(f);
		}
		return f != 0;
	}
}

GImageInfoBase* CryGFxImageLoader::LoadImage(const char* pUrl)
{
	// callback for loadMovie API
	const char* pFilePath(strstr(pUrl, "://"));
	const char* pTexId(strstr(pUrl, "://TEXID:"));
	if (!pFilePath && !pTexId)
		return 0;
	if (pFilePath)
		pFilePath += 3;
	if (pTexId)
		pTexId += 9;

	GImageInfoBase* pImageInfo(0);
	if (CFlashPlayer::GetFlashLoadMovieHandler())
	{
		IFlashLoadMovieImage* pImage(CFlashPlayer::GetFlashLoadMovieHandler()->LoadMovie(pFilePath));
		if (pImage && pImage->IsValid())
			pImageInfo = new GImageInfoILMISrcXRender(pImage);
	}

	if (!pImageInfo)
	{
		if (pTexId)
		{
			int texId(atoi(pTexId));
			ITexture* pTexture(gEnv->pRenderer->EF_GetTextureByID(texId));
			if (pTexture)
			{
				pImageInfo = new GImageInfoTextureXRender(pTexture);
			}
		}
		else if (stricmp(pFilePath, "undefined") != 0)
		{
			const char* pExt = PathUtil::GetExt(pFilePath);
			if (!stricmp(pExt, "dds"))
			{
				// Note: We need to know the dimensions in advance as otherwise the image won't show up!
				// The seemingly more elegant approach to pass zero as w/h and explicitly call GetTexture() on the image object to
				// query w/h won't work reliably with MT rendering (potential deadlock between Advance() and Render()). This approach
				// works but requires that we keep in sync with any changes to the streaming system on consoles (see LookupDDS()).
				uint32 width = 0, height = 0;
				if (LookupDDS(pFilePath, width, height))
					pImageInfo = new GImageInfoFileXRender(pFilePath, width, height);
			}
	#if defined(USE_GFX_JPG)
			else if (!stricmp(pExt, "jpg"))
			{
				GFxLoader2* pLoader = CSharedFlashPlayerResources::GetAccess().GetLoader(true);
				assert(pLoader);
				GPtr<GFxJpegSupportBase> pJPGLoader = pLoader->GetJpegSupport();
				if (pJPGLoader)
				{
					GFileCryPak file(pFilePath, !strnicmp(pFilePath, "%user%", 6) ? ICryPak::FOPEN_ONDISK : 0);
					GPtr<GImage> pImage = *pJPGLoader->ReadJpeg(&file, GMemory::GetGlobalHeap());
					if (pImage)
						pImageInfo = new GImageInfoXRender(pImage);
				}
			}
	#endif
	#if defined(USE_GFX_PNG)
			else if (!stricmp(pExt, "png"))
			{
				GFxLoader2* pLoader = CSharedFlashPlayerResources::GetAccess().GetLoader(true);
				assert(pLoader);
				GPtr<GFxPNGSupportBase> pPNGLoader = pLoader->GetPNGSupport();
				if (pPNGLoader)
				{
					GFileCryPak file(pFilePath, !strnicmp(pFilePath, "%user%", 6) ? ICryPak::FOPEN_ONDISK : 0);
					GPtr<GImage> pImage = *pPNGLoader->CreateImage(&file, GMemory::GetGlobalHeap());
					if (pImage)
						pImageInfo = new GImageInfoXRender(pImage);
				}
			}
	#endif
			else if ((pExt[0] == '\0') && (pFilePath[0] == '$'))
			{
				uint32 width = 0, height = 0;
				if (LookupDDS(pFilePath, width, height))
				{
					pImageInfo = new GImageInfoFileXRender(pFilePath, width, height);
				}
			}
			else
				CryGFxLog::GetAccess().LogWarning("\"%s\" cannot be loaded by loadMovie API! Invalid file or format passed.", pFilePath);
		}
	}

	return pImageInfo;
}

#endif // #ifdef INCLUDE_SCALEFORM_SDK
