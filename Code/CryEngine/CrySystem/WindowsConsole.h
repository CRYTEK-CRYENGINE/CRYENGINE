// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   WindowsConsole.h
//  Version:     v1.00
//  Created:     22/5/2012 by Paul Mikell.
//  Description: CWindowsConsole class definition
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef _WINDOWSCONSOLE_H_

#define _WINDOWSCONSOLE_H_

#include <CrySystem/IConsole.h>
#include <CrySystem/ITextModeConsole.h>
#include <CryNetwork/INetwork.h>

#include <CryThreading/IThreadManager.h>

#if defined(USE_WINDOWSCONSOLE)

class CWindowsConsole;
class CWindowsConsoleInputThread;

	#define WINDOWS_CONSOLE_MAX_INPUT_RECORDS    256
	#define WINDOWS_CONSOLE_NUM_CRYENGINE_COLORS 10

class CWindowsConsoleInputThread : public IThread
{
public:
	CWindowsConsoleInputThread(CWindowsConsole& console);

	~CWindowsConsoleInputThread();

	// Start accepting work on thread
	virtual void ThreadEntry();

	// Signals the thread that it should not accept anymore work and exit
	void SignalStopWork();

	void Interrupt()
	{
	}

private:

	enum EWaitHandle
	{
		eWH_Event,
		eWH_Console,
		eWH_NumWaitHandles
	};

	CWindowsConsole& m_WindowsConsole;
	HANDLE           m_handles[eWH_NumWaitHandles];
	INPUT_RECORD     m_inputRecords[WINDOWS_CONSOLE_MAX_INPUT_RECORDS];
};

class CWindowsConsole : public ITextModeConsole, public IOutputPrintSink, public ISystemUserCallback
{
public:

	CWindowsConsole();
	virtual ~CWindowsConsole();

	// ITextModeConsole
	virtual Vec2_tpl<int> BeginDraw();
	virtual void          PutText(int x, int y, const char* pMsg);
	virtual void          EndDraw();
	virtual void          SetTitle(const char* title);

	// ~ITextModeConsole

	// IOutputPrintSink
	virtual void Print(const char* pInszText);
	// ~IOutputPrintSink

	// ISystemUserCallback
	virtual bool OnSaveDocument();
	virtual void OnProcessSwitch();
	virtual void OnInitProgress(const char* sProgressMsg);
	virtual void OnInit(ISystem* pSystem);
	virtual void OnShutdown();
	virtual void OnUpdate();
	virtual void GetMemoryUsage(ICrySizer* pSizer);
	// ~ISystemUserCallback

	void         SetRequireDedicatedServer(bool value);
	virtual void SetHeader(const char* pHeader);
	void         InputIdle();

private:

	struct SConDrawCmd
	{
		int  x;
		int  y;
		char text[256];
	};

	DynArray<SConDrawCmd> m_drawCmds;
	DynArray<SConDrawCmd> m_newCmds;

	enum ECellBuffer
	{
		eCB_Log,
		eCB_Full,
		eCB_Status,
		eCB_Command,
		eCB_NumCellBuffers
	};

	enum ECellBufferBit
	{
		eCBB_Log     = BIT(eCB_Log),
		eCBB_Full    = BIT(eCB_Full),
		eCBB_Status  = BIT(eCB_Status),
		eCBB_Command = BIT(eCB_Command)
	};

	class CCellBuffer
	{
	public:

		CCellBuffer(SHORT x, short y, SHORT w, SHORT h, SHORT lines, WCHAR emptyChar, uint8 defaultFgColor, uint8 defaultBgColor);
		~CCellBuffer();

		void  PutText(int x, int y, const char* pMsg);
		void  Print(const char* pInszText);
		void  NewLine();
		void  SetCursor(HANDLE hScreenBuffer, SHORT offset);
		void  SetFgColor(WORD color);
		void  Blit(HANDLE hScreenBuffer);
		bool  Scroll(SHORT numLines);
		bool  IsScrolledUp();
		void  FmtScrollStatus(uint32 size, char* pBuffer);
		void  GetMemoryUsage(ICrySizer* pSizer);
		void  Clear();
		SHORT Width();

	private:

		struct SPosition
		{
			SHORT head;
			SHORT lines;
			SHORT wrap;
			SHORT offset;
			SHORT scroll;
		};

		typedef std::vector<CHAR_INFO> TBuffer;

		void Print(const char* pInszText, SPosition& position);
		void AddCharacter(WCHAR ch, SPosition& position);
		void NewLine(SPosition& position);
		void ClearLine(SPosition& position);
		void Tab(SPosition& position);
		void WrapLine(SPosition& position);
		void AdvanceLine(SPosition& position);
		void ClearCells(TBuffer::iterator pDst, TBuffer::iterator pDstEnd);

		TBuffer    m_buffer;
		CHAR_INFO  m_emptyCell;
		WORD       m_attr;
		COORD      m_size;
		SMALL_RECT m_screenArea;
		SPosition  m_position;
		bool       m_escape;
		bool       m_color;

	};

	void Lock();
	void Unlock();
	bool TryLock();
	void OnConsoleInputEvent(INPUT_RECORD inputRecord);
	void OnKey(const KEY_EVENT_RECORD& event);
	void OnResize(const COORD& size);
	void OnBackspace();
	void OnTab();
	void OnReturn();
	void OnPgUp();
	void OnPgDn();
	void OnLeft();
	void OnUp();
	void OnRight();
	void OnDown();
	void OnDelete();
	void OnHistory(const char* pHistoryElement);
	void OnChar(CHAR ch);
	void DrawFull();
	void DrawStatus();
	void DrawCommand();
	void Repaint();
	void CleanUp();

	CryCriticalSection           m_lock;
	COORD                        m_consoleScreenBufferSize;
	SMALL_RECT                   m_consoleWindow;
	HANDLE                       m_inputBufferHandle;
	HANDLE                       m_screenBufferHandle;
	CCellBuffer                  m_logBuffer;
	CCellBuffer                  m_fullScreenBuffer;
	CCellBuffer                  m_statusBuffer;
	CCellBuffer                  m_commandBuffer;
	uint32                       m_dirtyCellBuffers;
	std::deque<CryStringT<char>> m_commandQueue;
	CryStringT<char>             m_commandPrompt;
	uint32                       m_commandPromptLength;
	CryStringT<char>             m_command;
	uint32                       m_commandCursor;
	CryStringT<char>             m_logLine;
	CryStringT<char>             m_progressString;
	CryStringT<char>             m_header;
	INetNub::SStatistics         m_nubStats;
	SSystemUpdateStats           m_updStats;
	CWindowsConsoleInputThread*  m_pInputThread;
	ISystem*                     m_pSystem;
	IConsole*                    m_pConsole;
	ITimer*                      m_pTimer;
	ICVar*                       m_pCVarSvMap;
	ICVar*                       m_pCVarSvMission;
	CryStringT<char>             m_title;

	ICVar*                       m_pCVarSvGameRules;
	CTimeValue                   m_lastStatusUpdate;
	CTimeValue                   m_lastUpdateTime;
	bool                         m_initialized;
	bool                         m_OnUpdateCalled;
	bool                         m_requireDedicatedServer;

	static const uint8           s_colorTable[WINDOWS_CONSOLE_NUM_CRYENGINE_COLORS];

	friend class CWindowsConsoleInputThread;
};

#endif // USE_UNIXCONSOLE

#endif // _WINDOWSCONSOLE_H_
