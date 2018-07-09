// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "LoaderTCB.h"

#include <limits>
#include <CryString/CryPath.h>
#include <Cry3DEngine/CGF/CGFContent.h>
#include <CryString/StringUtils.h>
#include "ControllerPQ.h"

//////////////////////////////////////////////////////////////////////////
CLoaderTCB::CLoaderTCB()
{
	m_pSkinningInfo = 0;
	m_bHasNewControllers = false;
	m_bLoadOldChunks = false;
	m_bHasOldControllers = false;
	m_bLoadOnlyCommon = false;
}

//////////////////////////////////////////////////////////////////////////
CLoaderTCB::~CLoaderTCB()
{
	if (m_pSkinningInfo)
	{
		delete m_pSkinningInfo;
		m_pSkinningInfo = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
CHeaderTCB* CLoaderTCB::LoadTCB(const char* filename, ILoaderCAFListener* pListener)
{
	IChunkFile* chunkFile = g_pI3DEngine->CreateChunkFile(true);
	CHeaderTCB* pInfo = LoadTCB(filename, chunkFile, pListener);
	chunkFile->Release();
	return pInfo;
}
//////////////////////////////////////////////////////////////////////////
CHeaderTCB* CLoaderTCB::LoadTCB(const char* filename, IChunkFile* pChunkFile, ILoaderCAFListener* pListener)
{
	stack_string strPath = filename;
	PathUtil::UnifyFilePath(strPath);

	m_filename = filename;

	if (!pChunkFile->Read(filename))
	{
		m_LastError = pChunkFile->GetLastError();
		return 0;
	}

	// Load mesh from chunk file.

	m_pSkinningInfo = new CHeaderTCB;

	if (!LoadChunksTCB(pChunkFile))
	{
		delete m_pSkinningInfo;
		m_pSkinningInfo = 0;
	}

	return m_pSkinningInfo;
}

bool CLoaderTCB::LoadChunksTCB(IChunkFile* pChunkFile)
{
	// Load Nodes.
	uint32 numChunck = pChunkFile->NumChunks();

	uint32 statTCB(0), statNewChunck(0), statOldChunck(0);
	int maxTCBChunkID(0);

	for (uint32 i = 0; i < numChunck; i++)
	{
		const IChunkFile::ChunkDesc* const pChunkDesc = pChunkFile->GetChunk(i);
		if (pChunkDesc->chunkType == ChunkType_Controller &&
		    pChunkDesc->chunkVersion > CONTROLLER_CHUNK_DESC_0828::VERSION)
		{
			CryFatalError("CONTROLLER_CHUNK_DESC_0828");
			m_bHasNewControllers = true;
			++statNewChunck;
		}
		else
		{
			if (pChunkDesc->chunkType == ChunkType_Controller &&
			    (pChunkDesc->chunkVersion == CONTROLLER_CHUNK_DESC_0828::VERSION || pChunkDesc->chunkVersion == CONTROLLER_CHUNK_DESC_0827::VERSION))
			{
				CryFatalError("CONTROLLER_CHUNK_DESC_0828");
				m_bHasOldControllers = true;
				++statOldChunck;
			}
			else
			{
				if (pChunkDesc->chunkType == ChunkType_Controller &&
				    pChunkDesc->chunkVersion == CONTROLLER_CHUNK_DESC_0826::VERSION)
				{
					if (pChunkDesc->chunkId > maxTCBChunkID)
					{
						maxTCBChunkID = pChunkDesc->chunkId;
					}
					++statTCB;
				}
			}
		}
	}

	if (!m_bHasNewControllers)
		m_bLoadOldChunks = true;

	if (m_bLoadOnlyCommon == 0)
	{
		// Only old data will be loaded.
		if (m_bLoadOldChunks)
		{
			m_pSkinningInfo->m_arrControllerId.reserve(statOldChunck);
			m_pSkinningInfo->m_pControllers.reserve(statOldChunck);
		}
		else
		{
			m_pSkinningInfo->m_arrControllerId.reserve(statNewChunck);
			m_pSkinningInfo->m_pControllers.reserve(statNewChunck);

		}

		// We have a TCB data. Reserve memory
		if (statTCB > 0)
		{
			m_pSkinningInfo->m_arrControllersTCB.resize(maxTCBChunkID + 1);
			m_pSkinningInfo->m_TrackVec3QQQ.reserve(statTCB);
			m_pSkinningInfo->m_TrackQuat.reserve(statTCB);
		}
	}

	for (uint32 i = 0; i < numChunck; i++)
	{
		IChunkFile::ChunkDesc* const pChunkDesc = pChunkFile->GetChunk(i);
		switch (pChunkDesc->chunkType)
		{
		case ChunkType_Controller:
			if (!ReadController(pChunkDesc))
				return false;
			break;

		case ChunkType_Timing:
			if (!ReadTiming(pChunkDesc))
				return false;
			break;

		case ChunkType_MotionParameters:
			if (!ReadMotionParameters(pChunkDesc))
				return false;
			break;
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderTCB::ReadMotionParameters(IChunkFile::ChunkDesc* pChunkDesc)
{
	CryFatalError("ReadMotionParameters");
	if (pChunkDesc->chunkVersion == SPEED_CHUNK_DESC_2::VERSION)
	{
		//	CryFatalError("SPEED_CHUNK_DESC_2 not supported any more");
		SPEED_CHUNK_DESC_2* pChunk = (SPEED_CHUNK_DESC_2*)pChunkDesc->data;
		SwapEndian(*pChunk);
		m_pSkinningInfo->m_nAnimFlags = pChunk->AnimFlags;
		return 1;
	}

	if (pChunkDesc->chunkVersion == CHUNK_MOTION_PARAMETERS::VERSION)
	{
		if (pChunkDesc->bSwapEndian)
		{
			CryFatalError("%s: data are stored in non-native endian format", __FUNCTION__);
		}

		CHUNK_MOTION_PARAMETERS* pChunk = (CHUNK_MOTION_PARAMETERS*)pChunkDesc->data;

		m_pSkinningInfo->m_nAnimFlags = pChunk->mp.m_nAssetFlags;
		m_pSkinningInfo->m_nCompression = pChunk->mp.m_nCompression;

		m_pSkinningInfo->m_nStart = pChunk->mp.m_nStart;
		m_pSkinningInfo->m_nEnd = pChunk->mp.m_nEnd;
		return 1;
	}

	return 1;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderTCB::ReadTiming(IChunkFile::ChunkDesc* pChunkDesc)
{
	switch (pChunkDesc->chunkVersion)
	{

	case TIMING_CHUNK_DESC_0918::VERSION:
		{
			const auto pChunk = static_cast<TIMING_CHUNK_DESC_0918*>(pChunkDesc->data);
			SwapEndian(*pChunk, pChunkDesc->bSwapEndian);
			pChunkDesc->bSwapEndian = false;

			m_pSkinningInfo->m_nStart = pChunk->global_range.start;
			m_pSkinningInfo->m_nEnd = pChunk->global_range.end;
		}
		return true;

	case TIMING_CHUNK_DESC_0919::VERSION:
		{
			static const auto roundToFrames = [](double x) { return std::floor(x + (x >= 0.0 ? 0.5 : -0.5)); };

			const auto pChunk = static_cast<TIMING_CHUNK_DESC_0919*>(pChunkDesc->data);
			SwapEndian(*pChunk, pChunkDesc->bSwapEndian);
			pChunkDesc->bSwapEndian = false;

			const double startTimeInFrames = roundToFrames(double(pChunk->startTimeInSeconds) * ANIMATION_30Hz);
			const double durationInFrames = pChunk->numberOfSamples - 1;

			if (pChunk->numberOfSamples <= 0)
			{
				CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_ERROR, "Timing chunk (0x%04x) declares a nonpositive number of samples (%d). File: '%s'", pChunk->VERSION, pChunk->numberOfSamples, m_filename.c_str());
				return false;
			}

			if ((startTimeInFrames + durationInFrames) > double(std::numeric_limits<int32>::max()))
			{
				CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_ERROR, "Timing chunk (0x%04x) exceeds the maximum animation duration supported by the TCB loader. File: '%s'", pChunk->VERSION, m_filename.c_str());
				return false;
			}

			if ((std::abs(pChunk->samplesPerSecond - ANIMATION_30Hz) / ANIMATION_30Hz) > std::numeric_limits<float>::epsilon())
			{
				CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, "Timing chunk (0x%04x) declares a currently unsupported framerate value (%.2f fps). Framerate will be overridden to %.2f fps. File: '%s'", pChunk->VERSION, pChunk->samplesPerSecond, ANIMATION_30Hz, m_filename.c_str());
			}

			m_pSkinningInfo->m_nStart = static_cast<int32>(startTimeInFrames);
			m_pSkinningInfo->m_nEnd = static_cast<int32>(startTimeInFrames + durationInFrames);
		}
		return true;

	default:
		return false;

	}
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderTCB::ReadController(IChunkFile::ChunkDesc* pChunkDesc)
{
	if (m_bLoadOnlyCommon)
		return true;

	if (pChunkDesc->chunkVersion == CONTROLLER_CHUNK_DESC_0826::VERSION)
	{
		CONTROLLER_CHUNK_DESC_0826* pCtrlChunk = (CONTROLLER_CHUNK_DESC_0826*)pChunkDesc->data;
		const bool bSwapEndianness = pChunkDesc->bSwapEndian;
		SwapEndian(*pCtrlChunk, bSwapEndianness);
		pChunkDesc->bSwapEndian = false;

		if (pCtrlChunk->type == CTRL_TCB3)
		{
			DynArray<CryTCB3Key>* pTrack = new DynArray<CryTCB3Key>;
			TCBFlags trackflags;

			CryTCB3Key* pKeys = (CryTCB3Key*)(pCtrlChunk + 1);
			uint32 nkeys = pCtrlChunk->nKeys;
			pTrack->resize(nkeys);

			if (bSwapEndianness)
			{
				for (uint32 i = 0; i < nkeys; i++)
				{
					SwapEndian(pKeys[i], true);
				}
			}

			for (uint32 i = 0; i < nkeys; i++)
			{
				//track[i] = pKeys[i];//
				pTrack-> operator[](i) = (pKeys[i]);
			}

			if (pCtrlChunk->nFlags & CTRL_ORT_CYCLE)
				trackflags.f0 = 1;
			else if (pCtrlChunk->nFlags & CTRL_ORT_LOOP)
				trackflags.f1 = 1;

			uint32 numControllers = m_pSkinningInfo->m_TrackVec3QQQ.size();
			m_pSkinningInfo->m_TrackVec3FlagsQQQ.push_back(trackflags);
			m_pSkinningInfo->m_TrackVec3QQQ.push_back(pTrack);

			int32 num = m_pSkinningInfo->m_arrControllersTCB.size();
			if (pChunkDesc->chunkId < num)
			{
				m_pSkinningInfo->m_arrControllersTCB[pChunkDesc->chunkId].m_controllertype = POSCHANNEL;
				m_pSkinningInfo->m_arrControllersTCB[pChunkDesc->chunkId].m_index = numControllers;
			}
			return true;
		}

		if (pCtrlChunk->type == CTRL_TCBQ)
		{
			DynArray<CryTCBQKey>* pTrack = new DynArray<CryTCBQKey>;
			TCBFlags trackflags;

			CryTCBQKey* pKeys = (CryTCBQKey*)(pCtrlChunk + 1);
			uint32 nkeys = pCtrlChunk->nKeys;
			pTrack->resize(nkeys);

			if (bSwapEndianness)
			{
				for (uint32 i = 0; i < nkeys; i++)
				{
					SwapEndian(pKeys[i], true);
				}
			}

			for (uint32 i = 0; i < nkeys; i++)
			{
				//track[i] = pKeys[i];// * secsPerTick;
				pTrack-> operator[](i) = pKeys[i];
			}

			if (pCtrlChunk->nFlags & CTRL_ORT_CYCLE)
				trackflags.f0 = 1;
			else if (pCtrlChunk->nFlags & CTRL_ORT_LOOP)
				trackflags.f1 = 1;
			;

			uint32 numControllers = m_pSkinningInfo->m_TrackQuat.size();
			m_pSkinningInfo->m_TrackQuatFlagsQQQ.push_back(trackflags);
			m_pSkinningInfo->m_TrackQuat.push_back(pTrack);

			int32 num = m_pSkinningInfo->m_arrControllersTCB.size();
			if (pChunkDesc->chunkId < num)
			{
				m_pSkinningInfo->m_arrControllersTCB[pChunkDesc->chunkId].m_controllertype = ROTCHANNEL;
				m_pSkinningInfo->m_arrControllersTCB[pChunkDesc->chunkId].m_index = numControllers;
			}
			return true;
		}

		if (pCtrlChunk->type == CTRL_CRYBONE)
		{
			m_LastError.Format("animation not loaded: controller-type CTRL_CRYBONE not supported");
			return true;
		}

		if (pCtrlChunk->type == CTRL_BEZIER3)
		{
			m_LastError.Format("animation not loaded: controller-type CTRL_BEZIER3 not supported");
			return true;
		}

		m_LastError.Format("animation not loaded: found unsupported controller type in chunk");
		return false;
	}

	if (pChunkDesc->chunkVersion == CONTROLLER_CHUNK_DESC_0827::VERSION)
	{

		if (m_bLoadOldChunks)
		{
			CONTROLLER_CHUNK_DESC_0827* pCtrlChunk = (CONTROLLER_CHUNK_DESC_0827*)pChunkDesc->data;
			const bool bSwapEndianness = pChunkDesc->bSwapEndian;
			SwapEndian(*pCtrlChunk, bSwapEndianness);
			pChunkDesc->bSwapEndian = false;

			size_t numKeys = pCtrlChunk->numKeys;

			CryKeyPQLog* pCryKey = (CryKeyPQLog*)(pCtrlChunk + 1);
			SwapEndian(pCryKey, numKeys, bSwapEndianness);

			CControllerPQLog* pController = new CControllerPQLog;

			pController->m_nControllerId = pCtrlChunk->nControllerId;

			pController->m_arrKeys.resize(numKeys);
			pController->m_arrTimes.resize(numKeys);

			for (size_t i = 0; i < numKeys; ++i)
			{
				pController->m_arrTimes[i] = pCryKey[i].nTime / TICKS_CONVERT;
				pController->m_arrKeys[i].position = pCryKey[i].vPos / 100.0f;
				pController->m_arrKeys[i].rotationLog = pCryKey[i].vRotLog;
				pController->m_arrKeys[i].scale = Vec3(1.0f);
			}
			m_pSkinningInfo->m_arrControllerId.push_back(pCtrlChunk->nControllerId);
			m_pSkinningInfo->m_pControllers.push_back(pController);
		}

		return true;
	}

	if (pChunkDesc->chunkVersion == CONTROLLER_CHUNK_DESC_0828::VERSION)
	{
		CryFatalError("CONTROLLER_CHUNK_DESC_0828 is not supported anymore");
		return true;
	}

	if (pChunkDesc->chunkVersion == CONTROLLER_CHUNK_DESC_0829::VERSION)
	{

		if (m_bLoadOldChunks)
			return true;

		if (pChunkDesc->bSwapEndian)
			CryFatalError("%s: data are stored in non-native endian format", __FUNCTION__);

		CONTROLLER_CHUNK_DESC_0829* pCtrlChunk = (CONTROLLER_CHUNK_DESC_0829*)pChunkDesc->data;
		//		SwapEndian(*pCtrlChunk);

		CController* pController = new CController;

		pController->m_nControllerId = pCtrlChunk->nControllerId;

		m_pSkinningInfo->m_arrControllerId.push_back(pController->m_nControllerId);
		m_pSkinningInfo->m_pControllers.push_back(pController);

		char* pData = (char*)(pCtrlChunk + 1);

		_smart_ptr<IKeyTimesInformation> pRotTimeKeys;
		_smart_ptr<IKeyTimesInformation> pPosTimeKeys;
		//		KeyTimesInformationPtr pSclTimeKeys;

		IRotationController* pRotation;
		IPositionController* pPosition;
		//		PositionControllerPtr pScale;

		if (pCtrlChunk->numRotationKeys)
		{
			// we have a rotation info
			pRotation = new RotationTrackInformation;

			ITrackRotationStorage* pStorage = ControllerHelper::GetRotationControllerPtr(pCtrlChunk->RotationFormat);
			if (!pStorage)
				return false;

			pRotation->SetRotationStorage(pStorage);

			pData += pStorage->AssignData(pData, pCtrlChunk->numRotationKeys);

			pRotTimeKeys = ControllerHelper::GetKeyTimesControllerPtr(pCtrlChunk->RotationTimeFormat);//new F32KeyTimesInformation;//CKeyTimesInformation;
			if (!pRotTimeKeys)
				return false;

			pData += pRotTimeKeys->AssignKeyTime(pData, pCtrlChunk->numRotationKeys);

			pRotation->SetKeyTimesInformation(pRotTimeKeys);

			pController->SetRotationController(pRotation);
		}

		if (pCtrlChunk->numPositionKeys)
		{
			pPosition = new PositionTrackInformation;

			ITrackPositionStorage* pStorage = ControllerHelper::GetPositionControllerPtr(pCtrlChunk->PositionFormat);
			if (!pStorage)
				return false;

			pPosition->SetPositionStorage(pStorage);

			pData += pStorage->AssignData(pData, pCtrlChunk->numPositionKeys);

			if (pCtrlChunk->PositionKeysInfo == CONTROLLER_CHUNK_DESC_0829::eKeyTimeRotation)
			{
				pPosition->SetKeyTimesInformation(pRotTimeKeys);
			}
			else
			{
				// load from chunk
				pPosTimeKeys = ControllerHelper::GetKeyTimesControllerPtr(pCtrlChunk->PositionTimeFormat);
				;                                                                                          //PositionTimeFormat;new F32KeyTimesInformation;//CKeyTimesInformation;
				pData += pPosTimeKeys->AssignKeyTime(pData, pCtrlChunk->numPositionKeys);

				pPosition->SetKeyTimesInformation(pPosTimeKeys);
			}

			pController->SetPositionController(pPosition);
		}

		return true;
	}

	if (pChunkDesc->chunkVersion == CONTROLLER_CHUNK_DESC_0831::VERSION)
	{
		return true;
	}

	if (pChunkDesc->chunkVersion == CONTROLLER_CHUNK_DESC_0832::VERSION)
	{
		return false;
	}

	return false;
}
