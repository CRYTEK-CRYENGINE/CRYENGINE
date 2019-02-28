// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "TrackStorage.h"
#include "AnimationCompiler.h"
#include <Cry3DEngine/CGF/CGFContent.h>
#include "FileUtil.h"

#include "../../../../CryEngine/CryAnimation/ControllerOpt.h"

CTrackStorage::CTrackStorage(bool bBigEndianOutput)
: m_bBigEndianOutput(bBigEndianOutput)
{
}

CTrackStorage::~CTrackStorage(void)
{
}

uint32 CTrackStorage::FindAnimation(const string& name)
{
	ThreadUtils::AutoLock lock(m_lock);
	const size_t count = m_arrAnimNames.size();
	const char * pName = name.c_str();
	for (size_t i = 0; i < count; ++i)
	{
		if (!stricmp(m_arrAnimNames[i].c_str(), pName))
		{
			return i;
		}
	}

	return -1;
}


uint32 CTrackStorage::FindOrAddAnimationHeader(const GlobalAnimationHeaderCAF& header, const string& name)
{
	if (name.find("//") != string::npos)
	{
		RCLogError("Internal error: malformed animation file path: %s", name.c_str());
	}

	ThreadUtils::AutoLock lock(m_lock);
	// add new animation;
	const uint32 num = FindAnimation(name);
	if (num == -1)
	{
		m_arrGlobalAnimationHeaderCAF.push_back(header);
		m_arrAnimNames.push_back(name);
		return uint32(m_arrGlobalAnimationHeaderCAF.size() - 1);
	}

	return num;
}

void CTrackStorage::AddAnimation(const GlobalAnimationHeaderCAF& header)
{
	ThreadUtils::AutoLock lock(m_lock);

	const uint32 index = FindOrAddAnimationHeader(header, header.m_FilePath);

	GlobalAnimationHeaderCAF &addedHeader = m_arrGlobalAnimationHeaderCAF[index];

	addedHeader.SetFilePathCAF(m_arrAnimNames[index]);
	addedHeader.ClearAssetLoaded();

	const size_t numControllers = addedHeader.m_arrController.size();
	for (size_t c = 0; c < numControllers; ++c)
	{
		CController * pController = dynamic_cast<CController *>(addedHeader.m_arrController[c].get());
		if (!pController)
		{
			continue;
		}

		CControllerInfo info;

		info.m_nControllerID = pController->GetID();

		bool hasData = false;
		if (pController->GetPositionController() &&
				pController->GetPositionController()->GetNumKeys() != 0)
		{
			if (pController->GetPositionController()->GetNumKeys())
			{
				hasData = true;
			}
			// Check identical
			KeyTimesInformationPtr pKeyTimes = pController->GetPositionController()->GetKeyTimesInformation();

			pController->m_nPositionKeyTimesTrackId = FindKeyTimesTrack(pKeyTimes);

			if (pController->m_nPositionKeyTimesTrackId == -1)
			{
				// add new track
				m_arrKeyTimes.push_back(pKeyTimes);
				pController->m_nPositionKeyTimesTrackId = m_arrKeyTimes.size() - 1;
				int nkeys = pKeyTimes->GetNumKeys();
				int ptime = pController->m_nPositionKeyTimesTrackId;
				m_arrKeyTimesRemap.insert(std::make_pair(nkeys, ptime));
			}


			PositionControllerPtr pKeys = pController->GetPositionController();

			pController->m_nPositionTrackId = FindPositionTrack(pKeys);

			if (pController->m_nPositionTrackId == -1)
			{
				m_arrPositionTracks.push_back(pKeys->GetPositionStorage());
				pController->m_nPositionTrackId = m_arrPositionTracks.size() - 1;
				int nkeys = pKeyTimes->GetNumKeys();
				int ptime = pController->m_nPositionTrackId;
				m_arrKeyPosRemap.insert(std::make_pair(nkeys, ptime));
			}

			info.m_nPosKeyTimeTrack = pController->m_nPositionKeyTimesTrackId;
			info.m_nPosTrack = pController->m_nPositionTrackId;
		}

		if (pController->GetRotationController() &&
				pController->GetRotationController()->GetNumKeys() != 0)
		{
			if (pController->GetRotationController()->GetNumKeys())
			{
				hasData = true;
			}

			KeyTimesInformationPtr pKeyTimes = pController->GetRotationController()->GetKeyTimesInformation();

			pController->m_nRotationKeyTimesTrackId = FindKeyTimesTrack(pKeyTimes);

			if (pController->m_nRotationKeyTimesTrackId == -1)
			{
				// add new track
				m_arrKeyTimes.push_back(pKeyTimes);
				pController->m_nRotationKeyTimesTrackId = m_arrKeyTimes.size() - 1;
				int nkeys = pKeyTimes->GetNumKeys();
				int ptime = pController->m_nRotationKeyTimesTrackId;
				m_arrKeyTimesRemap.insert(std::make_pair(nkeys, ptime));
			}


			RotationControllerPtr pKeys = pController->GetRotationController();

			pController->m_nRotationTrackId = FindRotationTrack(pKeys);

			if (pController->m_nRotationTrackId == -1)
			{
				m_arrRotationTracks.push_back(pKeys->GetRotationStorage());
				pController->m_nRotationTrackId = m_arrRotationTracks.size() - 1;
				int nkeys = pKeyTimes->GetNumKeys();
				int ptime = pController->m_nRotationTrackId;
				m_arrKeyRotRemap.insert(std::make_pair(nkeys, ptime));
			}

			info.m_nRotKeyTimeTrack = pController->m_nRotationKeyTimesTrackId;
			info.m_nRotTrack = pController->m_nRotationTrackId;
		}

		if (hasData)
		{
			addedHeader.m_arrControllerInfo.push_back(info);
		}
	}

}

bool CTrackStorage::IsRotationIdentical(const RotationControllerPtr& track1, const TrackRotationStoragePtr& track2)
{
	bool bShared(false);

	if (track1->GetNumKeys() == track2->GetNumKeys())
	{
		uint32 ticks = track1->GetNumKeys();

		if (ticks > 0)
		{
			bShared = true;
		}

		for (uint32 t = 0; t < ticks; ++t)
		{
			const Quat pos0 = track1->GetValueFromKey(t);
			const Quat pos1 = track2->GetValue(t);
			if (!IsQuatEquivalent_dot(pos0, pos1, 0.0000001f))
			{
				bShared = false;
				break;
			}
		}
	}

	return bShared;
}

bool CTrackStorage::IsPositionIdentical(const PositionControllerPtr& track1, const TrackPositionStoragePtr& track2)
{
	bool bShared(false);

	if (track1->GetNumKeys() == track2->GetNumKeys())
	{
		// compare position tracks
		uint32 ticks = track1->GetNumKeys();

		if (ticks > 0)
		{
			bShared = true;
		}

		for (uint32 t = 0; t < ticks; ++t)
		{
			const Vec3 pos0 = track1->GetValueFromKey(t);
			const Vec3 pos1 = track2->GetValue(t);
			if (!pos0.IsEquivalent(pos1, 0.0000001f))
			{
				bShared = false;
				break;
			}
		}
	}

	return bShared;
}


bool CTrackStorage::IsKeyTimesIdentical(const KeyTimesInformationPtr& track1, const KeyTimesInformationPtr& track2)
{
	bool bShared(false);
	if (track1->GetNumKeys() == track2->GetNumKeys())
	{
		uint32 ticks = track1->GetNumKeys();

		if (ticks > 0)
		{
			bShared = true;
		}

		for (uint32 t = 0; t < ticks; ++t)
		{
			if (track1->GetKeyValueFloat(t) != track2->GetKeyValueFloat(t))
			{
				bShared = false;
				break;

			}
		}
	}

	return bShared;

}
// worse method. only for testing purposes
void CTrackStorage::Analyze(uint32& TrackShared, uint32& SizeDataShared, uint32& TotalTracks, uint32& TotalMemory,  CSkeletonInfo * currentSkeleton)
{
	ThreadUtils::AutoLock lock(m_lock);

	std::map<uint32, uint32> m_BonesCount;

	size_t totalAnims = m_arrGlobalAnimationHeaderCAF.size();

	for (size_t i = 0; i < totalAnims; ++i)
	{
		size_t srcSize = m_arrGlobalAnimationHeaderCAF[i].m_arrController.size();

		for (size_t k = 0; k < srcSize; k++)
		{
			CController * pSrc = dynamic_cast<CController *>(m_arrGlobalAnimationHeaderCAF[i].m_arrController[k].get());				
			if (pSrc )
			{

				if (pSrc->GetRotationController())
				{
					TotalMemory += pSrc->GetRotationController()->GetRotationStorage()->GetDataRawSize() + pSrc->GetRotationController()->GetKeyTimesInformation()->GetDataRawSize();
					++TotalTracks;
				}
				if (pSrc->GetPositionController())
				{
					TotalMemory += pSrc->GetPositionController()->GetPositionStorage()->GetDataRawSize() + pSrc->GetPositionController()->GetKeyTimesInformation()->GetDataRawSize();
					++TotalTracks;
				}


			}
			else
			{
				continue;
			}

			for (uint32 j = i +1; j < totalAnims; ++j)
			{

				size_t dstSize = m_arrGlobalAnimationHeaderCAF[j].m_arrController.size();

				for (size_t l = 0; l < dstSize; ++l)
				{

					CController * pDst = dynamic_cast<CController *>(m_arrGlobalAnimationHeaderCAF[j].m_arrController[l].get());

					if (pDst && pDst->m_bShared)
					{
						continue;
					}

					if (pSrc && pDst)
					{

						bool bAlready(false);

						if (pSrc->GetPositionController() && pDst->GetPositionController() &&(pSrc->GetPositionController()->GetNumKeys() == pDst->GetPositionController()->GetNumKeys()))
						{
							// compare position tracks
							uint32 ticks = pSrc->GetPositionController()->GetNumKeys();
							bool bShared(false);

							if (ticks > 0)
								bShared = true;

							for (uint32 t = 0; t < ticks; ++t)
							{
								const Vec3 pos0 = pSrc->GetPositionController()->GetValueFromKey(t);
								const Vec3 pos1 = pDst->GetPositionController()->GetValueFromKey(t);
								if (!pos0.IsEquivalent(pos1, 0.0000001f))
								{
									bShared = false;
									break;
								}
							}
							if (bShared)// && !pDst->m_bShared)
							{
								//								++TrackShared;
								//								SizeDataShared += pSrc->GetPositionController()->GetDataRawSize() + pSrc->GetPositionController()->GetKeyTimesInformation()->GetDataRawSize();
								bAlready = true;
							}

						}


						if (pSrc->GetRotationController() && pDst->GetRotationController() &&(pSrc->GetRotationController()->GetNumKeys() == pDst->GetRotationController()->GetNumKeys()))
						{
							// compare position tracks
							uint32 ticks = pSrc->GetRotationController()->GetNumKeys();
							bool bShared(false);

							if (ticks > 0)
								bShared = true;

							for (uint32 t = 0; t < ticks; ++t)
							{
								const Quat pos0 = pSrc->GetRotationController()->GetValueFromKey(t);
								const Quat pos1 = pDst->GetRotationController()->GetValueFromKey(t);
								if (!IsQuatEquivalent_dot(pos0, pos1, 0.0000001f))
								{
									bShared = false;
									break;
								}
							}
							if (bShared)// && !pDst->m_bShared)
							{
								//								if (!bAlready)
								//								++TrackShared;
								bAlready = true;
								//								SizeDataShared += pSrc->GetRotationController()->GetDataRawSize() + pSrc->GetRotationController()->GetKeyTimesInformation()->GetDataRawSize();
							}
						}

						if (bAlready)
						{
							pDst->m_bShared = true;
							//							m_arrGlobalAnimationHeaderCAF[j].m_arrController[l] = 0;
						}
					}
				}
			}
		}
	}


	for (uint32 i = 0; i < totalAnims; ++i)
	{
		size_t srcSize = m_arrGlobalAnimationHeaderCAF[i].m_arrController.size();

		for (size_t k = 0; k < srcSize; k++)
		{
			CController * pSrc = dynamic_cast<CController *>(m_arrGlobalAnimationHeaderCAF[i].m_arrController[k].get());				
			if (pSrc && pSrc->m_bShared )
			{
				m_BonesCount[pSrc->GetID()] += 1;

				if (pSrc->GetRotationController())
				{
					SizeDataShared += pSrc->GetRotationController()->GetRotationStorage()->GetDataRawSize() + pSrc->GetRotationController()->GetKeyTimesInformation()->GetDataRawSize();
					++TrackShared;
				}
				if (pSrc->GetPositionController())
				{
					SizeDataShared += pSrc->GetPositionController()->GetPositionStorage()->GetDataRawSize() + pSrc->GetPositionController()->GetKeyTimesInformation()->GetDataRawSize();
					++TrackShared;
				}
			}
		}
	}



	std::map<uint32, uint32>::iterator it= m_BonesCount.begin();
	std::map<uint32, uint32>::iterator end = m_BonesCount.end();

	for (; it != end; ++it)
	{
		size_t bones = currentSkeleton->m_SkinningInfo.m_arrBonesDesc.size();
		bool bNotFound(true);
		for (uint32 b =0; b < bones; ++b )
		{
			if (currentSkeleton->m_SkinningInfo.m_arrBonesDesc[b].m_nControllerID == it->first)
			{
				RCLog("Bone %s shared %d times", currentSkeleton->m_SkinningInfo.m_arrBonesDesc[b].m_arrBoneName, it->second);
				bNotFound = false;
				break;
			}
		}

		if (bNotFound)
		{
			RCLog("Unnamed bone(ID=%d) shared %d times", it->first, it->second);
		}

	}
}

// trying compress keytimes information
void CTrackStorage::AnalyzeKeyTimes()
{
	ThreadUtils::AutoLock lock(m_lock);
	size_t keyTimes =  m_arrKeyTimes.size();

	for (uint32 k = 0; k < keyTimes; ++k)
	{
		// store keytime format in the size

		const uint32 timeFormat = m_arrKeyTimes[k]->GetFormat();
		if (timeFormat == eF32 || timeFormat == eUINT16 || timeFormat == eByte)
		{
			uint16 keyTime = m_arrKeyTimes[k]->GetNumKeys();

			uint16 trackLength = (uint16)(m_arrKeyTimes[k]->GetKeyValueFloat(keyTime - 1) - m_arrKeyTimes[k]->GetKeyValueFloat(0));

			uint16 u8Length = (trackLength >> 3) + 3 * sizeof(uint16);

			if (u8Length < m_arrKeyTimes[k]->GetDataRawSize())
			{
				// convert to bitset
				m_Statistics.m_iSavedBytes += m_arrKeyTimes[k]->GetDataRawSize();
				CreateBitsetKeyTimes(k);
				m_Statistics.m_iSavedBytes -= m_arrKeyTimes[k]->GetDataRawSize();
			}
		}
	}
}

void CTrackStorage::CreateBitsetKeyTimes(int k)
{
	ThreadUtils::AutoLock lock(m_lock);
	DynArray<uint16> data;
	uint16 start = uint16(m_arrKeyTimes[k]->GetKeyValueFloat(0));
	uint16 stop = uint16(m_arrKeyTimes[k]->GetKeyValueFloat(m_arrKeyTimes[k]->GetNumKeys()-1));

	data.push_back(start);
	data.push_back(stop);
	data.push_back(m_arrKeyTimes[k]->GetNumKeys());

	uint16 currentWord(0);
	uint16 currentTime(0);
	//bool bLast(false);
	int j(1), i(0);
	for (i =0, j =0; i < stop - start + 1/*m_arrKeyTimes[k]->GetNumKeys()*/; ++i, ++j)
	{
		if (j == 16)
		{
			data.push_back(currentWord);
			currentWord = 0;
			//							bLast = true;
			j = 0;
		}

		uint16 curData = -1;
		if (currentTime < m_arrKeyTimes[k]->GetNumKeys())
			curData = uint16(m_arrKeyTimes[k]->GetKeyValueFloat(currentTime) - start);

		uint16 val(0);
		if ( i == curData)
		{
			val = 1 << j;
			++currentTime;
		}

		if (i == 0 || i == stop - start)
		{
			val = 1 << j;
		}

		currentWord += val;
	}

	if (j)
	{
		data.push_back(currentWord);
	}

	KeyTimesInformationPtr verify = m_arrKeyTimes[k];
	m_arrKeyTimes[k] = ControllerHelper::ConstructKeyTimesController(eBitset);
	m_arrKeyTimes[k]->ResizeKeyTime(data.size());
	//std::copy()
	memcpy(m_arrKeyTimes[k]->GetData(), &data[0], m_arrKeyTimes[k]->GetDataRawSize());


	for (int t = 0; t < verify->GetNumKeys(); ++t) 
	{
		if ( verify->GetKeyValueFloat(t) != m_arrKeyTimes[k]->GetKeyValueFloat(t))
		{
			int tada = 0;
			(void)tada;
		}
	}
}

static size_t AlignUpTo(size_t offset, size_t alignment)
{
	return offset + (offset % alignment > 0 ? (alignment - offset % alignment) : 0);
}

static int32 AllocateTrackStorage(int32& offset, int32 size, bool bForInPlace)
{
	size = Align(size, 4);
	if (bForInPlace)
	{
		offset -= size;
		return offset;
	}
	else
	{
		int32 ret = offset;
		offset += size;
		return ret;
	}
}

void CTrackStorage::SaveDataBase905(const char* name, bool bPrepareForInPlaceStream, int pointerSize)
{
	const bool bSwapEndian = (m_bBigEndianOutput ? (eEndianness_Big == eEndianness_NonNative) : (eEndianness_Big == eEndianness_Native));

	SEndiannessSwapper swapEndiannes(bSwapEndian);

	ThreadUtils::AutoLock lock(m_lock);

	// Save Controllers 
	std::vector<uint16> sizesKeyTimes;

	size_t keyTimes =  m_arrKeyTimes.size();
	sizesKeyTimes.reserve(keyTimes);

	AnalyzeKeyTimes();


	// build remap 

	// KeyTimes remap
	std::vector<uint32> ktRemap(keyTimes);
	std::vector<uint32> ktRevRemap(keyTimes);
	std::vector<uint32> ktFormats(eBitset + 1);
	std::vector<uint32> ktCount(eBitset + 1); 

	memset(&ktFormats[0], 0, ktFormats.size() * sizeof(uint32));
	memset(&ktCount[0], 0, ktCount.size() * sizeof(uint32));

	for (uint32 k = 0; k < keyTimes; ++k)
	{
		++ktFormats[m_arrKeyTimes[k]->GetFormat()];
	}

	for (uint32 i = 1; i < eBitset + 1; ++i)
	{
		ktCount[i] +=  ktCount[i-1] + ktFormats[i-1];
	}

	for (uint32 k = 0; k < keyTimes; ++k)
	{
		uint16 curFormat = m_arrKeyTimes[k]->GetFormat();
		ktRemap[k] =  ktCount[curFormat];
		ktRevRemap[ktRemap[k]] = k;
		++ktCount[curFormat];
	}

	// Positions remap
	size_t keyPos = m_arrPositionTracks.size();

	std::vector<uint32> pRemap(keyPos);
	std::vector<uint32> pRevRemap(keyPos);
	std::vector<uint32> pFormats(eAutomaticQuat);
	std::vector<uint32> pCount(eAutomaticQuat);

	memset(&pFormats[0], 0, pFormats.size() * sizeof(uint32));
	memset(&pCount[0], 0, pCount.size() * sizeof(uint32));

	for (uint32 k = 0; k < keyPos; ++k)
	{
		++pFormats[m_arrPositionTracks[k]->GetFormat()];
	}

	// transforming counts into offsets?
	for (uint32 i = 1; i < eAutomaticQuat; ++i)
	{
		pCount[i] += pCount[i-1] + pFormats[i-1];
	}

	// storing offsets into remap (by key index)
	// pRevRemap -> offset to index
	for (uint32 k = 0; k < keyPos; ++k)
	{
		uint16 curFormat = m_arrPositionTracks[k]->GetFormat();
		pRemap[k] = pCount[curFormat];
		pRevRemap[pRemap[k]] = k;
		++pCount[curFormat];
	}

	// Rotations remap
	size_t keyRot = m_arrRotationTracks.size();

	std::vector<uint32> rRemap(keyRot);
	std::vector<uint32> rRevRemap(keyRot);
	std::vector<uint32> rFormats(eAutomaticQuat);
	std::vector<uint32> rCount(eAutomaticQuat);

	memset(&rFormats[0], 0, rFormats.size() * sizeof(uint32));
	memset(&rCount[0], 0, rCount.size() * sizeof(uint32));

	for (size_t k = 0; k < keyRot; ++k)
	{
		++rFormats[m_arrRotationTracks[k]->GetFormat()];
	}

	for (size_t i = 1; i < eAutomaticQuat; ++i)
	{
		rCount[i] += rCount[i-1] + rFormats[i-1];
	}

	for (size_t k = 0; k < keyRot; ++k)
	{
		uint16 curFormat = m_arrRotationTracks[k]->GetFormat();
		rRemap[k] = rCount[curFormat];
		rRevRemap[rRemap[k]] = k;
		++rCount[curFormat];
	}

	std::vector<int32> keyTimeOffsets(keyTimes);

	// store data size for each 'keyTime'
	for (size_t k = 0; k < keyTimes; ++k)
	{
		uint16 keyTime = m_arrKeyTimes[k]->GetDataCount();
		sizesKeyTimes.push_back(keyTime);
	}

	// Tracks will be placed at the end of the block, so this will be negative (as will offsets)
	int32 trackOffset = 0;

	// calculate total size including aligns
	for (size_t kk = 0; kk < keyTimes; ++kk)
	{
		uint32 k = ktRevRemap[kk];
		keyTimeOffsets[kk] = AllocateTrackStorage(trackOffset, m_arrKeyTimes[k]->GetDataRawSize(), bPrepareForInPlaceStream);
	}

	std::vector<uint16> sizesPos;
	sizesPos.reserve(keyPos);

	std::vector<int32> keyPosOffsets(keyPos);

	for (uint32 p = 0; p < keyPos; ++p)
	{
		uint16 numKeyPos = m_arrPositionTracks[p]->GetDataCount();
		uint8 format = m_arrPositionTracks[p]->GetFormat();
		sizesPos.push_back(numKeyPos);
	}

	for (uint32 pp = 0; pp < keyPos; ++pp)
	{
		uint32 p = pRevRemap[pp];
		keyPosOffsets[pp] = AllocateTrackStorage(trackOffset, m_arrPositionTracks[p]->GetDataRawSize(), bPrepareForInPlaceStream);
	}



	std::vector<uint16> sizesRot;
	sizesRot.reserve(keyRot);

	std::vector<int32> keyRotOffsets(keyRot+1);

	for (uint32 r = 0; r < keyRot; ++r)
	{

		uint16 numKeyRot = m_arrRotationTracks[r]->GetDataCount();
		sizesRot.push_back(numKeyRot);
	}


	for (uint32 rr = 0; rr < keyRot; ++rr)
	{
		uint32 r = rRevRemap[rr];
		keyRotOffsets[rr] = AllocateTrackStorage(trackOffset, m_arrRotationTracks[r]->GetDataRawSize(), bPrepareForInPlaceStream);
	}

	keyRotOffsets[keyRot] = trackOffset;

	uint32 nTrackSize = abs(trackOffset);

	uint32 nSizeOffsBlockLen = 0;
	nSizeOffsBlockLen += pFormats.size() * sizeof(uint32) + sizesPos.size() * sizeof(uint16);
	nSizeOffsBlockLen += ktFormats.size() * sizeof(uint32) + sizesKeyTimes.size() * sizeof(uint16);
	nSizeOffsBlockLen += rFormats.size() * sizeof(uint32) + sizesRot.size() * sizeof(uint16);
	nSizeOffsBlockLen += (keyRot + keyPos + keyTimes + 1) * sizeof(uint32);
	if (bPrepareForInPlaceStream)
		nSizeOffsBlockLen += sizeof(uint32); // for padding length
	nSizeOffsBlockLen = Align(nSizeOffsBlockLen, 4);

	// count size for storing 
	uint32 totalAnims = m_arrGlobalAnimationHeaderCAF.size();
	size_t nTotalControllerCount = 0;

	uint32 nAnimHeaderBlockLen = 0;
	uint32 nAnimControllerBlockLen = 0;
	for (uint32 i = 0; i < totalAnims; ++i)
	{
		uint32 nAnimLen = 0;

		const uint32 numControllers = m_arrGlobalAnimationHeaderCAF[i].m_arrControllerInfo.size();
		nAnimLen += sizeof(MotionParams905);
		nAnimLen += m_arrGlobalAnimationHeaderCAF[i].m_FootPlantBits.size() * sizeof(uint8) + sizeof(uint16);

		nAnimLen += m_arrGlobalAnimationHeaderCAF[i].m_FilePath.size() + sizeof(uint16);
		// counter for ControllerInfo
		nAnimLen += sizeof(uint16);

		if (bPrepareForInPlaceStream)
		{
			// Offset to controller headers, relative to the start of the anim block
			nAnimLen += sizeof(int32);
		}

		nAnimHeaderBlockLen += nAnimLen;

		nAnimControllerBlockLen += numControllers * sizeof(CControllerInfo);
		nTotalControllerCount += numControllers;
	}

	uint32 nAnimBlockLen = Align(nAnimHeaderBlockLen + nAnimControllerBlockLen, 4);

	// Used to determine the amount of storage required by the engine for the DBA. Used to pad
	// the on-disk file.
	size_t estimatedEngineSize = 0;
	
	if (bPrepareForInPlaceStream)
	{
		size_t nControllerHeaderUpperBound = RC_GetSizeOfControllerOptNonVirtual(pointerSize);
		estimatedEngineSize += Align(totalAnims * sizeof(uint16), 16);
		estimatedEngineSize += Align(nControllerHeaderUpperBound * nTotalControllerCount, 16);
		estimatedEngineSize += Align(nTrackSize, 16);
	}

	size_t nRequiredStorageSize = Align(nSizeOffsBlockLen + nAnimBlockLen + nTrackSize, 4);

	// main storage
	std::vector<char> storage;
	// add real data
	storage.resize(max(nRequiredStorageSize, estimatedEngineSize), 0);
	uint32 currentPointer = 0;

	// keytime sizes

	std::vector<uint16> tmp(sizesKeyTimes.size());

	for (size_t i = 0; i < sizesKeyTimes.size(); ++i)
	{
		tmp[ktRemap[i]] = sizesKeyTimes[i];
	}

	swapEndiannes(&tmp[0], tmp.size());
	memcpy(&storage[currentPointer], &tmp[0], sizesKeyTimes.size() * sizeof(uint16));
	currentPointer += sizesKeyTimes.size() * sizeof(uint16);

	swapEndiannes(&ktFormats[0], ktFormats.size());
	memcpy(&storage[currentPointer], &ktFormats[0], ktFormats.size() * sizeof(uint32));
	currentPointer += ktFormats.size() * sizeof(uint32);

	tmp.resize(sizesPos.size(), 0);

	for (size_t i = 0; i < sizesPos.size(); ++i)
	{
		tmp[pRemap[i]] = sizesPos[i];
	}

	// pos sizes
	swapEndiannes(&tmp[0], tmp.size());
	memcpy(&storage[currentPointer], &tmp[0], sizesPos.size() * sizeof(uint16));
	currentPointer += sizesPos.size() * sizeof(uint16);

	swapEndiannes(&pFormats[0], pFormats.size());
	memcpy(&storage[currentPointer], &pFormats[0], pFormats.size()* sizeof(uint32));
	currentPointer += pFormats.size() * sizeof(uint32);


	tmp.resize(sizesRot.size(), 0);

	for (size_t i = 0; i < sizesRot.size(); ++i)
	{
		tmp[rRemap[i]] = sizesRot[i];
	}

	swapEndiannes(&tmp[0], tmp.size());
	memcpy(&storage[currentPointer], &tmp[0], sizesRot.size() * sizeof(uint16));
	currentPointer += sizesRot.size() * sizeof(uint16);

	swapEndiannes(&rFormats[0], rFormats.size());
	memcpy(&storage[currentPointer], &rFormats[0], rFormats.size() * sizeof(uint32));
	currentPointer += rFormats.size() * sizeof(uint32);



	memcpy(&storage[currentPointer], &keyTimeOffsets[0], keyTimeOffsets.size() * sizeof(uint32));
	swapEndiannes((uint32*)&storage[currentPointer], keyTimeOffsets.size());
	currentPointer += keyTimeOffsets.size() * sizeof(uint32);

	memcpy(&storage[currentPointer], &keyPosOffsets[0], keyPosOffsets.size() * sizeof(uint32));
	swapEndiannes((uint32*)&storage[currentPointer], keyPosOffsets.size());
	currentPointer += keyPosOffsets.size() * sizeof(uint32);

	//	keyRotOffsets[keyRot] += currentPointer % 4;
	memcpy(&storage[currentPointer], &keyRotOffsets[0], keyRotOffsets.size() * sizeof(uint32));
	swapEndiannes((uint32*)&storage[currentPointer], keyRotOffsets.size());
	currentPointer += keyRotOffsets.size() * sizeof(uint32);

	uint32 nPaddingLength = 0;
	if (bPrepareForInPlaceStream)
	{
		// Anim block and tracks need to be at the end of storage. Each needs to start on a 4 byte boundary,
		// storage.size() and the indiviual block lengths should be aligned.
		assert (!(nAnimBlockLen & 3));
		assert (!(nTrackSize & 3));
		assert (!(storage.size() & 3));
		nPaddingLength = storage.size() - (nAnimBlockLen + nTrackSize + nSizeOffsBlockLen);

		memcpy(&storage[currentPointer], &nPaddingLength, sizeof(nPaddingLength));
		swapEndiannes((uint32*)&storage[currentPointer], 1);
		currentPointer += sizeof(nPaddingLength);
	}

	currentPointer = Align(currentPointer, 4);
	assert(currentPointer == nSizeOffsBlockLen);

	// End of size/offs block. Pad to the start of the anim block.

	currentPointer += nPaddingLength;

	char* pTrackStorageBase = NULL;

	if (bPrepareForInPlaceStream)
	{
		// Start of track storage should be at the end of the storage, offsets are -ve
		pTrackStorageBase = &storage[0] + storage.size();
	}
	else
	{
		// Start of track storage is in the middle (here), offsets are +ve
		pTrackStorageBase = &storage[currentPointer];
		currentPointer += nTrackSize;
	}

	for (uint32 kk = 0; kk < keyTimes; ++kk)
	{
		uint32 k = ktRevRemap[kk];
		if (bSwapEndian)
		{
			m_arrKeyTimes[k]->SwapBytes();
		}

		memcpy(&pTrackStorageBase[keyTimeOffsets[kk]], m_arrKeyTimes[k]->GetData(), m_arrKeyTimes[k]->GetDataRawSize());
	}


	// copy raw data

	for (uint32 pp = 0; pp < keyPos; ++pp)
	{
		uint32 p = pRevRemap[pp];
		if (bSwapEndian)
		{
			m_arrPositionTracks[p]->SwapBytes();
		}

		memcpy(&pTrackStorageBase[keyPosOffsets[pp]], m_arrPositionTracks[p]->GetData(), m_arrPositionTracks[p]->GetDataRawSize());
	}


	// copy raw data

	for (uint32 rr = 0; rr < keyRot; ++rr)
	{
		uint32 r = rRevRemap[rr];
		if (bSwapEndian)
		{
			m_arrRotationTracks[r]->SwapBytes();
		}

		memcpy(&pTrackStorageBase[keyRotOffsets[rr]], m_arrRotationTracks[r]->GetData(),m_arrRotationTracks[r]->GetDataRawSize());
	}

	CONTROLLER_CHUNK_DESC_0905 chunk0905;
	chunk0905.numKeyPos = keyPos;
	chunk0905.numKeyRot = keyRot;
	chunk0905.numKeyTime = keyTimes;
	chunk0905.numAnims = totalAnims;

	// Save animations list

	uint32 animBlockPointer = currentPointer;
	uint32 animControllerBlockPointer = currentPointer + nAnimHeaderBlockLen;

	for (uint32 i = 0; i < totalAnims; ++i)
	{
		uint16 strSize = m_arrGlobalAnimationHeaderCAF[i].m_FilePath.size();
		swapEndiannes(strSize);
		memcpy(&storage[currentPointer], &strSize, sizeof(uint16));
		currentPointer += sizeof(uint16);

		// fill info
		// Name length
		// name
		memcpy(&storage[currentPointer], m_arrGlobalAnimationHeaderCAF[i].m_FilePath.c_str(),m_arrGlobalAnimationHeaderCAF[i].m_FilePath.size());
		currentPointer += m_arrGlobalAnimationHeaderCAF[i].m_FilePath.size();

		// info
		///!!!!
		MotionParams905 info;
		m_arrGlobalAnimationHeaderCAF[i].ExtractMotionParameters(&info, m_bBigEndianOutput);

		memcpy(&storage[currentPointer], &info, sizeof(info));
		currentPointer += sizeof(info);

		// footplants

		uint16 footplans;
		footplans = m_arrGlobalAnimationHeaderCAF[i].m_FootPlantBits.size();
		swapEndiannes(footplans);
		memcpy(&storage[currentPointer], &footplans, sizeof(uint16));
		currentPointer += sizeof(uint16);

		footplans = m_arrGlobalAnimationHeaderCAF[i].m_FootPlantBits.size();
		if (footplans > 0)
		{
			memcpy(&storage[currentPointer], &(m_arrGlobalAnimationHeaderCAF[i].m_FootPlantBits[0]), footplans);
			currentPointer += footplans;
		}

		// 
		uint16 controllerInfo = m_arrGlobalAnimationHeaderCAF[i].m_arrControllerInfo.size();
		swapEndiannes(controllerInfo);
		memcpy(&storage[currentPointer], &controllerInfo, sizeof(uint16));
		currentPointer += sizeof(uint16);
		controllerInfo = m_arrGlobalAnimationHeaderCAF[i].m_arrControllerInfo.size();

		if (bPrepareForInPlaceStream)
		{
			int32 offsetToControllerHeaders = animControllerBlockPointer - animBlockPointer;
			swapEndiannes(&offsetToControllerHeaders, 1);
			memcpy(&storage[currentPointer], &offsetToControllerHeaders, sizeof(offsetToControllerHeaders));
			currentPointer += sizeof(uint32);
		}
		else
		{
			animControllerBlockPointer = currentPointer;
		}

		///!!!!!!!!!
		std::vector<CControllerInfo> infos(m_arrGlobalAnimationHeaderCAF[i].m_arrControllerInfo.size());
		for (size_t  c =0; c < m_arrGlobalAnimationHeaderCAF[i].m_arrControllerInfo.size(); ++c)
		{
			if (m_arrGlobalAnimationHeaderCAF[i].m_arrControllerInfo[c].m_nPosKeyTimeTrack != -1)
				infos[c].m_nPosKeyTimeTrack = ktRemap[m_arrGlobalAnimationHeaderCAF[i].m_arrControllerInfo[c].m_nPosKeyTimeTrack];
			else
				infos[c].m_nPosKeyTimeTrack = - 1;
			swapEndiannes(infos[c].m_nPosKeyTimeTrack);

			if (m_arrGlobalAnimationHeaderCAF[i].m_arrControllerInfo[c].m_nRotKeyTimeTrack != - 1)
				infos[c].m_nRotKeyTimeTrack = ktRemap[m_arrGlobalAnimationHeaderCAF[i].m_arrControllerInfo[c].m_nRotKeyTimeTrack];
			else
				infos[c].m_nRotKeyTimeTrack = - 1;
			swapEndiannes(infos[c].m_nRotKeyTimeTrack);

			if (m_arrGlobalAnimationHeaderCAF[i].m_arrControllerInfo[c].m_nPosTrack != -1)
				infos[c].m_nPosTrack = pRemap[m_arrGlobalAnimationHeaderCAF[i].m_arrControllerInfo[c].m_nPosTrack];
			else
				infos[c].m_nPosTrack = -1;
			swapEndiannes(infos[c].m_nPosTrack);

			if (m_arrGlobalAnimationHeaderCAF[i].m_arrControllerInfo[c].m_nRotTrack != -1)
				infos[c].m_nRotTrack = rRemap[m_arrGlobalAnimationHeaderCAF[i].m_arrControllerInfo[c].m_nRotTrack];
			else
				infos[c].m_nRotTrack = -1;
			swapEndiannes(infos[c].m_nRotTrack);

			infos[c].m_nControllerID = m_arrGlobalAnimationHeaderCAF[i].m_arrControllerInfo[c].m_nControllerID;

			swapEndiannes(infos[c].m_nControllerID);
		}

		if (controllerInfo > 0)
		{
			memcpy(&storage[animControllerBlockPointer], &(infos[0]), controllerInfo * sizeof(CControllerInfo));
		}

		if (bPrepareForInPlaceStream)
		{
			animControllerBlockPointer += controllerInfo * sizeof(CControllerInfo);
		}
		else
		{
			currentPointer += controllerInfo * sizeof(CControllerInfo);
		}
	}

	swapEndiannes(chunk0905.numAnims);
	swapEndiannes(chunk0905.numKeyPos);
	swapEndiannes(chunk0905.numKeyRot);
	swapEndiannes(chunk0905.numKeyTime);

	CChunkFile chunkFile;
	CSaverCGF saver(chunkFile);

	saver.SaveControllerDB905(bSwapEndian, chunk0905, &storage[0], storage.size());

	if (!FileUtil::EnsureDirectoryExists(PathUtil::GetPathWithoutFilename(name).c_str()))
	{
		RCLogError("Failed creating directory for %s", name);
	}

	SetFileAttributes( name,FILE_ATTRIBUTE_ARCHIVE );
	chunkFile.Write( name );
}
