// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   -------------------------------------------------------------------------
   History:
   - Dec 05,2005:	Created by Marco Koegler

*************************************************************************/
#include "StdAfx.h"
#include "DXInputDevice.h"

#ifdef USE_DXINPUT

static BOOL CALLBACK EnumDeviceObjectsCallback(LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef)
{
	std::vector<DIOBJECTDATAFORMAT>* customFormat = (std::vector<DIOBJECTDATAFORMAT>*)pvRef;

	DIOBJECTDATAFORMAT format;

	format.pguid = 0;
	format.dwOfs = lpddoi->dwOfs;
	format.dwType = lpddoi->dwType;
	format.dwFlags = 0;

	customFormat->push_back(format);

	return DIENUM_CONTINUE;
}

CDXInputDevice::CDXInputDevice(CDXInput& input, const char* deviceName, const GUID& guid) : CInputDevice(input, deviceName),
	m_dxInput(input), m_pDevice(0), m_guid(guid), m_pDataFormat(0), m_dwCoopLevel(0),
	m_bNeedsPoll(false)
{
}

CDXInputDevice::~CDXInputDevice()
{
	if (m_pDevice)
	{
		Unacquire();
		m_pDevice->Release();
		m_pDevice = 0;
	}
}

CDXInput& CDXInputDevice::GetDXInput() const
{
	return m_dxInput;
}

LPDIRECTINPUTDEVICE8 CDXInputDevice::GetDirectInputDevice() const
{
	return m_pDevice;
}

bool CDXInputDevice::Acquire()
{
	if (!m_pDevice) return false;

	HRESULT hr = m_pDevice->Acquire();

	unsigned char maxAcquire = 10;

	while (hr == DIERR_INPUTLOST && maxAcquire > 0)
	{
		hr = m_pDevice->Acquire();
		--maxAcquire;
	}

	if (FAILED(hr) || maxAcquire == 0) return false;

	return true;
}

///////////////////////////////////////////
bool CDXInputDevice::Unacquire()
{
	return (m_pDevice && SUCCEEDED(m_pDevice->Unacquire()));
}

bool CDXInputDevice::CreateDirectInputDevice(LPCDIDATAFORMAT dataFormat, DWORD coopLevel, DWORD bufSize)
{
	HRESULT hr = GetDXInput().GetDirectInput()->CreateDevice(m_guid, &m_pDevice, 0);

	if (FAILED(hr))
	{
		return false;
	}

	// get capabilities
	DIDEVCAPS caps;
	caps.dwSize = sizeof(DIDEVCAPS);
	m_pDevice->GetCapabilities(&caps);

	if (caps.dwFlags & DIDC_POLLEDDEVICE)
		m_bNeedsPoll = true;

	if (!dataFormat)
	{
		// build a custom one, here
	}

	hr = m_pDevice->SetDataFormat(dataFormat);
	if (FAILED(hr))
	{
		return false;
	}

	m_pDataFormat = dataFormat;
	m_dwCoopLevel = coopLevel;

	hr = m_pDevice->SetCooperativeLevel(GetDXInput().GetHWnd(), m_dwCoopLevel);
	if (FAILED(hr))
	{
		return false;
	}

	DIPROPDWORD dipdw = { { sizeof(DIPROPDWORD), sizeof(DIPROPHEADER), 0, DIPH_DEVICE }, bufSize };
	hr = m_pDevice->SetProperty(DIPROP_BUFFERSIZE, &dipdw.diph);
	if (FAILED(hr))
	{
		gEnv->pLog->LogToFile("Cannot Set Di Buffer Size\n");
		return false;
	}

	return true;
	/*
	   // if no dataformat was specified, we build our own :)
	   std::vector<DIOBJECTDATAFORMAT>	customFormat;
	   if (dataFormat == 0)
	   {
	   m_pDevice->EnumObjects(EnumDeviceObjectsCallback, &customFormat, DIDFT_AXIS);
	   m_pDevice->EnumObjects(EnumDeviceObjectsCallback, &customFormat, DIDFT_BUTTON);
	   m_pDevice->EnumObjects(EnumDeviceObjectsCallback, &customFormat, DIDFT_POV);

	   // no axes or buttons :(
	   if (customFormat.empty())
	    return false;

	   // copy the accumulated data format
	   DIDATAFORMAT* format = new DIDATAFORMAT;

	   format->dwSize		= sizeof(DIDATAFORMAT);
	   format->dwObjSize	= sizeof(DIOBJECTDATAFORMAT);
	   format->dwFlags		= m_dwFlags;
	   format->dwNumObjs	= (DWORD)customFormat.size();
	   format->rgodf			= new DIOBJECTDATAFORMAT[customFormat.size()];

	   format->dwDataSize = 0;
	   for (unsigned int i = 0; i < customFormat.size(); ++i)
	   {
	    if (customFormat[i].dwOfs > format->dwDataSize)
	      format->dwDataSize = (((customFormat[i].dwOfs+3)/4)*4);

	    format->rgodf[i] = customFormat[i];
	   }

	   m_bCustomFormat = true;
	   dataFormat = format;
	   }
	   SetupKeyNames(customFormat);

	   if (dataFormat == 0)
	   return false;

	   // ok, now set it to a know state
	   return InitDirectInputDevice();
	 */
}

/*
   bool CDXInputDevice::InitDirectInputDevice()
   {
   if (m_pDevice)
   {
    // release any focus
    m_pDevice->Unacquire();

    // try to specify the desired data format
    if (!SUCCEEDED(m_pDevice->SetDataFormat(m_pDataFormat)))
    {
      printf("ERROR: Could not set data format!\n");
      return false;
    }

    // try to set the specified cooperation level
    if (!SUCCEEDED(m_pDevice->SetCooperativeLevel(GetDXInput().GetHWnd(), m_dwCoopLevel)))
    {
      //gDispatchError("(InputDevice::InitDInputDevice) Couldn't set Direct Input cooperative level.");
      return false;
    }

    // set up buffered input
    DIPROPDWORD dipdw;

    dipdw.diph.dwSize       = sizeof(dipdw);
    dipdw.diph.dwHeaderSize = sizeof(dipdw.diph);
    dipdw.diph.dwObj        = 0;
    dipdw.diph.dwHow        = DIPH_DEVICE;
    dipdw.dwData            = m_dwDataSize;

    return SUCCEEDED(m_pDevice->SetProperty(DIPROP_BUFFERSIZE, &dipdw.diph));
   }

   // report failure
   return false;
   }
 */

/*

   bool CDXInputDevice::Init()
   {
   m_dwDataSize = 16;
   m_pData = new DIDEVICEOBJECTDATA[m_dwDataSize];

   return CreateDirectInputDevice(0, DISCL_FOREGROUND | DISCL_EXCLUSIVE);
   }


   void CDXInputDevice::Update(bool bFocus)
   {
   HRESULT	res;

   LPDIRECTINPUTDEVICE8 pDevice = GetDirectInputDevice();
   if (!pDevice) return;

   if (m_bNeedsPoll)
    res = pDevice->Poll();

   // check the state of the device buffer
   DWORD num	= INFINITE;
   res	= pDevice->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), 0, &num, DIGDD_PEEK);

   // now check for error states
   switch (res)
   {
   case DI_OK :
    {
      // everything went ok
      break;
    }
   case DIERR_NOTACQUIRED :
   case DIERR_INPUTLOST :
    {
      // we lost the input focus, so
      // try to reaquire it
      pDevice->Acquire();
      return;
    }
   case DI_BUFFEROVERFLOW :
    {
      // the device captured more data than
      // our input buffer could hold, so allocate
      // a bigger buffer twice the size
      if (m_dwDataSize)	delete []m_pData;

      m_dwDataSize *= 2;
      m_pData	= new DIDEVICEOBJECTDATA[m_dwDataSize];

      // set the device to a known state
      InitDirectInputDevice();
      break;
    }
   default :
    {
      // an unknown error occured,
      // for know we'll simply ignore it
      return;
    }
   }


   // ok, now read the device buffer and pass it to the ProcessInput function
   // that has to be implemented by a derived class
   m_dwNumData = m_dwDataSize;
   res = pDevice->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), m_pData, &m_dwNumData,0);

   ProcessInput();
   }

   bool CDXInputDevice::CreateDirectInputDevice(LPCDIDATAFORMAT dataFormat, DWORD coopLevel)
   {
   HRESULT hr = GetDXInput().GetDirectInput()->CreateDevice(m_guid, &m_pDevice, 0);

   if (FAILED(hr))
   {
    return false;
   }

   // get capabilities
   DIDEVCAPS caps;
   caps.dwSize = sizeof(DIDEVCAPS);
   m_pDevice->GetCapabilities(&caps);

   if (caps.dwFlags & DIDC_POLLEDDEVICE)
    m_bNeedsPoll = true;

   // if no dataformat was specified, we build our own :)
   std::vector<DIOBJECTDATAFORMAT>	customFormat;
   if (dataFormat == 0)
   {
    m_pDevice->EnumObjects(EnumDeviceObjectsCallback, &customFormat, DIDFT_AXIS);
    m_pDevice->EnumObjects(EnumDeviceObjectsCallback, &customFormat, DIDFT_BUTTON);
    m_pDevice->EnumObjects(EnumDeviceObjectsCallback, &customFormat, DIDFT_POV);

    // no axes or buttons :(
    if (customFormat.empty())
      return false;

    // copy the accumulated data format
    DIDATAFORMAT* format = new DIDATAFORMAT;

    format->dwSize		= sizeof(DIDATAFORMAT);
    format->dwObjSize	= sizeof(DIOBJECTDATAFORMAT);
    format->dwFlags		= m_dwFlags;
    format->dwNumObjs	= (DWORD)customFormat.size();
    format->rgodf			= new DIOBJECTDATAFORMAT[customFormat.size()];

    format->dwDataSize = 0;
    for (unsigned int i = 0; i < customFormat.size(); ++i)
    {
      if (customFormat[i].dwOfs > format->dwDataSize)
        format->dwDataSize = (((customFormat[i].dwOfs+3)/4)*4);

      format->rgodf[i] = customFormat[i];
    }

    m_bCustomFormat = true;
    dataFormat = format;
   }
   SetupKeyNames(customFormat);

   if (dataFormat == 0)
    return false;

   m_pDataFormat	= dataFormat;
   m_dwCoopLevel	= coopLevel;

   // ok, now set it to a know state
   return InitDirectInputDevice();
   }

   bool CDXInputDevice::InitDirectInputDevice()
   {
   if (m_pDevice)
   {
    // release any focus
    m_pDevice->Unacquire();

    // try to specify the desired data format
    if (!SUCCEEDED(m_pDevice->SetDataFormat(m_pDataFormat)))
    {
      printf("ERROR: Could not set data format!\n");
      return false;
    }

    // try to set the specified cooperation level
    if (!SUCCEEDED(m_pDevice->SetCooperativeLevel(GetDXInput().GetHWnd(), m_dwCoopLevel)))
    {
      //gDispatchError("(InputDevice::InitDInputDevice) Couldn't set Direct Input cooperative level.");
      return false;
    }

    // set up buffered input
    DIPROPDWORD dipdw;

    dipdw.diph.dwSize       = sizeof(dipdw);
    dipdw.diph.dwHeaderSize = sizeof(dipdw.diph);
    dipdw.diph.dwObj        = 0;
    dipdw.diph.dwHow        = DIPH_DEVICE;
    dipdw.dwData            = m_dwDataSize;

    return SUCCEEDED(m_pDevice->SetProperty(DIPROP_BUFFERSIZE, &dipdw.diph));
   }

   // report failure
   return false;
   }

   void CDXInputDevice::SetupKeyNames(const std::vector<DIOBJECTDATAFORMAT>& customFormat)
   {
   int nAxis = 0;
   int nButton = 0;
   int nPov = 0;

   std::vector<DIOBJECTDATAFORMAT>::const_iterator i;
   for (i = customFormat.begin(); i!= customFormat.end(); ++i)
   {
    char tempBuff[128] = {0};

    if ((*i).dwType & DIDFT_AXIS)
    {
      cry_sprintf(tempBuff, "axis_%d", nAxis);
      //symbol.type = IT_Axis;
   ++nAxis;
    }
    else if ((*i).dwType & DIDFT_BUTTON)
    {
      cry_sprintf(tempBuff, "button_%d", nButton);
      //symbol.type = IT_Button;
   ++nButton;
    }
    else if ((*i).dwType & DIDFT_POV)
    {
      cry_sprintf(tempBuff, "pov_%d", nPov);
      //symbol.type = IT_Pov;
   ++nPov;
    }

    if (tempBuff[0])
    {
      m_idToName[(*i).dwOfs] = tempBuff;
      m_nameToId[tempBuff] = (*i).dwOfs;
    }
   }
   }

   void CDXInputDevice::ProcessInput()
   {
   for(unsigned int i = 0; i < m_dwNumData; i++)
   {
    m_pData[i].uAppData = 0;
   }
   }

 */
#endif //USE_DXINPUT
