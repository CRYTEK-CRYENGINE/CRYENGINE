%include "CryEngine.swig"

%import "CryCommon.i"

%{
#include <CryPhysics/IPhysics.h>
#include <CryPhysics/IPhysicsDebugRenderer.h>

#if !defined(_LIB)
bop_meshupdate::~bop_meshupdate() 
{
	if (pRemovedVtx) delete[] pRemovedVtx;
	if (pRemovedTri) delete[] pRemovedTri;
	if (pNewVtx) delete[] pNewVtx;
	if (pNewTri) delete[] pNewTri;
	if (pWeldedVtx) delete[] pWeldedVtx;
	if (pTJFixes) delete[] pTJFixes;
	if (pMovedBoxes) delete[] pMovedBoxes;
	if (next) delete next;
	prevRef->nextRef = nextRef; nextRef->prevRef = prevRef;
	prevRef = nextRef = this;
	if (pMesh[0]) pMesh[0]->Release(); 
	if (pMesh[1]) pMesh[1]->Release();
}
#endif

%}
%feature("nspace", 1);
%ignore IPhysicsEngineModule;
%ignore CreatePhysicalWorld;
%csconstvalue("primitives.triangle.entype.type") GEOM_TRIMESH;
%csconstvalue("primitives.heightfield.entype.type") GEOM_HEIGHTFIELD;
%csconstvalue("primitives.cylinder.entype.type") GEOM_CYLINDER;
%csconstvalue("primitives.capsule.entype.type") GEOM_CAPSULE;
%csconstvalue("primitives.ray.entype.type") GEOM_RAY;
%csconstvalue("primitives.sphere.entype.type") GEOM_SPHERE;
%csconstvalue("primitives.box.entype.type") GEOM_BOX;
%csconstvalue("primitives.voxelgrid.entype.type") GEOM_VOXELGRID;
%include "../../../../CryEngine/CryCommon/CryPhysics/IPhysics.h"
%include "../../../../CryEngine/CryCommon/CryPhysics/IPhysicsDebugRenderer.h"
%include "../../../../CryEngine/CryCommon/CryPhysics/primitives.h"
%ignore IPhysicalWorld::LoadPhysicalEntityPtr;
%ignore IPhysicalWorld::SavePhysicalEntityPtr;
%ignore IPhysicalWorld::SerializeGarbageTypedSnapshot;
%ignore IPhysicalEntity::SetStateFromTypedSnapshot;
%ignore IPhysicalEntity::GetStateSnapshot;
%ignore IPhysicalEntity::SetStateFromSnapshot;
%ignore *::entype;
%csconstvalue("surface_flags.sf_important") rwi_separate_important_hits;
%csconstvalue("phentity_flags.pef_update") ent_flagged_only;
%csconstvalue("phentity_flags.pef_update*2") ent_skip_flagged;
%include "../../../../CryEngine/CryCommon/CryPhysics/physinterface.h"
%extend IPhysicalEntity {
public:
virtual int SetParams(pe_simulation_params* params, int bThreadSafe=0) { return $self->SetParams(params, bThreadSafe); }
virtual int Action(pe_action_impulse* action, int bThreadSafe=0) { return $self->Action(action, bThreadSafe); }
}
%extend EventPhysCollision {
	Vec3 GetFirstVelocity() { return $self->vloc[0]; }
	Vec3 GetSecondVelocity() { return $self->vloc[1]; }
	float GetFirstMass() { return $self->mass[0]; }
	float GetSecondMass() { return $self->mass[1]; }
	int GetFirstPartID() { return $self->partid[0]; }
	int GetSecondPartID() { return $self->partid[1]; }
	short GetFirstIdMat() { return $self->idmat[0]; }
	short GetSecondIdMat() { return $self->idmat[1]; }
	short GetFirstPrim() { return $self->iPrim[0]; }
	short GetSecondPrim() { return $self->iPrim[1]; }
}
%extend EventPhysStereo {
	IPhysicalEntity* GetFirstEntity() { return $self->pEntity[0]; }
	IPhysicalEntity* GetSecondEntity() { return $self->pEntity[1]; }
	int GetFirstForeignData() { return $self->iForeignData[0]; }
	int GetSecondForeignData() { return $self->iForeignData[1]; }
}
%extend IPhysicalWorld::SRWIParams {
	void AllocateSkipEnts(int size)
	{
		$self->pSkipEnts = new IPhysicalEntity*[size];
		$self->nSkipEnts = size;
	}
	void SetSkipEnt(int index, IPhysicalEntity* pEnt)
	{
		$self->pSkipEnts[index] = pEnt;
	}
	void DeleteSkipEnts()
	{
		delete[] $self->pSkipEnts;
	}
}
%extend IPhysicalWorld {
	ray_hit* RayTraceEntity(IPhysicalEntity *pient, Vec3 origin,Vec3 dir)
	{
		ray_hit* hit = new ray_hit();
		if($self->RayTraceEntity(pient, origin, dir, hit) > 0)
			return hit;
		delete hit;
		return nullptr;
	}
}