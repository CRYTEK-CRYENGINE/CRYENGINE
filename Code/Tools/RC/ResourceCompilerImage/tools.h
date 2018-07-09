// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once 

// FUNCTION: CenterWindow (HWND, HWND) 
// PURPOSE:  Center one window over another 
// COMMENTS: 
//      Dialog boxes take on the screen position that they were designed at, 
//      which is not always appropriate. Centering the dialog over a particular 
//      window usually results in a better position. 
extern void CenterWindow( HWND hwndChild, HWND hwndParent=0 );

// redirect to the parent window
extern INT_PTR CALLBACK WndProcRedirect( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
