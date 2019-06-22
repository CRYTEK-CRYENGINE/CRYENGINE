// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	#include <CryAudio/IAudioInterfacesCommonData.h>
	#include <CryMath/Cry_Color.h>

namespace CryAudio
{
namespace Debug
{
// Filter for drawing debug info to the screen
enum class EDrawFilter : EnumFlagsType
{
	All                       = 0,
	Spheres                   = BIT(6),  // a
	ObjectLabel               = BIT(7),  // b
	ObjectTriggers            = BIT(8),  // c
	ObjectStates              = BIT(9),  // d
	ObjectParameters          = BIT(10), // e
	ObjectEnvironments        = BIT(11), // f
	ObjectDistance            = BIT(12), // g
	OcclusionRayLabels        = BIT(13), // h
	OcclusionRays             = BIT(14), // i
	OcclusionRayOffset        = BIT(15), // j
	OcclusionListenerPlane    = BIT(16), // k
	OcclusionCollisionSpheres = BIT(17), // l
	GlobalPlaybackInfo        = BIT(18), // m
	ObjectImplInfo            = BIT(19), // n

	HideMemoryInfo            = BIT(22), // q
	FilterAllObjectInfo       = BIT(23), // r
	DetailedMemoryInfo        = BIT(24), // s

	Contexts                  = BIT(26), // u
	ImplList                  = BIT(27), // v
	ActiveObjects             = BIT(28), // w
	FileCacheManagerInfo      = BIT(29), // x
	RequestInfo               = BIT(30), // y
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EDrawFilter);

constexpr EDrawFilter objectMask =
	EDrawFilter::Spheres |
	EDrawFilter::ObjectLabel |
	EDrawFilter::ObjectTriggers |
	EDrawFilter::ObjectStates |
	EDrawFilter::ObjectParameters |
	EDrawFilter::ObjectEnvironments |
	EDrawFilter::ObjectDistance |
	EDrawFilter::OcclusionRayLabels |
	EDrawFilter::OcclusionRays |
	EDrawFilter::OcclusionRayOffset |
	EDrawFilter::OcclusionListenerPlane |
	EDrawFilter::OcclusionCollisionSpheres |
	EDrawFilter::ObjectImplInfo;

// Debug draw style for objects.
constexpr float g_objectRadiusPositionSphere = 0.15f;
constexpr float g_objectColorSwitchGreenMin = 0.3f;
constexpr float g_objectColorSwitchGreenMax = 1.0f;
constexpr float g_objectColorSwitchUpdateValue = (g_objectColorSwitchGreenMax - g_objectColorSwitchGreenMin) / 100.0f;
static ColorF const s_objectColorPositionSphere = Col_Red;
static ColorF const s_objectColorOcclusionOffsetSphere = { Col_LimeGreen, 0.4f };
constexpr char const* g_szOcclusionTypes[] = { "None", "Ignore", "Adaptive", "Low", "Medium", "High" };

// Debug draw style for rays.
constexpr float g_rayRadiusCollisionSphere = 0.025f;
static ColorF const s_rayColorObstructed = Col_Red;
static ColorF const s_rayColorFree = Col_LimeGreen;

// Debug draw style for file cache manager.
static ColorF const s_afcmColorContextGlobal = Col_LimeGreen;
static ColorF const s_afcmColorContextUserDefined = Col_Yellow;
static ColorF const s_afcmColorFileLoading = Col_OrangeRed;
static ColorF const s_afcmColorFileMemAllocFail = Col_VioletRed;
static ColorF const s_afcmColorFileRemovable = Col_Cyan;
static ColorF const s_afcmColorFileNotCached = Col_Grey;
static ColorF const s_afcmColorFileNotFound = Col_Red;

class CStateDrawData final
{
public:

	CStateDrawData() = delete;
	CStateDrawData(CStateDrawData const&) = delete;
	CStateDrawData(CStateDrawData&&) = delete;
	CStateDrawData& operator=(CStateDrawData const&) = delete;
	CStateDrawData& operator=(CStateDrawData&&) = delete;

	explicit CStateDrawData(SwitchStateId const switchStateId)
		: m_currentStateId(switchStateId)
		, m_currentSwitchColor(Debug::g_objectColorSwitchGreenMax)
	{}

	void Update(SwitchStateId const switchStateId)
	{
		if ((switchStateId == m_currentStateId) && (m_currentSwitchColor > Debug::g_objectColorSwitchGreenMin))
		{
			m_currentSwitchColor -= Debug::g_objectColorSwitchUpdateValue;
		}
		else if (switchStateId != m_currentStateId)
		{
			m_currentStateId = switchStateId;
			m_currentSwitchColor = Debug::g_objectColorSwitchGreenMax;
		}
	}

	SwitchStateId m_currentStateId;
	float         m_currentSwitchColor;
};

using StateDrawInfo = std::map<ControlId, CStateDrawData>;
using TriggerCounts = std::map<ControlId const, uint8>;
}      // namespace Debug
}      // namespace CryAudio
#endif // CRY_AUDIO_USE_DEBUG_CODE