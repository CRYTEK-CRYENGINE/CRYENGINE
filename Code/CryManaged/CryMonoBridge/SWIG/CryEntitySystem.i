%include "CryEngine.swig"

%import "CryCommon.i"

%import "Cry3DEngine.i"
%import "CryScriptSystem.i"
%import "CryPhysics.i"
%import "CryRender.i"

%{
#include <CryEntitySystem/IEntityProxy.h>
#include <CryEntitySystem/IEntity.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryEntitySystem/IBreakableManager.h>
#include <CryEntitySystem/IComponent.h>
#include <Cry3DEngine/IStatObj.h>
#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>
%}
%include "../../../../CryEngine/CryCommon/CryEntitySystem/IComponent.h"

%ignore GetType;
%import "../../../../CryEngine/CryCommon/CryEntitySystem/IComponent.h"
%import "../../../../CryEngine/CryCommon/Cry3DEngine/IStatObj.h"

// TEMPORARY
%ignore IEntityProxy::GetSignature;
%ignore IEntityPhysicalProxy::SerializeTyped;
// ~TEMPORARY
%feature("director") IEntityScriptProxy;
%include "../../../../CryEngine/CryCommon/CryEntitySystem/IEntityProxy.h"
//TODO: %include "../../../CryEngine/CryCommon/IEntityAttributesProxy.h"
%include "../../../../CryEngine/CryCommon/CryEntitySystem/IEntityClass.h"
%ignore EPartIds;
%include "../../../../CryEngine/CryCommon/CryEntitySystem/IEntity.h"
%extend SEntityEvent {
	EventPhysCollision* GetEventPhysCollision()
	{
		if($self->nParam[0])
			return (EventPhysCollision*)$self->nParam[0];
		return nullptr;
	}
}
%extend IEntity {
	SEntitySlotInfo* GetSlotInfo(int slot)
	{
		SEntitySlotInfo* info = new SEntitySlotInfo();
		if(!$self->GetSlotInfo(slot, *info))
		{
			delete info;
			return nullptr;
		}
		return info;
	}
	IEntity* GetAttachedChild(int partID)
	{
		return $self->UnmapAttachedChild(partID);
	}
	IEntityRenderProxy* GetRenderProxy()
	{
		return (IEntityRenderProxy*)$self->GetProxy(ENTITY_PROXY_RENDER);
	}
}
//(maybe) TODO: %include "../../../CryEngine/CryCommon/IEntityRenderState.h"
//(maybe) TODO: %include "../../../CryEngine/CryCommon/IEntityRenderState_info.h"
//TODO: %include "../../../CryEngine/CryCommon/IEntitySerialize.h"
%typemap(csbase) IEntitySystem::SinkEventSubscriptions "uint"
%ignore CreateEntitySystem;
%feature("director") IEntityEventListener;
%feature("director") IEntitySystemSink;
%feature("director") IAreaManagerEventListener;
%include "../../../../CryEngine/CryCommon/CryEntitySystem/IEntitySystem.h"
%include "../../../../CryEngine/CryCommon/CryEntitySystem/IBreakableManager.h"
