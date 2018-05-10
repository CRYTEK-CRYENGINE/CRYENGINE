// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File:SmartGlassContext.h - API Independent
//
//	History:
//	-Jan 10,2013:Originally Created by Steve Barnett
//
//////////////////////////////////////////////////////////////////////

#ifndef _SMARTGLASS_CONTEXT_H
#define _SMARTGLASS_CONTEXT_H

#if _MSC_VER > 1000
	#pragma once
#endif

#if defined(SUPPORT_SMARTGLASS)

	#include <smartglassinterop.h>

using Windows::Xbox::SmartGlass::SmartGlassDevice;

	#include "ISmartGlassManager.h"

class ISmartGlassContext : public CMultiThreadRefCount
{
public:
	ISmartGlassContext()
		: CMultiThreadRefCount()
		, m_pInputListener(NULL)
		, m_bDisconnected(false)
	{
	}

	virtual ~ISmartGlassContext()
	{
		if (m_pInputListener)
		{
			m_pInputListener->Release();
		}
	}

	virtual void Update() = 0;
	virtual void SendInputEvents() = 0;

	virtual void RT_Render(class CTexture* pTexBackbuffer) = 0;

	virtual void SetInputListener(ISmartGlassInputListener* pListener)
	{
		if (pListener != m_pInputListener)
		{
			if (m_pInputListener)
			{
				m_pInputListener->Release();
			}
			pListener->AddRef();
			m_pInputListener = pListener;
		}
	}

	void          MarkDisconnected() { m_bDisconnected = true; }
	bool          IsDisconnected()   { return m_bDisconnected; }

	virtual void  SetDevice(SmartGlassDevice^ pDevice) = 0;

	virtual void  SetFlashPlayer(struct IFlashPlayer* pFlashPlayer) = 0;

	virtual float GetWidth() = 0;
	virtual float GetHeight() = 0;

protected:
	ISmartGlassInputListener* m_pInputListener;
	bool                      m_bDisconnected;

private:
	ISmartGlassContext(const ISmartGlassContext&);
	ISmartGlassContext& operator=(const ISmartGlassContext&);
};

#endif

#endif // _SMARTGLASS_CONTEXT_H
