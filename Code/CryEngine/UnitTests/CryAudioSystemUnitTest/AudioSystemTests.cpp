// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include <UnitTest.h>

// For INCLUDE_AUDIO_PRODUCTION_CODE. 
// Otherwise the created CryAudio::CSystem is silently broken due to ODR violation
#include <../../CryAudioSystem/stdafx.h>

#include <AudioSystem.h>
#include <ATLAudioObject.h>

class CAudioSystemTest : public ::testing::Test
{
protected:
	virtual void SetUp() override
	{
		gEnv->bTesting = true;
		m_pSystem = new CryAudio::CSystem;
	}

	virtual void TearDown() override
	{
		delete m_pSystem;
	}

	CryAudio::CSystem* m_pSystem = nullptr;
};

TEST_F(CAudioSystemTest, CreateObject)
{
	CryAudio::IObject* obj = m_pSystem->CreateObject();
	REQUIRE(obj != nullptr);
	m_pSystem->ReleaseObject(obj);
}