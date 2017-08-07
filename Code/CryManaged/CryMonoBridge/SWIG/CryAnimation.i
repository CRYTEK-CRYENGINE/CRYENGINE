%include "CryEngine.swig"

%import "CryCommon.i"
%import "CryEntitySystem.i"

%{
#include <CryAnimation/ICryAnimation.h>
#include <CryInput/IJoystick.h>
#include <CryAnimation/IVertexAnimation.h>
#include <CryAnimation/IFacialAnimation.h>
%}

%ignore IAnimationEngineModule;
%ignore IAnimationSerializable;
%ignore CreateCharManager;
%include "../../../../CryEngine/CryCommon/CryAnimation/ICryAnimation.h"
%typemap(csbase) CA_AssetFlags "uint"
%include "../../../../CryEngine/CryCommon/CryAnimation/CryCharAnimationParams.h"
%include "../../../../CryEngine/CryCommon/CryAnimation/IAttachment.h"
%include "../../../../CryEngine/CryCommon/CryAnimation/IAnimationPoseModifier.h"
%typemap(csbase) IFacialAnimSequence::ESerializationFlags "uint"
%include "../../../../CryEngine/CryCommon/CryAnimation/IFacialAnimation.h"
%include "../../../../CryEngine/CryCommon/CryInput/IJoystick.h"
%include "../../../../CryEngine/CryCommon/CryAnimation/IVertexAnimation.h"