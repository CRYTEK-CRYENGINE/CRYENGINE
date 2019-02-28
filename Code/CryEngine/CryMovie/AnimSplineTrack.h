// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __animsplinetrack_h__
#define __animsplinetrack_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryMovie/IMovieSystem.h>
#include "AnimTrack.h"

class CAnimSplineTrack : public TAnimTrack<S2DBezierKey>
{
public:
	CAnimSplineTrack(const CAnimParamType& paramType);

	virtual CAnimParamType    GetParameterType() const override { return m_paramType; };

	virtual EAnimValue        GetValueType() override           { return eAnimValue_Float; }

	virtual TMovieSystemValue GetValue(SAnimTime time) const override;
	virtual void              SetValue(SAnimTime time, const TMovieSystemValue& value) override;
	virtual TMovieSystemValue GetDefaultValue() const override;
	virtual void              SetDefaultValue(const TMovieSystemValue& value) override;

	virtual bool              Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks) override;
	virtual void              SerializeKey(S2DBezierKey& key, XmlNodeRef& keyNode, bool bLoading) override;

private:
	float SampleCurve(SAnimTime time) const;

	Vec2 m_defaultValue;

	//! Keys of float track.
	CAnimParamType m_paramType;
};

#endif // __animsplinetrack_h__
