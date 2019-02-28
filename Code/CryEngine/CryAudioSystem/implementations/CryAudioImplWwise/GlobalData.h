// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
static constexpr char const* s_szImplFolderName = "wwise";

// XML tags
static constexpr char const* s_szParameterTag = "Parameter";
static constexpr char const* s_szSwitchGroupTag = "SwitchGroup";
static constexpr char const* s_szStateGroupTag = "StateGroup";
static constexpr char const* s_szFileTag = "File";
static constexpr char const* s_szAuxBusTag = "AuxBus";
static constexpr char const* s_szValueTag = "Value";

// XML attributes
static constexpr char const* s_szValueAttribute = "value";
static constexpr char const* s_szMutiplierAttribute = "value_multiplier";
static constexpr char const* s_szShiftAttribute = "value_shift";
static constexpr char const* s_szLocalizedAttribute = "localized";

// XML values
static constexpr char const* s_szTrueValue = "true";

// Default values
static constexpr float const s_defaultParamMultiplier = 1.0f;
static constexpr float const s_defaultParamShift = 0.0f;
static constexpr float const s_defaultStateValue = 0.0f;
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio
