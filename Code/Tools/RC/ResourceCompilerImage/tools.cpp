// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "tools.h"


void CenterWindow(HWND hwndChild, HWND hwndParent)
{
	RECT rChild, rParent;
	int wChild, hChild, wParent, hParent;
	int wScreen, hScreen, xNew, yNew;
	HDC hdc;

	// Get the Height and Width of the child window
	GetWindowRect(hwndChild, &rChild);
	wChild = rChild.right - rChild.left;
	hChild = rChild.bottom - rChild.top;

	// Get the display limits
	hdc = GetDC(hwndChild);
	wScreen = GetDeviceCaps(hdc, HORZRES);
	hScreen = GetDeviceCaps(hdc, VERTRES);
	ReleaseDC(hwndChild, hdc);

	// Get the Height and Width of the parent window
	if (hwndParent)
	{
		GetWindowRect(hwndParent, &rParent);
		wParent = rParent.right - rParent.left;
		hParent = rParent.bottom - rParent.top;
	}
	else // 0 means it's the screen
	{
		wParent = wScreen;
		hParent = hScreen;
		rParent.left=0;
		rParent.top=0;
	}

	// Calculate new X position, then adjust for screen
	xNew = rParent.left + ((wParent - wChild) /2);

	if (xNew < 0)
	{
		xNew = 0;
	}
	else if (xNew + wChild > wScreen)
	{
		xNew = wScreen - wChild;
	}

	// Calculate new Y position, then adjust for screen
	yNew = rParent.top + ((hParent - hChild) / 2);
	if (yNew < 0)
	{
		yNew = 0;
	}
	else if (yNew + hChild > hScreen)
	{
		yNew = hScreen - hChild;
	}

	// Set it, and return
	//  SetWindowPos(hwndChild, NULL, xNew, yNew, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	MoveWindow(hwndChild, xNew, yNew, wChild, hChild, TRUE);
}


INT_PTR CALLBACK WndProcRedirect(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HWND hParent = GetParent(hWnd);

	if (hParent)
	{
		return SendMessage(hParent, uMsg, wParam, lParam) != 0 ? TRUE : FALSE;
	}

	//  return DefWindowProc( hWnd, uMsg, wParam, lParam );
	return FALSE;
}
