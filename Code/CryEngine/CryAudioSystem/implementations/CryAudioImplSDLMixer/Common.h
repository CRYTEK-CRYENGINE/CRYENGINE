// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <queue>

struct Mix_Chunk;

namespace CryAudio
{
class CTransformation;
namespace Impl
{
namespace SDL_mixer
{
class CEventInstance;
class CImpl;
class CObject;
class CEvent;

extern bool g_isMuted;
extern CImpl* g_pImpl;

using SampleId = uint;
using ChannelList = std::vector<int>;
using EventInstances = std::vector<CEventInstance*>;

constexpr int g_numMixChannels = 512;
constexpr SampleId g_invalidSampleId = 0;

using Objects = std::vector<CObject*>;
extern Objects g_objects;

using SampleDataMap = std::unordered_map<SampleId, Mix_Chunk*>;
extern SampleDataMap g_sampleData;

using SampleNameMap = std::unordered_map<SampleId, string>;
extern SampleNameMap g_samplePaths;

using TChannelQueue = std::queue<int>;
extern TChannelQueue g_freeChannels;

struct SChannelData final
{
	CObject* pObject;
};

extern SChannelData g_channels[g_numMixChannels];

float GetVolumeMultiplier(CObject* const pObject, SampleId const sampleId);
int   GetAbsoluteVolume(int const eventVolume, float const multiplier);
void  GetDistanceAngleToObject(
	CTransformation const& listenerTransformation,
	CTransformation const& objectTransformation,
	float& distance,
	float& angle);
void SetChannelPosition(CEvent const& event, int const channelID, float const distance, float const angle);
bool LoadSampleImpl(SampleId const id, string const& samplePath);

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
extern size_t g_loadedSampleSize;
#endif // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE
}      // namespace SDL_mixer
}      // namespace Impl
}      // namespace CryAudio