// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __charactertrack_h__
#define __charactertrack_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryMovie/IMovieSystem.h>
#include "AnimTrack.h"

/** CCharacterTrack contains entity keys, when time reach event key, it fires script event or start animation etc...
 */
class CCharacterTrack : public TAnimTrack<SCharacterKey>
{
public:
	CCharacterTrack() : m_iAnimationLayer(-1) {}

	virtual CAnimParamType GetParameterType() const override { return eAnimParamType_Animation; }

	virtual bool           Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks) override;

	virtual void           SerializeKey(SCharacterKey& key, XmlNodeRef& keyNode, bool bLoading) override;

	//! Gets the duration of an animation key. If it's a looped animation,
	//! a special consideration is required to compute the actual duration.
	float        GetKeyDuration(int key) const;

	virtual int  GetAnimationLayerIndex() const override    { return m_iAnimationLayer; }
	virtual void SetAnimationLayerIndex(int index) override { m_iAnimationLayer = index; }

private:
	CAnimParamType m_paramType;
	int            m_iAnimationLayer;
};

#endif // __charactertrack_h__
