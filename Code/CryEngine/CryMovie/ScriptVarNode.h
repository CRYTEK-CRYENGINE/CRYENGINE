// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ScriptVarNode_h__
#define __ScriptVarNode_h__
#pragma once

#include "AnimNode.h"

class CAnimScriptVarNode : public CAnimNode
{
public:
	CAnimScriptVarNode(const int id);

	virtual EAnimNodeType  GetType() const override { return eAnimNodeType_ScriptVar; }

	virtual void           Animate(SAnimContext& animContext) override;
	virtual void           CreateDefaultTracks() override;
	virtual void           OnReset() override;
	virtual void           OnResume() override;

	virtual unsigned int   GetParamCount() const override;
	virtual CAnimParamType GetParamType(unsigned int nIndex) const override;
	virtual bool           GetParamInfoFromType(const CAnimParamType& paramId, SParamInfo& info) const override;

private:
	void SetScriptValue();
	float m_value;
};

#endif // __ScriptVarNode_h__
