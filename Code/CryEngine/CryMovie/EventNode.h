// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __EVENTNODE_H__
#define __EVENTNODE_H__

#include "AnimNode.h"

class CAnimEventNode : public CAnimNode
{
public:
	CAnimEventNode(const int id);

	virtual EAnimNodeType  GetType() const override { return eAnimNodeType_Event; }

	virtual void           Animate(SAnimContext& animContext) override;
	virtual void           CreateDefaultTracks() override;
	virtual void           OnReset() override;

	virtual unsigned int   GetParamCount() const override;
	virtual CAnimParamType GetParamType(unsigned int nIndex) const override;
	virtual bool           GetParamInfoFromType(const CAnimParamType& paramId, SParamInfo& info) const override;

private:
	void SetScriptValue();

private:
	//! Last animated key in track.
	int m_lastEventKey;
};

#endif //__EVENTNODE_H__
