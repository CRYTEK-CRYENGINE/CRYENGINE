#include "StdAfx.h"
#include "AutoGeneratorDataBase.h"
#include "IEditor.h"
#include "Material/MaterialManager.h"
#include <Cry3DEngine/IIndexedMesh.h>

namespace LODGenerator 
{
	CAutoGeneratorDataBase* CAutoGeneratorDataBase::m_pInstance = NULL;

	CAutoGeneratorDataBase::CAutoGeneratorDataBase()
	{
		m_pLoadedStatObj = NULL;
		m_pLoadedMaterial = NULL;
		m_pLODMaterial = NULL;
		m_resultsMap.clear();
		memset(&m_results, 0, sizeof(m_results));
	}

	CAutoGeneratorDataBase::~CAutoGeneratorDataBase()
	{

	}

	CAutoGeneratorParams& CAutoGeneratorDataBase::GetParams()
	{
		return m_autoGeneratorParams;
	}

	CAutoGeneratorDataBase* CAutoGeneratorDataBase::Instance()
	{
		if (m_pInstance)
			return m_pInstance;

		m_pInstance = new CAutoGeneratorDataBase();
		return m_pInstance;
	}

	void CAutoGeneratorDataBase::DestroyInstance()
	{
		SAFE_DELETE(m_pInstance);
	}

	bool CAutoGeneratorDataBase::LoadStatObj(const string& filepath)
	{
		int sourceLod = m_autoGeneratorParams.GetGeometryOption<int>("nSourceLod");

		if (stricmp(PathUtil::GetExt(filepath),CRY_GEOMETRY_FILE_EXT) == 0)
		{
			m_pLoadedStatObj = GetIEditor()->Get3DEngine()->LoadStatObj(filepath, NULL, NULL, false);
			if (m_pLoadedStatObj)
			{
				const int nLods = NumberOfLods();
				m_autoGeneratorParams.EnableUpdateCallbacks(false);
				m_autoGeneratorParams.SetGeometryOptionLimit(0,nLods-1,1,true,true);
				m_autoGeneratorParams.SetGeometryOption("fViewResolution",128.0f/(2.0f*GetRadius(sourceLod)));

				string path(PathUtil::GetParentDirectory(filepath));
				string ext;
				ext.Format("%s_lod[LodId]_[AlphaBake][BakedTextureType].tif", PathUtil::GetFileName(filepath));
				m_autoGeneratorParams.SetMaterialOption("ExportPath", path);
				m_autoGeneratorParams.SetMaterialOption("FilenameTemplate", ext);

				m_autoGeneratorParams.EnableUpdateCallbacks(true);

			}
		}

		return m_pLoadedStatObj != NULL;
	}

	bool CAutoGeneratorDataBase::LoadMaterial(string strMaterial)
	{
		m_pLoadedMaterial = GetIEditor()->GetMaterialManager()->LoadMaterial(strMaterial);

		if (NumberOfLods() > 1)
		{
			m_pLODMaterial = m_pLoadedMaterial;
		}

		return m_pLoadedMaterial != NULL;
	}

	IStatObj* CAutoGeneratorDataBase::GetLoadedModel(int nLod)
	{
		if ( nLod == 0 )
			return m_pLoadedStatObj;
		return m_pLoadedStatObj->GetLodObject(nLod);
	}

	CMaterial* CAutoGeneratorDataBase::GetLoadedMaterial()
	{
		return m_pLoadedMaterial;	
	}

	CMaterial* CAutoGeneratorDataBase::GetLODMaterial()
	{
		return m_pLODMaterial;
	}

	void CAutoGeneratorDataBase::ReloadModel()
	{
		if(m_pLoadedStatObj)
			m_pLoadedStatObj->Refresh(FRO_GEOMETRY|FRO_FORCERELOAD);
	}

	void CAutoGeneratorDataBase::ReloadMaterial()
	{
		if (m_pLoadedMaterial)
			m_pLoadedMaterial->Reload();
	}

	int CAutoGeneratorDataBase::NumberOfLods()
	{
		if (m_pLoadedStatObj == NULL)
			return -1;

		int nLods = 0;
		for(int i=0; i<MAX_STATOBJ_LODS_NUM; ++i)
		{
			if ( m_pLoadedStatObj->GetLodObject(i) )
				nLods += 1;
		}
		return nLods;
	}

	CLODGeneratorLib::SLODSequenceGenerationOutput& CAutoGeneratorDataBase::GetBakeMeshResult()
	{
		return m_results;
	}

	std::map<int,SMeshBakingOutput>& CAutoGeneratorDataBase::GetBakeTextureResult()
	{
		return m_resultsMap;
	}

	void CAutoGeneratorDataBase::SetLODMaterial(CMaterial *pMat)
	{
		m_pLODMaterial=pMat;
	}

	void CAutoGeneratorDataBase::ClearTextureResults(int level)
	{
		std::map<int,SMeshBakingOutput>::iterator iter = m_resultsMap.find(level);
		if ( iter != m_resultsMap.end() )
		{
			for (int i=eTextureType_Diffuse; i<eTextureType_Max; i++)
			{
				if ( m_resultsMap[level].ppOuputTexture[i] != NULL )
				{
					m_resultsMap[level].ppOuputTexture[i]->Release();
					m_resultsMap[level].ppOuputTexture[i] = NULL;
				}

				if ( m_resultsMap[level].ppIntermediateTexture[i] != NULL )
				{
					m_resultsMap[level].ppIntermediateTexture[i]->Release();
					m_resultsMap[level].ppIntermediateTexture[i] = NULL;
				}
			}
			m_resultsMap.erase(iter);
		}
	}

	void CAutoGeneratorDataBase::ClearAllTextureResults()
	{
		for (int idx = MAX_STATOBJ_LODS_NUM-1; idx >= 1; --idx)
		{
			std::map<int,SMeshBakingOutput>::iterator iter = m_resultsMap.find(idx);
			if ( iter != m_resultsMap.end() )
			{
				for (int i=eTextureType_Diffuse; i<eTextureType_Max; i++)
				{
					if ( m_resultsMap[idx].ppOuputTexture[i] != NULL )
					{
						m_resultsMap[idx].ppOuputTexture[i]->Release();
						m_resultsMap[idx].ppOuputTexture[i] = NULL;
					}

					if ( m_resultsMap[idx].ppIntermediateTexture[i] != NULL )
					{
						m_resultsMap[idx].ppIntermediateTexture[i]->Release();
						m_resultsMap[idx].ppIntermediateTexture[i] = NULL;
					}
				}
				m_resultsMap.erase(iter);
			}
		}
		m_resultsMap.clear();
	}


	int CAutoGeneratorDataBase::GetSubMatId(const int nLodId)
	{
		int nMatId = -1;
		if (!m_pLoadedStatObj)
			return nMatId;

		IStatObj* pStatObj = NULL;
		if (nLodId == 0)
		{
			pStatObj = m_pLoadedStatObj;
		}
		else
		{
			pStatObj = m_pLoadedStatObj->GetLodObject(nLodId);
		}

		if (!pStatObj)
			return nMatId;

		IIndexedMesh* pIdxMesh = pStatObj->GetIndexedMesh(true);
		if (!pIdxMesh)
		{
			// validate that only one subobject has a mesh
			IStatObj *subobj = NULL;
			for(int i=0; i < pStatObj->GetSubObjectCount(); i++)
			{
				if(pStatObj->GetSubObject(i)->nType == STATIC_SUB_OBJECT_MESH &&
					pStatObj->GetSubObject(i)->pStatObj &&
					pStatObj->GetSubObject(i)->pStatObj->GetRenderMesh())
				{
					// NOTE: we fail if more than one sub object has a renderable mesh.
					if(pIdxMesh != NULL)
						return nMatId;

					pIdxMesh = pStatObj->GetSubObject(i)->pStatObj->GetIndexedMesh(true);
				}
			}
		}

		if (!pIdxMesh)
			return nMatId;

		return pIdxMesh->GetSubSet(0).nMatID;
	}

	float CAutoGeneratorDataBase::GetRadius(int nLod)
	{
		if (m_pLoadedStatObj)
		{
			const int numLods = NumberOfLods();
			if ( nLod < numLods )
			{
				IStatObj* pStatObjLod = m_pLoadedStatObj->GetLodObject(nLod);
				if ( pStatObjLod )
					return pStatObjLod->GetAABB().GetRadius();
			}
		}
		return 0.0f;
	}
}
