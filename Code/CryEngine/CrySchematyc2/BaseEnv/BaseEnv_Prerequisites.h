// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/TemplateUtils/TemplateUtils_Delegate.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_PreprocessorUtils.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_ScopedConnection.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_Signalv2.h>

#include <CrySchematyc2/Schematyc_ActionFactory.h>
#include <CrySchematyc2/Schematyc_Any.h>
#include <CrySchematyc2/Schematyc_ComponentFactory.h>
#include <CrySchematyc2/Schematyc_EnvTypeDesc.h>
#include <CrySchematyc2/Schematyc_GUID.h>
#include <CrySchematyc2/Schematyc_IAbstractInterface.h>
#include <CrySchematyc2/Schematyc_IEnvTypeDesc.h>
#include <CrySchematyc2/Schematyc_IFoundation.h>
#include <CrySchematyc2/Schematyc_IFramework.h>
#include <CrySchematyc2/Schematyc_ILib.h>
#include <CrySchematyc2/Schematyc_ILibRegistry.h>
#include <CrySchematyc2/Schematyc_IObject.h>
#include <CrySchematyc2/Schematyc_IObjectManager.h>
#include <CrySchematyc2/Schematyc_ISignal.h>
#include <CrySchematyc2/Schematyc_LibUtils.h>
#include <CrySchematyc2/Deprecated/Schematyc_ActionMemberFunction.h>
#include <CrySchematyc2/Deprecated/Schematyc_ComponentMemberFunction.h>
#include <CrySchematyc2/Deprecated/Schematyc_GlobalFunction.h>
#include <CrySchematyc2/Deprecated/Schematyc_IStringPool.h>
#include <CrySchematyc2/Deprecated/Schematyc_Stack.h>
#include <CrySchematyc2/Deprecated/Schematyc_StackIArchive.h>
#include <CrySchematyc2/Deprecated/Schematyc_StackOArchive.h>
#include <CrySchematyc2/Deprecated/Schematyc_Variant.h>
#include <CrySchematyc2/Deprecated/Schematyc_VariantStack.h>
#include <CrySchematyc2/Env/Schematyc_EnvFunctionDescriptor.h>
#include <CrySchematyc2/Serialization/Schematyc_SerializationUtils.h>
#include <CrySchematyc2/Services/Schematyc_ILog.h>
#include <CrySchematyc2/Services/Schematyc_ITimerSystem.h>
#include <CrySchematyc2/Services/Schematyc_IUpdateScheduler.h>