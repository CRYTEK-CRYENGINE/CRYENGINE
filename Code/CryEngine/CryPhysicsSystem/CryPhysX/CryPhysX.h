// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
//#ifndef _DEBUG
//	#ifndef NDEBUG
//		#define NDEBUG
//	#endif
//#endif
//////////////////////////////////////////////////////////////////////////

#undef alloc
#undef free

#include "PxPhysicsAPI.h"
#include "extensions/PxExtensionsAPI.h"

//////////////////////////////////////////////////////////////////////////

using namespace physx;

namespace cpx // CryPhysX
{
	namespace Helper
	{
		inline Vec3 V(physx::PxVec3 const& v) { return Vec3(v.x, v.y, v.z); }
		inline physx::PxVec3 V(Vec3 const& v) { return physx::PxVec3(v.x, v.y, v.z); }
		inline physx::PxVec3 V(Diag33 const& v) { return physx::PxVec3(v.x, v.y, v.z); }
		inline Quat Q(physx::PxQuat const& q) { return Quat(q.w, q.x, q.y, q.z); }
		inline physx::PxQuat Q(Quat const& q) { return physx::PxQuat(q.v.x, q.v.y, q.v.z, q.w); }
		inline QuatT T(physx::PxTransform const& trans) { return QuatT(Q(trans.q), V(trans.p)); }
		inline physx::PxTransform T(QuatT const& trans) { return physx::PxTransform(V(trans.t), Q(trans.q)); }
		inline physx::PxTransform T(Vec3	const& v) { return physx::PxTransform(V(v), PxQuat(physx::PxIdentity)); }
	}
#define PxQuat0 PxQuat(PxIDENTITY::PxIdentity)

	class CryPhysX
	{
	public:
		CryPhysX();
		~CryPhysX();

		physx::PxPhysics* Physics() { return m_Physics; }
		physx::PxScene* Scene() { return m_Scene; }
		physx::PxCooking* Cooking() { return m_Cooking; }
		float& dt() { return m_dt; }

		void Init();
		void SceneClear(); //<! removes all entities from scene
		void SceneResetDynamicEntities();

		void SetDebugVisualizationForAllSceneElements(bool enable = true);

	private:

		float                             m_dt;

		physx::PxPhysics*                 m_Physics;
		physx::PxFoundation*              m_Foundation;
		physx::PxScene*                   m_Scene;
		physx::PxCooking*                 m_Cooking;

		physx::PxPvd*                     m_Pvd;
		physx::PxPvdTransport*            m_PvdTransport; //!< Debugger Connection to PhysX
		bool m_DebugVisualizationForAllSceneElements;

		PxDefaultErrorCallback m_DefaultErrorCallback;
		PxDefaultAllocator m_DefaultAllocatorCallback;
	};

	extern CryPhysX g_cryPhysX; // PhysX context
}

namespace cpx { 
	namespace Helper {
		inline int PartId(const physx::PxShape *shape) { return (int)(INT_PTR)(shape->userData) & 0xffff; }

		struct ReadLockScene { 
			ReadLockScene() { cpx::g_cryPhysX.Scene()->lockRead(); }
			~ReadLockScene() { cpx::g_cryPhysX.Scene()->unlockRead(); }
		};
		struct ReadLockCondScene { 
			bool m_active;
			ReadLockCondScene(bool active) : m_active(active) { if (active) cpx::g_cryPhysX.Scene()->lockRead(); }
			~ReadLockCondScene() { if (m_active) cpx::g_cryPhysX.Scene()->unlockRead(); }
		};
		struct WriteLockScene { 
			WriteLockScene() { cpx::g_cryPhysX.Scene()->lockWrite(); }
			~WriteLockScene() { cpx::g_cryPhysX.Scene()->unlockWrite(); }
		};
		struct WriteLockCondScene { 
			bool m_active;
			WriteLockCondScene(bool active=true) : m_active(active) { if (active) cpx::g_cryPhysX.Scene()->lockWrite(); }
			~WriteLockCondScene() { if (m_active) cpx::g_cryPhysX.Scene()->unlockWrite(); }
		};
	}
}

#define isRigidBody is<PxRigidBody>
#define isRigidDynamic is<PxRigidDynamic>
#define isRigidActor is<PxRigidActor>


