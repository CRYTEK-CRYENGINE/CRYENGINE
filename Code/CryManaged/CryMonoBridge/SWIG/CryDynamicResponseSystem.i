%include "CryEngine.swig"

%import "CryCommon.i"

%{
#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>
#include <CryDynamicResponseSystem/IDynamicResponseAction.h>
#include <CryDynamicResponseSystem/IDynamicResponseCondition.h>
%}
%feature("nspace", 1);
%ignore DRS::IResponseAction::Execute;
%ignore DRS::IDynamicResponseSystemEngineModule;
%typemap(csbase) DRS::IDynamicResponseSystem::eResetHints "uint"
%typemap(csbase) DRS::IDynamicResponseSystem::eSerializationFilter "uint"
%include "../../../../CryEngine/CryCommon/CryDynamicResponseSystem/IDynamicResponseSystem.h"
%include "../../../../CryEngine/CryCommon/CryDynamicResponseSystem/IDynamicResponseAction.h"
%include "../../../../CryEngine/CryCommon/CryDynamicResponseSystem/IDynamicResponseCondition.h"