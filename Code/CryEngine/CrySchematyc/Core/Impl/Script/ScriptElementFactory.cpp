// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/ScriptElementFactory.h"

#include "Script/Elements/ScriptActionInstance.h"
#include "Script/Elements/ScriptBase.h"
#include "Script/Elements/ScriptClass.h"
#include "Script/Elements/ScriptComponentInstance.h"
#include "Script/Elements/ScriptConstructor.h"
#include "Script/Elements/ScriptEnum.h"
#include "Script/Elements/ScriptFunction.h"
#include "Script/Elements/ScriptInterface.h"
#include "Script/Elements/ScriptInterfaceFunction.h"
#include "Script/Elements/ScriptInterfaceImpl.h"
#include "Script/Elements/ScriptInterfaceTask.h"
#include "Script/Elements/ScriptModule.h"
#include "Script/Elements/ScriptSignal.h"
#include "Script/Elements/ScriptSignalReceiver.h"
#include "Script/Elements/ScriptState.h"
#include "Script/Elements/ScriptStateMachine.h"
#include "Script/Elements/ScriptStruct.h"
#include "Script/Elements/ScriptTimer.h"
#include "Script/Elements/ScriptVariable.h"

namespace Schematyc
{
IScriptElementPtr CScriptElementFactory::CreateElement(EScriptElementType elementType)
{
	switch (elementType)
	{
	case EScriptElementType::Module:
		{
			return std::make_shared<CScriptModule>();
		}
	case EScriptElementType::Enum:
		{
			return std::make_shared<CScriptEnum>();
		}
	case EScriptElementType::Struct:
		{
			return std::make_shared<CScriptStruct>();
		}
	case EScriptElementType::Signal:
		{
			return std::make_shared<CScriptSignal>();
		}
	case EScriptElementType::Constructor:
		{
			return std::make_shared<CScriptConstructor>();
		}
	case EScriptElementType::Function:
		{
			return std::make_shared<CScriptFunction>();
		}
	case EScriptElementType::Interface:
		{
			return std::make_shared<CScriptInterface>();
		}
	case EScriptElementType::InterfaceFunction:
		{
			return std::make_shared<CScriptInterfaceFunction>();
		}
	case EScriptElementType::InterfaceTask:
		{
			return std::make_shared<CScriptInterfaceTask>();
		}
	case EScriptElementType::Class:
		{
			return std::make_shared<CScriptClass>();
		}
	case EScriptElementType::Base:
		{
			return std::make_shared<CScriptBase>();
		}
	case EScriptElementType::StateMachine:
		{
			return std::make_shared<CScriptStateMachine>();
		}
	case EScriptElementType::State:
		{
			return std::make_shared<CScriptState>();
		}
	case EScriptElementType::Variable:
		{
			return std::make_shared<CScriptVariable>();
		}
	case EScriptElementType::Timer:
		{
			return std::make_shared<CScriptTimer>();
		}
	case EScriptElementType::SignalReceiver:
		{
			return std::make_shared<CScriptSignalReceiver>();
		}
	case EScriptElementType::InterfaceImpl:
		{
			return std::make_shared<CScriptInterfaceImpl>();
		}
	case EScriptElementType::ComponentInstance:
		{
			return std::make_shared<CScriptComponentInstance>();
		}
	case EScriptElementType::ActionInstance:
		{
			return std::make_shared<CScriptActionInstance>();
		}
	}
	return IScriptElementPtr();
}
} // Schematyc
