%include "CryEngine.swig"

%import "CryCommon.i"

%{
#include <CryInput/IInput.h>
%}

%{
#include <CryInput/IHardwareMouse.h>
%}
%ignore operator==(const char *str,const TKeyName &n);

%typemap(csbase) EKeyId "uint"
%typemap(csbase) KIN_IDENTITY_ENROLLMENT "uint"
%feature("director") IInputEventListener;
%feature("director") ITouchEventListener;
%feature("director") IKinectInputAudioListener;
%feature("director") IKinectInputListener;
%ignore IInputEngineModule;
%ignore CreateInput;
%include "../../../../CryEngine/CryCommon/CryInput/IInput.h"
%feature("director") IHardwareMouseEventListener;
%include "../../../../CryEngine/CryCommon/CryInput/IHardwareMouse.h"
%extend IHardwareMouse {
public:
void GetHardwareMouseClientPosition(float& fSwigRef1, float& fSwigRef2) { $self->GetHardwareMouseClientPosition(&fSwigRef1, &fSwigRef2); }
void GetHardwareMousePosition(float& fSwigRef1, float& fSwigRef2) { $self->GetHardwareMousePosition(&fSwigRef1, &fSwigRef2); }
}