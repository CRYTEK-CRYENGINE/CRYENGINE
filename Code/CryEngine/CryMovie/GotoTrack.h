// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef DiscreteFloatTrack_h__
#define DiscreteFloatTrack_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryMovie/IMovieSystem.h>
#include "AnimTrack.h"

/** Goto track, every key on this track negates boolean value.
 */
class CGotoTrack : public TAnimTrack<SDiscreteFloatKey>
{
public:
	CGotoTrack();

	virtual EAnimValue        GetValueType() override           { return eAnimValue_DiscreteFloat; }
	virtual CAnimParamType    GetParameterType() const override { return eAnimParamType_Goto; }

	virtual TMovieSystemValue GetValue(SAnimTime time) const override;
	virtual void              SetValue(SAnimTime time, const TMovieSystemValue& value) override;
	virtual void              SetDefaultValue(const TMovieSystemValue& value) override;

	virtual void              SerializeKey(SDiscreteFloatKey& key, XmlNodeRef& keyNode, bool bLoading) override;

private:
	float m_defaultValue;
};

#endif // DiscreteFloatTrack_h__
