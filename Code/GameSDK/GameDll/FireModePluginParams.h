// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Fire Mode Plugin Parameters

-------------------------------------------------------------------------
History:
- 09:02:2011 : Created by Claire Allan

*************************************************************************/
#pragma once

#ifndef __FIREMODEPLUGINPARAMS_H__
#define __FIREMODEPLUGINPARAMS_H__

#include "ItemString.h"
#include "ItemParamsRegistration.h"
#include "GameTypeInfo.h"



struct IFireModePluginParams
{
	IFireModePluginParams() {}
	virtual ~IFireModePluginParams() {}

	virtual void Reset(const XmlNodeRef& paramsNode, const XmlNodeRef& actionsNode, bool defaultInit = true) = 0;
	virtual void PreLoadAssets() = 0;
	virtual void GetMemoryUsage(ICrySizer* s) const = 0;
	virtual const CGameTypeInfo* GetPluginType() = 0;
};

struct SFireModePlugin_HeatingParams : public IFireModePluginParams
{
	SFireModePlugin_HeatingParams()	{ Reset(XmlNodeRef(NULL), XmlNodeRef(NULL)); }
	virtual ~SFireModePlugin_HeatingParams() {}

	virtual const CGameTypeInfo* GetPluginType();
	static const CGameTypeInfo* GetType();

	void Reset(const XmlNodeRef& paramsNode, const XmlNodeRef& actionsNode, bool defaultInit = true);
	void PreLoadAssets();
	void GetMemoryUsage(ICrySizer * s) const;

	float		attack;
	float		duration;
	float		decay;
	float		refire_heat;

	ItemString	helper[2];
	ItemString	effect[2];

	ItemString cooldown;
	ItemString overheating;
};

struct SEffectParams
{
	SEffectParams() 
	{ 
		Reset(XmlNodeRef(NULL)); 
	}

	void Reset(const XmlNodeRef& paramsNode, bool defaultInit=true);

	void PreLoadAssets() const 
	{
#if !defined(_RELEASE)
		if(!gEnv->IsDedicated())
		{
			for (int i = 0; i < 2; i++)
			{
				if(!effect[i].empty() && !gEnv->pParticleManager->FindEffect(effect[i].c_str()))
				{
					if(gEnv->IsEditor())
						GameWarning("Particle effect not found: %s! Remove reference or fix asset", effect[i].c_str());
					else
						GameWarning("!Particle effect not found: %s! Remove reference or fix asset", effect[i].c_str());
				}
			}
		}
#endif
	}

	void GetMemoryUsage(ICrySizer * s) const
	{
		for (int i=0; i<2; i++)
		{
			s->AddObject(effect[i]);
			s->AddObject(helper[i]);
		}
	}

#define EFFECT_PARAMS_MEMBERS(f) \
	f(float, aiVisibilityRadius) \
	f(bool, helperFromAccessory) 

#define EFFECT_PARAMS_ARRAYS(f) \
	f(ItemString, 2, effect) \
	f(ItemString, 2, helper) \
	f(float, 2, scale) \
	f(float, 2, time) \
	
	REGISTER_STRUCT_WITH_ARRAYS(EFFECT_PARAMS_MEMBERS, EFFECT_PARAMS_ARRAYS, SEffectParams)
};

struct SFireModePlugin_RejectParams : public IFireModePluginParams
{
	SFireModePlugin_RejectParams()	{ Reset(XmlNodeRef(NULL), XmlNodeRef(NULL)); }
	virtual ~SFireModePlugin_RejectParams() {}

	virtual const CGameTypeInfo* GetPluginType();
	static const CGameTypeInfo* GetType();

	void Reset(const XmlNodeRef& paramsNode, const XmlNodeRef& actionsNode, bool defaultInit = true);
	void PreLoadAssets();
	void GetMemoryUsage(ICrySizer * s) const;

	SEffectParams rejectEffect;
};

struct SFireModePlugin_RecoilShakeParams : public IFireModePluginParams
{
public:
	struct SRecoilShakeParams
	{
#define RECOILSHAKE_PARAMS_MEMBERS(f) \
		f(Vec3, m_rotateShake) \
		f(Vec3, m_shiftShake)  \
		f(float, m_frequency)  \
		f(float, m_shakeTime)  \
		f(float, m_randomness) \

		void SetDefaults();
		void ReadParams(const XmlNodeRef& paramsNode);

		REGISTER_STRUCT(RECOILSHAKE_PARAMS_MEMBERS, SRecoilShakeParams);
	};

	SFireModePlugin_RecoilShakeParams()	
	{ 
		Reset(XmlNodeRef(NULL), XmlNodeRef(NULL)); 
	}
	virtual ~SFireModePlugin_RecoilShakeParams();

	virtual const CGameTypeInfo* GetPluginType();
	static const CGameTypeInfo* GetType();

	void Reset(const XmlNodeRef& paramsNode, const XmlNodeRef& actionsNode, bool defaultInit = true);
	void PreLoadAssets();
	void GetMemoryUsage(ICrySizer * s) const;

	SRecoilShakeParams m_recoilShake;
};

struct SFireModePlugin_AutoAimParams : public IFireModePluginParams
{
public:
	struct SAutoAimModeParams
	{
#define AUTOAIM_MODE_PARAMS_MEMBERS(f) \
	f(float,	m_maxDistance) \
	f(float,	m_minDistance) \
	f(float,	m_outerConeRads) \
	f(float,	m_innerConeRads)  \
	f(bool,		m_enabled) \

		void SetDefaults();
		void ReadParams(const XmlNodeRef& paramsNode);

		REGISTER_STRUCT(AUTOAIM_MODE_PARAMS_MEMBERS, SAutoAimModeParams)
	};

public:
	SFireModePlugin_AutoAimParams()
	{
		m_normalParams.SetDefaults();
		m_zoomedParams.SetDefaults();
		Reset(XmlNodeRef(NULL),XmlNodeRef(NULL));
	}
	virtual ~SFireModePlugin_AutoAimParams() {}

	virtual const CGameTypeInfo* GetPluginType();
	static const CGameTypeInfo* GetType();

	virtual void Reset(const XmlNodeRef& paramsNode, const XmlNodeRef& actionsNode, bool defaultInit = true);
	virtual void PreLoadAssets() {}
	virtual void GetMemoryUsage(ICrySizer* s) const {}

#define AUTOAIM_PARAMS_MEMBERS(f) \
	f(SAutoAimModeParams, m_normalParams) \
	f(SAutoAimModeParams, m_zoomedParams) \

	REGISTER_STRUCT(AUTOAIM_PARAMS_MEMBERS, SFireModePlugin_AutoAimParams)

};



#endif //__FIREMODEPLUGINPARAMS_H__
