// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - Created by Marco Corbetta

	Runtime prefabs, getting Editor prefabs in game.

*************************************************************************/
#include "StdAfx.h"
#include "PrefabManager.h"
#include "RuntimePrefab.h"

using namespace CryGame;

//#pragma optimize("", off)
//#pragma inline_depth(0)

//////////////////////////////////////////////////////////////////////////
CRuntimePrefab::CRuntimePrefab(EntityId id)
{
	m_id=id;
	m_pPrefab=NULL;
	m_mat.SetIdentity();	
}

//////////////////////////////////////////////////////////////////////////
CRuntimePrefab::~CRuntimePrefab()
{
	Clear();
}

//////////////////////////////////////////////////////////////////////////
void CRuntimePrefab::Clear()
{	
	for (std::vector<CRuntimePrefab*>::iterator i=m_lstPrefabs.begin();i!=m_lstPrefabs.end();++i)			
	{
		CRuntimePrefab *pPrefab=(*i);
		delete pPrefab;
	}
	m_lstPrefabs.clear();

	for (std::vector<EntityId>::iterator i=m_lstIDs.begin();i!=m_lstIDs.end();++i)		
	{
		EntityId id=(*i);
		if (id!=0)			
			gEnv->pEntitySystem->RemoveEntity(id);			
	}
	m_lstIDs.clear();

	for (std::vector<IRenderNode*>::iterator i=m_lstNodes.begin();i!=m_lstNodes.end();++i)			
	{
		IRenderNode *pNode=(*i);
		if (pNode)
		{
			IStatObj *pStatObj = pNode->GetEntityStatObj();
			pNode->SetEntityStatObj(0,0);
			if (pStatObj)
				pStatObj->Release();		
			gEnv->p3DEngine->DeleteRenderNode(pNode);
		}
	}
	m_lstNodes.clear();

	m_pPrefab=NULL;
}

//////////////////////////////////////////////////////////////////////////
void	CRuntimePrefab::SpawnEntities(CPrefab &pRef, const Matrix34 &matSource,AABB &box)
{
	gEnv->pEntitySystem->BeginCreateEntities(pRef.m_lstEntities.size());
	
	m_lstIDs.reserve(pRef.m_lstEntities.size());
	for (std::vector<tEntityParams>::iterator i=pRef.m_lstEntities.begin();i!=pRef.m_lstEntities.end();++i)			
	{		
		tEntityParams &pParam=(*i);
		pParam.pSpawnParams.sLayerName=NULL; 
		//pParam.pSpawnParams.sLayerName="Global";		

		// world TM
		Matrix34 mat=matSource*pParam.mat;		
		
		mat.OrthonormalizeFast();
		SEntitySpawnParams sSpawnParams=pParam.pSpawnParams;
		sSpawnParams.vPosition=mat.GetTranslation();
		sSpawnParams.qRotation=Quat(mat);

		sSpawnParams.id=0;
		// need to clear unremovable flag otherwise desc actors don't work in Editor game mode.
		sSpawnParams.nFlags &= ~ENTITY_FLAG_UNREMOVABLE;
		
		EntityId id=0;
		gEnv->pEntitySystem->CreateEntity(pParam.pEntityNode,sSpawnParams,id);

		IEntity* pEnt = gEnv->pEntitySystem->GetEntity( id );
		if (pEnt)
		{
			// calc bbox for the editor because we spawn separate entities 				
			Matrix34 mat2; mat2.Set(Vec3Constants<float>::fVec3_One,Quat::CreateIdentity(),pParam.pSpawnParams.vPosition);		
			AABB box2;		
			pEnt->GetLocalBounds(box2);		// seems like localbounds for the entity already have scale and rotation component included
			box2.SetTransformedAABB(mat2,box2);
			box.Add(box2);				
		}

		m_lstIDs.push_back(id);				
	}

	gEnv->pEntitySystem->EndCreateEntities();
}

//////////////////////////////////////////////////////////////////////////
void CRuntimePrefab::SpawnBrushes(CPrefab &pRef, const Matrix34 &matSource,AABB &box)
{
	// brushes and decals
	m_lstNodes.reserve(pRef.m_lstBrushes.size());
	for (std::vector<tBrushParams>::const_iterator i=pRef.m_lstBrushes.begin();i!=pRef.m_lstBrushes.end();++i)			
	{		
		const tBrushParams &pParam=(*i);

		// world TM
		Matrix34 mat=matSource*pParam.mat;		

		_smart_ptr<IMaterial> pMaterial=NULL;
		if (!pParam.sMaterial.empty())		
			pMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(pParam.sMaterial);		

		IRenderNode *pNode=NULL;
		if (pParam.bIsDecal)
		{
			if (pMaterial!=NULL)
			{
				pNode=gEnv->p3DEngine->CreateRenderNode(eERType_Decal);

				pNode->SetRndFlags(pParam.dwRenderNodeFlags|ERF_PROCEDURAL,true);

				// set properties
				SDecalProperties decalProperties;
				switch( pParam.nProjectionType)
				{
				case SDecalProperties::eProjectOnTerrainAndStaticObjects:
					{
						decalProperties.m_projectionType = SDecalProperties::eProjectOnTerrainAndStaticObjects;
						break;
					}
				case SDecalProperties::eProjectOnStaticObjects:
					{
						decalProperties.m_projectionType = SDecalProperties::eProjectOnStaticObjects;
						break;
					}
				case SDecalProperties::eProjectOnTerrain:
					{
						decalProperties.m_projectionType = SDecalProperties::eProjectOnTerrain;
						break;
					}
				case SDecalProperties::ePlanar:
				default:
					{
						decalProperties.m_projectionType = SDecalProperties::ePlanar;
						break;
					}
				}

				// get normalized rotation (remove scaling)
				Matrix33 rotation( mat );
				if (pParam.nProjectionType != SDecalProperties::ePlanar)
				{
					rotation.SetRow( 0, rotation.GetRow( 0 ).GetNormalized() );
					rotation.SetRow( 1, rotation.GetRow( 1 ).GetNormalized() );
					rotation.SetRow( 2, rotation.GetRow( 2 ).GetNormalized() );
				}

				decalProperties.m_pos = mat.TransformPoint( Vec3( 0, 0, 0 ) );
				decalProperties.m_normal = mat.TransformVector( Vec3( 0, 0, 1 ) );			
				decalProperties.m_pMaterialName = pMaterial->GetName();
				decalProperties.m_radius = pParam.nProjectionType != SDecalProperties::ePlanar ? decalProperties.m_normal.GetLength() : 1;				
				decalProperties.m_explicitRightUpFront = rotation;
				decalProperties.m_sortPrio = pParam.nSortPrio;
				decalProperties.m_deferred = pParam.bDeferredDecal;
				decalProperties.m_depth = pParam.fDepth;				
				IDecalRenderNode *pRenderNode = static_cast< IDecalRenderNode* >(pNode);
				pRenderNode->SetDecalProperties( decalProperties );				
			}
		}
		else
		{
			// brush
			pNode=gEnv->p3DEngine->CreateRenderNode(eERType_Brush);
			IStatObj *pObj = NULL;
			if (!pParam.sFilename.empty())
				pObj=gEnv->p3DEngine->LoadStatObj(pParam.sFilename,NULL,NULL,false);
			else if (!pParam.vBinaryDesignerObject.empty())
				pObj=gEnv->p3DEngine->LoadDesignerObject(pParam.nBinaryDesignerObjectVersion,&pParam.vBinaryDesignerObject[0], (int)pParam.vBinaryDesignerObject.size());

			if (pObj)
			{	
				pObj->AddRef();	
				pNode->SetEntityStatObj(pObj,0);		

				// calc bbox for the editor because we spawn separate brushes	
				AABB box2=pObj->GetAABB();
				box2.SetTransformedAABB(pParam.mat,box2);
				box.Add(box2);
			}

			if( pMaterial != NULL )							
				pNode->SetMaterial(pMaterial);	
		}

		if (pNode)
		{
			pNode->SetMatrix(mat);
			pNode->SetRndFlags(pParam.dwRenderNodeFlags|ERF_PROCEDURAL,true);
			pNode->SetLodRatio(pParam.nLodRatio);
			pNode->SetViewDistRatio(pParam.nViewDistRatio);				

			gEnv->p3DEngine->RegisterEntity(pNode);			
		}

		m_lstNodes.push_back(pNode);
	}	
}

//////////////////////////////////////////////////////////////////////////
void	CRuntimePrefab::Spawn(IEntity *pEntity,CPrefab &pRef, const Matrix34 &matOff,AABB &box)
{	
	m_pPrefab=&pRef;

	// store the prefab that was chosen
	IScriptTable *pScriptTable(pEntity->GetScriptTable());
	if (pScriptTable)
	{		
		ScriptAnyValue value(pRef.m_szName);
		pScriptTable->SetValueAny( "PrefabSourceName", value );
	}	

	m_mat=matOff;

	Matrix34 matSource=pEntity->GetWorldTM()*matOff;

	SpawnEntities(pRef,matSource,box);
	SpawnBrushes(pRef,matSource,box);

}

//////////////////////////////////////////////////////////////////////////
void	CRuntimePrefab::Spawn(CPrefab &pRef)
{
	Clear();
	
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity( m_id );	
	if (!pEntity)
	{		
		gEnv->pLog->LogError("Prefab Spawn: Entity ID %d not found",m_id);
		//assert(pEntity);
		return;
	}
	
	AABB box(AABB::RESET);
		
	// expand embedded prefabs
	m_lstPrefabs.reserve(pRef.m_lstPrefabs.size());
	for (std::vector<tPrefabParams>::const_iterator i=pRef.m_lstPrefabs.begin();i!=pRef.m_lstPrefabs.end();++i)
	{
		const tPrefabParams &pParams=(*i);		
		if (pParams.pPrefab)
		{
			CRuntimePrefab *pPrefab=new CRuntimePrefab(m_id);			
			pPrefab->Spawn(pEntity,*pParams.pPrefab,pParams.mat,box);

			m_lstPrefabs.push_back(pPrefab);
		}
	}			

	Spawn(pEntity,pRef,Matrix34::CreateIdentity(),box);
	
	// safety check in case the entities cannot be spawned for some reason and therefore the bounding box doesnt get set properly.
	// otherwise this will cause issue when inserting into the 3d engine octree
	if (box.min.x>box.max.x || box.min.y>box.max.y || box.min.z>box.max.z)
		box=AABB( 1.0f ); 

	pEntity->GetRenderInterface()->SetLocalBounds(box, true);

	// make sure all transformations are set
	Move(); 
}

//////////////////////////////////////////////////////////////////////////
void	CRuntimePrefab::Move(const Matrix34 &matOff)
{

	// entities
	int nPrefabRef=0;
	for (std::vector<EntityId>::iterator i=m_lstIDs.begin();i!=m_lstIDs.end();++i,nPrefabRef++)			
	{
		EntityId id=(*i);		

		IEntity	*pEntity = gEnv->pEntitySystem->GetEntity(id);		
		if (pEntity)		
		{
			const tEntityParams &pParam=m_pPrefab->m_lstEntities[nPrefabRef];			
			Matrix34 mat=matOff*pParam.mat;
			pEntity->SetWorldTM(mat);		
		}
	}

	// brushes and decals
	nPrefabRef=0;
	for (std::vector<IRenderNode*>::iterator i=m_lstNodes.begin();i!=m_lstNodes.end();++i,nPrefabRef++)			
	{
		IRenderNode *pNode=(*i);				
		if (!pNode)
			continue;

		gEnv->p3DEngine->UnRegisterEntityDirect(pNode);				

		const tBrushParams &pParam=m_pPrefab->m_lstBrushes[nPrefabRef];
				
		if (!pParam.bIsDecal)			
		{
			IStatObj *pStatObj=pNode->GetEntityStatObj(0,0,0);		
			pNode->SetEntityStatObj(pStatObj,0);				
		}

		Matrix34 mat=matOff*pParam.mat;		
		pNode->SetMatrix(mat);
		
		gEnv->p3DEngine->RegisterEntity(pNode);
	}
}

//////////////////////////////////////////////////////////////////////////
void	CRuntimePrefab::Move()
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity( m_id );	
	assert(pEntity);
		
	Vec3	vPos=pEntity->GetPos();
	Quat	qRot=pEntity->GetRotation();
	Vec3	vScale=pEntity->GetScale();

	Matrix34 matSource;
	matSource.Set(vScale,qRot,vPos);
	
	for (std::vector<CRuntimePrefab*>::iterator i=m_lstPrefabs.begin();i!=m_lstPrefabs.end();++i)
	{
		CRuntimePrefab *pPrefab=(*i);
		pPrefab->Move(matSource*pPrefab->m_mat);
	}

	Move(matSource);
}

//////////////////////////////////////////////////////////////////////////
void	CRuntimePrefab::HideComponents(bool bHide)
{
	// entities
	int nPrefabRef=0;
	for (std::vector<EntityId>::iterator i=m_lstIDs.begin();i!=m_lstIDs.end();++i)			
	{
		EntityId id=(*i);		

		IEntity	*pEntity = gEnv->pEntitySystem->GetEntity(id);		
		if (pEntity)		
			pEntity->Hide(bHide);
	}

	// brushes and decals
	for (std::vector<IRenderNode*>::iterator i=m_lstNodes.begin();i!=m_lstNodes.end();++i)			
	{
		IRenderNode *pNode=(*i);				
		if (pNode)
			pNode->Hide(bHide);
	}	
}

//////////////////////////////////////////////////////////////////////////
void	CRuntimePrefab::Hide(bool bHide)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity( m_id );	
	assert(pEntity);

	for (std::vector<CRuntimePrefab*>::iterator i=m_lstPrefabs.begin();i!=m_lstPrefabs.end();++i)
	{
		CRuntimePrefab *pPrefab=(*i);
		pPrefab->HideComponents(bHide);
	}

	HideComponents(bHide);
}
