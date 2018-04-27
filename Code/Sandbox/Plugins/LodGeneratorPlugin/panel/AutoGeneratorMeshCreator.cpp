#include "StdAfx.h"
#include "AutoGeneratorMeshCreator.h"
#include "AutoGeneratorDataBase.h"
#include "AutoGeneratorHelper.h"
#include "Material/Material.h"
#include "IExportManager.h"
#include "IEditor.h"
#include "Material/MaterialManager.h"
#include "Util/FileUtil.h"

namespace LODGenerator 
{
	CAutoGeneratorMeshCreator::CAutoGeneratorMeshCreator()
	{
		m_fPercentage = 50;
	}

	CAutoGeneratorMeshCreator::~CAutoGeneratorMeshCreator()
	{

	}

	void CAutoGeneratorMeshCreator::SetPercentage(float fPercentage)
	{
		m_fPercentage = fPercentage;
	}

	bool CAutoGeneratorMeshCreator::Generate(const int nLodId)
	{
		CAutoGeneratorDataBase* pAutoGeneratorDataBase = CAutoGeneratorDataBase::Instance();
		CAutoGeneratorParams& autoGeneratorParams = CAutoGeneratorDataBase::Instance()->GetParams();

		const bool bExportObj = autoGeneratorParams.GetGeometryOption<bool>("bExportObj");
		const bool bAddToParentMaterial = autoGeneratorParams.GetGeometryOption<bool>("bAddToParentMaterial");

		IStatObj *pLodStatObj;
		bool bGenerated = false;

		CLODGeneratorLib::SLODSequenceGenerationOutput* pResults = &(CAutoGeneratorDataBase::Instance()->GetBakeMeshResult());
		{
			CLODGeneratorLib::SLODGenerationInputParams lodParams;
			CLODGeneratorLib::SLODGenerationOutput lodReturn;
			lodParams.pSequence=pResults;
			lodParams.nPercentage=(int)(100*m_fPercentage);
			lodParams.bUnwrap=true;
			bGenerated = CLODGeneratorLib::GenerateLOD(&lodParams, &lodReturn);
			pLodStatObj = lodReturn.pStatObj;
		}

		if (bGenerated)
		{
			pLodStatObj->AddRef();
			string relFile;
			string lodId;

			lodId.Format("_lod%d.", nLodId);
			relFile=pAutoGeneratorDataBase->GetLoadedModel()->GetFilePath();
			relFile.Replace(".", lodId);
			string submatName(PathUtil::GetFileName(relFile));
			string fullPath( PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), relFile));
			string newMaterialName(fullPath);
			PathUtil::RemoveExtension(newMaterialName);

			if ( PrepareMaterial(newMaterialName, bAddToParentMaterial) )
			{
				int matId = CreateLODMaterial(submatName);
				CAutoGeneratorHelper::SetLODMaterialId(pLodStatObj,matId);
				pLodStatObj->SetMaterial(pAutoGeneratorDataBase->GetLODMaterial()->GetMatInfo());
				bool checkedOut = CAutoGeneratorHelper::CheckoutOrExtract(fullPath);

				bool bMergeAllNode = pAutoGeneratorDataBase->GetLoadedModel()->GetSubObjectCount() > 0 ? false : true;
				pLodStatObj->SaveToCGF(fullPath,NULL,false/*,bMergeAllNode*/);
				if (!checkedOut)
					CAutoGeneratorHelper::CheckoutOrExtract(fullPath);
			}

			CryLogAlways("Exported lod file: %s",relFile);
			if (!bExportObj)
				return false;

			IExportManager * pExporter = GetIEditor()->GetExportManager();
			if (!pExporter)
				return false;

			string objPath(fullPath);
			objPath = PathUtil::ReplaceExtension(objPath,"obj");
			string filename(PathUtil::GetFile(objPath));
			string exportpath;
			exportpath.Format("%s\\_obj\\%s", PathUtil::GetParentDirectory(string(objPath)).c_str(),filename);
			CFileUtil::CreateDirectory(exportpath);
			pExporter->ExportSingleStatObj(pLodStatObj,exportpath/*,false*/);

			pAutoGeneratorDataBase->ReloadModel();
		}

		return true;
	}

	bool CAutoGeneratorMeshCreator::PrepareMaterial(const string& materialName, bool bAddToMultiMaterial)
	{
		CAutoGeneratorDataBase* pAutoGeneratorDataBase = CAutoGeneratorDataBase::Instance();

		CMaterial *pMaterial=NULL;
		if (bAddToMultiMaterial)
		{
			pMaterial = pAutoGeneratorDataBase->GetLoadedMaterial();

			if (!pMaterial)
				return false;

			//convert to multimaterial
			if ( pMaterial->GetSubMaterialCount() == 0 )
			{	
				pMaterial->SetFlags(MTL_FLAG_MULTI_SUBMTL);
			}
		}
		else
		{
			pMaterial = GetIEditor()->GetMaterialManager()->LoadMaterial(materialName, false);
			if (!pMaterial)
			{
				pMaterial = GetIEditor()->GetMaterialManager()->CreateMaterial(materialName, XmlNodeRef(), MTL_FLAG_MULTI_SUBMTL);
			}
			if (!pMaterial)
			{
				return false;
			}
			pMaterial->SetSubMaterialCount(0);
		}
		pMaterial->UpdateFileAttributes();
		pAutoGeneratorDataBase->SetLODMaterial(pMaterial);

		CAutoGeneratorHelper::CheckoutOrExtract(pMaterial->GetFilename());

		return true;
	}

	int CAutoGeneratorMeshCreator::CreateLODMaterial(const string& submatName)
	{
		CAutoGeneratorDataBase* pAutoGeneratorDataBase = CAutoGeneratorDataBase::Instance();

		CMaterial *pEditorMat = pAutoGeneratorDataBase->GetLODMaterial();
		if(!pEditorMat)
			return -1;

		int subMatId = FindExistingLODMaterial(submatName);
		if (subMatId != -1)
			return subMatId;

		if (subMatId == -1)
			subMatId = pEditorMat->GetSubMaterialCount();

		//load material template
		CMaterial * templateMat = CAutoGeneratorHelper::CreateMaterialFromTemplate(submatName);
		if (!templateMat)
			return -1;

		//set the submaterial correctly for the .mtl file
		pEditorMat->SetSubMaterialCount(subMatId+1);
		pEditorMat->SetSubMaterial(subMatId,templateMat);
		pEditorMat->Save(true);

		return subMatId;
	}


	int CAutoGeneratorMeshCreator::FindExistingLODMaterial(const string& matName)
	{
		CAutoGeneratorDataBase* pAutoGeneratorDataBase = CAutoGeneratorDataBase::Instance();

		CMaterial* pMat = pAutoGeneratorDataBase->GetLoadedMaterial();
		const int submatCount = pMat->GetSubMaterialCount();
		int matId = -1;
		//attempt to find existing material
		int count = 0;
		while (matId == -1 && count<submatCount)
		{
			CMaterial * pSubMat = pMat->GetSubMaterial(count);
			if ( pSubMat )
			{
				string name(pSubMat->GetName());
				int ret = name.CompareNoCase(matName);
				if (ret == 0)
				{
					matId = count;
					break;
				}
			}
			count++;
		}
		return matId;
	}
}