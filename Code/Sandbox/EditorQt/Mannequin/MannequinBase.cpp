// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryGame/IGameFramework.h>
#include <ICryMannequinEditor.h>
#include "MannequinBase.h"
#include "FragmentTrack.h"
#include "SequencerNode.h"
#include "SequencerSequence.h"
#include "MannequinDialog.h"

SScopeContextData* SMannequinContexts::GetContextDataForID(const SFragTagState& fragTagState, FragmentID fragID, uint32 contextID, EMannequinEditorMode editorMode)
{
	const CTagDefinition* pFragTagDef = (fragID != FRAGMENT_ID_INVALID) ? m_controllerDef->GetFragmentTagDef(fragID) : NULL;

	SScopeContextData* defaultRet = NULL;
	uint32 numContexts = m_contextData.size();
	for (uint32 i = 0; i < numContexts; i++)
	{
		SScopeContextData& contextData = m_contextData[i];
		if (contextData.contextID == contextID)
		{
			const bool hasTags = (contextData.tags != TAG_STATE_EMPTY) || !contextData.fragTags.empty();
			const bool matchesTags = (contextData.tags == TAG_STATE_EMPTY) || m_controllerDef->m_tags.Contains(fragTagState.globalTags, contextData.tags);
			bool matchesFragTags = true;

			if (!contextData.fragTags.empty())
			{
				TagState filteredFragTags;

				if (pFragTagDef && pFragTagDef->TagListToFlags(contextData.fragTags.c_str(), filteredFragTags))
				{
					matchesFragTags = pFragTagDef->Contains(fragTagState.fragmentTags, filteredFragTags);
				}
				else
				{
					matchesFragTags = false;
				}
			}

			if (hasTags && matchesTags && matchesFragTags)
			{
				return &contextData;
			}
			else if ((editorMode == eMEM_Previewer || editorMode == eMEM_TransitionEditor) && (contextData.viewData[editorMode].enabled && !hasTags))
			{
				//--- Maintain the existing selected contexts in the Previewer
				defaultRet = &contextData;
			}
			else if ((editorMode == eMEM_FragmentEditor) && contextData.startActive)
			{
				//--- Default to the start active contexts in the Fragment Editor
				defaultRet = &contextData;
			}
		}
	}

	return defaultRet;
}

SScopeContextData* SMannequinContexts::GetContextDataForID(TagState state, uint32 contextID, EMannequinEditorMode editorMode)
{
	SScopeContextData* defaultRet = NULL;
	uint32 numContexts = m_contextData.size();
	for (uint32 i = 0; i < numContexts; i++)
	{
		SScopeContextData& contextData = m_contextData[i];
		if (contextData.contextID == contextID)
		{
			const bool hasTags = (contextData.tags != TAG_STATE_EMPTY);
			if (hasTags && m_controllerDef->m_tags.Contains(state, contextData.tags))
			{
				return &contextData;
			}
			else if (!defaultRet && contextData.viewData[editorMode].enabled && !hasTags)
			{
				defaultRet = &contextData;
			}
		}
	}

	return defaultRet;
}

namespace MannUtils
{
static float TIME_FOR_AUTO_REFRESH = 0.5f;

bool InsertClipTracksToNode(CSequencerNode* node, const SFragmentData& fragData, float startTime, float endTime, float& maxTime, const SClipTrackContext& trackContext)
{
	assert(trackContext.context);

	const uint32 numAnimLayers = fragData.animLayers.size();
	const uint32 numProcLayers = fragData.procLayers.size();
	assert(numAnimLayers < 1000);
	assert(numProcLayers < 1000);
	uint32 numAnimClipTracks = 0;
	uint32 numProcClipTracks = 0;
	float fragmentStartTime = startTime;

	bool isLooping = false;

	IAnimationSet* animSet = trackContext.context->animSet;

	const uint32 numNodeLayers = node->GetTrackCount();
	for (uint32 i = 0; i < numNodeLayers; i++)
	{
		CSequencerTrack* animTrack = node->GetTrackByIndex(i);
		if (animTrack->GetParameterType() == SEQUENCER_PARAM_ANIMLAYER)
		{
			numAnimClipTracks++;
		}
		else if (animTrack->GetParameterType() == SEQUENCER_PARAM_PROCLAYER)
		{
			numProcClipTracks++;
		}
	}

	//--- Expand out tracks to receive values
	for (uint32 i = numAnimClipTracks; i < numAnimLayers; i++)
	{
		node->CreateTrack(SEQUENCER_PARAM_ANIMLAYER);
	}
	for (uint32 i = numProcClipTracks; i < numProcLayers; i++)
	{
		node->CreateTrack(SEQUENCER_PARAM_PROCLAYER);
	}

	const uint32 newNumNodeLayers = node->GetTrackCount();
	uint32 animClipTrack = 0;
	uint32 procClipTrack = 0;

	for (uint32 i = 0; i < newNumNodeLayers; i++)
	{
		CSequencerTrack* animTrack = node->GetTrackByIndex(i);

		if (animTrack->GetParameterType() == SEQUENCER_PARAM_ANIMLAYER)
		{
			CClipTrack* pClipTrack = static_cast<CClipTrack*>(animTrack);
			pClipTrack->SetMainAnimTrack(i == 0);
			if (animClipTrack < numAnimLayers && trackContext.context->animSet)
			{
				//--- Insert keys
				const TAnimClipSequence& clipSequence = fragData.animLayers[animClipTrack];
				const uint32 numClips = clipSequence.size();
				float timeTally = startTime;
				float lastDuration = 0.0f;
				for (uint32 c = 0; c < numClips; c++)
				{
					const SAnimClip& animClip = clipSequence[c];

					float time = 0.0f;
					if (animClip.blend.exitTime < 0.0f)
					{
						timeTally += lastDuration;
					}
					else
					{
						timeTally += animClip.blend.exitTime;
					}

					if ((endTime > 0.0f) && (timeTally > endTime))
					{
						lastDuration = 0.0f;
						break;
					}

					int key = animTrack->CreateKey(timeTally);
					CClipKey clipKey;
					clipKey.m_time = timeTally;
					clipKey.historyItem = trackContext.historyItem;
					clipKey.blendOutDuration = fragData.blendOutDuration;
					clipKey.Set(animClip, animSet, fragData.transitionType);

					lastDuration = clipKey.GetDuration();

					if (animClipTrack == 0)
					{
						isLooping = (clipKey.animFlags & CA_LOOP_ANIMATION) != 0;
					}

					animTrack->SetKey(key, &clipKey);
				}

				timeTally += lastDuration;

				maxTime = max(maxTime, timeTally);

				animClipTrack++;
			}
			else
			{
				//--- Dead track, fadeout
				int lastKey = animTrack->GetNumKeys() - 1;
				if (lastKey >= 0)
				{
					CClipKey lastClipKey;

					animTrack->GetKey(lastKey, &lastClipKey);
					if (!lastClipKey.animRef.IsEmpty())
					{
						int key = animTrack->CreateKey(fragmentStartTime);
						CClipKey newKey;
						newKey.m_time = fragmentStartTime;
						newKey.Set(SAnimClip(), animSet, fragData.transitionType);
						newKey.historyItem = trackContext.historyItem;

						animTrack->SetKey(key, &newKey);
					}
				}
			}
		}
		else if (animTrack->GetParameterType() == SEQUENCER_PARAM_PROCLAYER)
		{
			if (procClipTrack < numProcLayers)
			{
				//--- Insert keys
				const TProcClipSequence& clipSequence = fragData.procLayers[procClipTrack];
				const uint32 numClips = clipSequence.size();
				float timeTally = startTime;
				float lastDuration = 0.0f;
				for (uint32 c = 0; c < numClips; c++)
				{
					const SProceduralEntry& procClip = clipSequence[c];
					float blendDuration = procClip.blend.duration;

					float time = 0.0f;
					timeTally += procClip.blend.exitTime;
					if ((endTime > 0.0f) && (timeTally > endTime))
					{
						lastDuration = 0.0f;
						break;
					}
					int key = animTrack->CreateKey(timeTally);
					CProcClipKey clipKey;
					clipKey.FromProceduralEntry(procClip, fragData.transitionType);

					clipKey.m_time = timeTally;
					clipKey.historyItem = trackContext.historyItem;

					animTrack->SetKey(key, &clipKey);
				}

				timeTally += lastDuration;

				maxTime = max(maxTime, timeTally);

				procClipTrack++;
			}
			else
			{
				//--- Dead track, fadeout
				int lastKey = animTrack->GetNumKeys() - 1;
				if (lastKey >= 0)
				{
					CProcClipKey lastClipKey;

					animTrack->GetKey(lastKey, &lastClipKey);
					if (!lastClipKey.typeNameHash.IsEmpty())
					{
						int key = animTrack->CreateKey(fragmentStartTime);
						CProcClipKey newClipKey;
						newClipKey.m_time = fragmentStartTime;
						newClipKey.historyItem = trackContext.historyItem;
						newClipKey.FromProceduralEntry(SProceduralEntry(), fragData.transitionType);

						animTrack->SetKey(key, &newClipKey);
					}
				}
			}
		}

		animTrack->SetTimeRange(Range(0.0f, maxTime));
	}

	return isLooping;
}

void GetFragmentFromClipTracks(CFragment& fragment, CSequencerNode* animNode, uint32 historyItem, float fragStartTime, int fragPart)
{
	fragment = CFragment();

	const bool parsingAll = (historyItem == HISTORY_ITEM_INVALID);

	const int numLayers = animNode->GetTrackCount();
	int numAnimLayers = 0;
	int numProcLayers = 0;
	for (int i = 0; i < numLayers; i++)
	{
		CSequencerTrack* animTrack = animNode->GetTrackByIndex(i);
		if (animTrack->GetParameterType() == SEQUENCER_PARAM_ANIMLAYER)
		{
			numAnimLayers++;
		}
		else if (animTrack->GetParameterType() == SEQUENCER_PARAM_PROCLAYER)
		{
			numProcLayers++;
		}
	}

	fragment.m_animLayers.resize(numAnimLayers);
	fragment.m_procLayers.resize(numProcLayers);
	int animLayer = 0;
	int procLayer = 0;
	CClipKey clipKey;
	CProcClipKey procClipKey;
	for (int i = 0; i < numLayers; i++)
	{
		CSequencerTrack* animTrack = animNode->GetTrackByIndex(i);
		const int numKeys = animTrack->GetNumKeys();

		animTrack->SortKeys();

		if (animTrack->GetParameterType() == SEQUENCER_PARAM_ANIMLAYER)
		{
			TAnimClipSequence& animClipSeq = fragment.m_animLayers[animLayer];

			// Set fragment completion time based on the last clip of the first anim layer
			const bool firstAnimLayer = animLayer == 0;
			if (firstAnimLayer && numKeys)
			{
				const CClipKey* pLastKey = static_cast<const CClipKey*>(animTrack->GetKey(numKeys - 1));
				CRY_ASSERT(pLastKey);
				if (pLastKey)
				{
					fragment.m_blendOutDuration = pLastKey->blendOutDuration;
				}
			}

			float lastTime = fragStartTime;
			bool foundBlock = false;
			bool lastClipTypeMatched = false;
			for (uint32 c = 0; c < numKeys; c++)
			{
				animTrack->GetKey(c, &clipKey);

				const bool transOK = (clipKey.fragIndexBlend == fragPart) || (clipKey.fragIndexMain == fragPart);

				if ((transOK || foundBlock) && (parsingAll || (clipKey.historyItem == historyItem)))
				{
					foundBlock = true;
					SAnimClip animClip;
					clipKey.SetupAnimClip(animClip, lastTime, fragPart);

					animClipSeq.push_back(animClip);
					lastTime = clipKey.m_time;

					if (animClip.blend.terminal)
					{
						break;
					}
				}
				else if (!parsingAll && foundBlock)
				{
					break;
				}
			}

			animLayer++;
		}
		else if (animTrack->GetParameterType() == SEQUENCER_PARAM_PROCLAYER)
		{
			TProcClipSequence& procClipSeq = fragment.m_procLayers[procLayer];

			float lastTime = fragStartTime;
			bool foundBlock = false;
			for (uint32 c = 0; c < numKeys; c++)
			{
				animTrack->GetKey(c, &procClipKey);

				const bool transOK = (procClipKey.fragIndexBlend == fragPart) || (procClipKey.fragIndexMain == fragPart);

				if ((transOK || foundBlock) && (parsingAll || (procClipKey.historyItem == historyItem)))
				{
					foundBlock = true;
					SProceduralEntry procClip;
					procClipKey.ToProceduralEntry(procClip, lastTime, fragPart);

					procClipSeq.push_back(procClip);
					lastTime = procClipKey.m_time;

					if (procClip.blend.terminal)
					{
						break;
					}
				}
				else if (!parsingAll && foundBlock)
				{
					break;
				}
			}
			procLayer++;
		}
	}
}

void InsertFragmentTrackFromHistory(CSequencerNode* animNode, SFragmentHistoryContext& history, float startTime, float endTime, float& maxTime, SScopeData& scopeData)
{
	CTransitionPropertyTrack* propsTrack = (CTransitionPropertyTrack*)animNode->CreateTrack(SEQUENCER_PARAM_TRANSITIONPROPS);

	CFragmentTrack* fragTrack = (CFragmentTrack*)animNode->CreateTrack(SEQUENCER_PARAM_FRAGMENTID);
	assert(fragTrack != NULL);

	fragTrack->SetHistory(history);
	const uint32 numSiblingTracks = history.m_tracks.size();
	if (propsTrack)
	{
		history.m_tracks.push_back(propsTrack);
	}
	history.m_tracks.push_back(fragTrack);

	ActionScopes scopeFlag = BIT64(scopeData.scopeID);

	for (uint32 h = 0; h < history.m_history.m_items.size(); h++)
	{
		const CFragmentHistory::SHistoryItem& item = history.m_history.m_items[h];

		if (item.type == CFragmentHistory::SHistoryItem::Fragment)
		{
			ActionScopes scopeMask = item.installedScopeMask;
			scopeMask &= scopeData.mannContexts->potentiallyActiveScopes;

			uint32 rootScope = 0xffffffff;
			for (uint32 i = 0; i <= scopeData.scopeID; i++)
			{
				if ((BIT64(i) & scopeMask) != 0)
				{
					rootScope = i;
					break;
				}
			}

			if (scopeData.scopeID == rootScope)
			{
				float time = item.time - history.m_history.m_firstTime;
				int newKeyID = fragTrack->CreateKey(time);
				CFragmentKey newKey;
				newKey.m_time = time;
				newKey.fragmentID = item.fragment;
				newKey.fragOptionIdx = item.fragOptionIdx;
				newKey.flags = 0;
				newKey.scopeMask = scopeMask;
				newKey.historyItem = h;
				newKey.tagStateFull = item.tagState;

				fragTrack->SetKey(newKeyID, &newKey);
			}
			else if (scopeMask & scopeFlag)
			{
				//--- Shared key, hookup now
				for (uint32 t = 0; t < numSiblingTracks; t++)
				{
					CSequencerTrack* pTrack = history.m_tracks[t];

					if (pTrack->GetParameterType() == SEQUENCER_PARAM_FRAGMENTID)
					{
						CFragmentTrack* fragTrackSibling = (CFragmentTrack*)pTrack;
						if (fragTrackSibling->GetScopeData().scopeID == rootScope)
						{
							const uint32 numKeys = fragTrackSibling->GetNumKeys();
							for (uint32 k = 0; k < numKeys; k++)
							{
								CFragmentKey newKey;
								fragTrackSibling->GetKey(k, &newKey);
								if (newKey.historyItem == h)
								{
									int newKeyID = fragTrack->CreateKey(newKey.m_time);
									fragTrack->TSequencerTrack<CFragmentKey>::SetKey(newKeyID, &newKey);
									break;
								}
							}
							break;
						}
					}
				}
			}
		}
	}
}

void GetHistoryFromTracks(SFragmentHistoryContext& historyContext)
{
	bool finished = false;
	std::vector<int> keys;
	//const uint32 numScopes = historyContext.m_history.m_actionController.GetTotalScopes();
	const uint32 numTracks = historyContext.m_tracks.size();
	keys.resize(numTracks);

	CFragmentHistory::THistoryBuffer newHistory;
	uint32 curHistoryItem = 0;
	const uint32 curTotalItems = historyContext.m_history.m_items.size();

	ActionScopes scopeMask = 0;
	for (uint32 i = 0; i < numTracks; i++)
	{
		CSequencerTrack* pTrack = historyContext.m_tracks[i];
		if (pTrack && (pTrack->GetParameterType() == SEQUENCER_PARAM_FRAGMENTID))
		{
			CFragmentTrack* pFragTrack = (CFragmentTrack*)pTrack;
			scopeMask |= BIT64(pFragTrack->GetScopeData().scopeID);
		}
	}

	while (!finished)
	{
		float earliestTime = FLT_MAX;
		int bestTrack = -1;

		for (uint32 i = 0; i < numTracks; i++)
		{
			CSequencerTrack* pTrack = historyContext.m_tracks[i];

			if (pTrack && (pTrack->GetNumKeys() > keys[i]))
			{
				float time = pTrack->GetKeyTime(keys[i]) + historyContext.m_history.m_firstTime;

				if (time < earliestTime)
				{
					earliestTime = time;
					bestTrack = i;
				}
			}
		}

		for (; curHistoryItem < curTotalItems; curHistoryItem++)
		{
			const CFragmentHistory::SHistoryItem& item = historyContext.m_history.m_items[curHistoryItem];

			if (item.time < earliestTime)
			{
				if ((item.type == CFragmentHistory::SHistoryItem::Fragment) && ((item.installedScopeMask & scopeMask) == 0))
				{
					//--- Copy over untouched track
					newHistory.push_back(item);
				}
			}
			else
			{
				break;
			}
		}

		if (bestTrack >= 0)
		{
			CSequencerTrack* pTrack = historyContext.m_tracks[bestTrack];
			if (pTrack)
			{
				if (pTrack->GetParameterType() == SEQUENCER_PARAM_FRAGMENTID)
				{
					CFragmentKey fragKey;
					CFragmentTrack* pFragTrack = (CFragmentTrack*)pTrack;
					pFragTrack->GetKey(keys[bestTrack], &fragKey);
					if (!fragKey.transition)
					{
						const uint32 trackScopeID = pFragTrack->GetScopeData().scopeID;
						bool rootScope = true;
						for (uint32 i = 0; i < trackScopeID; i++)
						{
							if (BIT64(i) & (fragKey.scopeMask & scopeMask))
							{
								rootScope = false;
								break;
							}
						}
						if (rootScope)
						{
							ActionScopes rootScopeMask = BIT64(trackScopeID);
							CFragmentHistory::SHistoryItem historyItem(rootScopeMask, fragKey.fragmentID);
							historyItem.installedScopeMask = rootScopeMask;
							historyItem.tagState.fragmentTags = fragKey.tagStateFull.fragmentTags;
							historyItem.time = fragKey.m_time + historyContext.m_history.m_firstTime;
							historyItem.fragOptionIdx = fragKey.fragOptionIdx;
							historyItem.trumpsPrevious = fragKey.trumpPrevious;
							newHistory.push_back(historyItem);

							fragKey.historyItem = newHistory.size() - 1;
							pFragTrack->SetKey(keys[bestTrack], &fragKey);
						}
					}
				}
				else if (pTrack->GetParameterType() == SEQUENCER_PARAM_TAGS)
				{
					CTagKey tagKey;
					pTrack->GetKey(keys[bestTrack], &tagKey);
					CFragmentHistory::SHistoryItem historyItem(tagKey.tagState);
					historyItem.time = tagKey.m_time + historyContext.m_history.m_firstTime;
					newHistory.push_back(historyItem);
				}
				else if (pTrack->GetParameterType() == SEQUENCER_PARAM_PARAMS)
				{
					CParamKey paramKey;
					pTrack->GetKey(keys[bestTrack], &paramKey);
					CFragmentHistory::SHistoryItem historyItem(paramKey.name, paramKey.parameter);
					historyItem.isLocation = paramKey.isLocation;
					historyItem.time = paramKey.m_time + historyContext.m_history.m_firstTime;
					newHistory.push_back(historyItem);
					paramKey.historyItem = newHistory.size() - 1;
					pTrack->SetKey(keys[bestTrack], &paramKey);
				}
			}
			keys[bestTrack]++;
		}
		else
		{
			finished = true;
		}
	}

	historyContext.m_history.m_items = newHistory;

	//--- Saving this out at the start of the game causes the xml to overwrite memory
	//historyContext.m_history.SaveSequence(PathUtil::GetGameFolder() + CString("/Animations/Mannequin/FragmentSequences/test2.xml"));

	historyContext.m_history.FillInTagStates();
}

const char* MOTION_PARAMETERS[] =
{
	"TravelSpeed",
	"TurnSpeed",
	"TravelAngle",       //forward, backwards and sidestepping
	"TravelSlope",
	"TurnAngle",         //Idle2Move and idle-rotations
	"TravelDist",        //idle-steps
	"StopLeg",           //Move2Idle
	"BlendWeight",       //custom parameter
	"BlendWeight2",
	"BlendWeight3",
	"BlendWeight4",
	"BlendWeight5",
	"BlendWeight6",
	"BlendWeight7"
};
const uint32 NUM_MOTION_PARAMETERS = ARRAYSIZE(MOTION_PARAMETERS);
static_assert(eMotionParamID_COUNT == NUM_MOTION_PARAMETERS, "Invalid MotionParamID count!");

EMotionParamID GetMotionParam(const char* paramName)
{
	for (int32 i = 0; i < NUM_MOTION_PARAMETERS; i++)
	{
		if (strcmp(paramName, MOTION_PARAMETERS[i]) == 0)
		{
			return (EMotionParamID)i;
		}
	}

	return eMotionParamID_COUNT;
}

const char* GetMotionParamByIndex(size_t index)
{
	assert(index < NUM_MOTION_PARAMETERS);
	return MOTION_PARAMETERS[index];
}

static const char* const s_mannequinParamNames[] =
{
	"AimTarget",
	"LookTarget",
	"FastHeadTurn",
	"TargetPos",
	"expressionWeight",
	"TouchTarget"
};

size_t GetParamNameCount()
{
	return ARRAYSIZE(s_mannequinParamNames);
}

const char* GetParamNameByIndex(size_t index)
{
	assert(index < GetParamNameCount());
	return s_mannequinParamNames[index];
}

void AdjustFragDataForVEGs(SFragmentData& fragData, SScopeContextData& context, EMannequinEditorMode editorMode, float motionParams[eMotionParamID_COUNT])
{
	const IAnimationSet* pAnimSet = context.animSet;
	ICharacterInstance* pCharInst = context.viewData[editorMode].charInst;

	if (pAnimSet && pCharInst)
	{
		SAnimationProcessParams params;
		params.locationAnimation.SetIdentity();
		params.bOnRender = 0;
		params.zoomAdjustedDistanceFromCamera = 0.0f;
		params.overrideDeltaTime = .1f;   // to make this deterministic

		const uint32 numLayers = fragData.animLayers.size();
		for (uint32 layer = 0; layer < numLayers; layer++)
		{
			const uint32 numAnims = fragData.animLayers[layer].size();
			for (uint32 i = 0; i < numAnims; i++)
			{
				SAnimClip& animClip = fragData.animLayers[layer][i];
				if (animClip.isVariableLength)
				{
					const int id = pAnimSet->GetAnimIDByCRC(animClip.animation.animRef.crc);

					if (id >= 0)
					{
						pCharInst->GetISkeletonAnim()->StopAnimationsAllLayers();

						CryCharAnimationParams animParams;
						animParams.m_nFlags = CA_FORCE_SKELETON_UPDATE | CA_REPEAT_LAST_KEY;
						pCharInst->GetISkeletonAnim()->StartAnimationById(id, animParams);

						for (int32 mp = 0; mp < eMotionParamID_COUNT; mp++)
						{
							pCharInst->GetISkeletonAnim()->SetDesiredMotionParam((EMotionParamID)mp, motionParams[mp], 0.0f);
						}

						// Try to wait for the animation to be streamed in (activated). As there
						// currently is no simple way to check for invalid bspaces, wait only a
						// limited amount of time
						const int maxRetries = 3;
						const int sleepMillisPerRetry = 100;
						const CAnimation* pAnim = NULL;
						for (int retries = 0;; ++retries)
						{
							pCharInst->StartAnimationProcessing(params);
							pCharInst->FinishAnimationComputations();

							pAnim = &pCharInst->GetISkeletonAnim()->GetAnimFromFIFO(0, 0);
							if (pAnim->IsActivated() || (retries == maxRetries))
								break;

							CrySleep(sleepMillisPerRetry);
						}

						const float duration = pAnim->GetExpectedTotalDurationSeconds();
						if (duration >= 0.0f)
						{
							animClip.referenceLength = duration;
						}
					}
				}
			}
		}
	}
}

const char* GetEmptyTagString()
{
	static const char szTagListEmpty[] = "<default>";
	return szTagListEmpty;
}

void AppendTagsFromAnimName(const string& animName, SFragTagState& tagState)
{
	// Nothing for now
}

void FlagsToTagList(char* tagList, uint32 bufferSize, const SFragTagState& tagState, FragmentID fragID, const SControllerDef& def, const char* emptyString)
{
	CryStackStringT<char, 512> sBuffer;
	if (tagState == SFragTagState())
	{
		sBuffer = emptyString ? emptyString : GetEmptyTagString();
	}
	else
	{
		if (tagState.globalTags != TAG_STATE_EMPTY)
		{
			def.m_tags.FlagsToTagList(tagState.globalTags, sBuffer);
		}

		if (fragID != FRAGMENT_ID_INVALID)
		{
			const CTagDefinition* pFragTagDef = def.GetFragmentTagDef(fragID);
			if ((tagState.fragmentTags != TAG_STATE_EMPTY) && pFragTagDef)
			{
				CryStackStringT<char, 512> sFragmentTags;
				pFragTagDef->FlagsToTagList(tagState.fragmentTags, sFragmentTags);
				if (tagState.globalTags != TAG_STATE_EMPTY)
					sBuffer += "+";
				sBuffer += sFragmentTags;
			}
		}
	}

	cry_strcpy(tagList, bufferSize, sBuffer.c_str());
}

void MannUtils::SerializeFragTagState(SFragTagState& tagState, const SControllerDef& contDef, const FragmentID fragID, XmlNodeRef& xmlNode, bool bLoading)
{
	if (bLoading)
	{
		CString globalTagsString;
		CString fragmentTagsString;
		xmlNode->getAttr("globalTags", globalTagsString);
		xmlNode->getAttr("fragmentTags", fragmentTagsString);

		// Set global flags
		contDef.m_tags.TagListToFlags(globalTagsString.GetString(), tagState.globalTags);

		// Set fragment tags
		const CTagDefinition* pFragTagDef = contDef.GetFragmentTagDef(fragID);
		if (pFragTagDef)
		{
			pFragTagDef->TagListToFlags(fragmentTagsString.GetString(), tagState.fragmentTags);
		}
	}
	else
	{
		CryStackStringT<char, 512> globalTagsString;
		CryStackStringT<char, 512> fragmentTagsString;

		// Determine which global flags are set
		if (tagState.globalTags != TAG_STATE_EMPTY)
		{
			contDef.m_tags.FlagsToTagList(tagState.globalTags, globalTagsString);
		}

		// Determine which fragment tags are set
		if (fragID != FRAGMENT_ID_INVALID)
		{
			const CTagDefinition* pFragTagDef = contDef.GetFragmentTagDef(fragID);
			if ((tagState.fragmentTags != TAG_STATE_EMPTY) && pFragTagDef)
			{
				pFragTagDef->FlagsToTagList(tagState.fragmentTags, fragmentTagsString);
			}
		}

		xmlNode->setAttr("globalTags", globalTagsString);
		xmlNode->setAttr("fragmentTags", fragmentTagsString);
	}
}

bool IsSequenceDirty(SMannequinContexts* contexts, CSequencerSequence* sequence, SLastChange& lastChange, EMannequinEditorMode editorMode)
{
	uint32 totalChangeCount = 0;
	uint32 contextChangeCount = 0;

	for (uint32 i = 0; i < sequence->GetNodeCount(); i++)
	{
		CSequencerNode* seqNode = sequence->GetNode(i);
		const int trackCount = seqNode->GetTrackCount();
		for (int t = 0; t < trackCount; t++)
		{
			CSequencerTrack* seqTrack = seqNode->GetTrackByIndex(t);
			totalChangeCount += seqTrack->GetChangeCount();
		}
	}

	for (uint32 i = 0; i < contexts->m_contextData.size(); i++)
	{
		const SScopeContextData& contextData = contexts->m_contextData[i];
		if (contextData.changeCount)
		{
			contextChangeCount += contextData.changeCount;
		}
	}
	totalChangeCount += contextChangeCount;

	float curTime = gEnv->pTimer->GetCurrTime(ITimer::ETIMER_UI);

	if (lastChange.lastChangeCount != totalChangeCount)
	{
		lastChange.lastChangeCount = totalChangeCount;
		lastChange.changeTime = curTime + TIME_FOR_AUTO_REFRESH;
		lastChange.dirty = true;
	}

	if (lastChange.dirty && (curTime > lastChange.changeTime))
	{
		lastChange.lastChangeCount = editorMode == eMEM_FragmentEditor ? totalChangeCount : contextChangeCount;
		lastChange.dirty = false;
		return true;
	}

	return false;
}

void SaveSplitterToXml(const XmlNodeRef& xmlLayout, const CString& name, const CSplitterWnd& splitter)
{
	if (!xmlLayout)
	{
		return;
	}

	int nRows = splitter.GetRowCount() - 1;
	int nCols = splitter.GetColumnCount() - 1;

	XmlNodeRef node = xmlLayout->newChild(name);
	CString attr;
	int curr, min;

	for (int r = 0; r < nRows; r++)
	{
		attr.Format("Row%d", r);
		splitter.GetRowInfo(r, curr, min);

		node->setAttr(attr, curr);
	}

	for (int c = 0; c < nCols; c++)
	{
		attr.Format("Col%d", c);
		splitter.GetColumnInfo(c, curr, min);

		node->setAttr(attr, curr);
	}
}

void LoadSplitterFromXml(const XmlNodeRef& xmlLayout, const CString& name, CSplitterWnd& splitter, int min)
{
	if (!xmlLayout)
	{
		return;
	}

	int nRows = splitter.GetRowCount() - 1;
	int nCols = splitter.GetColumnCount() - 1;

	XmlNodeRef node = xmlLayout->findChild(name);
	if (!node)
	{
		return;
	}

	CString attr;
	int curr = 0;
	
	for (int r = 0; r < nRows; r++)
	{
		attr.Format("Row%d", r);

		node->getAttr(attr, curr);
		splitter.SetRowInfo(r, curr, min);
	}

	for (int c = 0; c < nCols; c++)
	{
		attr.Format("Col%d", c);

		node->getAttr(attr, curr);
		splitter.SetColumnInfo(c, curr, min);
	}
}

void SaveCheckboxToXml(const XmlNodeRef& xmlLayout, const CString& name, const CButton& checkbox)
{
	if (!xmlLayout)
	{
		return;
	}

	XmlNodeRef node = xmlLayout->newChild(name);
	node->setAttr("Value", checkbox.GetCheck());
}

void LoadCheckboxFromXml(const XmlNodeRef& xmlLayout, const CString& name, CButton& checkbox)
{
	if (!xmlLayout)
	{
		return;
	}
	
	XmlNodeRef node = xmlLayout->findChild(name);
	if (node)
	{
		int checkValue = 0;
		node->getAttr("Value", checkValue);
		checkbox.SetCheck(checkValue);
	}
}

void SaveDockingPaneToXml(const XmlNodeRef& xmlLayout, const CString& name, const CXTPDockingPane& dockingPane)
{	
	if (!xmlLayout)
	{
		return;
	}
	
	XmlNodeRef node = xmlLayout->newChild(name);
	CRect paneRect = dockingPane.GetPaneWindowRect();

	node->setAttr("Width", paneRect.Width());
	node->setAttr("Height", paneRect.Height());
}

void LoadDockingPaneFromXml(const XmlNodeRef& xmlLayout, const CString& name, CXTPDockingPane& dockingPane, CXTPDockingPaneManager* dockingPaneManager)
{
	if (!xmlLayout)
	{
		return;
	}
	
	XmlNodeRef node = xmlLayout->findChild(name);
	if (node)
	{
		CSize size;
		node->getAttr("Width", size.cx);
		node->getAttr("Height", size.cy);

		dockingPane.SetMinTrackSize(size);
		dockingPane.SetMaxTrackSize(size);

		dockingPaneManager->RecalcFramesLayout();
		dockingPaneManager->NormalizeSplitters();
		dockingPaneManager->RedrawPanes();

		dockingPane.SetMinTrackSize(CSize(0, 0));
		dockingPane.SetMaxTrackSize(CSize(32000, 32000));
		return;
	}
}

void SaveWindowSizeToXml(const XmlNodeRef& xmlLayout, const CString& name, const CRect& windowRect)
{
	if (!xmlLayout)
	{
		return;
	}

	XmlNodeRef node = xmlLayout->newChild(name);

	node->setAttr("Width", windowRect.Width());
	node->setAttr("Height", windowRect.Height());
}

void LoadWindowSizeFromXml(const XmlNodeRef& xmlLayout, const CString& name, CRect& windowRect)
{
	if (!xmlLayout)
	{
		return;
	}
	
	XmlNodeRef node = xmlLayout->findChild(name);
	if (node)
	{		
		int x = 0;
		int y = 0;
		node->getAttr("Width", x);
		node->getAttr("Height", y);

		windowRect.SetRect(0, 0, x, y);
	}
}

IMannequinEditorManager& GetMannequinEditorManager()
{
	IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
	IMannequinEditorManager* pManEditMan = mannequinSys.GetMannequinEditorManager();
	assert(pManEditMan);
	return *pManEditMan;
}
}

void CFragmentHistory::LoadSequence(const char* sequence)
{
	LoadSequence(GetISystem()->LoadXmlFromFile(sequence));
}

void CFragmentHistory::LoadSequence(XmlNodeRef root)
{
	if (root)
	{
		root->getAttr("StartTime", m_startTime);
		root->getAttr("EndTime", m_endTime);
		float firstTime = m_endTime;

		const uint32 numChildren = root->getChildCount();
		m_items.resize(numChildren);
		uint32 itemID = 0;
		for (uint32 i = 0; i < numChildren; i++)
		{
			SHistoryItem item;
			XmlNodeRef event = root->getChild(i);

			if (event->haveAttr("FragmentID"))
			{
				const char* scopeMask = event->getAttr("ScopeMask");

				if (scopeMask)
				{
					m_actionController.GetContext().controllerDef.m_scopeIDs.TagListToFlags(scopeMask, STagStateBase((uint8*)&item.loadedScopeMask, sizeof(item.loadedScopeMask)));
				}
				const char* fragmentID = event->getAttr("FragmentID");
				item.fragment = m_actionController.GetContext().controllerDef.m_fragmentIDs.Find(fragmentID);
				event->getAttr("OptionIdx", item.fragOptionIdx);
				item.type = SHistoryItem::Fragment;
				event->getAttr("Trump", item.trumpsPrevious);

				const char* fragTags = event->getAttr("TagState");
				const CTagDefinition* fragTagDef = (item.fragment == FRAGMENT_ID_INVALID) ? NULL : m_actionController.GetContext().controllerDef.GetFragmentTagDef(item.fragment);
				if (fragTags && fragTagDef)
				{
					fragTagDef->TagListToFlags(fragTags, item.tagState.fragmentTags);
				}
			}
			else if (event->haveAttr("TagState"))
			{
				const char* pTagState = event->getAttr("TagState");
				TagState tagState;
				m_actionController.GetContext().controllerDef.m_tags.TagListToFlags(pTagState, tagState);
				item = SHistoryItem(tagState);
			}
			else if (event->haveAttr("ParamName"))
			{
				if (event->getChildCount() > 0)
				{
					const char* paramName = event->getAttr("ParamName");
					SMannParameter param;
					XmlNodeRef valueXML = event->getChild(0);
					valueXML->getAttr("x", param.value.t.x);
					valueXML->getAttr("y", param.value.t.y);
					valueXML->getAttr("z", param.value.t.z);
					valueXML->getAttr("qx", param.value.q.v.x);
					valueXML->getAttr("qy", param.value.q.v.y);
					valueXML->getAttr("qz", param.value.q.v.z);
					valueXML->getAttr("qw", param.value.q.w);

					item = SHistoryItem(paramName, param);
				}
				else
				{
					continue;
				}
			}

			event->getAttr("Time", item.time);

			firstTime = min(firstTime, item.time);
			m_items[itemID++] = item;
		}
		m_items.resize(itemID);

		m_firstTime = firstTime;

		FillInTagStates();
	}
}

void CFragmentHistory::LoadSequence(const int scopeContextIdx, const FragmentID fromID, const FragmentID toID, const SFragTagState fromFragTag, const SFragTagState toFragTag, const SFragmentBlendUid blendUid, const TagState& tagState)
{
	static const float DEFAULT_TIME_STEP = 3.0f;

	//--- Remove all but the param items
	THistoryBuffer strippedList;
	const uint32 numItems = m_items.size();
	for (uint32 i = 0; i < numItems; i++)
	{
		if (m_items[i].type == SHistoryItem::Param)
		{
			strippedList.push_back(m_items[i]);
			continue;
		}
	}
	m_items = strippedList;

	m_startTime = 0.0f;
	m_firstTime = 0.0f;
	float lastTagsTime = 0.0f;
	float lastFragTime = 0.0f;
	float duration = 0.0f;

	SMannequinContexts* pContexts = CMannequinDialog::GetCurrentInstance()->Contexts();
	SScopeContextData* pScopeContextData = &pContexts->m_contextData[scopeContextIdx];
	assert(pScopeContextData != NULL);

	SFragmentSelection fragmentSelection;
	SFragmentData fragData;
	SBlendQuery blendQuery;

	blendQuery.fragmentTo = fromID;
	blendQuery.tagStateTo.fragmentTags = fromFragTag.fragmentTags;
	blendQuery.tagStateTo.globalTags = pScopeContextData->database->GetTagDefs().GetUnion(fromFragTag.globalTags, tagState);
	blendQuery.SetFlag(SBlendQuery::toInstalled, true);

	uint32 flags = pScopeContextData->database->Query(fragData, blendQuery, OPTION_IDX_RANDOM, pScopeContextData->animSet, &fragmentSelection);

	if ((flags != 0) && (fragData.isOneShot))
	{
		for (int i = 0; i < SFragmentData::PART_TOTAL; ++i)
		{
			duration += fragData.duration[i];
		}

		// If the duration is 0, then the animation was a pure procedural one and should be treated as not a one-shot
		if (duration == 0.0f)
		{
			duration = DEFAULT_TIME_STEP;
		}
	}
	else
	{
		duration = DEFAULT_TIME_STEP;
	}

	// Add the FROM tags
	// If we don't have the initial global tags then use the subsequent tags instead
	{
		SHistoryItem item = SHistoryItem(fromFragTag.globalTags);
		item.time = lastTagsTime;
		item.startTime = lastTagsTime;
		m_items.push_back(item);
		lastTagsTime += duration;
	}

	// add the FROM fragment
	{
		SHistoryItem item;

		item.fragment = fromID;
		item.type = SHistoryItem::Fragment;
		item.tagState.fragmentTags = fromFragTag.fragmentTags;

		item.time = lastFragTime;
		m_items.push_back(item);
		lastFragTime += duration;
	}

	blendQuery = SBlendQuery();
	blendQuery.fragmentFrom = fromID;
	blendQuery.fragmentTo = toID;
	blendQuery.tagStateFrom.fragmentTags = fromFragTag.fragmentTags;
	blendQuery.tagStateFrom.globalTags = pScopeContextData->database->GetTagDefs().GetUnion(fromFragTag.globalTags, tagState);
	blendQuery.tagStateTo.fragmentTags = toFragTag.fragmentTags;
	blendQuery.tagStateTo.globalTags = pScopeContextData->database->GetTagDefs().GetUnion(toFragTag.globalTags, tagState);
	blendQuery.SetFlag(SBlendQuery::fromInstalled, true);
	blendQuery.SetFlag(SBlendQuery::toInstalled, true);

	flags = pScopeContextData->database->Query(fragData, blendQuery, OPTION_IDX_RANDOM, pScopeContextData->animSet, &fragmentSelection);

	if ((flags != 0) && (fragData.isOneShot))
	{
		duration = 0.0f;
		for (int i = 0; i < SFragmentData::PART_TOTAL; ++i)
		{
			duration += fragData.duration[i];
		}
	}
	else
	{
		duration = DEFAULT_TIME_STEP;
	}
	// Add the TO tags
	{
		SHistoryItem item = SHistoryItem(toFragTag.globalTags);
		item.time = lastTagsTime;
		item.startTime = lastTagsTime;
		m_items.push_back(item);
		lastTagsTime += duration;
	}

	// Add the TO fragment
	{
		SHistoryItem item;

		item.fragment = toID;
		item.type = SHistoryItem::Fragment;
		item.tagState.fragmentTags = toFragTag.fragmentTags;

		item.time = lastFragTime;
		m_items.push_back(item);
		lastFragTime += duration;
	}

	m_endTime = lastFragTime;

	FillInTagStates();
}

void CFragmentHistory::SaveSequence(const char* filename)
{
	XmlNodeRef root = GetISystem()->CreateXmlNode("History");

	root->setAttr("StartTime", m_startTime);
	root->setAttr("EndTime", m_endTime);

	for (uint32 i = 0; i < m_items.size(); i++)
	{
		const SHistoryItem& item = m_items[i];

		XmlNodeRef event = GetISystem()->CreateXmlNode("Item");

		//--- Save slot out
		event->setAttr("Time", item.time);
		switch (item.type)
		{
		case SHistoryItem::Tag:
			{
				CryStackStringT<char, 512> sGlobalTags;
				m_actionController.GetContext().controllerDef.m_tags.FlagsToTagList(item.tagState.globalTags, sGlobalTags);
				event->setAttr("TagState", sGlobalTags.c_str());
				break;
			}
		case SHistoryItem::Fragment:
			{
				CryStackStringT<char, 512> sLoadedScopeMask;
				m_actionController.GetContext().controllerDef.m_scopeIDs.IntegerFlagsToTagList(item.loadedScopeMask, sLoadedScopeMask);
				event->setAttr("ScopeMask", sLoadedScopeMask.c_str());

				const char* tempBuffer = (item.fragment == FRAGMENT_ID_INVALID) ? "none" : m_actionController.GetContext().controllerDef.m_fragmentIDs.GetTagName(item.fragment);
				event->setAttr("FragmentID", tempBuffer);
				event->setAttr("OptionIdx", item.fragOptionIdx);
				const CTagDefinition* fragTagDef = (item.fragment == FRAGMENT_ID_INVALID) ? NULL : m_actionController.GetContext().controllerDef.GetFragmentTagDef(item.fragment);
				if ((item.tagState.fragmentTags != TAG_STATE_EMPTY) && fragTagDef)
				{
					CryStackStringT<char, 512> sFragmentTags;
					fragTagDef->FlagsToTagList(item.tagState.fragmentTags, sFragmentTags);
					event->setAttr("TagState", sFragmentTags.c_str());
				}
			}
			break;
		case SHistoryItem::Param:
			{
				event->setAttr("ParamName", item.paramName);
				event->setAttr("Type", "Location");
				XmlNodeRef valueXML = GetISystem()->CreateXmlNode("Value");
				event->addChild(valueXML);
				valueXML->setAttr("x", item.param.value.t.x);
				valueXML->setAttr("y", item.param.value.t.y);
				valueXML->setAttr("z", item.param.value.t.z);
				valueXML->setAttr("qx", item.param.value.q.v.x);
				valueXML->setAttr("qy", item.param.value.q.v.y);
				valueXML->setAttr("qz", item.param.value.q.v.z);
				valueXML->setAttr("qw", item.param.value.q.w);
			}
			break;
		default:
			continue;
			break;
		}

		root->addChild(event);
	}

	_ASSERTE(_CrtCheckMemory());

	root->saveToFile(filename);

	_ASSERTE(_CrtCheckMemory());
}

void CFragmentHistory::UpdateScopeMasks(const SMannequinContexts* contexts, const TagState& globalTags)
{
	const uint32 numScopes = contexts->m_controllerDef->m_scopeIDs.GetNum();
	for (THistoryBuffer::iterator iter = m_items.begin(); iter != m_items.end(); iter++)
	{
		SHistoryItem& item = *iter;
		if (item.type == SHistoryItem::Fragment)
		{
			if (item.fragment != FRAGMENT_ID_INVALID)
			{
				SFragTagState fragTagState = item.tagState;
				fragTagState.globalTags = contexts->m_controllerDef->m_tags.GetUnion(fragTagState.globalTags, globalTags);

				item.installedScopeMask = contexts->m_controllerDef->GetScopeMask(item.fragment, fragTagState);
			}
		}
	}
}

void CTagControl::Init(CVarBlock* pVarBlock, const CTagDefinition& tagDef, TagState enabledControls)
{
	m_tagDef = &tagDef;

	m_tagVarList.clear();
	m_tagGroupList.clear();

	const uint32 numTags = tagDef.GetNum();

	const char* lastGroupName = NULL;
	uint32 numGroups = tagDef.GetNumGroups();
	for (uint32 i = 0; i < numGroups; i++)
	{
		bool isGroupEnabled = tagDef.IsGroupSet(enabledControls, i);

		if (isGroupEnabled)
		{
			CSmartVariableEnum<int> enumList;
			enumList->SetName(tagDef.GetGroupName(i));

			enumList->AddEnumItem("", -1);
			for (uint32 t = 0; t < numTags; t++)
			{
				if (tagDef.GetGroupID(t) == i)
				{
					const char* tagName = tagDef.GetTagName(t);
					enumList->AddEnumItem(tagName, t);
				}
			}
			m_tagGroupList.push_back(enumList);
			CVariableBase& var = *enumList.GetVar();
			var.SetUserData((void*)(uintptr_t)i);

			pVarBlock->AddVariable(&var);
		}
	}
	for (uint32 t = 0; t < numTags; t++)
	{
		if ((tagDef.GetGroupID(t) < 0) && tagDef.IsSet(enabledControls, t))
		{
			const char* tagName = tagDef.GetTagName(t);

			CSmartVariable<bool> tagVar;
			tagVar->SetName(tagName);
			tagVar->SetDataType(IVariable::DT_SIMPLE);
			tagVar->SetUserData((void*)(uintptr_t)t);
			*tagVar = false;
			m_tagVarList.push_back(tagVar);
			pVarBlock->AddVariable(tagVar.GetVar());
		}
	}
}

TagState CTagControl::Get() const
{
	if (m_tagDef)
	{
		const CTagDefinition& tagDef = *m_tagDef;
		CTagState tagState(tagDef);

		const TagGroupID numGroups = (TagGroupID)m_tagGroupList.size();
		for (TagGroupID g = 0; g < numGroups; g++)
		{
			CVariableBase& var = *m_tagGroupList[g];
			int curTag = 0;
			var.Get(curTag);

			if (curTag >= 0)
			{
				tagState.Set(curTag, true);
			}
		}
		const uint32 numTags = m_tagVarList.size();
		for (uint32 i = 0; i < numTags; i++)
		{
			CVariableBase& var = *m_tagVarList[i];
			bool isSet = false;
			var.Get(isSet);

			tagState.Set((int)(intptr_t)var.GetUserData(), isSet);
		}

		return tagState.GetMask();
	}
	else
	{
		return TAG_STATE_EMPTY;
	}
}

void CTagControl::Set(const TagState& tagState) const
{
	if (m_tagDef)
	{
		const CTagDefinition& tagDef = *m_tagDef;
		const uint32 numTags = tagDef.GetNum();

		const uint32 numGroups = m_tagGroupList.size();
		for (uint32 g = 0; g < numGroups; g++)
		{
			CVariableBase& var = *m_tagGroupList[g];
			uint32 groupID = (uint32)(uintptr_t)var.GetUserData();

			int value = -1;
			for (uint32 i = 0; i < numTags; i++)
			{
				if ((tagDef.GetGroupID(i) == groupID) && tagDef.IsSet(tagState, i))
				{
					value = i;
					break;
				}
			}

			var.Set(value);
		}

		const uint32 numTagContols = m_tagVarList.size();
		for (uint32 i = 0; i < numTagContols; i++)
		{
			CVariableBase& var = *m_tagVarList[i];
			int tagID = (int)(intptr_t)var.GetUserData();
			var.Set(tagDef.IsSet(tagState, tagID));
		}
	}
	else
	{
		assert(0);
	}
}

void CTagControl::SetCallback(IVariable::OnSetCallback func)
{
	const uint32 numGroups = m_tagGroupList.size();
	for (uint32 g = 0; g < numGroups; g++)
	{
		CVariableBase& var = *m_tagGroupList[g];
		var.AddOnSetCallback(func);
	}
	const uint32 numTags = m_tagVarList.size();
	for (uint32 i = 0; i < numTags; i++)
	{
		CVariableBase& var = *m_tagVarList[i];
		var.AddOnSetCallback(func);
	}
}

