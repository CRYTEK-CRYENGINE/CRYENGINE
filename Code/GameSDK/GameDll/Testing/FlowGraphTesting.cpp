// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 06:06:2009   Created by Federico Rebora
*************************************************************************/

#include "StdAfx.h"

#include "FlowGraphTesting.h"

CryUnit::StringStream& operator << (CryUnit::StringStream& stringStream, const SInputPortConfig& portConfig)
{
    stringStream << portConfig.name << ":" << portConfig.humanName;
    return stringStream;
}

namespace GameTesting
{
	CFlowNodeTestingFacility::CFlowNodeTestingFacility( IFlowNode& nodeToTest, const unsigned int inputPortsCount ) : m_nodeToTest(nodeToTest)
		, m_inputData(0)
	{
		CRY_ASSERT(inputPortsCount > 0);

		SFlowNodeConfig flowNodeConfiguration;
		nodeToTest.GetConfiguration(flowNodeConfiguration);

		m_inputData = new TFlowInputData[inputPortsCount];
		for (unsigned int inputIndex = 0; inputIndex < inputPortsCount; ++inputIndex)
		{
			const SInputPortConfig& inputPort = flowNodeConfiguration.pInputPorts[inputIndex];
			const TFlowInputData& defaultData = inputPort.defaultData;

			m_inputData[inputIndex] = defaultData;
		}
	}

	CFlowNodeTestingFacility::~CFlowNodeTestingFacility()
	{
		delete[] m_inputData;
		m_inputData = 0;
	}

	void CFlowNodeTestingFacility::ProcessEvent( IFlowNode::EFlowEvent event )
	{
		IFlowNode::SActivationInfo activationInformation(0, 0, 0, m_inputData);
		m_nodeToTest.ProcessEvent(event, &activationInformation);
	}
}