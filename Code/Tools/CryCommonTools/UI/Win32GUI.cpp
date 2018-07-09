// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Win32GUI.h"
#include <cstdlib>
#include <cstring>
#include <string>
#include <map>
#include "commctrl.h"
#include "StringHelpers.h"

namespace Win32GUI
{
	const int WM_REFLECT_BASE = WM_USER + 0x1c00;
	const int WM_COMMAND_REFLECT = WM_REFLECT_BASE + WM_COMMAND;
	const int WM_NOTIFY_REFLECT = WM_REFLECT_BASE + WM_NOTIFY;

	class Window
	{
	public:
		typedef LRESULT (Window::*WindowProcMethod)(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

		explicit Window(WindowProcMethod windowProc);
		~Window();
		static LRESULT CALLBACK StaticWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
		LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
		LRESULT FrameWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

		void Subclass(HWND window);
		void Unsubclass(HWND window);

		WNDPROC m_oldWndProc;
		WindowProcMethod m_windowProc;
		typedef std::multimap<unsigned, EventCallbacks::ICallback*> CallbackMap;
		CallbackMap m_callbackMap;
	};
}

void Win32GUI::Initialize()
{
	InitCommonControls();
	LoadLibrary(_T("riched20.dll"));
}

void Win32GUI::RegisterFrameClass(const TCHAR* name)
{
	WNDCLASS cls;
	std::memset(&cls, 0, sizeof(cls));
  cls.style = 0;
	cls.lpfnWndProc = &Window::StaticWindowProc;
  cls.cbClsExtra = 0;
  cls.cbWndExtra = 0;
  cls.hInstance = GetModuleHandle(0);
  cls.hIcon = 0;
  cls.hCursor = LoadCursor(0, IDC_ARROW);
  cls.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
  cls.lpszMenuName = 0;
  cls.lpszClassName = name;

	RegisterClass(&cls);
}

HWND Win32GUI::CreateFrame(const TCHAR* className, unsigned style, int width, int height)
{
	Window* window = new Window(&Window::FrameWindowProc);

	HWND hwnd = CreateWindow(
    className,                                   // LPCTSTR lpClassName,
    TEXT(""),                                    // LPCTSTR lpWindowName,
    style,                                       // DWORD dwStyle,
    CW_USEDEFAULT,                               // int x,
    CW_USEDEFAULT,                               // int y,
    width,                                       // int nWidth,
    height,                                      // int nHeight,
    0,                                           // HWND hWndParent,
    0,                                           // HMENU hMenu,
    GetModuleHandle(0),                          // HINSTANCE hInstance,
    window);                                     // LPVOID lpParam

	return hwnd;
}

HWND Win32GUI::CreateControl(const TCHAR* className, unsigned style, HWND parent, int left, int top, int width, int height)
{
	Window* window = new Window(&Window::WindowProc);

	HWND hwnd = CreateWindow(
    className,                                   // LPCTSTR lpClassName,
    0,                                           // LPCTSTR lpWindowName,
    style | WS_CHILD | WS_VISIBLE,               // DWORD dwStyle,
    left,                                        // int x,
    top,                                         // int y,
    width,                                       // int nWidth,
    height,                                      // int nHeight,
    parent,                                      // HWND hWndParent,
    0,                                           // HMENU hMenu,
    GetModuleHandle(0),                          // HINSTANCE hInstance,
    0);                                          // LPVOID lpParam

	window->Subclass(hwnd);

	return hwnd;
}

int Win32GUI::Run()
{
	MSG msg;
	BOOL status;
	while ((status = GetMessage(&msg, HWND(0), UINT(0), UINT(0))) != 0)
	{
		if (status == -1)
		{
			return -1;
		}
		else
		{
			TranslateMessage(&msg); 
			DispatchMessage(&msg); 
		}
	}

	return (int)msg.wParam;
}

Win32GUI::Window::Window(WindowProcMethod windowProc)
: m_windowProc(windowProc),
	m_oldWndProc(0)
{
}

Win32GUI::Window::~Window()
{
	for (CallbackMap::iterator callbackPos = m_callbackMap.begin(); callbackPos != m_callbackMap.end(); ++callbackPos)
		(*callbackPos).second->Release();
}

LRESULT CALLBACK Win32GUI::Window::StaticWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	Window* window = 0;
	{
		if (uMsg == WM_CREATE)
		{
			CREATESTRUCT* create_struct = (CREATESTRUCT*)lParam;
			window = static_cast<Window*>(create_struct->lpCreateParams);
#if defined(_WIN64)
			SetWindowLongPtr(hwnd, GWLP_USERDATA, LONG_PTR(window));
#else
			SetWindowLong(hwnd, GWL_USERDATA, PtrToLong(window));
#endif
		}
		else
		{
#if defined(_WIN64)
			window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
#else
			window = static_cast<Window*>(LongToPtr(GetWindowLong(hwnd, GWL_USERDATA)));
#endif
		}
	}

	LRESULT result = 0;
	if (window)
		result = (window->*(window->m_windowProc))(hwnd, uMsg, wParam, lParam);
	else
		result = DefWindowProc(hwnd, uMsg, wParam, lParam);

	if (uMsg == WM_DESTROY)
	{
		delete window;
#if defined(_WIN64)
			SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
#else
			SetWindowLong(hwnd, GWL_USERDATA, 0);
#endif
	}

	return result;
}

LRESULT Win32GUI::Window::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_COMMAND:
		{
			HWND control = (HWND)lParam;
			if (control)
				SendMessage(control, WM_COMMAND_REFLECT, wParam, lParam);
		}
		break;

	case WM_NOTIFY:
		{
			NMHDR* notify_header = reinterpret_cast<NMHDR*>(lParam);
			if (notify_header->hwndFrom)
				SendMessage(notify_header->hwndFrom, WM_NOTIFY_REFLECT, wParam, lParam);
		}
		break;

	case WM_COMMAND_REFLECT:
		switch (HIWORD(wParam))
		{
		case EN_CHANGE:
			{
				std::string text = GetWindowString(hwnd);

				std::pair<CallbackMap::iterator, CallbackMap::iterator> range = m_callbackMap.equal_range(EventCallbacks::TextChanged::ID);
				for (CallbackMap::iterator callbackPos = range.first; callbackPos != range.second; ++callbackPos)
				{
					EventCallbacks::IStringCallback* callback = static_cast<EventCallbacks::IStringCallback*>((*callbackPos).second);
					callback->Call(text);
				}
			}
			break;

		case BN_CLICKED:
			{
				if (GetWindowLong(hwnd, GWL_STYLE) & BS_CHECKBOX)
				{
					bool checked = (0 != SendMessage(hwnd, BM_GETCHECK, 0, 0));
					std::pair<CallbackMap::iterator, CallbackMap::iterator> range = m_callbackMap.equal_range(EventCallbacks::Checked::ID);
					for (CallbackMap::iterator callbackPos = range.first; callbackPos != range.second; ++callbackPos)
					{
						EventCallbacks::IBoolCallback* callback = static_cast<EventCallbacks::IBoolCallback*>((*callbackPos).second);
						callback->Call(checked);
					}
				}
				else
				{
					std::pair<CallbackMap::iterator, CallbackMap::iterator> range = m_callbackMap.equal_range(EventCallbacks::Pushed::ID);
					for (CallbackMap::iterator callbackPos = range.first; callbackPos != range.second; ++callbackPos)
					{
						EventCallbacks::IVoidCallback* callback = static_cast<EventCallbacks::IVoidCallback*>((*callbackPos).second);
						callback->Call();
					}
				}
			}
			break;
		}
		break;

	case WM_GETMINMAXINFO:
		{
			int minW = 0, maxW = 0, minH = 100000, maxH = 100000;

			std::pair<CallbackMap::iterator, CallbackMap::iterator> range = m_callbackMap.equal_range(EventCallbacks::GetDimensions::ID);
			for (CallbackMap::iterator callbackPos = range.first; callbackPos != range.second; ++callbackPos)
			{
				EventCallbacks::IGetDimensionsCallback* callback = static_cast<EventCallbacks::IGetDimensionsCallback*>((*callbackPos).second);
				callback->Call(minW, maxW, minH, maxH);
			}

			MINMAXINFO* minMaxInfo = (MINMAXINFO*)lParam;
			minMaxInfo->ptMinTrackSize.x = minW;
			minMaxInfo->ptMaxTrackSize.x = maxW;
			minMaxInfo->ptMinTrackSize.y = minH;
			minMaxInfo->ptMaxTrackSize.y = maxH;
		}
		break;

	case WM_SIZE:
		{
			int width = LOWORD(lParam);
			int height = HIWORD(lParam);
			std::pair<CallbackMap::iterator, CallbackMap::iterator> range = m_callbackMap.equal_range(EventCallbacks::SizeChanged::ID);
			for (CallbackMap::iterator callbackPos = range.first; callbackPos != range.second; ++callbackPos)
			{
				EventCallbacks::ISizeCallback* callback = static_cast<EventCallbacks::ISizeCallback*>((*callbackPos).second);
				callback->Call(width, height);
			}
		}
		break;

	case WM_NOTIFY_REFLECT:
		break;
	}

	LRESULT result = 0;
	if (m_oldWndProc)
		result = CallWindowProc(m_oldWndProc, hwnd, uMsg, wParam, lParam);
	else
		result = DefWindowProc(hwnd, uMsg, wParam, lParam);

	return result;
}

LRESULT Win32GUI::Window::FrameWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CLOSE:
		PostQuitMessage(0);
		break;
	}

	return WindowProc(hwnd, uMsg, wParam, lParam);
}

void Win32GUI::Window::Subclass(HWND window)
{
#if defined(_WIN64)
	SetWindowLongPtr(window, GWLP_USERDATA, LONG_PTR(this));
	m_oldWndProc = (WNDPROC)GetWindowLongPtr(window, GWLP_WNDPROC);
	SetWindowLongPtr(window, GWLP_WNDPROC, LONG_PTR(&Window::StaticWindowProc));
#else
	SetWindowLong(window, GWL_USERDATA, PtrToLong(this));
	m_oldWndProc = (WNDPROC)LongToPtr(GetWindowLong(window, GWL_WNDPROC));
	SetWindowLong(window, GWL_WNDPROC, PtrToLong(&Window::StaticWindowProc));
#endif
}

void Win32GUI::Window::Unsubclass(HWND window)
{
#if defined(_WIN64)
	SetWindowLongPtr(window, GWLP_USERDATA, 0);
	SetWindowLongPtr(window, GWLP_WNDPROC, LONG_PTR(m_oldWndProc));
#else
	SetWindowLongPtr(window, GWL_USERDATA, 0);
	SetWindowLong(window, GWLP_WNDPROC, PtrToLong(m_oldWndProc));
#endif
	m_oldWndProc = 0;
}

std::string Win32GUI::GetWindowString(HWND hwnd)
{
	LRESULT length = SendMessage(hwnd, WM_GETTEXTLENGTH, 0, 0);
	std::wstring wtext(length, 0);
	SendMessageW(hwnd, WM_GETTEXT, length + 1, (LPARAM)&wtext[0]);
	return StringHelpers::ConvertUtf16ToAnsi(wtext.c_str(), '?');
}

void Win32GUI::SetWindowString(HWND hwnd, const std::string& text)
{
	std::wstring wtext = StringHelpers::ConvertAnsiToUtf16(text.c_str());
	SendMessageW(hwnd, WM_SETTEXT, 0, (LPARAM)&wtext[0]);
}

HFONT Win32GUI::CreateFont()
{
	HFONT hFont = 0;
	HFONT hGuiFont = static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));

	LOGFONT lfGuiFont = { 0 };
	if (::GetObject(hGuiFont, sizeof(LOGFONT), &lfGuiFont) == sizeof(LOGFONT))
	{
		_tcsncpy(lfGuiFont.lfFaceName, _T("MS Shell Dlg 2"), sizeof(lfGuiFont.lfFaceName) / sizeof(TCHAR));
		lfGuiFont.lfFaceName[(sizeof(lfGuiFont.lfFaceName) / sizeof(TCHAR)) - 1] = '\0';

		hFont = ::CreateFontIndirect(&lfGuiFont);

		return hFont;
	}
	return 0;
}

void Win32GUI::SetCallbackObject(HWND hwnd, unsigned eventID, EventCallbacks::ICallback* callback)
{
#if defined(_WIN64)
	Window* window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
#else
	Window* window = static_cast<Window*>(LongToPtr(GetWindowLong(hwnd, GWL_USERDATA)));
#endif

	window->m_callbackMap.insert(std::make_pair(eventID, callback));
}
