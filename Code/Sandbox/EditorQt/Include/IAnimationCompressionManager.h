// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct IAnimationCompressionManager
{
	virtual bool IsEnabled() const = 0;
	virtual void UpdateLocalAnimations() = 0;

	// animationName is defined as a path withing game data folder, e.g.
	// "animations/alien/cerberus/cerberus_activation.caf"
	virtual void QueueAnimationCompression(const char* animationName) = 0;
};

