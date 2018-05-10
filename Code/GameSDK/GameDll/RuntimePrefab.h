// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: 
  
 -------------------------------------------------------------------------
  History:
  - Created by Marco Corbetta

*************************************************************************/
#ifndef __GAME_RUNTIME_PREFAB_H__
#define __GAME_RUNTIME_PREFAB_H__

#if _MSC_VER > 1000
# pragma once
#endif

//////////////////////////////////////////////////////////////////////////
namespace CryGame 
{

	//////////////////////////////////////////////////////////////////////////
	class CRuntimePrefab
	{
	public:
		CRuntimePrefab(EntityId id);
		~CRuntimePrefab();
				
		void	Spawn(CPrefab &pRef); 
		void	Spawn_2(CPrefab &pRef); 
		void	Spawn(IEntity *pEntity,CPrefab &pRef, const Matrix34 &matOff,AABB &box);								
		void	Spawn_2(IEntity *pEntity,CPrefab &pRef, const Matrix34 &matOff,AABB &box);								
		void	Move();
		void	Hide(bool bHide);

		CPrefab	*GetSourcePrefab() { return(m_pPrefab); }
		
		Matrix34 m_mat; // local matrix

	protected:
		
		void	SpawnEntities(CPrefab &pRef, const Matrix34 &matSource,AABB &box);
		void	SpawnBrushes(CPrefab &pRef, const Matrix34 &matSource,AABB &box);

		void	SpawnEntities_2(IEntity *pEntity,CPrefab &pRef, const Matrix34 &matSource,AABB &box);
		void	SpawnBrushes_2(IEntity *pEntity,CPrefab &pRef, const Matrix34 &matSource,AABB &box);

		void	Clear();
		void	Move(const Matrix34 &matOff);
		void	HideComponents(bool bHide);
						
		CPrefab	*m_pPrefab;
		EntityId	m_id;												// original entity who spawned this prefab
		std::vector<EntityId>					m_lstIDs;			// list of entities spawned by this prefab
		std::vector<IRenderNode*>			m_lstNodes;		// list of brushes/decals	spawned by this prefab
		std::vector<CRuntimePrefab*>	m_lstPrefabs;	// list of prefabs	spawned by this prefab
	};
	
}

#endif