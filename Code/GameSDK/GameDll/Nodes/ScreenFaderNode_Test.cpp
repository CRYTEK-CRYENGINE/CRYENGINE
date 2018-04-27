// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 21:05:2009: Created by Benito G.R.

*************************************************************************/

#include "Stdafx.h"
#include "ScreenFaderNode.h"

#include "EngineFacade/EngineFacade.h"
#include "EngineFacade/GameFacade.h"
#include "GameEnvironment/GameEnvironment.h"

using namespace GameTesting;


CRY_TEST_FIXTURE(ScreenFaderNode_TestFixture, Actor2BaseTestFixture, Actor2TestSuite)
{
public:
	ScreenFaderNode_TestFixture()
			{
				m_environment = IGameEnvironment::Create(
					IEngineFacadePtr(new EngineFacade::CDummyEngineFacade),
					IGameFacadePtr(new CMockGameFacade));

				m_screenFaderNode.reset(new CFlowNode_ScreenFaderTest(*m_environment, &m_activationInfo));
				m_screenFaderNode->SetInputEntityLocalPlayer(true);
			}

	void ConfigureAndProcessEvent(IFlowNode::EFlowEvent event, bool fadeOut)
	{
		SFlowNodeConfig flowNodeConfiguration;
		m_screenFaderNode->GetConfiguration(flowNodeConfiguration);

		TFlowInputData inputData[] =
		{
			flowNodeConfiguration.pInputPorts[CFlowNode_ScreenFader::eInputPorts_FadeIn].defaultData,
			flowNodeConfiguration.pInputPorts[CFlowNode_ScreenFader::eInputPorts_FadeOut].defaultData,
			flowNodeConfiguration.pInputPorts[CFlowNode_ScreenFader::eInputPorts_FadeInTime].defaultData,
			flowNodeConfiguration.pInputPorts[CFlowNode_ScreenFader::eInputPorts_FadeOutTime].defaultData,
			flowNodeConfiguration.pInputPorts[CFlowNode_ScreenFader::eInputPorts_FadeColor].defaultData,
			flowNodeConfiguration.pInputPorts[CFlowNode_ScreenFader::eInputPorts_UseCurrentColor].defaultData,
			flowNodeConfiguration.pInputPorts[CFlowNode_ScreenFader::eInputPorts_Texture].defaultData,
			flowNodeConfiguration.pInputPorts[CFlowNode_ScreenFader::eInputPorts_Count].defaultData,
		};

		if (fadeOut)
			inputData[CFlowNode_ScreenFader::eInputPorts_FadeOut].SetUserFlag(true);
		else
			inputData[CFlowNode_ScreenFader::eInputPorts_FadeIn].SetUserFlag(true);

		IFlowNode::SActivationInfo activationInformation(0, 0, 0, inputData);
		m_screenFaderNode->ProcessEvent(event, &activationInformation);

	}

protected:

	class CMockGameFacade : public EngineFacade::CDummyGameFacade
	{
	public:
		CMockGameFacade()
			: m_fadingIn(false)
			, m_fadingOut(false)
		{

		}

		virtual void FadeInScreen(const string& texturePath, const ColorF& targetColor, float fadeTime, bool useCurrentColor = true)
		{
			m_fadingIn = true;
		}

		virtual void FadeOutScreen(const string& texturePath, const ColorF& targetColor, float fadeTime, bool useCurrentColor = true)
		{
			m_fadingOut = true;
		}

		bool IsScreenFadingIn() const
		{
			return m_fadingIn;
		}

		bool IsScreenFadingOut() const
		{
			return m_fadingOut;	
		}

	private:

		bool m_fadingIn;
		bool m_fadingOut;
	};


	CMockGameFacade& GetGame() const
	{
		return static_cast<CMockGameFacade&>(m_environment->GetGame());
	}

	class CFlowNode_ScreenFaderTest : public CFlowNode_ScreenFader
	{
	public:
		CFlowNode_ScreenFaderTest(IGameEnvironment& environment, SActivationInfo* activationInformation) 
			: CFlowNode_ScreenFader(environment, activationInformation)
		{
				m_localPlayer = false;
		}

		virtual bool InputEntityIsLocalPlayer(const SActivationInfo* const pActInfo ) const
		{
			return m_localPlayer;
		}

		void SetInputEntityLocalPlayer(bool localPlayer)
		{
			m_localPlayer = localPlayer;	
		}

	private:

		bool m_localPlayer;
	};

	IGameEnvironmentPtr m_environment;

	shared_ptr<CFlowNode_ScreenFaderTest> m_screenFaderNode;
	IFlowNode::SActivationInfo m_activationInfo;
	
};

CRY_TEST_WITH_FIXTURE(Test_ScreenFaderNode_OnFadeOutPortActive_FadesOutScreen, ScreenFaderNode_TestFixture)
{
	ConfigureAndProcessEvent(IFlowNode::eFE_Activate, true);

	ASSERT_IS_TRUE(GetGame().IsScreenFadingOut());
}

CRY_TEST_WITH_FIXTURE(Test_ScreenFaderNode_OnFadeInPortActive_FadesInScreen, ScreenFaderNode_TestFixture)
{
	ConfigureAndProcessEvent(IFlowNode::eFE_Activate, false);

	ASSERT_IS_TRUE(GetGame().IsScreenFadingIn()); 
}

CRY_TEST_WITH_FIXTURE(Test_ScreenFaderNode_DoesNotTriggerFadeInOrOut_IfNotLocalPlayer, ScreenFaderNode_TestFixture)
{
	m_screenFaderNode->SetInputEntityLocalPlayer(false);
	
	ConfigureAndProcessEvent(IFlowNode::eFE_Activate, true);
	ConfigureAndProcessEvent(IFlowNode::eFE_Activate, false);

	ASSERT_IS_FALSE(GetGame().IsScreenFadingOut());
	ASSERT_IS_FALSE(GetGame().IsScreenFadingIn()); 
}