// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Merge with EnvUtils/EnvRegistryUtils?

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/IAbstractInterface.h"
#include "CrySchematyc2/Env/IEnvRegistry.h"

namespace Schematyc2
{
	namespace AbstractInterfaceUtils
	{
		inline size_t FindFunction(const IAbstractInterface& abstractInterface, const SGUID& guid)
		{
			for(size_t functionIdx = 0, functionCount = abstractInterface.GetFunctionCount(); functionIdx < functionCount; ++ functionIdx)
			{
				IAbstractInterfaceFunctionConstPtr	pFunction = abstractInterface.GetFunction(functionIdx);
				if(pFunction->GetGUID() == guid)
				{
					return functionIdx;
				}
			}
			return INVALID_INDEX;
		}

		inline size_t FindFunctionInputByName(const IAbstractInterfaceFunction& abstractInterfaceFunction, const char* szInputName)
		{
			SCHEMATYC2_SYSTEM_ASSERT(szInputName);
			if(szInputName)
			{
				for(size_t inputIdx = 0, inputCount = abstractInterfaceFunction.GetInputCount() ; inputIdx < inputCount ; ++ inputIdx)
				{
					if(strcmp(abstractInterfaceFunction.GetInputName(inputIdx), szInputName) == 0)
					{
						return inputIdx;
					}
				}
			}
			return INVALID_INDEX;
		}

		inline size_t FindFunctionOutputByName(const IAbstractInterfaceFunction& abstractInterfaceFunction, const char* szOutputName)
		{
			SCHEMATYC2_SYSTEM_ASSERT(szOutputName);
			if(szOutputName)
			{
				for(size_t outputIdx = 0, outputCount = abstractInterfaceFunction.GetOutputCount() ; outputIdx < outputCount ; ++ outputIdx)
				{
					if(strcmp(abstractInterfaceFunction.GetOutputName(outputIdx), szOutputName) == 0)
					{
						return outputIdx;
					}
				}
			}
			return INVALID_INDEX;
		}
	}
}
