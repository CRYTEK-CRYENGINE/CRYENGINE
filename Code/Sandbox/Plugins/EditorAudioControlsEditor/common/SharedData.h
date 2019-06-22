// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <vector>
#include <memory>
#include <CryAudio/IAudioSystem.h>

namespace ACE
{
using ControlId = CryAudio::ControlId;
using ControlIds = std::vector<ControlId>;

constexpr ControlId g_invalidControlId = 0;
constexpr float g_precision = 0.0001f;

enum class EAssetType : CryAudio::EnumFlagsType
{
	None,
	Trigger,
	Parameter,
	Switch,
	State,
	Environment,
	Preload,
	Setting,
	Folder,
	Library,
	NumTypes };

enum class EPakStatus : CryAudio::EnumFlagsType
{
	None = 0,
	InPak = BIT(0),
	OnDisk = BIT(1), };
CRY_CREATE_ENUM_FLAG_OPERATORS(EPakStatus);

enum class EItemFlags : CryAudio::EnumFlagsType
{
	None = 0,
	IsPlaceHolder = BIT(0),
	IsLocalized = BIT(1),
	IsConnected = BIT(2),
	IsContainer = BIT(3), };
CRY_CREATE_ENUM_FLAG_OPERATORS(EItemFlags);

enum class EImplInfoFlags : CryAudio::EnumFlagsType
{
	None = 0,
	SupportsProjects = BIT(0),
	SupportsFileImport = BIT(1),
	SupportsTriggers = BIT(2),
	SupportsParameters = BIT(3),
	SupportsSwitches = BIT(4),
	SupportsStates = BIT(5),
	SupportsEnvironments = BIT(6),
	SupportsPreloads = BIT(7),
	SupportsSettings = BIT(8), };
CRY_CREATE_ENUM_FLAG_OPERATORS(EImplInfoFlags);

struct SImplInfo final
{
	CryFixedStringT<CryAudio::MaxInfoStringLength> name;
	CryFixedStringT<CryAudio::MaxInfoStringLength> folderName;
	CryFixedStringT<CryAudio::MaxFilePathLength>   projectPath;
	CryFixedStringT<CryAudio::MaxFilePathLength>   assetsPath;
	CryFixedStringT<CryAudio::MaxFilePathLength>   localizedAssetsPath;
	EImplInfoFlags flags;
};
} // namespace ACE
