// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 04:05:2012: Created by Stan Fichele

*************************************************************************/
#include "StdAfx.h"
#include "GamePhysicsSettings.h"
#include <CryCore/BitFiddling.h>
#ifdef GAME_PHYS_DEBUG
#include "Utility/CryWatch.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include <CryRenderer/IRenderer.h>
#endif //GAME_PHYS_DEBUG

AUTOENUM_BUILDNAMEARRAY(s_collision_class_names, COLLISION_CLASSES);
AUTOENUM_BUILDNAMEARRAY(s_game_collision_class_names, GAME_COLLISION_CLASSES);

const int k_num_collision_classes = CRY_ARRAY_COUNT(s_collision_class_names);
const int k_num_game_collision_classes = CRY_ARRAY_COUNT(s_game_collision_class_names);;

const char* CGamePhysicsSettings::GetCollisionClassName(unsigned int bitIndex)
{
	return (bitIndex<MAX_COLLISION_CLASSES) ? m_names[bitIndex] : "";
}


int CGamePhysicsSettings::GetBit(uint32 a)
{
	int bit = IntegerLog2(a);
	bool valid = a!=0 && ((a-1)&a)==0;
	return valid ? bit : MAX_COLLISION_CLASSES;
}

void CGamePhysicsSettings::Init()
{
	static_assert((k_num_collision_classes+k_num_game_collision_classes)<=MAX_COLLISION_CLASSES, "Too many collision classes!");

	// Automatically construct a list of string names for the collision clas enums

	for (int i=0; i<MAX_COLLISION_CLASSES; i++)
	{
		m_names[i] = "";
		m_classIgnoreMap[i] = 0;
	}

	#define GP_ASSIGN_NAME(a,...) m_names[GetBit(a)] = #a;
	#define GP_ASSIGN_NAMES(list) list(GP_ASSIGN_NAME)
	GP_ASSIGN_NAMES(COLLISION_CLASSES);

	#undef GP_ASSIGN_NAME
	#define GP_ASSIGN_NAME(a,b,...) m_names[GetBit(b)] = #a;
	GP_ASSIGN_NAMES(GAME_COLLISION_CLASSES);

	// Set up the default ignore flags.
	SetIgnoreMap(gcc_player_all, gcc_ragdoll);
	SetIgnoreMap(gcc_vtol, collision_class_terrain);
}

void CGamePhysicsSettings::ExportToLua()
{
	// Export enums to lua and construct a global table g_PhysicsCollisionClass
	
	IScriptSystem * pScriptSystem = gEnv->pScriptSystem;
	IScriptTable* physicsCollisionClassTable = pScriptSystem->CreateTable();
	physicsCollisionClassTable->AddRef();
	physicsCollisionClassTable->BeginSetGetChain();	
	for (int i=0; i<MAX_COLLISION_CLASSES; i++)
	{
		if (m_names[i][0])
		{
			static_assert(MAX_COLLISION_CLASSES <= 23, "Too many collision classes!"); //LUA can't support flags beyond bit 23 due to using floats

			stack_string name;
			name.Format("bT_%s", m_names[i]);  // Annoyingly we need to prefix with a b to make it a bool in lua
			physicsCollisionClassTable->SetValueChain(name.c_str(), 1<<i);
		}
	}
	physicsCollisionClassTable->EndSetGetChain();
	pScriptSystem->SetGlobalValue("g_PhysicsCollisionClass", physicsCollisionClassTable);
	physicsCollisionClassTable->Release();
	
	for (int i=0; i<MAX_COLLISION_CLASSES; i++)
	{
		if (m_names[i][0])
			pScriptSystem->SetGlobalValue(m_names[i], 1<<i);
	}

#undef GP_ASSIGN_NAME
#undef GP_ASSIGN_NAMES

#define GP_ASSIGN_NAMES(list) list(GP_ASSIGN_NAME)
#define GP_ASSIGN_NAME(a,b,...) pScriptSystem->SetGlobalValue(#a, b);
	GP_ASSIGN_NAMES(GAME_COLLISION_CLASS_COMBOS);

#undef GP_ASSIGN_NAME
#undef GP_ASSIGN_NAMES

}

void CGamePhysicsSettings::AddIgnoreMap( uint32 gcc_classTypes, const uint32 ignoreClassTypesOR, const uint32 ignoreClassTypesAND )
{
	for(int i=0; i<MAX_COLLISION_CLASSES && gcc_classTypes; i++,gcc_classTypes>>=1)
	{
		if(gcc_classTypes&0x1)
		{
			m_classIgnoreMap[i] |= ignoreClassTypesOR;
			m_classIgnoreMap[i] &= ignoreClassTypesAND;
		}
	}
}

void CGamePhysicsSettings::SetIgnoreMap( uint32 gcc_classTypes, const uint32 ignoreClassTypes )
{
	AddIgnoreMap( gcc_classTypes, ignoreClassTypes, ignoreClassTypes );
}

void CGamePhysicsSettings::SetCollisionClassFlags( IPhysicalEntity& physEnt, uint32 gcc_classTypes, const uint32 additionalIgnoreClassTypesOR /*= 0*/, const uint32 additionalIgnoreClassTypesAND /*= 0xFFFFFFFF */ )
{
	const uint32 defaultIgnores = GetIgnoreTypes(gcc_classTypes); 
	pe_params_collision_class gcc_params;
	gcc_params.collisionClassOR.type = gcc_params.collisionClassAND.type = gcc_classTypes;
	gcc_params.collisionClassOR.ignore = gcc_params.collisionClassAND.ignore = (defaultIgnores|additionalIgnoreClassTypesOR)&additionalIgnoreClassTypesAND;
	physEnt.SetParams(&gcc_params);
}

void CGamePhysicsSettings::AddCollisionClassFlags( IPhysicalEntity& physEnt, uint32 gcc_classTypes, const uint32 additionalIgnoreClassTypesOR /*= 0*/, const uint32 additionalIgnoreClassTypesAND /*= 0xFFFFFFFF */ )
{
	const uint32 defaultIgnores = GetIgnoreTypes(gcc_classTypes); 
	pe_params_collision_class gcc_params;
	gcc_params.collisionClassOR.type = gcc_classTypes;
	gcc_params.collisionClassOR.ignore = defaultIgnores|additionalIgnoreClassTypesOR;
	gcc_params.collisionClassAND.ignore = additionalIgnoreClassTypesAND;
	physEnt.SetParams(&gcc_params);
}

uint32 CGamePhysicsSettings::GetIgnoreTypes( uint32 gcc_classTypes ) const
{
	uint32 ignoreTypes = 0;
	for(int i=0; i<MAX_COLLISION_CLASSES && gcc_classTypes; i++,gcc_classTypes>>=1)
	{
		if(gcc_classTypes&0x1)
		{
			ignoreTypes |= m_classIgnoreMap[i];
		}
	}
	return ignoreTypes;
}

void CGamePhysicsSettings::Debug( const IPhysicalEntity& physEnt, const bool drawAABB ) const
{
#ifdef GAME_PHYS_DEBUG
	IRenderAuxGeom* pRender = gEnv->pRenderer->GetIRenderAuxGeom();
	if(drawAABB && !pRender)
		return;

	pe_params_collision_class gcc;
	if(physEnt.GetParams(&gcc))
	{
		// NAME:
		const int iForeign = physEnt.GetiForeignData();
		void * const pForeign = physEnt.GetForeignData(iForeign);
		static const int bufLen = 256;
		char buf[bufLen] = "Unknown";
		switch(iForeign)
		{
		case PHYS_FOREIGN_ID_ENTITY:
			{
				if(IEntity* pEntity=(IEntity*)physEnt.GetForeignData(PHYS_FOREIGN_ID_ENTITY))
				{
					cry_strcpy(buf, pEntity->GetName());
				}
			}
			break;
		default:
			break;
		}

		CryWatch("===============");
		CryWatch("PhysEnt[%s]", buf);
		
		ToString(gcc.collisionClassOR.type,&buf[0],bufLen);
		CryWatch("CollisionClass[ %s ]", buf);

		ToString(gcc.collisionClassOR.ignore,&buf[0],bufLen);
		CryWatch("Ignoring[ %s ]", buf);

		CryWatch("===============");

		pe_status_pos entpos;
		if(physEnt.GetStatus(&entpos))
		{
			pe_status_nparts np;
			const int numParts = physEnt.GetStatus(&np);
			for(int p=0; p<numParts; p++)
			{
				if(drawAABB)
				{
					pe_status_pos sp;
					sp.ipart = p;
					sp.flags |= status_local;
					if(physEnt.GetStatus(&sp))
					{
						if(IGeometry* pGeom = sp.pGeomProxy ? sp.pGeomProxy : sp.pGeom)
						{
							OBB obb;
							primitives::box lbbox;
							pGeom->GetBBox(&lbbox);
							const Vec3 center = entpos.pos + (entpos.q * (sp.pos + (sp.q * (lbbox.center*sp.scale))));
							obb.c.zero();
							obb.h = lbbox.size * sp.scale * entpos.scale;
							obb.m33 = Matrix33(entpos.q*sp.q) * lbbox.Basis.GetTransposed();

							pRender->DrawOBB( obb, center, false, ColorB(40, 200, 40), eBBD_Faceted);

							const float distSqr = GetISystem()->GetViewCamera().GetPosition().GetSquaredDistance(center);
							const float drawColor[4] = {0.15f, 0.8f, 0.15f, clamp_tpl(1.f-((distSqr-100.f)/(10000.f-100.f)), 0.f, 1.f)};
							cry_sprintf(buf,"%d",p);
							IRenderAuxText::DrawLabelEx(center, 1.2f, drawColor, true, true, &buf[0]);
						}
					}
				}

				pe_params_part pp;
				pp.ipart = p;
				if(physEnt.GetParams(&pp))
				{
					CryWatch("PART %d", p);
					const uint32 flagsSelf = pp.flagsOR;
					buf[0]=0;
					for(uint32 i=0, first=1; i<32; i++)
					{
						if((1<<i)&flagsSelf)
						{
							if(first) { buf[0]=0; }
							cry_sprintf(buf, "%s%s%d", buf, first?"":" + ", i );
							first=0;
						}
					}
					CryWatch(" Flags[ %s ]", buf);

					const uint32 flagsColl = pp.flagsColliderOR;
					buf[0]=0;
					for(uint32 i=0, first=1; i<32; i++)
					{
						if((1<<i)&flagsColl)
						{
							if(first) { buf[0]=0; }
							cry_sprintf(buf, "%s%s%d", buf, first?"":"+", i );
							first=0;
						}
					}
					CryWatch(" FlagsColl[ %s ]", buf);
				}
			}
		}
	}
	if(drawAABB)
	{
		pe_params_bbox bbox;
		if(physEnt.GetParams(&bbox))
		{
			pRender->DrawAABB( AABB(bbox.BBox[0],bbox.BBox[1]), Matrix34(IDENTITY), false, ColorB(255, 0, 0), eBBD_Faceted);
		}
	}
#endif //GAME_PHYS_DEBUG
}

int CGamePhysicsSettings::ToString( uint32 gcc_classTypes, char* buf, const int len, const bool trim /*= true*/ ) const
{
	int count = 0;
#ifdef GAME_PHYS_DEBUG
	cry_strcpy(buf, len, "None");
	for(int i=0; i<MAX_COLLISION_CLASSES && gcc_classTypes; i++,gcc_classTypes>>=1)
	{
		if(gcc_classTypes&0x1)
		{
			count++;
			const char* const name = m_names[i];
			size_t trimOffset = 0;
			if(trim)
			{
				const char* find;
				if(find=strstr(name,"gcc_"))
				{
					trimOffset = 4;
				}
				else if(find=strstr(name,"collision_class_"))
				{
					trimOffset = 16;
				}
			}
			cry_sprintf(buf, len, "%s%s%s", count>1?buf:"", count>1?" + ":"", &name[trimOffset] );
		}
	}
#endif //GAME_PHYS_DEBUG
	return count;
}
