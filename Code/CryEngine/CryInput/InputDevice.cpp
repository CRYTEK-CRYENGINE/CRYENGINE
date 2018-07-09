// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "InputDevice.h"

#if !defined(RELEASE)
	#include <CryRenderer/IRenderer.h>
#endif

#if !defined(RELEASE)
	#include "InputCVars.h"
extern CInputCVars* g_pInputCVars;
#endif

#include <CrySystem/ISystem.h>

CInputDevice::CInputDevice(IInput& input, const char* deviceName)
	: m_input(input)
	, m_deviceName(deviceName)
	, m_deviceType(eIDT_Unknown)
	, m_enabled(true)
{
}

CInputDevice::~CInputDevice()
{
	while (m_idToInfo.size())
	{
		TIdToSymbolMap::iterator iter = m_idToInfo.begin();
		SInputSymbol* pSymbol = (*iter).second;
		m_idToInfo.erase(iter);
		SAFE_DELETE(pSymbol);
	}
}

void CInputDevice::Update(bool bFocus)
{
}

bool CInputDevice::InputState(const TKeyName& keyName, EInputState state)
{
	SInputSymbol* pSymbol = NameToSymbol(keyName);
	if (pSymbol && pSymbol->state == state)
		return true;

	return false;
}

void CInputDevice::ClearKeyState()
{
	for (TIdToSymbolMap::iterator i = m_idToInfo.begin(); i != m_idToInfo.end(); ++i)
	{
		SInputSymbol* pSymbol = (*i).second;
		if (pSymbol && pSymbol->value > 0.0)
		{
			SInputEvent event;
			event.deviceType = m_deviceType;
			event.keyName = pSymbol->name;
			event.keyId = pSymbol->keyId;
			event.state = eIS_Released;
			event.value = 0.0f;
			event.pSymbol = pSymbol;
			pSymbol->value = 0.0f;
			pSymbol->state = eIS_Released;
			m_input.PostInputEvent(event);
		}
	}
}

void CInputDevice::ClearAnalogKeyState(TInputSymbols& clearedSymbols)
{

}

const char* CInputDevice::GetKeyName(const SInputEvent& event) const
{
	return GetKeyName(event.keyId);
}

const char* CInputDevice::GetKeyName(const EKeyId keyId) const
{
	TIdToSymbolMap::const_iterator iter = m_idToInfo.find(keyId);
	if (iter == m_idToInfo.end())
		return NULL;

	const SInputSymbol* pInputSymbol = iter->second;
	CRY_ASSERT(pInputSymbol != NULL);

	return pInputSymbol->name.c_str();
}

uint32 CInputDevice::GetInputCharUnicode(const SInputEvent& event)
{
	return '\0';
}

const char* CInputDevice::GetOSKeyName(const SInputEvent& event)
{
	return "";
}

/*
   const TKeyName& CInputDevice::IdToName(TKeyId id) const
   {
   static TKeyName sUnknown("<unknown>");

   TIdToSymbolMap::const_iterator i = m_idToInfo.find(id);
   if (i != m_idToInfo.end())
    return (*i).second->name;
   else
    return sUnknown;
   }
 */

SInputSymbol* CInputDevice::IdToSymbol(EKeyId id) const
{
	TIdToSymbolMap::const_iterator i = m_idToInfo.find(id);
	if (i != m_idToInfo.end())
		return (*i).second;
	else
		return 0;
}

SInputSymbol* CInputDevice::DevSpecIdToSymbol(uint32 devSpecId) const
{
	TDevSpecIdToSymbolMap::const_iterator i = m_devSpecIdToSymbol.find(devSpecId);
	if (i != m_devSpecIdToSymbol.end())
		return (*i).second;
	else
		return 0;
}

uint32 CInputDevice::NameToId(const TKeyName& name) const
{
	TNameToIdMap::const_iterator i = m_nameToId.find(name);
	if (i != m_nameToId.end())
		return (*i).second;
	else
		return 0xffffffff;
}

SInputSymbol* CInputDevice::NameToSymbol(const TKeyName& name) const
{
	TNameToSymbolMap::const_iterator i = m_nameToInfo.find(name);
	if (i != m_nameToInfo.end())
		return (*i).second;
	else
		return 0;
}

SInputSymbol* CInputDevice::MapSymbol(uint32 deviceSpecificId, EKeyId keyId, const TKeyName& name, SInputSymbol::EType type, uint32 user)
{
	SInputSymbol* pSymbol = new SInputSymbol(deviceSpecificId, keyId, name, type);
	pSymbol->user = user;
	pSymbol->deviceType = m_deviceType;
	m_idToInfo[keyId] = pSymbol;
	m_devSpecIdToSymbol[deviceSpecificId] = pSymbol;
	m_nameToId[name] = deviceSpecificId;
	m_nameToInfo[name] = pSymbol;

	return pSymbol;
}

//////////////////////////////////////////////////////////////////////////
SInputSymbol* CInputDevice::LookupSymbol(EKeyId id) const
{
	return IdToSymbol(id);
}

const SInputSymbol* CInputDevice::GetSymbolByName(const char* name) const
{
	TKeyName tKeyName(name);
	TNameToSymbolMap::const_iterator i = m_nameToInfo.find(tKeyName);
	if (i != m_nameToInfo.end())
		return (*i).second;
	else
		return 0;
}

void CInputDevice::Enable(bool enable)
{
	m_enabled = enable;
}

// ------------------------------------------------------------------------
// DEBUG CONTROLLER BUTTONS
// ------------------------------------------------------------------------
#if !defined(RELEASE)
namespace // anonymous
{
const string s_kButtonA = "xi_a";
const string s_kButtonB = "xi_b";
const string s_kButtonX = "xi_x";
const string s_kButtonY = "xi_y";

const ColorF s_KColA(0.f, 1.f, 0.f, 1.f);
const ColorF s_KColB(1.f, 0.f, 0.f, 1.f);
const ColorF s_KColX(0.39f, 0.58f, 0.93f, 1.f);
const ColorF s_KColY(1.f, 0.6f, 0.f, 1.f);
}
// ------------------------------------------------------------------------
CInputDevice::CDebugPressedButtons::SData::SData(const SInputSymbol* pSymbol_, uint32 frame_)
{
	frame = 0;
	if (pSymbol_)
	{
		frame = frame_;

		switch (pSymbol_->state)
		{
		case eIS_Unknown:
			state = "Unknown";
			break;
		case eIS_Pressed:
			state = "Pressed";
			break;
		case eIS_Released:
			state = "Released";
			break;
		case eIS_Down:
			state = "Down";
			break;
		case eIS_Changed:
			state = "Changed";
			break;
		default:
			state = "?State?";
		}
		key = pSymbol_->name.c_str();
		if (key == s_kButtonA)
		{
			color = s_KColA;
			key = "A";
		}
		else if (key == s_kButtonB)
		{
			color = s_KColB;
			key = "B";
		}
		else if (key == s_kButtonX)
		{
			color = s_KColX;
			key = "X";
		}
		else if (key == s_kButtonY)
		{
			color = s_KColY;
			key = "Y";
		}
		else
		{
			color = ColorF(1.f, 1.f, 0.f, 1.f);
		}
	}
}

// ------------------------------------------------------------------------
void CInputDevice::CDebugPressedButtons::Add(const SInputSymbol* pSymbol)
{
	if (g_pInputCVars->i_debugDigitalButtons && pSymbol)
	{
		if (pSymbol->state != eIS_Changed || ((g_pInputCVars->i_debugDigitalButtons & eDF_LogChangeState) != 0))
		{
			m_history.insert(m_history.begin(), SData(pSymbol, m_frameCnt));
			if (m_history.size() > e_MaxNumEntries)
			{
				m_history.resize(e_MaxNumEntries);
			}
		}
	}
}

	#include <CryRenderer/IRenderAuxGeom.h>

// ------------------------------------------------------------------------
void CInputDevice::CDebugPressedButtons::DebugRender()
{
	if (g_pInputCVars->i_debugDigitalButtons)
	{
		static float deltaY = 15.f;
		static float startX = 50;
		static float startY = 400.f;
		static float fontSize = 1.2f;

		ColorF colDefault(1.f, 1.f, 0.f, 1.f);
		m_textPos2d.x = startX;
		m_textPos2d.y = startY;
		IRenderAuxText::Draw2dLabel(m_textPos2d.x, m_textPos2d.y, 1.1f, ColorF(0.f, 1.f, 0.f, 1.f), false, "Controller's Digital Buttons Activity");
		m_textPos2d.y += deltaY;
		IRenderAuxText::Draw2dLabel(m_textPos2d.x, m_textPos2d.y, 1.1f, ColorF(1.f, 1.f, 1.f, 1.f), false, "CurrentFrame:[%u]", m_frameCnt);
		m_textPos2d.y += deltaY;

		for (size_t i = 0, kSize = m_history.size(); i < kSize; ++i)
		{
			IRenderAuxText::Draw2dLabel(m_textPos2d.x, m_textPos2d.y, fontSize, m_history[i].color, false, "[%u] %s [%s]", m_history[i].frame, m_history[i].key.c_str(), m_history[i].state.c_str());
			m_textPos2d.y += deltaY;
		}
		++m_frameCnt;
	}
}
#endif
