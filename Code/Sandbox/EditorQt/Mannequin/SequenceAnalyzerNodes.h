// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SEQUENCE_ANALAYZER_NODES__H__
#define __SEQUENCE_ANALAYZER_NODES__H__

#include "SequencerNode.h"
#include "MannequinBase.h"

class CRootNode
	: public CSequencerNode
{
public:
	CRootNode(CSequencerSequence* sequence, const SControllerDef& controllerDef);
	~CRootNode();

	virtual int              GetParamCount() const;
	virtual bool             GetParamInfo(int nIndex, SParamInfo& info) const;

	virtual CSequencerTrack* CreateTrack(ESequencerParamType nParamId);
};

class CScopeNode
	: public CSequencerNode
{
public:
	CScopeNode(CSequencerSequence* sequence, SScopeData* pScopeData, EMannequinEditorMode mode);

	CMenu menuSetContext;

	virtual void             InsertMenuOptions(CMenu& menu);

	virtual void             ClearMenuOptions(CMenu& menu);

	virtual void             OnMenuOption(int menuOption);
	virtual IEntity*         GetEntity();
	virtual int              GetParamCount() const;
	virtual bool             GetParamInfo(int nIndex, SParamInfo& info) const;

	virtual CSequencerTrack* CreateTrack(ESequencerParamType nParamId);

	virtual void             UpdateMutedLayerMasks(uint32 mutedAnimLayerMask, uint32 mutedProcLayerMask);

private:
	SScopeData*          m_pScopeData;
	EMannequinEditorMode m_mode;
};

#endif

