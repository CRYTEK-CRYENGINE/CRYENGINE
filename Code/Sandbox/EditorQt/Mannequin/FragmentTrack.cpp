// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "FragmentTrack.h"
#include "MannequinDialog.h"
#include "MannTransitionSettings.h"
#include "Controls/TransitionBrowser.h"

#include <CryGame/IGameFramework.h>
#include <ICryMannequinEditor.h>
#include <CrySerialization/CRCRef.h>
#include <CrySerialization/IArchiveHost.h>


uint32 CFragmentTrack::s_sharedKeyID = 1;
bool CFragmentTrack::s_distributingKey = false;

ColorB FRAG_TRACK_COLOUR(200, 240, 200);
ColorB CLIP_TRACK_COLOUR(220, 220, 220);
ColorB PROC_CLIP_TRACK_COLOUR(200, 200, 240);
ColorB TAG_TRACK_COLOUR(220, 220, 220);

SKeyColour FRAG_KEY_COLOUR = {
	{ 020, 240, 020 }, { 100, 250, 100 }, { 250, 250, 250 }
};
SKeyColour FRAG_TRAN_KEY_COLOUR = {
	{ 220, 120, 020 }, { 250, 150, 100 }, { 250, 250, 250 }
};
SKeyColour FRAG_TRANOUTRO_KEY_COLOUR = {
	{ 220, 180, 040 }, { 250, 200, 120 }, { 250, 250, 250 }
};
SKeyColour CLIP_KEY_COLOUR = {
	{ 220, 220, 020 }, { 250, 250, 100 }, { 250, 250, 250 }
};
SKeyColour CLIP_TRAN_KEY_COLOUR = {
	{ 220, 120, 020 }, { 250, 150, 100 }, { 250, 250, 250 }
};
SKeyColour CLIP_TRANOUTRO_KEY_COLOUR = {
	{ 200, 160, 020 }, { 220, 180, 100 }, { 250, 250, 250 }
};
SKeyColour CLIP_KEY_COLOUR_INVALID = {
	{ 220, 0, 0 }, { 250, 0, 0 }, { 250, 100, 100 }
};
SKeyColour PROC_CLIP_KEY_COLOUR = {
	{ 162, 208, 248 }, { 192, 238, 248 }, { 250, 250, 250 }
};
SKeyColour PROC_CLIP_TRAN_KEY_COLOUR = {
	{ 220, 120, 060 }, { 250, 150, 130 }, { 250, 250, 250 }
};
SKeyColour PROC_CLIP_TRANOUTRO_KEY_COLOUR = {
	{ 200, 160, 060 }, { 220, 180, 130 }, { 250, 250, 250 }
};
SKeyColour TAG_KEY_COLOUR = {
	{ 220, 220, 220 }, { 250, 250, 250 }, { 250, 250, 250 }
};

//////////////////////////////////////////////////////////////////////////
CFragmentTrack::CFragmentTrack(SScopeData& scopeData, EMannequinEditorMode editorMode)
	:
	m_scopeData(scopeData),
	m_history(NULL),
	m_editorMode(editorMode)
{
}

ColorB CFragmentTrack::GetColor() const
{
	return FRAG_TRACK_COLOUR;
}

const SKeyColour& CFragmentTrack::GetKeyColour(int key) const
{
	const CFragmentKey& fragKey = m_keys[key];
	if (fragKey.tranFlags & SFragmentBlend::ExitTransition)
	{
		return FRAG_TRANOUTRO_KEY_COLOUR;
	}
	else if (fragKey.transition)
	{
		return FRAG_TRAN_KEY_COLOUR;
	}
	else
	{
		return FRAG_KEY_COLOUR;
	}
}

const SKeyColour& CFragmentTrack::GetBlendColour(int key) const
{
	const CFragmentKey& fragKey = m_keys[key];
	if (fragKey.tranFlags & SFragmentBlend::ExitTransition)
	{
		return FRAG_TRANOUTRO_KEY_COLOUR;
	}
	else if (fragKey.transition)
	{
		return FRAG_TRAN_KEY_COLOUR;
	}
	else
	{
		return FRAG_KEY_COLOUR;
	}
}

void CFragmentTrack::SetHistory(SFragmentHistoryContext& history)
{
	m_history = &history;
}

int CFragmentTrack::GetNumSecondarySelPts(int key) const
{
	const CFragmentKey& fragKey = m_keys[key];

	if (fragKey.transition && IsKeySelected(key))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

int CFragmentTrack::GetSecondarySelectionPt(int key, float timeMin, float timeMax) const
{
	const CFragmentKey& fragKey = m_keys[key];

	if (fragKey.transition)
	{
		if ((fragKey.tranSelectTime >= timeMin) && (fragKey.tranSelectTime <= timeMax))
		{
			return eSK_SELECT_TIME;
		}
	}

	return 0;
}

int CFragmentTrack::FindSecondarySelectionPt(int& key, float timeMin, float timeMax) const
{
	const int numKeys = GetNumKeys();
	for (uint32 i = 0; i < numKeys; i++)
	{
		if (IsKeySelected(i))
		{
			const int ret = GetSecondarySelectionPt(i, timeMin, timeMax);
			if (ret != 0)
			{
				key = i;
				return ret;
			}
		}
	}

	return 0;
}

void CFragmentTrack::SetSecondaryTime(int key, int idx, float time)
{
	CFragmentKey& fragKey = m_keys[key];

	if (fragKey.transition)
	{
		int prevKey = GetPrecedingFragmentKey(key, true);
		if (prevKey >= 0)
		{
			CFragmentKey& fragKeyPrev = m_keys[prevKey];

			if (idx == eSK_SELECT_TIME)
			{
				fragKey.tranSelectTime = max(time, fragKeyPrev.m_time);
			}
		}
	}
}

float CFragmentTrack::GetSecondaryTime(int key, int idx) const
{
	const CFragmentKey& fragKey = m_keys[key];

	if (fragKey.transition)
	{
		if (idx == eSK_SELECT_TIME)
		{
			return fragKey.tranSelectTime;
		}
	}

	return 0.0f;
}

int CFragmentTrack::GetNextFragmentKey(int key) const
{
	for (int i = key + 1; i < m_keys.size(); i++)
	{
		if (!m_keys[i].transition)
		{
			return i;
		}
	}

	return -1;
}

int CFragmentTrack::GetPrecedingFragmentKey(int key, bool includeTransitions) const
{
	const CFragmentKey& fragKey = m_keys[key];
	const bool isTransition = fragKey.transition;
	const int historyItem = fragKey.historyItem;

	for (int i = key - 1; i >= 0; i--)
	{
		const CFragmentKey& fragKeyPrev = m_keys[i];
		//--- Skip the enclosing fragment key, but not any transition clips
		if ((fragKeyPrev.transition && includeTransitions)
		    || (!fragKeyPrev.transition && (fragKeyPrev.historyItem != historyItem)))
		{
			return i;
		}
	}

	return -1;
}

int CFragmentTrack::GetParentFragmentKey(int key) const
{
	const CFragmentKey& fragKey = m_keys[key];
	const bool isTransition = fragKey.transition;
	const int historyItem = fragKey.historyItem;

	for (int i = key - 1; i >= 0; i--)
	{
		const CFragmentKey& fragKeyPrev = m_keys[i];
		//--- Skip the enclosing fragment key, but not any transition clips
		if (!fragKeyPrev.transition && (fragKeyPrev.historyItem == historyItem))
		{
			return i;
		}
	}

	return -1;
}

CString CFragmentTrack::GetSecondaryDescription(int key, int idx) const
{
	if (idx == eSK_SELECT_TIME)
	{
		return "Select";
	}
	else if (idx == eSK_START_TIME)
	{
		return "Start";
	}
	return "";
}

void CFragmentTrack::InsertKeyMenuOptions(CMenu& menu, int keyID)
{
	menu.AppendMenu(MF_SEPARATOR, 0, "");
	CFragmentKey keyFrag;
	bool addSeparator = false;
	GetKey(keyID, &keyFrag);
	if (keyFrag.transition == false)
	{
		menu.AppendMenu(MF_STRING, EDIT_FRAGMENT, "Edit Fragment");
		addSeparator = true;
	}
	if (keyFrag.context)
	{
		if (keyFrag.transition)
		{
			menu.AppendMenu(MF_STRING, EDIT_TRANSITION, "Edit Transition");
			addSeparator = true;

			int prevFragKey = GetPrecedingFragmentKey(keyID, false);
			if (prevFragKey >= 0)
			{
				CFragmentKey& keyFragFrom = m_keys[prevFragKey];

				bool isMostSpecific = (keyFrag.tranTagTo == keyFrag.tagStateFull) && (keyFrag.tranTagFrom == keyFragFrom.tagStateFull)
				                      && (keyFrag.tranFragFrom != FRAGMENT_ID_INVALID) && (keyFrag.tranFragTo != FRAGMENT_ID_INVALID);
				if (!isMostSpecific)
				{
					menu.AppendMenu(MF_STRING, INSERT_TRANSITION, "Insert More Specific Transition");
				}
				else
				{
					menu.AppendMenu(MF_STRING, INSERT_TRANSITION, "Insert Additional Transition");
				}
			}
		}
		else if (keyID > 0)
		{
			menu.AppendMenu(MF_STRING, INSERT_TRANSITION, "Insert Transition");
			addSeparator = true;
		}
	}

	if (addSeparator)
	{
		menu.AppendMenu(MF_SEPARATOR);
	}
	menu.AppendMenu(MF_STRING, FIND_FRAGMENT_REFERENCES, "Find fragment transitions");
	menu.AppendMenu(MF_STRING, FIND_TAG_REFERENCES, "Find tag transitions");
}

void CFragmentTrack::OnKeyMenuOption(int menuOption, int keyID)
{
	IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();

	CFragmentKey& key = m_keys[keyID];
	SMannequinContexts* contexts = m_scopeData.mannContexts;
	const SControllerDef& contDef = *m_scopeData.mannContexts->m_controllerDef;

	switch (menuOption)
	{
	case EDIT_FRAGMENT:
		CMannequinDialog::GetCurrentInstance()->EditFragmentByKey(key, m_scopeData.contextID);
		break;
	case EDIT_TRANSITION:
		CMannequinDialog::GetCurrentInstance()->EditTransitionByKey(key, m_scopeData.contextID);
		break;
	case INSERT_TRANSITION:
		{
			int prevFragKey = GetPrecedingFragmentKey(keyID, false);
			CFragmentKey& lastKey = m_keys[prevFragKey];
			FragmentID fragFrom = lastKey.fragmentID;
			FragmentID fragTo = key.fragmentID;
			SFragTagState tagsFrom = lastKey.tagState;
			SFragTagState tagsTo = key.tagState;

			//--- Setup better defaults for to & from none fragment
			if (fragFrom == FRAGMENT_ID_INVALID)
			{
				tagsFrom = key.tagState;
			}
			else if (fragTo == FRAGMENT_ID_INVALID)
			{
				tagsTo = lastKey.tagState;
			}

			CMannTransitionSettingsDlg transitionInfoDlg("Insert Transition", fragFrom, fragTo, tagsFrom, tagsTo);
			if (transitionInfoDlg.DoModal() == IDOK)
			{
				key.tranFragFrom = fragFrom;
				key.tranFragTo = fragTo;
				key.tranTagFrom = tagsFrom;
				key.tranTagTo = tagsTo;

				//--- Add the first key from the target fragment here!
				CFragment fragmentNew;
				if (key.hasFragment || lastKey.hasFragment)
				{
					const CFragment* fragmentFrom = key.context->database->GetBestEntry(SFragmentQuery(lastKey.fragmentID, lastKey.tagState));
					const CFragment* fragmentTo = key.context->database->GetBestEntry(SFragmentQuery(key.fragmentID, key.tagState));
					if (fragmentFrom || fragmentTo)
					{
						const uint32 numLayersFrom = fragmentFrom ? fragmentFrom->m_animLayers.size() : 0;
						const uint32 numLayersTo = fragmentTo ? fragmentTo->m_animLayers.size() : 0;
						const uint32 numLayers = max(numLayersFrom, numLayersTo);
						fragmentNew.m_animLayers.resize(numLayers);
						for (uint32 i = 0; i < numLayers; i++)
						{
							fragmentNew.m_animLayers[i].resize(1);

							SAnimClip& animClipNew = fragmentNew.m_animLayers[i][0];

							if (i < numLayersTo)
							{
								animClipNew.blend = fragmentTo->m_animLayers[i][0].blend;
							}
							else
							{
								animClipNew.blend = SAnimBlend();
							}

							animClipNew.animation.animRef.SetByString(NULL);
							animClipNew.animation.flags = 0;
							animClipNew.animation.playbackSpeed = 1.0f;
							animClipNew.animation.playbackWeight = 1.0f;
						}
					}
				}

				SFragmentBlend fragBlend;
				fragBlend.selectTime = 0.0f;
				fragBlend.startTime = 0.0f;
				fragBlend.enterTime = 0.0f;
				fragBlend.pFragment = &fragmentNew;
				key.tranBlendUid = MannUtils::GetMannequinEditorManager().AddBlend(key.context->database, key.tranFragFrom, key.tranFragTo, key.tranTagFrom, key.tranTagTo, fragBlend);
				key.context->changeCount++;

				// Refresh the Transition Browser tree and show the new fragment in the Transition Editor
				CMannequinDialog::GetCurrentInstance()->TransitionBrowser()->Refresh();
				CMannequinDialog::GetCurrentInstance()->TransitionBrowser()->SelectAndOpenRecord(TTransitionID(fragFrom, fragTo, tagsFrom, tagsTo, key.tranBlendUid));
				CMannequinDialog::GetCurrentInstance()->GetDockingPaneManager()->ShowPane(CMannequinDialog::IDW_TRANSITION_EDITOR_PANE);
			}
		}
		break;
	case FIND_FRAGMENT_REFERENCES:
		{
			const char* description = NULL;
			float duration;
			GetKeyInfo(keyID, description, duration);
			CString fragmentName = description;

			int index = fragmentName.Find('(');
			fragmentName.Delete(index, fragmentName.GetLength() - index);

			CMannequinDialog::GetCurrentInstance()->FindFragmentReferences(fragmentName);
		} break;
	case FIND_TAG_REFERENCES:
		{
			const char* description = NULL;
			float duration;
			GetKeyInfo(keyID, description, duration);
			CString tagName = description;

			int startIndex = tagName.Find('(') + 1;
			int endIndex = tagName.Find(')');
			tagName.Delete(endIndex, tagName.GetLength() - endIndex);
			tagName.Delete(0, startIndex);

			CMannequinDialog::GetCurrentInstance()->FindTagReferences(tagName);
		} break;
	}
}

void CFragmentTrack::GetKeyInfo(int key, const char*& description, float& duration)
{
	CFragmentKey keyFrag;
	GetKey(key, &keyFrag);

	const SControllerDef& contDef = *m_scopeData.mannContexts->m_controllerDef;

	if (keyFrag.transition)
	{
		description = NULL;
		duration = keyFrag.clipDuration;

		const char* fragNameFrom = (keyFrag.tranFragFrom != FRAGMENT_ID_INVALID) ? contDef.m_fragmentIDs.GetTagName(keyFrag.tranFragFrom) : NULL;
		const char* fragNameTo = (keyFrag.tranFragTo != FRAGMENT_ID_INVALID) ? contDef.m_fragmentIDs.GetTagName(keyFrag.tranFragTo) : NULL;

		static char desc[128];
		if (fragNameFrom && fragNameTo)
		{
			cry_sprintf(desc, "%s->%s", fragNameFrom, fragNameTo);
		}
		else if (fragNameFrom)
		{
			cry_sprintf(desc, "%s to <Any>", fragNameFrom);
		}
		else if (fragNameTo)
		{
			cry_sprintf(desc, "<Any> to %s", fragNameTo);
		}
		else
		{
			cry_strcpy(desc, "ERROR");
		}
		description = desc;
	}
	else
	{
		if (keyFrag.fragmentID == FRAGMENT_ID_INVALID)
		{
			description = "none";
		}
		else
		{
			static char desc[128];
			static char tags[128];
			const char* fragName = contDef.m_fragmentIDs.GetTagName(keyFrag.fragmentID);
			if (keyFrag.hasFragment)
			{
				MannUtils::FlagsToTagList(tags, sizeof(tags), keyFrag.tagState, keyFrag.fragmentID, contDef);
			}
			else
			{
				cry_strcpy(tags, "<no match>");
			}
			if (keyFrag.fragOptionIdx < OPTION_IDX_RANDOM)
			{
				cry_sprintf(desc, "%s(%s - %d)", fragName, tags, keyFrag.fragOptionIdx + 1);
			}
			else
			{
				cry_sprintf(desc, "%s(%s)", fragName, tags);
			}
			desc[sizeof(desc) - 1] = '\0';
			description = desc;
		}

		int nextKey = GetNextFragmentKey(key);
		const float nextKeyTime = (nextKey >= 0) ? GetKeyTime(nextKey) : GetTimeRange().end;

		duration = nextKeyTime - keyFrag.m_time;
		if (!keyFrag.isLooping)
		{
			float transitionTime = max(0.0f, keyFrag.tranStartTime - keyFrag.m_time);
			duration = min(duration, keyFrag.clipDuration + transitionTime);
		}
	}
}

float CFragmentTrack::GetKeyDuration(const int key) const
{
	float duration = 0.0f;
	CFragmentKey keyFrag;
	GetKey(key, &keyFrag);

	const SControllerDef& contDef = *m_scopeData.mannContexts->m_controllerDef;

	if (keyFrag.transition)
	{
		duration = keyFrag.clipDuration;
	}
	else
	{
		const int nextKey = GetNextFragmentKey(key);
		const float nextKeyTime = (nextKey >= 0) ? GetKeyTime(nextKey) : GetTimeRange().end;

		duration = nextKeyTime - keyFrag.m_time;
		if (!keyFrag.isLooping)
		{
			const float transitionTime = max(0.0f, keyFrag.tranStartTime - keyFrag.m_time);
			duration = min(duration, keyFrag.clipDuration + transitionTime);
		}
	}

	return duration;
}

void CFragmentTrack::SerializeKey(CFragmentKey& key, XmlNodeRef& keyNode, bool bLoading)
{
	if (bLoading)
	{
		key.context = m_scopeData.context[m_editorMode];
		keyNode->getAttr("fragment", key.fragmentID);
		keyNode->getAttr("fragOptionIdx", key.fragOptionIdx);
		keyNode->getAttr("scopeMask", key.scopeMask);
		keyNode->getAttr("sharedID", key.sharedID);
		keyNode->getAttr("historyItem", key.historyItem);
		keyNode->getAttr("clipDuration", key.clipDuration);
		keyNode->getAttr("transitionTime", key.transitionTime);
		keyNode->getAttr("tranSelectTime", key.tranSelectTime);
		keyNode->getAttr("tranStartTime", key.tranStartTime);
		keyNode->getAttr("tranStartTimeValue", key.tranStartTimeValue);
		keyNode->getAttr("tranStartTimeRelative", key.tranStartTimeRelative);
		keyNode->getAttr("tranFragFrom", key.tranFragFrom);
		keyNode->getAttr("tranFragTo", key.tranFragTo);
		keyNode->getAttr("tranLastClipEffectiveStart", key.tranLastClipEffectiveStart);
		keyNode->getAttr("tranLastClipDuration", key.tranLastClipDuration);
		keyNode->getAttr("hasFragment", key.hasFragment);
		keyNode->getAttr("isLooping", key.isLooping);
		keyNode->getAttr("trumpPrevious", key.trumpPrevious);
		keyNode->getAttr("transition", key.transition);
		keyNode->getAttr("tranFlags", key.tranFlags);
		keyNode->getAttr("fragIndex", key.fragIndex);

		if ((key.scopeMask & BIT64(m_scopeData.scopeID)) == 0)
		{
			key.scopeMask = BIT64(m_scopeData.scopeID);
			key.fragmentID = FRAGMENT_ID_INVALID;
		}
	}
	else
	{
		// N.B. not serializing out key.context, that is set from the track information on load
		keyNode->setAttr("fragment", key.fragmentID);
		keyNode->setAttr("fragOptionIdx", key.fragOptionIdx);
		keyNode->setAttr("scopeMask", key.scopeMask);
		keyNode->setAttr("sharedID", key.sharedID);
		keyNode->setAttr("historyItem", key.historyItem);
		keyNode->setAttr("clipDuration", key.clipDuration);
		keyNode->setAttr("transitionTime", key.transitionTime);
		keyNode->setAttr("tranSelectTime", key.tranSelectTime);
		keyNode->setAttr("tranStartTime", key.tranStartTime);
		keyNode->setAttr("tranStartTimeValue", key.tranStartTimeValue);
		keyNode->setAttr("tranStartTimeRelative", key.tranStartTimeRelative);
		keyNode->setAttr("tranFragFrom", key.tranFragFrom);
		keyNode->setAttr("tranFragTo", key.tranFragTo);
		keyNode->setAttr("tranLastClipEffectiveStart", key.tranLastClipEffectiveStart);
		keyNode->setAttr("tranLastClipDuration", key.tranLastClipDuration);
		keyNode->setAttr("hasFragment", key.hasFragment);
		keyNode->setAttr("isLooping", key.isLooping);
		keyNode->setAttr("trumpPrevious", key.trumpPrevious);
		keyNode->setAttr("transition", key.transition);
		keyNode->setAttr("tranFlags", key.tranFlags);
		keyNode->setAttr("fragIndex", key.fragIndex);

		// Add some nodes to be populated below
		keyNode->addChild(XmlHelpers::CreateXmlNode("tagState"));
		keyNode->addChild(XmlHelpers::CreateXmlNode("tagStateFull"));
		keyNode->addChild(XmlHelpers::CreateXmlNode("tranTagFrom"));
		keyNode->addChild(XmlHelpers::CreateXmlNode("tranTagTo"));
	}

	const SControllerDef contDef = *(m_scopeData.mannContexts->m_controllerDef);
	MannUtils::SerializeFragTagState(key.tagState, contDef, key.fragmentID, keyNode->findChild("tagState"), bLoading);
	MannUtils::SerializeFragTagState(key.tagStateFull, contDef, key.fragmentID, keyNode->findChild("tagStateFull"), bLoading);
	MannUtils::SerializeFragTagState(key.tranTagFrom, contDef, key.fragmentID, keyNode->findChild("tranTagFrom"), bLoading);
	MannUtils::SerializeFragTagState(key.tranTagTo, contDef, key.fragmentID, keyNode->findChild("tranTagTo"), bLoading);
}

void CFragmentTrack::SelectKey(int keyID, bool select)
{
	__super::SelectKey(keyID, select);

	CFragmentKey& key = m_keys[keyID];
	if (!s_distributingKey && (key.sharedID != 0))
	{
		s_distributingKey = true;
		const uint32 numFragTracks = m_history->m_tracks.size();
		CFragmentKey& key = m_keys[keyID];
		for (uint32 f = 0; f < numFragTracks; f++)
		{
			CSequencerTrack* pTrack = m_history->m_tracks[f];

			if (pTrack && (pTrack != this) && (pTrack->GetParameterType() == SEQUENCER_PARAM_FRAGMENTID))
			{
				CFragmentTrack* fragTrack = (CFragmentTrack*)pTrack;
				const uint32 scopeID = fragTrack->GetScopeData().scopeID;
				const int numKeys = fragTrack->GetNumKeys();
				for (int nKey = 0; nKey < numKeys; nKey++)
				{
					if (fragTrack->m_keys[nKey].sharedID == key.sharedID)
					{
						fragTrack->CloneKey(nKey, key);
						break;
					}
				}
			}
		}
		s_distributingKey = false;
	}
}

void CFragmentTrack::CloneKey(int nKey, const CFragmentKey& key)
{
	CFragmentKey keyClone;
	GetKey(nKey, &keyClone);
	//--- Copy key details
	keyClone.fragmentID = key.fragmentID;
	keyClone.historyItem = key.historyItem;
	keyClone.sharedID = key.sharedID;
	keyClone.flags = key.flags;
	keyClone.tagStateFull.fragmentTags = key.tagStateFull.fragmentTags;
	keyClone.scopeMask = key.scopeMask;
	SetKey(nKey, &keyClone);
}

void CFragmentTrack::DistributeSharedKey(int keyID)
{
	s_distributingKey = true;
	const uint32 numScopes = m_scopeData.mannContexts->m_scopeData.size();

	CFragmentKey& key = m_keys[keyID];

	uint32 numSetScopes = 0;
	for (uint32 i = 0; i < numScopes; i++)
	{
		if (key.scopeMask & BIT64(i))
		{
			numSetScopes++;
		}
	}

	const bool isSharedKey = (numSetScopes > 1);
	const bool wasSharedKey = (key.sharedID != 0);
	int sharedID = key.sharedID;
	if (isSharedKey && !wasSharedKey)
	{
		key.sharedID = s_sharedKeyID++;
		sharedID = key.sharedID;
	}
	else if (!isSharedKey && wasSharedKey)
	{
		key.sharedID = 0;
	}

	if (isSharedKey || wasSharedKey)
	{
		const uint32 numFragTracks = m_history->m_tracks.size();
		for (uint32 f = 0; f < numFragTracks; f++)
		{
			CSequencerTrack* pTrack = m_history->m_tracks[f];

			if (pTrack && (pTrack != this) && (pTrack->GetParameterType() == SEQUENCER_PARAM_FRAGMENTID))
			{
				CFragmentTrack* fragTrack = (CFragmentTrack*)pTrack;
				const uint32 scopeID = fragTrack->GetScopeData().scopeID;
				const int numKeys = fragTrack->GetNumKeys();
				int nKey;
				for (nKey = 0; (nKey < numKeys) && (fragTrack->m_keys[nKey].sharedID != sharedID); nKey++)
					;

				const bool needsKey = ((key.scopeMask & BIT64(scopeID)) != 0);
				const bool hasKey = (nKey != numKeys) && (sharedID != 0);
				if (needsKey)
				{
					if (!hasKey)
					{
						//--- Insert key
						nKey = fragTrack->CreateKey(key.m_time);
					}

					fragTrack->CloneKey(nKey, key);
				}
				else
				{
					if (hasKey)
					{
						//--- Delete key
						fragTrack->RemoveKey(nKey);
					}
				}

				if (needsKey)
				{
					//--- HACK -> ensuring that we are sorted stops glitches
					fragTrack->MakeValid();
				}
			}
		}
	}

	s_distributingKey = false;
}

void CFragmentTrack::SetKey(int index, CSequencerKey* _key)
{
	CFragmentKey& key = *(CFragmentKey*)_key;
	uint32 lastID = m_keys[index].sharedID;

	if (key.scopeMask == 0)
	{
		key.scopeMask = BIT64(m_scopeData.scopeID);
	}
	__super::SetKey(index, _key);

	if (!s_distributingKey)
	{
		if (lastID != m_keys[index].sharedID)
		{
			//--- Cloning might try to dupe the sharedID, don't allow it
			m_keys[index].sharedID = 0;
		}
		DistributeSharedKey(index);
	}
}

//! Set time of specified key.

void CFragmentTrack::SetKeyTime(int index, float time)
{
	__super::SetKeyTime(index, time);

	//--- HACK -> ensuring that we are sorted stops glitches
	MakeValid();
}

SScopeData& CFragmentTrack::GetScopeData()
{
	return m_scopeData;
}

const SScopeData& CFragmentTrack::GetScopeData() const
{
	return m_scopeData;
}

bool CFragmentTrack::CanEditKey(int key) const
{
	const CFragmentKey& fragmentKey = m_keys[key];
	if (m_editorMode == eMEM_TransitionEditor)
		return false;
	else
		return !fragmentKey.transition;
}

bool CFragmentTrack::CanMoveKey(int key) const
{
	const CFragmentKey& fragmentKey = m_keys[key];
	if (m_editorMode == eMEM_TransitionEditor)
		return key > 0;
	else
		return !fragmentKey.transition;
}

bool CFragmentTrack::CanAddKey(float time) const
{
	return (m_editorMode == eMEM_Previewer);
}

bool CFragmentTrack::CanRemoveKey(int key) const
{
	return (m_editorMode == eMEM_Previewer);
}

//////////////////////////////////////////////////////////////////////////
CClipKey::CClipKey()
	: CSequencerKey(),
	historyItem(HISTORY_ITEM_INVALID),
	startTime(0.0f),
	playbackSpeed(1.0f),
	playbackWeight(1.0f),
	duration(0.0f),
	blendDuration(0.2f),
	blendOutDuration(0.2f),
	animFlags(0),
	alignToPrevious(false),
	clipType(eCT_Normal),
	blendType(eCT_Normal),
	fragIndexMain(0),
	fragIndexBlend(0),
	jointMask(0),
	referenceLength(-1.0f),
	animIsAdditive(FALSE),
	m_animSet(NULL)
{
	memset(blendChannels, 0, sizeof(blendChannels));
}

CClipKey::~CClipKey()
{
}

void CClipKey::Set(const SAnimClip& animClip, IAnimationSet* pAnimSet, const EClipType transitionType[SFragmentData::PART_TOTAL])
{
	animFlags = animClip.animation.flags | animClip.blend.flags;
	startTime = animClip.blend.startTime;
	playbackSpeed = animClip.animation.playbackSpeed;
	playbackWeight = animClip.animation.playbackWeight;
	for (uint32 i = 0; i < MANN_NUMBER_BLEND_CHANNELS; i++)
	{
		blendChannels[i] = animClip.animation.blendChannels[i];
	}
	blendDuration = animClip.blend.duration;
	fragIndexMain = animClip.part;
	fragIndexBlend = animClip.blendPart;
	clipType = transitionType[fragIndexMain];
	blendType = transitionType[fragIndexBlend];
	jointMask = animClip.animation.weightList;
	SetAnimation(animClip.animation.animRef.c_str(), pAnimSet);
	if (animClip.isVariableLength)
	{
		referenceLength = duration;
		duration = animClip.referenceLength;
	}
}

void CClipKey::SetupAnimClip(SAnimClip& animClip, float lastTime, int fragPart)
{
	const uint32 TRANSITION_ANIM_FLAGS = CA_TRANSITION_TIMEWARPING | CA_IDLE2MOVE | CA_MOVE2IDLE;

	animClip.animation.flags = animFlags & ~TRANSITION_ANIM_FLAGS;
	animClip.blend.startTime = startTime;
	animClip.blend.flags = animFlags & TRANSITION_ANIM_FLAGS;
	animClip.animation.playbackSpeed = playbackSpeed;
	animClip.animation.playbackWeight = playbackWeight;
	for (uint32 i = 0; i < MANN_NUMBER_BLEND_CHANNELS; i++)
	{
		animClip.animation.blendChannels[i] = blendChannels[i];
	}
	animClip.animation.animRef = animRef;
	animClip.animation.weightList = jointMask;
	animClip.blend.duration = blendDuration;
	animClip.blend.exitTime = alignToPrevious ? -1.0f : m_time - lastTime;
	animClip.part = fragIndexMain;
	animClip.blendPart = fragIndexBlend;
	animClip.blend.terminal = (fragIndexMain != fragPart);
}

const char* CClipKey::GetDBAPath() const
{
	if (m_animSet)
	{
		const int animId = m_animSet->GetAnimIDByCRC(animRef.crc);
		if (animId >= 0)
		{
			return m_animSet->GetDBAFilePath(animId);
		}
	}

	return NULL;
}

void CClipKey::SetAnimation(const char* szAnimName, IAnimationSet* pAnimSet)
{
	animRef.SetByString(szAnimName);
	const int id = pAnimSet ? pAnimSet->GetAnimIDByCRC(animRef.crc) : -1;
	m_animSet = pAnimSet;
	if (id >= 0)
	{
		duration = pAnimSet->GetDuration_sec(id);
		string animPath = pAnimSet->GetFilePathByID(id);
		m_fileName = animPath.substr(animPath.find_last_of('/') + 1);
		m_filePath = animPath.substr(0, animPath.find_last_of('/') + 1);
		m_fileName = PathUtil::ReplaceExtension(m_fileName.GetString(), "");
		animExtension = animPath.substr(animPath.find_last_of('.') + 1);

		m_animCache.Set(id, pAnimSet);
	}
	else
	{
		duration = 0.0f;
		m_animCache.Set(-1, NULL);
	}

	UpdateFlags();
}

void CClipKey::UpdateFlags()
{
	const int id = m_animSet ? m_animSet->GetAnimIDByCRC(animRef.crc) : -1;
	if (id >= 0)
	{
		// if an asset is in a DB, then we flag it as though it's in a PAK, because we can't hot load it anyway.
		string animPath = m_animSet->GetFilePathByID(id);
		const char* pFilePathDBA = m_animSet->GetDBAFilePath(id);
		const bool bIsOnDisk = gEnv->pCryPak->IsFileExist(animPath, ICryPak::eFileLocation_OnDisk);
		const bool bIsInsidePak = (pFilePathDBA || bIsOnDisk) ? false : gEnv->pCryPak->IsFileExist(animPath, ICryPak::eFileLocation_InPak);
		const bool bIsAdditive = (m_animSet->GetAnimationFlags(id) & CA_ASSET_ADDITIVE) != 0;
		const bool bIsLMG = (m_animSet->GetAnimationFlags(id) & CA_ASSET_LMG) != 0;

		m_fileState = eHasFile;
		m_fileState = (ESequencerKeyFileState)(m_fileState | (bIsInsidePak ? eIsInsidePak : eNone));
		m_fileState = (ESequencerKeyFileState)(m_fileState | (pFilePathDBA ? eIsInsideDB : eNone));
		m_fileState = (ESequencerKeyFileState)(m_fileState | (bIsOnDisk ? eIsOnDisk : eNone));
		animIsAdditive = bIsAdditive;
		animIsLMG = bIsLMG;
	}
	else
	{
		animIsAdditive = FALSE;
		m_fileState = eNone;
	}
}

void CClipKey::GetExtensions(std::vector<CString>& extensions, CString& editableExtension) const
{
	if (animIsLMG)
	{
		// .lmg .comb and .bspace are all LMG type
		extensions.clear();
		extensions.reserve(1);
		extensions.push_back('.' + animExtension);

		editableExtension.SetString('.' + animExtension);
	}
	else
	{
		extensions.clear();
		extensions.reserve(3);
		extensions.push_back(".i_caf");
		extensions.push_back(".ma");
		extensions.push_back(".animsettings");

		editableExtension.SetString(".ma");
	}
}

//////////////////////////////////////////////////////////////////////////
CClipTrack::CClipTrack(SScopeContextData* pContext, EMannequinEditorMode editorMode)
	:
	m_mainAnimTrack(false),
	m_pContext(pContext),
	m_editorMode(editorMode)
{
}

ColorB CClipTrack::GetColor() const
{
	return CLIP_TRACK_COLOUR;
}

const SKeyColour& CClipTrack::GetKeyColour(int key) const
{
	const CClipKey& clipKey = m_keys[key];

	if (!clipKey.HasValidFile())
		return CLIP_KEY_COLOUR_INVALID;
	else if (clipKey.clipType == eCT_TransitionOutro)
		return CLIP_TRANOUTRO_KEY_COLOUR;
	else if (clipKey.clipType == eCT_Transition)
		return CLIP_TRAN_KEY_COLOUR;
	else
		return CLIP_KEY_COLOUR;
}

const SKeyColour& CClipTrack::GetBlendColour(int key) const
{
	const CClipKey& clipKey = m_keys[key];

	if (clipKey.blendType == eCT_TransitionOutro)
		return CLIP_TRANOUTRO_KEY_COLOUR;
	else if (clipKey.blendType == eCT_Transition)
		return CLIP_TRAN_KEY_COLOUR;
	else
		return CLIP_KEY_COLOUR;
}

int CClipTrack::GetNumSecondarySelPts(int key) const
{
	if (key >= 0 && size_t(key) < m_keys.size())
	{
		return eCKSS_BlendIn;
	}
	else
	{
		return eCKSS_None;
	}
}

template<class F>
inline static int32 IsInRange(const F x, const F end1, const F end2) // this is more precise than inrange()
{
	return (x >= end1) && (x <= end2);
}

int CClipTrack::GetSecondarySelectionPt(int key, float timeMin, float timeMax) const
{
	const CClipKey& clipKey = m_keys[key];

	if (IsInRange(clipKey.m_time + clipKey.blendDuration, timeMin, timeMax))
	{
		return eCKSS_BlendIn;
	}
	else
	{
		return eCKSS_None;
	}
}

int CClipTrack::FindSecondarySelectionPt(int& key, float timeMin, float timeMax) const
{
	const int numKeys = GetNumKeys();
	for (uint32 i = 0; i < numKeys; i++)
	{
		const int ret = GetSecondarySelectionPt(i, timeMin, timeMax);
		if (ret != 0)
		{
			key = i;
			return ret;
		}
	}

	return 0;
}

void CClipTrack::SetSecondaryTime(int key, int idx, float time)
{
	if (idx == eCKSS_BlendIn)
	{
		CClipKey& clipKey = m_keys[key];
		clipKey.blendDuration = max(time - clipKey.m_time, 0.0f);
	}
}

float CClipTrack::GetSecondaryTime(int key, int id) const
{
	CRY_ASSERT(id == eCKSS_BlendIn);
	const CClipKey& clipKey = m_keys[key];
	return clipKey.m_time + clipKey.blendDuration;
}

bool CClipTrack::CanMoveSecondarySelection(int key, int id) const
{
	return CanEditKey(key);
}

void CClipTrack::InsertKeyMenuOptions(CMenu& menu, int keyID)
{
	menu.AppendMenu(MF_SEPARATOR, 0, "");
	menu.AppendMenu(MF_STRING, FIND_FRAGMENT_REFERENCES, "Find anim clip transitions");
}

void CClipTrack::ClearKeyMenuOptions(CMenu& menu, int keyID)
{
}

void CClipTrack::OnKeyMenuOption(int menuOption, int keyID)
{
	CClipKey& key = m_keys[keyID];
	switch (menuOption)
	{
	case FIND_FRAGMENT_REFERENCES:
		CMannequinDialog::GetCurrentInstance()->FindClipReferences(key);
		break;
	}
}

bool CClipTrack::CanAddKey(float time) const
{
	switch (m_editorMode)
	{
	case eMEM_Previewer:
		return false;
	case eMEM_FragmentEditor:
		return true;
	case eMEM_TransitionEditor:
		{
			const int numKeys = m_keys.size();
			bool wasLastKeyTransition = false;

			for (int i = 0; i < numKeys; i++)
			{
				const CClipKey& clipKey = m_keys[i];

				wasLastKeyTransition = (clipKey.blendType != eCT_Normal);
				if (clipKey.m_time > time)
				{
					if (wasLastKeyTransition)
					{
						return true;
					}
				}
			}
			return wasLastKeyTransition;
		}
	default:
		assert(false);
		return false;
	}
}

bool CClipTrack::CanRemoveKey(int key) const
{
	switch (m_editorMode)
	{
	case eMEM_Previewer:
		return false;
	case eMEM_FragmentEditor:
		return true;
	case eMEM_TransitionEditor:
		{
			const CClipKey& clipKey = m_keys[key];
			return (clipKey.blendType != eCT_Normal);
		}
	default:
		assert(false);
		return false;
	}
}

bool CClipTrack::CanEditKey(int key) const
{
	switch (m_editorMode)
	{
	case eMEM_Previewer:
		return false;
	case eMEM_FragmentEditor:
		return true;
	case eMEM_TransitionEditor:
		{
			const CClipKey& clipKey = m_keys[key];
			return (clipKey.blendType != eCT_Normal);
		}
	default:
		assert(false);
		return false;
	}
}

bool CClipTrack::CanMoveKey(int key) const
{
	switch (m_editorMode)
	{
	case eMEM_Previewer:
		return false;
	case eMEM_FragmentEditor:
		return true;
	case eMEM_TransitionEditor:
		{
			const CClipKey& clipKey = m_keys[key];
			return (clipKey.blendType != eCT_Normal);
		}
	default:
		assert(false);
		return false;
	}
}

int CClipTrack::CreateKey(float time)
{
	int keyID = __super::CreateKey(time);

	if (m_editorMode != eMEM_FragmentEditor)
	{
		CClipKey& newClipKey = m_keys[keyID];
		const int numKeys = m_keys.size();

		for (int i = 0; i < numKeys; i++)
		{
			const CClipKey& clipKey = m_keys[i];

			newClipKey.historyItem = clipKey.historyItem;
			if (clipKey.m_time > time)
			{
				newClipKey.fragIndexMain = newClipKey.fragIndexBlend = clipKey.fragIndexBlend;
				newClipKey.blendType = newClipKey.clipType = clipKey.blendType;
				break;
			}
			else
			{
				newClipKey.fragIndexMain = newClipKey.fragIndexBlend = clipKey.fragIndexMain;
				newClipKey.blendType = newClipKey.clipType = clipKey.clipType;
			}
		}
	}

	return keyID;
}

void CClipTrack::CheckKeyForSnappingToPrevious(int index)
{
	if (index > 0)
	{
		CClipKey& clipKey = m_keys[index];
		CClipKey& clipKeyLast = m_keys[index - 1];

		float lastEndTime = clipKeyLast.m_time + clipKeyLast.GetDuration();
		float timeDiff = fabs_tpl(clipKey.m_time - lastEndTime);
		clipKey.alignToPrevious = (timeDiff <= LOCK_TIME_DIFF) && ((clipKeyLast.animFlags & CA_LOOP_ANIMATION) == 0) && (clipKeyLast.playbackSpeed > 0.0f);
	}
}

void CClipTrack::SetKey(int index, CSequencerKey* _key)
{
	__super::SetKey(index, _key);

	CheckKeyForSnappingToPrevious(index);
}

void CClipTrack::SetKeyTime(int index, float time)
{
	__super::SetKeyTime(index, time);

	CheckKeyForSnappingToPrevious(index);
}

void CClipTrack::GetKeyInfo(int key, const char*& description, float& duration)
{
	static string desc;
	desc.clear();

	const CClipKey& clipKey = m_keys[key];

	duration = max(clipKey.GetDuration(), clipKey.blendDuration);
	if (clipKey.animRef.IsEmpty())
	{
		desc = "Blend out";
	}
	else
	{
		desc = clipKey.animRef.c_str();
	}
	description = desc.c_str();

	const float nextKeyTime = (key + 1 < m_keys.size()) ? GetKeyTime(key + 1) : GetTimeRange().end;
	const float durationToNextKey = nextKeyTime - clipKey.m_time;
	if (!clipKey.animRef.IsEmpty() && (clipKey.animFlags & CA_LOOP_ANIMATION))
	{
		duration = durationToNextKey;
	}
}

void CClipTrack::GetTooltip(int key, const char*& description, float& duration)
{
	static string desc;
	desc.clear();

	const char* szShortDesc = NULL;
	GetKeyInfo(key, szShortDesc, duration);
	desc = szShortDesc;

	static const char* const additiveString = "\nAdditive";
	static const char* const pakString = "\nFrom PAK";
	static const char* const lmgString = "\nBlend-space";
	const CClipKey& clipKey = m_keys[key];
	if (!clipKey.animRef.IsEmpty())
	{
		desc += string().Format("\nPath: %s%s", clipKey.GetFilePath().GetString(), clipKey.GetFileName().GetString());
		if (clipKey.IsAdditive())
		{
			desc += additiveString;
		}
		if (clipKey.IsLMG())
		{
			desc += lmgString;
		}
		if (clipKey.IsFileInsideDB())
		{
			if (const char* szDBAPath = clipKey.GetDBAPath())
			{
				desc += string().Format("\nDBA: %s", szDBAPath);
			}
		}
		else
		{
			desc += "\nNot in a DBA";
		}
		if (clipKey.IsFileInsidePak())
		{
			desc += pakString;
		}
	}

	description = desc.c_str();
}

float CClipTrack::GetKeyDuration(const int key) const
{
	const CClipKey& clipKey = m_keys[key];

	float duration = max(clipKey.GetDuration(), clipKey.blendDuration);
	const float nextKeyTime = (key + 1 < m_keys.size()) ? GetKeyTime(key + 1) : GetTimeRange().end;
	const float durationToNextKey = nextKeyTime - clipKey.m_time;
	if (!clipKey.animRef.IsEmpty() && (clipKey.animFlags & CA_LOOP_ANIMATION))
	{
		duration = durationToNextKey;
	}

	return duration;
}

void CClipTrack::SerializeKey(CClipKey& key, XmlNodeRef& keyNode, bool bLoading)
{
	if (bLoading)
	{
		const char* animName = keyNode->getAttr("animName");
		IAnimationSet* animSet = m_pContext ? m_pContext->animSet : NULL;
		key.SetAnimation(animName, animSet);
	}
	else
	{
		keyNode->setAttr("animName", key.animRef.c_str());
	}
	key.Serialize(keyNode, bLoading);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
/* TODO Jean: Port this to new audio system
   struct SQuerySoundDurationHelper
   : public ISoundEventListener
   {
   SQuerySoundDurationHelper(const char* const soundName, const bool isVoice)
    : m_duration(-1.f)
    , m_nAudioControlID(CryAudio::InvalidControlId)
   {
    const int voiceFlags = isVoice ? FLAG_SOUND_VOICE : 0;
    const int loadingFlags = FLAG_SOUND_DEFAULT_3D | FLAG_SOUND_LOAD_SYNCHRONOUSLY | voiceFlags;

    gEnv->pAudioSystem->GetAudioTriggerID(soundName, m_nAudioControlID);
    if (m_nAudioControlID != CryAudio::InvalidControlId)
    {
      SAudioRequest oRequest;
      SAudioObjectRequestData<eAORT_EXECUTE_TRIGGER> oRequestData(m_nAudioControlID, 0.0f);
      oRequest.nFlags = eARF_PRIORITY_HIGH | eARF_EXECUTE_BLOCKING;
      oRequest.pData = &oRequestData;
      gEnv->pAudioSystem->PushRequest(oRequest);
    }
   }

   ~SQuerySoundDurationHelper()
   {
    ClearSound();
   }

   virtual void OnSoundEvent(ESoundCallbackEvent event, ISound* pSound) override
   {
    assert(pSound == m_pSound.get());
    if (event == SOUND_EVENT_ON_INFO || event == SOUND_EVENT_ON_LOADED)
    {
      const bool looping = ((pSound->GetFlags() & FLAG_SOUND_LOOP) != 0);
      if (!looping)
      {
        const int lengthMs = pSound->GetLengthMs();
        if (lengthMs > 0)
        {
          m_duration = pSound->GetLengthMs() / 1000.f;
        }
      }

      ClearSound();
    }
   }

   float GetDuration() const
   {
    return m_duration;
   }

   private:
   void ClearSound()
   {
    if (m_pSound)
    {
      m_pSound->RemoveEventListener(this);
      m_pSound->Stop(ESoundStopMode_AtOnce);
      m_pSound.reset();
    }
   }

   private:
   float m_duration;
   TAudioControlID m_nAudioControlID;
   _smart_ptr<ISound> m_pSound;
   };
 */

//////////////////////////////////////////////////////////////////////////
CProcClipKey::CProcClipKey()
	:
	duration(0.0f),
	blendDuration(0.2f),
	historyItem(HISTORY_ITEM_INVALID),
	clipType(eCT_Normal),
	blendType(eCT_Normal),
	fragIndexMain(0),
	fragIndexBlend(0)
{
}

template<typename T>
T FindXmlProceduralClipParamValue(const XmlNodeRef& pXmlNode, const char* const paramName, const T& defaultValue)
{
	assert(pXmlNode);
	assert(paramName);
	XmlNodeRef pXmlParamNode = pXmlNode->findChild(paramName);
	if (!pXmlParamNode)
	{
		return defaultValue;
	}

	T value;
	const bool valueFound = pXmlParamNode->getAttr("value", value);
	if (!valueFound)
	{
		return defaultValue;
	}
	return value;
}

// TODO: Remove and give a more robust and maintainable solution to query duration of procedural clips in general!
// This function is a workaround to be able to display in the editor proper duration of sound clips.
void CProcClipKey::UpdateDurationBasedOnParams()
{
	/* TODO Jean: port to new audio system
	   static const IProceduralClipFactory::THash s_playSoundClipHash("PlaySound");
	   if (typeNameHash != s_playSoundClipHash)
	   {
	   return;
	   }

	   duration = -1;

	   XmlNodeRef pXml = Serialization::SaveXmlNode(*pParams, "Params");
	   if (!pXml)
	   {
	   return;
	   }

	   const XmlString soundName = FindXmlProceduralClipParamValue<XmlString>(pXml, "Sound", "");
	   if (soundName.empty())
	   {
	   return;
	   }

	   const bool isVoice = FindXmlProceduralClipParamValue<bool>(pXml, "IsVoice", false);

	   const SQuerySoundDurationHelper querySoundDurationHelper(soundName.c_str(), isVoice);
	   duration = querySoundDurationHelper.GetDuration();
	 */
}

IProceduralParamsPtr CloneProcClipParams(const IProceduralClipFactory::THash& typeNameHash, const IProceduralParamsPtr& pParams)
{
	if (!pParams)
	{
		return IProceduralParamsPtr();
	}

	IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
	IProceduralParamsPtr pNewParams = mannequinSys.GetProceduralClipFactory().CreateProceduralClipParams(typeNameHash);
	if (!pNewParams)
	{
		return IProceduralParamsPtr();
	}

	if (!Serialization::CloneBinary(*pNewParams, *pParams))
	{
		return IProceduralParamsPtr();
	}
	return pNewParams;
}

void CProcClipKey::FromProceduralEntry(const SProceduralEntry& procClip, const EClipType transitionType[SFragmentData::PART_TOTAL])
{
	typeNameHash = procClip.typeNameHash;
	blendDuration = procClip.blend.duration;
	fragIndexMain = procClip.part;
	fragIndexBlend = procClip.blendPart;
	clipType = transitionType[fragIndexMain];
	blendType = transitionType[fragIndexBlend];
	pParams = CloneProcClipParams(procClip.typeNameHash, procClip.pProceduralParams);
	UpdateDurationBasedOnParams();
}

void CProcClipKey::ToProceduralEntry(SProceduralEntry& procClip, const float lastTime, const int fragPart)
{
	procClip.typeNameHash = typeNameHash;

	procClip.blend.duration = blendDuration;
	procClip.blend.exitTime = m_time - lastTime;
	procClip.blend.terminal = (fragIndexMain != fragPart);

	procClip.part = fragIndexMain;
	procClip.blendPart = fragIndexBlend;

	procClip.pProceduralParams = CloneProcClipParams(typeNameHash, pParams);
}

//////////////////////////////////////////////////////////////////////////
CProcClipTrack::CProcClipTrack(SScopeContextData* pContext, EMannequinEditorMode editorMode)
	:
	m_pContext(pContext),
	m_editorMode(editorMode)
{
}

ColorB CProcClipTrack::GetColor() const
{
	return PROC_CLIP_TRACK_COLOUR;
}

const SKeyColour& CProcClipTrack::GetKeyColour(int key) const
{
	const CProcClipKey& clipKey = m_keys[key];

	if (clipKey.clipType == eCT_Transition)
		return PROC_CLIP_TRAN_KEY_COLOUR;
	else if (clipKey.clipType == eCT_TransitionOutro)
		return PROC_CLIP_TRANOUTRO_KEY_COLOUR;
	return PROC_CLIP_KEY_COLOUR;
}

const SKeyColour& CProcClipTrack::GetBlendColour(int key) const
{
	const CProcClipKey& clipKey = m_keys[key];

	if (clipKey.blendType == eCT_Transition)
		return PROC_CLIP_TRAN_KEY_COLOUR;
	else if (clipKey.blendType == eCT_TransitionOutro)
		return PROC_CLIP_TRANOUTRO_KEY_COLOUR;
	return PROC_CLIP_KEY_COLOUR;
}

int CProcClipTrack::GetNumSecondarySelPts(int key) const
{
	return 1;
}

int CProcClipTrack::GetSecondarySelectionPt(int key, float timeMin, float timeMax) const
{
	const CProcClipKey& clipKey = m_keys[key];

	const float blendTime = clipKey.m_time + clipKey.blendDuration;
	if ((blendTime >= timeMin) && (blendTime <= timeMax))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

int CProcClipTrack::FindSecondarySelectionPt(int& key, float timeMin, float timeMax) const
{
	const int numKeys = GetNumKeys();
	for (uint32 i = 0; i < numKeys; i++)
	{
		const int ret = GetSecondarySelectionPt(i, timeMin, timeMax);
		if (ret != 0)
		{
			key = i;
			return ret;
		}
	}

	return 0;
}

void CProcClipTrack::SetSecondaryTime(int key, int idx, float time)
{
	CProcClipKey& clipKey = m_keys[key];

	clipKey.blendDuration = max(time - clipKey.m_time, 0.0f);
}

float CProcClipTrack::GetSecondaryTime(int key, int id) const
{
	const CProcClipKey& clipKey = m_keys[key];
	return clipKey.m_time + clipKey.blendDuration;
}

bool CProcClipTrack::CanMoveSecondarySelection(int key, int id) const
{
	assert(id == 1);
	return CanEditKey(key);
}

void CProcClipTrack::InsertKeyMenuOptions(CMenu& menu, int keyID)
{
}
void CProcClipTrack::ClearKeyMenuOptions(CMenu& menu, int keyID)
{
}
void CProcClipTrack::OnKeyMenuOption(int menuOption, int keyID)
{
}

bool CProcClipTrack::CanAddKey(float time) const
{
	switch (m_editorMode)
	{
	case eMEM_Previewer:
		return false;
	case eMEM_FragmentEditor:
		return true;
	case eMEM_TransitionEditor:
		{
			const int numKeys = m_keys.size();
			bool wasLastKeyTransition = false;

			for (int i = 0; i < numKeys; i++)
			{
				const CProcClipKey& clipKey = m_keys[i];

				wasLastKeyTransition = (clipKey.blendType != eCT_Normal);
				if (clipKey.m_time > time)
				{
					if (wasLastKeyTransition)
					{
						return true;
					}
				}
			}
			return wasLastKeyTransition;
		}
	default:
		assert(false);
		return false;
	}
}

bool CProcClipTrack::CanEditKey(int key) const
{
	switch (m_editorMode)
	{
	case eMEM_Previewer:
		return false;
	case eMEM_FragmentEditor:
		return true;
	case eMEM_TransitionEditor:
		{
			const CProcClipKey& clipKey = m_keys[key];
			return (clipKey.blendType != eCT_Normal);
		}
	default:
		assert(false);
		return false;
	}

	return true;
}

bool CProcClipTrack::CanMoveKey(int key) const
{
	switch (m_editorMode)
	{
	case eMEM_Previewer:
		return false;
	case eMEM_FragmentEditor:
		return true;
	case eMEM_TransitionEditor:
		{
			const CProcClipKey& clipKey = m_keys[key];
			return (clipKey.blendType != eCT_Normal);
		}
	default:
		assert(false);
		return false;
	}

	return true;
}

int CProcClipTrack::CreateKey(float time)
{
	int keyID = __super::CreateKey(time);

	if (m_editorMode != eMEM_FragmentEditor)
	{
		CProcClipKey& newClipKey = m_keys[keyID];
		const int numKeys = m_keys.size();

		for (int i = 0; i < numKeys; i++)
		{
			const CProcClipKey& clipKey = m_keys[i];

			newClipKey.historyItem = clipKey.historyItem;
			if (clipKey.m_time > time)
			{
				newClipKey.fragIndexMain = newClipKey.fragIndexBlend = clipKey.fragIndexBlend;
				newClipKey.blendType = newClipKey.clipType = clipKey.blendType;
				break;
			}
			else
			{
				newClipKey.fragIndexMain = newClipKey.fragIndexBlend = clipKey.fragIndexMain;
				newClipKey.blendType = newClipKey.clipType = clipKey.clipType;
			}
		}
	}

	return keyID;
}

void CProcClipTrack::GetKeyInfo(int key, const char*& description, float& duration)
{
	static char desc[128];
	CProcClipKey& clipKey = m_keys[key];

	const char* name = "Blend out";
	if (!clipKey.typeNameHash.IsEmpty())
	{
		IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
		name = mannequinSys.GetProceduralClipFactory().FindTypeName(clipKey.typeNameHash);
	}

	{
		IProceduralParams::StringWrapper data;
		if (clipKey.pParams)
		{
			clipKey.pParams->GetExtraDebugInfo(data);
		}

		if (data.IsEmpty())
		{
			CRY_ASSERT(name);
			cry_sprintf(desc, "%s", name);
		}
		else
		{
			CRY_ASSERT(name);
			cry_sprintf(desc, "%s : %s", name, data.c_str());
		}
	}

	description = desc;

	duration = GetKeyDuration(key);
}

float CProcClipTrack::GetKeyDuration(const int key) const
{
	const CProcClipKey& clipKey = m_keys[key];

	const float nextKeyTime = (key + 1 < m_keys.size()) ? GetKeyTime(key + 1) : GetTimeRange().end;
	const float durationToNextKey = nextKeyTime - clipKey.m_time;

	const bool isNoneProcClip = clipKey.typeNameHash.IsEmpty();
	if (isNoneProcClip)
	{
		const float duration = min(clipKey.blendDuration, durationToNextKey);
		return duration;
	}
	else
	{
		const bool clipHasValidDuration = (0.f < clipKey.duration);
		if (clipHasValidDuration)
		{
			const float clipDuration = max(clipKey.blendDuration, clipKey.duration);
			const float duration = min(clipDuration, durationToNextKey);
			return duration;
		}
		else
		{
			const float duration = durationToNextKey;
			return duration;
		}
	}
}

void CProcClipKey::Serialize(Serialization::IArchive& ar)
{
	ar(typeNameHash, "TypeNameHash");
	ar(duration, "Duration");
	ar(blendDuration, "BlendDuration");
	ar(historyItem, "HistoryItem");
	ar(clipType, "ClipType");
	ar(blendType, "BlendType");
	ar(fragIndexMain, "FragIndexMain");
	ar(fragIndexBlend, "FragIndexBlend");

	if (ar.isInput())
	{
		pParams.reset();
		if (!typeNameHash.IsEmpty())
		{
			IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
			pParams = mannequinSys.GetProceduralClipFactory().CreateProceduralClipParams(typeNameHash);
		}
	}

	if (pParams)
	{
		ar(*pParams, "Params");
	}
}

void CProcClipTrack::SerializeKey(CProcClipKey& key, XmlNodeRef& keyNode, bool bLoading)
{
	if (bLoading)
	{
		Serialization::LoadXmlNode(key, keyNode);
	}
	else
	{
		Serialization::SaveXmlNode(keyNode, key);
	}
}

//////////////////////////////////////////////////////////////////////////
CTagTrack::CTagTrack(const CTagDefinition& tagDefinition)
	:
	m_tagDefinition(tagDefinition)
{
}

void CTagTrack::SetKey(int index, CSequencerKey* _key)
{
	__super::SetKey(index, _key);

	CTagKey& tagKey = m_keys[index];
	m_tagDefinition.FlagsToTagList(tagKey.tagState, tagKey.desc);
}

void CTagTrack::GetKeyInfo(int key, const char*& description, float& duration)
{
	CTagKey& tagKey = m_keys[key];
	duration = 0.0f;
	description = tagKey.desc.c_str();
}

float CTagTrack::GetKeyDuration(const int key) const
{
	return 0.0f;
}

void CTagTrack::InsertKeyMenuOptions(CMenu& menu, int keyID)
{
}

void CTagTrack::ClearKeyMenuOptions(CMenu& menu, int keyID)
{
}

void CTagTrack::OnKeyMenuOption(int menuOption, int keyID)
{
}

void CTagTrack::SerializeKey(CTagKey& key, XmlNodeRef& keyNode, bool bLoading)
{
	if (bLoading)
	{
		key.desc = keyNode->getAttr("tagState");
		m_tagDefinition.TagListToFlags(key.desc, key.tagState);
	}
	else
	{
		keyNode->setAttr("tagState", key.desc);
	}
}

ColorB CTagTrack::GetColor() const
{
	return TAG_TRACK_COLOUR;
}

const SKeyColour& CTagTrack::GetKeyColour(int key) const
{
	return TAG_KEY_COLOUR;
}

const SKeyColour& CTagTrack::GetBlendColour(int key) const
{
	return TAG_KEY_COLOUR;
}

//////////////////////////////////////////////////////////////////////////
CTransitionPropertyTrack::CTransitionPropertyTrack(SScopeData& scopeData)
	: m_scopeData(scopeData)
{
}

void CTransitionPropertyTrack::SetKey(int index, CSequencerKey* _key)
{
	__super::SetKey(index, _key);

	CTransitionPropertyKey& tagKey = m_keys[index];
}

void CTransitionPropertyTrack::GetKeyInfo(int key, const char*& description, float& duration)
{
	CTransitionPropertyKey& propKey = m_keys[key];
	duration = propKey.duration;

	const SControllerDef& contDef = *m_scopeData.mannContexts->m_controllerDef;

	description = NULL;

	const char* fragNameFrom = (propKey.blend.fragmentFrom != FRAGMENT_ID_INVALID) ? contDef.m_fragmentIDs.GetTagName(propKey.blend.fragmentFrom) : NULL;
	const char* fragNameTo = (propKey.blend.fragmentTo != FRAGMENT_ID_INVALID) ? contDef.m_fragmentIDs.GetTagName(propKey.blend.fragmentTo) : NULL;

	const char* pszPropName[CTransitionPropertyKey::eTP_Count] = {
		"Select Time",
		"Earliest Start Time",
		"Transition",
	};
	const char* propName = (propKey.prop >= 0 && propKey.prop < CTransitionPropertyKey::eTP_Count) ? pszPropName[propKey.prop] : NULL;

	static char desc[128];
	if (propName && fragNameFrom && fragNameTo)
	{
		cry_sprintf(desc, "%s->%s %s", fragNameFrom, fragNameTo, propName);
	}
	else if (propName && fragNameFrom)
	{
		cry_sprintf(desc, "%s to <Any>", fragNameFrom);
	}
	else if (propName && fragNameTo)
	{
		cry_sprintf(desc, "<Any> to %s", fragNameTo);
	}
	else
	{
		cry_strcpy(desc, "ERROR");
	}
	description = desc;
}

float CTransitionPropertyTrack::GetKeyDuration(const int key) const
{
	const CTransitionPropertyKey& propKey = m_keys[key];
	return propKey.duration;
}

bool CTransitionPropertyTrack::CanEditKey(int key) const
{
	return true;
}

bool CTransitionPropertyTrack::CanMoveKey(int key) const
{
	const CTransitionPropertyKey& propKey = m_keys[key];
	return propKey.prop != CTransitionPropertyKey::eTP_Transition;
}

void CTransitionPropertyTrack::InsertKeyMenuOptions(CMenu& menu, int keyID)
{
}

void CTransitionPropertyTrack::ClearKeyMenuOptions(CMenu& menu, int keyID)
{
}

void CTransitionPropertyTrack::OnKeyMenuOption(int menuOption, int keyID)
{
}

void CTransitionPropertyTrack::SerializeKey(CTransitionPropertyKey& key, XmlNodeRef& keyNode, bool bLoading)
{
	if (bLoading)
	{
		// Nobody should be copying or pasting this track - change callback will restore it
		OnChange();
	}
}

ColorB CTransitionPropertyTrack::GetColor() const
{
	return TAG_TRACK_COLOUR;
}

const SKeyColour& CTransitionPropertyTrack::GetKeyColour(int key) const
{
	return TAG_KEY_COLOUR;
}

const SKeyColour& CTransitionPropertyTrack::GetBlendColour(int key) const
{
	return TAG_KEY_COLOUR;
}

const int CTransitionPropertyTrack::GetNumBlendsForKey(int index) const
{
	const CTransitionPropertyKey& propKey = m_keys[index];
	if (m_scopeData.context[eMEM_TransitionEditor] == NULL)
		return 0;
	return m_scopeData.context[eMEM_TransitionEditor]->database->GetNumBlends(propKey.blend.fragmentFrom, propKey.blend.fragmentTo, propKey.blend.tagStateFrom, propKey.blend.tagStateTo);
}

const SFragmentBlend* CTransitionPropertyTrack::GetBlendForKey(int index) const
{
	const CTransitionPropertyKey& propKey = m_keys[index];
	if (m_scopeData.context[eMEM_TransitionEditor] == NULL)
		return 0;
	return m_scopeData.context[eMEM_TransitionEditor]->database->GetBlend(propKey.blend.fragmentFrom, propKey.blend.fragmentTo, propKey.blend.tagStateFrom, propKey.blend.tagStateTo, propKey.blend.blendIdx);
}

const SFragmentBlend* CTransitionPropertyTrack::GetAlternateBlendForKey(int index, int blendIdx) const
{
	const CTransitionPropertyKey& propKey = m_keys[index];
	if (m_scopeData.context[eMEM_TransitionEditor] == NULL)
		return 0;
	return m_scopeData.context[eMEM_TransitionEditor]->database->GetBlend(propKey.blend.fragmentFrom, propKey.blend.fragmentTo, propKey.blend.tagStateFrom, propKey.blend.tagStateTo, blendIdx);
}

void CTransitionPropertyTrack::UpdateBlendForKey(int index, SFragmentBlend& blend) const
{
	const CTransitionPropertyKey& propKey = m_keys[index];
	MannUtils::GetMannequinEditorManager().SetBlend(m_scopeData.context[eMEM_TransitionEditor]->database, propKey.blend.fragmentFrom, propKey.blend.fragmentTo, propKey.blend.tagStateFrom, propKey.blend.tagStateTo, propKey.blend.blendUid, blend);

	CMannequinDialog& mannDialog = *CMannequinDialog::GetCurrentInstance();
	mannDialog.TransitionBrowser()->Refresh();
}

//////////////////////////////////////////////////////////////////////////
CParamTrack::CParamTrack()
{
}

void CParamTrack::GetKeyInfo(int key, const char*& description, float& duration)
{
	static char desc[128];

	CParamKey& paramKey = m_keys[key];
	duration = 0.0f;
	description = paramKey.name;
}

float CParamTrack::GetKeyDuration(const int key) const
{
	return 0.0f;
}

void CParamTrack::InsertKeyMenuOptions(CMenu& menu, int keyID)
{
}
void CParamTrack::ClearKeyMenuOptions(CMenu& menu, int keyID)
{
}
void CParamTrack::OnKeyMenuOption(int menuOption, int keyID)
{
}
void CParamTrack::SerializeKey(CParamKey& key, XmlNodeRef& keyNode, bool bLoading)
{
	if (bLoading)
	{
		keyNode->getAttr("paramCRC", key.parameter.crc);
		keyNode->getAttr("paramQuatVec", key.parameter.value.q.v);
		keyNode->getAttr("paramQuatScalar", key.parameter.value.q.w);
		keyNode->getAttr("paramVec", key.parameter.value.t);
		keyNode->getAttr("name", key.name);
		keyNode->getAttr("historyItem", key.historyItem);
		keyNode->getAttr("isLocation", key.isLocation);
	}
	else
	{
		keyNode->setAttr("paramCRC", key.parameter.crc);
		keyNode->setAttr("paramQuatVec", key.parameter.value.q.v);
		keyNode->setAttr("paramQuatScalar", key.parameter.value.q.w);
		keyNode->setAttr("paramVec", key.parameter.value.t);
		keyNode->setAttr("name", key.name);
		keyNode->setAttr("historyItem", key.historyItem);
		keyNode->setAttr("isLocation", key.isLocation);
	}
}

ColorB            CParamTrack::GetColor() const { return TAG_TRACK_COLOUR; }
const SKeyColour& CParamTrack::GetKeyColour(int key) const
{
	return TAG_KEY_COLOUR;
}
const SKeyColour& CParamTrack::GetBlendColour(int key) const
{
	return TAG_KEY_COLOUR;
}

