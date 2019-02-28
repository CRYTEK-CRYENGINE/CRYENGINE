// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	BaseInput implementation. This is primarily a "get things to
              compile" thing for new platforms which haven't gotten a
              real input implementation done yet. It implements all
              the listener functionality and offers a uniform device
              interface using IInputDevice.
   -------------------------------------------------------------------------
   History:
   - Dec 05,2005:	Created by Marco Koegler

*************************************************************************/
#ifndef __BASEINPUT_H__
#define __BASEINPUT_H__
#pragma once

#include <CryCore/Platform/platform.h>

#if CRY_PLATFORM_DURANGO
	#include "KinectInputWinRT.h"
#else
	#ifdef USE_KINECT
		#include "KinectInput.h"
	#else
		#include "KinectInputNULL.h"
	#endif
#endif

//#define USE_TRACKIR

#if (CRY_PLATFORM_WINDOWS && defined(USE_TRACKIR))
	#include "TrackIRInput.h"
typedef CTrackIRInput TNaturalPointInput;
#else
	#include "NaturalPointInputNULL.h"
typedef CNaturalPointInputNull TNaturalPointInput;
#endif

#include <CryCore/Containers/CryListenerSet.h>

struct  IInputDevice;
class CInputCVars;

class CBaseInput : public IInput, public ISystemEventListener
{
public:
	CBaseInput();
	virtual ~CBaseInput();

	// IInput
	// stub implementation
	virtual bool                Init();
	virtual void                PostInit();
	virtual void                Update(bool bFocus);
	virtual void                ShutDown();
	virtual void                SetExclusiveMode(EInputDeviceType deviceType, bool exclusive, void* pUser);
	virtual bool                InputState(const TKeyName& keyName, EInputState state);
	virtual const char*         GetKeyName(const SInputEvent& event) const;
	virtual const char*         GetKeyName(EKeyId keyId) const;
	virtual uint32              GetInputCharUnicode(const SInputEvent& event);
	virtual SInputSymbol*       LookupSymbol(EInputDeviceType deviceType, int deviceIndex, EKeyId keyId);
	virtual const SInputSymbol* GetSymbolByName(const char* name) const;
	virtual const char*         GetOSKeyName(const SInputEvent& event);
	virtual void                ClearKeyState();
	virtual void                ClearAnalogKeyState();
	virtual void                RetriggerKeyState();
	virtual bool                Retriggering() { return m_retriggering;  }
	virtual bool                HasInputDeviceOfType(EInputDeviceType type);
	virtual void                SetDeadZone(float fThreshold);
	virtual void                RestoreDefaultDeadZone();
	virtual IInputDevice*       GetDevice(uint16 id, EInputDeviceType deviceType);

	// listener functions (implemented)
	virtual void                 AddEventListener(IInputEventListener* pListener);
	virtual void                 RemoveEventListener(IInputEventListener* pListener);
	virtual bool                 AddTouchEventListener(ITouchEventListener* pListener, const char* name);
	virtual void                 RemoveTouchEventListener(ITouchEventListener* pListener);
	virtual void                 AddConsoleEventListener(IInputEventListener* pListener);
	virtual void                 RemoveConsoleEventListener(IInputEventListener* pLstener);
	virtual void                 SetExclusiveListener(IInputEventListener* pListener);
	virtual IInputEventListener* GetExclusiveListener();
	virtual bool                 AddInputDevice(IInputDevice* pDevice);
	virtual bool                 RemoveInputDevice(IInputDevice* pDevice);
	virtual void                 EnableEventPosting(bool bEnable);
	virtual bool                 IsEventPostingEnabled() const;
	virtual void                 PostInputEvent(const SInputEvent& event, bool bForce = false);
	virtual void                 PostTouchEvent(const STouchEvent& event, bool bForce = false);
	virtual void                 PostUnicodeEvent(const SUnicodeEvent& event, bool bForce = false);
	virtual void                 ForceFeedbackEvent(const SFFOutputEvent& event);
	virtual void                 ForceFeedbackSetDeviceIndex(int index);
	virtual void                 EnableDevice(EInputDeviceType deviceType, bool enable);
	virtual void                 ProcessKey(uint32 key, bool pressed, wchar_t unicode, bool repeat) {};
	// ~IInput

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam);
	// ~ISystemEventListener

	bool           HasFocus() const            { return m_hasFocus;  }
	int            GetModifiers() const        { return m_modifiers; }
	void           SetModifiers(int modifiers) { m_modifiers = modifiers;  }

	virtual uint32 GetPlatformFlags() const    { return m_platformFlags; }

	// Input blocking functionality
	virtual bool                SetBlockingInput(const SInputBlockData& inputBlockData);
	virtual bool                RemoveBlockingInput(const SInputBlockData& inputBlockData);
	virtual bool                HasBlockingInput(const SInputBlockData& inputBlockData) const;
	virtual int                 GetNumBlockingInputs() const;
	virtual void                ClearBlockingInputs();
	virtual bool                ShouldBlockInputEventPosting(const EKeyId keyId, const EInputDeviceType deviceType, const uint8 deviceIndex) const;

	virtual IKinectInput*       GetKinectInput()       { return m_pKinectInput; }
	virtual IEyeTrackerInput*   GetEyeTrackerInput()   { return m_pEyeTrackerInput; }

	virtual INaturalPointInput* GetNaturalPointInput() { return m_pNaturalPointInput; }

	virtual bool                GrabInput(bool bGrab);

	virtual int                 ShowCursor(const bool bShow) { return 0; }

protected:
	// Input blocking functionality
	void UpdateBlockingInputs();

	typedef std::vector<IInputDevice*> TInputDevices;
	void PostHoldEvents();
	void ClearHoldEvent(SInputSymbol* pSymbol);

private:
	bool        SendEventToListeners(const SInputEvent& event);
	bool        SendEventToListeners(const SUnicodeEvent& event);
	void        AddEventToHoldSymbols(const SInputEvent& event);
	void        RemoveDeviceHoldSymbols(EInputDeviceType deviceType, uint8 deviceIndex);
	static bool OnFilterInputEventDummy(SInputEvent* pInput);

	// listener functionality
	typedef std::list<IInputEventListener*> TInputEventListeners;
	TInputSymbols                      m_holdSymbols;
	TInputEventListeners               m_listeners;
	TInputEventListeners               m_consoleListeners;
	IInputEventListener*               m_pExclusiveListener;
	CListenerSet<ITouchEventListener*> m_touchListeners;

	bool                               m_enableEventPosting;
	bool                               m_retriggering;
	CryCriticalSection                 m_postInputEventMutex;

	bool                               m_hasFocus;

	// input device management
	TInputDevices m_inputDevices;

	//Filter for exclusive force-feedback output on an individual device
	int m_forceFeedbackDeviceIndex;

	// Input blocking functionality
	typedef std::list<SInputBlockData> TInputBlockData;
	TInputBlockData m_inputBlockData;

	// marcok: a bit nasty ... but I want to restrict access to CKeyboard ... this makes sure that
	// even mouse events could have a modifier
	int m_modifiers;

	//CVars
	CInputCVars* m_pCVars;

#if CRY_PLATFORM_DURANGO
	CKinectInputWinRT* m_pKinectInput;
#else
	#ifdef USE_KINECT
	CKinectInput*     m_pKinectInput;
	#else
	CKinectInputNULL* m_pKinectInput;
	#endif
#endif

	IEyeTrackerInput*   m_pEyeTrackerInput;

	TNaturalPointInput* m_pNaturalPointInput;

protected:
	uint32 m_platformFlags;
};

#endif //__BASEINPUT_H__
