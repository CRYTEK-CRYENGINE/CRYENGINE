// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace CryAudio
{
namespace Impl
{
namespace PortAudio
{
static constexpr char const* s_szImplFolderName = "portaudio";

// XML tags
static constexpr char const* s_szFileTag = "Sample";

// XML attributes
static constexpr char const* s_szPathAttribute = "path";
static constexpr char const* s_szLoopCountAttribute = "loop_count";

// XML values
static constexpr char const* s_szStartValue = "start";
static constexpr char const* s_szStopValue = "stop";
} // namespace PortAudio
} // namespace Impl
} // namespace CryAudio
