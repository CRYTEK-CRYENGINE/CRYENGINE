// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __COMMENTNODE_H__
#define __COMMENTNODE_H__

#pragma once

#include "AnimNode.h"

class CCommentContext;

class CCommentNode : public CAnimNode
{
public:
	CCommentNode(const int id);
	static void            Initialize();

	virtual EAnimNodeType  GetType() const override { return eAnimNodeType_Comment; }

	virtual void           CreateDefaultTracks() override;
	virtual void           Activate(bool bActivate) override;
	virtual void           Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks) override;

	virtual unsigned int   GetParamCount() const override;
	virtual CAnimParamType GetParamType(unsigned int nIndex) const override;

protected:
	virtual bool GetParamInfoFromType(const CAnimParamType& paramId, SParamInfo& info) const override;
};

#endif //__COMMENTNODE_H__
