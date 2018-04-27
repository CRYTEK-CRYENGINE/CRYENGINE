// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"

#include "AnimationDatabase.h"

int CAnimationDatabase::s_mnAllowEditableDatabasesInPureGame = 0;
bool CAnimationDatabase::s_registeredCVars = false;

void CAnimationDatabase::RegisterCVars()
{
	if (!s_registeredCVars)
	{
		REGISTER_CVAR3("mn_allowEditableDatabasesInPureGame", s_mnAllowEditableDatabasesInPureGame, 0, 0, "Do not store editable databases");
		s_registeredCVars = true;
	}
}

float AppendLayers(SFragmentData& fragmentData, const CFragment& fragment, uint8 partID, const IAnimationSet* animSet, float startTime, float startOffset, bool isBlend, bool& isOneShot)
{
	float totalTime = 0.0f;

	bool calcTime = true;

	SAnimClip nullClip;
	nullClip.blend.exitTime = 0.0f;

	SProceduralEntry nullProc;
	nullProc.blend.exitTime = 0.0f;

	uint32 numALayers = fragment.m_animLayers.size();
	uint32 numPLayers = fragment.m_procLayers.size();
	if (fragmentData.animLayers.size() < numALayers)
		fragmentData.animLayers.resize(numALayers);
	if (fragmentData.procLayers.size() < numPLayers)
		fragmentData.procLayers.resize(numPLayers);
	for (uint32 i = 0; i < numALayers; i++)
	{
		const uint32 oldLength = fragmentData.animLayers[i].size();
		const uint32 fragLength = fragment.m_animLayers[i].size();
		const bool hadEntry = !fragmentData.animLayers[i].empty();
		const bool hasNewEntry = (fragLength > 0);
		float lastDuration = 0.0f;
		int startIdx = oldLength;

		bool shouldOverride = false;
		if (hadEntry && hasNewEntry)
		{
			//--- Should we merge over the existing final blend or append to it?
			const SAnimBlend& prevBlend = fragmentData.animLayers[i][oldLength - 1].blend;
			shouldOverride = (prevBlend.exitTime >= startTime) || prevBlend.terminal;
			if (shouldOverride)
			{
				startIdx = max(startIdx - 1, 0);
			}
		}

		float layerTotalTime = 0.0f;

		uint32 installFragLength = isBlend ? max(fragLength, 1u) : fragLength;

		fragmentData.animLayers[i].resize(startIdx + installFragLength);
		for (uint32 k = 0; k < installFragLength; k++)
		{
			const bool firstClip = (k == 0);
			SAnimClip& animClip = fragmentData.animLayers[i][startIdx + k];
			const SAnimClip* sourceClip = (k < fragLength) ? &fragment.m_animLayers[i][k] : &nullClip;

			animClip.animation = sourceClip->animation;
			animClip.part = partID;
			if (shouldOverride && firstClip)
			{
				if (isBlend)
				{
					float oldExitTime = animClip.blend.exitTime;
					animClip.blend = sourceClip->blend;
					animClip.blend.exitTime = oldExitTime;
					animClip.blendPart = partID;
				}
			}
			else
			{
				animClip.blend = sourceClip->blend;
				int blendPart = partID;

				if (firstClip)
				{
					animClip.blend.exitTime = max(animClip.blend.exitTime, 0.0f);
					animClip.blend.exitTime += startTime;

					if (!isBlend)
					{
						blendPart = animClip.blendPart;
					}
				}
				animClip.blendPart = blendPart;
			}
			animClip.animation.flags |= animClip.blend.flags;

			if (animSet)
			{
				float animDuration = 0.0f;
				bool isVariableLength = false;
				if (false == animClip.animation.animRef.IsEmpty()
				    && (0 == (animClip.animation.flags & CA_LOOP_ANIMATION))
				    && (animClip.animation.playbackSpeed > 0.0f))
				{
					int animID = animSet->GetAnimIDByCRC(animClip.animation.animRef.crc);
					if (0 <= animID)
					{
						animDuration = ((animSet->GetDuration_sec(animID) - animClip.blend.startTime) / animClip.animation.playbackSpeed);
						const uint32 flags = animSet->GetAnimationFlags(animID);
						isVariableLength = (flags & CA_ASSET_LMG) != 0;
					}
				}

				animClip.referenceLength = animDuration;
				animClip.isVariableLength = isVariableLength;

				if (animClip.blend.exitTime < 0.0f)
				{
					animClip.blend.exitTime = lastDuration;
				}

				if (calcTime)
				{
					totalTime += animClip.blend.exitTime;
				}

				if (!isBlend)
				{
					float previousStartTime = layerTotalTime;
					layerTotalTime += animClip.blend.exitTime;
					float animStartTime = layerTotalTime;

					if (i == 0)
					{
						//--- First layer is be used to check for a looping fragment
						isOneShot = ((animClip.animation.flags & CA_LOOP_ANIMATION) == 0);
					}

					if (startOffset > animStartTime)
					{
						// animation start is clipped by fragment transition
						float animStartOffset = startOffset - animStartTime;
						animClip.blend.startTime += animStartOffset / max(animDuration, 0.001f);
						animClip.blend.startTime = clamp_tpl(animClip.blend.startTime, 0.0f, 1.0f);
						animClip.blend.exitTime = 0.0f;
					}
					else if (startOffset > previousStartTime)
					{
						// previous animation start was clipped by fragment transition: update exit time
						animClip.blend.exitTime -= startOffset - previousStartTime;
						animClip.blend.exitTime = max(0.0f, animClip.blend.exitTime);
					}
				}

				lastDuration = animDuration;
			}
		}

		if (calcTime && !isBlend)
		{
			totalTime += lastDuration;
		}
		calcTime = false;
	}
	//--- Ensure that any dangling entries from the previous blend get assigned to last part
	for (uint32 i = numALayers; i < fragmentData.animLayers.size(); i++)
	{
		const uint32 oldLength = fragmentData.animLayers[i].size();
		const bool hadEntry = !fragmentData.animLayers[i].empty();

		if (hadEntry)
		{
			//--- Should we merge over the existing final blend or append to it?
			SAnimClip& animClip = fragmentData.animLayers[i][oldLength - 1];
			if ((animClip.blend.exitTime >= startTime) || animClip.blend.terminal)
			{
				animClip.part = partID;
			}
			else
			{
				SAnimClip newClip;
				newClip.blend.exitTime = startTime;
				newClip.blendPart = animClip.part;
				newClip.part = partID;
				fragmentData.animLayers[i].push_back(newClip);
			}
		}
	}

	for (uint32 i = 0; i < numPLayers; i++)
	{
		const uint32 oldLength = fragmentData.procLayers[i].size();
		const uint32 fragLength = fragment.m_procLayers[i].size();
		const bool hadEntry = (oldLength > 0);
		const bool hasNewEntry = (fragLength > 0);
		int startIdx = oldLength;

		bool shouldOverride = false;
		if (hadEntry && hasNewEntry)
		{
			//--- Should we merge over the existing final blend or append to it?
			const SAnimBlend& prevBlend = fragmentData.procLayers[i][oldLength - 1].blend;
			shouldOverride = (prevBlend.exitTime >= startTime) || prevBlend.terminal;
			if (shouldOverride)
			{
				startIdx = max(startIdx - 1, 0);
			}
		}

		uint32 installProcLength = isBlend ? max(fragLength, 1u) : fragLength;

		fragmentData.procLayers[i].resize(startIdx + installProcLength);

		float procTotalTime = 0.0f;
		for (uint32 k = 0; k < installProcLength; k++)
		{
			const bool firstClip = (k == 0);
			const SProceduralEntry* sourceClip = (k < fragLength) ? &fragment.m_procLayers[i][k] : &nullProc;
			SProceduralEntry& procClip = fragmentData.procLayers[i][startIdx + k];

			procClip.typeNameHash = sourceClip->typeNameHash;
			procClip.pProceduralParams = sourceClip->pProceduralParams;
			procClip.part = partID;
			if (shouldOverride && firstClip)
			{
				if (isBlend)
				{
					float oldExitTime = procClip.blend.exitTime;
					procClip.blend = sourceClip->blend;
					procClip.blend.exitTime = oldExitTime;
					procClip.blendPart = partID;
				}
			}
			else
			{
				int blendPart = partID;
				procClip.blend = sourceClip->blend;

				if (firstClip)
				{
					procClip.blend.exitTime = max(procClip.blend.exitTime, 0.0f);
					procClip.blend.exitTime += startTime;

					if (!isBlend)
					{
						blendPart = procClip.blendPart;
					}
				}
				procClip.blendPart = blendPart;
			}

			if (calcTime)
			{
				totalTime += procClip.blend.exitTime;
			}

			if (!isBlend)
			{
				float previousStartTime = procTotalTime;
				procTotalTime += procClip.blend.exitTime;
				float layerStartTime = procTotalTime;

				if (startOffset > layerStartTime)
				{
					procClip.blend.exitTime = 0.0f;
				}
				else if (startOffset > previousStartTime)
				{
					procClip.blend.exitTime -= startOffset - previousStartTime;
					procClip.blend.exitTime = max(0.0f, procClip.blend.exitTime);
				}
			}
		}
		calcTime = false;
	}
	//--- Ensure that any dangling entries from the previous blend get assigned to last part
	for (uint32 i = numPLayers; i < fragmentData.procLayers.size(); i++)
	{
		const uint32 oldLength = fragmentData.procLayers[i].size();
		const bool hadEntry = !fragmentData.procLayers[i].empty();

		if (hadEntry)
		{
			//--- Should we merge over the existing final blend or append to it?
			SProceduralEntry& procClip = fragmentData.procLayers[i][oldLength - 1];
			if ((procClip.blend.exitTime >= startTime) || procClip.blend.terminal)
			{
				procClip.part = partID;
			}
			else
			{
				SProceduralEntry newEntry;
				newEntry.blend.exitTime = startTime;
				newEntry.blendPart = procClip.part;
				newEntry.part = partID;
				fragmentData.procLayers[i].push_back(newEntry);
			}
		}
	}
	return totalTime - startTime;
}

float AppendBlend(SFragmentData& outFragmentData, const SBlendQueryResult& blend, const IAnimationSet* inAnimSet, uint8 partID, float& timeOffset, float& timeTally, uint32& retFlags)
{
	const bool isExitTransition = blend.pFragmentBlend->IsExitTransition();
	const float fragmentTime = AppendLayers(outFragmentData, *blend.pFragmentBlend->pFragment, partID, inAnimSet, timeTally, 0.0f, true, outFragmentData.isOneShot);
	timeOffset = blend.pFragmentBlend->enterTime;
	timeTally += fragmentTime;
	retFlags |= (isExitTransition ? eSF_TransitionOutro : eSF_Transition);
	outFragmentData.transitionType[partID] = (isExitTransition ? eCT_TransitionOutro : eCT_Transition);
	outFragmentData.duration[partID] += fragmentTime;

	return fragmentTime;
}

uint32 CAnimationDatabase::Query(SFragmentData& outFragmentData, const SBlendQuery& inBlendQuery, uint32 inOptionIdx, const IAnimationSet* inAnimSet, SFragmentSelection* outFragSelection) const
{
	uint32 retFlags = 0;

	SBlendQueryResult blend1, blend2;
	if (!inBlendQuery.IsFlagSet(SBlendQuery::noTransitions))
	{
		FindBestBlends(inBlendQuery, blend1, blend2);
	}
	const CFragment* fragment = NULL;

	if (inBlendQuery.IsFlagSet(SBlendQuery::toInstalled))
	{
		SFragmentQuery fragQuery;
		fragQuery.fragID = inBlendQuery.fragmentTo;
		fragQuery.requiredTags = inBlendQuery.additionalTags;
		fragQuery.tagState = inBlendQuery.tagStateTo;
		fragQuery.optionIdx = inOptionIdx;

		fragment = GetBestEntry(fragQuery, outFragSelection);
	}

	outFragmentData.animLayers.resize(0);
	outFragmentData.procLayers.resize(0);
	outFragmentData.isOneShot = true;

	for (uint32 i = 0; i < SFragmentData::PART_TOTAL; i++)
	{
		outFragmentData.duration[i] = 0.0f;
		outFragmentData.transitionType[i] = eCT_Normal;
	}

	float timeTally = 0.0f;
	float timeOffset = 0.0f;
	EClipType prevClipType = eCT_Normal;
	int clipIdx = 0;
	if (blend1.pFragmentBlend)
	{
		AppendBlend(outFragmentData, blend1, inAnimSet, clipIdx, timeOffset, timeTally, retFlags);
		clipIdx++;
	}
	if (blend2.pFragmentBlend)
	{
		AppendBlend(outFragmentData, blend2, inAnimSet, clipIdx, timeOffset, timeTally, retFlags);
		clipIdx++;
	}
	if (fragment)
	{
		outFragmentData.blendOutDuration = fragment->m_blendOutDuration;
		outFragmentData.duration[clipIdx] = AppendLayers(outFragmentData, *fragment, clipIdx, inAnimSet, timeTally, timeOffset, false, outFragmentData.isOneShot);
		outFragmentData.transitionType[clipIdx] = eCT_Normal;
		retFlags |= eSF_Fragment;
	}

	return retFlags;
}

bool CAnimationDatabase::ValidateSet(const TFragmentTagSetList& tagSetList)
{
	uint32 totalAnimClips = 0;
	uint32 totalProcClips = 0;
	const uint32 numFragmentTypes = m_fragmentList.size();
	for (uint32 i = 0; i < numFragmentTypes; i++)
	{
		const TFragmentOptionList& optionList = tagSetList.m_values[i];
		uint32 numOptions = optionList.size();
		for (uint32 o = 0; o < numOptions; o++)
		{
			uint32 numAnimLayers = optionList[o].fragment->m_animLayers.size();
			uint32 numProcLayers = optionList[o].fragment->m_procLayers.size();

			for (uint32 l = 0; l < numAnimLayers; l++)
			{
				totalAnimClips += optionList[o].fragment->m_animLayers[l].size();
			}

			for (uint32 l = 0; l < numProcLayers; l++)
			{
				totalProcClips += optionList[o].fragment->m_procLayers[l].size();
			}
		}
	}

	return true;
}

void CAnimationDatabase::SetEntry(FragmentID fragmentID, const SFragTagState& tags, uint32 optionIdx, const CFragment& fragment)
{
	const uint32 numFragmentTypes = m_fragmentList.size();
	if (fragmentID < numFragmentTypes)
	{
		SFragmentEntry& fragmentEntry = *m_fragmentList[fragmentID];
		TFragmentOptionList* pOptionList = fragmentEntry.tagSetList.Find(tags);
		if (pOptionList)
		{
			CRY_ASSERT(optionIdx < (uint32)pOptionList->size());
			if (optionIdx < (uint32)pOptionList->size())
			{
				SFragmentOption& fragOption = (*pOptionList)[optionIdx];
				*fragOption.fragment = fragment;
			}

			return;
		}

		//--- New fragment
		AddEntry(fragmentID, tags, fragment);
	}
}

bool CAnimationDatabase::DeleteEntry(FragmentID fragmentID, const SFragTagState& tags, uint32 optionIdx)
{
	const uint32 numFragmentTypes = m_fragmentList.size();
	if (fragmentID < numFragmentTypes)
	{
		SFragmentEntry& fragmentEntry = *m_fragmentList[fragmentID];

		int idx = fragmentEntry.tagSetList.FindIdx(tags);

		if (idx < 0)
		{
			CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, "[AnimDatabase:DeleteEntry] Invalid fragment tags: %d passed to %s", fragmentID, m_filename.c_str());
			return false;
		}
		else if (optionIdx == OPTION_IDX_INVALID)
		{
			fragmentEntry.tagSetList.Erase(idx);
		}
		else
		{
			TFragmentOptionList& optionList = fragmentEntry.tagSetList.m_values[idx];
			if (optionIdx < (uint32)optionList.size())
			{
				SFragmentOption& fragOption = optionList[optionIdx];
				delete fragOption.fragment;

				optionList.erase(optionList.begin() + optionIdx);

				if (optionList.empty())
				{
					fragmentEntry.tagSetList.Erase(idx);
				}
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, "AnimDatabase: Invalid option index: %u for fragment id: %d passed to %s", optionIdx, fragmentID, m_filename.c_str());
				return false;
			}
		}

		CompressFragmentID(fragmentID);

		return true;
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, "AnimDatabase: Invalid fragment id: %d passed to %s", fragmentID, m_filename.c_str());
	}

	return false;
}

uint32 CAnimationDatabase::AddEntry(FragmentID fragmentID, const SFragTagState& tags, const CFragment& fragment)
{
	const uint32 numFragmentTypes = m_fragmentList.size();
	if (fragmentID < numFragmentTypes)
	{
		CFragment* fragmentPtr = new CFragment(fragment);

		SFragmentEntry& fragmentEntry = *m_fragmentList[fragmentID];
		TFragmentOptionList* pOptionList = fragmentEntry.tagSetList.Find(tags);
		bool addNew = (pOptionList == NULL);
		if (addNew)
		{
			pOptionList = &fragmentEntry.tagSetList.Insert(tags, TFragmentOptionList());
		}

		pOptionList->push_back(SFragmentOption(fragmentPtr));
		const uint32 newOption = pOptionList->size() - 1;

		if (m_autoSort)
		{
			if (addNew)
			{
				//--- Re-sort based on priority
				fragmentEntry.tagSetList.Sort(*m_pTagDef, m_pFragDef->GetSubTagDefinition(fragmentID));
			}

			CompressFragmentID(fragmentID);
		}

		return newOption;
	}

	return 0;
}

void CAnimationDatabase::Sort()
{
	const uint32 numFragmentTypes = m_fragmentList.size();
	for (uint32 i = 0; i < numFragmentTypes; i++)
	{
		m_fragmentList[i]->tagSetList.Sort(*m_pTagDef, m_pFragDef->GetSubTagDefinition(i));
	}

	for (TFragmentBlendDatabase::iterator iter = m_fragmentBlendDB.begin(); iter != m_fragmentBlendDB.end(); ++iter)
	{
		FragmentID fragmentIDFrom = iter->first.fragFrom;
		FragmentID fragmentIDTo = iter->first.fragTo;

		const CTagDefinition* fragFromDef = (fragmentIDFrom != FRAGMENT_ID_INVALID) ? m_pFragDef->GetSubTagDefinition(fragmentIDFrom) : NULL;
		const CTagDefinition* fragToDef = (fragmentIDTo != FRAGMENT_ID_INVALID) ? m_pFragDef->GetSubTagDefinition(fragmentIDTo) : NULL;
		SCompareBlendVariantFunctor comparisonFunctor(*m_pTagDef, fragFromDef, fragToDef);
		std::stable_sort(iter->second.variantList.begin(), iter->second.variantList.end(), comparisonFunctor);
	}

	Compress();
}

void CAnimationDatabase::CompressFragmentID(FragmentID fragID)
{
	CAnimationDatabase::SFragmentEntry* pEntry = m_fragmentList[fragID];

	if (pEntry)
	{
		SAFE_DELETE(pEntry->compiledList);
		const uint32 numUsed = pEntry->tagSetList.Size();
		if (numUsed > 0)
		{
			const CTagDefinition* pFragTagDef = m_pFragDef->GetSubTagDefinition(fragID);
			pEntry->compiledList = pEntry->tagSetList.Compress(*m_pTagDef, pFragTagDef);
		}
	}
}

void CAnimationDatabase::Compress()
{
	const uint32 numFragments = m_pFragDef->GetNum();

	SFragTagState usedTags(TAG_STATE_EMPTY, TAG_STATE_EMPTY);
#if 0
	uint32 totalMem = 0;
	uint32 totalNewMem = 0;
	uint32 totalEntries = 0;
#endif //0
	for (uint32 i = 0; i < numFragments; i++)
	{
		CAnimationDatabase::SFragmentEntry* pEntry = m_fragmentList[i];

		SAFE_DELETE(pEntry->compiledList);

		const uint32 numUsed = pEntry->tagSetList.Size();
		if (numUsed > 0)
		{
			const CTagDefinition* pFragTagDef = m_pFragDef->GetSubTagDefinition(i);
			pEntry->compiledList = pEntry->tagSetList.Compress(*m_pTagDef, pFragTagDef);

#if 0
			uint32 cost = numUsed * 8 * 2;
			uint32 minmem = pEntry->compiledList->GetKeySize();
			uint32 newCost = numUsed * minmem;

			totalMem += cost;
			totalNewMem += newCost;
			totalEntries += numUsed;
#endif //0
		}
	}

#if 0
	{
		//--- Test performance
		LARGE_INTEGER updateStart, updateEnd;
		updateStart.QuadPart = 0;
		updateEnd.QuadPart = 0;

		const uint32 numIts = 100;
		SFragTagState fragTagState;
		uint32 hash = 0;

		QueryPerformanceCounter(&updateStart);

		for (uint32 i = 0; i < numIts; i++)
		{
			for (uint32 f = 0; f < numFragments; f++)
			{
				const CAnimationDatabase::SFragmentEntry& fragEntry = *m_fragmentList[f];
				const CTagDefinition* fragTagDef = m_pFragDef->GetSubTagDefinition(f);
				const CAnimationDatabase::TFragmentOptionList* pOptionList = fragEntry.tagSetList.GetBestMatch(fragTagState, m_pTagDef, fragTagDef);
				if (pOptionList)
				{
					hash += pOptionList->size();
				}
			}
		}
		QueryPerformanceCounter(&updateEnd);
		uint32 ticks = (uint32)(updateEnd.QuadPart - updateStart.QuadPart);
		CryLogAlways("Database %s Fragments: %d Time: %d Hash: %d", GetFilename(), numFragments, ticks, hash);

		CryLogAlways("Compressing %s Num FragIDs: %d Num Frags: %d old cost: %d new cost: %d saving: %d", GetFilename(), numFragments, totalEntries, totalMem, totalNewMem, totalMem - totalNewMem);

		//--- Validation pass
		for (uint32 f = 0; f < numFragments; f++)
		{
			const CAnimationDatabase::SFragmentEntry& fragEntry = *m_fragmentList[f];
			const CTagDefinition* fragTagDef = m_pFragDef->GetSubTagDefinition(f);
			const CAnimationDatabase::TFragmentOptionList* pOptionList = fragEntry.tagSetList.GetBestMatch(fragTagState, m_pTagDef, fragTagDef);
			const CAnimationDatabase::TFragmentOptionList* pOptionListNew = NULL;

			if (fragEntry.compiledList)
			{
				pOptionListNew = fragEntry.compiledList->GetBestMatch(fragTagState);
			}

			if (pOptionListNew && pOptionList)
			{
				CRY_ASSERT(pOptionList->size() == pOptionListNew->size());
			}
			else
			{
				CRY_ASSERT(pOptionList == pOptionListNew);
			}
		}
	}
#endif //0

	if (!gEnv->IsEditor() && !s_mnAllowEditableDatabasesInPureGame)
	{
		//--- Clear the existing ones
		for (uint32 f = 0; f < numFragments; f++)
		{
			CAnimationDatabase::SFragmentEntry& fragEntry = *m_fragmentList[f];

			fragEntry.tagSetList.Resize(0);
		}
	}
}

/*
   bool ValidateBlend(const IAnimationSet *animSet, const SAnimBlend &animBlend, AnimID anim1, const char *animName1, AnimID anim2, const char *animName2, const TagState &tags, MannErrorCallback errorCallback, void *errorCallbackContext)
   {
   if (!animBlend.transition.animRef.IsEmpty())
   {
    int animID = animSet->GetAnimIDByCRC(animBlend.transition.animRef.crc);
    if (animID == -1)
    {
      if (errorCallback)
      {
        //--- Missing animation
        SMannequinErrorReport mannErrorReport;
        mannErrorReport.errorType = SMannequinErrorReport::Blend;
        cry_sprintf(mannErrorReport.error, "Animation not found: %s in blend from %s to %s", animBlend.transition.animRef.GetAnimName(), animName1, animName2);
        mannErrorReport.animFrom = anim1;
        mannErrorReport.animTo	 = anim2;
        mannErrorReport.tags		 = tags;

        (*errorCallback)(mannErrorReport, errorCallbackContext);
      }
      CryLog("Animation not found: %s in blend from %s to %s", animBlend.transition.animRef.GetAnimName(), animName1, animName2);
      return false;
    }
   }
   return true;
   }
 */

CAnimationDatabase::~CAnimationDatabase()
{
	const uint32 numFragments = m_fragmentList.size();
	for (uint32 i = 0; i < numFragments; i++)
	{
		SFragmentEntry& entry = *m_fragmentList[i];

		if (entry.compiledList)
		{
			const uint32 numTagVars = entry.compiledList->Size();
			for (uint32 t = 0; t < numTagVars; t++)
			{
				TFragmentOptionList& optionList = entry.compiledList->m_values[t];
				for (TFragmentOptionList::iterator optIt = optionList.begin(), optItEnd = optionList.end(); optIt != optItEnd; ++optIt)
				{
					SFragmentOption& opt = *optIt;
					delete opt.fragment;
				}
			}

			delete entry.compiledList;
		}

		delete &entry;
	}

	for (TFragmentBlendDatabase::iterator blDbIt = m_fragmentBlendDB.begin(), blDbItEnd = m_fragmentBlendDB.end(); blDbIt != blDbItEnd; ++blDbIt)
	{
		SFragmentBlendEntry& entry = blDbIt->second;
		for (TFragmentVariantList::iterator vrIt = entry.variantList.begin(), vrItEnd = entry.variantList.end(); vrIt != vrItEnd; ++vrIt)
		{
			SFragmentBlendVariant& variant = *vrIt;
			for (TFragmentBlendList::iterator fragIt = variant.blendList.begin(), fragItEnd = variant.blendList.end(); fragIt != fragItEnd; ++fragIt)
			{
				delete fragIt->pFragment;
			}
		}
	}
}

bool CAnimationDatabase::ValidateFragment(const CFragment* pFragment, const IAnimationSet* animSet, SMannequinErrorReport& errorReport, MannErrorCallback errorCallback, void* errorCallbackContext) const
{
	bool noErrors = true;

	const uint32 numLayers = pFragment->m_animLayers.size();
	for (uint32 l = 0; l < numLayers; l++)
	{
		const TAnimClipSequence& animLayer = pFragment->m_animLayers[l];
		const uint32 numClips = animLayer.size();
		for (uint32 c = 0; c < numClips; c++)
		{
			const SAnimClip& animClip = animLayer[c];
			if (!animClip.animation.IsEmpty())
			{
				int animID = animSet->GetAnimIDByCRC(animClip.animation.animRef.crc);
				if (animID == -1)
				{
					noErrors = false;
					if (errorCallback)
					{
						cry_sprintf(errorReport.error, "Animation not found: %s in layer: %u", animClip.animation.animRef.c_str(), l);

						(*errorCallback)(errorReport, errorCallbackContext);
					}
					CryLog("Animation not found: %s in layer: %u", animClip.animation.animRef.c_str(), l);
				}
			}
		}
	}

	return noErrors;
}

bool CAnimationDatabase::ValidateAnimations(FragmentID fragID, uint32 tagSetID, uint32 numOptions, const SFragTagState& tagState, const IAnimationSet* animSet, MannErrorCallback errorCallback, void* errorCallbackContext) const
{
	bool noErrors = true;

	SMannequinErrorReport mannErrorReport;
	mannErrorReport.errorType = SMannequinErrorReport::Fragment;
	mannErrorReport.fragID = fragID;
	mannErrorReport.tags = tagState;

	for (uint32 o = 0; o < numOptions; o++)
	{
		const CFragment* fragment = GetEntry(fragID, tagSetID, o);

		mannErrorReport.fragOptionID = o;

		noErrors = ValidateFragment(fragment, animSet, mannErrorReport, errorCallback, errorCallbackContext) && noErrors;
	}

	return noErrors;
}

bool CAnimationDatabase::IsDuplicate(const CFragment* pFragmentA, const CFragment* pFragmentB) const
{
	if (pFragmentA == NULL || pFragmentB == NULL)
	{
		return false;
	}

	const uint32 numAnimLayersA = pFragmentA->m_animLayers.size();
	const uint32 numAnimLayersB = pFragmentB->m_animLayers.size();

	const uint32 numProcLayersA = pFragmentA->m_procLayers.size();
	const uint32 numProcLayersB = pFragmentB->m_procLayers.size();

	if (numAnimLayersA != numAnimLayersB || numProcLayersA != numProcLayersB)
	{
		return false;
	}

	for (uint32 l = 0; l < numAnimLayersA; l++)
	{
		const TAnimClipSequence& animLayerA = pFragmentA->m_animLayers[l];
		const TAnimClipSequence& animLayerB = pFragmentB->m_animLayers[l];

		const uint32 numAnimClipsA = animLayerA.size();
		const uint32 numAnimClipsB = animLayerB.size();

		if (numAnimClipsA != numAnimClipsB)
		{
			return false;
		}

		for (uint32 c = 0; c < numAnimClipsA; c++)
		{
			const SAnimClip& animClipA = animLayerA[c];
			const SAnimClip& animClipB = animLayerB[c];

			if (!(animClipA == animClipB))
			{
				return false;
			}
		}
	}

	for (uint32 l = 0; l < numProcLayersA; l++)
	{
		const TProcClipSequence& procLayerA = pFragmentA->m_procLayers[l];
		const TProcClipSequence& procLayerB = pFragmentB->m_procLayers[l];

		const uint32 numProcClipsA = procLayerA.size();
		const uint32 numProcClipsB = procLayerB.size();

		if (numProcClipsA != numProcClipsB)
		{
			return false;
		}

		for (uint32 c = 0; c < numProcClipsA; c++)
		{
			const SProceduralEntry& procClipA = procLayerA[c];
			const SProceduralEntry& procClipB = procLayerB[c];

			if (!(procClipA == procClipB))
			{
				return false;
			}
		}
	}

	return true;
}

bool CAnimationDatabase::Validate(const IAnimationSet* animSet, MannErrorCallback errorCallback, MannErrorCallback warningCallback, void* errorCallbackContext) const
{
	bool noErrors = true;
	const uint32 numFragments = m_fragmentList.size();
	for (uint32 i = 0; i < numFragments; i++)
	{
		const uint32 numTagSets = GetTotalTagSets(i);
		for (uint32 k = 0; k < numTagSets; k++)
		{
			SFragTagState tags;
			uint32 numOptions = GetTagSetInfo(i, k, tags);

			if (animSet)
			{
				noErrors = ValidateAnimations(i, k, numOptions, tags, animSet, errorCallback, errorCallbackContext);
			}

			if (warningCallback)
			{
				for (uint32 m = 0; m < numTagSets; m++)
				{
					if (k != m)
					{
						SFragTagState compareTags;
						uint32 compareNumOptions = GetTagSetInfo(i, m, compareTags);

						if (compareNumOptions == numOptions)
						{
							const CTagDefinition* pFragTagDef = m_pFragDef->GetSubTagDefinition(i);

							if ((m_pTagDef->Contains(tags.globalTags, compareTags.globalTags))
							    && ((pFragTagDef == NULL) || pFragTagDef->Contains(tags.fragmentTags, compareTags.fragmentTags)))
							{
								//fragment uses a parent set of tags compare for unnecessary duplication

								bool allOptionsAreDuplicated = true;
								for (uint32 n = 0; n < compareNumOptions && allOptionsAreDuplicated; n++)
								{
									const CFragment* childFragment = GetEntry(i, k, n);

									bool foundDuplicateOption = false;
									for (uint32 p = 0; (p < compareNumOptions) && !foundDuplicateOption; p++)
									{
										const CFragment* parentFragment = GetEntry(i, m, p);

										foundDuplicateOption = IsDuplicate(childFragment, parentFragment);
									}

									if (!foundDuplicateOption)
									{
										allOptionsAreDuplicated = false;
									}
								}

								if (allOptionsAreDuplicated)
								{
									SMannequinErrorReport mannErrorReport;
									mannErrorReport.errorType = SMannequinErrorReport::Fragment;
									mannErrorReport.fragID = i;
									mannErrorReport.fragOptionID = OPTION_IDX_INVALID;

									CryStackStringT<char, 1024> sTagList;
									CryStackStringT<char, 1024> sFragTagList;

									m_pTagDef->FlagsToTagList(compareTags.globalTags, sTagList);
									if (pFragTagDef)
									{
										pFragTagDef->FlagsToTagList(compareTags.fragmentTags, sFragTagList);
									}

									cry_sprintf(mannErrorReport.error, "Fragment is duplicate of a parent fragment! Parent Tags - Global: '%s' Fragment: '%s'", sTagList.c_str(), sFragTagList.c_str());
									mannErrorReport.tags = tags;

									(*warningCallback)(mannErrorReport, errorCallbackContext);

									noErrors = false;
									break;
								}
							}
						}
					}
				}
			}
		}
	}

	if (animSet)
	{
		//--- Validate blends
		SMannequinErrorReport mannErrorReport;
		mannErrorReport.errorType = SMannequinErrorReport::Blend;
		for (TFragmentBlendDatabase::const_iterator iter = m_fragmentBlendDB.begin(); iter != m_fragmentBlendDB.end(); ++iter)
		{
			const TFragmentVariantList& variantList = iter->second.variantList;

			mannErrorReport.fragID = iter->first.fragFrom;
			mannErrorReport.fragIDTo = iter->first.fragTo;

			const uint32 numVariants = variantList.size();
			for (uint32 i = 0; i < numVariants; i++)
			{
				mannErrorReport.tags = variantList[i].tagsFrom;
				mannErrorReport.tagsTo = variantList[i].tagsTo;
				const TFragmentBlendList& blendList = variantList[i].blendList;
				const uint32 numBlends = blendList.size();
				for (uint32 b = 0; b < numBlends; b++)
				{
					mannErrorReport.fragOptionID = b;
					ValidateFragment(blendList[b].pFragment, animSet, mannErrorReport, errorCallback, errorCallbackContext);
				}
			}
		}
	}

	return noErrors;
}

void CAnimationDatabase::EnumerateFragmentAnimAssets(const CFragment* pFragment, const IAnimationSet* animSet, SAnimAssetReport& assetReport, MannAssetCallback assetCallback, void* callbackContext) const
{
	const uint32 numLayers = pFragment->m_animLayers.size();
	for (uint32 l = 0; l < numLayers; l++)
	{
		const TAnimClipSequence& animLayer = pFragment->m_animLayers[l];
		const uint32 numClips = animLayer.size();
		for (uint32 c = 0; c < numClips; c++)
		{
			const SAnimClip& animClip = animLayer[c];
			if (!animClip.animation.IsEmpty())
			{
				assetReport.pAnimName = animClip.animation.animRef.c_str();
				assetReport.pAnimPath = NULL;
				assetReport.animID = -1;
				if (animSet)
				{
					int animID = animSet->GetAnimIDByCRC(animClip.animation.animRef.crc);
					assetReport.animID = animID;
					assetReport.pAnimPath = animSet->GetFilePathByID(animID);
				}

				(*assetCallback)(assetReport, callbackContext);
			}
		}
	}
}

void CAnimationDatabase::EnumerateAnimAssets(const IAnimationSet* animSet, MannAssetCallback assetCallback, void* callbackContext) const
{
	SAnimAssetReport mannAssetReport;

	const uint32 numFragments = m_fragmentList.size();
	for (uint32 fragID = 0; fragID < numFragments; fragID++)
	{
		const uint32 numTagSets = GetTotalTagSets(fragID);
		for (uint32 k = 0; k < numTagSets; k++)
		{
			SFragTagState tags;
			uint32 numOptions = GetTagSetInfo(fragID, k, tags);

			mannAssetReport.fragID = fragID;
			mannAssetReport.tags = tags;

			for (uint32 o = 0; o < numOptions; o++)
			{
				const CFragment* fragment = GetEntry(fragID, k, o);

				mannAssetReport.fragOptionID = o;

				EnumerateFragmentAnimAssets(fragment, animSet, mannAssetReport, assetCallback, callbackContext);
			}
		}
	}

	//--- Enumerate blends
	for (TFragmentBlendDatabase::const_iterator iter = m_fragmentBlendDB.begin(); iter != m_fragmentBlendDB.end(); ++iter)
	{
		const TFragmentVariantList& variantList = iter->second.variantList;

		mannAssetReport.fragID = iter->first.fragFrom;
		mannAssetReport.fragIDTo = iter->first.fragTo;

		const uint32 numVariants = variantList.size();
		for (uint32 i = 0; i < numVariants; i++)
		{
			mannAssetReport.tags = variantList[i].tagsFrom;
			mannAssetReport.tagsTo = variantList[i].tagsTo;
			const TFragmentBlendList& blendList = variantList[i].blendList;
			const uint32 numBlends = blendList.size();
			for (uint32 b = 0; b < numBlends; b++)
			{
				mannAssetReport.fragOptionID = b;
				EnumerateFragmentAnimAssets(blendList[b].pFragment, animSet, mannAssetReport, assetCallback, callbackContext);
			}
		}
	}

}

bool CAnimationDatabase::SCompareBlendVariantFunctor::operator()(const CAnimationDatabase::SFragmentBlendVariant& lhs, const CAnimationDatabase::SFragmentBlendVariant& rhs)
{
	uint32 bits1 = m_tagDefs.RateTagState(lhs.tagsFrom.globalTags) + (m_pFragTagDefsFrom ? m_pFragTagDefsFrom->RateTagState(lhs.tagsFrom.fragmentTags) : 0);
	uint32 bits2 = m_tagDefs.RateTagState(rhs.tagsFrom.globalTags) + (m_pFragTagDefsFrom ? m_pFragTagDefsFrom->RateTagState(rhs.tagsFrom.fragmentTags) : 0);
	bits1 += m_tagDefs.RateTagState(lhs.tagsTo.globalTags) + (m_pFragTagDefsTo ? m_pFragTagDefsTo->RateTagState(lhs.tagsTo.fragmentTags) : 0);
	bits2 += m_tagDefs.RateTagState(rhs.tagsTo.globalTags) + (m_pFragTagDefsTo ? m_pFragTagDefsTo->RateTagState(rhs.tagsTo.fragmentTags) : 0);

	return (bits1 > bits2);
}

SFragmentBlendUid CAnimationDatabase::AddBlendInternal(FragmentID fragmentIDFrom, FragmentID fragmentIDTo, const SFragTagState& tagsFrom, const SFragTagState& tagsTo, const SFragmentBlend& fragBlend)
{
	const SFragmentBlendID blendID = { fragmentIDFrom, fragmentIDTo };

	TFragmentBlendDatabase::iterator iter = m_fragmentBlendDB.find(blendID);
	if (iter == m_fragmentBlendDB.end())
	{
		std::pair<TFragmentBlendDatabase::iterator, bool> insert = m_fragmentBlendDB.insert(std::make_pair(blendID, SFragmentBlendEntry()));
		iter = insert.first;
	}

	SFragmentBlendEntry& blendEntry = iter->second;

	const uint32 numVariants = blendEntry.variantList.size();
	SFragmentBlendVariant* variant = NULL;
	for (uint32 i = 0; i < numVariants; i++)
	{
		SFragmentBlendVariant& variantIt = blendEntry.variantList[i];
		if ((variantIt.tagsFrom == tagsFrom) && (variantIt.tagsTo == tagsTo))
		{
			variant = &variantIt;
			break;
		}
	}

	bool addedVariant = false;
	if (!variant)
	{
		//--- Insert a new variant
		blendEntry.variantList.resize(numVariants + 1);
		variant = &blendEntry.variantList[numVariants];
		variant->tagsFrom = tagsFrom;
		variant->tagsTo = tagsTo;

		addedVariant = true;
	}

	variant->blendList.push_back(fragBlend);
	std::sort(variant->blendList.begin(), variant->blendList.end());

	if (addedVariant && m_autoSort)
	{
		//--- Re-sort based on priority
		const CTagDefinition* fragFromDef = (fragmentIDFrom != FRAGMENT_ID_INVALID) ? m_pFragDef->GetSubTagDefinition(fragmentIDFrom) : NULL;
		const CTagDefinition* fragToDef = (fragmentIDTo != FRAGMENT_ID_INVALID) ? m_pFragDef->GetSubTagDefinition(fragmentIDTo) : NULL;
		SCompareBlendVariantFunctor comparisonFunctor(*m_pTagDef, fragFromDef, fragToDef);
		std::stable_sort(blendEntry.variantList.begin(), blendEntry.variantList.end(), comparisonFunctor);
	}

	return fragBlend.uid;
}

SFragmentBlendUid CAnimationDatabase::AddBlend(FragmentID fragmentIDFrom, FragmentID fragmentIDTo, const SFragTagState& tagFrom, const SFragTagState& tagTo, const SFragmentBlend& fragBlend)
{
	CFragment* internalFragment = new CFragment();
	*internalFragment = *fragBlend.pFragment;

	SFragmentBlend fragBlendCopy = fragBlend;
	fragBlendCopy.pFragment = internalFragment;

	return AddBlendInternal(fragmentIDFrom, fragmentIDTo, tagFrom, tagTo, fragBlendCopy);
}

uint32 CAnimationDatabase::GetNumBlends(FragmentID fragmentIDFrom, FragmentID fragmentIDTo, const SFragTagState& tagFrom, const SFragTagState& tagTo) const
{
	const SFragmentBlendVariant* variant = GetVariant(fragmentIDFrom, fragmentIDTo, tagFrom, tagTo);

	if (variant)
	{
		return variant->blendList.size();
	}

	return 0;
}

const SFragmentBlend* CAnimationDatabase::GetBlend(FragmentID fragmentIDFrom, FragmentID fragmentIDTo, const SFragTagState& tagFrom, const SFragTagState& tagTo, uint32 blendNum) const
{
	const SFragmentBlendVariant* variant = GetVariant(fragmentIDFrom, fragmentIDTo, tagFrom, tagTo);

	if (variant && (variant->blendList.size() > blendNum))
	{
		return &variant->blendList[blendNum];
	}

	return NULL;
}

const SFragmentBlend* CAnimationDatabase::GetBlend(FragmentID fragmentIDFrom, FragmentID fragmentIDTo, const SFragTagState& tagFrom, const SFragTagState& tagTo, SFragmentBlendUid blendUid) const
{
	const SFragmentBlendVariant* variant = GetVariant(fragmentIDFrom, fragmentIDTo, tagFrom, tagTo);

	return variant ? variant->FindBlend(blendUid) : NULL;
}

void CAnimationDatabase::SetBlend(FragmentID fragmentIDFrom, FragmentID fragmentIDTo, const SFragTagState& tagFrom, const SFragTagState& tagTo, SFragmentBlendUid blendUid, const SFragmentBlend& newFragmentBlend)
{
	CAnimationDatabase::SFragmentBlendVariant* variant = GetVariant(fragmentIDFrom, fragmentIDTo, tagFrom, tagTo);

	if (!variant)
		return;

	SFragmentBlend* pFragmentBlend = variant->FindBlend(blendUid);
	CRY_ASSERT(pFragmentBlend);

	const bool sortIsNeeded = (pFragmentBlend->selectTime != newFragmentBlend.selectTime);

	CFragment* pOldFragment = pFragmentBlend->pFragment;
	*pFragmentBlend = newFragmentBlend;
	pFragmentBlend->pFragment = new CFragment(*newFragmentBlend.pFragment);

	delete(pOldFragment);

	if (sortIsNeeded)
	{
		std::sort(variant->blendList.begin(), variant->blendList.end());
	}
}

void CAnimationDatabase::DeleteBlend(FragmentID fragmentIDFrom, FragmentID fragmentIDTo, const SFragTagState& tagFrom, const SFragTagState& tagTo, SFragmentBlendUid blendUid)
{
	CAnimationDatabase::SFragmentBlendVariant* variant = GetVariant(fragmentIDFrom, fragmentIDTo, tagFrom, tagTo);

	if (variant)
	{
		const uint32 blendNum = variant->FindBlendIndexForUid(blendUid);
		if (blendNum < variant->blendList.size())
		{
			delete variant->blendList[blendNum].pFragment;
			variant->blendList.erase(variant->blendList.begin() + blendNum);
		}

		if (variant->blendList.empty())
		{
			//--- Clear out empty variants
			const SFragmentBlendID blendID = { fragmentIDFrom, fragmentIDTo };
			TFragmentBlendDatabase::iterator iter = m_fragmentBlendDB.find(blendID);
			SFragmentBlendEntry& blendEntry = iter->second;

			const uint32 numVariants = blendEntry.variantList.size();
			for (TFragmentVariantList::iterator _iter = blendEntry.variantList.begin(); _iter != blendEntry.variantList.end(); ++_iter)
			{
				SFragmentBlendVariant& variantDel = *_iter;

				if (&variantDel == variant)
				{
					blendEntry.variantList.erase(_iter);
					break;
				}
			}
		}
	}
}

const char* CAnimationDatabase::FindSubADBFilenameForIDInternal(FragmentID fragmentID, const SSubADB& pSubADB) const
{
	// Search FragIDs
	if (std::find(pSubADB.vFragIDs.begin(), pSubADB.vFragIDs.end(), fragmentID) != pSubADB.vFragIDs.end())
		return pSubADB.filename.c_str();

	// Search subADBs
	for (TSubADBList::const_iterator itSubADB = pSubADB.subADBs.begin(); itSubADB != pSubADB.subADBs.end(); ++itSubADB)
	{
		const char* fName = FindSubADBFilenameForIDInternal(fragmentID, *itSubADB);
		if (fName != 0 && strlen(fName) > 0)
			return fName;
	}

	// Nothing
	return NULL;
}

const char* CAnimationDatabase::FindSubADBFilenameForID(FragmentID fragmentID) const
{
	// Find SubADB with the filename
	for (TSubADBList::const_iterator itSubADB = m_subADBs.begin(); itSubADB != m_subADBs.end(); ++itSubADB)
	{
		const char* fName = FindSubADBFilenameForIDInternal(fragmentID, *itSubADB);
		if (fName != 0 && strlen(fName) > 0)
			return fName;
	}

	// Return me
	return m_filename.c_str();
}

bool CAnimationDatabase::RemoveSubADBFragmentFilterInternal(FragmentID fragmentID, SSubADB& subADB)
{
	bool bRemoved = false;

	SSubADB::TFragIDList::iterator itFID = std::find(subADB.vFragIDs.begin(), subADB.vFragIDs.end(), fragmentID);
	if (itFID != subADB.vFragIDs.end())
	{
		subADB.vFragIDs.erase(itFID);
		bRemoved = true;
	}

	for (TSubADBList::iterator itADB = subADB.subADBs.begin(); itADB != subADB.subADBs.end(); ++itADB)
		bRemoved = bRemoved || RemoveSubADBFragmentFilterInternal(fragmentID, *itADB);

	return bRemoved;
}

bool CAnimationDatabase::RemoveSubADBFragmentFilter(FragmentID fragmentID)
{
	bool bRemoved = false;
	for (TSubADBList::iterator itADB = m_subADBs.begin(); itADB != m_subADBs.end(); ++itADB)
		if (RemoveSubADBFragmentFilterInternal(fragmentID, *itADB))
			bRemoved = true;

	return bRemoved;
}

const CAnimationDatabase::SSubADB* CAnimationDatabase::FindSubADB(const TSubADBList& subAdbList, const char* szSubADBFilename, bool recursive)
{
	CRY_ASSERT(szSubADBFilename);

	for (TSubADBList::const_iterator cit = subAdbList.begin(); cit != subAdbList.end(); ++cit)
	{
		const SSubADB& subADB = *cit;
		if (subADB.IsSubADB(szSubADBFilename))
		{
			return &subADB;
		}
	}

	if (recursive)
	{
		for (TSubADBList::const_iterator cit = subAdbList.begin(); cit != subAdbList.end(); ++cit)
		{
			const SSubADB& subADB = *cit;
			const SSubADB* pFoundSubADB = FindSubADB(subADB.subADBs, szSubADBFilename, true);
			if (pFoundSubADB)
			{
				return pFoundSubADB;
			}
		}
	}

	return NULL;
}

CAnimationDatabase::SSubADB* CAnimationDatabase::FindSubADB(const char* szSubADBFilename, bool recursive)
{
	return const_cast<SSubADB*>(FindSubADB(m_subADBs, szSubADBFilename, recursive));
}

const CAnimationDatabase::SSubADB* CAnimationDatabase::FindSubADB(const char* szSubADBFilename, bool recursive) const
{
	return FindSubADB(m_subADBs, szSubADBFilename, recursive);
}

bool CAnimationDatabase::AddSubADBFragmentFilterInternal(const string& sADBFileName, FragmentID fragmentID, SSubADB& subADB)
{
	if (sADBFileName.compareNoCase(subADB.filename) == 0)
	{
		SSubADB::TFragIDList::iterator itFID = std::find(subADB.vFragIDs.begin(), subADB.vFragIDs.end(), fragmentID);
		if (itFID == subADB.vFragIDs.end())
			subADB.vFragIDs.push_back(fragmentID);
		return true;
	}

	for (TSubADBList::iterator itADB = subADB.subADBs.begin(); itADB != subADB.subADBs.end(); ++itADB)
		if (AddSubADBFragmentFilterInternal(sADBFileName, fragmentID, *itADB))
			return true;

	return false;
}

bool CAnimationDatabase::AddSubADBFragmentFilter(const string& sADBFileName, FragmentID fragmentID)
{
	// If the ADB's the root, just return true since it'll be added to the root~!
	if (sADBFileName.compareNoCase(m_filename) == 0)
		return true;

	for (TSubADBList::iterator itADB = m_subADBs.begin(); itADB != m_subADBs.end(); ++itADB)
		if (AddSubADBFragmentFilterInternal(sADBFileName, fragmentID, *itADB))
			return true;

	// Got here, not found the file, so add it
	m_subADBs.push_back(SSubADB());
	SSubADB& newSubADB = m_subADBs.back();
	newSubADB.tags = TagState(TAG_STATE_EMPTY);
	newSubADB.comparisonMask = m_pTagDef->GenerateMask(newSubADB.tags);
	newSubADB.filename = sADBFileName;
	newSubADB.pTagDef = m_pTagDef;
	newSubADB.pFragDef = m_pFragDef;
	newSubADB.knownTags = TAG_STATE_FULL;
	newSubADB.vFragIDs.push_back(fragmentID);
	return false;
}

void CAnimationDatabase::FillMiniSubADB(SMiniSubADB& outMiniSub, const SSubADB& inSub) const
{
	outMiniSub.vSubADBs.push_back(SMiniSubADB());
	SMiniSubADB& miniSub = outMiniSub.vSubADBs.back();
	miniSub.tags = inSub.tags;
	miniSub.filename = inSub.filename;
	miniSub.pFragDef = inSub.pFragDef;

	for (SSubADB::TFragIDList::const_iterator itFragID = inSub.vFragIDs.begin(); itFragID != inSub.vFragIDs.end(); ++itFragID)
	{
		miniSub.vFragIDs.push_back(*itFragID);
	}

	for (TSubADBList::const_iterator subitADB = inSub.subADBs.begin(); subitADB != inSub.subADBs.end(); ++subitADB)
	{
		FillMiniSubADB(miniSub, *subitADB);
	}
}

void CAnimationDatabase::GetSubADBFragmentFilters(SMiniSubADB::TSubADBArray& outList) const
{
	outList.clear();
	for (TSubADBList::const_iterator itADB = m_subADBs.begin(); itADB != m_subADBs.end(); ++itADB)
	{
		outList.push_back(SMiniSubADB());
		SMiniSubADB& miniSub = outList.back();
		miniSub.tags = (*itADB).tags;
		miniSub.filename = (*itADB).filename;
		miniSub.pFragDef = (*itADB).pFragDef;

		for (SSubADB::TFragIDList::const_iterator itFragID = (*itADB).vFragIDs.begin(); itFragID != (*itADB).vFragIDs.end(); ++itFragID)
		{
			miniSub.vFragIDs.push_back(*itFragID);
		}

		for (TSubADBList::const_iterator subitADB = (*itADB).subADBs.begin(); subitADB != (*itADB).subADBs.end(); ++subitADB)
		{
			FillMiniSubADB(miniSub, *subitADB);
		}
	}
}

bool CAnimationDatabase::AddSubADBTagFilterInternal(const string& sParentFilename, const string& sADBFileName, const TagState& tag, SSubADB& subADB)
{
	const string compSubFilename = subADB.filename.Right(sADBFileName.size());
	if (compSubFilename.compareNoCase(sADBFileName) == 0)
	{
		subADB.tags = tag;
		subADB.comparisonMask = subADB.pTagDef->GenerateMask(subADB.tags);
		return true;
	}

	// is this the parent we're looking for?
	const string compFilename = subADB.filename.Right(sParentFilename.size());
	if (compFilename.compareNoCase(sParentFilename) == 0)
	{
		subADB.subADBs.push_back(SSubADB());
		SSubADB& newSubADB = subADB.subADBs.back();
		newSubADB.tags = tag;
		newSubADB.comparisonMask = subADB.pTagDef->GenerateMask(newSubADB.tags);
		newSubADB.filename = sADBFileName;
		newSubADB.pTagDef = subADB.pTagDef;
		newSubADB.pFragDef = subADB.pFragDef;
		newSubADB.knownTags = TAG_STATE_FULL;
		newSubADB.vFragIDs.clear();
		return true;
	}

	for (TSubADBList::iterator itADB = subADB.subADBs.begin(); itADB != subADB.subADBs.end(); ++itADB)
		if (AddSubADBTagFilterInternal(sParentFilename, sADBFileName, tag, *itADB))
			return true;

	return false;
}

bool CAnimationDatabase::AddSubADBTagFilter(const string& sParentFilename, const string& sADBFileName, const TagState& tag)
{
	// If the ADB's the root, just return true since it'll be added to the root~!
	if (sADBFileName.compareNoCase(m_filename) == 0)
		return true;

	for (TSubADBList::iterator itADB = m_subADBs.begin(); itADB != m_subADBs.end(); ++itADB)
		if (AddSubADBTagFilterInternal(sParentFilename, sADBFileName, tag, *itADB))
			return true;

	// Got here, not found the file, so add it
	m_subADBs.push_back(SSubADB());
	SSubADB& newSubADB = m_subADBs.back();
	newSubADB.tags = tag;
	newSubADB.comparisonMask = m_pTagDef->GenerateMask(newSubADB.tags);
	newSubADB.filename = sADBFileName;
	newSubADB.pTagDef = m_pTagDef;
	newSubADB.pFragDef = m_pFragDef;
	newSubADB.knownTags = TAG_STATE_FULL;
	newSubADB.vFragIDs.clear();
	return false;
}

bool CAnimationDatabase::MoveSubADBFilterInternal(const string& sADBFileName, SSubADB& subADB, const bool bMoveUp)
{
	for (TSubADBList::iterator subitADB = subADB.subADBs.begin(); subitADB != subADB.subADBs.end(); ++subitADB)
	{
		const string compFilename = (*subitADB).filename.Right(sADBFileName.size());
		if (compFilename.compareNoCase(sADBFileName) == 0)
		{
			if (bMoveUp)
			{
				if (subADB.subADBs.begin() != subitADB)
					std::iter_swap(subitADB, subitADB - 1);
			}
			else  // move down
			{
				if (subADB.subADBs.end() - 1 != subitADB)
					std::iter_swap(subitADB, subitADB + 1);
			}
			return true;
		}
		else
		{
			if (MoveSubADBFilterInternal(sADBFileName, *subitADB, bMoveUp))
				return true;
		}
	}
	return false;
}

bool CAnimationDatabase::MoveSubADBFilter(const string& sADBFileName, const bool bMoveUp)
{
	for (TSubADBList::iterator itADB = m_subADBs.begin(); itADB != m_subADBs.end(); ++itADB)
	{
		const string compFilename = (*itADB).filename.Right(sADBFileName.size());
		if (compFilename.compareNoCase(sADBFileName) == 0)
		{
			if (bMoveUp)
			{
				if (m_subADBs.begin() != itADB)
					std::iter_swap(itADB, itADB - 1);
			}
			else  // move down
			{
				if (m_subADBs.end() - 1 != itADB)
					std::iter_swap(itADB, itADB + 1);
			}
			return true;
		}
		else
		{
			if (MoveSubADBFilterInternal(sADBFileName, *itADB, bMoveUp))
				return true;
		}
	}

	return false;
}

bool CAnimationDatabase::DeleteSubADBFilterInternal(const string& sADBFileName, SSubADB& subADB)
{
	for (TSubADBList::iterator subitADB = subADB.subADBs.begin(); subitADB != subADB.subADBs.end(); ++subitADB)
	{
		const string compFilename = (*subitADB).filename.Right(sADBFileName.size());
		if (compFilename.compareNoCase(sADBFileName) == 0)
		{
			subADB.subADBs.erase(subitADB);
			return true;
		}
		else
		{
			if (DeleteSubADBFilterInternal(sADBFileName, *subitADB))
				return true;
		}
	}
	return false;
}

bool CAnimationDatabase::DeleteSubADBFilter(const string& sADBFileName)
{
	if (sADBFileName.compareNoCase(m_filename) == 0)
		return false;

	for (TSubADBList::iterator itADB = m_subADBs.begin(); itADB != m_subADBs.end(); ++itADB)
	{
		const string compFilename = (*itADB).filename.Right(sADBFileName.size());
		if (compFilename.compareNoCase(sADBFileName) == 0)
		{
			m_subADBs.erase(itADB);
			return true;
		}
		else
		{
			if (DeleteSubADBFilterInternal(sADBFileName, *itADB))
				return true;
		}
	}

	return false;
}

bool CAnimationDatabase::ClearSubADBFilterInternal(const string& sADBFileName, SSubADB& subADB)
{
	for (TSubADBList::iterator subitADB = subADB.subADBs.begin(); subitADB != subADB.subADBs.end(); ++subitADB)
	{
		const string compFilename = (*subitADB).filename.Right(sADBFileName.size());
		if (compFilename.compareNoCase(sADBFileName) == 0)
		{
			(*subitADB).tags = TagState(TAG_STATE_EMPTY);
			(*subitADB).vFragIDs.clear();
			return true;
		}
		else
		{
			if (ClearSubADBFilterInternal(sADBFileName, *subitADB))
				return true;
		}
	}
	return false;
}

bool CAnimationDatabase::ClearSubADBFilter(const string& sADBFileName)
{
	if (sADBFileName.compareNoCase(m_filename) == 0)
		return false;

	for (TSubADBList::iterator itADB = m_subADBs.begin(); itADB != m_subADBs.end(); ++itADB)
	{
		const string compFilename = (*itADB).filename.Right(sADBFileName.size());
		if (compFilename.compareNoCase(sADBFileName) == 0)
		{
			(*itADB).tags = TagState(TAG_STATE_EMPTY);
			(*itADB).vFragIDs.clear();
			return true;
		}
		else
		{
			if (ClearSubADBFilterInternal(sADBFileName, *itADB))
				return true;
		}
	}

	return false;
}

CAnimationDatabase::SFragmentBlendVariant* CAnimationDatabase::GetVariant(FragmentID fragmentIDFrom, FragmentID fragmentIDTo, const SFragTagState& tagFrom, const SFragTagState& tagTo) const
{
	const SFragmentBlendID blendID = { fragmentIDFrom, fragmentIDTo };
	TFragmentBlendDatabase::const_iterator iter = m_fragmentBlendDB.find(blendID);

	if (iter != m_fragmentBlendDB.end())
	{
		SFragmentBlendEntry& entry = (SFragmentBlendEntry&)iter->second;
		const uint32 numVariants = entry.variantList.size();
		for (uint32 i = 0; i < numVariants; i++)
		{
			SFragmentBlendVariant& variant = entry.variantList[i];
			if ((tagFrom == variant.tagsFrom) && (tagTo == variant.tagsTo))
			{
				return &variant;
			}
		}
	}

	return NULL;
}

const CAnimationDatabase::SFragmentBlendVariant* CAnimationDatabase::FindBestVariant(const SFragmentBlendID& fragmentBlendID, const SFragTagState& tagFrom, const SFragTagState& tagTo, const TagState& requiredTags, FragmentID& outFragmentFrom, FragmentID& outFragmentTo) const
{
	TFragmentBlendDatabase::const_iterator iter = m_fragmentBlendDB.find(fragmentBlendID);

	if (iter != m_fragmentBlendDB.end())
	{
		const CTagDefinition* fragFromDef = (fragmentBlendID.fragFrom != FRAGMENT_ID_INVALID) ? m_pFragDef->GetSubTagDefinition(fragmentBlendID.fragFrom) : NULL;
		const CTagDefinition* fragToDef = (fragmentBlendID.fragTo != FRAGMENT_ID_INVALID) ? m_pFragDef->GetSubTagDefinition(fragmentBlendID.fragTo) : NULL;

		outFragmentFrom = iter->first.fragFrom;
		outFragmentTo = iter->first.fragTo;

		TagState requiredComparisonMask = m_pTagDef->GenerateMask(requiredTags);

		//--- Found an entry, search for the best match
		SFragmentBlendEntry& entry = (SFragmentBlendEntry&)iter->second;
		const uint32 numVariants = entry.variantList.size();
		for (uint32 j = 0; j < numVariants; j++)
		{
			SFragmentBlendVariant& variant = entry.variantList[j];
			bool valid = (m_pTagDef->Contains(tagFrom.globalTags, variant.tagsFrom.globalTags) && m_pTagDef->Contains(tagTo.globalTags, variant.tagsTo.globalTags));
			valid = valid && m_pTagDef->Contains(variant.tagsFrom.globalTags, requiredTags, requiredComparisonMask);
			valid = valid && m_pTagDef->Contains(variant.tagsTo.globalTags, requiredTags, requiredComparisonMask);
			valid = valid && (!fragFromDef || fragFromDef->Contains(tagFrom.fragmentTags, variant.tagsFrom.fragmentTags));
			valid = valid && (!fragToDef || fragToDef->Contains(tagTo.fragmentTags, variant.tagsTo.fragmentTags));
			if (valid)
			{
				return &variant;
			}
		}
	}

	return NULL;
}

void CAnimationDatabase::FindBestBlendInVariant(const SFragmentBlendVariant& variant, const SBlendQuery& blendQuery, SBlendQueryResult& result) const
{
	const uint32 numBlends = variant.blendList.size();

	result.tagStateFrom = variant.tagsFrom;
	result.tagStateTo = variant.tagsTo;

	if (blendQuery.forceBlendUid.IsValid())
	{
		const uint32 blendIdx = variant.FindBlendIndexForUid(blendQuery.forceBlendUid);
		if (blendIdx < variant.blendList.size())
		{
			const SFragmentBlend& fragBlend = variant.blendList[blendIdx];
			result.pFragmentBlend = &fragBlend;
			result.blendIdx = blendIdx;
			result.blendUid = fragBlend.uid;
			result.selectTime = fragBlend.selectTime;
			return;
		}
	}

	for (uint32 i = 0; i < numBlends; i++)
	{
		const SFragmentBlend& fragBlend = variant.blendList[i];
		const float sourceTime = (fragBlend.flags & SFragmentBlend::CycleLocked) ? blendQuery.prevNormalisedTime : ((fragBlend.flags & SFragmentBlend::Cyclic) ? blendQuery.normalisedTime : blendQuery.fragmentTime);
		if ((result.pFragmentBlend == NULL) || (sourceTime >= fragBlend.selectTime))
		{
			result.pFragmentBlend = &fragBlend;
			result.blendIdx = i;
			result.blendUid = fragBlend.uid;
			result.selectTime = fragBlend.selectTime;
		}
	}
}

//----------------------------------------------------------------------------
//	Selects the best matching variant from fragFrom to fragTo
//	If it fails to match tags at all then it will drop back to "any to fragTo" (an entry transition),
//	"fragFrom to any" (an exit transition) and finally "any to any" (a pure tag based transition)
//----------------------------------------------------------------------------
void CAnimationDatabase::FindBestBlends(const SBlendQuery& blendQuery, SBlendQueryResult& result1, SBlendQueryResult& result2) const
{
	const uint32 numVariants = 4;
	const SFragmentBlendID blendIDOptions[numVariants] = {
		{ blendQuery.fragmentFrom, blendQuery.fragmentTo }, { blendQuery.fragmentFrom, FRAGMENT_ID_INVALID }, { FRAGMENT_ID_INVALID, blendQuery.fragmentTo }, { FRAGMENT_ID_INVALID, FRAGMENT_ID_INVALID }
	};
	bool viableVariants[4] = { false, false, false, false };

	//--- Limit transitions based on whether our from or to are primary installations (as apposed to enslaved scopes)
	if (blendQuery.IsFlagSet(SBlendQuery::toInstalled))
	{
		viableVariants[0] = (blendQuery.fragmentFrom != FRAGMENT_ID_INVALID) && (blendQuery.fragmentTo != FRAGMENT_ID_INVALID);
		viableVariants[2] = (blendQuery.fragmentTo != FRAGMENT_ID_INVALID);
		viableVariants[3] = true;
	}
	if (blendQuery.IsFlagSet(SBlendQuery::fromInstalled))
	{
		viableVariants[0] = (blendQuery.fragmentFrom != FRAGMENT_ID_INVALID) && (blendQuery.fragmentTo != FRAGMENT_ID_INVALID);
		viableVariants[1] = (blendQuery.fragmentFrom != FRAGMENT_ID_INVALID) && ((blendQuery.flags & SBlendQuery::higherPriority) == 0);
	}

	const SFragmentBlendVariant* pVariant1 = NULL;
	const SFragmentBlendVariant* pVariant2 = NULL;

	for (uint32 i = 0; i < numVariants; i++)
	{
		if (viableVariants[i])
		{
			pVariant1 = FindBestVariant(blendIDOptions[i], blendQuery.tagStateFrom, blendQuery.tagStateTo, blendQuery.additionalTags, result1.fragmentFrom, result1.fragmentTo);

			if (pVariant1)
			{
				if ((i == 1) && viableVariants[2])
				{
					//--- This is exiting from the previous fragment, we can apply an entry to the new one if one exists too
					pVariant2 = FindBestVariant(blendIDOptions[2], blendQuery.tagStateFrom, blendQuery.tagStateTo, blendQuery.additionalTags, result2.fragmentFrom, result2.fragmentTo);
				}
				break;
			}
		}
	}

	if (pVariant1)
	{
		FindBestBlendInVariant(*pVariant1, blendQuery, result1);
	}
	if (pVariant2)
	{
		FindBestBlendInVariant(*pVariant2, blendQuery, result2);
	}
}

void CAnimationDatabase::AdjustSubADBListAfterFragmentIDDeletion(SSubADB::TSubADBList& subADBs, const FragmentID fragmentID)
{
	for (SSubADB::TSubADBList::iterator itSubADB = subADBs.begin(); itSubADB != subADBs.end(); ++itSubADB)
	{
		for (SSubADB::TFragIDList::iterator itFragID = itSubADB->vFragIDs.begin(); itFragID != itSubADB->vFragIDs.end(); )
		{
			if (*itFragID == fragmentID)
			{
				// Remove direct references to the removed fragmentID.
				// Note that this does not remove the sub ADB rule, it just leaves a subADB rule with less fragmentIDs.
				itFragID = itSubADB->vFragIDs.erase(itFragID);
			}
			else
			{
				// Adjust any references to fragmentIDs above the deleted one
				if (*itFragID > fragmentID)
				{
					--(*itFragID);
				}
				++itFragID;
			}
		}

		AdjustSubADBListAfterFragmentIDDeletion(itSubADB->subADBs, fragmentID);
	}
}

void CAnimationDatabase::DeleteFragmentID(FragmentID fragmentID)
{
	m_fragmentList.erase(m_fragmentList.begin() + fragmentID);

	// Adjust blends
	{
		TFragmentBlendDatabase newBlendDb;

		for (TFragmentBlendDatabase::iterator it = m_fragmentBlendDB.begin(); it != m_fragmentBlendDB.end(); ++it)
		{
			const SFragmentBlendID& blendID = it->first;

			// Drop any blends that refer to the deleted fragmentID
			if ((blendID.fragFrom != fragmentID) && (blendID.fragTo != fragmentID))
			{
				SFragmentBlendID newBlendID;
				newBlendID.fragFrom = blendID.fragFrom;
				newBlendID.fragTo = blendID.fragTo;

				// Adjust any references to fragmentIDs above the deleted one
				if (newBlendID.fragFrom > fragmentID)
					newBlendID.fragFrom--;
				if (newBlendID.fragTo > fragmentID)
					newBlendID.fragTo--;

				newBlendDb[newBlendID] = it->second;
			}
		}

		// Swap the new one for the old one
		m_fragmentBlendDB = newBlendDb;
	}

	// Adjust fragmentIDs in subADB definitions
	AdjustSubADBListAfterFragmentIDDeletion(m_subADBs, fragmentID);
}

void CAnimationDatabase::QueryUsedTags(const FragmentID fragmentID, const SFragTagState& filter, SFragTagState& usedTags) const
{
	const uint32 numFragmentTypes = m_fragmentList.size();
	if (fragmentID < numFragmentTypes)
	{
		SFragmentEntry& fragmentEntry = *m_fragmentList[fragmentID];
		const CTagDefinition* pFragTagDef = m_pFragDef->GetSubTagDefinition(fragmentID);
		fragmentEntry.tagSetList.QueryUsedTags(usedTags, filter, m_pTagDef, pFragTagDef);
	}
}
