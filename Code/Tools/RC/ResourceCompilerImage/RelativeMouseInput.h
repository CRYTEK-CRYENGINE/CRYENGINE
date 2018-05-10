// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// RelativeMouseInput.h: interface for the CRelativeMouseInput class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_RELATIVEMOUSEINPUT_H__FB763154_32E5_4857_B51E_C7EB1F946812__INCLUDED_)
#define AFX_RELATIVEMOUSEINPUT_H__FB763154_32E5_4857_B51E_C7EB1F946812__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// hides the mouse cursor,
// returns relative mouse movement

class CRelativeMouseInput  
{
public:
	//! constructor
	CRelativeMouseInput( void );
	//! destructor
	virtual ~CRelativeMouseInput( void );

	//! call in OnLButtonDown/OnMButtonDown/OnRButtonDown
	void OnButtonDown( HWND inHwnd );
	//! call in OnLButtonUp/OnMButtonUp/OnRButtonUp
	void OnButtonUp( void );

	//! call in OnMouseMove
	//! return true=mouse is captured an there was movement, false otherwise
	bool OnMouseMove( HWND inHwnd, bool inbButtonDown, int &outRelx, int &outRely );
	//!
	bool IsCaptured( void );

private:

	bool			m_Captured;			//!<
	int				m_oldx;					//!<
	int				m_oldy;					//!<
	POINT			m_savedpos;			//!<
};

#endif // !defined(AFX_RELATIVEMOUSEINPUT_H__FB763154_32E5_4857_B51E_C7EB1F946812__INCLUDED_)
