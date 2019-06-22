// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Common.h"
#include "Object.h"
#include "Event.h"
#include <CryAudio/IAudioInterfacesCommonData.h>

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	#include <Logger.h>
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

#include <SDL_mixer.h>

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
constexpr uint8 g_sdlMaxDistance = 255;

bool g_isMuted = false;
CImpl* g_pImpl = nullptr;
Objects g_objects;
SampleDataMap g_sampleData;
SampleNameMap g_samplePaths;
TChannelQueue g_freeChannels;
SChannelData g_channels[g_numMixChannels];

//////////////////////////////////////////////////////////////////////////
int GetAbsoluteVolume(int const eventVolume, float const multiplier)
{
	int absoluteVolume = static_cast<int>(static_cast<float>(eventVolume) * multiplier);
	absoluteVolume = crymath::clamp(absoluteVolume, 0, 128);
	return absoluteVolume;
}

//////////////////////////////////////////////////////////////////////////
float GetVolumeMultiplier(CObject* const pObject, SampleId const sampleId)
{
	float volumeMultiplier = 1.0f;
	auto const volumeMultiplierPair = pObject->m_volumeMultipliers.find(sampleId);

	if (volumeMultiplierPair != pObject->m_volumeMultipliers.end())
	{
		volumeMultiplier = volumeMultiplierPair->second;
	}

	return volumeMultiplier;
}

//////////////////////////////////////////////////////////////////////////
void GetDistanceAngleToObject(
	CTransformation const& listenerTransformation,
	CTransformation const& objectTransformation,
	float& distance,
	float& angle)
{
	Vec3 const listenerToObject = objectTransformation.GetPosition() - listenerTransformation.GetPosition();

	// Distance
	distance = listenerToObject.len();

	// Angle
	// Project point to plane formed by the listeners position/direction
	Vec3 const listenerUpDir = listenerTransformation.GetUp().GetNormalized();
	Vec3 const objectDir = Vec3::CreateProjection(listenerToObject, listenerUpDir).normalized();

	// Get angle between listener position and projected point
	Vec3 const listenerDir = listenerTransformation.GetForward().GetNormalizedFast();
	angle = RAD2DEG(asin_tpl(objectDir.Cross(listenerDir).Dot(listenerUpDir)));
}

//////////////////////////////////////////////////////////////////////////
void SetChannelPosition(CEvent const& event, int const channelID, float const distance, float const angle)
{
	uint8 distanceToSet = 0;

	if ((event.GetFlags() & EEventFlags::IsAttenuationEnnabled) != 0)
	{
		float const minDistance = event.GetAttenuationMinDistance();

		if (distance > minDistance)
		{
			float const maxDistance = event.GetAttenuationMaxDistance();

			if (minDistance != maxDistance)
			{
				float const finalDistance = distance - minDistance;
				float const range = maxDistance - minDistance;
				distanceToSet = static_cast<uint8>((std::min((finalDistance / range), 1.0f) * g_sdlMaxDistance) + 0.5f);
			}
			else
			{
				distanceToSet = g_sdlMaxDistance;
			}
		}
	}

	// Temp code, to be reviewed during the SetChannelPosition rewrite:
	Mix_SetDistance(channelID, distanceToSet);

	if ((event.GetFlags() & EEventFlags::IsPanningEnabled) != 0)
	{
		// Temp code, to be reviewed during the SetChannelPosition rewrite:
		float const absAngle = fabs(angle);
		float const frontAngle = (angle > 0.0f ? 1.0f : -1.0f) * (absAngle > 90.0f ? 180.f - absAngle : absAngle);
		float const rightVolume = (frontAngle + 90.0f) / 180.0f;
		float const leftVolume = 1.0f - rightVolume;
		Mix_SetPanning(channelID,
		               static_cast<uint8>(255.0f * leftVolume),
		               static_cast<uint8>(255.0f * rightVolume));
	}
}

//////////////////////////////////////////////////////////////////////////
SampleId LoadSampleFromMemory(void* pMemory, size_t const size, string const& samplePath, SampleId const overrideId)
{
	SampleId const id = (overrideId != g_invalidSampleId) ? overrideId : StringToId(samplePath.c_str());
	Mix_Chunk* pSample = stl::find_in_map(g_sampleData, id, nullptr);

	if (pSample != nullptr)
	{
		Mix_FreeChunk(pSample);

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
		Cry::Audio::Log(ELogType::Warning, "Loading sample %s which had already been loaded", samplePath.c_str());
#endif    // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE
	}

	SDL_RWops* const pData = SDL_RWFromMem(pMemory, size);

	if (pData != nullptr)
	{
		pSample = Mix_LoadWAV_RW(pData, 0);

		if (pSample != nullptr)
		{
			g_sampleData[id] = pSample;
			g_samplePaths[id] = samplePath;

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
			g_loadedSampleSize += size;
#endif      // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

			return id;
		}
#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Error, R"(SDL Mixer failed to load sample. Error: "%s")", Mix_GetError());
		}
#endif    // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE
	}
#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Error, R"(SDL Mixer failed to transform the audio data. Error: "%s")", SDL_GetError());
	}
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

	return g_invalidSampleId;
}

//////////////////////////////////////////////////////////////////////////
bool LoadSampleImpl(SampleId const id, string const& samplePath)
{
	bool isLoaded = true;
	Mix_Chunk* const pSample = Mix_LoadWAV(samplePath.c_str());

	if (pSample != nullptr)
	{
#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
		SampleNameMap::const_iterator it = g_samplePaths.find(id);

		if ((it != g_samplePaths.end()) && (it->second != samplePath))
		{
			Cry::Audio::Log(ELogType::Error, "Loaded a Sample with the already existing ID %u, but from a different path source path '%s' <-> '%s'.", static_cast<uint>(id), it->second.c_str(), samplePath.c_str());
		}

		if (stl::find_in_map(g_sampleData, id, nullptr) != nullptr)
		{
			Cry::Audio::Log(ELogType::Error, "Loading sample '%s' which had already been loaded", samplePath.c_str());
		}
#endif    // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE
		g_sampleData[id] = pSample;
		g_samplePaths[id] = samplePath;
	}
	else
	{
		// Sample could be inside a pak file so we need to open it manually and load it from the raw file
		size_t const fileSize = gEnv->pCryPak->FGetSize(samplePath);
		FILE* const pFile = gEnv->pCryPak->FOpen(samplePath, "rb");

		if (pFile && (fileSize > 0))
		{
			void* const pData = CryModuleMalloc(fileSize);
			gEnv->pCryPak->FReadRawAll(pData, fileSize, pFile);
			SampleId const newId = LoadSampleFromMemory(pData, fileSize, samplePath, id);

			if (newId == g_invalidSampleId)
			{
#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
				Cry::Audio::Log(ELogType::Error, R"(SDL Mixer failed to load sample %s. Error: "%s")", samplePath.c_str(), Mix_GetError());
#endif        // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

				isLoaded = false;
			}

			CryModuleFree(pData);
		}
	}

	return isLoaded;
}

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
size_t g_loadedSampleSize = 0;
#endif // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE
}      // namespace SDL_mixer
}      // namespace Impl
}      // namespace CryAudio
