// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __LAYERNODE_H__
#define __LAYERNODE_H__

#pragma once

#include "AnimNode.h"

class CLayerNode : public CAnimNode
{
public:
	CLayerNode(const int id);
	static void            Initialize();

	virtual EAnimNodeType  GetType() const override { return eAnimNodeType_Layer; }

	virtual void           Animate(SAnimContext& animContext) override;

	virtual void           CreateDefaultTracks() override;
	virtual void           OnReset() override;
	virtual void           Activate(bool bActivate) override;
	virtual void           Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks) override;

	virtual unsigned int   GetParamCount() const override;
	virtual CAnimParamType GetParamType(unsigned int nIndex) const override;

protected:
	virtual bool GetParamInfoFromType(const CAnimParamType& paramId, SParamInfo& info) const override;

private:
	bool m_bInit;
	bool m_bPreVisibility;

};

#endif//__LAYERNODE_H__
