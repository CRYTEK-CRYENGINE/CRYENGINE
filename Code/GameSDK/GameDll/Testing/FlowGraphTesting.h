// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 13:05:2009   Created by Federico Rebora
*************************************************************************/

#pragma once

#ifndef FLOW_GRAPH_TESTING_H_INCLUDED
#define FLOW_GRAPH_TESTING_H_INCLUDED

#include <CryFlowGraph/IFlowSystem.h>

CryUnit::StringStream& operator << (CryUnit::StringStream& stringStream, const SInputPortConfig& portConfig);

namespace GameTesting
{
	class CFlowNodeTestingFacility
	{
	public:
		CFlowNodeTestingFacility(IFlowNode& nodeToTest, const unsigned int inputPortsCount);
		~CFlowNodeTestingFacility();

		void ProcessEvent(IFlowNode::EFlowEvent event);

		template <class InputType>
		void SetInputByIndex(const unsigned int inputIndex, const InputType& value)
		{
			m_inputData[inputIndex].Set(value);
		}

	private:
		IFlowNode& m_nodeToTest;
		TFlowInputData* m_inputData;
	};
}

#endif //FLOW_GRAPH_TESTING_H_INCLUDED
