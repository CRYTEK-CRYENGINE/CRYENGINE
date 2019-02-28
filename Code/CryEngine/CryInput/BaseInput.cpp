// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	BaseInput
   -------------------------------------------------------------------------
   History:
   - Dec 05,2005:	Created by Marco Koegler
   - Sep 24,2012:	Modified by Dario Sancho
* added support for Durango GamePad
* fix potential crash in PostHoldEvent and RetriggerKeyState

*************************************************************************/
#include "StdAfx.h"
#include "BaseInput.h"
#include "InputDevice.h"
#include "InputCVars.h"
#include "EyeTrackerInput.h"
#include <CryString/UnicodeFunctions.h>

bool compareInputListener(const IInputEventListener* pListenerA, const IInputEventListener* pListenerB)
{
	CRY_ASSERT(pListenerA != NULL && pListenerB != NULL);
	if (!pListenerA || !pListenerB)
		return false;

	if (pListenerA->GetPriority() > pListenerB->GetPriority())
	{
		return true;
	}

	return false;
}

CBaseInput::CBaseInput()
	: m_pExclusiveListener(0)
	, m_enableEventPosting(true)
	, m_retriggering(false)
	, m_hasFocus(false)
	, m_modifiers(0)
	, m_pCVars(new CInputCVars())
	, m_platformFlags(0)
	, m_forceFeedbackDeviceIndex(EFF_INVALID_DEVICE_INDEX)
	, m_pKinectInput(0)
	, m_pEyeTrackerInput(0)
	, m_pNaturalPointInput(0)
	, m_touchListeners(1)
{
	GetISystem()->GetISystemEventDispatcher()->RegisterListener(this, "CBaseInput");

	g_pInputCVars = m_pCVars;

	m_holdSymbols.reserve(64);

	OnFilterInputEvent = OnFilterInputEventDummy;
}

bool CBaseInput::OnFilterInputEventDummy(SInputEvent* pInput)
{
	return true;
}

CBaseInput::~CBaseInput()
{
	ShutDown();
	GetISystem()->GetISystemEventDispatcher()->RemoveListener(this);

	SAFE_DELETE(m_pCVars);
	g_pInputCVars = NULL;

	SAFE_DELETE(m_pKinectInput);
	SAFE_DELETE(m_pEyeTrackerInput);
	SAFE_DELETE(m_pNaturalPointInput);
}

bool CBaseInput::Init()
{
	m_modifiers = 0;
	// always pass
#if CRY_PLATFORM_DURANGO
	m_pKinectInput = new CKinectInputWinRT();
#else
	#ifdef USE_KINECT
	m_pKinectInput = new CKinectInput();
	#else
	m_pKinectInput = new CKinectInputNULL();
	#endif
#endif // CRY_PLATFORM_DURANGO
	m_pKinectInput->Init();

	m_pNaturalPointInput = new TNaturalPointInput;
	m_pNaturalPointInput->Init();

#ifdef USE_EYETRACKER
	m_pEyeTrackerInput = new CEyeTrackerInput();
	m_pEyeTrackerInput->Init();
#endif //USE_EYETRACKER

	return true;
}

void CBaseInput::PostInit()
{
	for (TInputDevices::iterator i = m_inputDevices.begin(); i != m_inputDevices.end(); ++i)
	{
		(*i)->PostInit();
	}
}

void CBaseInput::Update(bool bFocus)
{
	CRY_PROFILE_FUNCTION(PROFILE_INPUT);

	m_hasFocus = bFocus;

	// Update blocking inputs
	UpdateBlockingInputs();

	PostHoldEvents();

	for (TInputDevices::iterator i = m_inputDevices.begin(); i != m_inputDevices.end(); ++i)
	{
		if ((*i)->IsEnabled())
			(*i)->Update(bFocus);
	}

	// send commit event after all input processing for this frame has finished
	SInputEvent event;
	event.modifiers = m_modifiers;
	event.deviceType = eIDT_Unknown;
	event.state = eIS_Changed;
	event.value = 0;
	event.keyName = "commit";
	event.keyId = eKI_SYS_Commit;
	PostInputEvent(event);

	if (m_pKinectInput)
	{
		m_pKinectInput->Update();
	}

#ifdef USE_EYETRACKER
	if (m_pEyeTrackerInput)
	{
		m_pEyeTrackerInput->Update();
	}
#endif //USE_EYETRACKER

	if (m_pNaturalPointInput)
	{
		m_pNaturalPointInput->Update();
	}
}

void CBaseInput::ShutDown()
{
	std::for_each(m_inputDevices.begin(), m_inputDevices.end(), stl::container_object_deleter());
	m_inputDevices.clear();
}

void CBaseInput::SetExclusiveMode(EInputDeviceType deviceType, bool exclusive, void* pUser)
{
	if (g_pInputCVars->i_debug)
	{
		gEnv->pLog->Log("InputDebug: SetExclusiveMode(%d, %s)", deviceType, exclusive ? "true" : "false");
	}

	if (deviceType < (int)m_inputDevices.size())
	{
		m_inputDevices[deviceType]->ClearKeyState();
		m_inputDevices[deviceType]->SetExclusiveMode(exclusive);

		// try to flush the device ... perform a dry update
		EnableEventPosting(false);
		m_inputDevices[deviceType]->Update(m_hasFocus);
		EnableEventPosting(true);
	}
}

bool CBaseInput::InputState(const TKeyName& keyName, EInputState state)
{
	for (TInputDevices::iterator i = m_inputDevices.begin(); i != m_inputDevices.end(); ++i)
	{
		if ((*i)->InputState(keyName, state)) return true;
	}
	return false;
}

const char* CBaseInput::GetKeyName(const SInputEvent& event) const
{
	for (TInputDevices::const_iterator i = m_inputDevices.begin(); i != m_inputDevices.end(); ++i)
	{
		const IInputDevice* pInputDevice = (*i);
		CRY_ASSERT(pInputDevice != NULL);
		if (event.deviceType == eIDT_Unknown || pInputDevice->GetDeviceType() == event.deviceType)
		{
			const char* szRet = pInputDevice->GetKeyName(event);
			if (szRet)
			{
				return szRet;
			}
		}
	}

	return "";
}

const char* CBaseInput::GetKeyName(EKeyId keyId) const
{
	TInputDevices::const_iterator iter = m_inputDevices.begin();
	TInputDevices::const_iterator iterEnd = m_inputDevices.end();
	for (; iter != iterEnd; ++iter)
	{
		const IInputDevice* pDevice = *iter;
		CRY_ASSERT(pDevice != NULL);
		const char* szKeyName = pDevice->GetKeyName(keyId);
		if (szKeyName)
		{
			return szKeyName;
		}
	}

	return NULL;
}

uint32 CBaseInput::GetInputCharUnicode(const SInputEvent& event)
{
	for (TInputDevices::iterator i = m_inputDevices.begin(); i != m_inputDevices.end(); ++i)
	{
		IInputDevice* pInputDevice = (*i);
		CRY_ASSERT(pInputDevice != NULL);
		if (event.deviceType == eIDT_Unknown || pInputDevice->GetDeviceType() == event.deviceType)
		{
			uint32 inputChar = pInputDevice->GetInputCharUnicode(event);
			if (inputChar)
			{
				return inputChar;
			}
		}
	}

	return '\0';
}

const char* CBaseInput::GetOSKeyName(const SInputEvent& event)
{
	for (TInputDevices::iterator i = m_inputDevices.begin(); i != m_inputDevices.end(); ++i)
	{
		const char* ret = (*i)->GetOSKeyName(event);
		if (ret && *ret)
			return ret;
	}
	return "";
}

//////////////////////////////////////////////////////////////////////////
SInputSymbol* CBaseInput::LookupSymbol(EInputDeviceType deviceType, int deviceIndex, EKeyId keyId)
{
	for (TInputDevices::iterator i = m_inputDevices.begin(); i != m_inputDevices.end(); ++i)
	{
		IInputDevice* pDevice = *i;
		if (pDevice->GetDeviceType() == deviceType && pDevice->GetDeviceIndex() == deviceIndex)
		{
			return pDevice->LookupSymbol(keyId);
		}
	}

	// if no symbol found try finding it in any device
	for (TInputDevices::iterator i = m_inputDevices.begin(); i != m_inputDevices.end(); ++i)
	{
		IInputDevice* pDevice = *i;
		SInputSymbol* pSym = pDevice->LookupSymbol(keyId);
		if (pSym)
			return pSym;
	}

	return NULL;
}

const SInputSymbol* CBaseInput::GetSymbolByName(const char* name) const
{
	TInputDevices::const_iterator iter = m_inputDevices.begin();
	TInputDevices::const_iterator iterEnd = m_inputDevices.end();
	for (; iter != iterEnd; ++iter)
	{
		const IInputDevice* pDevice = *iter;
		CRY_ASSERT(pDevice != NULL);
		const SInputSymbol* pSymbol = pDevice->GetSymbolByName(name);
		if (pSymbol)
		{
			return pSymbol;
		}
	}

	return NULL;
}

void CBaseInput::ClearKeyState()
{
	if (g_pInputCVars->i_debug)
	{
		gEnv->pLog->Log("InputDebug: ClearKeyState");
	}
	m_modifiers = 0;
	for (TInputDevices::iterator i = m_inputDevices.begin(); i != m_inputDevices.end(); ++i)
	{
		(*i)->ClearKeyState();
	}
	m_retriggering = false;

	m_holdSymbols.clear();
	if (g_pInputCVars->i_debug)
	{
		gEnv->pLog->Log("InputDebug: ~ClearKeyState");
	}
}

void CBaseInput::ClearAnalogKeyState()
{
	if (g_pInputCVars->i_debug)
	{
		gEnv->pLog->Log("InputDebug: ClearAnalogKeyState");
	}

	for (TInputDevices::iterator i = m_inputDevices.begin(); i != m_inputDevices.end(); ++i)
	{
		TInputSymbols clearedSymbols;
		(*i)->ClearAnalogKeyState(clearedSymbols);

		int clearedSize = clearedSymbols.size();

		for (int j = 0; j < clearedSize; j++)
		{
			ClearHoldEvent(clearedSymbols[j]);
		}
	}

	if (g_pInputCVars->i_debug)
	{
		gEnv->pLog->Log("InputDebug: ~ClearAnalogKeyState");
	}
}

void CBaseInput::RetriggerKeyState()
{
	if (g_pInputCVars->i_debug)
	{
		gEnv->pLog->Log("InputDebug: RetriggerKeyState");
	}

	m_retriggering = true;
	SInputEvent event;

	// DARIO_NOTE: In this loop, m_holdSymbols size and content is changed so previous
	// code was resulting in occasional index out of bounds.
	// Note that PostInputEvent() can either push or pop elements from m_holdSymbols.
	// The original intent of the code has been lost in time... so....
	// the fix includes checking for (i < m_holdSymbols.size()) inside the loop and
	// caching m_holdSymbols[i]

	const size_t count = m_holdSymbols.size();
	for (size_t i = 0; i < count; ++i)
	{
		if (i < m_holdSymbols.size())
		{
			SInputSymbol* pSymbol = m_holdSymbols[i];
			EInputState oldState = pSymbol->state;
			pSymbol->state = eIS_Pressed;
			pSymbol->AssignTo(event, GetModifiers());
			event.deviceIndex = pSymbol->deviceIndex;
			PostInputEvent(event);
			pSymbol->state = oldState;
		}
	}
	m_retriggering = false;
	if (g_pInputCVars->i_debug)
	{
		gEnv->pLog->Log("InputDebug: ~RetriggerKeyState");
	}
}

void CBaseInput::AddEventListener(IInputEventListener* pListener)
{
	// Add new listener to list if not added yet.
	if (std::find(m_listeners.begin(), m_listeners.end(), pListener) == m_listeners.end())
	{
		m_listeners.push_back(pListener);
		m_listeners.sort(compareInputListener);
	}
}

void CBaseInput::RemoveEventListener(IInputEventListener* pListener)
{
	// Remove listener if it is in list.
	TInputEventListeners::iterator it = std::find(m_listeners.begin(), m_listeners.end(), pListener);
	if (it != m_listeners.end())
	{
		m_listeners.erase(it);
	}
}

void CBaseInput::AddConsoleEventListener(IInputEventListener* pListener)
{
	if (std::find(m_consoleListeners.begin(), m_consoleListeners.end(), pListener) == m_consoleListeners.end())
	{
		m_consoleListeners.push_back(pListener);
		m_consoleListeners.sort(compareInputListener);
	}
}

void CBaseInput::RemoveConsoleEventListener(IInputEventListener* pListener)
{
	TInputEventListeners::iterator it = std::find(m_consoleListeners.begin(), m_consoleListeners.end(), pListener);
	if (it != m_consoleListeners.end()) m_consoleListeners.erase(it);
}

void CBaseInput::SetExclusiveListener(IInputEventListener* pListener)
{
	m_pExclusiveListener = pListener;
}

IInputEventListener* CBaseInput::GetExclusiveListener()
{
	return m_pExclusiveListener;
}

bool CBaseInput::AddInputDevice(IInputDevice* pDevice)
{
	if (pDevice)
	{
		if (pDevice->Init())
		{
			static uint8 uniqueID = 0;
			pDevice->SetUniqueId(uniqueID++);
			m_inputDevices.push_back(pDevice);
			return true;
		}
		delete pDevice;
	}
	return false;
}

bool CBaseInput::RemoveInputDevice(IInputDevice* pDevice)
{
	for(auto it = m_inputDevices.begin(), end = m_inputDevices.end(); it != end; ++it)
	{
		if (*it == pDevice)
		{
			m_inputDevices.erase(it);
			delete pDevice;
			return true;
		}
	}
	return false;
}

void CBaseInput::EnableEventPosting(bool bEnable)
{
	m_enableEventPosting = bEnable;
	if (g_pInputCVars->i_debug)
	{
		gEnv->pLog->Log("InputDebug: EnableEventPosting(%s)", bEnable ? "true" : "false");
	}
}

bool CBaseInput::IsEventPostingEnabled() const
{
	return m_enableEventPosting;
}

void CBaseInput::PostInputEvent(const SInputEvent& event, bool bForce)
{
	CRY_PROFILE_FUNCTION(PROFILE_INPUT);
	//CryAutoCriticalSection postInputLock(m_postInputEventMutex);

	if (!bForce && !m_enableEventPosting)
	{
		return;
	}

	if (event.keyId == eKI_Unknown)
	{
		return;
	}

	if (g_pInputCVars->i_debug)
	{
		// log out key press and release events
		if (event.state == eIS_Pressed || event.state == eIS_Released)
			gEnv->pLog->Log("InputDebug: '%s' - %s", event.keyName.c_str(), event.state == eIS_Pressed ? "pressed" : "released");
		else if ((event.state == eIS_Changed) && (g_pInputCVars->i_debug == 2))
			gEnv->pLog->Log("InputDebug (Changed): '%s' - %.4f", event.keyName.c_str(), event.value);
	}

	if (!SendEventToListeners(event))
		return;
	AddEventToHoldSymbols(event);

	if (event.keyId == eKI_SYS_DisconnectDevice)
	{
		// Zero everything out
		RemoveDeviceHoldSymbols(event.deviceType, event.deviceIndex);
		ClearKeyState();
		ClearAnalogKeyState();
	}
}

void CBaseInput::PostUnicodeEvent(const SUnicodeEvent& event, bool bForce)
{
	CRY_PROFILE_FUNCTION(PROFILE_INPUT);
	assert(event.inputChar != 0 && Unicode::Validate(event.inputChar) && "Attempt to post invalid unicode event");

	if (!bForce && !m_enableEventPosting)
	{
		return;
	}

	if (g_pInputCVars->i_debug)
	{
		char utf8_buf[5];
		Unicode::Convert(utf8_buf, event.inputChar);
		gEnv->pLog->Log("InputDebug: Unicode input codepoint (%u), %s", event.inputChar, utf8_buf);
	}

	if (!SendEventToListeners(event))
		return;
}

bool CBaseInput::SendEventToListeners(const SInputEvent& event)
{
	// console listeners get to process the event first
	for (TInputEventListeners::const_iterator it = m_consoleListeners.begin(); it != m_consoleListeners.end(); ++it)
	{
		bool ret = (*it)->OnInputEvent(event);
		if (ret)
			return false;
	}

	// If we're in the editor, Escape will leave game mode, so don't process the key press beyond that point
	// If Shift is held down, allow the press to go through as a normal (non-shifted) press
	SInputEvent e(event);
	if (gEnv->IsEditor() && e.keyId == eKI_Escape)
	{
		if (e.modifiers & eMM_Shift)
		{
			e.modifiers &= ~eMM_Shift;
		}
		else
		{
			return false;
		}
	}

	// exclusive listener can filter out the event if he wants to and cause this call to return
	// before any of the regular listeners get to process it
	if (m_pExclusiveListener)
	{
		bool ret = m_pExclusiveListener->OnInputEvent(e);
		if (ret)
			return false;
	}

	// Don't post events if input is being blocked
	// but still need to update held inputs
	bool bInputBlocked = ShouldBlockInputEventPosting(e.keyId, e.deviceType, e.deviceIndex);
	if (!bInputBlocked)
	{
		// Send this event to all listeners until the first one returns true.
		for (TInputEventListeners::const_iterator it = m_listeners.begin(); it != m_listeners.end(); ++it)
		{
			assert(*it);
			if (*it == NULL)
				continue;

			bool ret = (*it)->OnInputEvent(e);
			if (ret) break;
		}
	}

	return true;
}

bool CBaseInput::SendEventToListeners(const SUnicodeEvent& event)
{
	// console listeners get to process the event first
	for (TInputEventListeners::const_iterator it = m_consoleListeners.begin(); it != m_consoleListeners.end(); ++it)
	{
		bool ret = (*it)->OnInputEventUI(event);
		if (ret)
			return false;
	}

	// exclusive listener can filter out the event if he wants to and cause this call to return
	// before any of the regular listeners get to process it
	if (m_pExclusiveListener)
	{
		bool ret = m_pExclusiveListener->OnInputEventUI(event);
		if (ret)
			return false;
	}

	// Send this event to all listeners until the first one returns true.
	for (TInputEventListeners::const_iterator it = m_listeners.begin(); it != m_listeners.end(); ++it)
	{
		assert(*it);
		if (*it == NULL)
			continue;

		bool ret = (*it)->OnInputEventUI(event);
		if (ret) break;
	}

	return true;
}

void CBaseInput::AddEventToHoldSymbols(const SInputEvent& event)
{
	if (!m_retriggering)
	{
		if (event.pSymbol && event.pSymbol->state == eIS_Pressed)
		{
			if (g_pInputCVars->i_debug)
			{
				gEnv->pLog->Log("InputDebug: 0x%p AddEventToHoldSymbols Symbol %d %s, Event %s %s", this, event.pSymbol->keyId, event.pSymbol->name.c_str(), event.keyName.c_str(), event.state == eIS_Pressed ? "pressed" : "released");
			}
			event.pSymbol->state = eIS_Down;
			//cache device index in the hold symbol
			event.pSymbol->deviceIndex = event.deviceIndex;
			m_holdSymbols.push_back(event.pSymbol);
		}
		else if (event.pSymbol && event.pSymbol->state == eIS_Released && !m_holdSymbols.empty())
		{
			ClearHoldEvent(event.pSymbol);
		}
	}
}

void CBaseInput::RemoveDeviceHoldSymbols(EInputDeviceType deviceType, uint8 deviceIndex)
{
	if (!m_holdSymbols.empty())
	{
		TInputSymbols::iterator begin = m_holdSymbols.begin();
		for (TInputSymbols::iterator it = begin; it != m_holdSymbols.end(); )
		{
			SInputSymbol* symbol = *it;
			if (symbol->deviceType == deviceType && symbol->deviceIndex == deviceIndex)
			{
				SInputEvent releaseEvent;
				releaseEvent.deviceType = deviceType;
				releaseEvent.deviceIndex = deviceIndex;
				releaseEvent.state = eIS_Released;
				releaseEvent.keyName = symbol->name;
				releaseEvent.keyId = symbol->keyId;
				releaseEvent.pSymbol = symbol;

				if (g_pInputCVars->i_debug)
				{
					gEnv->pLog->Log("InputDebug: RemoveDeviceHoldSymbols Device %d %u, Symbol %s %d", deviceType, deviceIndex, releaseEvent.keyName.c_str(), releaseEvent.keyId);
				}

				SendEventToListeners(releaseEvent);

				it = m_holdSymbols.erase(it);
			}
			else
			{
				++it;
			}
		}
	}
}

bool CBaseInput::ShouldBlockInputEventPosting(const EKeyId keyId, const EInputDeviceType deviceType, const uint8 deviceIndex) const
{
#if CRY_PLATFORM_DURANGO
	return false;
#else
	bool bBlocked = false;
	TInputBlockData::const_iterator iter = m_inputBlockData.begin();
	TInputBlockData::const_iterator iterEnd = m_inputBlockData.end();
	for (; iter != iterEnd; ++iter)
	{
		const SInputBlockData& curBlockingInput = *iter;
		if (curBlockingInput.keyId == keyId)
		{
			// Now check if correct device index since can block certain devices only if wanted to
			if (curBlockingInput.bAllDeviceIndices) // All device indices match to this one
			{
				bBlocked = true;
				break;
			}
			else if (curBlockingInput.deviceIndex == deviceIndex) // Device index matches
			{
				bBlocked = true;
				break;
			}
		}
	}

	return bBlocked;
#endif
}

void CBaseInput::UpdateBlockingInputs()
{
	const float fElapsedTime = gEnv->pTimer->GetFrameTime(ITimer::ETIMER_UI);

	TInputBlockData::iterator iter = m_inputBlockData.begin();
	TInputBlockData::iterator iterEnd = m_inputBlockData.end();
	while (iter != iterEnd)
	{
		SInputBlockData& curBlockingInput = *iter;
		float& fBlockDuration = curBlockingInput.fBlockDuration;
		fBlockDuration -= fElapsedTime;
		if (fBlockDuration < 0.0f) // Time is up, remove now
		{
			iter = m_inputBlockData.erase(iter);
		}
		else
		{
			++iter;
		}
	}
}

void CBaseInput::PostHoldEvents()
{
	CRY_PROFILE_FUNCTION(PROFILE_INPUT);
	SInputEvent event;

	// DARIO_NOTE: In this loop, m_holdSymbols size and content is changed so previous
	// code was resulting in occasional index out of bounds.
	// The desired behaviour as I understand it is as follows:
	// (PRESSED) symbols should be added to the to the end of m_holdSymbols but not
	// processed this frame (thus the keep the loop comparison as is).
	// On the other hand, (RELEASED) symbols need to be removed from the m_holdSymbols this frame
	// so the size of m_holdSymbols may decrease. The search for a RELEASED symbol is done backwards
	// i.e. from m_holdSymbols.end() - 1 to 0 thus it will never go before the sending index (i.e. i)
	// In order to allow for this behaviour, the (i < kCurrentCount) check is added.

	const size_t kInitialCount = m_holdSymbols.size();
	for (size_t i = 0; i < kInitialCount; ++i)
	{
		const size_t kCurrentCount = m_holdSymbols.size();
		if (i < kCurrentCount)
		{
			m_holdSymbols[i]->AssignTo(event, GetModifiers());
			event.deviceIndex = m_holdSymbols[i]->deviceIndex;

			if (g_pInputCVars->i_debug)
			{
				gEnv->pLog->Log("InputDebug: 0x%p PostHoldEvent Symbol %d %s", this, m_holdSymbols[i]->keyId, m_holdSymbols[i]->name.c_str());
			}

			PostInputEvent(event);
		}
	}
}

void CBaseInput::ClearHoldEvent(SInputSymbol* pSymbol)
{
	// remove hold key
	int slot = -1;
	int last = m_holdSymbols.size() - 1;

	for (int i = last; i >= 0; --i)
	{
		if (m_holdSymbols[i] == pSymbol)
		{
			slot = i;
			break;
		}
	}
	if (slot != -1)
	{

		if (g_pInputCVars->i_debug)
		{
			gEnv->pLog->Log("InputDebug: 0x%p ClearHoldEvent Symbol %d %s", this, pSymbol->keyId, pSymbol->name.c_str());
		}
		// swap last and found symbol
		m_holdSymbols[slot] = m_holdSymbols[last];
		// pop last ... which is now the one we want to get rid of
		m_holdSymbols.pop_back();
	}
}

void CBaseInput::ForceFeedbackEvent(const SFFOutputEvent& event)
{
	if (g_pInputCVars->i_forcefeedback == 0 || m_hasFocus == false)
		return;

	for (TInputDevices::iterator i = m_inputDevices.begin(); i != m_inputDevices.end(); ++i)
	{
		if ((*i)->GetDeviceType() == event.deviceType && (m_forceFeedbackDeviceIndex == (*i)->GetDeviceIndex()))
		{
			IFFParams params;
			params.effectId = event.eventId;
			params.timeInSeconds = event.timeInSeconds;
			params.triggerData = event.triggerData;

			switch (event.eventId)
			{
			case eFF_Rumble_Basic:
				params.strengthA = event.amplifierS;
				params.strengthB = event.amplifierA;
				break;

			case eFF_Rumble_Frame:
				params.timeInSeconds = 0.0f;
				params.strengthA = event.amplifierS;
				params.strengthB = event.amplifierA;
				break;
			default:
				break;
			}

			if ((*i)->SetForceFeedback(params))
				break;
		}
	}
}

void CBaseInput::ForceFeedbackSetDeviceIndex(int index)
{
	if (m_forceFeedbackDeviceIndex != index)
	{
		// Disable rumble on current device before switching, otherwise it will stay on
		ForceFeedbackEvent(SFFOutputEvent(eIDT_Gamepad, eFF_Rumble_Frame, SFFTriggerOutputData::Initial::ZeroIt, 0.0f, 0.0f, 0.0f));
		m_forceFeedbackDeviceIndex = index;
	}
}

void CBaseInput::EnableDevice(EInputDeviceType deviceType, bool enable)
{
	if (deviceType < (int)m_inputDevices.size())
	{
		// try to flush the device ... perform a dry update
		EnableEventPosting(false);
		m_inputDevices[deviceType]->Update(m_hasFocus);
		EnableEventPosting(true);
		m_inputDevices[deviceType]->Enable(enable);
	}
}

bool CBaseInput::HasInputDeviceOfType(EInputDeviceType type)
{
	for (TInputDevices::iterator i = m_inputDevices.begin(); i != m_inputDevices.end(); ++i)
	{
		if ((*i)->IsOfDeviceType(type))
			return true;
	}
	return false;
}

void CBaseInput::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	if (event == ESYSTEM_EVENT_LANGUAGE_CHANGE)
	{
		for (TInputDevices::iterator i = m_inputDevices.begin(); i != m_inputDevices.end(); ++i)
		{
			(*i)->OnLanguageChange();
		}
		// Clear all key states and hold symbols, as their symbol pointers have now changed
		ClearKeyState();
	}
	else if (
	  event == ESYSTEM_EVENT_CHANGE_FOCUS ||
	  event == ESYSTEM_EVENT_LEVEL_LOAD_START ||
	  event == ESYSTEM_EVENT_LEVEL_GAMEPLAY_START ||
	  event == ESYSTEM_EVENT_LEVEL_POST_UNLOAD)
	{
		ClearKeyState();
	}
	else if (event == ESYSTEM_EVENT_FAST_SHUTDOWN)
	{
		CryLogAlways("BaseInput - Shutting down all force feedback");
		ForceFeedbackEvent(SFFOutputEvent(eIDT_Gamepad, eFF_Rumble_Basic, SFFTriggerOutputData::Initial::ZeroIt, 0.0f, 0.0f, 0.0f));
	}
}

// Input blocking functionality
bool CBaseInput::SetBlockingInput(const SInputBlockData& inputBlockData)
{
	// Don't allow blocking special or unknown events
	if (inputBlockData.keyId >= eKI_SYS_Commit)
		return false;

	bool bSet = false;
	bool bFound = false;

	TInputBlockData::iterator iter = m_inputBlockData.begin();
	TInputBlockData::iterator iterEnd = m_inputBlockData.end();
	for (; iter != iterEnd; ++iter)
	{
		SInputBlockData& curBlockingInputData = *iter;
		if (curBlockingInputData.keyId == inputBlockData.keyId)
		{
			// Now match the deviceIndex if using it
			if ((curBlockingInputData.bAllDeviceIndices && inputBlockData.bAllDeviceIndices) ||    // All indices match
			    ((!curBlockingInputData.bAllDeviceIndices && !inputBlockData.bAllDeviceIndices) && // Not all indices + deviceIndex matches
			     (curBlockingInputData.deviceIndex == inputBlockData.deviceIndex)))
			{
				// It already exists, but only set if the specified time exceeds current time
				if (curBlockingInputData.fBlockDuration < inputBlockData.fBlockDuration)
				{
					curBlockingInputData.fBlockDuration = inputBlockData.fBlockDuration;
					bSet = true;
				}
				bFound = true;
				break;
			}
		}
	}

	if (!bFound) // Not found so now add it
	{
		m_inputBlockData.push_back(inputBlockData);
		bSet = true;
	}

	return bSet;
}

bool CBaseInput::RemoveBlockingInput(const SInputBlockData& inputBlockData)
{
	bool bRemoved = false;

	TInputBlockData::iterator iter = m_inputBlockData.begin();
	TInputBlockData::iterator iterEnd = m_inputBlockData.end();
	for (; iter != iterEnd; ++iter)
	{
		const SInputBlockData& curBlockingInputData = *iter;
		// Now match the deviceIndex if using it
		if ((curBlockingInputData.bAllDeviceIndices && inputBlockData.bAllDeviceIndices) ||    // All indices match
		    ((!curBlockingInputData.bAllDeviceIndices && !inputBlockData.bAllDeviceIndices) && // Not all indices + deviceIndex matches
		     (curBlockingInputData.deviceIndex == inputBlockData.deviceIndex)))
		{
			m_inputBlockData.erase(iter);
			bRemoved = true;
			break;
		}
	}

	return bRemoved;
}

bool CBaseInput::HasBlockingInput(const SInputBlockData& inputBlockData) const
{
	bool bHasBlockingInput = false;

	TInputBlockData::const_iterator iter = m_inputBlockData.begin();
	TInputBlockData::const_iterator iterEnd = m_inputBlockData.end();
	for (; iter != iterEnd; ++iter)
	{
		const SInputBlockData& curBlockingInputData = *iter;
		// Now match the deviceIndex if using it
		if ((curBlockingInputData.bAllDeviceIndices && inputBlockData.bAllDeviceIndices) ||    // All indices match
		    ((!curBlockingInputData.bAllDeviceIndices && !inputBlockData.bAllDeviceIndices) && // Not all indices + deviceIndex matches
		     (curBlockingInputData.deviceIndex == inputBlockData.deviceIndex)))
		{
			bHasBlockingInput = true;
			break;
		}
	}

	return bHasBlockingInput;
}

int CBaseInput::GetNumBlockingInputs() const
{
	return (int)m_inputBlockData.size();
}

void CBaseInput::ClearBlockingInputs()
{
	m_inputBlockData.clear();
}

void CBaseInput::SetDeadZone(float fThreshold)
{
	if (g_pInputCVars->i_debug)
	{
		gEnv->pLog->Log("InputDebug: SetDeadZone [%0.3f]", fThreshold);
	}

	for (TInputDevices::iterator i = m_inputDevices.begin(); i != m_inputDevices.end(); ++i)
	{
		(*i)->SetDeadZone(fThreshold);
	}

	if (g_pInputCVars->i_debug)
	{
		gEnv->pLog->Log("InputDebug: ~SetDeadZone");
	}
}

void CBaseInput::RestoreDefaultDeadZone()
{
	if (g_pInputCVars->i_debug)
	{
		gEnv->pLog->Log("InputDebug: RestoreDefaultDeadZone");
	}

	for (TInputDevices::iterator i = m_inputDevices.begin(); i != m_inputDevices.end(); ++i)
	{
		(*i)->RestoreDefaultDeadZone();
	}

	if (g_pInputCVars->i_debug)
	{
		gEnv->pLog->Log("InputDebug: ~RestoreDefaultDeadZone");
	}
}

bool CBaseInput::GrabInput(bool bGrab)
{
	return false;
}

bool CBaseInput::AddTouchEventListener(ITouchEventListener* pListener, const char* name)
{
	return m_touchListeners.Add(pListener, name);
}

void CBaseInput::RemoveTouchEventListener(ITouchEventListener* pListener)
{
	m_touchListeners.Remove(pListener);
}

void CBaseInput::PostTouchEvent(const STouchEvent& event, bool bForce /*=false */)
{
	if (!bForce && !m_enableEventPosting)
		return;

	if (g_pInputCVars->i_debug)
		gEnv->pLog->Log("InputDebug [Touch]: x: %.4f - y: %.4f - id: %d", event.pos.x, event.pos.y, event.id);

	for (CListenerSet<ITouchEventListener*>::Notifier notifier(m_touchListeners); notifier.IsValid(); notifier.Next())
		notifier->OnTouchEvent(event);
}

IInputDevice* CBaseInput::GetDevice(uint16 id, EInputDeviceType deviceType)
{
	for (TInputDevices::const_iterator it = m_inputDevices.begin(); it != m_inputDevices.end(); ++it)
	{
		IInputDevice* pDevice = *it;

		if (id == pDevice->GetDeviceIndex() && pDevice->GetDeviceType() == deviceType)
			return pDevice;
	}

	return NULL;
}
