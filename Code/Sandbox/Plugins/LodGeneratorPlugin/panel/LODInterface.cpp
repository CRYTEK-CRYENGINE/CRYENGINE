/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2013.
*************************************************************************/

#include "StdAfx.h"
#include "LODInterface.h"
#include "Util/PakFile.h"
#include "Objects/BrushObject.h"
#include "Objects/EntityObject.h"
#include "Objects/GeomEntity.h"
#include "Material/MaterialManager.h"
#include "Export/ExportManager.h"
#include "Util/Image.h"
#include "Util/ImageTIF.h"
#include "Util/BoostPythonHelpers.h"
#include "ISourceControl.h"
#include "../control/RampControl.h"
#include "../control/MeshBakerPopupPreview.h"
#include "Util/FileUtil.h"
#include "Controls/QuestionDialog.h"

namespace LODGenerator 
{

#define MAX_MAT_SLOTS 127

	//////////////////////////////////////////////////////////////////////////
	// Engine Interface 
	//////////////////////////////////////////////////////////////////////////

	//! Returns true when the given path is relative, otherwise false
	inline bool IsAbsolute(const char* path)
	{
	#if CRY_PLATFORM_WINDOWS
		return path && isalpha(path[0]) && (path[1] == ':') && (path[2] == '\\' || path[2] == '/');
	#else
		return path && (path[0] == '/');
	#endif
	}

	inline bool IsRelative(const char *path)
	{
		return !IsAbsolute(path);
	}

	CLodGeneratorInteractionManager* CLodGeneratorInteractionManager::m_pInstance = NULL;

	CLodGeneratorInteractionManager* CLodGeneratorInteractionManager::Instance()
	{
		if (m_pInstance)
			return m_pInstance;

		m_pInstance = new CLodGeneratorInteractionManager();
		return m_pInstance;
	}

	void CLodGeneratorInteractionManager::DestroyInstance()
	{
		SAFE_DELETE(m_pInstance);
	}

	CLodGeneratorInteractionManager::CLodGeneratorInteractionManager()
	{
		m_pLoadedStatObj = NULL;
		memset(&m_results, 0, sizeof(m_results));
		m_bAwaitingResults = false;
		m_parameterFilePath = "";
		m_changeId = string("default");

		m_pMaterialVarBlock = new CVarBlock();
		m_pMaterialVarBlock->AddVariable(m_fRayLength,"RayTestLength");
		m_fRayLength->SetHumanName("Ray Test Length");
		m_fRayLength->SetDescription("The length of the ray used to hit the mesh surface for sampling");
		m_fRayLength->SetLimits(0.001f, 100.0f, 0.25f, true, false);
		m_fRayLength=0.5f;

		m_pMaterialVarBlock->AddVariable(m_fRayStartDist,"RayStartDist");
		m_fRayStartDist->SetHumanName("Ray Start Distance");
		m_fRayStartDist->SetDescription("The distance from the case to start the ray tracing");
		m_fRayStartDist->SetLimits(0.001f, 100.0f, 0.25f, true, false);
		m_fRayStartDist=0.25f;

		m_pMaterialVarBlock->AddVariable(m_bBakeAlpha,"BakeAlpha");
		m_bBakeAlpha->SetHumanName("Bake the Alpha Channel");
		m_bBakeAlpha->SetDescription("Enables baking of alpha into the alpha channel");
		m_bBakeAlpha=false;

		m_pMaterialVarBlock->AddVariable(m_bBakeSpec,"BakeUniqueSpecularMap");
		m_bBakeSpec->SetHumanName("Bake A Unique Specular Map");
		m_bBakeSpec->SetDescription("toggles the baking of a full unique specular map per material lod");
		m_bBakeSpec=false;


		m_pMaterialVarBlock->AddVariable(m_bSmoothCage,"SmoothCage");
		m_bSmoothCage->SetHumanName("Smooth Cage");
		m_bSmoothCage->SetDescription("Uses averge face normal instead of vertex normal for ray casting");
		m_bSmoothCage=false;

		m_pMaterialVarBlock->AddVariable(m_bDilationPass,"DilationPass");
		m_bDilationPass->SetHumanName("Dilation Pass");
		m_bDilationPass->SetDescription("Reduces filtering errors by filling in none baked areas");
		m_bDilationPass=true;

		m_pMaterialVarBlock->AddVariable(m_cBackgroundColour,"BackgroundColour");
		m_cBackgroundColour->SetHumanName("Background Colour");
		m_cBackgroundColour->SetDescription("Used to fill any areas of the texture that didn't get filled by the ray casting");
		m_cBackgroundColour->SetDataType(IVariable::DT_COLOR);
		m_cBackgroundColour = Vec3(0.0f, 0.0f, 0.0f);

		m_pMaterialVarBlock->AddVariable(m_strExportPath,"ExportPath");
		m_strExportPath->SetHumanName("Export Path");
		m_strExportPath->SetDescription("Where to export the baked textures");

		m_pMaterialVarBlock->AddVariable(m_strFilename,"FilenameTemplate");
		m_strFilename->SetHumanName("Filename Pattern");
		m_strFilename->SetDescription("Filename pattern template to use for the newly created texture filenames");

		m_pMaterialVarBlock->AddVariable(m_strPresetDiffuse,"DiffuseTexturePreset");
		m_strPresetDiffuse->SetHumanName("Diffuse Preset");
		m_strPresetDiffuse->SetDescription("The RC preset for the Albedo Map texture");

		m_pMaterialVarBlock->AddVariable(m_strPresetNormal,"NormalTexturePreset");
		m_strPresetNormal->SetHumanName("Normal Preset");
		m_strPresetNormal->SetDescription("The RC preset for the Normal Map texture");

		m_pMaterialVarBlock->AddVariable(m_strPresetSpecular,"SpecularTexturePreset");
		m_strPresetSpecular->SetHumanName("Specular Preset");
		m_strPresetSpecular->SetDescription("The RC preset for the Reflectance Map texture");

		m_pMaterialVarBlock->AddVariable(m_useAutoTextureSize,"UseAutoTextureSize");
		m_useAutoTextureSize->SetHumanName("Use Auto Texture Size");
		m_useAutoTextureSize->SetDescription("Choose the texture size based on the size of the object");
		m_useAutoTextureSize->SetFlags(IVariable::UI_INVISIBLE);

		m_pMaterialVarBlock->AddVariable(m_autoTextureRadius1,"AutoTextureRadius1");
		m_autoTextureRadius1->SetHumanName("Auto Texture Radius 1");
		m_autoTextureRadius1->SetDescription("The radius for auto size option 1");
		m_autoTextureRadius1->SetFlags(IVariable::UI_INVISIBLE);
		m_autoTextureRadius1=8.0f;
		m_pMaterialVarBlock->AddVariable(m_autoTextureSize1,"AutoTextureSize1");
		m_autoTextureSize1->SetHumanName("Auto Texture Size 1");
		m_autoTextureSize1->SetDescription("The texture size for auto size option 1");
		m_autoTextureSize1->SetFlags(IVariable::UI_INVISIBLE);
		m_autoTextureSize1=1024;

		m_pMaterialVarBlock->AddVariable(m_autoTextureRadius2,"AutoTextureRadius2");
		m_autoTextureRadius2->SetHumanName("Auto Texture Radius 2");
		m_autoTextureRadius2->SetDescription("The radius for auto size option 2");
		m_autoTextureRadius2->SetFlags(IVariable::UI_INVISIBLE);
		m_autoTextureRadius2=4.0f;
		m_pMaterialVarBlock->AddVariable(m_autoTextureSize2,"AutoTextureSize2");
		m_autoTextureSize2->SetHumanName("Auto Texture Size 2");
		m_autoTextureSize2->SetDescription("The texture size for auto size option 2");
		m_autoTextureSize2->SetFlags(IVariable::UI_INVISIBLE);
		m_autoTextureSize2=512;

		m_pMaterialVarBlock->AddVariable(m_autoTextureRadius3,"AutoTextureRadius3");
		m_autoTextureRadius3->SetHumanName("Auto Texture Radius 3");
		m_autoTextureRadius3->SetDescription("The radius for auto size option 3");
		m_autoTextureRadius3->SetFlags(IVariable::UI_INVISIBLE);
		m_autoTextureRadius3=0.0f;
		m_pMaterialVarBlock->AddVariable(m_autoTextureSize3,"AutoTextureSize3");
		m_autoTextureSize3->SetHumanName("Auto Texture Size 3");
		m_autoTextureSize3->SetDescription("The texture size for auto size option 3");
		m_autoTextureSize3->SetFlags(IVariable::UI_INVISIBLE);
		m_autoTextureSize3=128;

		m_strPresetDiffuse = "Albedo /reduce=1";
		m_strPresetNormal = "NormalsWithSmoothness /reduce=1";
		m_strPresetSpecular = "Reflectance /reduce=1";

		m_pGeometryVarBlock = new CVarBlock();

		m_pGeometryVarBlock->AddVariable(m_nSourceLod,"nSourceLod");
		m_nSourceLod->SetHumanName("Source Lod");
		m_nSourceLod->SetDescription("Source Lod to generate the Lod chain from");

		m_pGeometryVarBlock->AddVariable(m_bObjectHasBase, "bObjectHasBase");
		m_bObjectHasBase->SetHumanName("Object has base");
		m_bObjectHasBase->SetDescription("If object doesn't have a base then we won't have use any views looking up at it when calculating error.");
		m_bObjectHasBase=false;

		m_pGeometryVarBlock->AddVariable(m_fViewResolution, "fViewResolution");
		m_fViewResolution->SetHumanName("Pixels per metre");
		m_fViewResolution->SetDescription("The LOD generator judges if a change is good by considering what the object looks like from many views. This alters the resolution of these views. A low value means quicker results but potentially worst accuracy. This is in pixels per meter.");
		m_fViewResolution=25;
		m_fViewResolution->SetLimits(0.0001f, 1024.0f, 1.0f, true, false);

		m_pGeometryVarBlock->AddVariable(m_nViewsAround, "nViewsAround");
		m_nViewsAround->SetHumanName("Views around");
		m_nViewsAround->SetDescription("The LOD generator judges if a change is good by considering what the object looks like from many views. This alters the number of views around the object. Each elevation has a ring of this many views. Less views means quicker results but potentially worst accuracy.");
		m_nViewsAround->SetLimits(1, 128);
		m_nViewsAround=12;

		m_pGeometryVarBlock->AddVariable(m_nViewElevations, "nViewElevations");
		m_nViewElevations->SetHumanName("View elevations");
		m_nViewElevations->SetDescription("The LOD generator judges if a change is good by considering what the object looks like from many views. This alters the number of levels at which views are taken. Less views means quicker results but potentially worst accuracy.");
		m_nViewElevations=3;
		m_nViewElevations->SetLimits(1, 128);

		m_pGeometryVarBlock->AddVariable(m_fSilhouetteWeight, "fSilhouetteWeight");
		m_fSilhouetteWeight->SetHumanName("Silhouette weighting");
		m_fSilhouetteWeight->SetDescription("Changes the weighting given to silhouette changes. A low value means a silhouette change isn't considered much than other changes in depth. A high value means the silhouette is preserved above all else.");
		m_fSilhouetteWeight=5.0f;
		m_fSilhouetteWeight->SetLimits(0.0f, 1000.0f, 1.0f, true, false);

		m_pGeometryVarBlock->AddVariable(m_fVertexWelding, "fVertexWelding");
		m_fVertexWelding->SetHumanName("Vertex weld distance");
		m_fVertexWelding->SetDescription("Before starting the LOD process vertices within this this distance will be welded together.");
		m_fVertexWelding=0.001f;
		m_fVertexWelding->SetLimits(0.0f, 0.1f, 0.001f, true, false);

		m_pGeometryVarBlock->AddVariable(m_bCheckTopology, "bCheckTopology");
		m_bCheckTopology->SetHumanName("Check topology is correct");
		m_bCheckTopology->SetDescription("If a move makes a bad bit topology (a bow tie or more than two triangles sharing a single end) then that move is rejected. This can cause problems if the topology is bad to begin with (or caused by vertex welding).");
		m_bCheckTopology=true;

		m_pGeometryVarBlock->AddVariable(m_bWireframe, "bWireframe");
		m_bWireframe = true;
		m_bWireframe->SetDescription("Toggles wireframe preview of the generated lod");
		m_bWireframe->SetHumanName("Toggle Wireframe");

		m_pGeometryVarBlock->AddVariable(m_bExportObj, "bExportObj");
		m_bExportObj = true;
		m_bExportObj->SetDescription("Toggles saving the generated geometry and uvs to an obj file");
		m_bExportObj->SetHumanName("Toggle obj export");

		m_pGeometryVarBlock->AddVariable(m_bAddToParentMaterial, "bAddToParentMaterial");
		m_bAddToParentMaterial = true;
		m_bAddToParentMaterial->SetDescription("If true, adds the new lod material to the parent else creates a separate lod material");
		m_bAddToParentMaterial->SetHumanName("Add to parent material");

		m_pGeometryVarBlock->AddVariable(m_bUseCageMesh, "bUseCageMesh");
		m_bUseCageMesh = false;
		m_bUseCageMesh->SetDescription("If true, will use a lod cage for generating the lod");
		m_bUseCageMesh->SetHumanName("Use lod cage");

		m_pGeometryVarBlock->AddVariable(m_bPreviewSourceLod, "bSourceLod");
		m_bPreviewSourceLod->SetHumanName("PreviewSourceLod");
		m_bPreviewSourceLod->SetDescription("Toggles on the preview of the Source Lod");
	}

	CLodGeneratorInteractionManager::~CLodGeneratorInteractionManager() 
	{
		m_pLoadedStatObj = NULL;
	}

	const string CLodGeneratorInteractionManager::GetSelectedBrushFilepath()
	{
		string objectName("");
		CBaseObject * pObject = GetIEditor()->GetSelectedObject();
		if (!pObject)
			return objectName;

		if (pObject->IsKindOf(RUNTIME_CLASS(CBrushObject)))
		{
			CBrushObject *pBrushObject = (CBrushObject*)pObject;
			return string(pBrushObject->GetIStatObj()->GetFilePath());
		}
		// 	else if(pObject->IsKindOf(RUNTIME_CLASS(CGeomEntity)))
		// 	{
		// 		CGeomEntity *pBrushObject = (CGeomEntity*)pObject;
		// 		return CString(pBrushObject->GetGeometryFile());
		// 	}

		return objectName;
	}

	const string CLodGeneratorInteractionManager::GetSelectedBrushMaterial()
	{
		string materialName("");
		CBaseObject * pObject = GetIEditor()->GetSelectedObject();
		if (!pObject)
			return materialName;

		IEditorMaterial* pMaterial = NULL;
		if (pObject->IsKindOf(RUNTIME_CLASS(CBrushObject)))
		{
			CBrushObject* pBrushObject = (CBrushObject*)pObject;
			pMaterial = pBrushObject->GetRenderMaterial();
		}
		// 	else if (pObject->IsKindOf(RUNTIME_CLASS(CGeomEntity)))
		// 	{
		// 		CGeomEntity* pBrushObject = (CGeomEntity*)pObject;
		// 		pMaterial = pBrushObject->GetRenderMaterial();
		// 	}

		if (!pMaterial)
			return materialName;

		string convertedPath(PathUtil::AbsolutePathToGamePath(pMaterial->GetFullName()));
		//PathUtil::Sl(convertedPath);
		return convertedPath;
	}

	const string CLodGeneratorInteractionManager::GetDefaultBrushMaterial(const string& filepath)
	{
		string matFilename("");
		if (m_pLoadedStatObj)
		{
			IMaterial* pMaterial = m_pLoadedStatObj->GetMaterial();
			if (pMaterial)
				return string(pMaterial->GetName());
		}

		string tempMatFilename;
		tempMatFilename.Format("%s",PathUtil::ReplaceExtension(filepath,"mtl"));
		if ( CFileUtil::FileExists(tempMatFilename) )
			matFilename = tempMatFilename;

		return matFilename;
	}

	void CLodGeneratorInteractionManager::OpenMaterialEditor()
	{
		//bug where it doesnt select it first time so do it twice meh
		GetIEditor()->GetMaterialManager()->GotoMaterial(m_pLoadedMaterial);
		GetIEditor()->GetMaterialManager()->GotoMaterial(m_pLoadedMaterial);
	}

	bool CLodGeneratorInteractionManager::LoadStatObj(const string& filepath)
	{
		if (stricmp(PathUtil::GetExt(filepath),CRY_GEOMETRY_FILE_EXT) == 0)
		{
			m_pLoadedStatObj = GetIEditor()->Get3DEngine()->LoadStatObj(filepath, NULL, NULL, false);
			if (m_pLoadedStatObj)
			{
				const int nLods = NumberOfLods();
				m_pGeometryVarBlock->EnableUpdateCallbacks(false);
				m_pMaterialVarBlock->EnableUpdateCallbacks(false);

				m_nSourceLod->SetLimits(0,nLods-1,1,true,true);
				m_fViewResolution=128.0f/(2.0f*GetRadius(m_nSourceLod));

				LoadSettings();
				UpdateFilename(filepath);
				CreateChangelist();

				m_pGeometryVarBlock->EnableUpdateCallbacks(true);
				m_pMaterialVarBlock->EnableUpdateCallbacks(true);

			}
		}

		return m_pLoadedStatObj != NULL;
	}

	bool CLodGeneratorInteractionManager::LoadMaterial(string strMaterial)
	{
		m_pLoadedMaterial = GetIEditor()->GetMaterialManager()->LoadMaterial(strMaterial);

		if (NumberOfLods() > 1)
		{
			m_pLODMaterial = m_pLoadedMaterial;
		}

		return m_pLoadedMaterial != NULL;
	}

	CMaterial* CLodGeneratorInteractionManager::LoadSpecificMaterial(string strMaterial)
	{
		return GetIEditor()->GetMaterialManager()->LoadMaterial(strMaterial);
	}

	void CLodGeneratorInteractionManager::ReloadModel()
	{
		if(m_pLoadedStatObj)
			m_pLoadedStatObj->Refresh(FRO_GEOMETRY|FRO_FORCERELOAD);
	}

	void CLodGeneratorInteractionManager::ReloadMaterial()
	{
		if (m_pLoadedMaterial)
			m_pLoadedMaterial->Reload();
	}

	IStatObj* CLodGeneratorInteractionManager::GetLoadedModel(int nLod)
	{
		if ( nLod == 0 )
			return m_pLoadedStatObj;
		return m_pLoadedStatObj->GetLodObject(nLod);
	}

	CMaterial* CLodGeneratorInteractionManager::GetLoadedMaterial()
	{
		return m_pLoadedMaterial;	
	}

	CMaterial* CLodGeneratorInteractionManager::GetLODMaterial()
	{
		return m_pLODMaterial;
	}

	void CLodGeneratorInteractionManager::SetLODMaterial(CMaterial *pMat)
	{
		m_pLODMaterial=pMat;
	}

	const string CLodGeneratorInteractionManager::GetLoadedFilename()
	{
		string path("");
		if ( m_pLoadedStatObj )
			path = string(m_pLoadedStatObj->GetFilePath());
		return path;
	}

	const string CLodGeneratorInteractionManager::GetLoadedMaterialFilename()
	{
		string path("");
		if ( m_pLoadedMaterial )
			path = string(m_pLoadedMaterial->GetFilename());
		return path;
	}

	float CLodGeneratorInteractionManager::GetRadius(int nLod)
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

	const IStatObj::SStatistics CLodGeneratorInteractionManager::GetLoadedStatistics()
	{
		IStatObj::SStatistics stats;
		if (m_pLoadedStatObj)
			m_pLoadedStatObj->GetStatistics(stats);
		return stats;
	}

	int CLodGeneratorInteractionManager::NumberOfLods()
	{
		int nLods = 0;
		for(int i=0; i<MAX_STATOBJ_LODS_NUM; ++i)
		{
			if ( m_pLoadedStatObj->GetLodObject(i) )
				nLods += 1;
		}
		return nLods;
	}

	int CLodGeneratorInteractionManager::GetHighestLod()
	{
		int nHighestLod = 0;
		if ( m_pLoadedStatObj )
			nHighestLod = m_pLoadedStatObj->FindHighestLOD(0);
		return nHighestLod;
	}

	int CLodGeneratorInteractionManager::GetMaxNumLods()
	{
		return MAX_STATOBJ_LODS_NUM;
	}

	float CLodGeneratorInteractionManager::GetLodsPercentage(int nLodIdx)
	{
		if (!m_pLoadedStatObj)
			return 0.0f;

		const IStatObj::SStatistics sourceStats = GetLoadedStatistics();
		float maintris = (float)sourceStats.nIndices / 3;
		if ( IStatObj * pStatObjLod = m_pLoadedStatObj->GetLodObject(nLodIdx) )
		{
			IStatObj::SStatistics lodstats;
			pStatObjLod->GetStatistics(lodstats);
			float lodtris = (float)lodstats.nIndices / 3;
			return (lodtris/maintris) * 100.0f;
		}

		return 0.0f;
	}

	bool CLodGeneratorInteractionManager::LodGenGenerate()
	{
		int nSourceLod = GetGeometryOption<int>("nSourceLod");
		float fViewResolution = GetGeometryOption<float>("fViewResolution");
		int nViewsAround = GetGeometryOption<float>("nViewsAround");
		int nViewElevations = GetGeometryOption<float>("nViewElevations");
		float fSilhouetteWeight = GetGeometryOption<float>("fSilhouetteWeight");
		float fVertexWelding = GetGeometryOption<float>("fVertexWelding");
		bool bCheckTopology = GetGeometryOption<float>("bCheckTopology");
		bool bObjectHasBase = GetGeometryOption<bool>("bObjectHasBase");

		CLODGeneratorLib::SLODSequenceGenerationInputParams inputParams;
		inputParams.pInputMesh= nSourceLod == 0 ? m_pLoadedStatObj : m_pLoadedStatObj->GetLodObject(nSourceLod);
		inputParams.metersPerPixel=1.0f/fViewResolution;
		inputParams.numViewElevations=nViewElevations;
		inputParams.numViewsAround=nViewsAround;
		inputParams.bObjectHasBase=bObjectHasBase;
		inputParams.silhouetteWeight=fSilhouetteWeight;
		inputParams.vertexWeldingDistance=fVertexWelding;
		inputParams.bCheckTopology=bCheckTopology;

		CLODGeneratorLib::FreeLODSequence(&m_results);
		m_bAwaitingResults = false;
		if (CLODGeneratorLib::GenerateLODSequence(&inputParams, &m_results, true))
		{
			m_bAwaitingResults = true;
			return true;
		}
		return false;
	}

	bool CLodGeneratorInteractionManager::LodGenGenerateTick(float* fProgress)
	{
		if (m_bAwaitingResults)
		{
			if (CLODGeneratorLib::GetAsyncLODSequenceResults(&m_results, fProgress))
			{
				m_bAwaitingResults=false;
			}
		}
		return !m_bAwaitingResults;
	}

	void CLodGeneratorInteractionManager::LogGenCancel()
	{
		CLODGeneratorLib::CancelLODGeneration(&m_results);
		m_bAwaitingResults = false;
	}

	void CLodGeneratorInteractionManager::ClearUnusedLods(const int nNewLods)
	{
		if (!m_pLoadedStatObj)
			return;

		string filepath(m_pLoadedStatObj->GetFilePath());

		int nSourceLod = GetGeometryOption<int>("nSourceLod");
		for ( int idx = nSourceLod+nNewLods+1; idx < MAX_STATOBJ_LODS_NUM; ++idx )
		{
			// remove old lods when deleted
			string extension;
			string lodFilepath(filepath);
			extension.Format("_lod%d.cgf",idx);
			lodFilepath.Replace(".cgf",extension);

			// check if the file exists on disk if it does remove it.
			if ( CFileUtil::FileExists(lodFilepath) )
			{
				string gamefolder = PathUtil::GetGameFolder();
				string fullpath = gamefolder + string("/") + lodFilepath;
				CFileUtil::DeleteFile(CString(fullpath));
			}

			// remove lod material from material file
			string submatName(PathUtil::GetFileName(lodFilepath));

			CMaterial *pEditorMat = GetLoadedMaterial();
			if (!pEditorMat)
				continue;

			int submatIdx = FindExistingLODMaterial(submatName);
			if ( submatIdx == -1 )
				continue;

			pEditorMat->SetSubMaterial(submatIdx,NULL);
			pEditorMat->Save(true);
		}

		// strip embedded lods
		{
			for (int idx = 1; idx < MAX_STATOBJ_LODS_NUM; ++idx)
			{
				m_pLoadedStatObj->SetLodObject(idx, 0);
			}

			string gamefolder = PathUtil::GetGameFolder();
			string fullpath = gamefolder + string("/") + filepath;
			bool checkedOut = CheckoutOrExtract(fullpath);

			bool bMergeAllNode = m_pLoadedStatObj->GetSubObjectCount() > 0 ? false : true;
			m_pLoadedStatObj->SaveToCGF(fullpath,NULL,false/*,bMergeAllNode*/);
			if (!checkedOut)
				CheckoutOrExtract(fullpath);
		}
	}

	bool CLodGeneratorInteractionManager::PrepareMaterial(const string& materialName, bool bAddToMultiMaterial)
	{
		CMaterial *pMaterial=NULL;
		if (bAddToMultiMaterial)
		{
			pMaterial = GetLoadedMaterial();

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
		SetLODMaterial(pMaterial);

		CheckoutOrExtract(pMaterial->GetFilename());

		return true;
	}

	int CLodGeneratorInteractionManager::FindExistingLODMaterial(const string& matName)
	{
		CMaterial* pMat = GetLoadedMaterial();
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

	CMaterial * CLodGeneratorInteractionManager::CreateMaterialFromTemplate(const string& matName)
	{
		CMaterial * templateMaterial = GetIEditor()->GetMaterialManager()->LoadMaterial("%EDITOR%/Materials/lodgen_template.mtl");
		if (templateMaterial)
			templateMaterial->AddRef();

		CMaterial * pNewMaterial = (CMaterial*)GetIEditor()->GetMaterialManager()->FindItemByName(matName);
		if (!pNewMaterial)
			pNewMaterial = GetIEditor()->GetMaterialManager()->DuplicateMaterial(matName,templateMaterial);

		if (pNewMaterial)
			pNewMaterial->SetFlags(pNewMaterial->GetFlags()|MTL_FLAG_PURE_CHILD);

		templateMaterial->Release();
		return pNewMaterial;
	}

	int CLodGeneratorInteractionManager::CreateLODMaterial(const string& submatName)
	{
		CMaterial *pEditorMat = GetLODMaterial();
		if(!pEditorMat)
			return -1;

		int subMatId = FindExistingLODMaterial(submatName);
		if (subMatId != -1)
			return subMatId;

		if (subMatId == -1)
			subMatId = pEditorMat->GetSubMaterialCount();

		//load material template
		CMaterial * templateMat = CreateMaterialFromTemplate(submatName);
		if (!templateMat)
			return -1;

		//set the submaterial correctly for the .mtl file
		pEditorMat->SetSubMaterialCount(subMatId+1);
		pEditorMat->SetSubMaterial(subMatId,templateMat);
		pEditorMat->Save(true);

		return subMatId;
	}

	void CLodGeneratorInteractionManager::SetLODMaterialId(IStatObj * pStatObj, int matId)
	{
		if (!pStatObj)
			return;

		if (IIndexedMesh * pIMesh = pStatObj->GetIndexedMesh())
		{
			const int subsetCount = pIMesh->GetSubSetCount();
			for (int subsetIdx = 0; subsetIdx < subsetCount; ++subsetIdx)
			{
				pIMesh->SetSubsetMaterialId(subsetIdx,matId);
			}
		}

		const int subObjCount = pStatObj->GetSubObjectCount();
		for (int subObjIdx = 0; subObjIdx < subObjCount; ++subObjIdx)
		{
			IStatObj::SSubObject * pSubObj = pStatObj->GetSubObject(subObjIdx);
			if ( !pSubObj )
				continue;
			SetLODMaterialId(pSubObj->pStatObj, matId);
		}
	}

	bool CLodGeneratorInteractionManager::GenerateTemporaryLod(const float fPercentage, CCMeshBakerPopupPreview* ctrl, CCRampControl* ramp)
	{
		CLODGeneratorLib::SLODGenerationInputParams lodParams;
		CLODGeneratorLib::SLODGenerationOutput lodReturn;
		lodParams.pSequence=&m_results;
		lodParams.nPercentage=(int)fPercentage;
		lodParams.bUnwrap=false;
		if (ctrl && ramp && CLODGeneratorLib::GenerateLOD(&lodParams, &lodReturn))
		{
			ctrl->ReleaseObject();
			ctrl->SetObject(lodReturn.pStatObj);

			IStatObj::SStatistics stats;
			lodReturn.pStatObj->GetStatistics(stats);
			ramp->SetStats(RampControlMeshInfo(stats.nIndices,stats.nVertices));

			return true;
		}
		return false;
	}

	void CLodGeneratorInteractionManager::GenerateLod(const int nLodId, const float fPercentage)
	{
		const bool bExportObj = GetGeometryOption<bool>("bExportObj");
		const bool bAddToParentMaterial = GetGeometryOption<bool>("bAddToParentMaterial");
		const bool bUseCageMesh = GetGeometryOption<bool>("bUseCageMesh");

		IStatObj *pLodStatObj;
		bool bGenerated = false;
		if(bUseCageMesh)
		{
			bGenerated = GenerateLodFromCageMesh(&pLodStatObj);
			if(pLodStatObj)
			{
				for(int i=0; i < m_pLoadedStatObj->GetSubObjectCount(); i++)
				{
					if(m_pLoadedStatObj->GetSubObject(i)->nType == STATIC_SUB_OBJECT_MESH &&
						m_pLoadedStatObj->GetSubObject(i)->pStatObj &&
						m_pLoadedStatObj->GetSubObject(i)->pStatObj->GetRenderMesh())
					{
						pLodStatObj->SetGeoName(m_pLoadedStatObj->GetSubObject(i)->pStatObj->GetGeoName());
					}
				}
			}
		}

		if(!bGenerated)
		{
			CLODGeneratorLib::SLODGenerationInputParams lodParams;
			CLODGeneratorLib::SLODGenerationOutput lodReturn;
			lodParams.pSequence=&m_results;
			lodParams.nPercentage=(int)fPercentage;
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
			relFile=m_pLoadedStatObj->GetFilePath();
			relFile.Replace(".", lodId);
			string submatName(PathUtil::GetFileName(relFile));
			string fullPath(relFile);
			string newMaterialName(fullPath);
			PathUtil::RemoveExtension(newMaterialName);

			if ( PrepareMaterial(newMaterialName, bAddToParentMaterial) )
			{
				int matId = CreateLODMaterial(submatName);
				SetLODMaterialId(pLodStatObj,matId);
				pLodStatObj->SetMaterial(GetLODMaterial()->GetMatInfo());
				bool checkedOut = CheckoutOrExtract(fullPath);

				bool bMergeAllNode = m_pLoadedStatObj->GetSubObjectCount() > 0 ? false : true;
				pLodStatObj->SaveToCGF(fullPath,NULL,false/*,bMergeAllNode*/);
				if (!checkedOut)
					CheckoutOrExtract(fullPath);
			}

			CryLogAlways("Exported lod file: %s",relFile);
			if (!bExportObj)
				return;

			IExportManager * pExporter = GetIEditor()->GetExportManager();
			if (!pExporter)
				return;

			string objPath(fullPath);
			objPath = PathUtil::ReplaceExtension(objPath,"obj");
			string filename(PathUtil::GetFile(objPath));
			string exportpath;
			exportpath.Format("%s\\_obj\\%s", PathUtil::GetParentDirectory(string(objPath)).c_str(),filename);
			CFileUtil::CreateDirectory(exportpath);
			pExporter->ExportSingleStatObj(pLodStatObj,exportpath/*,false*/);
		}
	}

	bool CLodGeneratorInteractionManager::GenerateLodFromCageMesh(IStatObj **pLodStatObj)
	{
		string cageMeshFilename;
		string cageMeshString = "_autolodcage.";
		cageMeshFilename = m_pLoadedStatObj->GetFilePath();
		cageMeshFilename.Replace(".", cageMeshString);
		_smart_ptr<IStatObj> pCageStatObj = GetIEditor()->Get3DEngine()->LoadStatObj(cageMeshFilename, NULL, NULL, false);
		if (!pCageStatObj)
			return false;
		pCageStatObj->GetIndexedMesh(true); // This causes the index mesh to be created. 

		bool genIndexedMesh = false;
		if ( pCageStatObj->GetRenderMesh() && !pCageStatObj->GetIndexedMesh(false))
			genIndexedMesh = true;

		IStatObj *pOutStatObj=NULL;
		if ( pCageStatObj->GetIndexedMesh(genIndexedMesh) )
		{
			IIndexedMesh *pIndexedMesh = pCageStatObj->GetIndexedMesh();
			CMesh *pMesh = pIndexedMesh->GetMesh();

			Vec3 *pPositions = &(pMesh->m_pPositions[0]);
			vtx_idx *pIndices = &(pMesh->m_pIndices[0]);
			int newFaces = pMesh->GetIndexCount() / 3;

			pOutStatObj = CLODGeneratorLib::GenerateUVs(pPositions, pIndices, newFaces);
		}
		else
		{
			pOutStatObj=gEnv->p3DEngine->CreateStatObjOptionalIndexedMesh(false);
		}

		pOutStatObj->Invalidate();
		(*pLodStatObj) = pOutStatObj;
		return true;
	}

	int CLodGeneratorInteractionManager::GetNumMoves()
	{
		return m_results.numMoves;
	}

	float CLodGeneratorInteractionManager::GetErrorAtMove(const int nIndex)
	{
		if ( nIndex > 0 && nIndex < m_results.numMoves )
			return m_results.moveList[nIndex].error;
		return 0.0f;
	}

	void CLodGeneratorInteractionManager::SaveSettings()
	{
		if (!m_pLoadedStatObj)
			return;

		if (m_pGeometryVarBlock)
			m_pGeometryVarBlock->Serialize(GetSettings("GeometryLodSettings"), false);
		if (m_pMaterialVarBlock)
			m_pMaterialVarBlock->Serialize(GetSettings("MaterialLodSettings"), false);

		string settingsPath;
		settingsPath.Format("%s/_lodsettings.xml", ExportPath());
		if (IsRelative(settingsPath.c_str()))
			settingsPath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), settingsPath);
		CFileUtil::CreatePath(PathUtil::GetParentDirectory(settingsPath));

		if (m_xmlSettings)
			m_xmlSettings->saveToFile(settingsPath);

		CheckoutInSourceControl(settingsPath);
	}

	void CLodGeneratorInteractionManager::LoadSettings()
	{
		string settingsPath;
		settingsPath.Format("%s/_lodsettings.xml",ExportPath());
		m_xmlSettings = gEnv->pSystem->LoadXmlFromFile(settingsPath);
		if (!m_xmlSettings)
			CreateSettings();

		if (m_pGeometryVarBlock)
			m_pGeometryVarBlock->Serialize(GetSettings("GeometryLodSettings"),true);
		if (m_pMaterialVarBlock)
			m_pMaterialVarBlock->Serialize(GetSettings("MaterialLodSettings"),true);
	}

	void CLodGeneratorInteractionManager::CreateSettings()
	{
		m_xmlSettings = gEnv->pSystem->CreateXmlNode("GlobalLodSettings");
		m_xmlSettings->addChild(m_xmlSettings->createNode("GeometryLodSettings"));
		m_xmlSettings->addChild(m_xmlSettings->createNode("MaterialLodSettings"));
	}

	XmlNodeRef CLodGeneratorInteractionManager::GetSettings(const string& settings)
	{
		if (!m_xmlSettings)
			CreateSettings();
		return m_xmlSettings->findChild(settings);
	}

	void CLodGeneratorInteractionManager::ResetSettings()
	{
		m_pGeometryVarBlock->EnableUpdateCallbacks(false);
		m_pMaterialVarBlock->EnableUpdateCallbacks(false);
		m_bObjectHasBase=false;
		m_fViewResolution=25;
		float fRadius = CLodGeneratorInteractionManager::Instance()->GetRadius(m_nSourceLod);
		m_fViewResolution=128.0f/(2.0f*fRadius);
		m_nViewsAround=12;
		m_nViewElevations=3;
		m_fSilhouetteWeight=5.0f;
		m_fVertexWelding=0.001f;
		m_bCheckTopology=true;
		m_pGeometryVarBlock->EnableUpdateCallbacks(true);
		m_pMaterialVarBlock->EnableUpdateCallbacks(true);
	}

	string CLodGeneratorInteractionManager::ExportPath()
	{
		if (m_pLoadedStatObj)
			return PathUtil::GetParentDirectory(m_pLoadedStatObj->GetFilePath());
		return string("");
	}

	void CLodGeneratorInteractionManager::OpenExportPath()
	{
		CFileUtil::ShowInExplorer(CString(GetLoadedMaterialFilename()));
	}

	void CLodGeneratorInteractionManager::ShowLodInExplorer(const int nLodId)
	{
		string filename(GetLoadedFilename());
		string extension;
		extension.Format("_lod%d.cgf",nLodId);
		filename.Replace(".cgf",extension);
		CFileUtil::ShowInExplorer(CString(filename));
	}

	void CLodGeneratorInteractionManager::UpdateFilename(string filename)
	{
		string path(PathUtil::GetParentDirectory(filename));
		string ext;
		ext.Format("%s_lod[LodId]_[AlphaBake][BakedTextureType].tif", PathUtil::GetFileName(filename));

		SetMaterialOption("ExportPath", path);
		SetMaterialOption("FilenameTemplate", ext);
	}

	bool CLodGeneratorInteractionManager::CreateChangelist()
	{
		if (!GetIEditor()->IsSourceControlAvailable())
			return false;

		ISourceControl * pSControl = GetIEditor()->GetSourceControl();
		string description("Sandbox LOD Generator: ");
		char changeId[16];
		if ( m_pLoadedStatObj )
			description.Append(PathUtil::GetFileName(m_pLoadedStatObj->GetFilePath()));

		bool ret = pSControl->DoesChangeListExist(description,changeId,sizeof(changeId));
		if (!ret)
			ret = pSControl->CreateChangeList(description,changeId,sizeof(changeId));

		if (ret)
			m_changeId = string(changeId);

		return ret;
	}

	bool CLodGeneratorInteractionManager::CheckoutInSourceControl(const string& filename)
	{
		//try from source control
		bool bCheckedOut = false;
		ISourceControl * pSControl = GetIEditor()->GetSourceControl();
		if(pSControl)
		{
			string path(filename);
			int eFileAttribs = pSControl->GetFileAttributes(path);
			if (!(eFileAttribs & SCC_FILE_ATTRIBUTE_MANAGED) && (eFileAttribs & SCC_FILE_ATTRIBUTE_NORMAL))
			{
				char * changeId = (char*)m_changeId.GetBuffer();
				bCheckedOut = pSControl->Add(path, "LOD Tool Generation", ADD_WITHOUT_SUBMIT|ADD_CHANGELIST, changeId);
			}
			else if (!(eFileAttribs & SCC_FILE_ATTRIBUTE_BYANOTHER) && !(eFileAttribs & SCC_FILE_ATTRIBUTE_CHECKEDOUT) && (eFileAttribs & SCC_FILE_ATTRIBUTE_MANAGED))
			{
				if (pSControl->GetLatestVersion(path))
				{
					char * changeId = (char*)m_changeId.GetBuffer();
					bCheckedOut = pSControl->CheckOut(path, ADD_CHANGELIST, changeId);
				}
			}
		}
		else
		{
			CFileUtil::OverwriteFile(filename);
		}

		return bCheckedOut;
	}

	bool CLodGeneratorInteractionManager::CheckoutOrExtract(const char * filename)
	{
		if(GetIEditor()->IsSourceControlAvailable())
		{
			if(CheckoutInSourceControl(filename))
				return true;
		}
		uint32 attribs = CFileUtil::GetAttributes(filename);
		if (attribs & SCC_FILE_ATTRIBUTE_CHECKEDOUT)
		{
			return true;
		}
		if (attribs & SCC_FILE_ATTRIBUTE_INPAK)
		{
			string path(filename);
			CString st(path);
			return CFileUtil::ExtractFile(st,false);
		}
		return true;
	}

	int CLodGeneratorInteractionManager::GetSubMatId(const int nLodId)
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

	int CLodGeneratorInteractionManager::GetSubMatCount(const int nLodId)
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
			return nMatId;

		return pIdxMesh->GetSubSetCount();
	}

	bool CLodGeneratorInteractionManager::RunProcess(const int nLod, int nWidth, const int nHeight)
	{
		SMeshBakingMaterialParams params[MAX_MAT_SLOTS];
		CMaterial* pMat = GetLoadedMaterial();
		if (!pMat)
			return false;

		const int nSubMatId = GetSubMatId(nLod);
		const int nSourceLod = GetGeometryOption<int>("nSourceLod");
		const float fRayLength = GetMaterialOption<float>("RayTestLength");
		const float fRayIndent = GetMaterialOption<float>("RayStartDist");
		const bool bAlpha = GetMaterialOption<bool>("BakeAlpha");
		const bool bSmoothCage = GetMaterialOption<bool>("SmoothCage");
		const bool bDilationPass = GetMaterialOption<bool>("DilationPass");
		const Vec3 nBackgroundColour = GetMaterialOption<Vec3>("BackgroundColour");
		const bool bSaveSpec = GetMaterialOption<bool>("BakeUniqueSpecularMap");
		const bool bUseAutoTextureSize = GetMaterialOption<bool>("UseAutoTextureSize");

		int nTWidth = nWidth;
		int nTHeight = nHeight;	
		if(bUseAutoTextureSize)
		{
			CryLog("Using AutoTextureSize...");
			// Fill out the list with the size options
			std::vector< std::pair<float,int> > textureSizeList;
			const float autoRadius1 = GetMaterialOption<float>("AutoTextureRadius1");
			const int autoSize1 = GetMaterialOption<int>("AutoTextureSize1");
			textureSizeList.push_back(std::pair<float,int>(autoRadius1,autoSize1));

			const float autoRadius2 = GetMaterialOption<float>("AutoTextureRadius2");
			const int autoSize2 = GetMaterialOption<int>("AutoTextureSize2");
			textureSizeList.push_back(std::pair<float,int>(autoRadius2,autoSize2));

			const float autoRadius3 = GetMaterialOption<float>("AutoTextureRadius3");
			const int autoSize3 = GetMaterialOption<int>("AutoTextureSize3");
			textureSizeList.push_back(std::pair<float,int>(autoRadius3,autoSize3));

			std::sort(textureSizeList.begin(),textureSizeList.end(),std::greater< std::pair<float,int> >());

			// Find the best setting to use on this model
			int textureSize = nWidth; // Width and height should always be the same! There's only one size option in the UI.
			IStatObj *mesh = GetLoadedModel(nLod);
			if(mesh)
			{
				float objectSize = mesh->GetAABB().GetRadius();
				for(int i = 0;i<textureSizeList.size();i++)
				{
					if(objectSize > textureSizeList[i].first)
					{
						textureSize = textureSizeList[i].second;
						break;
					}
				}
				CryLog("AutoTextureSize Object Radius: %f, Texture Size: %d",objectSize,textureSize);
			}
			nTWidth = textureSize;
			nTHeight = textureSize;
		}

		const int submatCount = pMat->GetSubMaterialCount();
		for ( int idx = 0; idx < submatCount && idx < MAX_MAT_SLOTS; ++idx)
		{
			params[idx].rayLength=fRayLength;
			params[idx].rayIndent=fRayIndent;
			params[idx].bAlphaCutout=bAlpha;
			params[idx].bIgnore = (idx != nSubMatId);

			if (!params[idx].bIgnore && !VerifyNoOverlap(nSubMatId))
			{
				if (CQuestionDialog::SQuestion("UV Layout", "Detected overlapping UVs in target mesh. This could cause artifacts. Continue anyway?") == QDialogButtonBox::StandardButton::No)
					return false;
			}
		}

		SMeshBakingInputParams inParams;
		inParams.pInputMesh=GetLoadedModel(nSourceLod);
		inParams.pInputCharacter=NULL;
		inParams.pCageMesh=GetLoadedModel(nLod);
		inParams.pCageCharacter=NULL;
		inParams.outputTextureWidth=2*nTWidth; // Bake out at twice resolution, we'll half it during conversion so it'll match the selected resolution
		inParams.outputTextureHeight=2*nTHeight;
		inParams.pMaterialParams=params;
		inParams.numMaterialParams=submatCount;
		inParams.nLodId = nLod;
		inParams.bDoDilationPass=bDilationPass;
		inParams.bSaveSpecular = bSaveSpec;
		inParams.dilateMagicColour=ColorF(-16.0f, -16.0f, -16.0f, -16.0f); // Use the fact it's a floating point target to use a colour that'll never occur
		inParams.defaultBackgroundColour = ColorF(nBackgroundColour.x, nBackgroundColour.y, nBackgroundColour.z, 1.0f);
		inParams.bSmoothNormals=bSmoothCage;
		inParams.pMaterial=pMat->GetMatInfo();

		SMeshBakingOutput pReturnValues;
		bool ret = gEnv->pRenderer->BakeMesh(&inParams, &pReturnValues);
		if ( ret )
		{
			m_resultsMap.insert(std::pair<int,SMeshBakingOutput>(nLod,pReturnValues));
		}
		return ret;
	}

	void CLodGeneratorInteractionManager::SaveTextures(const int nLod)
	{
		if (m_resultsMap.find(nLod) == m_resultsMap.end())
			return;

		string exportPath = GetMaterialOption<string>("ExportPath");
		bool bBakeAlpha = GetMaterialOption<bool>("BakeAlpha");
		string filenamePattern = GetMaterialOption<string>("FilenameTemplate");
		bool bSaveSpec = GetMaterialOption<bool>("BakeUniqueSpecularMap");

		for (int i = eTextureType_Diffuse; i < eTextureType_Max; i++)
		{
			if (i == eTextureType_Spec && !bSaveSpec)
				continue;

			string cPath = GetDefaultTextureName(i,nLod,bBakeAlpha,exportPath,filenamePattern);
			if (IsRelative(cPath.c_str()))
				cPath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), cPath);
			CFileUtil::CreatePath(PathUtil::AddSlash(PathUtil::GetParentDirectory(cPath)));
			const bool checkedOut = CheckoutOrExtract(cPath);
			const string preset = GetPreset(i);
			const bool bSaveAlpha = (i == eTextureType_Diffuse && bBakeAlpha) || (i == eTextureType_Normal);
			SaveTexture(m_resultsMap[nLod].ppOuputTexture[i], bSaveAlpha, cPath, preset);
			if (!checkedOut)
				CheckoutOrExtract(cPath);
		}
		AssignToMaterial(nLod,bBakeAlpha,bSaveSpec,exportPath,filenamePattern);
		ReloadModel();
	}

	string CLodGeneratorInteractionManager::GetPreset(const int texturetype)
	{
		string preset("");
		switch(texturetype)
		{
		case eTextureType_Diffuse: preset = GetMaterialOption<string>("DiffuseTexturePreset");
			break;
		case eTextureType_Normal: preset = GetMaterialOption<string>("NormalTexturePreset");
			break;
		case eTextureType_Spec: preset = GetMaterialOption<string>("SpecularTexturePreset");
			break;
		}

		// NOTE: backwards-compatibility for old lod-settings
		preset.Replace("Normalmap_lowQ", "NormalsWithSmoothness");
		preset.Replace("Normalmap_highQ", "NormalsWithSmoothness");
		preset.Replace("NormalmapWithGlossInAlpha_highQ", "NormalsWithSmoothness");
		preset.Replace("Diffuse_lowQ", "Albedo");
		preset.Replace("Diffuse_highQ", "Albedo");
		preset.Replace("Specular_highQ", "Reflectance");
		preset.Replace("Specular_lowQ", "Reflectance");

		return preset;
	}

	const SMeshBakingOutput* CLodGeneratorInteractionManager::GetResults(const int nLod)
	{
		if ( m_resultsMap.find(nLod) == m_resultsMap.end())
			return NULL;

		return &m_resultsMap[nLod];
	}

	void CLodGeneratorInteractionManager::ClearResults()
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

	string CLodGeneratorInteractionManager::GetDefaultTextureName(const int i, const int nLod, const bool bAlpha, const string& exportPath, const string& fileNameTemplate)
	{
		char *defaultSubName[]={"diff", "ddna", "spec"};

		string strLodId;
		strLodId.Format("%d",nLod);

		string outName;
		outName=PathUtil::AddSlash(exportPath+"textures")+fileNameTemplate;
		outName.Replace("[BakedTextureType]", defaultSubName[i]);
		outName.Replace("[LodId]", strLodId);
		outName.Replace("[AlphaBake]", !bAlpha ? "" : "alpha_");
		outName=PathUtil::AbsolutePathToGamePath(outName);
		return outName;
	}

	bool CLodGeneratorInteractionManager::SaveTexture(ITexture* pTex,const bool bAlpha,const string& fileName,const string& texturePreset)
	{
		if (!pTex)
			return false;

		bool bSaved = false;
		CImageTIF tif;
		byte *pStorage=new byte[pTex->GetWidth()*pTex->GetHeight()*4];
		if (pStorage)
		{
			byte *pData=pTex->GetData32(0,0,pStorage);
			if (pData)
			{
				if (!bAlpha)
				{
					int h=pTex->GetHeight();
					int w=pTex->GetWidth();
					byte *pThreeChannelData=new byte[w*h*3];
					for (int y=0; y<h; y++)
					{
						for (int x=0; x<w; x++)
						{
							int pidx=(y*w+x);
							int pidx4=pidx*4;
							int pidx3=pidx*3;
							pThreeChannelData[pidx3+0]=pData[pidx4+0];
							pThreeChannelData[pidx3+1]=pData[pidx4+1];
							pThreeChannelData[pidx3+2]=pData[pidx4+2];
						}
					}
					bSaved=tif.SaveRAW(fileName, pThreeChannelData, pTex->GetWidth(), pTex->GetHeight(), 1, 3, false, texturePreset);
					delete[] pThreeChannelData;
				}
				else
				{
					bSaved=tif.SaveRAW(fileName, pData, pTex->GetWidth(), pTex->GetHeight(), 1, 4, false, texturePreset);
				}
			}
			delete[] pStorage;
		}
		return bSaved;
	}

	void CLodGeneratorInteractionManager::AssignToMaterial(const int nLod, const bool bAlpha, const bool bSaveSpec, const string& exportPath, const string& fileNameTemplate)
	{
		CMaterial* pMtl = GetLODMaterial();
		if(pMtl)
		{
			bool checkedOut = CheckoutOrExtract(pMtl->GetFilename());
			if (!checkedOut)
				return;

			// Generate base name
			string relFile;
			string lodId;
			lodId.Format("_lod%d.", nLod);
			relFile=m_pLoadedStatObj->GetFilePath();
			relFile.Replace(".", lodId);
			string submatName(PathUtil::GetFileName(relFile));

			//	const int nSubMatId = FindExistingLODMaterial(submatName);
			const int nSubMatId = GetSubMatId(nLod);
			float half=0.5f;
			ColorF diffuse(half, half, half, 1.0f);
			ColorF specular(half, half, half, 1.0f);
			ColorF emittance(half, half, half, 0.0f);
			float opacity = 1.0f;
			float smoothness = 1.0f;

			if(nSubMatId == -1)
			{
				CryLog("Error in AssignToMaterial: nSubMatId is -1");
				return;
			}

			CMaterial * pTemplateMaterial = GetIEditor()->GetMaterialManager()->LoadMaterial("%EDITOR%/Materials/lodgen_template.mtl");
			if (pTemplateMaterial)
			{
				pTemplateMaterial->AddRef();
				SInputShaderResources &sr=pTemplateMaterial->GetShaderResources();

				diffuse    = sr.m_LMaterial.m_Diffuse;
				specular   = sr.m_LMaterial.m_Specular;
				emittance  = sr.m_LMaterial.m_Emittance;
				opacity    = sr.m_LMaterial.m_Opacity;
				smoothness = sr.m_LMaterial.m_Smoothness;

				pTemplateMaterial->Release();
			}

			int subMaterialCount=pMtl->GetSubMaterialCount();
			CMaterial *pSubMat=pMtl->GetSubMaterial(nSubMatId);
			if (!pSubMat)
			{
				//load material template
				pSubMat = CreateMaterialFromTemplate(submatName);

				if ( nSubMatId >= subMaterialCount )
					pMtl->SetSubMaterialCount(nSubMatId+1);
				pMtl->SetSubMaterial(nSubMatId,pSubMat);
			}

			if (pSubMat)
			{
				SInputShaderResources &sr=pSubMat->GetShaderResources();
				sr.m_Textures[EFTT_DIFFUSE].m_Name = GetDefaultTextureName(0, nLod, bAlpha, exportPath, fileNameTemplate);
				sr.m_Textures[EFTT_NORMALS].m_Name = GetDefaultTextureName(1, nLod, bAlpha, exportPath, fileNameTemplate);

				if ( bSaveSpec)
					sr.m_Textures[EFTT_SPECULAR].m_Name = GetDefaultTextureName(2, nLod, bAlpha, exportPath, fileNameTemplate);
				else
					sr.m_Textures[EFTT_SPECULAR].m_Name = "";

				if ( bSaveSpec )
					specular = ColorF(1.0f,1.0f,1.0f,1.0f);

				sr.m_LMaterial.m_Diffuse    = diffuse;
				sr.m_LMaterial.m_Specular   = specular;
				sr.m_LMaterial.m_Emittance  = emittance;
				sr.m_LMaterial.m_Opacity    = opacity;
				sr.m_LMaterial.m_Smoothness = smoothness;

				sr.m_AlphaRef=bAlpha?0.5f:0.0f;
				sr.m_FurAmount=0;
				sr.m_VoxelCoverage=255;
				sr.m_HeatAmount=0;
			}

			pMtl->Update();
			if(!pMtl->Save(false))
			{
				CQuestionDialog::SWarning("Material File Save Failed", "The material file cannot be saved. The file is located in a PAK archive or access is denied (The mtl file is read-only).");
			}

			pMtl->Reload();
		}
	}

	bool CLodGeneratorInteractionManager::VerifyNoOverlap(const int nSubMatIdx)
	{
		IStatObj *pCage=GetLoadedModel();
		if (pCage)
		{
			IRenderMesh *pRM=pCage->GetRenderMesh();
			if (pRM)
			{
				InputLayoutHandle fmt=pRM->GetVertexFormat();
				if (fmt==EDefaultInputLayouts::P3F_C4B_T2F || fmt==EDefaultInputLayouts::P3S_C4B_T2S || fmt==EDefaultInputLayouts::P3S_N4B_C4B_T2S)
				{
					std::vector<Vec2> uvList;
					pRM->LockForThreadAccess();
					const vtx_idx *pIndices=pRM->GetIndexPtr(FSL_READ);
					int32 uvStride;

					const byte *pUVs=pRM->GetUVPtr(uvStride, FSL_READ);
					if (pIndices && pUVs)
					{
						TRenderChunkArray& chunkList=pRM->GetChunks();
						for (int j=0; j<chunkList.size(); j++)
						{
							CRenderChunk &c=chunkList[j];
							if (c.m_nMatID==nSubMatIdx && c.nNumVerts>0)
							{
								for (int idx=c.nFirstIndexId; idx<c.nFirstIndexId+c.nNumIndices; idx++)
								{
									vtx_idx index=pIndices[idx];
									if (fmt==EDefaultInputLayouts::P3F_C4B_T2F)
									{
										const Vec2 *uv=(Vec2*)&pUVs[uvStride*index];
										uvList.push_back(*uv);
									}
									else
									{
										const Vec2f16 *uv16=(Vec2f16*)&pUVs[uvStride*index];
										uvList.push_back(uv16->ToVec2());
									}
								}
							}
						}
					}

					pRM->UnLockForThreadAccess();
					if (uvList.size())
					{
						int width=1024;
						byte *buffer=new byte[width*width];
						memset(buffer, 0, sizeof(byte)*width*width);
						bool bOverlap=false;
						for (int i=0; i<uvList.size(); i+=3)
						{
							if (RasteriseTriangle(&uvList[i], buffer, width, width))
							{
								bOverlap=true;
								break;
							}
						}
						delete[] buffer;
						return !bOverlap;
					}
				}
			}
		}
		return true;
	}

	bool CLodGeneratorInteractionManager::RasteriseTriangle(Vec2 *tri, byte *buffer, const int width, const int height)
	{
		int mx=width,Mx=0,my=width,My=0;
		Vec2 edgeNormal[3];
		for (int i=0; i<3; i++)
		{
			float x=tri[i].x;
			float y=tri[i].y;
			int mxi=(int)floorf(x*width);
			int myi=(int)floorf(y*height);
			if (mxi<mx)
				mx=mxi;
			if (mxi>Mx)
				Mx=mxi;
			if (myi<my)
				my=myi;
			if (myi>My)
				My=myi;
			Vec2 edge=tri[(i+1)%3]-tri[i];
			edgeNormal[i]=Vec2(edge.y, -edge.x);
		}

		if (Mx>=width)
			Mx=width-1;
		if (My>=height)
			My=height-1;
		if (mx<0)
			mx=0;
		if (my<0)
			my=0;

		for (int y=my; y<=My; y++)
		{
			float yf=(y+0.5f)/(float)height;
			for (int x=mx; x<=Mx; x++)
			{
				float xf=(x+0.5f)/(float)width;
				Vec2 point(xf, yf);

				float sign=0.0f;
				bool bHit=true;
				for (int z=0; z<3; z++)
				{
					float dot=edgeNormal[z].Dot(point-tri[z]);
					if (dot==0.0f || sign*dot<0.0f) // To improve. Detect if we're on the left edge when dot==0
					{
						bHit=false;
						break;
					}
					sign=dot;
				}

				if (bHit)
				{
					if (buffer[y*width+x])
						return true;
					buffer[y*width+x]=1;
				}
			}
		}

		return false;
	}

	namespace
	{
		const string PyGetSelectedBrushFilepath()
		{
			return CLodGeneratorInteractionManager::Instance()->GetSelectedBrushFilepath();
		}

		const string PyGetSelectedBrushMaterial()
		{
			return CLodGeneratorInteractionManager::Instance()->GetSelectedBrushMaterial();
		}

		const string PyGetDefaultBrushMaterial(const string& filepath)
		{
			return CLodGeneratorInteractionManager::Instance()->GetDefaultBrushMaterial(filepath);
		}

		bool PyLoadStatObj(const string& filepath)
		{
			return CLodGeneratorInteractionManager::Instance()->LoadStatObj(filepath);
		}

		bool PyLoadMaterial(const string& filepath)
		{
			return CLodGeneratorInteractionManager::Instance()->LoadMaterial(filepath);
		}

		void PySetGeometryOptions(int viewElevations, int viewsAround, float viewResolution)
		{
			CLodGeneratorInteractionManager::Instance()->SetGeometryOption<int>("nViewElevations", viewElevations);
			CLodGeneratorInteractionManager::Instance()->SetGeometryOption<int>("nViewsAround", viewsAround);
			CLodGeneratorInteractionManager::Instance()->SetGeometryOption<float>("fViewResolution", viewResolution);
		}

		void PySetMaterialGenOptions(bool bAddToParentMaterial)
		{
			CLodGeneratorInteractionManager::Instance()->SetGeometryOption<bool>("bAddToParentMaterial", bAddToParentMaterial);
		}

		void PyReloadModel()
		{
			CLodGeneratorInteractionManager::Instance()->ReloadModel();
		}

		const string PyGetLoadedFilename()
		{
			return CLodGeneratorInteractionManager::Instance()->GetLoadedFilename();
		}

		const string PyGetLoadedMaterialFilename()
		{
			return CLodGeneratorInteractionManager::Instance()->GetLoadedMaterialFilename();
		}

		bool PyGenerateLodChain()
		{
			return CLodGeneratorInteractionManager::Instance()->LodGenGenerate();
		}

		float PyLodChainGenerationTick()
		{
			float fProgress = 0.0f;
			if ( CLodGeneratorInteractionManager::Instance()->LodGenGenerateTick(&fProgress) )
				return 1.0f;
			return fProgress;
		}

		void PyCancelLodChainGeneration()
		{
			CLodGeneratorInteractionManager::Instance()->LogGenCancel();
		}

		void PyGenerateGeometryLod(const int nLodId, const float fPercentage)
		{
			CLodGeneratorInteractionManager::Instance()->GenerateLod(nLodId,fPercentage);
		}

		bool PyGenerateMaterialLod(const int nLod, int nWidth, const int nHeight)
		{
			return CLodGeneratorInteractionManager::Instance()->RunProcess(nLod,nWidth,nHeight);
		}

		void PySaveTextures(const int nLod)
		{
			CLodGeneratorInteractionManager::Instance()->SaveTextures(nLod);
		}

		bool PyCreateChangelist()
		{
			return CLodGeneratorInteractionManager::Instance()->CreateChangelist();
		}

		void PySaveSettings()
		{
			CLodGeneratorInteractionManager::Instance()->SaveSettings();
		}

		void PyOpenGeneratorWithParameter(const char* filepath)
		{
			CLodGeneratorInteractionManager::Instance()->SetParameterFilePath(filepath);
			GetIEditor()->ExecuteCommand("general.open_pane 'LOD Generator'");
		}
	}

}
/*
REGISTER_PYTHON_COMMAND(PyOpenGeneratorWithParameter, lodtools, loadcgfintool, 
	"opens up the LOD Generator tool with given path");

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetSelectedBrushFilepath, lodtools, getselected,
	"returns the path to the editor selected brush object",
	"lodtools.getselected()");

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetSelectedBrushMaterial, lodtools, getselectedmaterial,
	"returns the path to the selected brush material",
	"lodtools.getselectedmaterial()");

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetDefaultBrushMaterial, lodtools, getdefaultmaterial,
	"returns the path to the brush's material",
	"lodtools.getselectedmaterial(str cgfpath)");

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyLoadStatObj, lodtools, loadcgf,
	"loads a cgf given the filepath",
	"lodtools.loadcgf(str cgfpath)");

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyLoadMaterial, lodtools, loadmaterial,
	"loads a material given the filepath ",
	"lodtools.loadmaterial(str materialpath)");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyReloadModel, lodtools, reloadmodel,
	"reloads the currently loaded cgf into the lod tool, usefull to reload after generating lods and materials",
	"lodtools.reload()");

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetLoadedFilename, lodtools, getloadedfilename,
	"gets the file name of the currently loaded model",
	"lodtools.getloadedfilename()");

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetLoadedMaterialFilename, lodtools, getloadedmaterialfilename,
	"gets the file name of the currently loaded material",
	"lodtools.getloadedmaterialfilename()");

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGenerateLodChain, lodtools, generatelodchain,
	"generates a lod chain for the currently loaded model",
	"lodtools.generatelodchain()");

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyLodChainGenerationTick, lodtools, generatetick,
	"tick function for the background lod chain generation task should be called every second",
	"lodtools.generatetick()");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyCancelLodChainGeneration, lodtools, generatecancel,
	"cancels the current lod chain generation task",
	"lodtools.generatecancel()");

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGenerateGeometryLod, lodtools, createlod,
	"create a new lod mesh given its id and percentage",
	"lodtools.createlod(int nLodId, float fPercentage)");

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PySetGeometryOptions, lodtools, set_geometry_options,
	"set the parameters for the lod chain generation",
	"lodtools.set_geometry_options(int viewElevations, int viewsAround, float viewResolution)");

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PySetMaterialGenOptions, lodtools, set_material_gen_options,
	"set the parameters for the generating the lods material",
	"lodtools.set_material_gen_options(bool bAddToParentMaterial)");

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGenerateMaterialLod, lodtools, generatematerials,
	"generates the material textures for the given lod",
	"lodtools.generatematerials(int nLod, int nWidth, int nHeight)");

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PySaveTextures, lodtools, savetextures,
	"saves the generated material textures to disk",
	"lodtools.savetextures(int nLod)");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyCreateChangelist, lodtools, createchangelist,
	"creates a new changelist to add any lod generated files too",
	"lodtools.createchangelist()");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySaveSettings, lodtools, savesettings,
	"saves out the lod settings file for future use",
	"lodtools.savesettings()");
*/