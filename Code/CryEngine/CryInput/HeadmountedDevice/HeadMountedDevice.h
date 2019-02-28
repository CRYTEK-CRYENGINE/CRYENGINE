// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	Head mounted device, for Windows
   -------------------------------------------------------------------------
   History:
   - 2013/05/03 - Created by Paulo Zaffari
   Limitations:

   -  If the player will turn the head more than 90 degrees horizontally,
   there will be rotation around the vertical AND horizontal axis while
   only rotation around the vertical axis was intended.
   While most humans can't turn their heads more than 90 degrees, if one
   would be sitting on a rotating chair or standing, this could be possible
   and perceivable.
   In order to avoid the limitation, instead of using deltas of Euler
   angles, the view control should take the headset's orientation as
   absolute. Mouse based transformations can still be added on top of the
   head mounted device's ones. (for example, to allow the player do 180
   degree turns)

   For devices line the Cinemizer, we can also have a second approach:
   - As this device has a mouse emulator, we can disable the device
   choosing sys_CurrentHMDType=0 and enable its mouse tracking by using
   sys_EnableCinemizerMouse=1.
*************************************************************************/

#ifndef __HEADMOUNTED_DEVICE_H__
#define __HEADMOUNTED_DEVICE_H__
#pragma once

#include "InputDevice.h"

struct IHMDDevice;

class CHeadMountedDevice : public CInputDevice
{
public:
	CHeadMountedDevice(IInput& input);
	virtual ~CHeadMountedDevice();

	// IInputDevice overrides
	virtual int  GetDeviceIndex() const { return 0; } //Assume only one device of this type
	virtual bool Init();
	virtual void Update(bool bFocus);
	virtual bool SetExclusiveMode(bool value);
	// ~IInputDevice

private:
	void PostEvent(SInputSymbol* pSymbol);

	IHMDDevice*          m_pDevice;

	Quat                 m_PreviousOrientation;

	const static int     MAX_HMD_SYMBOLS = eKI_HMD_Last - KI_HMD_BASE;
	static SInputSymbol* Symbol[MAX_HMD_SYMBOLS];
};

#endif // __HEADMOUNTED_DEVICE_H__
