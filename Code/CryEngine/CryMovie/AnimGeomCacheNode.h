// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ANIMGEOMCACHENODE_H__
#define __ANIMGEOMCACHENODE_H__

#pragma once

#if defined(USE_GEOM_CACHES)
	#include "EntityNode.h"

class CAnimGeomCacheNode : public CAnimEntityNode
{
public:
	CAnimGeomCacheNode(const int id);
	~CAnimGeomCacheNode();
	static void            Initialize();

	virtual EAnimNodeType  GetType() const override { return eAnimNodeType_GeomCache; }
	virtual void           Animate(SAnimContext& animContext) override;
	virtual void           CreateDefaultTracks() override;
	virtual void           OnReset() override;
	virtual void           Activate(bool bActivate) override;
	virtual void           PrecacheDynamic(SAnimTime startTime) override;

	virtual unsigned int   GetParamCount() const override;
	virtual CAnimParamType GetParamType(unsigned int nIndex) const override;

private:
	virtual bool          GetParamInfoFromType(const CAnimParamType& paramId, SParamInfo& info) const override;

	IGeomCacheRenderNode* GetGeomCacheRenderNode();

	CAnimGeomCacheNode(const CAnimGeomCacheNode&);
	CAnimGeomCacheNode& operator=(const CAnimGeomCacheNode&);

	bool m_bActive;
};

#endif
#endif
