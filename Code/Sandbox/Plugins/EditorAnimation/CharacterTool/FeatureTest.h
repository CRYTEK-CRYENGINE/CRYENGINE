// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Cry_Math.h>
#include <CryMath/Cry_Geo.h>
#include <CryMath/Cry_Camera.h>
#include <CryCore/smartptr.h>
#include <CrySerialization/Forward.h>

struct SRenderContext;
struct AnimEventInstance;

namespace CharacterTool
{
class CharacterDocument;
using Serialization::IArchive;

struct IFeatureTest : public _i_reference_target_t
{
	virtual void Update(CharacterDocument* document)                                    {}
	// Tell if regular Character Tool update should be skipped
	virtual bool OverrideUpdate() const                                                 { return false; }
	virtual bool OverrideCameraPosition(CCamera* camera)                                { return false; }

	virtual void Render(const SRenderContext& x, CharacterDocument* document)           {}
	virtual bool AnimEvent(const AnimEventInstance& event, CharacterDocument* document) { return false; }
	virtual void Serialize(IArchive& ar)                                                {}
};

}

