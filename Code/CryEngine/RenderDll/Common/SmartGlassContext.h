// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File:SmartGlassContext.h - API Independent
//
//	History:
//	-Jan 10,2013:Originally Created by Steve Barnett
//
//////////////////////////////////////////////////////////////////////

#ifndef _SMARTGLASS_CONTEXT_IMPL_H
#define _SMARTGLASS_CONTEXT_IMPL_H

#if _MSC_VER > 1000
	#pragma once
#endif

#if defined(SUPPORT_SMARTGLASS)

	#include "ISmartGlassContext.h"

using Windows::Xbox::SmartGlass::SmartGlassDirectSurface;
using Windows::UI::Core::PointerEventArgs;

ref class CSmartGlassInputListenerWrapper;

class CSmartGlassContext : public ISmartGlassContext
{
public:
	CSmartGlassContext();
	virtual ~CSmartGlassContext();

	virtual void  SetDevice(SmartGlassDevice^ pDevice);
	virtual void  SetFlashPlayer(struct IFlashPlayer* pFlashPlayer);

	virtual float GetWidth()  { return static_cast<float>(m_screenWidth); }
	virtual float GetHeight() { return static_cast<float>(m_screenHeight); }

	virtual void  Update();
	virtual void  SendInputEvents();

	virtual void RT_Render(class CTexture* pTexBackbuffer);

	void InitializeDevice(D3DDevice* pD3DDevice, D3DDeviceContext* pD3DDeviceContext);
	void SetSwapChain(IDXGISwapChain* pSwapChain);
	void CreateSwapChain();

	void OnPointerPressed(SmartGlassDirectSurface^ sender, PointerEventArgs^ args);
	void OnPointerMoved(SmartGlassDirectSurface^ sender, PointerEventArgs^ args);
	void OnPointerReleased(SmartGlassDirectSurface^ sender, PointerEventArgs^ args);

private:
	struct D3DDevice*                           m_pDevice;               // Not owned
	struct D3DDeviceContext*                    m_pDeviceContext;        // Not owned
	struct DXGISwapChain*                       m_pSwapChain;            // Owned
	struct D3DSurface*                          m_pRenderTargetView;     // Owned
	struct D3DSurface*                          m_pPrevRenderTargetView; // Temporarily owned, borrowed from m_pCryTexture
	class CDeviceTexture*                       m_pPrevDeviceTexture;    // Temporarily owned, borrowed from m_pCryTexture
	D3DViewPort                                 m_viewport;
	CTexture*                                   m_pCryTexture; // Owned

	int                                         m_screenWidth;
	int                                         m_screenHeight;

	SmartGlassDevice^                           m_pSmartGlassDevice;              // Ref counted
	ISmartGlassDirectSurfaceNative*             m_pSmartGlassDirectSurfaceNative; // Not owned

	CSmartGlassInputListenerWrapper^            m_pInputWrapper; // Ref counted
	Windows::Foundation::EventRegistrationToken m_pressedEventCookie;
	Windows::Foundation::EventRegistrationToken m_movedEventCookie;
	Windows::Foundation::EventRegistrationToken m_releasedEventCookie;

	struct IFlashPlayer*                        m_pFlashPlayer;

	enum
	{
		EVENT_RESERVE_COUNT = 16
	};
	enum EInputEventType
	{
		EIET_PRESSED = 0,
		EIET_MOVED,
		EIET_RELEASED,
	};
	struct SInputEvent
	{
		EInputEventType type;
		uint32          pointerId;
		float           x;
		float           y;
	};
	typedef std::vector<SInputEvent> InputEventVec;
	CryCriticalSection m_eventQueueLock;
	InputEventVec      m_eventQueue;
};

#endif

#endif // _SMARTGLASS_CONTEXT_IMPL_H
