%include "CryEngine.swig"

%import "CryCommon.i"
%import "CryAudio.i"

%{
#include <CrySystem/IStreamEngine.h>
#include <Cry3DEngine/IIndexedMesh.h>
#include <Cry3DEngine/CGF/CGFContent.h>
#include <CryPhysics/IDeferredCollisionEvent.h>
#include <CrySystem/IProcess.h>
#include <Cry3DEngine/IStatObj.h>
#include <Cry3DEngine/IGeomCache.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryParticleSystem/IParticles.h>
#include <CryEntitySystem/IEntity.h>
#include <Cry3DEngine/ITimeOfDay.h>
%}
%ignore I3DEngineModule;
%ignore ITimeOfDay::NetSerialize;
%ignore I3DEngine::SerializeState;
%ignore I3DEngine::SaveStatObj;
%ignore I3DEngine::LoadStatObj;
%typemap(csbase) EMaterialLayerFlags "uint"
%template(IMaterialPtr) _smart_ptr<IMaterial>;
%template(IStatObjPtr) _smart_ptr<IStatObj>;
%template(IReadStreamPtr) _smart_ptr<IReadStream>;
%template(IRenderMeshPtr) _smart_ptr<IRenderMesh>;

%ignore CryRWLock;
%ignore SRenderNodeTempData;
%ignore IRenderNode::m_pTempData;
%ignore IRenderNode::m_manipulationFrame;
%ignore IRenderNode::m_manipulationLock;

%typemap(cscode) IParticleEffect
%{
	public string Name { get { return GetName (); } }
%}

%include "../../../../CryEngine/CryCommon/Cry3DEngine/ITimeOfDay.h"
%import "../../../../CryEngine/CryCommon/Cry3DEngine/CGF/CryHeaders.h"
%include "../../../../CryEngine/CryCommon/Cry3DEngine/IMaterial.h"
%include "../../../../CryEngine/CryCommon/CryRenderer/IRenderMesh.h"
%include "../../../../CryEngine/CryCommon/Cry3DEngine/ISurfaceType.h"
%include "../../../../CryEngine/CryCommon/CrySystem/IStreamEngine.h"
%include "../../../../CryEngine/CryCommon/CrySystem/IStreamEngineDefs.h"
%include "../../../../CryEngine/CryCommon/Cry3DEngine/I3DEngine.h"
%include "../../../../CryEngine/CryCommon/CryRenderer/IFlares.h"
%ignore IParticleManager::SerializeEmitter;
%include "../../../../CryEngine/CryCommon/CryParticleSystem/IParticles.h"
%csconstvalue("1 << EStreamIDs.VSF_GENERAL") VSM_GENERAL;
%csconstvalue("((1 << EStreamIDs.VSF_TANGENTS)|(1 << EStreamIDs.VSF_QTANGENTS))") VSM_TANGENTS;
%csconstvalue("1 << EStreamIDs.VSF_HWSKIN_INFO") VSM_HWSKIN;
%csconstvalue("1 << EStreamIDs.VSF_VERTEX_VELOCITY") VSM_VERTEX_VELOCITY;
%csconstvalue("1 << EStreamIDs.VSF_NORMALS") VSM_NORMALS;
%csconstvalue("1 << EStreamIDs.VSF_MORPHBUDDY") VSM_MORPHBUDDY;
%csconstvalue("1 << EStreamIDs.VSF_INSTANCED") VSM_INSTANCED;
%csconstvalue("((1 << EStreamIDs.VSF_NUM) -1)") VSM_MASK;
%include "../../../../CryEngine/CryCommon/CryRenderer/VertexFormats.h"
%include "../../../../CryEngine/CryCommon/Cry3DEngine/IIndexedMesh.h"
%ignore PQLog::blendPQ;
%include "../../../../CryEngine/CryCommon/Cry3DEngine/CGF/CGFContent.h"
%include "../../../../CryEngine/CryCommon/Cry3DEngine/CGF/IChunkFile.h"

%typemap(csbase) ERenderNodeFlags "long"

%include "../../../../CryEngine/CryCommon/Cry3DEngine/IRenderNode.h"
%include "../../../../CryEngine/CryCommon/CryPhysics/IDeferredCollisionEvent.h"
%include "../../../../CryEngine/CryCommon/CrySystem/IProcess.h"
%include "../../../../CryEngine/CryCommon/Cry3DEngine/IStatObj.h"
%include "../../../../CryEngine/CryCommon/Cry3DEngine/IGeomCache.h"

%extend IMaterial
{
	void SetMaterialParamFloat(const char* sParamName, float v)
	{
		$self->SetGetMaterialParamFloat(sParamName,v,false);
	}

	float GetMaterialParamFloat(const char* sParamName)
	{
		float v = 0.0f;
		$self->SetGetMaterialParamFloat(sParamName,v,true);

		return v;
	}

	void SetMaterialParamVec3(const char* sParamName, Vec3 v)
	{
		$self->SetGetMaterialParamVec3(sParamName,v,false);
	}

	Vec3 GetMaterialParamVec3(const char* sParamName)
	{
		Vec3 v(0.0f,0.0f,0.0f);
		$self->SetGetMaterialParamVec3(sParamName,v,true);

		return v;
	}
}