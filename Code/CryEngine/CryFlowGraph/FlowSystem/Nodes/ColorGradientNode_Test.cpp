// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 12:05:2009   Created by Federico Rebora
*************************************************************************/

#include "Stdafx.h"

#include "ColorGradientNode.h"

#include "Testing/MockCrySizer.h"
#include "Testing/FlowGraphTesting.h"

#include "EngineFacade/EngineFacade.h"
#include "EngineFacade/GameFacade.h"

CRY_TEST_FIXTURE(ColorGradientNodeTestFixture, CryUnit::ITestFixture, GameTesting::FlowGraphTestSuite)
{
	class CMockGameFacade : public EngineFacade::CDummyGameFacade
	{
	public:
		void TriggerFadingColorChart(const string& filePath, const float fadeInTimeInSeconds)
		{
			m_lastTriggeredColorChartPath = filePath;
			m_lastTriggeredFadeTime = fadeInTimeInSeconds;
		}

		const string& GetLastTriggeredColorChartPath() const
		{
			return m_lastTriggeredColorChartPath;
		}

		float GetLastTriggeredFadeInTime() const
		{
			return m_lastTriggeredFadeTime;
		}

	private:
		string m_lastTriggeredColorChartPath;
		float m_lastTriggeredFadeTime;
	};

	ColorGradientNodeTestFixture()
	{
		m_environment = IGameEnvironment::Create(
			IEngineFacadePtr(new EngineFacade::CDummyEngineFacade),
			IGameFacadePtr(new CMockGameFacade));

		m_colorGradientNodeToTest.reset(new CFlowNode_ColorGradient(GetEnvironment(), 0));
	}

	void ConfigureAndProcessEvent(IFlowNode::EFlowEvent event, const string filePath, const float fadeInTime)
	{
		GameTesting::CFlowNodeTestingFacility flowNodeTester(*m_colorGradientNodeToTest, CFlowNode_ColorGradient::eInputPorts_Count);

		flowNodeTester.SetInputByIndex(CFlowNode_ColorGradient::eInputPorts_TexturePath, filePath);
		flowNodeTester.SetInputByIndex(CFlowNode_ColorGradient::eInputPorts_TransitionTime, fadeInTime);

		flowNodeTester.ProcessEvent(event);
	}

	IGameEnvironment& GetEnvironment() const
	{
		return* m_environment;
	}

	EngineFacade::CDummyEngineFacade& GetEngine() const
	{
		return static_cast<EngineFacade::CDummyEngineFacade&>(GetEnvironment().GetEngine());
	}

	CMockGameFacade& GetGame() const
	{
		return static_cast<CMockGameFacade&>(GetEnvironment().GetGame());
	}

	shared_ptr<CFlowNode_ColorGradient> m_colorGradientNodeToTest;

private:
	
	IGameEnvironmentPtr m_environment;

};

CRY_TEST_WITH_FIXTURE(TestColorGradientNode_MemoryStatisticsAreSetCorrectly, ColorGradientNodeTestFixture)
{
	CMockCrySizer sizer;
	m_colorGradientNodeToTest->GetMemoryStatistics(&sizer);

	ASSERT_ARE_EQUAL(sizeof(CFlowNode_ColorGradient), sizer.GetTotalSize());
}

CRY_TEST_WITH_FIXTURE(TestColorGradientNode_IsSetInAdvancedCategory, ColorGradientNodeTestFixture)
{
	SFlowNodeConfig flowNodeConfiguration;
	m_colorGradientNodeToTest->GetConfiguration(flowNodeConfiguration);

	ASSERT_ARE_EQUAL(static_cast<uint32>(EFLN_ADVANCED), flowNodeConfiguration.GetCategory());
}

CRY_TEST_WITH_FIXTURE(TestColorGradientNode_SetsNoOutputs, ColorGradientNodeTestFixture)
{
	SFlowNodeConfig flowNodeConfiguration;
	m_colorGradientNodeToTest->GetConfiguration(flowNodeConfiguration);

	const SOutputPortConfig* nullOutputPorts = 0;

	ASSERT_ARE_EQUAL(nullOutputPorts, flowNodeConfiguration.pOutputPorts);
}

CRY_TEST_WITH_FIXTURE(TestColorGradientNode_SetsNodeInputs, ColorGradientNodeTestFixture)
{
	SFlowNodeConfig flowNodeConfiguration;
	m_colorGradientNodeToTest->GetConfiguration(flowNodeConfiguration);

	const SInputPortConfig* inputPorts = CFlowNode_ColorGradient::inputPorts;
	ASSERT_ARE_EQUAL(inputPorts, flowNodeConfiguration.pInputPorts);
}


CRY_TEST_WITH_FIXTURE(TestColorGradientNode_SetsTriggerInput, ColorGradientNodeTestFixture)
{
	SFlowNodeConfig flowNodeConfiguration;
	m_colorGradientNodeToTest->GetConfiguration(flowNodeConfiguration);

	const SInputPortConfig inputToTest = InputPortConfig_Void("Trigger", _HELP(""));
	ASSERT_ARE_EQUAL(inputToTest, flowNodeConfiguration.pInputPorts[CFlowNode_ColorGradient::eInputPorts_Trigger]);
}


CRY_TEST_WITH_FIXTURE(TestColorGradientNode_SetsTexturePathInput, ColorGradientNodeTestFixture)
{
	SFlowNodeConfig flowNodeConfiguration;
	m_colorGradientNodeToTest->GetConfiguration(flowNodeConfiguration);

	const SInputPortConfig inputToTest = InputPortConfig<string>("TexturePath", _HELP(""));
	ASSERT_ARE_EQUAL(inputToTest, flowNodeConfiguration.pInputPorts[CFlowNode_ColorGradient::eInputPorts_TexturePath]);
}

CRY_TEST_WITH_FIXTURE(TestColorGradientNode_SetsTransitionTimeInput, ColorGradientNodeTestFixture)
{
	SFlowNodeConfig flowNodeConfiguration;
	m_colorGradientNodeToTest->GetConfiguration(flowNodeConfiguration);

	const SInputPortConfig inputToTest = InputPortConfig<float>("TransitionTime", _HELP(""));
	ASSERT_ARE_EQUAL(inputToTest, flowNodeConfiguration.pInputPorts[CFlowNode_ColorGradient::eInputPorts_TransitionTime]);
}

CRY_TEST_WITH_FIXTURE(TestColorGradientNode_InputsAreNullTerminated, ColorGradientNodeTestFixture)
{
	SFlowNodeConfig flowNodeConfiguration;
	m_colorGradientNodeToTest->GetConfiguration(flowNodeConfiguration);

	const SInputPortConfig inputToTest = {0};
	ASSERT_ARE_EQUAL(inputToTest, CFlowNode_ColorGradient::inputPorts[CFlowNode_ColorGradient::eInputPorts_Count]);
}

CRY_TEST_WITH_FIXTURE(TestColorGradientNode_ActivationLoadsCorrectColorChartFile1, ColorGradientNodeTestFixture)
{
	const string filePath("filePath1");
	ConfigureAndProcessEvent(IFlowNode::eFE_Activate, filePath, 0.0f);

	const string pathOfLastLoadedColorChart = GetGame().GetLastTriggeredColorChartPath();

	ASSERT_ARE_EQUAL(filePath, pathOfLastLoadedColorChart);
}

CRY_TEST_WITH_FIXTURE(TestColorGradientNode_ActivationLoadsCorrectColorChartFile2, ColorGradientNodeTestFixture)
{
	const string filePath("filePath2");
	ConfigureAndProcessEvent(IFlowNode::eFE_Activate, filePath, 0.0f);

	const string pathOfLastLoadedColorChart = GetGame().GetLastTriggeredColorChartPath();

	ASSERT_ARE_EQUAL(filePath, pathOfLastLoadedColorChart);
}

CRY_TEST_WITH_FIXTURE(TestColorGradientNode_ActivationSetsCorrectFadeInTime1, ColorGradientNodeTestFixture)
{
	const float expectedFadeInTime = 10.0f;

	const string filePath("");
	ConfigureAndProcessEvent(IFlowNode::eFE_Activate, filePath, expectedFadeInTime);

	const float resultedFadeInTime = GetGame().GetLastTriggeredFadeInTime();

	ASSERT_ARE_EQUAL(expectedFadeInTime, resultedFadeInTime);
}

CRY_TEST_WITH_FIXTURE(TestColorGradientNode_ActivationSetsCorrectFadeInTime2, ColorGradientNodeTestFixture)
{
	const float expectedFadeInTime = 5.0f;

	const string filePath("");
	ConfigureAndProcessEvent(IFlowNode::eFE_Activate, filePath, expectedFadeInTime);

	const float resultedFadeInTime = GetGame().GetLastTriggeredFadeInTime();

	ASSERT_ARE_EQUAL(expectedFadeInTime, resultedFadeInTime);
}


CRY_TEST_WITH_FIXTURE(TestColorGradientNode_OnlyActivationEventsSetUpColorCharts, ColorGradientNodeTestFixture)
{
	const string filePath("filePath");

	static const uint32 numberOfNonActivationEvents = 11;
	const IFlowNode::EFlowEvent nonActivationEvents[numberOfNonActivationEvents] =
	{
		IFlowNode::eFE_Update,
		IFlowNode::eFE_FinalActivate,
		IFlowNode::eFE_Initialize,
		IFlowNode::eFE_FinalInitialize,
		IFlowNode::eFE_SetEntityId,
		IFlowNode::eFE_Suspend,
		IFlowNode::eFE_Resume,
		IFlowNode::eFE_ConnectInputPort,
		IFlowNode::eFE_DisconnectInputPort,
		IFlowNode::eFE_ConnectOutputPort,
		IFlowNode::eFE_DisconnectOutputPort,
	};

	for (uint32 eventIndex = 0; eventIndex < numberOfNonActivationEvents; ++eventIndex)
	{
		ConfigureAndProcessEvent(nonActivationEvents[eventIndex], filePath, 0.0f);

		ASSERT_ARE_EQUAL(string(""), GetGame().GetLastTriggeredColorChartPath());
	}
}

