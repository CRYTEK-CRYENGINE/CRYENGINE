// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ----------------------------------------------------------------------------------------
//  File name:   MFXContainer.h
//  Description: Contains a set of effects to be executed (particles, sound, decals, ...)
//
////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _MFX_CONTAINER_H_
#define _MFX_CONTAINER_H_

#pragma once

#include "MFXEffectBase.h"

struct SMFXRunTimeEffectParams;
struct SMFXResourceList;

//////////////////////////////////////////////////////////////////////////

class CMFXContainer : public _i_reference_target_t
{

protected:
	typedef std::vector<TMFXEffectBasePtr> TMFXEffects;

public:

	struct SParams
	{
		SParams()
			: effectId(InvalidEffectId)
			, delay(0.0f)
		{

		}

		TMFXNameId   libraryName;     // only top level effects have valid lib name
		TMFXNameId   name;            // only top level effects have valid effect name
		TMFXEffectId effectId;        // only top level effects have valid ids!

		float        delay;
	};

public:

	virtual ~CMFXContainer();

	void BuildFromXML(const XmlNodeRef& paramsNode);

	void Execute(const SMFXRunTimeEffectParams& params);

	void SetCustomParameter(const char* customParameter, const SMFXCustomParamValue& customParameterValue);
	void GetResources(SMFXResourceList& resourceList) const;

	void SetLibraryAndId(const TMFXNameId& libraryName, const TMFXEffectId effectId)
	{
		m_params.libraryName = libraryName;
		m_params.effectId = effectId;
	}

	const SParams& GetParams() const
	{
		return m_params;
	}

	void PreLoadAssets();
	void ReleasePreLoadAssets();
	void GetMemoryUsage(ICrySizer* pSizer) const;

protected:

	virtual void BuildChildEffects(const XmlNodeRef& paramsNode);

private:

	void LoadParamsFromXml(const XmlNodeRef& paramsNode);

protected:
	TMFXEffects m_effects;

private:
	SParams m_params;
};

typedef _smart_ptr<CMFXContainer> TMFXContainerPtr;

//////////////////////////////////////////////////////////////////////////
// This is used by dedicated server (no sound, particles, etc...)

class CMFXDummyContainer : public CMFXContainer
{
protected:
	virtual void BuildChildEffects(const XmlNodeRef& paramsNode) override;
};

#endif // _MFX_CONTAINER_H_
