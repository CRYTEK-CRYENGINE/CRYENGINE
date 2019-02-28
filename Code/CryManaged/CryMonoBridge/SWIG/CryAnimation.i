%include "CryEngine.swig"

%import "CryCommon.i"
%import "CryEntitySystem.i"

%{
#include <CryAnimation/ICryAnimation.h>
#include <CryInput/IJoystick.h>
#include <CryAnimation/IVertexAnimation.h>
#include <CryAnimation/IFacialAnimation.h>
#include <CryAnimation/ICryMannequin.h>

#include <CryAnimation/ICryMannequinDefs.h>
#define MannGenCRC CCrc32::ComputeLowercase
#include <CryAnimation/ICryMannequinTagDefs.h>
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

%ignore SAnimationContext::randGenerator;

%include "../../../CryEngine/CryCommon/CryAnimation/ICryMannequinTagDefs.h"
%extend CTagDefinition {
		STagState<12U> GenerateMaskManaged(STagStateBase tagState){
			return $self->GenerateMask(tagState);
	
	}
}

%include "../../../CryEngine/CryCommon/CryAnimation/ICryMannequin.h"
%extend TAction {
	static TAction* CreateSAnimationContext(int priority, int fragmentID, STagState<12U> fragTags, uint flags, ulong scopeMask, uint userToken){
		return new TAction<SAnimationContext>(priority, fragmentID, fragTags, flags, scopeMask, userToken);
	}
}
%include "../../../CryEngine/CryCommon/CryAnimation/ICryMannequinProceduralClipFactory.h"
%include "../../../CryEngine/CryCommon/CryAnimation/ICryMannequinDefs.h"
%template(AnimationContextActionList) TAction<SAnimationContext>;
%template(TagState) STagState<12U>;