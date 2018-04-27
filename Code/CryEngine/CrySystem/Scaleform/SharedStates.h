// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _SHARED_STATES_H_
#define _SHARED_STATES_H_

#pragma once

#ifdef INCLUDE_SCALEFORM_SDK

	#pragma warning(push)
	#pragma warning(disable : 6326)// Potential comparison of a constant with another constant
	#pragma warning(disable : 6011)// Dereferencing NULL pointer
	#include <CryCore/Platform/CryWindows.h>
	#include <CrySystem/IWindowMessageHandler.h>
	#include <GFxLoader.h>
	#include <GFxPlayer.h> // includes <windows.h>
	#include <GFxLog.h>
	#include <GFxImageResource.h>
	#pragma warning(pop)

struct ILocalizationManager;
struct ILog;
struct ICVar;

	#ifndef GFC_NO_THREADSUPPORT
//////////////////////////////////////////////////////////////////////////
// Convert custom CryEngine thread priority to a GFx acceptable thread priority value

GThread::ThreadPriority ConvertToGFxThreadPriority(int32 nCryPriority);
	#endif

//////////////////////////////////////////////////////////////////////////
// CryGFxFileOpener

class CryGFxFileOpener : public GFxFileOpener
{
public:
	// GFxFileOpener interface
	virtual GFile* OpenFile(const char* pUrl, SInt flags = GFileConstants::Open_Read | GFileConstants::Open_Buffered, SInt mode = GFileConstants::Mode_ReadWrite);
	virtual SInt64 GetFileModifyTime(const char* pUrl);
	// no overwrite for OpenFileEx needed yet as it calls out OpenFile impl
	//virtual GFile* OpenFileEx(const char* pUrl, class GFxLog *pLog, SInt flags = GFileConstants::Open_Read|GFileConstants::Open_Buffered, SInt mode = GFileConstants::Mode_ReadWrite);

public:
	static CryGFxFileOpener& GetAccess();
	~CryGFxFileOpener();

private:
	CryGFxFileOpener();
};

//////////////////////////////////////////////////////////////////////////
// CryGFxURLBuilder

class CryGFxURLBuilder : public GFxURLBuilder
{
	// GFxURLBuilder interface
	virtual void BuildURL(GString* pPath, const LocationInfo& loc);

public:
	static CryGFxURLBuilder& GetAccess();
	~CryGFxURLBuilder();

private:
	CryGFxURLBuilder();
};

//////////////////////////////////////////////////////////////////////////
// CryGFxTextClipboard

class CryGFxTextClipboard : public GFxTextClipboard, IWindowMessageHandler
{
public:
	void OnTextStore(const wchar_t* szText, UPInt length) override;
	static CryGFxTextClipboard& GetAccess();
	~CryGFxTextClipboard();

#if CRY_PLATFORM_WINDOWS
	bool HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* pResult) override;
#endif // CRY_PLATFORM_WINDOWS

private:
	CryGFxTextClipboard();

#if CRY_PLATFORM_WINDOWS
	bool m_bSyncingClipboardFromWindows;
#endif // CRY_PLATFORM_WINDOWS
};

//////////////////////////////////////////////////////////////////////////
// CryGFxTranslator

class CryGFxTranslator : public GFxTranslator
{
public:
	// GFxTranslator interface
	virtual UInt GetCaps() const;
	virtual void Translate(TranslateInfo* pTranslateInfo);
	virtual bool IsDirty() const      { return m_bDirty; }
	void         SetDirty(bool dirty) { m_bDirty = dirty; }

public:
	static CryGFxTranslator& GetAccess();
	~CryGFxTranslator();

	void SetWordWrappingMode(const char* pLanguage);

private:
	CryGFxTranslator();

private:
	ILocalizationManager* m_pILocMan;
	bool                  m_bDirty;
};

//////////////////////////////////////////////////////////////////////////
// CryGFxLog

	#if CRY_PLATFORM_ORBIS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
		#define CRYGFXLOG_USE_IMPLICT_TLS
	#endif

class CryGFxLog : public GFxLog
{
public:
	// GFxLog interface
	virtual void LogMessageVarg(LogMessageType messageType, const char* pFmt, va_list argList);

public:
	static CryGFxLog& GetAccess();
	~CryGFxLog();

	void        SetContext(const char* pFlashContext);
	const char* GetContext() const;

private:
	CryGFxLog();

private:
	static ICVar* CV_sys_flash_debuglog;
	static int    ms_sys_flash_debuglog;

private:
	ILog* m_pLog;
};

//////////////////////////////////////////////////////////////////////////
// CryGFxFSCommandHandler

class CryGFxFSCommandHandler : public GFxFSCommandHandler
{
public:
	// GFxFSCommandHandler interface
	virtual void Callback(GFxMovieView* pMovieView, const char* pCommand, const char* pArgs);

public:
	static CryGFxFSCommandHandler& GetAccess();
	~CryGFxFSCommandHandler();

private:
	CryGFxFSCommandHandler();
};

//////////////////////////////////////////////////////////////////////////
// CryGFxExternalInterface

class CryGFxExternalInterface : public GFxExternalInterface
{
public:
	// GFxExternalInterface interface
	virtual void Callback(GFxMovieView* pMovieView, const char* pMethodName, const GFxValue* pArgs, UInt numArgs);

public:
	static CryGFxExternalInterface& GetAccess();
	~CryGFxExternalInterface();

private:
	CryGFxExternalInterface();
};

//////////////////////////////////////////////////////////////////////////
// CryGFxUserEventHandler

class CryGFxUserEventHandler : public GFxUserEventHandler
{
public:
	// GFxUserEventHandler interface
	virtual void HandleEvent(class GFxMovieView* pMovieView, const class GFxEvent& event);

public:
	static CryGFxUserEventHandler& GetAccess();
	~CryGFxUserEventHandler();

private:
	CryGFxUserEventHandler();
};

//////////////////////////////////////////////////////////////////////////
// CryGFxImageCreator

class CryGFxImageCreator : public GFxImageCreator
{
public:
	// GFxImageCreator interface
	virtual GImageInfoBase* CreateImage(const GFxImageCreateInfo& info);

public:
	static CryGFxImageCreator& GetAccess();
	~CryGFxImageCreator();

private:
	CryGFxImageCreator();
};

//////////////////////////////////////////////////////////////////////////
// CryGFxImageLoader

class CryGFxImageLoader : public GFxImageLoader
{
public:
	// GFxImageLoader interface
	virtual GImageInfoBase* LoadImage(const char* pUrl);

public:
	static CryGFxImageLoader& GetAccess();
	~CryGFxImageLoader();

private:
	CryGFxImageLoader();
};

#endif // #ifdef INCLUDE_SCALEFORM_SDK

#endif // #ifndef _SHARED_STATES_H_
