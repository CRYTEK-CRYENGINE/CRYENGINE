// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "FragmentEditorNodes.h"
#include "FragmentTrack.h"
#include "FragmentEditor.h"

#include <CryGame/IGameFramework.h>
#include "ICryMannequinEditor.h"

const int NUM_PARAM_IDS = 3;
CSequencerNode::SParamInfo g_paramInfoFE[NUM_PARAM_IDS] =
{
	CSequencerNode::SParamInfo("FragmentId", SEQUENCER_PARAM_FRAGMENTID, 0),
	CSequencerNode::SParamInfo("AnimLayer",  SEQUENCER_PARAM_ANIMLAYER,  CSequencerNode::PARAM_MULTIPLE_TRACKS),
	CSequencerNode::SParamInfo("ProcLayer",  SEQUENCER_PARAM_PROCLAYER,  CSequencerNode::PARAM_MULTIPLE_TRACKS)
};

//////////////////////////////////////////////////////////////////////////
const int NUM_FE_ROOT_PARAM_IDS = 1;
CSequencerNode::SParamInfo g_paramInfoFERoot[NUM_FE_ROOT_PARAM_IDS] =
{
	CSequencerNode::SParamInfo("Params", SEQUENCER_PARAM_PARAMS, 0)
};

//////////////////////////////////////////////////////////////////////////
CFragmentNode::CFragmentNode(CSequencerSequence* sequence, SScopeData* pScopeData, CMannFragmentEditor* fragmentEditor)
	: CSequencerNode(sequence, *pScopeData->mannContexts->m_controllerDef),
	m_fragmentEditor(fragmentEditor),
	m_pScopeData(pScopeData),
	m_isSequenceNode(false),
	m_hasFragment(false)
{
}

void CFragmentNode::OnChange()
{
	if (m_pScopeData->context[eMEM_FragmentEditor])
	{
		m_pScopeData->context[eMEM_FragmentEditor]->changeCount++;
	}
}

uint32 CFragmentNode::GetChangeCount() const
{
	if (m_pScopeData->context[eMEM_FragmentEditor])
	{
		return m_pScopeData->context[eMEM_FragmentEditor]->changeCount;
	}
	return 0;
}

ESequencerNodeType CFragmentNode::GetType() const
{
	if (m_isSequenceNode)
	{
		return SEQUENCER_NODE_FRAGMENT;
	}
	else
	{
		return SEQUENCER_NODE_FRAGMENT_CLIPS;
	}
}

SScopeData* CFragmentNode::GetScopeData()
{
	return m_pScopeData;
}

void CFragmentNode::SetIsSequenceNode(const bool isSequenceNode)
{
	m_isSequenceNode = isSequenceNode;
	m_startExpanded = !isSequenceNode;
}

bool CFragmentNode::IsSequenceNode() const
{
	return m_isSequenceNode;
}

void CFragmentNode::SetSelection(const bool hasFragment, const SFragmentSelection& fragSelection)
{
	m_hasFragment = hasFragment;
	m_fragSelection = fragSelection;
}

bool CFragmentNode::HasFragment() const
{
	return m_hasFragment;
}

const SFragmentSelection& CFragmentNode::GetSelection() const
{
	return m_fragSelection;
}

uint32 CFragmentNode::GetNumTracksOfParamID(ESequencerParamType paramID) const
{
	uint32 numTracksOfType = 0;
	const uint32 numTracks = m_tracks.size();
	for (uint32 i = 0; i < numTracks; i++)
	{
		if (m_tracks[i].paramId == paramID)
		{
			numTracksOfType++;
		}
	}

	return numTracksOfType;
}

bool CFragmentNode::CanAddParamType(ESequencerParamType paramID) const
{
	switch (paramID)
	{
	case SEQUENCER_PARAM_ANIMLAYER:
		return !m_isSequenceNode && (GetNumTracksOfParamID(paramID) < m_pScopeData->numLayers);
		break;
	case SEQUENCER_PARAM_PROCLAYER:
		return !m_isSequenceNode;
		break;
	case SEQUENCER_PARAM_FRAGMENTID:
		return m_isSequenceNode;
		break;
	}

	return true;
}

IEntity* CFragmentNode::GetEntity()
{
	return m_pScopeData->context[eMEM_FragmentEditor]->viewData[eMEM_FragmentEditor].entity;
}

const int MI_ENABLE_SCOPE = 10000;
const int MI_DISABLE_SCOPE = 10001;

void CFragmentNode::InsertMenuOptions(CMenu& menu)
{
	menu.AppendMenu(MF_SEPARATOR, 0, "");

	if (m_isSequenceNode)
	{
		menu.AppendMenu(MF_STRING, MI_ENABLE_SCOPE, "Include Scope");
	}
	else
	{
		menu.AppendMenu(MF_STRING, MI_DISABLE_SCOPE, "Exclude Scope");
	}
}

void CFragmentNode::ClearMenuOptions(CMenu& menu)
{
}

void CFragmentNode::OnMenuOption(int menuOption)
{
	if ((menuOption == MI_ENABLE_SCOPE) || (menuOption == MI_DISABLE_SCOPE))
	{
		const bool enable = (menuOption == MI_ENABLE_SCOPE);

		m_fragmentEditor->FlushChanges();

		IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
		IMannequinEditorManager* pManEditMan = mannequinSys.GetMannequinEditorManager();
		assert(pManEditMan);

		FragmentID fragID = m_fragmentEditor->GetFragmentID();
		SFragmentDef fragDef = m_controllerDef.GetFragmentDef(fragID);
		const CTagDefinition* pFragTagDef = m_controllerDef.GetFragmentTagDef(fragID);

		const SFragTagState& fragTagState = m_fragmentEditor->GetTagState();
		SFragTagState fragTagStateBest;
		const ActionScopes* pScopeMask = fragDef.scopeMaskList.GetBestMatch(fragTagState, &m_controllerDef.m_tags, pFragTagDef, &fragTagStateBest);

		const ActionScopes scopeFlag = BIT64(m_pScopeData->scopeID);
		ActionScopes newMask = *pScopeMask;
		if (enable)
		{
			newMask |= scopeFlag;
		}
		else
		{
			newMask &= ~scopeFlag;
		}

		if (fragTagState != fragTagStateBest)
		{
			//--- Insert new
			fragDef.scopeMaskList.Insert(fragTagState, newMask);
			fragDef.scopeMaskList.Sort(m_controllerDef.m_tags, pFragTagDef);
		}
		else
		{
			*fragDef.scopeMaskList.Find(fragTagState) = newMask;
		}

		pManEditMan->SetFragmentDef(m_controllerDef, fragID, fragDef);

		m_fragmentEditor->SetCurrentFragment();
	}
}

int CFragmentNode::GetParamCount() const
{
	return NUM_PARAM_IDS;
}

bool CFragmentNode::GetParamInfo(int nIndex, SParamInfo& info) const
{
	if (nIndex < NUM_PARAM_IDS)
	{
		info = g_paramInfoFE[nIndex];

		return true;
	}
	return false;
}

CSequencerTrack* CFragmentNode::CreateTrack(ESequencerParamType nParamId)
{
	CSequencerTrack* newTrack = NULL;

	switch (nParamId)
	{
	case SEQUENCER_PARAM_ANIMLAYER:
		newTrack = new CClipTrack(m_pScopeData->context[eMEM_FragmentEditor], eMEM_FragmentEditor);
		break;
	case SEQUENCER_PARAM_PROCLAYER:
		newTrack = new CProcClipTrack(m_pScopeData->context[eMEM_FragmentEditor], eMEM_FragmentEditor);
		break;
	case SEQUENCER_PARAM_FRAGMENTID:
		newTrack = new CFragmentTrack(*m_pScopeData, eMEM_FragmentEditor);
		break;
	}

	if (newTrack)
	{
		AddTrack(nParamId, newTrack);
	}

	return newTrack;
}

bool CFragmentNode::CanAddTrackForParameter(ESequencerParamType paramId) const
{
	if (CSequencerNode::CanAddTrackForParameter(paramId))
	{
		return CanAddParamType(paramId);
	}

	return false;
}

void CFragmentNode::UpdateMutedLayerMasks(uint32 mutedAnimLayerMask, uint32 mutedProcLayerMask)
{
	if (m_pScopeData && m_pScopeData->mannContexts && m_pScopeData->mannContexts->viewData[eMEM_FragmentEditor].m_pActionController)
	{
		IScope* pScope = m_pScopeData->mannContexts->viewData[eMEM_FragmentEditor].m_pActionController->GetScope(m_pScopeData->scopeID);
		if (pScope)
		{
			pScope->MuteLayers(mutedAnimLayerMask, mutedProcLayerMask);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

CFERootNode::CFERootNode(CSequencerSequence* sequence, const SControllerDef& controllerDef)
	: CSequencerNode(sequence, controllerDef)
{
}

CFERootNode::~CFERootNode()
{
}

ESequencerNodeType CFERootNode::GetType() const
{
	return SEQUENCER_NODE_FRAGMENT_EDITOR_GLOBAL;
}

int CFERootNode::GetParamCount() const
{
	return NUM_FE_ROOT_PARAM_IDS;
}

bool CFERootNode::GetParamInfo(int nIndex, SParamInfo& info) const
{
	if (nIndex < NUM_FE_ROOT_PARAM_IDS)
	{
		info = g_paramInfoFERoot[nIndex];
		return true;
	}
	return false;
}

CSequencerTrack* CFERootNode::CreateTrack(ESequencerParamType nParamId)
{
	CSequencerTrack* newTrack = NULL;
	switch (nParamId)
	{
	case SEQUENCER_PARAM_PARAMS:
		newTrack = new CParamTrack();
		break;
	}

	if (newTrack)
	{
		AddTrack(nParamId, newTrack);
	}

	return newTrack;
}

