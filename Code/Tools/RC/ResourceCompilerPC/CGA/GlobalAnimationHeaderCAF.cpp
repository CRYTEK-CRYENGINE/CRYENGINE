// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GlobalAnimationHeaderCAF.h"
#include "AnimationCompiler.h" // for SEndiannessSwapper
#include "../CryEngine/Cry3DEngine/CGF/ChunkData.h"


static const size_t MAX_NUMBER_OF_KEYS = 65535;


 // Checks if time keys stored within the given IKeyTimesInformation object are in a strictly ascending order.
static bool ValidateTimeKeys(const IKeyTimesInformation& timeKeys)
{
	const uint32 keysCount = timeKeys.GetNumKeys();

	float lastTime = -(std::numeric_limits<float>::max)();
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


GlobalAnimationHeaderCAF::GlobalAnimationHeaderCAF()
{
	m_nFlags = 0;
	m_nCompression		 = -2;

	m_FilePathCRC32		 = 0;
	m_FilePathDBACRC32 = 0;

	m_nStartKey = -1;
	m_nEndKey   = -1;
	m_fStartSec = -1; // Start time in seconds.
	m_fEndSec   = -1; // End time in seconds.
	m_nCompressedControllerCount = 0;

	m_fDistance		= -1.0f;
	m_fSpeed			= -1.0f;
	m_fSlope			=  0.0f;
	m_fTurnSpeed 	=	0.0f;					//asset-features
	m_fAssetTurn 	=	0.0f;					//asset-features

	m_StartLocation2.SetIdentity();
	m_LastLocatorKey2.SetIdentity();
}

IController* GlobalAnimationHeaderCAF::GetController(uint32 nControllerID)
{
	const size_t numController = m_arrController.size();
	for (size_t i = 0; i < numController; ++i)
	{
		if (m_arrController[i] && m_arrController[i]->GetID() == nControllerID)
		{
			return m_arrController[i];
		}
	}
	return 0;
}


bool GlobalAnimationHeaderCAF::ReplaceController(IController* old, IController* newContoller)
{
	const size_t numControllers = m_arrController.size();
	for (size_t i = 0; i < numControllers; ++i)
	{
		if (m_arrController[i] == old)
		{
			m_arrController[i] = newContoller;
			return true;
		}
	}
	return false;
}

size_t GlobalAnimationHeaderCAF::CountCompressedControllers() const 
{
	size_t result = 0;

	const size_t numControllers = m_arrController.size();
	for (uint32 i = 0; i < numControllers; ++i)
	{
		const IController_AutoPtr& ptr = m_arrController[i];
		if (ptr && ptr->GetCController())
		{
			bool alreadyCounted = false;
			for (size_t j = 0; j < i; ++j)
			{
				if(ptr.get() == m_arrController[j].get())
				{
					alreadyCounted = true;
					break;
				}
			}
			if (!alreadyCounted)
			{
				++result;
			}
		}
	}
	return result;
}

bool GlobalAnimationHeaderCAF::LoadCAF(const string& filename, ECAFLoadMode loadMode, CChunkFile* chunkFile)
{
	CChunkFile localChunkFile;
	if (!chunkFile)
	{
		chunkFile = &localChunkFile;
	}

	m_arrController.clear();
	m_FootPlantBits.clear();

	if (!chunkFile->Read(filename.c_str()))
	{
		RCLogError("Chunk file error: %s", chunkFile->GetLastError());
		return false;
	}

	int32 lastControllerID = -1;
	if (!LoadChunks(chunkFile, loadMode, lastControllerID, filename.c_str()))
	{
		// TODO descriptive message
		/*
		const char *lastControllerName = "NONE";
		for (uint32 boneID = 0; boneID < m_pInfo->m_SkinningInfo.m_arrBonesDesc.size(); boneID++)
			if (lastControllerID == m_pInfo->m_SkinningInfo.m_arrBonesDesc[boneID].m_nControllerID)
			{
				lastControllerName = m_pInfo->m_SkinningInfo.m_arrBonesDesc[boneID].m_arrBoneName;
				break;
			}
		RCLogError( "Failed to load animation file %s - %s \n Last Controller: %s", filename.c_str(), cgfLoader.GetLastError(), lastControllerName);
		*/
		RCLogError( "Failed to load animation file %s \n Last Controller: %i", filename.c_str(), lastControllerID);
		return false;
	}

	BindControllerIdsToControllerNames();

	return true;
}

static CControllerPQLog* ReadControllerV0827(const CONTROLLER_CHUNK_DESC_0827* chunk, bool bSwapEndianness, const char* chunkEnd, const char* logFilename, bool& strippedFrames)
{
	int numKeys = chunk->numKeys;
	if (numKeys > MAX_NUMBER_OF_KEYS)
	{
		RCLogError("animation not loaded: insane number of keys (%u). Please stay below %u: %s", numKeys, MAX_NUMBER_OF_KEYS, logFilename);
		return 0;
	}
	CryKeyPQLog* pCryKey = (CryKeyPQLog*)(chunk+1);
	SwapEndian(pCryKey, numKeys, bSwapEndianness);
	if ((char*)(pCryKey + numKeys) > chunkEnd)
	{
		RCLogError("animation not loaded: estimated keys data size exceeds chunk size: %s", logFilename);
		return 0;
	}

	std::auto_ptr<CControllerPQLog> controller(new CControllerPQLog);
	controller->m_nControllerId = chunk->nControllerId;
	controller->m_arrKeys.reserve(numKeys);
	controller->m_arrTimes.reserve(numKeys);

	int lasttime = INT_MIN; 
	for (int i=0; i<numKeys; ++i)
	{
		// Using zero-based time allows our compressor to encode time as uint8 or uint16 instead of float
		// We are adding +1 here as some assets have negative keys shifted by one tick, eg. -319 -159 0 160 320 ...
		int curtime = (pCryKey[i].nTime - pCryKey[0].nTime + 1) / TICKS_CONVERT;
		if (curtime > lasttime)
		{
			if (!pCryKey[i].vPos.IsValid())
			{
				RCLogError("animation not loaded: controller key %u has invalid float value(s) in position: %s", i, logFilename);
				return 0;
			}

			if (!pCryKey[i].vRotLog.IsValid())
			{
				RCLogError("animation not loaded: controller key %u has invalid float value(s) in rotation: %s", i, logFilename);
				return 0;
			}
			PQLogS pqs;
			pqs.position = pCryKey[i].vPos / 100.0f;
			pqs.rotationLog = pCryKey[i].vRotLog;
			pqs.scale = Diag33(IDENTITY);
			controller->m_arrKeys.push_back(pqs);
			controller->m_arrTimes.push_back(curtime);
			lasttime = curtime;
		}
		else if (curtime == lasttime && i == numKeys - 1)
		{
			// Gap between last two frames can be different from typical frame duration.
			strippedFrames = true;
		}
		else
		{
			RCLogError("controller contains repeated or non-sorted time key: %s", logFilename);
			return 0;
		}
	}
	return controller.release();
}

static CController* ReadControllerV0829(const CONTROLLER_CHUNK_DESC_0829* chunk, bool bSwapEndianness, const char* chunkEnd, const char* logFilename)
{
	if (bSwapEndianness)
	{
		RCLogError("%s: changing endianness is not implemented yet: %s", __FUNCTION__, logFilename);
		return 0;
	}

	std::auto_ptr<CController> pController(new CController);
	pController->SetID(chunk->nControllerId);

	char *pData = (char*)(chunk+1);
	int nAlignment = chunk->TracksAligned ? 4 : 1;

	KeyTimesInformationPtr pRotTimeKeys = 0;

	if (chunk->numRotationKeys)
	{
		if (chunk->numRotationKeys > MAX_NUMBER_OF_KEYS)
		{
			RCLogError("animation not loaded: insane number of rotation keys (%u). Please stay below %u. %s", (unsigned)chunk->numRotationKeys, MAX_NUMBER_OF_KEYS, logFilename);
			return 0;
		}
		// we have a rotation info
		TrackRotationStoragePtr pStorage = ControllerHelper::ConstructRotationController((ECompressionFormat)chunk->RotationFormat);
		if (!pStorage)
		{
			RCLogError("Unable to to get rotation storage: %s", logFilename);
			return 0;
		}

		RotationControllerPtr rotation = RotationControllerPtr(new RotationTrackInformation);
		rotation->SetRotationStorage(pStorage);


		pStorage->Resize(chunk->numRotationKeys);
		if (pData + pStorage->GetDataRawSize() > chunkEnd)
		{
			RCLogError("animation not loaded: estimated data size exceeds chunk size: %s", logFilename);
			return 0;
		}
		// its not very safe but its extremely fast!
		memcpy(pStorage->GetData(), pData, pStorage->GetDataRawSize());
		pData+=Align(pStorage->GetDataRawSize(), nAlignment);

		pRotTimeKeys = ControllerHelper::ConstructKeyTimesController(chunk->RotationTimeFormat);//new F32KeyTimesInformation;//CKeyTimesInformation;
		pRotTimeKeys->ResizeKeyTime(chunk->numRotationKeys);

		if (pData + pRotTimeKeys->GetDataRawSize() > chunkEnd)
		{
			RCLogError("animation not loaded: estimated data size exceeds chunk size: %s", logFilename);
			return 0;
		}
		memcpy(pRotTimeKeys->GetData(), pData, pRotTimeKeys->GetDataRawSize());
		pData += Align(pRotTimeKeys->GetDataRawSize(), nAlignment);

		rotation->SetKeyTimesInformation(pRotTimeKeys);
		pController->SetRotationController(rotation);

		if (!ValidateTimeKeys(*pRotTimeKeys))
		{
			RCLogError("Controller contains repeated or non-sorted time keys");
			return nullptr;
		}
	}

	if (chunk->numPositionKeys)
	{
		if (chunk->numPositionKeys > MAX_NUMBER_OF_KEYS)
		{
			RCLogError("animation not loaded: insane number of position keys (%u). Please stay below %u. %s", (unsigned)chunk->numPositionKeys, MAX_NUMBER_OF_KEYS, logFilename);
			return 0;
		}

		TrackPositionStoragePtr pStorage = ControllerHelper::ConstructPositionController((ECompressionFormat)chunk->PositionFormat);
		if (!pStorage)
		{
			RCLogError("Unable to to get position storage: %s", logFilename);
			return 0;
		}

		PositionControllerPtr pPosition = PositionControllerPtr(new PositionTrackInformation);
		pPosition->SetPositionStorage(pStorage);
		pStorage->Resize(chunk->numPositionKeys);
		if (pData + pStorage->GetDataRawSize() > chunkEnd)
		{
			RCLogError("animation not loaded: estimated data size exceeds chunk size: %s", logFilename);
			return 0;
		}

		memcpy(pStorage->GetData(), pData, pStorage->GetDataRawSize());
		pData += Align(pStorage->GetDataRawSize(), nAlignment);

		if (chunk->PositionKeysInfo == CONTROLLER_CHUNK_DESC_0829::eKeyTimeRotation)
		{
			pPosition->SetKeyTimesInformation(pRotTimeKeys);
		}
		else
		{
			// load from chunk
			KeyTimesInformationPtr pPosTimeKeys = ControllerHelper::ConstructKeyTimesController(chunk->PositionTimeFormat);;//PositionTimeFormat;new F32KeyTimesInformation;//CKeyTimesInformation;
			pPosTimeKeys->ResizeKeyTime(chunk->numPositionKeys);

			if (pData + pPosTimeKeys->GetDataRawSize() > chunkEnd)
			{
				RCLogError("animation not loaded: estimated data size exceeds chunk size: %s", logFilename);
				return 0;
			}
			memcpy(pPosTimeKeys->GetData(), pData, pPosTimeKeys->GetDataRawSize());
			pData += Align(pPosTimeKeys->GetDataRawSize(), nAlignment);
			(void)pData;  // just to quiet compiler warning about unused pData value after '+='

			pPosition->SetKeyTimesInformation(pPosTimeKeys);

			if (!ValidateTimeKeys(*pPosTimeKeys))
			{
				RCLogError("Controller contains repeating or non-sorted time keys");
				return nullptr;
			}
		}
		pController->SetPositionController(pPosition);

	}
	return pController.release();
}

static CController* ReadControllerV0832(const CONTROLLER_CHUNK_DESC_0832* chunk, bool bSwapEndianness, const char* chunkEnd, const char* logFilename)
{
	if (bSwapEndianness)
	{
		RCLogError("%s: File contains data in a non-native endian format: %s", __FUNCTION__, logFilename);
		return nullptr;
	}

	const char* pData = reinterpret_cast<const char*>(chunk + 1);

	_smart_ptr<IRotationController> pRotation;
	if (chunk->numRotationKeys > 0)
	{
		_smart_ptr<ITrackRotationStorage> pValues = ControllerHelper::ConstructRotationController(static_cast<ECompressionFormat>(chunk->rotationFormat));
		if (!pValues)
		{
			RCLogError("Unable to construct rotation controller for the specified format id (%u): '%s'", static_cast<uint32>(chunk->rotationFormat), logFilename);
			return nullptr;
		}

		pValues->Resize(chunk->numRotationKeys);
		if (pData + pValues->GetDataRawSize() > chunkEnd)
		{
			RCLogError("Unexpected end of chunk. The animation file is corrupted: '%s'", logFilename);
			return nullptr;
		}

		memcpy(pValues->GetData(), pData, pValues->GetDataRawSize());
		pData += Align(pValues->GetDataRawSize(), 4);

		_smart_ptr<IKeyTimesInformation> pTimeKeys = ControllerHelper::ConstructKeyTimesController(chunk->rotationTimeFormat);
		if (!pTimeKeys)
		{
			RCLogError("Unable to construct rotation timing info for the specified format id (%u): '%s'", static_cast<uint32>(chunk->rotationTimeFormat), logFilename);
			return nullptr;
		}

		pTimeKeys->ResizeKeyTime(chunk->numRotationKeys);
		if (pData + pTimeKeys->GetDataRawSize() > chunkEnd)
		{
			RCLogError("Unexpected end of chunk. The animation file is corrupted: '%s'", logFilename);
			return nullptr;
		}

		memcpy(pTimeKeys->GetData(), pData, pTimeKeys->GetDataRawSize());
		pData += Align(pTimeKeys->GetDataRawSize(), 4);

		if (!ValidateTimeKeys(*pTimeKeys))
		{
			RCLogError("Rotation controller contains repeated or non-sorted time keys: '%s'", logFilename);
			return nullptr;
		}

		pRotation = new RotationTrackInformation;
		pRotation->SetRotationStorage(pValues);
		pRotation->SetKeyTimesInformation(pTimeKeys);
	}

	_smart_ptr<IPositionController> pPosition;
	if (chunk->numPositionKeys > 0)
	{
		_smart_ptr<ITrackPositionStorage> pValues = ControllerHelper::ConstructPositionController(static_cast<ECompressionFormat>(chunk->positionFormat));
		if (!pValues)
		{
			RCLogError("Unable to construct position controller for the specified format id (%u): '%s'", static_cast<uint32>(chunk->positionFormat), logFilename);
			return nullptr;
		}

		pValues->Resize(chunk->numPositionKeys);
		if (pData + pValues->GetDataRawSize() > chunkEnd)
		{
			RCLogError("Unexpected end of chunk. The animation file is corrupted: '%s'", logFilename);
			return nullptr;
		}

		memcpy(pValues->GetData(), pData, pValues->GetDataRawSize());
		pData += Align(pValues->GetDataRawSize(), 4);

		_smart_ptr<IKeyTimesInformation> pPosTimeKeys;
		if (chunk->positionKeysInfo == CONTROLLER_CHUNK_DESC_0832::eKeyTimeRotation)
		{
			pPosTimeKeys = pRotation->GetKeyTimesInformation();
		}
		else
		{
			pPosTimeKeys = ControllerHelper::ConstructKeyTimesController(chunk->positionTimeFormat);
			if (!pPosTimeKeys)
			{
				RCLogError("Unable to construct position timing info for the specified format id (%u): '%s'", static_cast<uint32>(chunk->positionTimeFormat), logFilename);
				return nullptr;
			}

			pPosTimeKeys->ResizeKeyTime(chunk->numPositionKeys);
			if (pData + pPosTimeKeys->GetDataRawSize() > chunkEnd)
			{
				RCLogError("Unexpected end of chunk. The animation file is corrupted: '%s'", logFilename);
				return nullptr;
			}

			memcpy(pPosTimeKeys->GetData(), pData, pPosTimeKeys->GetDataRawSize());
			pData += Align(pPosTimeKeys->GetDataRawSize(), 4);

			if (!ValidateTimeKeys(*pPosTimeKeys))
			{
				RCLogError("Position controller contains repeated or non-sorted time keys: '%s'", logFilename);
				return nullptr;
			}
		}

		pPosition = new PositionTrackInformation;
		pPosition->SetPositionStorage(pValues);
		pPosition->SetKeyTimesInformation(pPosTimeKeys);
	}

	_smart_ptr<IPositionController> pScale;
	if (chunk->numScaleKeys > 0)
	{
		_smart_ptr<ITrackPositionStorage> pValues = ControllerHelper::ConstructScaleController(static_cast<ECompressionFormat>(chunk->scaleFormat));
		if (!pValues)
		{
			RCLogError("Unable to construct scale controller for the specified format id (%u): '%s'", static_cast<uint32>(chunk->scaleFormat), logFilename);
			return nullptr;
		}

		pValues->Resize(chunk->numScaleKeys);
		if (pData + pValues->GetDataRawSize() > chunkEnd)
		{
			RCLogError("Unexpected end of chunk. The animation file is corrupted: '%s'", logFilename);
			return nullptr;
		}

		memcpy(pValues->GetData(), pData, pValues->GetDataRawSize())	;
		pData += Align(pValues->GetDataRawSize(), 4);

		_smart_ptr<IKeyTimesInformation> pTimeKeys;
		if (chunk->scaleKeysInfo == CONTROLLER_CHUNK_DESC_0832::eKeyTimeRotation)
		{
			pTimeKeys = pRotation->GetKeyTimesInformation();
		}
		else if(chunk->scaleKeysInfo == CONTROLLER_CHUNK_DESC_0832::eKeyTimePosition)
		{
			pTimeKeys = pPosition->GetKeyTimesInformation();
		}
		else
		{
			pTimeKeys = ControllerHelper::ConstructKeyTimesController(chunk->scaleTimeFormat);
			if (!pTimeKeys)
			{
				RCLogError("Unable to construct scale timing info for the specified format id (%u): '%s'", static_cast<uint32>(chunk->scaleTimeFormat), logFilename);
				return nullptr;
			}

			pTimeKeys->ResizeKeyTime(chunk->numScaleKeys);
			if (pData + pTimeKeys->GetDataRawSize() > chunkEnd)
			{
				RCLogError("Unexpected end of chunk. The animation file is corrupted: '%s'", logFilename);
				return nullptr;
			}

			memcpy(pTimeKeys->GetData(), pData, pTimeKeys->GetDataRawSize());
			// cppcheck-suppress unreadVariable
			pData += Align(pTimeKeys->GetDataRawSize(), 4);

			if (!ValidateTimeKeys(*pTimeKeys))
			{
				RCLogError("Scale controller contains repeated or non-sorted time keys: '%s'", logFilename);
				return nullptr;
			}
		}

		pScale = new PositionTrackInformation;
		pScale->SetPositionStorage(pValues);
		pScale->SetKeyTimesInformation(pTimeKeys);
	}

	std::unique_ptr<CController> pController(new CController);
	pController->SetID(chunk->nControllerId);
	pController->SetRotationController(pRotation);
	pController->SetPositionController(pPosition);
	pController->SetScaleController(pScale);

	return pController.release();
}

static CControllerPQLog* ReadControllerV0833(const CONTROLLER_CHUNK_DESC_0833* chunk, bool bSwapEndianness, const char* chunkEnd, const char* logFilename)
{
	const size_t numKeys = chunk->numKeys;
	if (numKeys > MAX_NUMBER_OF_KEYS)
	{
		RCLogError("animation not loaded: too many keys (%u); please stay below %u: %s", numKeys, MAX_NUMBER_OF_KEYS, logFilename);
		return 0;
	}

	CryKeyPQS* pCryKey = (CryKeyPQS*)(chunk + 1);
	if ((char*)(pCryKey + numKeys) > chunkEnd)
	{
		RCLogError("animation not loaded: file is corrupted: %s", logFilename);
		return 0;
	}

	SwapEndian(pCryKey, numKeys, bSwapEndianness);

	std::unique_ptr<CControllerPQLog> controller(new CControllerPQLog);
	controller->m_nControllerId = chunk->controllerId;
	controller->m_arrKeys.reserve(numKeys);
	controller->m_arrTimes.reserve(numKeys);

	for (size_t i = 0; i < numKeys; ++i)
	{
		const int curtime = int(i);

		if (!pCryKey[i].position.IsValid())
		{
			RCLogError("animation not loaded: controller key %u has invalid float value(s) in position: %s", i, logFilename);
			return 0;
		}

		if (!pCryKey[i].rotation.IsValid())
		{
			RCLogError("animation not loaded: controller key %u has invalid float value(s) in rotation: %s", i, logFilename);
			return 0;
		}

		if (!pCryKey[i].scale.IsValid())
		{
			RCLogError("animation not loaded: controller key %u has invalid float value(s) in scale: %s", i, logFilename);
			return 0;
		}

		PQLogS pqs;
		pqs.position = pCryKey[i].position / 100.0f;
		pqs.rotationLog = Quat::log(!pCryKey[i].rotation);
		pqs.scale = pCryKey[i].scale;
		controller->m_arrKeys.push_back(pqs);
		controller->m_arrTimes.push_back(curtime);
	}
	return controller.release();
}

bool CAF_ReadController(IChunkFile::ChunkDesc *chunk, ECAFLoadMode loadMode, IController_AutoPtr& controller, int32& lastControllerID, const char* logFilename, bool& strippedFrames)
{
	const char* chunkEnd = (const char*)chunk->data + chunk->size;

	switch(chunk->chunkVersion)
	{
		case CONTROLLER_CHUNK_DESC_0826::VERSION:
		{
			RCLogError("Controller V0826 is not supported anymore: %s", logFilename);
			return false;
		}
		case CONTROLLER_CHUNK_DESC_0827::VERSION:
		{
			if (loadMode == eCAFLoadCompressedOnly)
			{
				RCLogError("Uncompressed controller (V0827) in 'compressed' animation. Aborting: %s", logFilename);
				return false;
			}

			CONTROLLER_CHUNK_DESC_0827* pCtrlChunk = (CONTROLLER_CHUNK_DESC_0827*)chunk->data;

			const bool bSwapEndianness = chunk->bSwapEndian;
			SwapEndian(*pCtrlChunk, bSwapEndianness);
			chunk->bSwapEndian = false;

			lastControllerID = pCtrlChunk->nControllerId;
			controller = ReadControllerV0827(pCtrlChunk, bSwapEndianness, chunkEnd, logFilename, strippedFrames);

			return (controller != nullptr);
		}
		case CONTROLLER_CHUNK_DESC_0828::VERSION:
		{
			RCLogWarning("Animation contains controllers in unsupported format 0x0828, please reexport it: %s", logFilename);
			controller = nullptr;
			return true;
		}
		case CONTROLLER_CHUNK_DESC_0829::VERSION:
		{
			if (loadMode == eCAFLoadUncompressedOnly)
			{
				// skip compressed controllers
				controller = nullptr;
				return true;
			}

			CONTROLLER_CHUNK_DESC_0829* pCtrlChunk = (CONTROLLER_CHUNK_DESC_0829*)chunk->data;

			const bool bSwapEndianness = chunk->bSwapEndian;
			SwapEndian(*pCtrlChunk, bSwapEndianness);
			chunk->bSwapEndian = false;

			lastControllerID = pCtrlChunk->nControllerId;
			controller = ReadControllerV0829(pCtrlChunk, bSwapEndianness, chunkEnd, logFilename);

			return (controller != nullptr);
		}
		case CONTROLLER_CHUNK_DESC_0830::VERSION:
		{
			controller = nullptr;
			RCLogWarning("Skipping 0830 controller chunk: %s", logFilename);
			return true;
		}
		case CONTROLLER_CHUNK_DESC_0832::VERSION:
		{
			if (loadMode == eCAFLoadUncompressedOnly)
			{
				// skip compressed controllers
				controller = nullptr;
				return true;
			}

			CONTROLLER_CHUNK_DESC_0832* pCtrlChunk = (CONTROLLER_CHUNK_DESC_0832*)chunk->data;

			const bool bSwapEndianness = chunk->bSwapEndian;
			SwapEndian(*pCtrlChunk, bSwapEndianness);
			chunk->bSwapEndian = false;

			lastControllerID = pCtrlChunk->nControllerId;
			controller = ReadControllerV0832(pCtrlChunk, bSwapEndianness, chunkEnd, logFilename);

			return (controller != nullptr);
		}
		case CONTROLLER_CHUNK_DESC_0833::VERSION:
		{
			if (loadMode == eCAFLoadCompressedOnly)
			{
				RCLogError("Uncompressed controller (V0833) in 'compressed' animation. Aborting: %s", logFilename);
				return false;
			}

			CONTROLLER_CHUNK_DESC_0833* pCtrlChunk = (CONTROLLER_CHUNK_DESC_0833*)chunk->data;

			const bool bSwapEndianness = chunk->bSwapEndian;
			SwapEndian(*pCtrlChunk, bSwapEndianness);
			chunk->bSwapEndian = false;

			lastControllerID = pCtrlChunk->controllerId;
			controller = ReadControllerV0833(pCtrlChunk, bSwapEndianness, chunkEnd, logFilename);

			return (controller != nullptr);
		}

		default:
			RCLogWarning("Unknown controller chunk version (0x%08x): %s", chunk->chunkVersion, logFilename);
			return false;
	}
}

bool CAF_EnsureLoadModeApplies(const IChunkFile* chunkFile, ECAFLoadMode loadMode, const char* logFilename)
{
	bool hasTcbControllers = false;
	bool hasUncompressedControllers = false;
	bool hasCompressedControllers = false;

	const int numChunks = chunkFile->NumChunks();
	for (int i = 0; i < numChunks; i++)
	{
		const IChunkFile::ChunkDesc* const pChunkDesc = chunkFile->GetChunk(i);

		if (pChunkDesc->chunkType == ChunkType_Controller)
		{
			switch (pChunkDesc->chunkVersion)
			{

			case CONTROLLER_CHUNK_DESC_0826::VERSION:
				hasTcbControllers = true;
				break;

			case CONTROLLER_CHUNK_DESC_0827::VERSION:
			case CONTROLLER_CHUNK_DESC_0833::VERSION:
				hasUncompressedControllers = true;
				break;

			case CONTROLLER_CHUNK_DESC_0829::VERSION:
			case CONTROLLER_CHUNK_DESC_0832::VERSION:
				hasCompressedControllers = true;
				break;

			case CONTROLLER_CHUNK_DESC_0828::VERSION:
			case CONTROLLER_CHUNK_DESC_0830::VERSION:
			case CONTROLLER_CHUNK_DESC_0831::VERSION:
				RCLogWarning("Obsolete controller chunk version (0x%04x): '%s'", pChunkDesc->chunkVersion, logFilename);
				break;

			default:
				RCLogWarning("Unrecognized controller chunk version (0x%04x): '%s'", pChunkDesc->chunkVersion, logFilename);
				break;

			}
		}
	}

	if (loadMode == eCAFLoadUncompressedOnly && !hasUncompressedControllers)
	{
		RCLogError("CAF file doesn't contain animation in uncompressed format: '%s'", logFilename);
		return false;
	}

	if (loadMode == eCAFLoadCompressedOnly && !hasCompressedControllers && (hasUncompressedControllers || hasTcbControllers))
	{
		RCLogWarning("CAF file doesn't contain compressed animation: '%s'", logFilename);
	}

	return true;
}

bool GlobalAnimationHeaderCAF::ReadMotionParameters(const IChunkFile::ChunkDesc *chunk, ECAFLoadMode loadMode, const char* logFilename)
{
	// TODO check chunk data size
	
	if (chunk->chunkVersion == SPEED_CHUNK_DESC_2::VERSION)
	{
		if (loadMode == eCAFLoadCompressedOnly)
		{
			RCLogWarning("Old motion-parameters chunk in 'compressed' animation: %s", logFilename);
			return false;
		}

		SPEED_CHUNK_DESC_2* desc = (SPEED_CHUNK_DESC_2*)chunk->data;
		SwapEndian(*desc);

		m_fSpeed		= desc->Speed;
		m_fDistance = desc->Distance;
		m_fSlope		= desc->Slope;

		const uint32 validFlags = (CA_ASSET_BIG_ENDIAN|CA_ASSET_CREATED|CA_ASSET_ONDEMAND|CA_ASSET_LOADED|CA_ASSET_ADDITIVE|CA_ASSET_CYCLE|CA_ASSET_NOT_FOUND);
		m_nFlags = desc->AnimFlags & validFlags;
		m_StartLocation2 = desc->StartPosition;
		assert(m_StartLocation2.IsValid());
		return true;
	}
	else if (chunk->chunkVersion == CHUNK_MOTION_PARAMETERS::VERSION)
	{
		if (loadMode == eCAFLoadUncompressedOnly)
		{
			// someone checked in a file, that has only compressed data. 
			// RC will not process this data, but we should initialize the header correctly to prevent a crash
		}

		CHUNK_MOTION_PARAMETERS* c = (CHUNK_MOTION_PARAMETERS*)chunk->data;
		static_assert(eLittleEndian == false, "Only little endian is allowed here!");

		const MotionParams905* motionParams = &c->mp;

		m_nFlags = motionParams->m_nAssetFlags;
		m_nCompression = motionParams->m_nCompression;

		const int32 nStartKey = 0;
		const int32 nEndKey = motionParams->m_nEnd - motionParams->m_nStart;
		m_nStartKey	= nStartKey;
		m_nEndKey	= nEndKey;

		const int32 fTicksPerFrame = TICKS_PER_FRAME;
		const f32 fSecsPerTick = SECONDS_PER_TICK;
		const f32 fSecsPerFrame = fSecsPerTick * fTicksPerFrame;
		m_fStartSec = nStartKey * fSecsPerFrame;
		m_fEndSec = nEndKey * fSecsPerFrame;
		if (m_fEndSec<=m_fStartSec)
		{
			m_fEndSec  = m_fStartSec+(1.0f/30.0f);
		}

		m_fSpeed = motionParams->m_fMoveSpeed;
		m_fTurnSpeed = motionParams->m_fTurnSpeed;
		m_fAssetTurn = motionParams->m_fAssetTurn;
		m_fDistance = motionParams->m_fDistance;
		m_fSlope = motionParams->m_fSlope;

		m_StartLocation2 = motionParams->m_StartLocation;
		m_LastLocatorKey2 = motionParams->m_EndLocation;

		m_FootPlantVectors.m_LHeelStart = motionParams->m_LHeelStart;
		m_FootPlantVectors.m_LHeelEnd = motionParams->m_LHeelEnd;
		m_FootPlantVectors.m_LToe0Start = motionParams->m_LToe0Start;
		m_FootPlantVectors.m_LToe0End = motionParams->m_LToe0End;
		m_FootPlantVectors.m_RHeelStart = motionParams->m_RHeelStart;
		m_FootPlantVectors.m_RHeelEnd = motionParams->m_RHeelEnd;
		m_FootPlantVectors.m_RToe0Start = motionParams->m_RToe0Start;
		m_FootPlantVectors.m_RToe0End = motionParams->m_RToe0End;

		return true;
	}
	else
	{
		RCLogWarning("Unknown version of motion parameters chunk: 0x%08x in %s", chunk->chunkVersion, logFilename);
		return false;
	}
}

bool GlobalAnimationHeaderCAF::ReadGlobalAnimationHeader(const IChunkFile::ChunkDesc *chunk, ECAFLoadMode loadMode, const char* logFilename)
{
	if (chunk->chunkVersion == CHUNK_GAHCAF_INFO::VERSION)
	{
		if (loadMode != eCAFLoadCompressedOnly)
		{
			RCLogError("Unexpected CHUNK_GAHCAF_INFO (ChunkType_GlobalAnimationHeaderCAF) in %s", logFilename);
			return false;
		}

		const CHUNK_GAHCAF_INFO* const gah = (CHUNK_GAHCAF_INFO*)chunk->data;

		m_FilePathCRC32 = gah->m_FilePathCRC32;
		m_FilePathDBACRC32 = gah->m_FilePathDBACRC32;

		const size_t maxFilePathLen = sizeof(gah->m_FilePath);
		const size_t chunkFilePathLen = strnlen(gah->m_FilePath, maxFilePathLen);
		m_FilePath = string(gah->m_FilePath, gah->m_FilePath + chunkFilePathLen);

		m_nFlags = gah->m_Flags;
		m_nCompressedControllerCount = gah->m_nControllers;

		m_fSpeed = gah->m_fSpeed;
		m_fDistance = gah->m_fDistance;
		m_fSlope = gah->m_fSlope;
		m_fAssetTurn = gah->m_fAssetTurn;
		m_fTurnSpeed = gah->m_fTurnSpeed;

		m_StartLocation2 = gah->m_StartLocation;
		m_LastLocatorKey2 = gah->m_LastLocatorKey;

		m_fStartSec = 0;
		m_fEndSec = gah->m_fEndSec - gah->m_fStartSec;

		m_FootPlantVectors.m_LHeelStart = gah->m_LHeelStart;
		m_FootPlantVectors.m_LHeelEnd   = gah->m_LHeelEnd;
		m_FootPlantVectors.m_LToe0Start = gah->m_LToe0Start;
		m_FootPlantVectors.m_LToe0End   = gah->m_LToe0End;
		m_FootPlantVectors.m_RHeelStart = gah->m_RHeelStart;
		m_FootPlantVectors.m_RHeelEnd   = gah->m_RHeelEnd;
		m_FootPlantVectors.m_RToe0Start = gah->m_RToe0Start;
		m_FootPlantVectors.m_RToe0End   = gah->m_RToe0End;
		return true;
	}
	else
	{
		RCLogWarning("Unknown GAH_CAF version (0x%08x) in %s", chunk->chunkVersion, logFilename);
		return false;
	}
}

bool GlobalAnimationHeaderCAF::ReadTiming(const IChunkFile::ChunkDesc* pChunkDesc, ECAFLoadMode loadMode, const char* logFilename)
{
	if (loadMode == eCAFLoadCompressedOnly)
	{
		RCLogWarning("Uncompressed timing chunk has been found in a compressed animation: '%s'", logFilename);
		return true;
	}

	switch (pChunkDesc->chunkVersion)
	{

	case TIMING_CHUNK_DESC_0918::VERSION:
		{
			const auto pChunk = static_cast<TIMING_CHUNK_DESC_0918*>(pChunkDesc->data);
			SwapEndian(*pChunk);

			if (pChunk->global_range.end < pChunk->global_range.start)
			{
				RCLogError("Timing chunk with a negative animation duration has been found in animation file: '%s'", logFilename);
				return false;
			}

			const f32 fSecsPerFrame = SECONDS_PER_TICK * TICKS_PER_FRAME;

			m_nStartKey = 0;
			m_nEndKey = pChunk->global_range.end - pChunk->global_range.start;
			m_fStartSec = m_nStartKey * fSecsPerFrame;
			m_fEndSec = (std::max)(m_nEndKey, 1) * fSecsPerFrame;
		}
		return true; 

	case TIMING_CHUNK_DESC_0919::VERSION:
		{
			auto pChunk = static_cast<TIMING_CHUNK_DESC_0919*>(pChunkDesc->data);
			SwapEndian(*pChunk);

			const f32 hardcodedSamplingRate =  float(TICKS_PER_SECOND) / TICKS_PER_FRAME;

			if (pChunk->numberOfSamples <= 0)
			{
				RCLogError("Timing chunk (0x%04x) declares a nonpositive number of samples (%d). File: '%s'", pChunk->VERSION, pChunk->numberOfSamples, logFilename);
				return false;
			}

			if ((std::abs(pChunk->samplesPerSecond - hardcodedSamplingRate) / hardcodedSamplingRate) > std::numeric_limits<float>::epsilon())
			{
				RCLogWarning("Timing chunk (0x%04x) declares a currently unsupported sampling rate (%.2f). Sampling rate will be overridden to %.2f. File: '%s'", pChunk->VERSION, pChunk->samplesPerSecond, hardcodedSamplingRate, logFilename);
			}

			m_nStartKey = 0;
			m_nEndKey = pChunk->numberOfSamples - 1;

			m_fStartSec = 0.0f;
			m_fEndSec = (std::max)(pChunk->numberOfSamples - 1, 1) / hardcodedSamplingRate;
		}
		return true;

	default:
		RCLogWarning("Unknown timing chunk version (0x%04x). File: '%s'", pChunkDesc->chunkVersion, logFilename);
		return false;
	}
}

bool GlobalAnimationHeaderCAF::ReadBoneNameList(IChunkFile::ChunkDesc* const pChunkDesc, ECAFLoadMode loadMode, const char* const logFilename)
{
	if (pChunkDesc->chunkVersion != BONENAMELIST_CHUNK_DESC_0745::VERSION)
	{
		RCLogError("Unknown version of bone name list chunk in animation '%s'", logFilename);
		return false;
	}

	// the chunk contains variable-length packed strings following tightly each other
	BONENAMELIST_CHUNK_DESC_0745* const pNameChunk = (BONENAMELIST_CHUNK_DESC_0745*)(pChunkDesc->data);
	SwapEndian(*pNameChunk, pChunkDesc->bSwapEndian);
	pChunkDesc->bSwapEndian = false;

	const uint32 expectedNameCount = pNameChunk->numEntities;

	// we know how many strings there are there; note that the whole bunch
	// of strings may be followed by padding zeros
	m_arrOriginalControllerNameAndId.resize(expectedNameCount);

	// scan through all the strings (strings are zero-terminated)
	const char* const pNameListEnd = ((const char*)pNameChunk) + pChunkDesc->size;
	const char* pName = (const char*)(pNameChunk + 1);
	uint32 nameCount = 0;
	while (*pName && (pName < pNameListEnd) && (nameCount < expectedNameCount))
	{
		m_arrOriginalControllerNameAndId[nameCount].name = pName;
		pName += strnlen(pName, pNameListEnd - pName) + 1;
		++nameCount;
	}
	if (nameCount < expectedNameCount)
	{
		RCLogError("Inconsistent bone name list chunk: only %d out of %d bone names have been read in animation '%s'", nameCount, expectedNameCount, logFilename);
		return false;
	}

	return true;
}

bool GlobalAnimationHeaderCAF::LoadChunks(IChunkFile *chunkFile, ECAFLoadMode loadMode, int32& lastControllerID, const char* logFilename)
{
	if (!CAF_EnsureLoadModeApplies(chunkFile, loadMode, logFilename) )
	{
		return false;
	}

	int numChunk = chunkFile->NumChunks();
	m_arrController.reserve(numChunk);	

	bool strippedFrames = false;

	for (int i = 0; i < numChunk; ++i)
	{
		IChunkFile::ChunkDesc* const pChunkDesc = chunkFile->GetChunk(i);
		switch (pChunkDesc->chunkType)
		{
		case ChunkType_Controller:
		{
			IController_AutoPtr controller;
			if (CAF_ReadController(pChunkDesc, loadMode, controller, lastControllerID, logFilename, strippedFrames))
			{
				if (controller)
					m_arrController.push_back(controller);
			}
			else
			{
				RCLogError("Unable to read controller: %i (v%04x) in %s", lastControllerID, pChunkDesc->chunkVersion, logFilename);
				return false;
			}
			break;
		}
		case ChunkType_Timing:
			if (!ReadTiming(pChunkDesc, loadMode, logFilename))
			{
				return false;
			}
			break;

		case ChunkType_MotionParameters:
			if(!ReadMotionParameters(pChunkDesc, loadMode, logFilename))
			{
				return false;
			}
			break;
		case ChunkType_GlobalAnimationHeaderCAF:
			if (!ReadGlobalAnimationHeader(pChunkDesc, loadMode, logFilename))
			{
				return false;
			}
			break;
		case ChunkType_Timestamp:
			// ignore timestamp
			break;
		case ChunkType_BoneNameList:
			if (!ReadBoneNameList(pChunkDesc, loadMode, logFilename))
			{
				return false;
			}
			break;
		}
	}

	if (strippedFrames)
	{
		RCLogWarning("Stripping last incomplete frame of animation. Please adjust animation range to be a multiple of the frame duration and re-export animation: %s", logFilename);
	}

	return true;
}

bool GlobalAnimationHeaderCAF::SaveToChunkFile(IChunkFile* chunkFile, bool bigEndianOutput) const
{
	SEndiannessSwapper swap(bigEndianOutput);

	CHUNK_GAHCAF_INFO GAH_Info;
	ZeroStruct(GAH_Info);

	uint32 num = m_FilePath.size();
	if (num > sizeof(GAH_Info.m_FilePath) - 1)
	{
		RCLogError("Truncating animation path to %i characters: %s", sizeof(GAH_Info.m_FilePath) - 1, m_FilePath.c_str());
		num = sizeof(GAH_Info.m_FilePath) - 1;
	}
	memcpy(GAH_Info.m_FilePath, m_FilePath.c_str(), num);
	GAH_Info.m_FilePath[sizeof(GAH_Info.m_FilePath) - 1] = '\0';

	swap(GAH_Info.m_FilePathCRC32 = m_FilePathCRC32);
	swap(GAH_Info.m_FilePathDBACRC32 = m_FilePathDBACRC32);

	swap(GAH_Info.m_Flags	=	m_nFlags);
	swap(GAH_Info.m_nControllers = m_nCompressedControllerCount);

	swap(GAH_Info.m_fSpeed = m_fSpeed);
	assert(m_fDistance >= 0.0f);
	swap(GAH_Info.m_fDistance = m_fDistance);
	swap(GAH_Info.m_fSlope = m_fSlope);

	swap(GAH_Info.m_fAssetTurn = m_fAssetTurn);
	swap(GAH_Info.m_fTurnSpeed = m_fTurnSpeed);

	GAH_Info.m_StartLocation		= m_StartLocation2;
	swap(GAH_Info.m_StartLocation.q.w);
	swap(GAH_Info.m_StartLocation.q.v.x);
	swap(GAH_Info.m_StartLocation.q.v.y);
	swap(GAH_Info.m_StartLocation.q.v.z);
	swap(GAH_Info.m_StartLocation.t.x);
	swap(GAH_Info.m_StartLocation.t.y);
	swap(GAH_Info.m_StartLocation.t.z);

	GAH_Info.m_LastLocatorKey		= m_LastLocatorKey2;
	swap(GAH_Info.m_LastLocatorKey.q.w);
	swap(GAH_Info.m_LastLocatorKey.q.v.x);
	swap(GAH_Info.m_LastLocatorKey.q.v.y);
	swap(GAH_Info.m_LastLocatorKey.q.v.z);
	swap(GAH_Info.m_LastLocatorKey.t.x);
	swap(GAH_Info.m_LastLocatorKey.t.y);
	swap(GAH_Info.m_LastLocatorKey.t.z);

	swap(GAH_Info.m_fStartSec = m_fStartSec); //asset-feature: Start time in seconds.
	swap(GAH_Info.m_fEndSec = m_fEndSec); //asset-feature: End time in seconds.
	swap(GAH_Info.m_fTotalDuration = m_fEndSec - m_fStartSec); //asset-feature: asset-feature: total duration in seconds.

	swap(GAH_Info.m_LHeelStart = m_FootPlantVectors.m_LHeelStart);
	swap(GAH_Info.m_LHeelEnd   = m_FootPlantVectors.m_LHeelEnd);
	swap(GAH_Info.m_LToe0Start = m_FootPlantVectors.m_LToe0Start);
	swap(GAH_Info.m_LToe0End   = m_FootPlantVectors.m_LToe0End);
	swap(GAH_Info.m_RHeelStart = m_FootPlantVectors.m_RHeelStart);
	swap(GAH_Info.m_RHeelEnd   = m_FootPlantVectors.m_RHeelEnd);
	swap(GAH_Info.m_RToe0Start = m_FootPlantVectors.m_RToe0Start);
	swap(GAH_Info.m_RToe0End   = m_FootPlantVectors.m_RToe0End);

	CChunkData chunkData;
	chunkData.Add( GAH_Info );

	chunkFile->AddChunk(
		ChunkType_GlobalAnimationHeaderCAF,
		CHUNK_GAHCAF_INFO::VERSION,
		(bigEndianOutput ? eEndianness_Big : eEndianness_Little),
		chunkData.GetData(), chunkData.GetSize());

	return true;
}

void GlobalAnimationHeaderCAF::ExtractMotionParameters(MotionParams905* mp, bool bigEndianOutput) const
{
	SEndiannessSwapper swap(bigEndianOutput);

	swap(mp->m_nAssetFlags = m_nFlags);
	swap(mp->m_nCompression = m_nCompression);

	swap(mp->m_nTicksPerFrame = TICKS_PER_FRAME);
	swap(mp->m_fSecsPerTick = SECONDS_PER_TICK);
	swap(mp->m_nStart = m_nStartKey);
	swap(mp->m_nEnd = m_nEndKey);

	swap(mp->m_fMoveSpeed = m_fSpeed);
	swap(mp->m_fAssetTurn = m_fAssetTurn);
	swap(mp->m_fTurnSpeed = m_fTurnSpeed);
	swap(mp->m_fDistance = m_fDistance);
	swap(mp->m_fSlope = m_fSlope);


	mp->m_StartLocation = m_StartLocation2;
	swap(mp->m_StartLocation.q.w);
	swap(mp->m_StartLocation.q.v.x);
	swap(mp->m_StartLocation.q.v.y);
	swap(mp->m_StartLocation.q.v.z);
	swap(mp->m_StartLocation.t.x);
	swap(mp->m_StartLocation.t.y);
	swap(mp->m_StartLocation.t.z);

	mp->m_EndLocation = m_LastLocatorKey2;
	swap(mp->m_EndLocation.q.w);
	swap(mp->m_EndLocation.q.v.x);
	swap(mp->m_EndLocation.q.v.y);
	swap(mp->m_EndLocation.q.v.z);
	swap(mp->m_EndLocation.t.x);
	swap(mp->m_EndLocation.t.y);
	swap(mp->m_EndLocation.t.z);

	swap(mp->m_LHeelEnd = m_FootPlantVectors.m_LHeelEnd);
	swap(mp->m_LHeelStart = m_FootPlantVectors.m_LHeelStart);
	swap(mp->m_LToe0Start = m_FootPlantVectors.m_LToe0Start);
	swap(mp->m_LToe0End = m_FootPlantVectors.m_LToe0End);

	swap(mp->m_RHeelEnd = m_FootPlantVectors.m_RHeelEnd);
	swap(mp->m_RHeelStart = m_FootPlantVectors.m_RHeelStart);
	swap(mp->m_RToe0Start = m_FootPlantVectors.m_RToe0Start);
	swap(mp->m_RToe0End = m_FootPlantVectors.m_RToe0End);
}

const char* GlobalAnimationHeaderCAF::GetOriginalControllerName(uint32 controllerId) const
{
	for (size_t i = 0; i < m_arrOriginalControllerNameAndId.size(); ++i)
	{
		if (m_arrOriginalControllerNameAndId[i].id == controllerId)
		{
			return m_arrOriginalControllerNameAndId[i].name.c_str();
		}
	}
	return "<unknown>";
}

void GlobalAnimationHeaderCAF::BindControllerIdsToControllerNames()
{
	const size_t numOriginalControllerNames = m_arrOriginalControllerNameAndId.size();

	if (!numOriginalControllerNames)
		return; // there was no bone name list (or an empty one)

	if (m_arrController.size() != numOriginalControllerNames) // if there is a bone name list, there should be an entry for every controller
	{
		RCLogWarning("Size of bone name table (%d) does not match number of controllers (%d). Ignoring bone name table.", (int)m_arrOriginalControllerNameAndId.size(), (int)m_arrController.size());
		return;
	}

	for (size_t i = 0; i < numOriginalControllerNames; ++i)
	{
		m_arrOriginalControllerNameAndId[i].id = m_arrController[i]->GetID();
	}
}
