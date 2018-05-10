// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SequenceAnalyzerNodes.h"
#include "FragmentTrack.h"
#include "MannequinDialog.h"
#include "Controls/PreviewerPage.h"

const int NUM_SCOPE_PARAM_IDS = 4;
CSequencerNode::SParamInfo g_scopeParamInfo[NUM_SCOPE_PARAM_IDS] =
{
	CSequencerNode::SParamInfo("TransitionProperties", SEQUENCER_PARAM_TRANSITIONPROPS, 0),
	CSequencerNode::SParamInfo("FragmentId",           SEQUENCER_PARAM_FRAGMENTID,      0),
	CSequencerNode::SParamInfo("AnimLayer",            SEQUENCER_PARAM_ANIMLAYER,       CSequencerNode::PARAM_MULTIPLE_TRACKS),
	CSequencerNode::SParamInfo("ProcLayer",            SEQUENCER_PARAM_PROCLAYER,       CSequencerNode::PARAM_MULTIPLE_TRACKS)
};

const int NUM_GLOBAL_PARAM_IDS = 2;
CSequencerNode::SParamInfo g_scopeParamInfoGlobal[NUM_GLOBAL_PARAM_IDS] =
{
	CSequencerNode::SParamInfo("TagState", SEQUENCER_PARAM_TAGS,   0),
	CSequencerNode::SParamInfo("Params",   SEQUENCER_PARAM_PARAMS, 0)
};

const int MI_SET_CONTEXT_NONE = 10000;
const int MI_SET_CONTEXT_BASE = 10001;

//////////////////////////////////////////////////////////////////////////
CRootNode::CRootNode(CSequencerSequence* sequence, const SControllerDef& controllerDef)
	: CSequencerNode(sequence, controllerDef)
{
}

CRootNode::~CRootNode()
{
}

int CRootNode::GetParamCount() const
{
	return NUM_GLOBAL_PARAM_IDS;
}

bool CRootNode::GetParamInfo(int nIndex, SParamInfo& info) const
{
	if (nIndex < NUM_GLOBAL_PARAM_IDS)
	{
		info = g_scopeParamInfoGlobal[nIndex];
		return true;
	}
	return false;
}

CSequencerTrack* CRootNode::CreateTrack(ESequencerParamType nParamId)
{
	CSequencerTrack* newTrack = NULL;
	switch (nParamId)
	{
	case SEQUENCER_PARAM_TAGS:
		newTrack = new CTagTrack(m_controllerDef.m_tags);
		break;
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

//////////////////////////////////////////////////////////////////////////

CScopeNode::CScopeNode(CSequencerSequence* sequence, SScopeData* pScopeData, EMannequinEditorMode mode)
	: CSequencerNode(sequence, *pScopeData->mannContexts->m_controllerDef)
	, m_pScopeData(pScopeData)
	, m_mode(mode)
{
	assert(m_mode == eMEM_Previewer || m_mode == eMEM_TransitionEditor);
}

void CScopeNode::InsertMenuOptions(CMenu& menu)
{
	menu.AppendMenu(MF_SEPARATOR, 0, "");
	menuSetContext.CreatePopupMenu();

	uint32 contextID = m_pScopeData->contextID;
	uint32 numContextDatas = m_pScopeData->mannContexts->m_contextData.size();
	uint32 numMatchingContexts = 0;
	for (uint32 i = 0; i < numContextDatas; i++)
	{
		if (m_pScopeData->mannContexts->m_contextData[i].contextID == contextID)
		{
			menuSetContext.AppendMenu(MF_STRING, MI_SET_CONTEXT_BASE + i, m_pScopeData->mannContexts->m_contextData[i].name);
			numMatchingContexts++;
		}
	}
	if ((numMatchingContexts > 0) && (contextID > 0))
	{
		menuSetContext.AppendMenu(MF_STRING, MI_SET_CONTEXT_NONE, "None");
		numMatchingContexts++;
	}
	const int flags = (numMatchingContexts > 1) ? 0 : MF_DISABLED;
	menu.AppendMenu(MF_POPUP | flags, (UINT_PTR)menuSetContext.m_hMenu, "Change Context");
}

void CScopeNode::ClearMenuOptions(CMenu& menu)
{
	menuSetContext.DestroyMenu();
}

void CScopeNode::OnMenuOption(int menuOption)
{
	if (menuOption == MI_SET_CONTEXT_NONE)
	{
		CMannequinDialog::GetCurrentInstance()->ClearContextData(m_mode, m_pScopeData->contextID);
		if (m_mode == eMEM_Previewer)
		{
			CMannequinDialog::GetCurrentInstance()->Previewer()->SetUIFromHistory();
		}
		else if (m_mode == eMEM_TransitionEditor)
		{
			CMannequinDialog::GetCurrentInstance()->TransitionEditor()->SetUIFromHistory();
		}
	}
	else if ((menuOption >= MI_SET_CONTEXT_BASE) && ((menuOption < MI_SET_CONTEXT_BASE + 100)))
	{
		int scopeContext = menuOption - MI_SET_CONTEXT_BASE;
		CMannequinDialog::GetCurrentInstance()->EnableContextData(m_mode, scopeContext);
		if (m_mode == eMEM_Previewer)
		{
			CMannequinDialog::GetCurrentInstance()->Previewer()->SetUIFromHistory();
		}
		else if (m_mode == eMEM_TransitionEditor)
		{
			CMannequinDialog::GetCurrentInstance()->TransitionEditor()->SetUIFromHistory();
		}
	}
}

IEntity* CScopeNode::GetEntity()
{
	return m_pScopeData->context[m_mode] ? m_pScopeData->context[m_mode]->viewData[m_mode].entity : NULL;
}

int CScopeNode::GetParamCount() const
{
	return NUM_SCOPE_PARAM_IDS;
}

bool CScopeNode::GetParamInfo(int nIndex, SParamInfo& info) const
{
	if (nIndex < NUM_SCOPE_PARAM_IDS)
	{
		info = g_scopeParamInfo[nIndex];
		return true;
	}
	return false;
}

CSequencerTrack* CScopeNode::CreateTrack(ESequencerParamType nParamId)
{
	CSequencerTrack* newTrack = NULL;
	switch (nParamId)
	{
	case SEQUENCER_PARAM_ANIMLAYER:
		newTrack = new CClipTrack(m_pScopeData->context[m_mode], m_mode);
		break;
	case SEQUENCER_PARAM_PROCLAYER:
		newTrack = new CProcClipTrack(m_pScopeData->context[m_mode], m_mode);
		break;
	case SEQUENCER_PARAM_FRAGMENTID:
		newTrack = new CFragmentTrack(*m_pScopeData, m_mode);
		break;
	case SEQUENCER_PARAM_TRANSITIONPROPS:
		if (m_mode == eMEM_TransitionEditor)
		{
			newTrack = new CTransitionPropertyTrack(*m_pScopeData);
		}
		break;
	}

	if (newTrack)
	{
		AddTrack(nParamId, newTrack);
	}

	return newTrack;
}

void CScopeNode::UpdateMutedLayerMasks(uint32 mutedAnimLayerMask, uint32 mutedProcLayerMask)
{
	if (m_pScopeData && m_pScopeData->mannContexts && m_pScopeData->mannContexts->viewData[m_mode].m_pActionController)
	{
		IScope* pScope = m_pScopeData->mannContexts->viewData[m_mode].m_pActionController->GetScope(m_pScopeData->scopeID);
		if (pScope)
		{
			pScope->MuteLayers(mutedAnimLayerMask, mutedProcLayerMask);
		}
	}
}

