// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "LoaderDBA.h"

#include <CryString/StringUtils.h>
#include <CryString/CryPath.h>
#include "ControllerPQ.h"
#include "ControllerOpt.h"
#include <Cry3DEngine/CGF/CGFContent.h>
#include "CharacterManager.h"

void CGlobalHeaderDBA::CreateDatabaseDBA(const char* filename)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_DBA, 0, filename);
	stack_string strPath = filename;
	PathUtil::UnifyFilePath(strPath);
	m_strFilePathDBA = strPath.c_str();
	m_FilePathDBACRC32 = CCrc32::Compute(strPath.c_str());
	m_nUsedAnimations = 0;
}

void CGlobalHeaderDBA::LoadDatabaseDBA(const char* sForCharacter)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_DBA, 0, m_strFilePathDBA.c_str());
	m_nLastUsedTimeDelta = 0;
	if (m_pStream || m_pDatabaseInfo)
		return;

	_smart_ptr<IChunkFile> pChunkFile = g_pI3DEngine->CreateChunkFile(true);
	if (!pChunkFile->Read(m_strFilePathDBA))
	{
		ReportLoadError(sForCharacter, pChunkFile->GetLastError());
		return;
	}

	// Load mesh from chunk file.

	SAFE_DELETE(m_pDatabaseInfo);
	m_pDatabaseInfo = new CInternalDatabaseInfo(m_FilePathDBACRC32, m_strFilePathDBA);

	if (!m_pDatabaseInfo->LoadChunks(pChunkFile, false))
	{
		delete m_pDatabaseInfo;
		m_pDatabaseInfo = 0;
		ReportLoadError(sForCharacter, "Failed to load chunks");
	}
}

//////////////////////////////////////////////////////////////////////////
bool CGlobalHeaderDBA::StartStreamingDBA(bool highPriority)
{
	//	LoadDatabaseDBA(0);
	//	return true;

	if (m_pStream || m_pDatabaseInfo)
		return true;

	CInternalDatabaseInfo* pStreamInfo = new CInternalDatabaseInfo(m_FilePathDBACRC32, m_strFilePathDBA);

	// start streaming
	StreamReadParams params;
	params.dwUserData = 0;
	params.ePriority = highPriority ? estpUrgent : estpNormal;
	params.nSize = 0;
	params.pBuffer = NULL;
	params.nLoadTime = 10000;
	params.nMaxLoadTime = 1000;

	m_pStream = g_pISystem->GetStreamEngine()->StartRead(eStreamTaskTypeAnimation, m_strFilePathDBA, pStreamInfo, &params);

	return true;
}

void CGlobalHeaderDBA::CompleteStreamingDBA(CInternalDatabaseInfo* pInfo)
{
	SAFE_DELETE(m_pDatabaseInfo);
	m_pDatabaseInfo = pInfo;

	if (pInfo->m_bLoadFailed)
	{
		ReportLoadError(m_strFilePathDBA.c_str(), pInfo->m_strLastError.c_str());
	}

	if (g_pCharacterManager->m_pStreamingListener)
	{
		uint32 numHeadersCAF = g_AnimationManager.m_arrGlobalCAF.size();
		for (uint32 i = 0; i < numHeadersCAF; i++)
		{
			GlobalAnimationHeaderCAF& rGAH = g_AnimationManager.m_arrGlobalCAF[i];
			if (rGAH.m_FilePathDBACRC32 == m_FilePathDBACRC32)
			{
				int32 globalID = g_AnimationManager.GetGlobalIDbyFilePath_CAF(rGAH.GetFilePath());
				g_pCharacterManager->m_pStreamingListener->NotifyAnimLoaded(globalID);
			}
		}
	}

	m_pStream.reset();
}

CInternalDatabaseInfo::CInternalDatabaseInfo(uint32 filePathCRC, const string& strFilePath)
	: m_FilePathCRC(filePathCRC)
	, m_strFilePath(strFilePath)
	, m_bLoadFailed(false)
	, m_nRelocatableCAFs(0)
	, m_iTotalControllers(0)
	, m_nStorageLength(0)
	, m_pControllersInplace(nullptr)
{
}

CInternalDatabaseInfo::~CInternalDatabaseInfo()
{
	m_pControllersInplace = 0;
	m_nRelocatableCAFs = 0;

	if (m_hStorage.IsValid())
	{
		g_controllerHeap.Free(m_hStorage);
		m_hStorage = CControllerDefragHdl();
	}
}

void CInternalDatabaseInfo::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(m_pControllersInplace, sizeof(CControllerOptNonVirtual) * m_iTotalControllers);
}

void CInternalDatabaseInfo::StreamOnComplete(IReadStream* pStream, unsigned nError)
{
	CGlobalHeaderDBA* pDBA = g_AnimationManager.FindGlobalHeaderByCRC_DBA(m_FilePathCRC);

	if (pDBA)
	{
		pDBA->CompleteStreamingDBA(this);
	}
	else
	{
		delete this;
	}
}

void* CInternalDatabaseInfo::StreamOnNeedStorage(IReadStream* pStream, unsigned nSize, bool& bAbortOnFailToAlloc)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_DBA, 0, m_strFilePath.c_str());

	char* pData = NULL;

	if (Console::GetInst().ca_StreamDBAInPlace)
	{
		m_hStorage = g_controllerHeap.AllocPinned(nSize, this);
		if (m_hStorage.IsValid())
			pData = (char*)g_controllerHeap.WeakPin(m_hStorage);
	}

	return pData;
}

void CInternalDatabaseInfo::StreamAsyncOnComplete(IReadStream* pStream, unsigned nError)
{
	DEFINE_PROFILER_FUNCTION();
	//LOADING_TIME_PROFILE_SECTION(g_pISystem);
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_DBA, 0, m_strFilePath.c_str());

	if (pStream->IsError())
	{
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_strFilePath.c_str(), "Failed to stream DBA-file %x", nError);
		return;
	}

	//--------------------------------------------------------------------------

	const char* buffer = (char*)pStream->GetBuffer();
	size_t size = pStream->GetBytesRead();
	_smart_ptr<IChunkFile> pChunkFile = g_pI3DEngine->CreateChunkFile(true);
	if (!pChunkFile->ReadFromMemory(buffer, size))
	{
		m_bLoadFailed = true;
		m_strLastError = pChunkFile->GetLastError();
		return;
	}

	if (!LoadChunks(pChunkFile, true))
	{
		if (!m_bLoadFailed)
		{
			m_bLoadFailed = true;
			m_strLastError = "Failed to load chunks";
		}
	}

#ifndef _RELEASE
	if (Console::GetInst().ca_DebugAnimUsageOnFileAccess)
		g_AnimationManager.DebugAnimUsage(0);
#endif

	pStream->FreeTemporaryMemory();
}

void CGlobalHeaderDBA::DeleteDatabaseDBA()
{
	if (m_pStream)
	{
		m_pStream->Abort();
		m_pStream.reset();
	}

	if (!m_pDatabaseInfo)
		return;

	if (m_nUsedAnimations)
		return;

	// make sure that the DBA is GONE from the outside first
	// so it cannot be accessed from another thread while this is
	// running. Also we have to make sure to communicate that we are currently in the process of deleting
	// this DBA.
	CInternalDatabaseInfo* dbInfo = m_pDatabaseInfo;
	m_pDatabaseInfo = NULL;

	uint32 numHeadersCAF = g_AnimationManager.m_arrGlobalCAF.size();
	for (uint32 i = 0; i < numHeadersCAF; i++)
	{
		GlobalAnimationHeaderCAF& rGAH = g_AnimationManager.m_arrGlobalCAF[i];
		if (rGAH.m_FilePathDBACRC32 != m_FilePathDBACRC32)
			continue;
		rGAH.m_nControllers = 0;
		rGAH.m_arrController.clear();
		rGAH.ClearControllerData();
		rGAH.m_arrControllerLookupVector.clear();
	}

	// now we can really delete it, since the data is no longer referenced through
	// CAF headers
	if (dbInfo)
		delete dbInfo;

	if (g_pCharacterManager->m_pStreamingListener)
	{
		for (uint32 i = 0; i < numHeadersCAF; i++)
		{
			GlobalAnimationHeaderCAF& rGAH = g_AnimationManager.m_arrGlobalCAF[i];
			if (rGAH.m_FilePathDBACRC32 == m_FilePathDBACRC32)
			{
				int32 globalID = g_AnimationManager.GetGlobalIDbyFilePath_CAF(rGAH.GetFilePath());
				g_pCharacterManager->m_pStreamingListener->NotifyAnimUnloaded(globalID);
			}
		}
	}
#ifndef _RELEASE
	if (Console::GetInst().ca_DebugAnimUsageOnFileAccess)
		g_AnimationManager.DebugAnimUsage(0);
#endif
}

//--------------------------------------------------------------------------------

bool CGlobalHeaderDBA::InMemory()
{
	return (m_pDatabaseInfo != 0);
}

//--------------------------------------------------------------------------------

bool CInternalDatabaseInfo::LoadChunks(IChunkFile* pChunkFile, bool bStreaming)
{
	uint32 numChunck = pChunkFile->NumChunks();
	for (uint32 i = 0; i < numChunck; i++)
	{
		IChunkFile::ChunkDesc* const pChunkDesc = pChunkFile->GetChunk(i);
		switch (pChunkDesc->chunkType)
		{

		case ChunkType_Controller:
			if (!ReadControllers(pChunkDesc, bStreaming))
				return false;
			break;
		}
	}

	return true;
}

//--------------------------------------------------------------------------------------

bool CInternalDatabaseInfo::ReadControllers(IChunkFile::ChunkDesc* pChunkDesc, bool bStreaming)
{
	if (pChunkDesc->chunkVersion == CONTROLLER_CHUNK_DESC_0905::VERSION)
		return ReadController905(pChunkDesc, bStreaming);
	CryFatalError("CryAnimation: version 0x%03x of controllers is not supported. Use DBAs generated with latest RC", (int)pChunkDesc->chunkVersion);
	return true;
}

bool CInternalDatabaseInfo::ReadController905(IChunkFile::ChunkDesc* pChunkDesc, bool bStreaming)
{
	LOADING_TIME_PROFILE_SECTION(g_pISystem);

	if (pChunkDesc->bSwapEndian)
		CryFatalError("%s: data are stored in non-native endian format", __FUNCTION__);

	CONTROLLER_CHUNK_DESC_0905* pCtrlChunk = (CONTROLLER_CHUNK_DESC_0905*)pChunkDesc->data;
	char* pCtrlChunkEnd = reinterpret_cast<char*>(pChunkDesc->data) + pChunkDesc->size;

	m_iTotalControllers = 0;

	uint64 startedStatistics = 0;
	if (Console::GetInst().ca_MemoryUsageLog)
	{
		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		startedStatistics = info.allocated - info.freed;
		g_pILog->UpdateLoadingScreen("ReadController905. Init. Memstat %li", (long int)(info.allocated - info.freed));
	}

	if (Console::GetInst().ca_MemoryUsageLog)
	{
		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		g_pILog->UpdateLoadingScreen("ReadController905. TracksResize. Memstat %li", (long int)(info.allocated - info.freed));
	}

	char* pData = (char*)(pCtrlChunk + 1);
	char* pData2 = pData;

	uint32 totalAnims = pCtrlChunk->numAnims;
	uint32 keyTime = pCtrlChunk->numKeyTime;
	uint32 keyPos = pCtrlChunk->numKeyPos;
	uint32 keyRot = pCtrlChunk->numKeyRot;

	// Keytimes pointers
	std::vector<uint16> pktSizes((uint16*)pData, (uint16*)pData + keyTime);
	pData += pktSizes.size() * sizeof(uint16);
	std::vector<uint32> pktFormats((uint32*)pData, (uint32*)pData + (eBitset + 1));
	pData += pktFormats.size() * sizeof(uint32);

	// Positions pointers
	std::vector<uint16> ppSizes((uint16*)pData, (uint16*)pData + keyPos);
	pData += keyPos * sizeof(uint16);
	std::vector<uint32> ppFormats((uint32*)pData, (uint32*)pData + eAutomaticQuat);
	pData += eAutomaticQuat * sizeof(uint32);

	//Rotations pointers
	std::vector<uint16> prSizes((uint16*)pData, (uint16*)pData + keyRot);
	pData += keyRot * sizeof(uint16);
	std::vector<uint32> prFormats((uint32*)pData, (uint32*)pData + eAutomaticQuat);
	pData += eAutomaticQuat * sizeof(uint32);

	std::vector<int32> ktOffsets((int32*)pData, (int32*)pData + keyTime);
	pData += keyTime * sizeof(int32);

	std::vector<int32> pOffsets((int32*)pData, (int32*)pData + keyPos);
	pData += keyPos * sizeof(int32);

	std::vector<int32> rOffsets((int32*)pData, (int32*)pData + (keyRot + 1));
	pData += (keyRot + 1) * sizeof(int32);

	// Determine if this appears to be an in-place streamable file. If it is, the offsets will be negative.
	bool bIsInPlacePrepared = rOffsets[keyRot] < 0;

	uint32 nPaddingLength = 0;
	if (bIsInPlacePrepared)
	{
		memcpy(&nPaddingLength, pData, sizeof(nPaddingLength));
		pData += sizeof(nPaddingLength);
	}

	ptrdiff_t diff = pData - pData2;
	pData += diff % 4 > 0 ? 4 - diff % 4 : 0;

	uint32 nTrackLen = Align(abs(rOffsets[keyRot]), 4);

	if (nTrackLen == 0)
	{
		if (!m_bLoadFailed)
		{
			m_bLoadFailed = true;
			m_strLastError = "Empty controller found";
		}
		return false;
	}

	uint32 rCountSize = eAutomaticQuat + 1;
	std::vector<uint32> rCount(rCountSize);
	for (uint32 i = 1; i < rCountSize; ++i)
	{
		rCount[i] += rCount[i - 1] + prFormats[i - 1];
	}

	uint32 pCountSize = eAutomaticQuat + 1;
	std::vector<uint32> pCount(pCountSize);
	for (uint32 i = 1; i < pCountSize; ++i)
	{
		pCount[i] += pCount[i - 1] + ppFormats[i - 1];
	}

	uint32 ktCountSize = eBitset + 2;
	std::vector<uint32> ktCount(ktCountSize);
	for (uint32 i = 1; i < ktCountSize; ++i)
	{
		ktCount[i] += ktCount[i - 1] + pktFormats[i - 1];
	}

	uint32 curFormat = 0;

	while (curFormat + 1 < ktCountSize && ktCount[curFormat + 1] == 0)
		++curFormat;
	uint32 curSizeof = ControllerHelper::GetKeyTimesFormatSizeOf(curFormat);

	for (uint32 i = 0; i < keyTime; ++i)
	{
		while (i >= ktCount[curFormat + 1])
		{
			++curFormat;
			curSizeof = ControllerHelper::GetKeyTimesFormatSizeOf(curFormat);
		}
	}

	curFormat = 0;
	while (curFormat + 1 < pCountSize && pCount[curFormat + 1] == 0)
		++curFormat;

	curSizeof = ControllerHelper::GetPositionsFormatSizeOf(curFormat);

	for (uint32 i = 0; i < keyPos; ++i)
	{
		while (i >= pCount[curFormat + 1])
		{
			++curFormat;
			curSizeof = ControllerHelper::GetPositionsFormatSizeOf(curFormat);
		}

	}

	curFormat = 0;
	while (curFormat + 1 < rCountSize && rCount[curFormat + 1] == 0)
		++curFormat;

	curSizeof = ControllerHelper::GetRotationFormatSizeOf(curFormat);

	for (uint32 i = 0; i < keyRot; ++i)
	{
		while (i >= rCount[curFormat + 1])
		{
			++curFormat;
			curSizeof = ControllerHelper::GetRotationFormatSizeOf(curFormat);
		}
	}

	const char* pStorageData = NULL;

	if (!bIsInPlacePrepared)
	{
		// Old assets have the tracks in the middle of the file
		pStorageData = pData;
		pData += nTrackLen;
	}
	else
	{
		pStorageData = pCtrlChunkEnd - nTrackLen;
	}

	// set pointers to keytimes, positions, rotations
	if (Console::GetInst().ca_MemoryUsageLog)
	{
		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		g_pILog->UpdateLoadingScreen("ReadController905. All dynamic data resize. Memstat %li", (long int)(info.allocated - info.freed));
	}

	std::vector<CControllerInfo*> controllersOffsets(totalAnims, 0);

	uint32 nDBACRC = CCrc32::Compute(m_strFilePath.c_str());

	uint32 nRelocatableCAFs = 0;

	pData += nPaddingLength;

	char* pAnimBlockStart = pData;

	std::vector<uint16> controllerCount(totalAnims, 0);

	std::vector<uint16> arrGlobalAnimID(totalAnims, (uint16) - 1);
	for (uint32 i = 0; i < totalAnims; ++i)
	{

		uint16 strSize;// = pData;
		memcpy(&strSize, pData, sizeof(uint16));
		pData += sizeof(uint16);

#define BIG_STRING 1024

		if (strSize > BIG_STRING)
		{
			assert(0);
			return false;
		}
		char tmp[BIG_STRING];

		memset(tmp, 0, BIG_STRING);
		memcpy(tmp, pData, strSize);

#undef BIG_STRING

		pData += strSize;

		int nGlobalAnimID = -1;

		if (bStreaming)
		{
			nGlobalAnimID = g_AnimationManager.GetGlobalIDbyFilePath_CAF(tmp);
			if (nGlobalAnimID < 0)
				CryLog("Going to create a CAF header on streaming thread - possible race condition!");
		}

		if (nGlobalAnimID < 0)
		{
			nGlobalAnimID = g_AnimationManager.CreateGAH_CAF(tmp);
			if (nGlobalAnimID < 0)
				CryFatalError("GAH does not exist");
		}

		arrGlobalAnimID[i] = nGlobalAnimID;

		GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[nGlobalAnimID];
		if (rCAF.m_FilePathDBACRC32 == 0 && Console::GetInst().ca_UseIMG_CAF)
			CryFatalError("DBA/CAF mismatch in %s", rCAF.GetFilePath());

		MotionParams905 info;
		memcpy(&info, pData, sizeof(info));
		pData += sizeof(info);

		if (rCAF.m_FilePathDBACRC32)
		{
			rCAF.SetFlags(info.m_nAssetFlags);

			rCAF.OnAssetCreated();

			rCAF.m_FilePathDBACRC32 = nDBACRC;
			//	m_pDatabaseInfo->m_SkinningInfo[i]->m_nCompression		= info.m_nCompression;
			int32 nStartKey = info.m_nStart;
			int32 nEndKey = info.m_nEnd;
			if (rCAF.GetFlags() & CA_ASSET_ADDITIVE)
				nStartKey++;
			rCAF.m_fStartSec = nStartKey / ANIMATION_30Hz;
			rCAF.m_fEndSec = nEndKey / ANIMATION_30Hz;
			if (rCAF.m_fEndSec <= rCAF.m_fStartSec)
				rCAF.m_fEndSec = rCAF.m_fStartSec;
			rCAF.m_fTotalDuration = rCAF.m_fEndSec - rCAF.m_fStartSec;
			rCAF.m_StartLocation = info.m_StartLocation;

			++nRelocatableCAFs;
		}

		// footplants
		uint16 footplans;// = m_arrAnimations[i].m_FootPlantBits.size();
		memcpy(&footplans, pData, sizeof(uint16));
		pData += sizeof(uint16);
		pData += footplans;

		//
		uint16 controllerInfo;// = m_arrAnimations[i].m_arrControlerInfo.size();
		memcpy(&controllerInfo, pData, sizeof(uint16));
		pData += sizeof(uint16);

		int32 offsetToControllerInfos = static_cast<int32>(pData - pAnimBlockStart);
		if (bIsInPlacePrepared)
		{
			memcpy(&offsetToControllerInfos, pData, sizeof(int32));
			pData += sizeof(int32);
		}

		if (controllerInfo == 0)
			continue;

		CControllerInfo* pController = (CControllerInfo*)(pAnimBlockStart + offsetToControllerInfos);
		controllersOffsets[i] = pController;

		if (!bIsInPlacePrepared)
		{
			pData += controllerInfo * sizeof(CControllerInfo);
		}

		if (rCAF.m_FilePathDBACRC32)
		{
			rCAF.m_arrController.resize(controllerInfo);
		}

		//		rCAF.m_bEmpty=uint8(controllerInfo);
		rCAF.m_bEmpty = (controllerInfo == 0) ? 1 : 0;
		controllerCount[i] = controllerInfo;

		m_iTotalControllers += controllerInfo;
	}

	uint32 nCAFListSize = Align(nRelocatableCAFs * sizeof(uint16), 16);
	uint32 nAlignedStorageSize = Align(nTrackLen, 16);
	uint32 nAlignedControllerSize = Align(sizeof(CControllerOptNonVirtual) * m_iTotalControllers, 16);
	uint32 nTotalSize = nCAFListSize + nAlignedControllerSize + nAlignedStorageSize;

	CControllerDefragHdl hStorage;

	// If we're currently attempting to stream in place, the file is prepared for in place, and
	// there's room to install the controllers, then don't allocate. Otherwise do.

	bool bIsInPlace = false;

	if (m_hStorage.IsValid())
	{
		char* pStorageBase = reinterpret_cast<char*>(pCtrlChunk);
		if (bIsInPlacePrepared)
		{
			if ((pStorageBase + nCAFListSize + nAlignedControllerSize) <= (pCtrlChunkEnd - nTrackLen))
			{
				hStorage = m_hStorage;
				bIsInPlace = true;
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, "Failed to stream DBA '%s' inplace - has sizeof(CControllerOptNonVirtual) changed?", m_strFilePath.c_str());
			}
		}
	}

	if (!hStorage.IsValid())
	{
		hStorage = g_controllerHeap.AllocPinned(nTotalSize, this);
	}

	if (!hStorage.IsValid())
	{
		return false;
	}

	char* pAllocData = (char*)g_controllerHeap.WeakPin(hStorage);
	uint16* pCAFList = (uint16*)pAllocData;
	char* pControllers = pAllocData + nCAFListSize;

	char* pStorage = NULL;
	if (bIsInPlace)
	{
		// offsets are negative, relative to the chunk end
		pStorage = pCtrlChunkEnd;
	}
	else
	{
		pStorage = pControllers + nAlignedControllerSize;
		memcpy(pStorage, pStorageData, nTrackLen);

		if (bIsInPlacePrepared)
		{
			// Offsets are relative to the end of the storage block
			pStorage += nTrackLen;
		}
	}

	m_pControllersInplace = reinterpret_cast<CControllerOptNonVirtual*>(pControllers);

	uint32 controllersCount = 0, nRelocateCAFIdx = 0;
	for (uint32 i = 0; i < totalAnims; ++i)
	{
		uint16 nGlobalAnimID = arrGlobalAnimID[i];
		if (nGlobalAnimID == 0xffff)
			CryFatalError("Invalid GAH ID");
		GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[nGlobalAnimID];
		//		uint32 numController = rCAF.m_bEmpty;
		uint32 numController = controllerCount[i];

		CControllerInfo* pController = controllersOffsets[i];
		if (pController)
		{
			for (uint32 t = 0; t < numController; ++t)
			{
				// Must load all pController members, as controller construction may trample it
				uint32 p = const_cast<volatile uint32&>(pController[t].m_nPosTrack);
				uint32 r = const_cast<volatile uint32&>(pController[t].m_nRotTrack);
				uint32 pk = const_cast<volatile uint32&>(pController[t].m_nPosKeyTimeTrack);
				uint32 rk = const_cast<volatile uint32&>(pController[t].m_nRotKeyTimeTrack);
				uint32 id = const_cast<volatile uint32&>(pController[t].m_nControllerID);

				uint32 rotkt = (rk == ~0) ? eNoFormat : FindFormat(rk, ktCount);
				uint32 rotk = (r == ~0) ? eNoFormat : FindFormat(r, rCount);
				uint32 poskt = (pk == ~0) ? eNoFormat : FindFormat(pk, ktCount);
				uint32 posk = (p == ~0) ? eNoFormat : FindFormat(p, pCount);

				CControllerOptNonVirtual* pNewController = new(&m_pControllersInplace[controllersCount++])CControllerOptNonVirtual;
				pNewController->Init(rotk, rotkt, posk, poskt);

				if (r != ~0 && rk != ~0)
				{
					char* pRKT = &pStorage[ktOffsets[rk]];
					char* pRK = &pStorage[rOffsets[r]];

					pNewController->SetRotationKeyTimes(pktSizes[rk], pRKT);
					pNewController->SetRotationKeys(prSizes[r], pRK);
				}

				if (p != ~0 && pk != ~0)
				{
					char* pPKT = &pStorage[ktOffsets[pk]];
					char* pPK = &pStorage[pOffsets[p]];

					pNewController->SetPositionKeyTimes(pktSizes[pk], pPKT);
					pNewController->SetPositionKeys(ppSizes[p], pPK);
				}

				pNewController->m_nControllerId = id;

				if (rCAF.m_FilePathDBACRC32)
					rCAF.m_arrController[t] = pNewController;
			}
		}

		if (rCAF.m_FilePathDBACRC32)
		{
			pCAFList[nRelocateCAFIdx++] = nGlobalAnimID;
			std::sort(rCAF.m_arrController.begin(), rCAF.m_arrController.end(), AnimCtrlSortPred());
			rCAF.InitControllerLookup(numController);
			//		rCAF.m_FilePathDBACRC32 = nDBACRC32;
			rCAF.OnAssetCreated();
			rCAF.ClearAssetRequested();
			rCAF.ClearAssetNotFound();
			rCAF.m_nControllers = numController;
			rCAF.m_nControllers2 = numController;
		}
	}

	if (Console::GetInst().ca_MemoryUsageLog)
	{
		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		g_pILog->UpdateLoadingScreen("ReadController903. Finished. Memstat %li", (long int)(info.allocated - info.freed));
		uint64 endStatistics = info.allocated - info.freed;
		g_AnimStatisticsInfo.m_iDBASizes += endStatistics - startedStatistics;
		g_pILog->UpdateLoadingScreen("Current DBA. Memstat %li", (long int)(endStatistics - startedStatistics));
		g_pILog->UpdateLoadingScreen("ALL DBAs %li", (long int)g_AnimStatisticsInfo.m_iDBASizes);
	}

	m_nRelocatableCAFs = nRelocatableCAFs;

	// Couldn't stream in place for some reason :(. Free the temporary alloc.
	if (m_hStorage.IsValid() && m_hStorage != hStorage)
	{
		g_controllerHeap.Free(m_hStorage);
	}

	m_hStorage = hStorage;
	m_nStorageLength = g_controllerHeap.UsableSize(hStorage);

	g_controllerHeap.Unpin(hStorage);

	return true;
}

void CInternalDatabaseInfo::Relocate(char* pDst, char* pSrc)
{
	uint16* pCAFList = reinterpret_cast<uint16*>(pDst);

	size_t nStorageLength = m_nStorageLength;

	for (uint32 i = 0; i < m_nRelocatableCAFs; ++i)
	{
		GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[pCAFList[i]];

		for (int ci = 0, cc = rCAF.m_arrController.size(); ci != cc; ++ci)
		{
			IController* pSrcCont = &*rCAF.m_arrController[ci];
			size_t offs = static_cast<size_t>((char*)pSrcCont - pSrc);

			if (offs < nStorageLength)
				rCAF.m_arrController[ci] = reinterpret_cast<IController*>(pDst + offs);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CGlobalHeaderDBA::ReportLoadError(const char* sForCharacter, const char* sReason)
{
	if (!m_bLoadFailed)
	{
		m_strLastError = sReason;
		// Report loading error only once.
		m_bLoadFailed = true;
		gEnv->pLog->LogError("Failed To Load Animation DBA '%s' for model '%s'.  Reason: %s", m_strFilePathDBA.c_str(), sForCharacter, sReason);
	}
}

const size_t CGlobalHeaderDBA::SizeOf_DBA() const
{
	size_t nSize = sizeof(CGlobalHeaderDBA);
	nSize += m_strFilePathDBA.capacity();
	nSize += m_strLastError.capacity();
	if (m_pDatabaseInfo == 0)
		return nSize;

	if (m_pDatabaseInfo->m_hStorage.IsValid())
		nSize += g_controllerHeap.UsableSize(m_pDatabaseInfo->m_hStorage);

	return nSize;
}

void CGlobalHeaderDBA::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(m_pDatabaseInfo);
	pSizer->AddObject(m_strLastError);
}

CGlobalHeaderDBA::CGlobalHeaderDBA()
{
	m_pDatabaseInfo = 0;
	m_FilePathDBACRC32 = 0;
	m_nUsedAnimations = 0;
	m_nLastUsedTimeDelta = 0;
	m_bDBALock = 0;
	m_bLoadFailed = false;
	m_nEmpty = 0;
	m_pStream = 0;
}

CGlobalHeaderDBA::~CGlobalHeaderDBA()
{
	if (m_pStream)
	{
		m_pStream->Abort();
		m_pStream.reset();
	}

	if (m_pDatabaseInfo)
		delete m_pDatabaseInfo;
}
