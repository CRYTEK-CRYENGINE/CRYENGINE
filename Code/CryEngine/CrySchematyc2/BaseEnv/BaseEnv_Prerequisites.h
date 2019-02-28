// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/TemplateUtils/TemplateUtils_Delegate.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_PreprocessorUtils.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_ScopedConnection.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_Signalv2.h>

#include <CrySchematyc2/ActionFactory.h>
#include <CrySchematyc2/Any.h>
#include <CrySchematyc2/ComponentFactory.h>
#include <CrySchematyc2/EnvTypeDesc.h>
#include <CrySchematyc2/GUID.h>
#include <CrySchematyc2/IAbstractInterface.h>
#include <CrySchematyc2/IEnvTypeDesc.h>
#include <CrySchematyc2/IFoundation.h>
#include <CrySchematyc2/IFramework.h>
#include <CrySchematyc2/ILib.h>
#include <CrySchematyc2/ILibRegistry.h>
#include <CrySchematyc2/IObject.h>
#include <CrySchematyc2/IObjectManager.h>
#include <CrySchematyc2/ISignal.h>
#include <CrySchematyc2/LibUtils.h>
#include <CrySchematyc2/Deprecated/ActionMemberFunction.h>
#include <CrySchematyc2/Deprecated/ComponentMemberFunction.h>
#include <CrySchematyc2/Deprecated/GlobalFunction.h>
#include <CrySchematyc2/Deprecated/IStringPool.h>
#include <CrySchematyc2/Deprecated/Stack.h>
#include <CrySchematyc2/Deprecated/StackIArchive.h>
#include <CrySchematyc2/Deprecated/StackOArchive.h>
#include <CrySchematyc2/Deprecated/Variant.h>
#include <CrySchematyc2/Deprecated/VariantStack.h>
#include <CrySchematyc2/Env/EnvFunctionDescriptor.h>
#include <CrySchematyc2/Serialization/SerializationUtils.h>
#include <CrySchematyc2/Services/ILog.h>
#include <CrySchematyc2/Services/ITimerSystem.h>
#include <CrySchematyc2/Services/IUpdateScheduler.h>
