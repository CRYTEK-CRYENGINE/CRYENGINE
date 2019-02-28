// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SYNERGYCONTEXT_H__
#define __SYNERGYCONTEXT_H__

#ifdef USE_SYNERGY_INPUT

	#include <CryNetwork/CrySocks.h>
	#include <CryThreading/IThreadManager.h>

	#define MAX_CLIPBOARD_SIZE         1024

	#define SYNERGY_MODIFIER_SHIFT     0x1
	#define SYNERGY_MODIFIER_CTRL      0x2
	#define SYNERGY_MODIFIER_LALT      0x4
	#define SYNERGY_MODIFIER_WINDOWS   0x10
	#define SYNERGY_MODIFIER_RALT      0x20
	#define SYNERGY_MODIFIER_CAPSLOCK  0x1000
	#define SYNERGY_MODIFIER_NUMLOCK   0x2000
	#define SYNERGY_MODIFIER_SCROLLOCK 0x4000

class CSynergyContext : public IThread, public CMultiThreadRefCount
{
public:
	CSynergyContext(const char* pIdentifier, const char* pHost);
	~CSynergyContext();

	// Start accepting work on thread
	virtual void ThreadEntry();

	bool         GetKey(uint32& key, bool& bPressed, bool& bRepeat, uint32& modifier);
	bool         GetMouse(uint16& x, uint16& y, uint16& wheelX, uint16& wheelY, bool& buttonL, bool& buttonM, bool& buttonR);
	const char*  GetClipboard();

public:
	struct KeyPress
	{
		KeyPress(uint32 _key, bool _bPressed, bool _bRepeat, uint32 _modifier) { key = _key; bPressed = _bPressed; bRepeat = _bRepeat; modifier = _modifier; }
		uint32 key;
		bool   bPressed;
		bool   bRepeat;
		uint32 modifier;
	};
	struct MouseEvent
	{
		uint16 x, y, wheelX, wheelY;
		bool   button[3];
	};

	CryCriticalSection     m_keyboardLock;
	CryCriticalSection     m_mouseLock;
	CryCriticalSection     m_clipboardLock;
	std::deque<KeyPress>   m_keyboardQueue;
	std::deque<MouseEvent> m_mouseQueue;
	char                   m_clipboard[MAX_CLIPBOARD_SIZE];
	char                   m_clipboardThread[MAX_CLIPBOARD_SIZE];
	string                 m_host;
	string                 m_name;
	MouseEvent             m_mouseState;
	CRYSOCKET              m_socket;
	int                    m_packetOverrun;

private:

	bool m_bQuit;
};

#endif // USE_SYNERGY_INPUT

#endif // __SYNERGY_CONTEXT_H__
