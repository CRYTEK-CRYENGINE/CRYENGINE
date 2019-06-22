// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioInterfacesCommonData.h>
#include <CryMath/Cry_Color.h>
#include <CryRenderer/IRenderAuxGeom.h>

namespace CryAudio
{
namespace Debug
{
// Global debug draw style.
static ColorF const s_globalColorError = Col_Red;
static ColorF const s_globalColorHeader = Col_Orange;
static ColorF const s_globalColorVirtual = Col_Cyan;
static ColorF const s_globalColorInactive = Col_Grey;

// Debug draw style for system information.
constexpr float g_systemHeaderFontSize = 1.5f;
constexpr float g_systemHeaderLineHeight = 20.0f;
constexpr float g_systemHeaderLineSpacerHeight = 4.0f;
constexpr float g_systemFontSize = 1.35f;
constexpr float g_systemLineHeight = 16.0f;
static ColorF const s_systemColorTextPrimary = Col_LightGrey;
static ColorF const s_systemColorTextSecondary = Col_CornflowerBlue;
static ColorF const s_systemColorListenerActive = Col_LimeGreen;
static ColorF const s_systemColorListenerInactive = Col_OrangeRed;

// Debug draw style for objects.
constexpr float g_objectFontSize = 1.35f;
constexpr float g_objectLineHeight = 14.0f;
static ColorF const s_objectColorActive = Col_LightGrey;
static ColorF const s_objectColorTrigger = Col_LimeGreen;
static ColorF const s_objectColorParameter = Col_SlateBlue;
static ColorF const s_objectColorEnvironment = Col_Orange;

// Debug draw style for lists.
constexpr float g_listHeaderFontSize = 1.5f;
constexpr float g_listHeaderLineHeight = 16.0f;
constexpr float g_listFontSize = 1.25f;
constexpr float g_listLineHeight = 11.0f;
static ColorF const s_listColorItemActive = Col_LimeGreen;
static ColorF const s_listColorItemLoading = Col_OrangeRed;
static ColorF const s_listColorItemStopping = Col_Yellow;

constexpr uint8 MaxMemInfoStringLength = 16;

//////////////////////////////////////////////////////////////////////////
static void FormatMemoryString(CryFixedStringT<MaxMemInfoStringLength>& string, size_t const size)
{
	(size < 1024) ? (string.Format("%" PRISIZE_T " Byte", size)) : (string.Format("%" PRISIZE_T " KiB", size >> 10));
}

//////////////////////////////////////////////////////////////////////////
static void DrawMemoryPoolInfo(
	IRenderAuxGeom& auxGeom,
	float const posX,
	float& posY,
	size_t memAlloc,
	stl::SMemoryUsage const& pool,
	char const* const szType,
	uint16 const preallocated)
{
	CryFixedStringT<MaxMemInfoStringLength> memAllocString;
	FormatMemoryString(memAllocString, memAlloc);

	ColorF const& color = (static_cast<uint16>(pool.nUsed) > preallocated) ? s_globalColorError : s_systemColorTextPrimary;

	posY += g_systemLineHeight;
	auxGeom.Draw2dLabel(posX, posY, g_systemFontSize, color, false,
	                    "[%s] Constructed: %" PRISIZE_T " | Allocated: %" PRISIZE_T " | Preallocated: %u | Pool Size: %s",
	                    szType, pool.nUsed, pool.nAlloc, preallocated, memAllocString.c_str());
}
} // namespace Debug
} // namespace CryAudio
