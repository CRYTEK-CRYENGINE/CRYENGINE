// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "GlobalAnimationHeaderAIM.h"

#include "CharacterManager.h"
#include "Model.h"

//////////////////////////////////////////////////////////////////////////
/**
 * Checks if time keys stored within the given IKeyTimesInformation object are in a strictly ascending order.
 */
static bool ValidateTimeKeys(const IKeyTimesInformation& timeKeys)
{
	const uint32_t keysCount = timeKeys.GetNumKeys();

	float lastTime = -std::numeric_limits<float>::max();
	for (uint32 i = 0; i < keysCount; ++i)
	{
		const float time = timeKeys.GetKeyValueFloat(i);
		if (time <= lastTime)
		{
			return false;
		}
		lastTime = time;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
uint32 GlobalAnimationHeaderAIM::LoadAIM()
{
	_smart_ptr<IChunkFile> pChunkFile = g_pI3DEngine->CreateChunkFile(true);

	m_nFlags = 0;
	OnAssetNotFound();

	stack_string strPath = m_FilePath.c_str();
	PathUtil::UnifyFilePath(strPath);

	if (!pChunkFile->Read(m_FilePath))
	{
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_ERROR, VALIDATOR_FLAG_FILE, GetFilePath(), "%s", pChunkFile->GetLastError());
		return 0;
	}

	if (!LoadChunks(pChunkFile))
	{
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_ERROR, VALIDATOR_FLAG_FILE, GetFilePath(), "File is malformed.");
		return 0;
	}

	if (m_arrController.empty())
	{
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, GetFilePath(), "No controllers have been loaded from this file.");
	}

	std::sort(m_arrController.begin(), m_arrController.end(), AnimCtrlSortPred());
	OnAssetCreated();
	ClearAssetRequested();
	ClearAssetNotFound();
	m_nControllers = m_arrController.size();
	return 1;
}

bool GlobalAnimationHeaderAIM::LoadChunks(IChunkFile* pChunkFile)
{
	const uint32 numChunks = pChunkFile->NumChunks();

	uint32 numCompressedChunks = 0;
	uint32 numUncompressedChunks = 0;

	for (uint32 i = 0; i < numChunks; i++)
	{
		const IChunkFile::ChunkDesc* const pChunkDesc = pChunkFile->GetChunk(i);

		if (pChunkDesc->chunkType == ChunkType_Controller)
		{
			switch (pChunkDesc->chunkVersion)
			{
			case CONTROLLER_CHUNK_DESC_0826::VERSION:
				g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_ERROR, VALIDATOR_FLAG_FILE, GetFilePath(), "File contains TCB controller chunks which are not supported by the Aim/LookPoses system.");
				return false;

			case CONTROLLER_CHUNK_DESC_0827::VERSION:
			case CONTROLLER_CHUNK_DESC_0833::VERSION:
				numUncompressedChunks++;
				break;

			case CONTROLLER_CHUNK_DESC_0829::VERSION:
			case CONTROLLER_CHUNK_DESC_0832::VERSION:
				numCompressedChunks++;
				break;

			default:
				g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_ERROR, VALIDATOR_FLAG_FILE, GetFilePath(), "File contains an unsupported controller chunk version: 0x%04x", pChunkDesc->chunkVersion);
				return false;
			}
		}
	}

	if (Console::GetInst().ca_AnimWarningLevel > 2 && numUncompressedChunks > 0)
	{
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, GetFilePath(), "File contains uncompressed controller data.");
	}
	if (Console::GetInst().ca_AnimWarningLevel > 2 && numCompressedChunks == 0)
	{
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, GetFilePath(), "File does not contain any compressed controller data.");
	}

	// Only uncompressed data will be loaded.
	const bool bLoadUncompressedChunks = (numCompressedChunks == 0);

	m_arrController.reserve(bLoadUncompressedChunks ? numUncompressedChunks : numCompressedChunks);
	m_arrController.resize(0);

	for (uint32 i = 0; i < numChunks; i++)
	{
		IChunkFile::ChunkDesc* const pChunkDesc = pChunkFile->GetChunk(i);

		switch (pChunkDesc->chunkType)
		{
		case ChunkType_MotionParameters:
			if (!ReadMotionParameters(pChunkDesc))
			{
				return false;
			}
			break;

		case ChunkType_Controller:
			if (!ReadController(pChunkDesc, bLoadUncompressedChunks))
			{
				return false;
			}
			break;

		case ChunkType_Timing:
			if (!ReadTiming(pChunkDesc))
			{
				return false;
			}
			break;
		}
	}

	return true;
}

uint32 GlobalAnimationHeaderAIM::FlagsSanityFilter(uint32 flags)
{
	// sanity check
	uint32 validFlags = (CA_ASSET_BIG_ENDIAN | CA_ASSET_LOADED | CA_ASSET_CREATED | CA_AIMPOSE | CA_ASSET_NOT_FOUND);
	if (flags & ~validFlags)
	{
		CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_WARNING, "Badly exported animation-asset: flags: %08x %s", flags, this->GetFilePath());
		flags &= validFlags;
	}
	return flags;
}

//////////////////////////////////////////////////////////////////////////
bool GlobalAnimationHeaderAIM::ReadMotionParameters(IChunkFile::ChunkDesc* pChunkDesc)
{

	if (pChunkDesc->chunkVersion == SPEED_CHUNK_DESC_2::VERSION)
	{
		//	CryFatalError("SPEED_CHUNK_DESC_2 not supported any more");
		SPEED_CHUNK_DESC_2* pChunk = (SPEED_CHUNK_DESC_2*)pChunkDesc->data;
		SwapEndian(*pChunk);

		m_nFlags = FlagsSanityFilter(pChunk->AnimFlags);

		return true;
	}

	if (pChunkDesc->chunkVersion == CHUNK_MOTION_PARAMETERS::VERSION)
	{
		if (pChunkDesc->bSwapEndian)
		{
			CryFatalError("%s: data are stored in non-native endian format", __FUNCTION__);
		}

		CHUNK_MOTION_PARAMETERS* pChunk = (CHUNK_MOTION_PARAMETERS*)pChunkDesc->data;

		m_nFlags = FlagsSanityFilter(pChunk->mp.m_nAssetFlags);
		//m_pSkinningInfo->m_nCompression		= pChunk->mp.m_nCompression;

		int32 nStartKey = pChunk->mp.m_nStart;
		int32 nEndKey = pChunk->mp.m_nEnd;
		if (pChunk->mp.m_nAssetFlags & CA_ASSET_ADDITIVE)
			nStartKey++;
		m_fStartSec = nStartKey / ANIMATION_30Hz;
		m_fEndSec = nEndKey / ANIMATION_30Hz;
		if (m_fEndSec <= m_fStartSec)
			m_fEndSec = m_fStartSec;
		m_fTotalDuration = m_fEndSec - m_fStartSec;

		return true;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool GlobalAnimationHeaderAIM::ReadTiming(IChunkFile::ChunkDesc* pChunkDesc)
{
	switch (pChunkDesc->chunkVersion)
	{

	case TIMING_CHUNK_DESC_0918::VERSION:
		{
			const auto pChunk = static_cast<TIMING_CHUNK_DESC_0918*>(pChunkDesc->data);
			SwapEndian(*pChunk, pChunkDesc->bSwapEndian);
			pChunkDesc->bSwapEndian = false;

			const int32 nStartKey = 0;
			const int32 nEndKey = pChunk->global_range.end - pChunk->global_range.start;

			m_fStartSec = nStartKey / ANIMATION_30Hz;
			m_fEndSec = std::max(nStartKey, nEndKey) / ANIMATION_30Hz;
			m_fTotalDuration = m_fEndSec - m_fStartSec;
		}
		return true;

	case TIMING_CHUNK_DESC_0919::VERSION:
		{
			const auto pChunk = static_cast<TIMING_CHUNK_DESC_0919*>(pChunkDesc->data);
			SwapEndian(*pChunk, pChunkDesc->bSwapEndian);
			pChunkDesc->bSwapEndian = false;

			if (pChunk->numberOfSamples <= 0)
			{
				CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, "Timing chunk (0x%04x) declares a nonpositive number of samples (%d). File: '%s'", pChunk->VERSION, pChunk->numberOfSamples, GetFilePath());
				return false;
			}

			if ((std::abs(pChunk->samplesPerSecond - ANIMATION_30Hz) / ANIMATION_30Hz) > std::numeric_limits<float>::epsilon())
			{
				CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, "Timing chunk (0x%04x) declares a currently unsupported framerate value (%.2f fps). Framerate will be overridden to %.2f fps. File: '%s'", pChunk->VERSION, pChunk->samplesPerSecond, ANIMATION_30Hz, GetFilePath());
			}

			m_fStartSec = 0.0f;
			m_fTotalDuration = (pChunk->numberOfSamples - 1) / ANIMATION_30Hz;
			m_fEndSec = m_fStartSec + m_fTotalDuration;
		}
		return true;

	default:
		return false;

	}
}

//////////////////////////////////////////////////////////////////////////
bool GlobalAnimationHeaderAIM::ReadController(IChunkFile::ChunkDesc* pChunkDesc, uint32 bLoadUncompressedChunks)
{
	if (pChunkDesc->chunkVersion == CONTROLLER_CHUNK_DESC_0826::VERSION)
	{
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_ERROR, VALIDATOR_FLAG_FILE, GetFilePath(), "Could not load controller chunk: TCB controller chunks which are not supported by the Aim/LookPoses system.");
		return false;
	}

	if (pChunkDesc->chunkVersion == CONTROLLER_CHUNK_DESC_0827::VERSION)
	{
		if (bLoadUncompressedChunks)
		{
			CONTROLLER_CHUNK_DESC_0827* pCtrlChunk = static_cast<CONTROLLER_CHUNK_DESC_0827*>(pChunkDesc->data);
			const bool bSwapEndianness = pChunkDesc->bSwapEndian;
			SwapEndian(*pCtrlChunk, bSwapEndianness);
			pChunkDesc->bSwapEndian = false;

			const size_t numKeys = pCtrlChunk->numKeys;

			CryKeyPQLog* pCryKey = reinterpret_cast<CryKeyPQLog*>(pCtrlChunk + 1);
			SwapEndian(pCryKey, numKeys, bSwapEndianness);

			std::unique_ptr<CControllerPQLog> pController(new CControllerPQLog);
			pController->m_nControllerId = pCtrlChunk->nControllerId;
			pController->m_arrKeys.resize(numKeys);
			pController->m_arrTimes.resize(numKeys);

			const int startTime = (numKeys > 0) ? pCryKey[0].nTime : 0;
			int lastTime = INT_MIN;
			for (size_t i = 0; i < numKeys; ++i)
			{
				pController->m_arrTimes[i] = (pCryKey[i].nTime - startTime) / TICKS_CONVERT;
				pController->m_arrKeys[i].position = pCryKey[i].vPos / 100.0f;
				pController->m_arrKeys[i].rotationLog = pCryKey[i].vRotLog;
				pController->m_arrKeys[i].scale = Vec3(1.0f);
			}

#ifndef _RELEASE
			if (!std::is_sorted(pCryKey, pCryKey + numKeys, [](const CryKeyPQLog& lhs, const CryKeyPQLog& rhs) { return lhs.nTime < rhs.nTime; }))
			{
				g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_ERROR, VALIDATOR_FLAG_FILE, GetFilePath(), "Could not load controller chunk: controller contains repeated or unsorted time keys.");
				return false;
			}
#endif       //_RELEASE

			m_arrController.push_back(pController.release());
		}

		return true;
	}

	if (pChunkDesc->chunkVersion == CONTROLLER_CHUNK_DESC_0829::VERSION)
	{
		if (!bLoadUncompressedChunks)
		{
			if (pChunkDesc->bSwapEndian)
			{
				g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_ERROR, VALIDATOR_FLAG_FILE, GetFilePath(), "Could not load controller chunk: data has been provided in a non-native endian format.");
				return false;
			}

			const char* pData = static_cast<const char*>(pChunkDesc->data);

			auto pCtrlChunk = reinterpret_cast<const CONTROLLER_CHUNK_DESC_0829*>(pData);
			pData += sizeof(CONTROLLER_CHUNK_DESC_0829);

			const uint32 trackAlignment = pCtrlChunk->TracksAligned ? 4 : 1;

			IRotationController* pRotation = nullptr;
			if (pCtrlChunk->numRotationKeys > 0)
			{
				ITrackRotationStorage* pStorage = ControllerHelper::GetRotationControllerPtr(pCtrlChunk->RotationFormat);
				if (!pStorage)
				{
					return false;
				}
				pData += Align(pStorage->AssignData(pData, pCtrlChunk->numRotationKeys), trackAlignment);

				_smart_ptr<IKeyTimesInformation> pRotTimeKeys = ControllerHelper::GetKeyTimesControllerPtr(pCtrlChunk->RotationTimeFormat);
				if (!pRotTimeKeys)
				{
					return false;
				}
				pData += Align(pRotTimeKeys->AssignKeyTime(pData, pCtrlChunk->numRotationKeys), trackAlignment);

#ifndef _RELEASE
				if (!ValidateTimeKeys(*pRotTimeKeys))
				{
					g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_ERROR, VALIDATOR_FLAG_FILE, GetFilePath(), "Could not load controller chunk: controller contains repeated or unsorted time keys.");
					return false;
				}
#endif         //_RELEASE

				pRotation = new RotationTrackInformation;
				pRotation->SetRotationStorage(pStorage);
				pRotation->SetKeyTimesInformation(pRotTimeKeys);
			}

			IPositionController* pPosition = nullptr;
			if (pCtrlChunk->numPositionKeys > 0)
			{
				ITrackPositionStorage* pStorage = ControllerHelper::GetPositionControllerPtr(pCtrlChunk->PositionFormat);
				if (!pStorage)
				{
					return false;
				}
				pData += Align(pStorage->AssignData(pData, pCtrlChunk->numPositionKeys), trackAlignment);

				_smart_ptr<IKeyTimesInformation> pPosTimeKeys;
				if (pCtrlChunk->PositionKeysInfo == CONTROLLER_CHUNK_DESC_0829::eKeyTimeRotation)
				{
					pPosTimeKeys = pRotation->GetKeyTimesInformation();
				}
				else
				{
					pPosTimeKeys = ControllerHelper::GetKeyTimesControllerPtr(pCtrlChunk->PositionTimeFormat);
					if (!pPosTimeKeys)
					{
						return false;
					}
					pData += Align(pPosTimeKeys->AssignKeyTime(pData, pCtrlChunk->numPositionKeys), trackAlignment);

#ifndef _RELEASE
					if (!ValidateTimeKeys(*pPosTimeKeys))
					{
						g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_ERROR, VALIDATOR_FLAG_FILE, GetFilePath(), "Could not load controller chunk: controller contains repeated or unsorted time keys.");
						return false;
					}
#endif           //_RELEASE
				}

				pPosition = new PositionTrackInformation;
				pPosition->SetPositionStorage(pStorage);
				pPosition->SetKeyTimesInformation(pPosTimeKeys);
			}

			CController* pController = new CController;
			pController->m_nControllerId = pCtrlChunk->nControllerId;
			pController->SetRotationController(pRotation);
			pController->SetPositionController(pPosition);

			m_arrController.push_back(pController);
		}

		return true;
	}

	if (pChunkDesc->chunkVersion == CONTROLLER_CHUNK_DESC_0832::VERSION)
	{
		if (!bLoadUncompressedChunks)
		{
			if (pChunkDesc->bSwapEndian)
			{
				g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_ERROR, VALIDATOR_FLAG_FILE, GetFilePath(), "Could not load controller chunk: data has been provided in a non-native endian format.");
				return false;
			}

			char* pData = static_cast<char*>(pChunkDesc->data);

			auto controllerChunk = reinterpret_cast<const CONTROLLER_CHUNK_DESC_0832&>(*pData);
			pData += sizeof(CONTROLLER_CHUNK_DESC_0832);

			// Consumes bytesCount of input data and returns a pointer to the consumed buffer. Takes care of all memory-related logic.
			auto consumeData = [&pData](size_t bytesCount) -> char*
			{
				char* out = pData;
				pData += Align(bytesCount, 4);
				return out;
			};

			ITrackRotationStorage* pTrackRotStorage = nullptr;
			_smart_ptr<IKeyTimesInformation> pRotTimeKeys = nullptr;
			if (controllerChunk.numRotationKeys > 0)
			{
				const size_t rotationStorageSize = ControllerHelper::GetRotationFormatSizeOf(controllerChunk.rotationFormat) * controllerChunk.numRotationKeys;
				pTrackRotStorage = ControllerHelper::GetRotationControllerPtr(controllerChunk.rotationFormat, consumeData(rotationStorageSize), controllerChunk.numRotationKeys);
				if (!pTrackRotStorage)
				{
					return false;
				}

				const size_t timeStorageSize = ControllerHelper::GetKeyTimesFormatSizeOf(controllerChunk.rotationTimeFormat) * controllerChunk.numRotationKeys;
				pRotTimeKeys = ControllerHelper::GetKeyTimesControllerPtr(controllerChunk.rotationTimeFormat, consumeData(timeStorageSize), controllerChunk.numRotationKeys);
				if (!pRotTimeKeys)
				{
					return false;
				}
			}

			ITrackPositionStorage* pTrackPosStorage = nullptr;
			_smart_ptr<IKeyTimesInformation> pPosTimeKeys = nullptr;
			if (controllerChunk.numPositionKeys > 0)
			{
				const size_t positionStorageSize = ControllerHelper::GetPositionsFormatSizeOf(controllerChunk.positionFormat) * controllerChunk.numPositionKeys;
				pTrackPosStorage = ControllerHelper::GetPositionControllerPtr(controllerChunk.positionFormat, consumeData(positionStorageSize), controllerChunk.numPositionKeys);
				if (!pTrackPosStorage)
				{
					return false;
				}

				if (controllerChunk.positionKeysInfo == CONTROLLER_CHUNK_DESC_0832::eKeyTimePosition)
				{
					const size_t timeStorageSize = ControllerHelper::GetKeyTimesFormatSizeOf(controllerChunk.positionTimeFormat) * controllerChunk.numPositionKeys;
					pPosTimeKeys = ControllerHelper::GetKeyTimesControllerPtr(controllerChunk.positionTimeFormat, consumeData(timeStorageSize), controllerChunk.numPositionKeys);
					if (!pPosTimeKeys)
					{
						return false;
					}
				}
			}

			ITrackPositionStorage* pTrackSclStorage = nullptr;
			_smart_ptr<IKeyTimesInformation> pSclTimeKeys = nullptr;
			if (controllerChunk.numScaleKeys > 0)
			{
				const size_t scalingStorageSize = ControllerHelper::GetScalingFormatSizeOf(controllerChunk.scaleFormat) * controllerChunk.numScaleKeys;
				pTrackSclStorage = ControllerHelper::GetScalingControllerPtr(controllerChunk.scaleFormat, consumeData(scalingStorageSize), controllerChunk.numScaleKeys);
				if (!pTrackSclStorage)
				{
					return false;
				}

				if (controllerChunk.scaleKeysInfo == CONTROLLER_CHUNK_DESC_0832::eKeyTimeScale)
				{
					const size_t timeStorageSize = ControllerHelper::GetKeyTimesFormatSizeOf(controllerChunk.scaleTimeFormat) * controllerChunk.numScaleKeys;
					pSclTimeKeys = ControllerHelper::GetKeyTimesControllerPtr(controllerChunk.scaleTimeFormat, consumeData(timeStorageSize), controllerChunk.numScaleKeys);
					if (!pSclTimeKeys)
					{
						return false;
					}
				}
			}

			IRotationController* pRotation = nullptr;
			if (controllerChunk.numRotationKeys > 0)
			{
				pRotation = new RotationTrackInformation;
				pRotation->SetRotationStorage(pTrackRotStorage);
				pRotation->SetKeyTimesInformation(pRotTimeKeys);
			}

			IPositionController* pPosition = nullptr;
			if (controllerChunk.numPositionKeys > 0)
			{
				pPosition = new PositionTrackInformation;
				pPosition->SetPositionStorage(pTrackPosStorage);
				pPosition->SetKeyTimesInformation(
				  controllerChunk.positionKeysInfo == CONTROLLER_CHUNK_DESC_0832::eKeyTimeRotation ? pRotTimeKeys :
				  pPosTimeKeys
				  );
			}

			IPositionController* pScaling = nullptr;
			if (controllerChunk.numScaleKeys > 0)
			{
				pScaling = new PositionTrackInformation;
				pScaling->SetPositionStorage(pTrackSclStorage);
				pScaling->SetKeyTimesInformation(
				  controllerChunk.scaleKeysInfo == CONTROLLER_CHUNK_DESC_0832::eKeyTimeRotation ? pRotTimeKeys :
				  controllerChunk.scaleKeysInfo == CONTROLLER_CHUNK_DESC_0832::eKeyTimePosition ? pPosTimeKeys :
				  pSclTimeKeys
				  );
			}

			CController* pController = new CController;
			pController->m_nControllerId = controllerChunk.nControllerId;
			pController->SetRotationController(pRotation);
			pController->SetPositionController(pPosition);
			pController->SetScaleController(pScaling);
			m_arrController.push_back(pController);
		}

		return true;
	}

	if (pChunkDesc->chunkVersion == CONTROLLER_CHUNK_DESC_0833::VERSION)
	{
		if (bLoadUncompressedChunks)
		{
			const auto pCtrlChunk = static_cast<CONTROLLER_CHUNK_DESC_0833*>(pChunkDesc->data);
			const bool bSwapEndianness = pChunkDesc->bSwapEndian;
			SwapEndian(*pCtrlChunk, bSwapEndianness);
			pChunkDesc->bSwapEndian = false;

			const size_t numKeys = pCtrlChunk->numKeys;

			const auto pCryKey = reinterpret_cast<CryKeyPQS*>(pCtrlChunk + 1);
			SwapEndian(pCryKey, numKeys, bSwapEndianness);

			std::unique_ptr<CControllerPQLog> pController(new CControllerPQLog);

			pController->m_nControllerId = pCtrlChunk->controllerId;
			pController->m_arrKeys.resize(numKeys);
			pController->m_arrTimes.resize(numKeys);

			for (size_t i = 0; i < numKeys; ++i)
			{
				pController->m_arrTimes[i] = int(i);
				pController->m_arrKeys[i].position = pCryKey[i].position / 100.0f;
				pController->m_arrKeys[i].rotationLog = Quat::log(!pCryKey[i].rotation);
				pController->m_arrKeys[i].scale = pCryKey[i].scale;
			}

			m_arrController.push_back(pController.release());
		}

		return true;
	}

	g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_ERROR, VALIDATOR_FLAG_FILE, GetFilePath(), "Could not load controller chunk: unsupported controller chunk version: 0x%04x", pChunkDesc->chunkVersion);
	return false;
}

//////////////////////////////////////////////////////////////////////////
// Implementation of ICryAnimationSet, holding the information about animations
// and bones for a single model. Animations also include the subclass of morph targets
//////////////////////////////////////////////////////////////////////////
void GlobalAnimationHeaderAIM::ProcessPoses(const IDefaultSkeleton* pIDefaultSkeleton, const char* szAnimationName)
{
	if (pIDefaultSkeleton == 0)
		return;

	const CDefaultSkeleton* pDefaultSkeleton = (const CDefaultSkeleton*)pIDefaultSkeleton;

	const uint32 numAimDB = pDefaultSkeleton->m_poseBlenderAimDesc.m_blends.size();
	for (uint32 d = 0; d < numAimDB; d++)
	{
		const char* strAimIK_Token = pDefaultSkeleton->m_poseBlenderAimDesc.m_blends[d].m_AnimToken;
		uint32 IsAIM = (CryStringUtils::stristr(szAnimationName, strAimIK_Token) != 0);
		if (IsAIM)
		{
			const DynArray<DirectionalBlends>& rDirBlends = pDefaultSkeleton->m_poseBlenderAimDesc.m_blends;
			const DynArray<SJointsAimIK_Rot>& rRot = pDefaultSkeleton->m_poseBlenderAimDesc.m_rotations;
			const DynArray<SJointsAimIK_Pos>& rPos = pDefaultSkeleton->m_poseBlenderAimDesc.m_positions;
			ProcessDirectionalPoses(pDefaultSkeleton, rDirBlends, rRot, rPos);
			break;
		}
	}

	uint32 numLookDB = pDefaultSkeleton->m_poseBlenderLookDesc.m_blends.size();
	for (uint32 d = 0; d < numLookDB; d++)
	{
		const char* strLookIK_Token = pDefaultSkeleton->m_poseBlenderLookDesc.m_blends[d].m_AnimToken;
		uint32 IsLookIK = (CryStringUtils::stristr(szAnimationName, strLookIK_Token) != 0);
		if (IsLookIK)
		{
			const DynArray<DirectionalBlends>& rDirBlends = pDefaultSkeleton->m_poseBlenderLookDesc.m_blends;
			const DynArray<SJointsAimIK_Rot>& rRot = pDefaultSkeleton->m_poseBlenderLookDesc.m_rotations;
			const DynArray<SJointsAimIK_Pos>& rPos = pDefaultSkeleton->m_poseBlenderLookDesc.m_positions;
			ProcessDirectionalPoses(pDefaultSkeleton, rDirBlends, rRot, rPos);
			break;
		}
	}

}

void GlobalAnimationHeaderAIM::ProcessDirectionalPoses(const CDefaultSkeleton* pDefaultSkeleton, const DynArray<DirectionalBlends>& rDirBlends, const DynArray<SJointsAimIK_Rot>& rRot, const DynArray<SJointsAimIK_Pos>& rPos)
{
	if (pDefaultSkeleton == 0)
		return;

	uint32 nAnimTokenCRC32 = 0;
	int32 nParameterJointIdx = -1;
	const char* strParameterJoint = "\0";

	uint32 numAimDB = rDirBlends.size();
	for (uint32 d = 0; d < numAimDB; d++)
	{
		const char* strAimIK_Token = rDirBlends[d].m_AnimToken;
		uint32 IsAimIK = (CryStringUtils::stristr(m_FilePath.c_str(), strAimIK_Token) != 0);
		if (IsAimIK)
		{
			nAnimTokenCRC32 = rDirBlends[d].m_AnimTokenCRC32;
			nParameterJointIdx = rDirBlends[d].m_nParaJointIdx;
			strParameterJoint = rDirBlends[d].m_strParaJointName;
			break;
		}
	}

	if (IsAssetCreated() == 0)
		return;
	if (IsAssetLoaded() == 0)
		return;

	//if all joints in the aim-pose are valid
	uint32 numRot = rRot.size();
	for (uint32 i = 0; i < numRot; i++)
	{
		int32 idx = rRot[i].m_nJointIdx;
		if (idx < 0)
		{
			//		const char* pname = rRot[i].m_strJointName.c_str();
			g_pILog->LogError("CryAnimation: Aim/lookpose '%s' disabled. Model '%s' doesn't have Jointname '%s'", m_FilePath.c_str(), pDefaultSkeleton->GetModelFilePath(), rRot[i].m_strJointName);
			return;
		}
	}

	uint32 numPos = rPos.size();
	for (uint32 i = 0; i < numPos; i++)
	{
		int32 idx = rPos[i].m_nJointIdx;
		if (idx < 0)
		{
			g_pILog->LogError("CryAnimation: Aim/lookpose '%s' disabled. Model '%s' doesn't have Jointname '%s'", m_FilePath.c_str(), pDefaultSkeleton->GetModelFilePath(), rRot[i].m_strJointName);
			return;
		}
	}

	//----------------------------------------------------------------------------------------------------------------------------------------
	//int32 match1 = stricmp(m_FilePath,"animations/human/male/weapons/scar/3p/aimposes/stand_tac_aimposes_idle_scar_shoulder_3p_01.caf")==0;
	//int32 match1 = stricmp(m_FilePath,"animations/alien/grunt/weapons/stand_walk_aimposes_idle_gun_09.caf")==0;
	//int32 match1 = stricmp(m_FilePath,"animations/alien/grunt/weapons/stand_idle_AimPoses_com_gun_09.caf")==0;
	//	int32 match1 = stricmp(m_FilePath,"animations/human/male/weapons/scar/3p/gould_lookposes_karl3.caf")==0;

	//	int32 match1 = stricmp(m_FilePath,"animations/human/story/gould/lookposes/lookposes_gouldfullbody_01.caf")==0;
	//	int32 match1 = stricmp(m_FilePath,"animations/human/story/barclay/barclay_lookposes_01.caf")==0;
	//	if (match1==0)
	//		return;

	if (m_AnimTokenCRC32 != nAnimTokenCRC32)
		g_pILog->Log("CryAnimation: Processing Aim/lookpose '%s'", m_FilePath.c_str());

	uint32 numPoses = uint32(m_fTotalDuration * ANIMATION_30Hz + 1.1f);
	const bool validNumberOfPoses = (numPoses == 9 || numPoses == 15 || numPoses == 21);
	if (!validNumberOfPoses)
	{
		g_pILog->LogError("CryAnimation: Aim/lookpose '%s' disabled. Asset contains '%u' poses (supported 9, 15 or 21).", m_FilePath.c_str(), numPoses);
		return;
	}

	uint32 found = 0;
	uint32 numRotJoints = rRot.size();
	for (uint32 r = 0; r < numRotJoints; r++)
	{
		int32 j = rRot[r].m_nJointIdx;
		if (j != nParameterJointIdx)
			continue;
		found = 1;
		if (rRot[r].m_nPreEvaluate == 0)
		{
			g_pILog->LogError("CryAnimation: Aim/lookpose '%s' disabled. Parameter joint '%s' not marked as primary: '%s'", m_FilePath.c_str(), strParameterJoint, pDefaultSkeleton->GetModelFilePath());
			return;
		}
	}
	if (found == 0)
	{
		g_pILog->LogError("CryAnimation: Aim/lookpose '%s' disabled. Parameter joint '%s' not defined in RotationList: '%s'", m_FilePath.c_str(), strParameterJoint, pDefaultSkeleton->GetModelFilePath());
		return;
	}

	m_AnimTokenCRC32 = nAnimTokenCRC32;
	VExampleInit* pVEI = new VExampleInit();
	pVEI->Init(pDefaultSkeleton, rRot, rPos, *this, nParameterJointIdx, numPoses);
	delete pVEI;
}

//---------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------
void GlobalAnimationHeaderAIM::Debug_NLerp2AimPose(const CDefaultSkeleton* pDefaultSkeleton, const SJointsAimIK_Rot* rRot, uint32 numRotJoints, const SJointsAimIK_Pos* rPos, uint32 numPosJoints, const QuatT* arrRelPose0, const QuatT* arrRelPose1, f32 t, QuatT* arrAbsPose) const
{
	const CDefaultSkeleton::SJoint* parrModelJoints = &pDefaultSkeleton->m_arrModelJoints[0];
	f32 t0 = 1.0f - t;
	f32 t1 = t;
	QuatT qt;
	for (uint32 r = 0; r < numRotJoints; r++)
	{
		if (rRot[r].m_nPreEvaluate)
		{
			int32 j = rRot[r].m_nJointIdx;
			int32 p = parrModelJoints[j].m_idxParent;
			qt.q = arrRelPose0[j].q * t0 + arrRelPose1[j].q * t1;
			qt.t = arrRelPose0[j].t * t0 + arrRelPose1[j].t * t1;
			qt.q.Normalize();
			arrAbsPose[j] = arrAbsPose[p] * qt;
		}
	}
}

void GlobalAnimationHeaderAIM::Debug_Blend4AimPose(const CDefaultSkeleton* pDefaultSkeleton, const SJointsAimIK_Rot* rRot, uint32 numRotJoints, const SJointsAimIK_Pos* rPos, uint32 numPosJoints, int8 i0, int8 i1, int8 i2, int8 i3, const Vec4& w, QuatT* arrRelPose, QuatT* arrAbsPose) const
{
	for (uint32 r = 0; r < numRotJoints; r++)
	{
		if (rRot[r].m_nPreEvaluate)
		{
			int32 j = rRot[r].m_nJointIdx;
			arrRelPose[j].q.w = 0.0f;
			arrRelPose[j].q.v.x = 0.0f;
			arrRelPose[j].q.v.y = 0.0f;
			arrRelPose[j].q.v.z = 0.0f;

			arrRelPose[j].t.x = 0.0f;
			arrRelPose[j].t.y = 0.0f;
			arrRelPose[j].t.z = 0.0f;
		}
	}

	if (w.x)
	{
		for (uint32 r = 0; r < numRotJoints; r++)
		{
			if (rRot[r].m_nPreEvaluate)
			{
				int32 j = rRot[r].m_nJointIdx;
				Quat qRot = m_arrAimIKPosesAIM[i0].m_arrRotation[r];
				Vec3 vPos = pDefaultSkeleton->m_poseDefaultData.GetJointRelative(j).t;
				int32 p = rRot[r].m_nPosIndex;
				if (p > -1)
					vPos = m_arrAimIKPosesAIM[i0].m_arrPosition[p];

				arrRelPose[j].q += qRot * w.x;
				arrRelPose[j].t += vPos * w.x;
			}
		}
	}

	if (w.y)
	{
		for (uint32 r = 0; r < numRotJoints; r++)
		{
			if (rRot[r].m_nPreEvaluate)
			{
				int32 j = rRot[r].m_nJointIdx;
				Quat qRot = m_arrAimIKPosesAIM[i1].m_arrRotation[r];
				Vec3 vPos = pDefaultSkeleton->m_poseDefaultData.GetJointRelative(j).t;
				int32 p = rRot[r].m_nPosIndex;
				if (p > -1)
					vPos = m_arrAimIKPosesAIM[i1].m_arrPosition[p];

				arrRelPose[j].q += qRot * w.y;
				arrRelPose[j].t += vPos * w.y;
			}
		}
	}

	if (w.z)
	{
		for (uint32 r = 0; r < numRotJoints; r++)
		{
			if (rRot[r].m_nPreEvaluate)
			{
				int32 j = rRot[r].m_nJointIdx;
				Quat qRot = m_arrAimIKPosesAIM[i2].m_arrRotation[r];
				Vec3 vPos = pDefaultSkeleton->m_poseDefaultData.GetJointRelative(j).t;
				int32 p = rRot[r].m_nPosIndex;
				if (p > -1)
					vPos = m_arrAimIKPosesAIM[i2].m_arrPosition[p];

				arrRelPose[j].q += qRot * w.z;
				arrRelPose[j].t += vPos * w.z;
			}
		}
	}

	if (w.w)
	{
		for (uint32 r = 0; r < numRotJoints; r++)
		{
			if (rRot[r].m_nPreEvaluate)
			{
				int32 j = rRot[r].m_nJointIdx;
				Quat qRot = m_arrAimIKPosesAIM[i3].m_arrRotation[r];
				Vec3 vPos = pDefaultSkeleton->m_poseDefaultData.GetJointRelative(j).t;
				int32 p = rRot[r].m_nPosIndex;
				if (p > -1)
					vPos = m_arrAimIKPosesAIM[i3].m_arrPosition[p];

				arrRelPose[j].q += qRot * w.w;
				arrRelPose[j].t += vPos * w.w;
			}
		}
	}

	const CDefaultSkeleton::SJoint* parrModelJoints = &pDefaultSkeleton->m_arrModelJoints[0];
	for (uint32 r = 0; r < numRotJoints; r++)
	{
		if (rRot[r].m_nPreEvaluate)
		{
			int32 j = rRot[r].m_nJointIdx;
			int32 p = parrModelJoints[j].m_idxParent;
			arrRelPose[j].q.Normalize();
			arrAbsPose[j] = arrAbsPose[p] * arrRelPose[j];
		}
	}
}

Vec3 GlobalAnimationHeaderAIM::Debug_PolarCoordinate(const Quat& q) const
{
	Matrix33 m = Matrix33(q);
	assert(m.IsOrthonormal());
	f32 l = sqrtf(m.m01 * m.m01 + m.m11 * m.m11);
	if (l > 0.0001)
		return Vec3(atan2f(-m.m01 / l, m.m11 / l), -atan2f(m.m21, l), atan2f(-m.m20 / l, m.m22 / l));
	else
		return Vec3(0, atan2f(m.m21, l), 0);
}

uint32 GlobalAnimationHeaderAIM::Debug_AnnotateExamples2(uint32 numPoses, QuadIndices* arrQuat) const
{
	uint32 q = (numPoses / 3) - 1;
	for (uint32 i = 0; i < q; i++)
	{
		arrQuat[i + 0].i0 = i + 0;
		arrQuat[i + 0].w0 = Vec4(1, 0, 0, 0);
		arrQuat[i + 0].i1 = i + 1;
		arrQuat[i + 0].w1 = Vec4(0, 1, 0, 0);
		arrQuat[i + 0].i2 = i + q + 2;
		arrQuat[i + 0].w2 = Vec4(0, 0, 1, 0);
		arrQuat[i + 0].i3 = i + q + 1;
		arrQuat[i + 0].w3 = Vec4(0, 0, 0, 1);
		arrQuat[i + 0].col = RGBA8(0x00, 0xff, 0x00, 0xff);
		arrQuat[i + 0].height = Vec3(0, 0, 0.001f);

		arrQuat[i + q].i0 = i + q + 1;
		arrQuat[i + q].w0 = Vec4(1, 0, 0, 0);
		arrQuat[i + q].i1 = i + q + 2;
		arrQuat[i + q].w1 = Vec4(0, 1, 0, 0);
		arrQuat[i + q].i2 = i + q + q + 3;
		arrQuat[i + q].w2 = Vec4(0, 0, 1, 0);
		arrQuat[i + q].i3 = i + q + q + 2;
		arrQuat[i + q].w3 = Vec4(0, 0, 0, 1);
		arrQuat[i + q].col = RGBA8(0x00, 0xff, 0x00, 0xff);
		arrQuat[i + q].height = Vec3(0, 0, 0.001f);
	}

	if (numPoses != 9)
		return q * 2;

	uint32 i = -1;
	f32 t = 0;

	f32 diag = 1.70f;

	f32 eup = 1.60f;
	f32 mup = 1.20f;

	f32 mside = 2.00f;
	f32 eside = 1.90f;

	f32 edown = 1.6f;
	f32 mdown = 1.20f;

	i = 4;
	arrQuat[i].i0 = 3;
	t = eup;
	arrQuat[i].w0 = Vec4(1 - t, 0, 0, t);                           //3-mirrored 0-scaled
	arrQuat[i].i1 = 4;
	t = mup;
	arrQuat[i].w1 = Vec4(0, 1 - t, t, 0);                           //4-mirrored 1-scaled
	arrQuat[i].i2 = 1;
	t = 1.00f;
	arrQuat[i].w2 = Vec4(0, 0, 1, 0);
	arrQuat[i].i3 = 0;
	t = 1.00f;
	arrQuat[i].w3 = Vec4(0, 0, 0, 1);
	arrQuat[i].col = RGBA8(0xff, 0x00, 0x00, 0xff);
	arrQuat[i].height = Vec3(0, 0, 0.001f);

	i = 5;
	arrQuat[i].i0 = 3;
	t = 1.00f;
	arrQuat[i].w0 = Vec4(1, 0, 0, 0);
	arrQuat[i].i1 = 4;
	t = mside;
	arrQuat[i].w1 = Vec4(t, 1 - t, 0, 0);                           //4-mirrored 3-scaled
	arrQuat[i].i2 = 1;
	t = eside;
	arrQuat[i].w2 = Vec4(0, 0, 1 - t, t);                           //1-mirrored 0-scaled
	arrQuat[i].i3 = 0;
	t = 1.00f;
	arrQuat[i].w3 = Vec4(0, 0, 0, 1);
	arrQuat[i].col = RGBA8(0xff, 0x00, 0x00, 0xff);
	arrQuat[i].height = Vec3(0, 0, 0.001f);

	i = 6;
	arrQuat[i].i0 = 4;
	t = diag;
	arrQuat[i].w0 = Vec4(1 - t, 0, t, 0);                           //4-mirrored 0-scaled
	arrQuat[i].i1 = 3;
	t = eup;
	arrQuat[i].w1 = Vec4(0, 1 - t, t, 0);                           //3-mirrored 0-scaled
	arrQuat[i].i2 = 0;
	t = 1.00f;
	arrQuat[i].w2 = Vec4(0, 0, 1, 0);
	arrQuat[i].i3 = 1;
	t = eside;
	arrQuat[i].w3 = Vec4(0, 0, t, 1 - t);                           //1-mirrored 0-scaled
	arrQuat[i].col = RGBA8(0xff, 0x00, 0x00, 0xff);
	arrQuat[i].height = Vec3(0, 0, 0.001f);

	i = 7;
	arrQuat[i].i0 = 2;
	t = 1.00f;
	arrQuat[i].w0 = Vec4(1, 0, 0, 0);
	arrQuat[i].i1 = 1;
	t = eside;
	arrQuat[i].w1 = Vec4(t, 1 - t, 0, 0);                           //1-mirrored 2-scaled
	arrQuat[i].i2 = 4;
	t = mside;
	arrQuat[i].w2 = Vec4(0, 0, 1 - t, t);                           //4-mirrored 5-scaled
	arrQuat[i].i3 = 5;
	t = 1.00f;
	arrQuat[i].w3 = Vec4(0, 0, 0, 1);
	arrQuat[i].col = RGBA8(0xff, 0x00, 0x00, 0xff);
	arrQuat[i].height = Vec3(0, 0, 0.001f);

	i = 8;
	arrQuat[i].i0 = 4;
	t = mup;
	arrQuat[i].w0 = Vec4(1 - t, 0, 0, t);                           //4-mirrored 1-scaled
	arrQuat[i].i1 = 5;
	t = eup;
	arrQuat[i].w1 = Vec4(0, 1 - t, t, 0);                           //5-mirrored 2-scaled
	arrQuat[i].i2 = 2;
	t = 1.00f;
	arrQuat[i].w2 = Vec4(0, 0, 1, 0);
	arrQuat[i].i3 = 1;
	t = 1.00f;
	arrQuat[i].w3 = Vec4(0, 0, 0, 1);
	arrQuat[i].col = RGBA8(0xff, 0x00, 0x00, 0xff);
	arrQuat[i].height = Vec3(0, 0, 0.001f);

	i = 9;
	arrQuat[i].i0 = 5;
	t = eup;
	arrQuat[i].w0 = Vec4(1 - t, 0, 0, t);                           //5-mirrored 2-scaled
	arrQuat[i].i1 = 4;
	t = diag;
	arrQuat[i].w1 = Vec4(0, 1 - t, 0, t);                           //4-mirrored 2-scaled
	arrQuat[i].i2 = 1;
	t = eside;
	arrQuat[i].w2 = Vec4(0, 0, 1 - t, t);                           //1-mirrored 2-scaled
	arrQuat[i].i3 = 2;
	t = 1.0f;
	arrQuat[i].w3 = Vec4(0, 0, 0, 1);
	arrQuat[i].col = RGBA8(0xff, 0x00, 0x00, 0xff);
	arrQuat[i].height = Vec3(0, 0, 0.001f);

	i = 10;
	arrQuat[i].i0 = 4;
	t = mside;
	arrQuat[i].w0 = Vec4(1 - t, t, 0, 0);                           //4-mirrored 3-scaled
	arrQuat[i].i1 = 3;
	t = 1.00f;
	arrQuat[i].w1 = Vec4(0, 1, 0, 0);
	arrQuat[i].i2 = 6;
	t = 1.00f;
	arrQuat[i].w2 = Vec4(0, 0, 1, 0);
	arrQuat[i].i3 = 7;
	t = eside;
	arrQuat[i].w3 = Vec4(0, 0, t, 1 - t);                           //7-mirrored 6-scaled
	arrQuat[i].col = RGBA8(0xff, 0x00, 0x00, 0xff);
	arrQuat[i].height = Vec3(0, 0, 0.001f);
	i = 11;
	arrQuat[i].i0 = 6;
	t = 1.00f;
	arrQuat[i].w0 = Vec4(1, 0, 0, 0);
	arrQuat[i].i1 = 7;
	t = 1.00f;
	arrQuat[i].w1 = Vec4(0, 1, 0, 0);
	arrQuat[i].i2 = 4;
	t = mdown;
	arrQuat[i].w2 = Vec4(0, t, 1 - t, 0);                           //4-mirrored 7-scaled
	arrQuat[i].i3 = 3;
	t = edown;
	arrQuat[i].w3 = Vec4(t, 0, 0, 1 - t);                           //3-mirrored 6-scaled
	arrQuat[i].col = RGBA8(0xff, 0x00, 0x00, 0xff);
	arrQuat[i].height = Vec3(0, 0, 0.001f);
	i = 12;
	arrQuat[i].i0 = 7;
	t = eside;
	arrQuat[i].w0 = Vec4(1 - t, t, 0, 0);                           //7-mirrored 6-scaled
	arrQuat[i].i1 = 6;
	t = 1.00f;
	arrQuat[i].w1 = Vec4(0, 1, 0, 0);
	arrQuat[i].i2 = 3;
	t = edown;
	arrQuat[i].w2 = Vec4(0, t, 1 - t, 0);                           //3-mirrored 6-scaled
	arrQuat[i].i3 = 4;
	t = diag;
	arrQuat[i].w3 = Vec4(0, t, 0, 1 - t);                           //4-mirrored 6-scaled
	arrQuat[i].col = RGBA8(0xff, 0x00, 0x00, 0xff);
	arrQuat[i].height = Vec3(0, 0, 0.001f);

	i = 13;
	arrQuat[i].i0 = 5;
	t = 1.00f;
	arrQuat[i].w0 = Vec4(1, 0, 0, 0);
	arrQuat[i].i1 = 4;
	t = mside;
	arrQuat[i].w1 = Vec4(t, 1 - t, 0, 0);                            //4 mirrored 5-scaled
	arrQuat[i].i2 = 7;
	t = eside;
	arrQuat[i].w2 = Vec4(0, 0, 1 - t, t);                            //7 mirrored 8-scaled
	arrQuat[i].i3 = 8;
	t = 1.00f;
	arrQuat[i].w3 = Vec4(0, 0, 0, 1);
	arrQuat[i].col = RGBA8(0xff, 0x00, 0x00, 0xff);
	arrQuat[i].height = Vec3(0, 0, 0.001f);
	i = 14;
	arrQuat[i].i0 = 7;
	t = 1.00f;
	arrQuat[i].w0 = Vec4(1, 0, 0, 0);
	arrQuat[i].i1 = 8;
	t = 1.00f;
	arrQuat[i].w1 = Vec4(0, 1, 0, 0);
	arrQuat[i].i2 = 5;
	t = edown;
	arrQuat[i].w2 = Vec4(0, t, 1 - t, 0);                           //5-mirrored 8-scaled
	arrQuat[i].i3 = 4;
	t = mdown;
	arrQuat[i].w3 = Vec4(t, 0, 0, 1 - t);                           //4-mirrored 7-scaled
	arrQuat[i].col = RGBA8(0xff, 0x00, 0x00, 0xff);
	arrQuat[i].height = Vec3(0, 0, 0.001f);
	i = 15;
	arrQuat[i].i0 = 8;
	t = 1.00f;
	arrQuat[i].w0 = Vec4(1, 0, 0, 0);
	arrQuat[i].i1 = 7;
	t = eside;
	arrQuat[i].w1 = Vec4(t, 1 - t, 0, 0);                           //7-mirrored 8-scaled
	arrQuat[i].i2 = 4;
	t = diag;
	arrQuat[i].w2 = Vec4(t, 0, 1 - t, 0);                           //4-mirrored 8-scaled cross
	arrQuat[i].i3 = 5;
	t = edown;
	arrQuat[i].w3 = Vec4(t, 0, 0, 1 - t);                           //5-mirrored 8-scaled
	arrQuat[i].col = RGBA8(0xff, 0x00, 0x00, 0xff);
	arrQuat[i].height = Vec3(0, 0, 0.001f);

	return q * 2 + 3 + 3 + 3 + 3;
}

//-----------------------------------------------------------------------------------------------------

void VExampleInit::Init(const CDefaultSkeleton* pDefaultSkeleton, const DynArray<SJointsAimIK_Rot>& rRot, const DynArray<SJointsAimIK_Pos>& rPos, GlobalAnimationHeaderAIM& rAIM, int nWeaponBoneIdx, uint32 numAimPoses)
{
	uint32 numJoints = pDefaultSkeleton->m_poseDefaultData.GetJointCount();

	CRY_ASSERT(numJoints < MAX_JOINT_AMOUNT);

	numJoints = min(numJoints, uint32(MAX_JOINT_AMOUNT));

	for (uint32 i = 0; i < numJoints; i++)
	{
		m_arrJointCRC32[i] = pDefaultSkeleton->m_arrModelJoints[i].m_nJointCRC32;
		m_arrParentIdx[i] = pDefaultSkeleton->m_arrModelJoints[i].m_idxParent;
		m_arrDefaultRelPose[i] = pDefaultSkeleton->m_poseDefaultData.GetJointRelative(i);
		m_arrRelPose[i] = pDefaultSkeleton->m_poseDefaultData.GetJointRelative(i);
	}

	//-------------------------------------------------------------------------------------------------
	//from here on the computation of aim-poses in RC and Engine is supposed to be identical
	//-------------------------------------------------------------------------------------------------

	rAIM.OnAimpose();
	//	rAIM.m_nTouchedCounter++;

	rAIM.m_arrAimIKPosesAIM.resize(numAimPoses);
	uint32 numRot = rRot.size();
	uint32 numPos = rPos.size();
	for (uint32 a = 0; a < numAimPoses; a++)
	{
		rAIM.m_arrAimIKPosesAIM[a].m_arrRotation.resize(numRot);
		rAIM.m_arrAimIKPosesAIM[a].m_arrPosition.resize(numPos);
	}

	m_arrAbsPose[0] = m_arrRelPose[0];
	for (uint32 i = 1; i < numJoints; i++)
		m_arrAbsPose[i] = m_arrAbsPose[m_arrParentIdx[i]] * m_arrRelPose[i];

	for (uint32 p = 0; p < numAimPoses; p++)
		CopyPoses2(rRot, rPos, rAIM, numAimPoses, p);

	//-----------------------------------------------------------------------------

	uint32 nMidPoseIdx = uint32(numAimPoses / 2);
	uint32 nUpPoseIdx = uint32(numAimPoses / 3 / 2);
	ComputeAimPose(rAIM, pDefaultSkeleton, rRot, rPos, m_arrAbsPose, nMidPoseIdx);

	Quatd qDefaultMidAimPose = !m_arrAbsPose[nWeaponBoneIdx].q;
	Quatd arrOriginalAimPoses[27];
	uint32 numRotJoints = rRot.size();
	for (uint32 ap = 0; ap < numAimPoses; ap++)
	{
		ComputeAimPose(rAIM, pDefaultSkeleton, rRot, rPos, m_arrAbsPose, ap);
		Quat q = m_arrAbsPose[nWeaponBoneIdx].q;
		arrOriginalAimPoses[ap] = qDefaultMidAimPose * m_arrAbsPose[nWeaponBoneIdx].q;
	}

	Vec3 v0 = arrOriginalAimPoses[nUpPoseIdx].GetColumn1();
	Vec3 v1 = arrOriginalAimPoses[nMidPoseIdx].GetColumn1();
	Vec3 upvector = v0 - v1;
	upvector.y = 0;
	upvector.Normalize();
	rAIM.m_MiddleAimPoseRot.SetRotationV0V1(Vec3(0, 0, 1), upvector);
	rAIM.m_MiddleAimPose = !rAIM.m_MiddleAimPoseRot* Quat(qDefaultMidAimPose);
	for (uint32 ap = 0; ap < numAimPoses; ap++)
	{
		ComputeAimPose(rAIM, pDefaultSkeleton, rRot, rPos, m_arrAbsPose, ap);
		arrOriginalAimPoses[ap] = qDefaultMidAimPose * m_arrAbsPose[nWeaponBoneIdx].q;
	}

	for (uint32 i = 0; i < (CHUNK_GAHAIM_INFO::YGRID * CHUNK_GAHAIM_INFO::XGRID); i++)
	{
		PolarGrid[i].m_fSmalest = 9999.0f;
		PolarGrid[i].i0 = 0xff;
		PolarGrid[i].i1 = 0xff;
		PolarGrid[i].i2 = 0xff;
		PolarGrid[i].i3 = 0xff;
		PolarGrid[i].w0 = 99.0f;
		PolarGrid[i].w1 = 99.0f;
		PolarGrid[i].w2 = 99.0f;
		PolarGrid[i].w3 = 99.0f;
	}

	QuadIndices arrQuat[64];
	uint32 numQuats = AnnotateExamples(numAimPoses, arrQuat);

	for (uint32 i = 0; i < numJoints; i++)
		m_arrRelPose0[i] = pDefaultSkeleton->m_poseDefaultData.GetJointRelative(i);
	for (uint32 i = 0; i < numJoints; i++)
		m_arrRelPose1[i] = pDefaultSkeleton->m_poseDefaultData.GetJointRelative(i);
	for (uint32 i = 0; i < numJoints; i++)
		m_arrRelPose2[i] = pDefaultSkeleton->m_poseDefaultData.GetJointRelative(i);
	for (uint32 i = 0; i < numJoints; i++)
		m_arrRelPose3[i] = pDefaultSkeleton->m_poseDefaultData.GetJointRelative(i);

	m_arrAbsPose0[0] = m_arrRelPose0[0];
	m_arrAbsPose1[0] = m_arrRelPose1[0];
	m_arrAbsPose2[0] = m_arrRelPose2[0];
	m_arrAbsPose3[0] = m_arrRelPose3[0];
	for (uint32 i = 1; i < numJoints; i++)
	{
		uint32 p = m_arrParentIdx[i];
		m_arrAbsPose0[i] = m_arrAbsPose0[p] * m_arrRelPose0[i];
		m_arrAbsPose1[i] = m_arrAbsPose1[p] * m_arrRelPose1[i];
		m_arrAbsPose2[i] = m_arrAbsPose2[p] * m_arrRelPose2[i];
		m_arrAbsPose3[i] = m_arrAbsPose3[p] * m_arrRelPose3[i];
	}

	for (int32 y = 0; y < CHUNK_GAHAIM_INFO::YGRID; y++)
	{
		for (int32 x = 0; x < CHUNK_GAHAIM_INFO::XGRID; x++)
		{
			f32 fx = x * (gf_PI / 8) - gf_PI;
			f32 fy = y * (gf_PI / 8) - gf_PI * 0.5f;
			for (uint32 i = 0; i < numQuats; i++)
			{
				int8 i0 = arrQuat[i].i0;
				int8 i1 = arrQuat[i].i1;
				int8 i2 = arrQuat[i].i2;
				int8 i3 = arrQuat[i].i3;
				Vec4 w0 = arrQuat[i].w0;
				Vec4 w1 = arrQuat[i].w1;
				Vec4 w2 = arrQuat[i].w2;
				Vec4 w3 = arrQuat[i].w3;

				m_nIterations = 0;
				m_fSmallest = 9999.0f;
				RecursiveTest(Vec2d(fx, fy), rAIM, rRot, rPos, nWeaponBoneIdx, i0, i1, i2, i3, w0, w1, w2, w3); // This method computes m_fSmalest and m_Weight.

				if (m_fSmallest < 10.0)
				{
					PolarGrid[y * CHUNK_GAHAIM_INFO::XGRID + x].m_fSmalest = f32(m_fSmallest);
					PolarGrid[y * CHUNK_GAHAIM_INFO::XGRID + x].i0 = i0;
					PolarGrid[y * CHUNK_GAHAIM_INFO::XGRID + x].i1 = i1;
					PolarGrid[y * CHUNK_GAHAIM_INFO::XGRID + x].i2 = i2;
					PolarGrid[y * CHUNK_GAHAIM_INFO::XGRID + x].i3 = i3;
					PolarGrid[y * CHUNK_GAHAIM_INFO::XGRID + x].w0 = m_Weight.x;
					PolarGrid[y * CHUNK_GAHAIM_INFO::XGRID + x].w1 = m_Weight.y;
					PolarGrid[y * CHUNK_GAHAIM_INFO::XGRID + x].w2 = m_Weight.z;
					PolarGrid[y * CHUNK_GAHAIM_INFO::XGRID + x].w3 = m_Weight.w;
					break;
				}
			}
		}
	}

	//----------------------------------------------------------------------------------------

	if (1)
	{
		if (rAIM.VE2.capacity() == 0)
			rAIM.VE2.reserve(1024);

		for (uint32 i = 0; i < numQuats; i++)
		//		for (uint32 i=0; i<1; i++)
		{
			int8 i0 = arrQuat[i].i0;
			int8 i1 = arrQuat[i].i1;
			int8 i2 = arrQuat[i].i2;
			int8 i3 = arrQuat[i].i3;

			Vec4d w0 = arrQuat[i].w0;
			Vec4d w1 = arrQuat[i].w1;
			Vec4d w2 = arrQuat[i].w2;
			Vec4d w3 = arrQuat[i].w3;

			Quatd mid;
			mid.w = f32(rAIM.m_MiddleAimPose.w);
			mid.v.x = f32(rAIM.m_MiddleAimPose.v.x);
			mid.v.y = f32(rAIM.m_MiddleAimPose.v.y);
			mid.v.z = f32(rAIM.m_MiddleAimPose.v.z);

			Vec4d x0 = w0;
			Blend4AimPose(rAIM, rRot, rPos, i0, i1, i2, i3, w0, m_arrRelPose0, m_arrAbsPose0);
			Quatd q0 = mid * m_arrAbsPose0[nWeaponBoneIdx].q;
			for (f64 t = 0; t < 1.001; t += 0.05)
			{
				Vec4d x1;
				x1.SetLerp(w0, w1, t);
				Blend4AimPose(rAIM, rRot, rPos, i0, i1, i2, i3, x1, m_arrRelPose0, m_arrAbsPose0);
				Quatd q1 = mid * m_arrAbsPose0[nWeaponBoneIdx].q;

				Vec2d p0 = Vec2d(PolarCoordinate(q0));
				Vec2d p1 = Vec2d(PolarCoordinate(q1));
				for (int32 gx = -8; gx < +8; gx++)
				{
					f64 e = gx * (g_PI / 8);
					if ((p0.x < e && p1.x > e) || (p0.x > e && p1.x < e))
					{
						f64 d0 = e - p0.x;
						f64 d1 = p1.x - e;
						f64 d = d0 + d1;
						f64 b0 = 1 - d0 / d;
						f64 b1 = 1 - d1 / d;
						Vec2d polcor = p0 * b0 + p1 * b1;
						Vec4d weight = x0 * b0 + x1 * b1;
						CHUNK_GAHAIM_INFO::VirtualExampleInit2 ve2;
						ve2.polar.x = f32(polcor.x);
						ve2.polar.y = f32(polcor.y);
						ve2.i0 = i0;
						ve2.i1 = i1;
						ve2.i2 = i2;
						ve2.i3 = i3;
						ve2.w0 = f32(weight.x);
						ve2.w1 = f32(weight.y);
						ve2.w2 = f32(weight.z);
						ve2.w3 = f32(weight.w);
						rAIM.VE2.push_back(ve2);
					}
				}
				for (int32 gy = -5; gy < +5; gy++)
				{
					f64 e = gy * (g_PI / 8);
					if ((p0.y < e && p1.y > e) || (p0.y > e && p1.y < e))
					{
						f64 d0 = e - p0.y;
						f64 d1 = p1.y - e;
						f64 d = d0 + d1;
						f64 b0 = 1 - d0 / d;
						f64 b1 = 1 - d1 / d;
						Vec2d polcor = p0 * b0 + p1 * b1;
						Vec4d weight = x0 * b0 + x1 * b1;
						CHUNK_GAHAIM_INFO::VirtualExampleInit2 ve2;
						ve2.polar.x = f32(polcor.x);
						ve2.polar.y = f32(polcor.y);
						ve2.i0 = i0;
						ve2.i1 = i1;
						ve2.i2 = i2;
						ve2.i3 = i3;
						ve2.w0 = f32(weight.x);
						ve2.w1 = f32(weight.y);
						ve2.w2 = f32(weight.z);
						ve2.w3 = f32(weight.w);
						rAIM.VE2.push_back(ve2);
					}
				}
				q0 = q1;
				x0 = x1;
			}

			x0 = w1;
			Blend4AimPose(rAIM, rRot, rPos, i0, i1, i2, i3, w1, m_arrRelPose0, m_arrAbsPose0);
			q0 = mid * m_arrAbsPose0[nWeaponBoneIdx].q;
			for (f64 t = 0; t < 1.0001; t += 0.02)
			{
				Vec4d x1;
				x1.SetLerp(w1, w2, t);
				Blend4AimPose(rAIM, rRot, rPos, i0, i1, i2, i3, x1, m_arrRelPose0, m_arrAbsPose0);
				Quatd q1 = mid * m_arrAbsPose0[nWeaponBoneIdx].q;
				Vec2d p0 = Vec2d(PolarCoordinate(q0));
				Vec2d p1 = Vec2d(PolarCoordinate(q1));
				for (int32 gx = -8; gx < +8; gx++)
				{
					f64 e = gx * (g_PI / 8);
					if ((p0.x < e && p1.x > e) || (p0.x > e && p1.x < e))
					{
						f64 d0 = e - p0.x;
						f64 d1 = p1.x - e;
						f64 d = d0 + d1;
						f64 b0 = 1 - d0 / d;
						f64 b1 = 1 - d1 / d;
						Vec2d polcor = p0 * b0 + p1 * b1;
						Vec4d weight = x0 * b0 + x1 * b1;
						CHUNK_GAHAIM_INFO::VirtualExampleInit2 ve2;
						ve2.polar.x = f32(polcor.x);
						ve2.polar.y = f32(polcor.y);
						ve2.i0 = i0;
						ve2.i1 = i1;
						ve2.i2 = i2;
						ve2.i3 = i3;
						ve2.w0 = f32(weight.x);
						ve2.w1 = f32(weight.y);
						ve2.w2 = f32(weight.z);
						ve2.w3 = f32(weight.w);
						rAIM.VE2.push_back(ve2);
					}
				}
				for (int32 gy = -5; gy < +5; gy++)
				{
					f64 e = gy * (g_PI / 8);
					if ((p0.y < e && p1.y > e) || (p0.y > e && p1.y < e))
					{
						f64 d0 = e - p0.y;
						f64 d1 = p1.y - e;
						f64 d = d0 + d1;
						f64 b0 = 1 - d0 / d;
						f64 b1 = 1 - d1 / d;
						Vec2d polcor = p0 * b0 + p1 * b1;
						Vec4d weight = x0 * b0 + x1 * b1;
						CHUNK_GAHAIM_INFO::VirtualExampleInit2 ve2;
						ve2.polar.x = f32(polcor.x);
						ve2.polar.y = f32(polcor.y);
						ve2.i0 = i0;
						ve2.i1 = i1;
						ve2.i2 = i2;
						ve2.i3 = i3;
						ve2.w0 = f32(weight.x);
						ve2.w1 = f32(weight.y);
						ve2.w2 = f32(weight.z);
						ve2.w3 = f32(weight.w);
						rAIM.VE2.push_back(ve2);
					}
				}
				q0 = q1;
				x0 = x1;
			}

			x0 = w2;
			Blend4AimPose(rAIM, rRot, rPos, i0, i1, i2, i3, w2, m_arrRelPose0, m_arrAbsPose0);
			q0 = mid * m_arrAbsPose0[nWeaponBoneIdx].q;
			for (f64 t = 0; t < 1.0001; t += 0.05)
			{
				Vec4d x1;
				x1.SetLerp(w2, w3, t);
				Blend4AimPose(rAIM, rRot, rPos, i0, i1, i2, i3, x1, m_arrRelPose0, m_arrAbsPose0);
				Quatd q1 = mid * m_arrAbsPose0[nWeaponBoneIdx].q;
				Vec2d p0 = Vec2d(PolarCoordinate(q0));
				Vec2d p1 = Vec2d(PolarCoordinate(q1));
				for (int32 gx = -8; gx < +8; gx++)
				{
					f64 e = gx * (g_PI / 8);
					if ((p0.x < e && p1.x > e) || (p0.x > e && p1.x < e))
					{
						f64 d0 = e - p0.x;
						f64 d1 = p1.x - e;
						f64 d = d0 + d1;
						f64 b0 = 1 - d0 / d;
						f64 b1 = 1 - d1 / d;
						Vec2d polcor = p0 * b0 + p1 * b1;
						Vec4d weight = x0 * b0 + x1 * b1;
						CHUNK_GAHAIM_INFO::VirtualExampleInit2 ve2;
						ve2.polar.x = f32(polcor.x);
						ve2.polar.y = f32(polcor.y);
						ve2.i0 = i0;
						ve2.i1 = i1;
						ve2.i2 = i2;
						ve2.i3 = i3;
						ve2.w0 = f32(weight.x);
						ve2.w1 = f32(weight.y);
						ve2.w2 = f32(weight.z);
						ve2.w3 = f32(weight.w);
						rAIM.VE2.push_back(ve2);
					}
				}
				for (int32 gy = -5; gy < +5; gy++)
				{
					f64 e = gy * (g_PI / 8);
					if ((p0.y < e && p1.y > e) || (p0.y > e && p1.y < e))
					{
						f64 d0 = e - p0.y;
						f64 d1 = p1.y - e;
						f64 d = d0 + d1;
						f64 b0 = 1 - d0 / d;
						f64 b1 = 1 - d1 / d;
						Vec2d polcor = p0 * b0 + p1 * b1;
						Vec4d weight = x0 * b0 + x1 * b1;
						CHUNK_GAHAIM_INFO::VirtualExampleInit2 ve2;
						ve2.polar.x = f32(polcor.x);
						ve2.polar.y = f32(polcor.y);
						ve2.i0 = i0;
						ve2.i1 = i1;
						ve2.i2 = i2;
						ve2.i3 = i3;
						ve2.w0 = f32(weight.x);
						ve2.w1 = f32(weight.y);
						ve2.w2 = f32(weight.z);
						ve2.w3 = f32(weight.w);
						rAIM.VE2.push_back(ve2);
					}
				}
				q0 = q1;
				x0 = x1;
			}

			x0 = w3;
			Blend4AimPose(rAIM, rRot, rPos, i0, i1, i2, i3, w3, m_arrRelPose0, m_arrAbsPose0);
			q0 = mid * m_arrAbsPose0[nWeaponBoneIdx].q;
			for (f64 t = 0; t < 1.0001; t += 0.02)
			{
				Vec4d x1;
				x1.SetLerp(w3, w0, t);
				Blend4AimPose(rAIM, rRot, rPos, i0, i1, i2, i3, x1, m_arrRelPose0, m_arrAbsPose0);
				Quatd q1 = mid * m_arrAbsPose0[nWeaponBoneIdx].q;
				Vec2d p0 = Vec2d(PolarCoordinate(q0));
				Vec2d p1 = Vec2d(PolarCoordinate(q1));
				for (int32 gx = -8; gx < +8; gx++)
				{
					f64 e = gx * (g_PI / 8);
					if ((p0.x < e && p1.x > e) || (p0.x > e && p1.x < e))
					{
						f64 d0 = e - p0.x;
						f64 d1 = p1.x - e;
						f64 d = d0 + d1;
						f64 b0 = 1 - d0 / d;
						f64 b1 = 1 - d1 / d;
						Vec2d polcor = p0 * b0 + p1 * b1;
						Vec4d weight = x0 * b0 + x1 * b1;
						CHUNK_GAHAIM_INFO::VirtualExampleInit2 ve2;
						ve2.polar.x = f32(polcor.x);
						ve2.polar.y = f32(polcor.y);
						ve2.i0 = i0;
						ve2.i1 = i1;
						ve2.i2 = i2;
						ve2.i3 = i3;
						ve2.w0 = f32(weight.x);
						ve2.w1 = f32(weight.y);
						ve2.w2 = f32(weight.z);
						ve2.w3 = f32(weight.w);
						rAIM.VE2.push_back(ve2);
					}
				}
				for (int32 gy = -5; gy < +5; gy++)
				{
					f64 e = gy * (g_PI / 8);
					if ((p0.y < e && p1.y > e) || (p0.y > e && p1.y < e))
					{
						f64 d0 = e - p0.y;
						f64 d1 = p1.y - e;
						f64 d = d0 + d1;
						f64 b0 = 1 - d0 / d;
						f64 b1 = 1 - d1 / d;
						Vec2d polcor = p0 * b0 + p1 * b1;
						Vec4d weight = x0 * b0 + x1 * b1;
						CHUNK_GAHAIM_INFO::VirtualExampleInit2 ve2;
						ve2.polar.x = f32(polcor.x);
						ve2.polar.y = f32(polcor.y);
						ve2.i0 = i0;
						ve2.i1 = i1;
						ve2.i2 = i2;
						ve2.i3 = i3;
						ve2.w0 = f32(weight.x);
						ve2.w1 = f32(weight.y);
						ve2.w2 = f32(weight.z);
						ve2.w3 = f32(weight.w);
						rAIM.VE2.push_back(ve2);
					}
				}
				q0 = q1;
				x0 = x1;
			}

			uint32 ddd = 0;
		}

		CHUNK_GAHAIM_INFO::VirtualExampleInit2 ve;
		for (int32 gy = -4; gy < 5; gy++)
		{
			for (int32 gx = -8; gx < 9; gx++)
			{
				f32 ex = gx * (gf_PI / 8);
				f32 ey = gy * (gf_PI / 8);

				if (PolarGrid[(4 + gy) * CHUNK_GAHAIM_INFO::XGRID + (gx + 8)].m_fSmalest > 1.00f)
				{
					f32 dist = 9999.0f;
					size_t nHowManyVE = rAIM.VE2.size();
					if (nHowManyVE == 0)
						break;
					for (uint32 i = 0; i < nHowManyVE; i++)
					{
						f32 fDistance = (Vec2(ex, ey) - rAIM.VE2[i].polar).GetLength();
						if (dist > fDistance)
						{
							dist = fDistance;
							ve = rAIM.VE2[i];
						}
					}

					PolarGrid[(4 + gy) * CHUNK_GAHAIM_INFO::XGRID + (gx + 8)].m_fSmalest = 0;

					PolarGrid[(4 + gy) * CHUNK_GAHAIM_INFO::XGRID + (gx + 8)].i0 = ve.i0;
					PolarGrid[(4 + gy) * CHUNK_GAHAIM_INFO::XGRID + (gx + 8)].i1 = ve.i1;
					PolarGrid[(4 + gy) * CHUNK_GAHAIM_INFO::XGRID + (gx + 8)].i2 = ve.i2;
					PolarGrid[(4 + gy) * CHUNK_GAHAIM_INFO::XGRID + (gx + 8)].i3 = ve.i3;

					PolarGrid[(4 + gy) * CHUNK_GAHAIM_INFO::XGRID + (gx + 8)].w0 = ve.w0;
					PolarGrid[(4 + gy) * CHUNK_GAHAIM_INFO::XGRID + (gx + 8)].w1 = ve.w1;
					PolarGrid[(4 + gy) * CHUNK_GAHAIM_INFO::XGRID + (gx + 8)].w2 = ve.w2;
					PolarGrid[(4 + gy) * CHUNK_GAHAIM_INFO::XGRID + (gx + 8)].w3 = ve.w3;
				}
			}
		}
		rAIM.VE2.clear();
	}

	for (int32 i = 0; i < (CHUNK_GAHAIM_INFO::XGRID * CHUNK_GAHAIM_INFO::YGRID); i++)
	{
		rAIM.m_PolarGrid[i].i0 = PolarGrid[i].i0;
		rAIM.m_PolarGrid[i].i1 = PolarGrid[i].i1;
		rAIM.m_PolarGrid[i].i2 = PolarGrid[i].i2;
		rAIM.m_PolarGrid[i].i3 = PolarGrid[i].i3;
		rAIM.m_PolarGrid[i].v0 = int16(PolarGrid[i].w0 * f64(0x2000));
		rAIM.m_PolarGrid[i].v1 = int16(PolarGrid[i].w1 * f64(0x2000));
		rAIM.m_PolarGrid[i].v2 = int16(PolarGrid[i].w2 * f64(0x2000));
		rAIM.m_PolarGrid[i].v3 = int16(PolarGrid[i].w3 * f64(0x2000));
	}
}

//----------------------------------------------------------------------------------------------

void VExampleInit::CopyPoses2(const DynArray<SJointsAimIK_Rot>& rRot, const DynArray<SJointsAimIK_Pos>& rPos, GlobalAnimationHeaderAIM& rAIM, uint32 numPoses, uint32 skey)
{

	f32 fRealTime = rAIM.NTime2KTime(f32(skey) / (numPoses - 1));

	uint32 numRot = rRot.size();
	for (uint32 r = 0; r < numRot; r++)
	{
		const char* pJointName = rRot[r].m_strJointName;
		int32 j = rRot[r].m_nJointIdx;
		assert(j > 0);
		rAIM.m_arrAimIKPosesAIM[skey].m_arrRotation[r] = m_arrDefaultRelPose[j].q;
		IController* pFAimController = rAIM.GetControllerByJointCRC32(m_arrJointCRC32[j]);
		if (pFAimController)
		{
			int32 numKeyRot = pFAimController->GetO_numKey();
			int32 numKeyPos = pFAimController->GetP_numKey();
			if (numKeyRot > 0 || numKeyPos > 0) //do we have a rotation channel
			{
				Quat q = m_arrDefaultRelPose[j].q;
				if (numKeyRot > 0)
					pFAimController->GetO(fRealTime, q);

				f32 dot = m_arrDefaultRelPose[j].q | q;
				if (dot < 0)
					q = -q;

				rAIM.m_arrAimIKPosesAIM[skey].m_arrRotation[r] = q;
				rAIM.m_nExist |= uint64(1) << r;
			}
		}
	}

	uint32 numPos = rPos.size();
	for (uint32 r = 0; r < numPos; r++)
	{
		const char* pJointName = rPos[r].m_strJointName;
		int32 j = rPos[r].m_nJointIdx;
		assert(j > 0);
		rAIM.m_arrAimIKPosesAIM[skey].m_arrPosition[r] = m_arrDefaultRelPose[j].t;
		IController* pFAimController = rAIM.GetControllerByJointCRC32(m_arrJointCRC32[j]);
		if (pFAimController)
		{
			int32 numKeys = pFAimController->GetP_numKey();
			if (numKeys > 0)  //do we have a position channel
			{
				Vec3 p;
				pFAimController->GetP(fRealTime, p);
				rAIM.m_arrAimIKPosesAIM[skey].m_arrPosition[r] = p;
			}
		}
	}
}

//----------------------------------------------------------------------------------------------

void VExampleInit::RecursiveTest(const Vec2d& ControlPoint, GlobalAnimationHeaderAIM& rAIM, const DynArray<SJointsAimIK_Rot>& rRot, const DynArray<SJointsAimIK_Pos>& rPos, int nWBone, int i0, int i1, int i2, int i3, const Vec4d& w0, const Vec4d& w1, const Vec4d& w2, const Vec4d& w3)
{
	uint32 nOverlap = PointInQuat(ControlPoint, rAIM, rRot, rPos, nWBone, i0, i1, i2, i3, w0, w1, w2, w3);
	if (nOverlap)
	{

		Vec4d mid = (w0 + w1 + w2 + w3) * 0.25;

		//w0 w1
		//w3 w2
		Vec4d x0a = w0;
		Vec4d x1a = (w0 + w1) * 0.5;
		Vec4d x0b = (w0 + w1) * 0.5;
		Vec4d x1b = w1;
		Vec4d x3a = (w0 + w3) * 0.5;
		Vec4d x2a = mid;
		Vec4d x3b = mid;
		Vec4d x2b = (w1 + w2) * 0.5;
		Vec4d x0d = (w0 + w3) * 0.5;
		Vec4d x1d = mid;
		Vec4d x0c = mid;
		Vec4d x1c = (w1 + w2) * 0.5;
		Vec4d x3d = w3;
		Vec4d x2d = (w3 + w2) * 0.5;
		Vec4d x3c = (w3 + w2) * 0.5;
		Vec4d x2c = w2;

		uint32 o0 = 0;
		uint32 o1 = 0;
		uint32 o2 = 0;
		uint32 o3 = 0;
		Vec4d w0a = x0a;
		Vec4d w1a = x1a;
		Vec4d w0b = x0b;
		Vec4d w1b = x1b;
		Vec4d w3a = x3a;
		Vec4d w2a = x2a;
		Vec4d w3b = x3b;
		Vec4d w2b = x2b;
		Vec4d w0d = x0d;
		Vec4d w1d = x1d;
		Vec4d w0c = x0c;
		Vec4d w1c = x1c;
		Vec4d w3d = x3d;
		Vec4d w2d = x2d;
		Vec4d w3c = x3c;
		Vec4d w2c = x2c;
		for (f32 s = 1.005f; s < 1.1f; s += 0.001f)
		{
			f64 a = s;
			f64 b = 1.0 - s;

			w0a = x0a * a + x2a * b;
			w1a = x1a * a + x3a * b;
			w0b = x0b * a + x2b * b;
			w1b = x1b * a + x3b * b;
			w3a = x3a * a + x1a * b;
			w2a = x2a * a + x0a * b;
			w3b = x3b * a + x1b * b;
			w2b = x2b * a + x0b * b;
			w0d = x0d * a + x2d * b;
			w1d = x1d * a + x3d * b;
			w0c = x0c * a + x2c * b;
			w1c = x1c * a + x3c * b;
			w3d = x3d * a + x1d * b;
			w2d = x2d * a + x0d * b;
			w3c = x3c * a + x1c * b;
			w2c = x2c * a + x0c * b;

			o0 = PointInQuat(ControlPoint, rAIM, rRot, rPos, nWBone, i0, i1, i2, i3, w0a, w1a, w2a, w3a);
			o1 = PointInQuat(ControlPoint, rAIM, rRot, rPos, nWBone, i0, i1, i2, i3, w0b, w1b, w2b, w3b);
			o2 = PointInQuat(ControlPoint, rAIM, rRot, rPos, nWBone, i0, i1, i2, i3, w0c, w1c, w2c, w3c);
			o3 = PointInQuat(ControlPoint, rAIM, rRot, rPos, nWBone, i0, i1, i2, i3, w0d, w1d, w2d, w3d);
			uint32 sum = o0 + o1 + o2 + o3;
			if (sum)
				break;
		}

		m_nIterations++;
		if (m_nIterations > 50)
			return;
		if (m_fSmallest < PRECISION)
			return;

		if (o0)
			RecursiveTest(ControlPoint, rAIM, rRot, rPos, nWBone, i0, i1, i2, i3, w0a, w1a, w2a, w3a);
		if (o1)
			RecursiveTest(ControlPoint, rAIM, rRot, rPos, nWBone, i0, i1, i2, i3, w0b, w1b, w2b, w3b);
		if (o2)
			RecursiveTest(ControlPoint, rAIM, rRot, rPos, nWBone, i0, i1, i2, i3, w0c, w1c, w2c, w3c);
		if (o3)
			RecursiveTest(ControlPoint, rAIM, rRot, rPos, nWBone, i0, i1, i2, i3, w0d, w1d, w2d, w3d);
	}
}

#pragma warning(push)
#pragma warning(disable : 6262) // this is only run in RC which has enough stack space
uint32 VExampleInit::PointInQuat(const Vec2d& ControlPoint, GlobalAnimationHeaderAIM& rAIM, const DynArray<SJointsAimIK_Rot>& rRot, const DynArray<SJointsAimIK_Pos>& rPos, int nWBone, int i0, int i1, int i2, int i3, const Vec4d& w0, const Vec4d& w1, const Vec4d& w2, const Vec4d& w3)
{
	f64 sum0 = w0.x + w0.y + w0.z + w0.w;
	assert(fabs(sum0 - 1.0f) < 0.00001);
	f64 sum1 = w1.x + w1.y + w1.z + w1.w;
	assert(fabs(sum1 - 1.0f) < 0.00001);
	f64 sum2 = w2.x + w2.y + w2.z + w2.w;
	assert(fabs(sum2 - 1.0f) < 0.00001);
	f64 sum3 = w3.x + w3.y + w3.z + w3.w;
	assert(fabs(sum3 - 1.0f) < 0.00001);

	Quatd mid;
	mid.w = f32(rAIM.m_MiddleAimPose.w);
	mid.v.x = f32(rAIM.m_MiddleAimPose.v.x);
	mid.v.y = f32(rAIM.m_MiddleAimPose.v.y);
	mid.v.z = f32(rAIM.m_MiddleAimPose.v.z);

	Blend4AimPose(rAIM, rRot, rPos, i0, i1, i2, i3, w0, m_arrRelPose0, m_arrAbsPose0);
	Quatd tq0 = mid * m_arrAbsPose0[nWBone].q;
	Blend4AimPose(rAIM, rRot, rPos, i0, i1, i2, i3, w1, m_arrRelPose1, m_arrAbsPose1);
	Quatd tq1 = mid * m_arrAbsPose1[nWBone].q;
	Blend4AimPose(rAIM, rRot, rPos, i0, i1, i2, i3, w2, m_arrRelPose2, m_arrAbsPose2);
	Quatd tq2 = mid * m_arrAbsPose2[nWBone].q;
	Blend4AimPose(rAIM, rRot, rPos, i0, i1, i2, i3, w3, m_arrRelPose3, m_arrAbsPose3);
	Quatd tq3 = mid * m_arrAbsPose3[nWBone].q;

	Vec2d tp0 = Vec2d(PolarCoordinate(tq0));
	assert(fabs(tp0.x) < 3.1416f);
	assert(fabs(tp0.y) < 2.0f);

	Vec2d tp1 = Vec2d(PolarCoordinate(tq1));
	assert(fabs(tp1.x) < 3.1416f);
	assert(fabs(tp1.y) < 2.0f);

	Vec2d tp2 = Vec2d(PolarCoordinate(tq2));
	assert(fabs(tp2.x) < 3.1416f);
	assert(fabs(tp2.y) < 2.0f);

	Vec2d tp3 = Vec2d(PolarCoordinate(tq3));
	assert(fabs(tp3.x) < 3.1416f);
	assert(fabs(tp3.y) < 2.0f);

	f64 t;
	uint32 c = 0;
	Vec2d polar[2000];
	Vec4d weight[2000];
	polar[0] = Vec2d(PolarCoordinate(tq0));
	weight[0] = w0;
	f64 maxstep = 0.250f;

	f64 angle0 = std::max(acos_tpl(tq1 | tq2), 0.01);
	assert(angle0 >= 0.009);
	f64 step0 = std::min((1.0 / (angle0 * angle0 * 30.0)), maxstep);
	for (f64 i = step0; i < 3.0; i += step0)
	{
		c++;
		t = i;
		if (i > 0.999) t = 1.0;
		NLerp2AimPose(rRot, rPos, m_arrRelPose0, m_arrRelPose1, t, m_arrAbsPose);
		Quatd qt = mid * m_arrAbsPose[nWBone].q;
		polar[c] = Vec2d(PolarCoordinate(qt));
		weight[c].SetLerp(w0, w1, t);
		;
		if (t == 1.0) break;
	}

	f64 angle1 = std::max(acos_tpl(tq1 | tq2), 0.01);
	assert(angle1 >= 0.009);
	f64 step1 = std::min((1.0 / (angle1 * angle1 * 30.0)), maxstep);
	for (f64 i = step1; i < 3.0; i += step1)
	{
		c++;
		t = i;
		if (i > 0.999) t = 1.0;
		NLerp2AimPose(rRot, rPos, m_arrRelPose1, m_arrRelPose2, t, m_arrAbsPose);
		Quatd qt = mid * m_arrAbsPose[nWBone].q;
		polar[c] = Vec2d(PolarCoordinate(qt));
		weight[c].SetLerp(w1, w2, t);
		;
		if (t == 1.0) break;
	}

	f64 angle2 = std::max(acos_tpl(tq2 | tq3), 0.01);
	assert(angle2 >= 0.009);
	f64 step2 = std::min((1.0 / (angle2 * angle2 * 30.0)), maxstep);
	for (f64 i = step2; i < 3.0; i += step2)
	{
		c++;
		t = i;
		if (i > 0.999) t = 1.0;
		NLerp2AimPose(rRot, rPos, m_arrRelPose2, m_arrRelPose3, t, m_arrAbsPose);
		Quatd qt = mid * m_arrAbsPose[nWBone].q;
		polar[c] = Vec2d(PolarCoordinate(qt));
		weight[c].SetLerp(w2, w3, t);
		;
		if (t == 1.0) break;
	}

	f64 angle3 = std::max(acos_tpl(tq3 | tq0), 0.01);
	assert(angle3 >= 0.009);
	f64 step3 = std::min((1.0 / (angle3 * angle3 * 30.0)), maxstep);
	for (f64 i = step3; i < 3.0; i += step3)
	{
		c++;
		t = i;
		if (i > 0.999) t = 1.0;
		NLerp2AimPose(rRot, rPos, m_arrRelPose3, m_arrRelPose0, t, m_arrAbsPose);
		Quat qt = mid * m_arrAbsPose[nWBone].q;
		polar[c] = Vec2d(PolarCoordinate(qt));
		weight[c].SetLerp(w3, w0, t);
		;
		if (t == 1.0) break;
	}
	assert(c);
	assert(c < 1000);
	f64 length = (polar[c] - polar[0]).GetLength();
	polar[c] = polar[0];

	Vec2d ControlPointEnd = ControlPoint + Vec2d(1, 0) * 10;
	Planed plane;
	plane.SetPlane(Vec2d(0, -1), ControlPoint);
	//pRenderer->Draw_Lineseg( Vec3(ControlPoint), RGBA8(0xff,0xff,0xff,0xff),Vec3(ControlPointEnd), RGBA8(0xff,0xff,0xff,0xff)  );

	int32 nOverlap = 0;
	Vec2d p0 = polar[0];
	for (uint32 l = 1; l < (c + 1); l++)
	{
		Vec2d p1 = polar[l];
		nOverlap += LinesegOverlap2D(plane, ControlPoint, ControlPointEnd, p0, p1);
		p0 = p1;
	}

	if (nOverlap & 1)
	{
		for (uint32 l = 0; l < (c + 1); l++)
		{
			f64 dif = (ControlPoint - polar[l]).GetLength();
			if (m_fSmallest > dif)
				m_fSmallest = dif, m_Weight = weight[l];
		}

	}

	return nOverlap & 1;
}
#pragma warning(pop)

void VExampleInit::ComputeAimPose(GlobalAnimationHeaderAIM& rAIM, const CDefaultSkeleton* pDefaultSkeleton, const DynArray<SJointsAimIK_Rot>& rRot, const DynArray<SJointsAimIK_Pos>& rPos, QuatTd* arrAbsPose, uint32 nAimPoseMid)
{
	uint32 numRotJoints = rRot.size();
	for (uint32 r = 0; r < numRotJoints; r++)
	{
		if (rRot[r].m_nPreEvaluate)
		{
			int32 j = rRot[r].m_nJointIdx;
			const char* pName = rRot[r].m_strJointName;
			QuatTd qtemp;
			qtemp.q = rAIM.m_arrAimIKPosesAIM[nAimPoseMid].m_arrRotation[r];
			qtemp.t = m_arrDefaultRelPose[j].t;
			int32 p = rRot[r].m_nPosIndex;
			if (p > -1)
				qtemp.t = rAIM.m_arrAimIKPosesAIM[nAimPoseMid].m_arrPosition[p];

			uint32 pidx = m_arrParentIdx[j];
			arrAbsPose[j] = arrAbsPose[pidx] * qtemp;
		}
	}
}

void VExampleInit::Blend4AimPose(GlobalAnimationHeaderAIM& rAIM, const DynArray<SJointsAimIK_Rot>& rRot, const DynArray<SJointsAimIK_Pos>& rPos, int8 i0, int8 i1, int8 i2, int8 i3, const Vec4d& w, QuatTd* arrRelPose, QuatTd* arrAbsPose)
{
	uint32 numRotJoints = rRot.size();
	for (uint32 r = 0; r < numRotJoints; r++)
	{
		if (rRot[r].m_nPreEvaluate)
		{
			int32 j = rRot[r].m_nJointIdx;
			arrRelPose[j].q.w = 0.0f;
			arrRelPose[j].q.v.x = 0.0f;
			arrRelPose[j].q.v.y = 0.0f;
			arrRelPose[j].q.v.z = 0.0f;

			arrRelPose[j].t.x = 0.0f;
			arrRelPose[j].t.y = 0.0f;
			arrRelPose[j].t.z = 0.0f;
		}
	}

	if (w.x)
	{
		for (uint32 r = 0; r < numRotJoints; r++)
		{
			if (rRot[r].m_nPreEvaluate)
			{
				int32 j = rRot[r].m_nJointIdx;
				Quatd qRot = rAIM.m_arrAimIKPosesAIM[i0].m_arrRotation[r];
				Vec3d vPos = m_arrDefaultRelPose[j].t;
				int32 p = rRot[r].m_nPosIndex;
				if (p > -1)
					vPos = rAIM.m_arrAimIKPosesAIM[i0].m_arrPosition[p];

				arrRelPose[j].q += qRot * w.x;
				arrRelPose[j].t += vPos * w.x;
			}
		}
	}

	if (w.y)
	{
		for (uint32 r = 0; r < numRotJoints; r++)
		{
			if (rRot[r].m_nPreEvaluate)
			{
				int32 j = rRot[r].m_nJointIdx;
				Quatd qRot = rAIM.m_arrAimIKPosesAIM[i1].m_arrRotation[r];
				Vec3d vPos = m_arrDefaultRelPose[j].t;
				int32 p = rRot[r].m_nPosIndex;
				if (p > -1)
					vPos = rAIM.m_arrAimIKPosesAIM[i1].m_arrPosition[p];

				arrRelPose[j].q += qRot * w.y;
				arrRelPose[j].t += vPos * w.y;
			}
		}
	}

	if (w.z)
	{
		for (uint32 r = 0; r < numRotJoints; r++)
		{
			if (rRot[r].m_nPreEvaluate)
			{
				int32 j = rRot[r].m_nJointIdx;
				Quatd qRot = rAIM.m_arrAimIKPosesAIM[i2].m_arrRotation[r];
				Vec3d vPos = m_arrDefaultRelPose[j].t;
				int32 p = rRot[r].m_nPosIndex;
				if (p > -1)
					vPos = rAIM.m_arrAimIKPosesAIM[i2].m_arrPosition[p];

				arrRelPose[j].q += qRot * w.z;
				arrRelPose[j].t += vPos * w.z;
			}
		}
	}

	if (w.w)
	{
		for (uint32 r = 0; r < numRotJoints; r++)
		{
			if (rRot[r].m_nPreEvaluate)
			{
				int32 j = rRot[r].m_nJointIdx;
				Quatd qRot = rAIM.m_arrAimIKPosesAIM[i3].m_arrRotation[r];
				Vec3d vPos = m_arrDefaultRelPose[j].t;
				int32 p = rRot[r].m_nPosIndex;
				if (p > -1)
					vPos = rAIM.m_arrAimIKPosesAIM[i3].m_arrPosition[p];

				arrRelPose[j].q += qRot * w.w;
				arrRelPose[j].t += vPos * w.w;
			}
		}
	}

	for (uint32 r = 0; r < numRotJoints; r++)
	{
		if (rRot[r].m_nPreEvaluate)
		{
			int32 j = rRot[r].m_nJointIdx;
			int32 p = m_arrParentIdx[j];
			arrRelPose[j].q.Normalize();
			arrAbsPose[j] = arrAbsPose[p] * arrRelPose[j];
		}
	}
}

//---------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------
void VExampleInit::NLerp2AimPose(const DynArray<SJointsAimIK_Rot>& rRot, const DynArray<SJointsAimIK_Pos>& rPos, QuatTd* arrRelPose0, QuatTd* arrRelPose1, f64 t, QuatTd* arrAbsPose)
{
	uint32 numRotJoints = rRot.size();
	f64 t0 = 1.0 - t;
	f64 t1 = t;
	QuatTd qt;
	for (uint32 r = 0; r < numRotJoints; r++)
	{
		if (rRot[r].m_nPreEvaluate)
		{
			int32 j = rRot[r].m_nJointIdx;
			int32 p = m_arrParentIdx[j];
			qt.q = arrRelPose0[j].q * t0 + arrRelPose1[j].q * t1;
			qt.t = arrRelPose0[j].t * t0 + arrRelPose1[j].t * t1;
			qt.q.Normalize();
			arrAbsPose[j] = arrAbsPose[p] * qt;
		}
	}
}

uint32 VExampleInit::LinesegOverlap2D(const Planed& plane, const Vec2d& ls0, const Vec2d& le0, const Vec2d& tp0, const Vec2d& tp1)
{
	f64 d0 = plane | tp0;
	f64 d1 = plane | tp1;
	if (d0 < 0 && d1 < 0) return 0;
	if (d0 >= 0 && d1 >= 0) return 0;

	Vec2d n = tp1 - tp0;
	if ((n | n) < 0.0000000001)
		return 0;
	n.Normalize();
	f64 d = -tp0.y * n.x + tp0.x * n.y;
	f64 d2 = n.y * ls0.x - n.x * ls0.y - d;
	f64 d3 = n.y * le0.x - n.x * le0.y - d;
	if (d2 <= 0 && d3 <= 0) return 0;
	if (d2 > 0 && d3 > 0) return 0;
	return 1;
}

Vec3d VExampleInit::PolarCoordinate(const Quatd& q)
{
	Matrix33d m = Matrix33d(q);
	f64 l = sqrt(m.m01 * m.m01 + m.m11 * m.m11);
	if (l > static_cast<f64>(0.0001))
		return Vec3d(atan2(-m.m01 / l, m.m11 / l), -atan2(m.m21, l), atan2(-m.m20 / l, m.m22 / l));
	else
		return Vec3d(0, atan2(m.m21, l), 0);
}

uint32 VExampleInit::AnnotateExamples(uint32 numPoses, QuadIndices* arrQuat)
{
	uint32 q = (numPoses / 3) - 1;
	for (uint32 i = 0; i < q; i++)
	{
		arrQuat[i + 0].i0 = i + 0;
		arrQuat[i + 0].w0 = Vec4(1, 0, 0, 0);
		arrQuat[i + 0].i1 = i + 1;
		arrQuat[i + 0].w1 = Vec4(0, 1, 0, 0);
		arrQuat[i + 0].i2 = i + q + 2;
		arrQuat[i + 0].w2 = Vec4(0, 0, 1, 0);
		arrQuat[i + 0].i3 = i + q + 1;
		arrQuat[i + 0].w3 = Vec4(0, 0, 0, 1);
		arrQuat[i + 0].col = RGBA8(0x00, 0xff, 0x00, 0xff);
		arrQuat[i + 0].height = Vec3(0, 0, 0.001f);

		arrQuat[i + q].i0 = i + q + 1;
		arrQuat[i + q].w0 = Vec4(1, 0, 0, 0);
		arrQuat[i + q].i1 = i + q + 2;
		arrQuat[i + q].w1 = Vec4(0, 1, 0, 0);
		arrQuat[i + q].i2 = i + q + q + 3;
		arrQuat[i + q].w2 = Vec4(0, 0, 1, 0);
		arrQuat[i + q].i3 = i + q + q + 2;
		arrQuat[i + q].w3 = Vec4(0, 0, 0, 1);
		arrQuat[i + q].col = RGBA8(0x00, 0xff, 0x00, 0xff);
		arrQuat[i + q].height = Vec3(0, 0, 0.001f);
	}

	if (numPoses != 9)
		return q * 2;

	uint32 i = -1;
	f32 t = 0;

	f32 diag = 1.70f;

	f32 eup = 1.60f;
	f32 mup = 1.20f;

	f32 mside = 2.00f;
	f32 eside = 1.90f;

	f32 edown = 1.6f;
	f32 mdown = 1.20f;

	i = 4;
	arrQuat[i].i0 = 3;
	t = eup;
	arrQuat[i].w0 = Vec4(1 - t, 0, 0, t);                           //3-mirrored 0-scaled
	arrQuat[i].i1 = 4;
	t = mup;
	arrQuat[i].w1 = Vec4(0, 1 - t, t, 0);                           //4-mirrored 1-scaled
	arrQuat[i].i2 = 1;
	t = 1.00f;
	arrQuat[i].w2 = Vec4(0, 0, 1, 0);
	arrQuat[i].i3 = 0;
	t = 1.00f;
	arrQuat[i].w3 = Vec4(0, 0, 0, 1);
	arrQuat[i].col = RGBA8(0xff, 0x00, 0x00, 0xff);
	arrQuat[i].height = Vec3(0, 0, 0.001f);

	i = 5;
	arrQuat[i].i0 = 3;
	t = 1.00f;
	arrQuat[i].w0 = Vec4(1, 0, 0, 0);
	arrQuat[i].i1 = 4;
	t = mside;
	arrQuat[i].w1 = Vec4(t, 1 - t, 0, 0);                           //4-mirrored 3-scaled
	arrQuat[i].i2 = 1;
	t = eside;
	arrQuat[i].w2 = Vec4(0, 0, 1 - t, t);                           //1-mirrored 0-scaled
	arrQuat[i].i3 = 0;
	t = 1.00f;
	arrQuat[i].w3 = Vec4(0, 0, 0, 1);
	arrQuat[i].col = RGBA8(0xff, 0x00, 0x00, 0xff);
	arrQuat[i].height = Vec3(0, 0, 0.001f);

	i = 6;
	arrQuat[i].i0 = 4;
	t = diag;
	arrQuat[i].w0 = Vec4(1 - t, 0, t, 0);                           //4-mirrored 0-scaled
	arrQuat[i].i1 = 3;
	t = eup;
	arrQuat[i].w1 = Vec4(0, 1 - t, t, 0);                           //3-mirrored 0-scaled
	arrQuat[i].i2 = 0;
	t = 1.00f;
	arrQuat[i].w2 = Vec4(0, 0, 1, 0);
	arrQuat[i].i3 = 1;
	t = eside;
	arrQuat[i].w3 = Vec4(0, 0, t, 1 - t);                           //1-mirrored 0-scaled
	arrQuat[i].col = RGBA8(0xff, 0x00, 0x00, 0xff);
	arrQuat[i].height = Vec3(0, 0, 0.001f);

	i = 7;
	arrQuat[i].i0 = 2;
	t = 1.00f;
	arrQuat[i].w0 = Vec4(1, 0, 0, 0);
	arrQuat[i].i1 = 1;
	t = eside;
	arrQuat[i].w1 = Vec4(t, 1 - t, 0, 0);                           //1-mirrored 2-scaled
	arrQuat[i].i2 = 4;
	t = mside;
	arrQuat[i].w2 = Vec4(0, 0, 1 - t, t);                           //4-mirrored 5-scaled
	arrQuat[i].i3 = 5;
	t = 1.00f;
	arrQuat[i].w3 = Vec4(0, 0, 0, 1);
	arrQuat[i].col = RGBA8(0xff, 0x00, 0x00, 0xff);
	arrQuat[i].height = Vec3(0, 0, 0.001f);

	i = 8;
	arrQuat[i].i0 = 4;
	t = mup;
	arrQuat[i].w0 = Vec4(1 - t, 0, 0, t);                           //4-mirrored 1-scaled
	arrQuat[i].i1 = 5;
	t = eup;
	arrQuat[i].w1 = Vec4(0, 1 - t, t, 0);                           //5-mirrored 2-scaled
	arrQuat[i].i2 = 2;
	t = 1.00f;
	arrQuat[i].w2 = Vec4(0, 0, 1, 0);
	arrQuat[i].i3 = 1;
	t = 1.00f;
	arrQuat[i].w3 = Vec4(0, 0, 0, 1);
	arrQuat[i].col = RGBA8(0xff, 0x00, 0x00, 0xff);
	arrQuat[i].height = Vec3(0, 0, 0.001f);

	i = 9;
	arrQuat[i].i0 = 5;
	t = eup;
	arrQuat[i].w0 = Vec4(1 - t, 0, 0, t);                           //5-mirrored 2-scaled
	arrQuat[i].i1 = 4;
	t = diag;
	arrQuat[i].w1 = Vec4(0, 1 - t, 0, t);                           //4-mirrored 2-scaled
	arrQuat[i].i2 = 1;
	t = eside;
	arrQuat[i].w2 = Vec4(0, 0, 1 - t, t);                           //1-mirrored 2-scaled
	arrQuat[i].i3 = 2;
	t = 1.0f;
	arrQuat[i].w3 = Vec4(0, 0, 0, 1);
	arrQuat[i].col = RGBA8(0xff, 0x00, 0x00, 0xff);
	arrQuat[i].height = Vec3(0, 0, 0.001f);

	i = 10;
	arrQuat[i].i0 = 4;
	t = mside;
	arrQuat[i].w0 = Vec4(1 - t, t, 0, 0);                           //4-mirrored 3-scaled
	arrQuat[i].i1 = 3;
	t = 1.00f;
	arrQuat[i].w1 = Vec4(0, 1, 0, 0);
	arrQuat[i].i2 = 6;
	t = 1.00f;
	arrQuat[i].w2 = Vec4(0, 0, 1, 0);
	arrQuat[i].i3 = 7;
	t = eside;
	arrQuat[i].w3 = Vec4(0, 0, t, 1 - t);                           //7-mirrored 6-scaled
	arrQuat[i].col = RGBA8(0xff, 0x00, 0x00, 0xff);
	arrQuat[i].height = Vec3(0, 0, 0.001f);
	i = 11;
	arrQuat[i].i0 = 6;
	t = 1.00f;
	arrQuat[i].w0 = Vec4(1, 0, 0, 0);
	arrQuat[i].i1 = 7;
	t = 1.00f;
	arrQuat[i].w1 = Vec4(0, 1, 0, 0);
	arrQuat[i].i2 = 4;
	t = mdown;
	arrQuat[i].w2 = Vec4(0, t, 1 - t, 0);                           //4-mirrored 7-scaled
	arrQuat[i].i3 = 3;
	t = edown;
	arrQuat[i].w3 = Vec4(t, 0, 0, 1 - t);                           //3-mirrored 6-scaled
	arrQuat[i].col = RGBA8(0xff, 0x00, 0x00, 0xff);
	arrQuat[i].height = Vec3(0, 0, 0.001f);
	i = 12;
	arrQuat[i].i0 = 7;
	t = eside;
	arrQuat[i].w0 = Vec4(1 - t, t, 0, 0);                           //7-mirrored 6-scaled
	arrQuat[i].i1 = 6;
	t = 1.00f;
	arrQuat[i].w1 = Vec4(0, 1, 0, 0);
	arrQuat[i].i2 = 3;
	t = edown;
	arrQuat[i].w2 = Vec4(0, t, 1 - t, 0);                           //3-mirrored 6-scaled
	arrQuat[i].i3 = 4;
	t = diag;
	arrQuat[i].w3 = Vec4(0, t, 0, 1 - t);                           //4-mirrored 6-scaled
	arrQuat[i].col = RGBA8(0xff, 0x00, 0x00, 0xff);
	arrQuat[i].height = Vec3(0, 0, 0.001f);

	i = 13;
	arrQuat[i].i0 = 5;
	t = 1.00f;
	arrQuat[i].w0 = Vec4(1, 0, 0, 0);
	arrQuat[i].i1 = 4;
	t = mside;
	arrQuat[i].w1 = Vec4(t, 1 - t, 0, 0);                            //4 mirrored 5-scaled
	arrQuat[i].i2 = 7;
	t = eside;
	arrQuat[i].w2 = Vec4(0, 0, 1 - t, t);                            //7 mirrored 8-scaled
	arrQuat[i].i3 = 8;
	t = 1.00f;
	arrQuat[i].w3 = Vec4(0, 0, 0, 1);
	arrQuat[i].col = RGBA8(0xff, 0x00, 0x00, 0xff);
	arrQuat[i].height = Vec3(0, 0, 0.001f);
	i = 14;
	arrQuat[i].i0 = 7;
	t = 1.00f;
	arrQuat[i].w0 = Vec4(1, 0, 0, 0);
	arrQuat[i].i1 = 8;
	t = 1.00f;
	arrQuat[i].w1 = Vec4(0, 1, 0, 0);
	arrQuat[i].i2 = 5;
	t = edown;
	arrQuat[i].w2 = Vec4(0, t, 1 - t, 0);                           //5-mirrored 8-scaled
	arrQuat[i].i3 = 4;
	t = mdown;
	arrQuat[i].w3 = Vec4(t, 0, 0, 1 - t);                           //4-mirrored 7-scaled
	arrQuat[i].col = RGBA8(0xff, 0x00, 0x00, 0xff);
	arrQuat[i].height = Vec3(0, 0, 0.001f);
	i = 15;
	arrQuat[i].i0 = 8;
	t = 1.00f;
	arrQuat[i].w0 = Vec4(1, 0, 0, 0);
	arrQuat[i].i1 = 7;
	t = eside;
	arrQuat[i].w1 = Vec4(t, 1 - t, 0, 0);                           //7-mirrored 8-scaled
	arrQuat[i].i2 = 4;
	t = diag;
	arrQuat[i].w2 = Vec4(t, 0, 1 - t, 0);                           //4-mirrored 8-scaled cross
	arrQuat[i].i3 = 5;
	t = edown;
	arrQuat[i].w3 = Vec4(t, 0, 0, 1 - t);                           //5-mirrored 8-scaled
	arrQuat[i].col = RGBA8(0xff, 0x00, 0x00, 0xff);
	arrQuat[i].height = Vec3(0, 0, 0.001f);

	return q * 2 + 3 + 3 + 3 + 3;
}
