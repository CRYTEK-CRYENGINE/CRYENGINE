// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

//! Manages overload values (eg CPU,GPU etc).
//! 1.0="everything is ok"  0.0="very bad frame rate".
//! Various systems can use this information and control what is currently in the scene.
struct IOverloadSceneManager
{
public:
	// <interfuscator:shuffle>
	virtual ~IOverloadSceneManager() {}

	virtual void Reset() = 0;
	virtual void Update() = 0;

	//! Override auto-calculated scale to reach targetfps.
	//! \param frameScale Clamped to internal min/max values.
	//! \param dt Length of time in seconds to transition.
	virtual void OverrideScale(float frameScale, float dt) = 0;

	//! Go back to auto-calculated scale from an overridden scale.
	virtual void ResetScale(float dt) = 0;
	// </interfuscator:shuffle>
};

//! \endcond