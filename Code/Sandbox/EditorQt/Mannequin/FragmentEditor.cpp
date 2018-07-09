// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FragmentEditor.h"

#include "Dialogs/ToolbarDialog.h"

#include <CryGame/IGameFramework.h>

#include "MannequinDialog.h"
#include "MannequinBase.h"
#include "FragmentTrack.h"
#include "SequencerSequence.h"
#include "FragmentEditorNodes.h"
#include "ICryMannequinEditor.h"
#include "Controls/FragmentEditorPage.h"

namespace
{
static const float FRAGMENT_MIN_TIME_RANGE = 1.0f;
};

#define IDW_KEYS_PANE AFX_IDW_CONTROLBAR_FIRST + 10

IMPLEMENT_DYNAMIC(CMannFragmentEditor, CSequencerDopeSheet)

CMannFragmentEditor::CMannFragmentEditor()
	:
	m_contexts(NULL),
	m_fragID(FRAGMENT_ID_INVALID),
	m_bEditingFragment(false),
	m_fragmentHistory(NULL),
	m_fragmentScopeMask(0),
	m_fragOptionIdx(0)
{
	const char* FRAGEDITOR_NAME = "Fragment Editor";
	CSequencerSequence* sequenceFE = new CSequencerSequence();
	sequenceFE->SetName(FRAGEDITOR_NAME);
	sequenceFE->AddRef();
	SetSequence(sequenceFE);
}

CMannFragmentEditor::~CMannFragmentEditor()
{
}

void CMannFragmentEditor::InitialiseToPreviewFile(const XmlNodeRef& xmlSequenceRoot)
{
	m_bEditingFragment = false;

	SAFE_DELETE(m_fragmentHistory);
	m_fragmentHistory = new SFragmentHistoryContext(*m_contexts->viewData[eMEM_FragmentEditor].m_pActionController);
	m_fragmentHistory->m_history.LoadSequence(xmlSequenceRoot);
	m_fragmentHistory->m_history.UpdateScopeMasks(m_contexts, m_tagState.globalTags);
}

void CMannFragmentEditor::SetMannequinContexts(SMannequinContexts* contexts)
{
	m_contexts = contexts;
}

//void CMannFragmentEditor::DrawKeys( CSequencerTrack *track,CDC *dc,CRect &rc,Range &timeRange )
//{
//	__super::DrawKeys(track, dc, rc, timeRange);
//}

void CMannFragmentEditor::SetCurrentFragment()
{
	if (GetFragmentID() != FRAGMENT_ID_INVALID)
	{
		SetFragment(GetFragmentID(), GetTagState(), GetFragmentOptionIdx());
	}
}

void CalculateMotionParams(float motionParams[eMotionParamID_COUNT], const CFragmentHistory& history)
{
	memset(motionParams, 0, sizeof(float) * eMotionParamID_COUNT);

	const uint32 numHistoryItems = history.m_items.size();
	for (uint32 h = 0; h < numHistoryItems; h++)
	{
		const CFragmentHistory::SHistoryItem& item = history.m_items[h];

		if (item.type == CFragmentHistory::SHistoryItem::Param)
		{
			EMotionParamID paramID = MannUtils::GetMotionParam(item.paramName);
			if (paramID != eMotionParamID_COUNT)
			{
				motionParams[paramID] = item.param.value.q.v.x;
			}
		}
	}
}

void CMannFragmentEditor::SetFragment(const FragmentID fragID, const SFragTagState& tagStateIn, const uint32 fragOption)
{
	if (m_bEditingFragment)
	{
		StoreChanges();
	}

	if (m_contexts->m_controllerDef == NULL || !m_fragmentHistory)
	{
		return;
	}

	//--- Inform about the new tag state so any contexts can be updated
	SFragTagState newTagState(tagStateIn);
	CMannequinDialog::GetCurrentInstance()->FragmentEditor()->SetTagState(fragID, newTagState);

	// - merge in the global tags defined in the top-right panel
	const CTagDefinition& tagDef = m_contexts->m_controllerDef->m_tags;
	const TagState globalTags = CMannequinDialog::GetCurrentInstance()->FragmentEditor()->GetGlobalTags();
	newTagState.globalTags = tagDef.GetUnion(globalTags, tagStateIn.globalTags);

	CSequencerSequence* sequenceFE = GetSequence();
	sequenceFE->RemoveAll();

	m_fragmentHistory->m_tracks.clear();

	if (fragID != FRAGMENT_ID_INVALID)
	{
		const uint32 numScopes = m_contexts->m_scopeData.size();

		CalculateMotionParams(m_motionParams, m_fragmentHistory->m_history);

		{
			const CTagDefinition& fragDefs = m_contexts->m_controllerDef->m_fragmentIDs;
			sequenceFE->SetName(fragDefs.GetTagName(fragID));
		}

		SFragTagState tagStateQuery = newTagState;
		// build the tagStateQuery, it's used when populating the tracks with fragments from the available scopes
		const CTagDefinition& tagDefs = m_contexts->m_controllerDef->m_tags;
		for (uint32 i = 0; i < numScopes; i++)
		{
			tagStateQuery.globalTags = tagDefs.GetDifference(tagStateQuery.globalTags, m_contexts->m_controllerDef->m_scopeDef[i].additionalTags);
		}

		// Populate the "Params" track with new keys
		{
			CFERootNode* rootNode = new CFERootNode(sequenceFE, *m_contexts->m_controllerDef);
			CSequencerTrack* paramTrack = rootNode->CreateTrack(SEQUENCER_PARAM_PARAMS);
			sequenceFE->AddNode(rootNode);
			m_fragmentHistory->m_tracks.push_back(paramTrack);
			const uint32 numHistoryItems = m_fragmentHistory->m_history.m_items.size();
			for (uint32 h = 0; h < numHistoryItems; h++)
			{
				const CFragmentHistory::SHistoryItem& item = m_fragmentHistory->m_history.m_items[h];

				if (item.type == CFragmentHistory::SHistoryItem::Param)
				{
					const float time = item.time - m_fragmentHistory->m_history.m_firstTime;
					const int newKeyID = paramTrack->CreateKey(time);
					CParamKey newKey;
					newKey.m_time = time;
					newKey.name = item.paramName;
					newKey.parameter = item.param;
					newKey.isLocation = item.isLocation;
					newKey.historyItem = h;
					paramTrack->SetKey(newKeyID, &newKey);
				}
			}
		}

		ActionScopes scopeMask = m_contexts->m_controllerDef->GetScopeMask(fragID, newTagState);

		//--- If this is refering to a subcontext then ensure that we include its scopes too
		const int numSubContexts = m_contexts->m_controllerDef->m_subContext.size();
		for (int i = 0; i < numSubContexts; i++)
		{
			const SSubContext& subContext = m_contexts->m_controllerDef->m_subContext[i];

			if (tagDefs.Contains(newTagState.globalTags, subContext.additionalTags))
			{
				scopeMask |= subContext.scopeMask;
			}
		}
		m_fragmentScopeMask = scopeMask;

		uint32 installedContexts = 0;
		float maxTime = FRAGMENT_MIN_TIME_RANGE;

		for (uint32 i = 0; i < numScopes; i++)
		{
			SScopeData& scopeData = m_contexts->m_scopeData[i];

			if (scopeData.context[eMEM_FragmentEditor] && scopeData.context[eMEM_FragmentEditor]->database)
			{
				const IScope* scope = m_contexts->viewData[eMEM_FragmentEditor].m_pActionController->GetScope(i);
				if (scopeMask & BIT64(i))
				{
					const uint32 scopeContextID = 1 << scope->GetContextID();

					const TagState& additionalScopeTags = m_contexts->m_controllerDef->m_scopeDef[i].additionalTags;
					const TagState& additionalContextTags = m_contexts->m_controllerDef->m_scopeContextDef[scope->GetContextID()].additionalTags;
					const TagState additionalTags = tagDefs.GetUnion(additionalScopeTags, additionalContextTags);

					const bool editableNode = ((scopeContextID & installedContexts) == 0) || (additionalTags != TAG_STATE_EMPTY);
					SScopeContextData* pCurrentContextData = scopeData.context[eMEM_FragmentEditor];

					if (editableNode)
					{
						CFragmentNode* fragNode = new CFragmentNode(sequenceFE, &scopeData, this);
						sequenceFE->AddNode(fragNode);

						// construct a query...
						SBlendQuery blendQuery;
						blendQuery.fragmentTo = fragID;
						blendQuery.tagStateTo.globalTags = tagDefs.GetUnion(tagStateQuery.globalTags, additionalTags);
						blendQuery.tagStateTo.fragmentTags = tagStateQuery.fragmentTags;
						blendQuery.additionalTags = additionalTags;
						blendQuery.SetFlag(SBlendQuery::toInstalled, true);
						blendQuery.SetFlag(SBlendQuery::higherPriority, true);
						blendQuery.SetFlag(SBlendQuery::noTransitions, true);

						// ...fetch the fragData and fragSelection with it (passed in by reference and filled)
						SFragmentSelection fragSelection;

						const IAnimationDatabase* pDatabase = pCurrentContextData->database;
						SFragmentData fragData;

						const uint32 queryResult = pDatabase->Query(fragData, blendQuery, fragOption, pCurrentContextData->animSet, &fragSelection);
						const bool foundMatch = (queryResult & eSF_Fragment) != 0;

						MannUtils::AdjustFragDataForVEGs(fragData, *pCurrentContextData, eMEM_FragmentEditor, m_motionParams);

						// get the correctly formatted global and, optionally, fragments names.
						CryStackStringT<char, 1024> sGlobalTags;
						CryStackStringT<char, 1024> sFragmentTags;
						m_contexts->m_controllerDef->m_tags.FlagsToTagList(fragSelection.tagState.globalTags, sGlobalTags);

						if (fragSelection.tagState.fragmentTags != TAG_STATE_EMPTY)
						{
							const CTagDefinition* pFragmentTagDefinition = m_contexts->m_controllerDef->GetFragmentTagDef(fragID);
							CRY_ASSERT(pFragmentTagDefinition);

							pFragmentTagDefinition->FlagsToTagList(fragSelection.tagState.fragmentTags, sFragmentTags);
						}

						uint32 numOptions = 0;
						if (foundMatch)
						{
							numOptions = pDatabase->FindBestMatchingTag(SFragmentQuery(fragID, fragSelection.tagState));
						}
						else
						{
							fragSelection.tagState.globalTags = m_contexts->m_controllerDef->m_tags.GetUnion(tagStateQuery.globalTags, additionalTags);
							fragSelection.tagState.fragmentTags = tagStateQuery.fragmentTags;
							fragSelection.optionIdx = fragOption;
						}

						CString buffer = scopeData.name;
						buffer.AppendFormat("(%s)", pCurrentContextData->name.GetString());
						buffer.AppendFormat(": %s", sGlobalTags.c_str());
						if (!sFragmentTags.empty())
						{
							buffer.AppendFormat("+[%s]", sFragmentTags.c_str());
						}
						if (numOptions > 1)
						{
							buffer.AppendFormat("(%d of %d)", fragSelection.optionIdx + 1, numOptions);
						}
						fragNode->SetName(buffer);
						fragNode->SetSelection(foundMatch, fragSelection);

						SClipTrackContext trackContext;
						trackContext.context = pCurrentContextData;
						trackContext.tagState = fragSelection.tagState;

						MannUtils::InsertClipTracksToNode(fragNode, fragData, 0.0f, -1.0f, maxTime, trackContext);

						// Update the installedContexts bit flags to indicate which Scope Contexts we have.
						installedContexts |= scopeContextID;
					}
				}
				else
				{
					CFragmentNode* fragNode = new CFragmentNode(sequenceFE, &scopeData, this);
					sequenceFE->AddNode(fragNode);

					fragNode->SetIsSequenceNode(true);
					fragNode->SetName(scopeData.name);

					float maxTime2;
					MannUtils::InsertFragmentTrackFromHistory(fragNode, *m_fragmentHistory, 0.0f, -1.0f, maxTime2, scopeData);
				}
			}
		}

		sequenceFE->SetTimeRange(Range(0.0f, maxTime));
		SetTimeRange(0.0f, maxTime);

		m_tagState = tagStateIn;
		m_fragOptionIdx = fragOption;
		m_bEditingFragment = true;

		UpdateFragmentClips();
	}
	else
	{
		sequenceFE->SetName("None");
		m_fragmentScopeMask = 0;
		m_tagState = SFragTagState();
		m_fragOptionIdx = 0;
		m_bEditingFragment = false;
	}

	GetIEditorImpl()->Notify(eNotify_OnUpdateSequencer);

	m_fragID = fragID;

	CMannequinDialog::GetCurrentInstance()->FragmentEditor()->UpdateSelectedFragment();
}

void CMannFragmentEditor::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	CSequencerDopeSheetBase::OnEditorNotifyEvent(event);

	switch (event)
	{
	case eNotify_OnUpdateSequencerKeySelection:
		if (m_bEditingFragment)
		{
			CSequencerUtils::SelectedKeys selectedKeys;
			CSequencerUtils::GetSelectedKeys(GetSequence(), selectedKeys);

			CClipKey animClip;

			Range timeRange = GetSequence()->GetTimeRange();
			float maxTime = FRAGMENT_MIN_TIME_RANGE;
			const uint32 numKeys = selectedKeys.keys.size();
			for (uint32 i = 0; i < numKeys; i++)
			{
				CSequencerUtils::SelectedKey& key = selectedKeys.keys[i];

				if (key.pTrack->GetParameterType() == SEQUENCER_PARAM_ANIMLAYER)
				{
					key.pTrack->GetKey(key.nKey, &animClip);
					float time = (animClip.m_time + animClip.GetDuration());
					if (time > timeRange.end)
					{
						timeRange.end = time;
					}
				}
			}

			if (timeRange.end != GetSequence()->GetTimeRange().end)
			{
				GetSequence()->SetTimeRange(timeRange);
				SetTimeRange(timeRange.start, timeRange.end);

				GetIEditorImpl()->Notify(eNotify_OnUpdateSequencer);
			}
		}
		break;
	}
}

void CMannFragmentEditor::UpdateFragmentClips()
{
	CSequencerSequence* sequence = GetSequence();
	uint32 numNodes = sequence->GetNodeCount();

	for (uint32 i = 0; i < numNodes; i++)
	{
		CSequencerNode* animNode = sequence->GetNode(i);
		ESequencerNodeType nodeType = animNode->GetType();
		if (nodeType == SEQUENCER_NODE_FRAGMENT)
		{
			CFragmentNode* fragNode = (CFragmentNode*)animNode;
			SScopeData* scopeData = fragNode->GetScopeData();

			if (scopeData->context[eMEM_FragmentEditor] && scopeData->context[eMEM_FragmentEditor]->database && (fragNode->GetTrackCount() > 0))
			{
				const IAnimationDatabase* pDatabase = scopeData->context[eMEM_FragmentEditor]->database;
				CSequencerTrack* pTrack = fragNode->GetTrackByIndex(0);

				const int numFragKeys = pTrack->GetNumKeys();
				for (int i = 0; i < numFragKeys; i++)
				{
					CFragmentKey fragKey;
					pTrack->GetKey(i, &fragKey);

					const CFragmentHistory::SHistoryItem& item = m_fragmentHistory->m_history.m_items[fragKey.historyItem];

					SFragTagState tagState = item.tagState;
					tagState.globalTags = pDatabase->GetTagDefs().GetUnion(tagState.globalTags, m_tagState.globalTags);

					SFragTagState matchingState;
					bool foundEntry = pDatabase->FindBestMatchingTag(SFragmentQuery(fragKey.fragmentID, tagState), &matchingState) > 0;

					fragKey.hasFragment = foundEntry;
					fragKey.tagState = matchingState;

					pTrack->SetKey(i, &fragKey);
				}
			}
		}
	}
}

void CMannFragmentEditor::StoreChanges()
{
	CSequencerSequence* sequence = GetSequence();
	uint32 numNodes = sequence->GetNodeCount();
	IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
	for (uint32 i = 0; i < numNodes; i++)
	{
		CSequencerNode* animNode = sequence->GetNode(i);
		ESequencerNodeType nodeType = animNode->GetType();
		if (nodeType == SEQUENCER_NODE_FRAGMENT_CLIPS)
		{
			CFragmentNode* fragNode = (CFragmentNode*)animNode;
			SScopeData* scopeData = fragNode->GetScopeData();

			if (scopeData->context[eMEM_FragmentEditor] && scopeData->context[eMEM_FragmentEditor]->database)
			{
				bool trackHasData = fragNode->GetTrackCount() != 0;

				if (trackHasData)// || dbaHasData)
				{
					MannUtils::GetFragmentFromClipTracks(m_fragmentTEMP, fragNode);
					mannequinSys.GetMannequinEditorManager()->SetFragmentEntry(scopeData->context[eMEM_FragmentEditor]->database, m_fragID, fragNode->GetSelection().tagState, fragNode->GetSelection().optionIdx, m_fragmentTEMP);
				}
			}
		}
	}

	MannUtils::GetHistoryFromTracks(*m_fragmentHistory);
	UpdateFragmentClips();
}

void CMannFragmentEditor::OnAccept()
{
	if (m_bEditingFragment)
	{
		StoreChanges();

		float motionParams[eMotionParamID_COUNT];
		CalculateMotionParams(motionParams, m_fragmentHistory->m_history);
		const bool changedParams = memcmp(m_motionParams, motionParams, sizeof(float) * eMotionParamID_COUNT) != 0;

		if (changedParams)
		{
			//--- Adjust clip tracks for the new parameters in case they have changed length
			memcpy(m_motionParams, motionParams, sizeof(float) * eMotionParamID_COUNT);

			CSequencerSequence* sequence = GetSequence();
			uint32 numNodes = sequence->GetNodeCount();
			IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
			for (uint32 i = 0; i < numNodes; i++)
			{
				CSequencerNode* animNode = sequence->GetNode(i);
				ESequencerNodeType nodeType = animNode->GetType();
				if (nodeType == SEQUENCER_NODE_FRAGMENT_CLIPS)
				{
					CFragmentNode* fragNode = (CFragmentNode*)animNode;
					SScopeData* scopeData = fragNode->GetScopeData();

					if (scopeData->context[eMEM_FragmentEditor] && scopeData->context[eMEM_FragmentEditor]->database)
					{
						bool trackHasData = fragNode->GetTrackCount() != 0;

						if (trackHasData)
						{
							const TagState additionalTags = m_contexts->m_controllerDef->m_scopeDef[scopeData->scopeID].additionalTags;

							SBlendQuery blendQuery;
							blendQuery.fragmentTo = m_fragID;
							blendQuery.tagStateTo = fragNode->GetSelection().tagState;
							blendQuery.additionalTags = additionalTags;
							blendQuery.SetFlag(SBlendQuery::toInstalled, true);
							blendQuery.SetFlag(SBlendQuery::higherPriority, true);
							blendQuery.SetFlag(SBlendQuery::noTransitions, true);
							uint32 fragOption = fragNode->GetSelection().optionIdx;

							//--- Flush current clip keys
							const uint32 numTracks = fragNode->GetTrackCount();
							for (int32 k = numTracks - 1; k >= 0; k--)
							{
								CSequencerTrack* pTrack = fragNode->GetTrackByIndex(k);
								int paramType = pTrack->GetParameterType();
								if ((paramType == SEQUENCER_PARAM_ANIMLAYER)
								    || (paramType == SEQUENCER_PARAM_PROCLAYER))
								{
									pTrack->SetNumKeys(0);
								}
							}

							const IAnimationDatabase* pDatabase = scopeData->context[eMEM_FragmentEditor]->database;
							SFragmentData fragData;
							const uint32 queryResult = pDatabase->Query(fragData, blendQuery, fragOption, scopeData->context[eMEM_FragmentEditor]->animSet);
							const bool foundMatch = (queryResult & eSF_Fragment) == 0;
							MannUtils::AdjustFragDataForVEGs(fragData, *scopeData->context[eMEM_FragmentEditor], eMEM_FragmentEditor, m_motionParams);

							SClipTrackContext trackContext;
							trackContext.context = scopeData->context[eMEM_FragmentEditor];
							float maxTime = FRAGMENT_MIN_TIME_RANGE;
							MannUtils::InsertClipTracksToNode(fragNode, fragData, 0.0f, -1.0f, maxTime, trackContext);
						}
					}
				}
			}

			Invalidate();
		}
	}
}

