// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CrySystem/Testing/CryTest.h>
#include "ParticleSystem.h"
#include "../ParticleEffect.h"
#include "ParticleEffect.h"

#include <CrySerialization/IArchiveHost.h>

CRY_TEST_SUITE(CryParticleSystemTest)
{
	using namespace pfx2;

#define CRY_PFX2_TEST_ASSERT(cond) \
  CRY_TEST_ASSERT(cond)


#if CRY_PFX2_GROUP_STRIDE == 4

	//These tests require the particle system to be running
	CRY_TEST_FIXTURE(CParticleEffectTests)
	{
	public:
		virtual void Init() override
		{
			m_pEffect = std::unique_ptr<pfx2::CParticleEffect>(new pfx2::CParticleEffect());
		}

		virtual void Done() override
		{
			m_pEffect.reset();
		}

		pfx2::CParticleEffect& GetEffect()
		{
			return *m_pEffect;
		}

		void LoadSimpleEffect()
		{
			const char* jsonCode =
			  "{ \"Version\": 3,\n"
			  "	\"Components\": [\n"
			  "		{ \"Component\": { \"Name\": \"Test01\" }},\n"
			  "		{ \"Component\": { \"Name\": \"Test02\" }}]}\n";
			Serialization::LoadJsonBuffer(GetEffect(), jsonCode, strlen(jsonCode));
		}

		void LoadDefaultComponent(uint componentId)
		{
			const char* jsonCode =
			  "{ \"Name\": \"Default\", \"Features\": [\n"
			  "	{ \"RenderSprites\": { \"SortMode\": \"None\" } },\n"
			  "	{ \"SpawnRate\": {\n"
			  "		\"Amount\": { \"value\": 1.0, \"modifiers\": [ ] },\n"
			  "		\"Delay\": { \"State\": false, \"Value\": { \"value\": 0.0, \"modifiers\": [ ] } },\n"
			  "		\"Duration\": { \"State\": false, \"Value\": { \"value\": 0.0, \"modifiers\": [ ] } },\n"
			  "		\"Mode\": \"ParticlesPerSecond\"} },\n"
			  "	{ \"LifeTime\": { \"LifeTime\": { \"value\": 1, \"var\": 0 } } },\n"
			  "	{ \"FieldSize\": { \"value\": { \"value\": 1, \"modifiers\": [ ] } } } ] }\n";
			Serialization::LoadJsonBuffer(*GetEffect().GetComponent(componentId), jsonCode, strlen(jsonCode));
		}

	private:
		std::unique_ptr<pfx2::CParticleEffect> m_pEffect;
	};

	CRY_TEST_WITH_FIXTURE(CParticleSystem_USimpleEffect, CParticleEffectTests)
	{
		pfx2::CParticleEffect& effect = GetEffect();
		LoadSimpleEffect();
		CRY_PFX2_TEST_ASSERT(strcmp(effect.GetComponent(0)->GetName(), "Test01") == 0);
		CRY_PFX2_TEST_ASSERT(strcmp(effect.GetComponent(1)->GetName(), "Test02") == 0);
	}

	CRY_TEST_WITH_FIXTURE(CParticleSystem_UniqueAutoName, CParticleEffectTests)
	{
		pfx2::CParticleEffect& effect = GetEffect();
		effect.AddComponent();
		effect.AddComponent();
		CRY_PFX2_TEST_ASSERT(strcmp(effect.GetComponent(0)->GetName(), "Component01") == 0);
		CRY_PFX2_TEST_ASSERT(strcmp(effect.GetComponent(1)->GetName(), "Component02") == 0);
	}

	CRY_TEST_WITH_FIXTURE(CParticleSystem_UniqueCustomName, CParticleEffectTests)
	{
		pfx2::CParticleEffect& effect = GetEffect();
		effect.AddComponent();
		effect.GetComponent(0)->SetName("Test");
		effect.AddComponent();
		effect.GetComponent(1)->SetName("Test");
		effect.AddComponent();
		effect.GetComponent(2)->SetName("Test");
		CRY_PFX2_TEST_ASSERT(strcmp(effect.GetComponent(0)->GetName(), "Test") == 0);
		CRY_PFX2_TEST_ASSERT(strcmp(effect.GetComponent(1)->GetName(), "Test1") == 0);
		CRY_PFX2_TEST_ASSERT(strcmp(effect.GetComponent(2)->GetName(), "Test2") == 0);
	}

	CRY_TEST_WITH_FIXTURE(CParticleSystem_UniqueLoadedName, CParticleEffectTests)
	{
		pfx2::CParticleEffect& effect = GetEffect();
		effect.AddComponent();
		LoadDefaultComponent(0);
		effect.AddComponent();
		LoadDefaultComponent(1);
		effect.AddComponent();
		LoadDefaultComponent(2);
		CRY_PFX2_TEST_ASSERT(strcmp(effect.GetComponent(0)->GetName(), "Default") == 0);
		CRY_PFX2_TEST_ASSERT(strcmp(effect.GetComponent(1)->GetName(), "Default1") == 0);
		CRY_PFX2_TEST_ASSERT(strcmp(effect.GetComponent(2)->GetName(), "Default2") == 0);
	}

#endif

}
