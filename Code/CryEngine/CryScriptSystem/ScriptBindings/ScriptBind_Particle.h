// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ScriptBind_Particle.h
//  Version:     v1.00
//  Created:     8/7/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ScriptBind_Particle_h__
#define __ScriptBind_Particle_h__

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000

#include <CryScriptSystem/IScriptSystem.h>

struct ISystem;
struct I3DEngine;
struct ParticleParams;
struct IParticleEffect;
struct CryEngineDecalInfo;

/*
   <description>This class implements script-functions for particles and decals.</description>
   <remarks>After initialization of the script-object it will be globally accessable through scripts using the namespace "Particle".</remarks>
   <example>Particle.CreateDecal(pos, normal, scale, lifetime, decal.texture, decal.object, rotation)</example>
 */
class CScriptBind_Particle : public CScriptableBase
{
public:
	CScriptBind_Particle(IScriptSystem* pScriptSystem, ISystem* pSystem);
	virtual ~CScriptBind_Particle();
	virtual void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}

	//! <code>Particle.SpawnEffect( effectName, pos, dir )</code>
	//!		<param name="effectName">Effect name.</param>
	//!		<param name="pos">Position vector.</param>
	//!		<param name="dir">Direction vector.</param>
	//!		<return>SlotId where the emitter was bound to if a valid EntityId was given.</return>
	//! <description>Spawns an effect.</description>
	int SpawnEffect(IFunctionHandler* pH, const char* effectName, Vec3 pos, Vec3 dir);

	//! <code>Particle.EmitParticle( pos, dir, entityId, slotId )</code>
	//!		<param name="pos">Particle position.</param>
	//!		<param name="dir">Particle direction.</param>
	//!		<param name="entityId">EntityId of the previously associated entity.</param>
	//!		<param name="slotId">SlotId where the emitter was bound to.</param>
	//! <description>Emits an individual particle previously associanted with an entity.</description>
	int EmitParticle(IFunctionHandler* pH, Vec3 pos, Vec3 dir, ScriptHandle entityId, int slotId);

	//! <code>Particle.CreateDecal( pos, normal, size, lifeTime, textureName )</code>
	//!		<param name="pos">Decal position.</param>
	//!		<param name="normal">Decal normal vector.</param>
	//!		<param name="size">Decal size.</param>
	//!		<param name="lifeTime">Decal life time.</param>
	//!		<param name="textureName - Name of the texture.</param>
	//! <description>Creates a decal with the specified parameters.</description>
	int CreateDecal(IFunctionHandler* pH, Vec3 pos, Vec3 normal, float size, float lifeTime, const char* textureName);

	//! <code>Particle.CreateMatDecal( pos, normal, size, lifeTime, materialName )</code>
	//!		<param name="pos">Decal position.</param>
	//!		<param name="normal">Decal normal vector.</param>
	//!		<param name="size">Decal size.</param>
	//!		<param name="lifeTime">Decal life time.</param>
	//!		<param name="materialName">Name of the Material.</param>
	//! <description>Creates a material decal.</description>
	int CreateMatDecal(IFunctionHandler* pH, Vec3 pos, Vec3 normal, float size, float lifeTime, const char* materialName);

private:
	void ReadParams(SmartScriptTable& table, ParticleParams* params, IParticleEffect* pEffect);
	void CreateDecalInternal(IFunctionHandler* pH, const Vec3& pos, const Vec3& normal, float size, float lifeTime, const char* name, bool nameIsMaterial);

	ISystem*   m_pSystem;
	I3DEngine* m_p3DEngine;
};

#endif // __ScriptBind_Particle_h__
