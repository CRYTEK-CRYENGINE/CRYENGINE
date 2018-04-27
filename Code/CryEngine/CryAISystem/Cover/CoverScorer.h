// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CoverScorer_h__
#define __CoverScorer_h__
#pragma once

#include "Cover.h"

struct ICoverLocationScorer
{
	virtual ~ICoverLocationScorer(){}
	struct Params
	{
		Vec3            location;
		Vec3            direction;
		float           height;

		Vec3            userLocation;
		float           totalLength;

		ECoverUsageType usage;

		Vec3            target;
	};

	virtual float Score(const Params& params) const = 0;
};

struct DefaultCoverScorer : public ICoverLocationScorer
{
public:
	float         ScoreByDistance(const ICoverLocationScorer::Params& params) const;
	float         ScoreByDistanceToTarget(const ICoverLocationScorer::Params& params) const;
	float         ScoreByAngle(const ICoverLocationScorer::Params& params) const;
	float         ScoreByCoverage(const ICoverLocationScorer::Params& params) const;

	virtual float Score(const ICoverLocationScorer::Params& params) const;
};

#endif //__CoverScorer_h__
