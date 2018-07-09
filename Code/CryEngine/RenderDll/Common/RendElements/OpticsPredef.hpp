// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "OpticsElement.h"
#include "RootOpticsElement.h"
#include "Ghost.h"
#include "Glow.h"
#include "ChromaticRing.h"
#include "IrisShafts.h"
#include "Streaks.h"
#include "CameraOrbs.h"

class OpticsPredef
{
public:
	COpticsGroup PREDEF_MULTIGLASS_GHOST;

private:
	void InitPredef()
	{
		static CTexture* s_pCenterFlare = CTexture::ForName("%ENGINE%/EngineAssets/Textures/flares/lens_flare1-wide.tif", FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown);

		PREDEF_MULTIGLASS_GHOST.SetName("[Multi-glass Reflection]");

		Glow* rotStreak = new Glow("RotatingStreak");
		rotStreak->SetSize(0.28f);
		rotStreak->SetAutoRotation(true);
		rotStreak->SetFocusFactor(-0.18f);
		rotStreak->SetBrightness(6.f);
		rotStreak->SetScale(Vec2(0.034f, 25.f));
		PREDEF_MULTIGLASS_GHOST.Add(rotStreak);

		CameraOrbs* orbs = new CameraOrbs("Orbs");
		orbs->SetIllumRange(1.2f);
		orbs->SetUseLensTex(true);
		PREDEF_MULTIGLASS_GHOST.Add(orbs);

		ChromaticRing* ring = new ChromaticRing("Forward Ring");
		PREDEF_MULTIGLASS_GHOST.Add(ring);

		ChromaticRing* backRing = new ChromaticRing("Backward Ring");
		backRing->SetCompletionFading(10.f);
		backRing->SetCompletionSpanAngle(25.f);
		PREDEF_MULTIGLASS_GHOST.Add(backRing);

		CLensGhost* centerCorona = new CLensGhost("Center Corona");
		centerCorona->SetTexture(s_pCenterFlare);
		centerCorona->SetSize(0.6f);
		PREDEF_MULTIGLASS_GHOST.Add(centerCorona);
	}
	OpticsPredef()
	{
		InitPredef();
	}

	OpticsPredef(OpticsPredef const& copy);
	void operator=(OpticsPredef const& copy);

public:
	static OpticsPredef* GetInstance()
	{
		static OpticsPredef instance;
		return &instance;
	}
};
