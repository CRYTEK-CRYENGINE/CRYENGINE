%include "CryEngine.swig"

%import "CryCommon.i"

%{
#include <CryScriptSystem/IScriptSystem.h>
%}

%{
bool IScriptTableIterator::Next( ScriptAnyValue &var )
{
	return false;
}
void IScriptTableIterator::AddRef() {}
void IScriptTableIterator::Release() {}
%}
%ignore IScriptSystemEngineModule;
%ignore CreateScriptSystem;
%include "../../../../CryEngine/CryCommon/CryScriptSystem/IScriptSystem.h"
%include "../../../../CryEngine/CryCommon/CryScriptSystem/ScriptHelpers.h"