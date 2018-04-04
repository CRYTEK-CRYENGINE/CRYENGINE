%include "CryEngine.swig"

%import "CryCommon.i"

%{
#include <CryFont/IFont.h>
%}

%ignore CreateCryFontInterface;
%ignore IFontEngineModule;
%include "../../../../CryEngine/CryCommon/CryFont/IFont.h"