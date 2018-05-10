// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AutoGeneratorTextureCreator.h"
#include "AutoGeneratorHelper.h"
#include "AutoGeneratorDataBase.h"
#include "Material/Material.h"
#include "IEditor.h"
#include "Material/MaterialManager.h"
#include <CryRenderer/IMeshBaking.h>
#include <CryRenderer/ITexture.h>
#include "Util/Image.h"
#include "Util/ImageTIF.h"

#define MAX_MAT_SLOTS 127

namespace LODGenerator 
{
	CAutoGeneratorTextureCreator::CAutoGeneratorTextureCreator()
	{
		m_iWidth = 512;
		m_iHeight = 512;
	}

	CAutoGeneratorTextureCreator::~CAutoGeneratorTextureCreator()
	{

	}

	void CAutoGeneratorTextureCreator::SetWidth(int iWidth)
	{
		m_iWidth = iWidth;
	}

	void CAutoGeneratorTextureCreator::SetHeight(int iHeight)
	{
		m_iHeight = iHeight;
	}

	bool CAutoGeneratorTextureCreator::Generate(int level)
	{
		if (!GenerateTexture(level))
			return false;
		//if (!SaveTextures(level))
		//	return false;
		return true;
	}

	bool CAutoGeneratorTextureCreator::GenerateTexture(int level)
	{
		CAutoGeneratorDataBase* pAutoGeneratorDataBase = CAutoGeneratorDataBase::Instance();
		CAutoGeneratorParams& autoGeneratorParams = CAutoGeneratorDataBase::Instance()->GetParams();

		SMeshBakingMaterialParams params[MAX_MAT_SLOTS];
		CMaterial* pMat = pAutoGeneratorDataBase->GetLoadedMaterial();
		if (!pMat)
			return false;

		const int nSubMatId = pAutoGeneratorDataBase->GetSubMatId(level);
		const int nSourceLod = autoGeneratorParams.GetGeometryOption<int>("nSourceLod");
		const float fRayLength = autoGeneratorParams.GetMaterialOption<float>("RayTestLength");
		const float fRayIndent = autoGeneratorParams.GetMaterialOption<float>("RayStartDist");
		const bool bAlpha = autoGeneratorParams.GetMaterialOption<bool>("BakeAlpha");
		const bool bSmoothCage = autoGeneratorParams.GetMaterialOption<bool>("SmoothCage");
		// visn test
		const bool bDilationPass =  false;//autoGeneratorParams.GetMaterialOption<bool>("DilationPass");
		const int nBackgroundColour = autoGeneratorParams.GetMaterialOption<int>("BackgroundColour");
		const bool bSaveSpec = autoGeneratorParams.GetMaterialOption<bool>("BakeUniqueSpecularMap");
		const bool bUseAutoTextureSize = autoGeneratorParams.GetMaterialOption<bool>("UseAutoTextureSize");

		int nTWidth = m_iWidth;
		int nTHeight = m_iHeight;	
		if(bUseAutoTextureSize)
		{
			CryLog("Using AutoTextureSize...");
			// Fill out the list with the size options
			std::vector< std::pair<float,int> > textureSizeList;
			const float autoRadius1 = autoGeneratorParams.GetMaterialOption<float>("AutoTextureRadius1");
			const int autoSize1 = autoGeneratorParams.GetMaterialOption<int>("AutoTextureSize1");
			textureSizeList.push_back(std::pair<float,int>(autoRadius1,autoSize1));

			const float autoRadius2 = autoGeneratorParams.GetMaterialOption<float>("AutoTextureRadius2");
			const int autoSize2 = autoGeneratorParams.GetMaterialOption<int>("AutoTextureSize2");
			textureSizeList.push_back(std::pair<float,int>(autoRadius2,autoSize2));

			const float autoRadius3 = autoGeneratorParams.GetMaterialOption<float>("AutoTextureRadius3");
			const int autoSize3 = autoGeneratorParams.GetMaterialOption<int>("AutoTextureSize3");
			textureSizeList.push_back(std::pair<float,int>(autoRadius3,autoSize3));

			std::sort(textureSizeList.begin(),textureSizeList.end(),std::greater< std::pair<float,int> >());

			// Find the best setting to use on this model
			int textureSize = m_iWidth; // Width and height should always be the same! There's only one size option in the UI.
			IStatObj *mesh = pAutoGeneratorDataBase->GetLoadedModel(level);
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
				if( CryMessageBox("Detected overlapping UVs in target mesh. This could cause artifacts. Continue anyway?", "UV Layout",MB_YESNO|MB_ICONQUESTION ) == IDNO)
					return false;
			}
		}

		SMeshBakingInputParams inParams;
		inParams.pInputMesh = pAutoGeneratorDataBase->GetLoadedModel(nSourceLod);
		inParams.pInputCharacter=NULL;
		inParams.pCageMesh = pAutoGeneratorDataBase->GetLoadedModel(level);
		inParams.pCageCharacter=NULL;
		inParams.outputTextureWidth=2*nTWidth; // Bake out at twice resolution, we'll half it during conversion so it'll match the selected resolution
		inParams.outputTextureHeight=2*nTHeight;
		inParams.pMaterialParams=params;
		inParams.numMaterialParams=submatCount;
		inParams.nLodId = level;
		inParams.bDoDilationPass=bDilationPass;
		inParams.bSaveSpecular = bSaveSpec;
		inParams.dilateMagicColour=ColorF(-16.0f, -16.0f, -16.0f, -16.0f); // Use the fact it's a floating point target to use a colour that'll never occur
		inParams.defaultBackgroundColour=ColorF(GetRValue(nBackgroundColour)/255.0f, GetGValue(nBackgroundColour)/255.0f, GetBValue(nBackgroundColour)/255.0f, 1.0f);
		inParams.bSmoothNormals=bSmoothCage;
		inParams.pMaterial=pMat->GetMatInfo();

		inParams.pInputMesh->Invalidate();

		SMeshBakingOutput pReturnValues;
		bool ret = gEnv->pRenderer->BakeMesh(&inParams, &pReturnValues);
		if ( ret )
		{
			pAutoGeneratorDataBase->GetBakeTextureResult().insert(std::pair<int,SMeshBakingOutput>(level,pReturnValues));
		}
		return ret;
	}

	bool CAutoGeneratorTextureCreator::SaveTextures(const int level)
	{
		CAutoGeneratorDataBase* pAutoGeneratorDataBase = CAutoGeneratorDataBase::Instance();
		std::map<int,SMeshBakingOutput>&  resultsMap = pAutoGeneratorDataBase->GetBakeTextureResult();
		CAutoGeneratorParams& autoGeneratorParams = CAutoGeneratorDataBase::Instance()->GetParams();

		if ( resultsMap.find(level) == resultsMap.end())
			return false;

		CString exportPath = autoGeneratorParams.GetMaterialOption<CString>("ExportPath");
		bool bBakeAlpha = autoGeneratorParams.GetMaterialOption<bool>("BakeAlpha");
		CString filenamePattern = autoGeneratorParams.GetMaterialOption<CString>("FilenameTemplate");
		bool bSaveSpec = autoGeneratorParams.GetMaterialOption<bool>("BakeUniqueSpecularMap");

		for (int i=CAutoGeneratorDataBase::eTextureType_Diffuse; i<CAutoGeneratorDataBase::eTextureType_Max; i++)
		{
			if ( i == CAutoGeneratorDataBase::eTextureType_Spec && !bSaveSpec )
				continue;

			CString cPath = CAutoGeneratorHelper::GetDefaultTextureName(i,level,bBakeAlpha,exportPath,filenamePattern);
			cPath = Path::GamePathToFullPath(cPath, true);
			CFileUtil::CreatePath(Path::GetPath(cPath));
			bool checkedOut = CAutoGeneratorHelper::CheckoutOrExtract(cPath);
			CString preset = GetPreset(i);
			bool bSaveAlpha = (i==CAutoGeneratorDataBase::eTextureType_Diffuse && bBakeAlpha) || (i==CAutoGeneratorDataBase::eTextureType_Normal);
			SaveTexture(resultsMap[level].ppOuputTexture[i], bSaveAlpha, cPath, preset);
			if (!checkedOut)
				CAutoGeneratorHelper::CheckoutOrExtract(cPath);
		}
		AssignToMaterial(level,bBakeAlpha,bSaveSpec,exportPath,filenamePattern);
		pAutoGeneratorDataBase->ReloadModel();

		return true;
	}

	CString CAutoGeneratorTextureCreator::GetPreset(const int texturetype)
	{
		CAutoGeneratorParams& autoGeneratorParams = CAutoGeneratorDataBase::Instance()->GetParams();

		CString preset("");
		switch(texturetype)
		{
		case CAutoGeneratorDataBase::eTextureType_Diffuse: preset = autoGeneratorParams.GetMaterialOption<CString>("DiffuseTexturePreset");
			break;
		case CAutoGeneratorDataBase::eTextureType_Normal: preset = autoGeneratorParams.GetMaterialOption<CString>("NormalTexturePreset");
			break;
		case CAutoGeneratorDataBase::eTextureType_Spec: preset = autoGeneratorParams.GetMaterialOption<CString>("SpecularTexturePreset");
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


	void CAutoGeneratorTextureCreator::AssignToMaterial(const int nLod, const bool bAlpha, const bool bSaveSpec, const CString& exportPath, const CString& fileNameTemplate)
	{
		CAutoGeneratorDataBase* pAutoGeneratorDataBase = CAutoGeneratorDataBase::Instance();
		CAutoGeneratorParams& autoGeneratorParams = CAutoGeneratorDataBase::Instance()->GetParams();

		CMaterial* pMtl = pAutoGeneratorDataBase->GetLODMaterial();
		if(pMtl)
		{
			bool checkedOut = CAutoGeneratorHelper::CheckoutOrExtract(pMtl->GetFilename());
			if (!checkedOut)
				return;

			// Generate base name
			CString relFile;
			CString lodId;
			lodId.Format("_lod%d.", nLod);
			relFile=pAutoGeneratorDataBase->GetLoadedModel()->GetFilePath();
			relFile.Replace(".", lodId);
			CString submatName(Path::GetFileName(relFile));

			//	const int nSubMatId = FindExistingLODMaterial(submatName);
			const int nSubMatId = pAutoGeneratorDataBase->GetSubMatId(nLod);
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

			CMaterial * pTemplateMaterial = GetIEditor()->GetMaterialManager()->LoadMaterial("Editor/Materials/lodgen_template.mtl");
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
				pSubMat = CAutoGeneratorHelper::CreateMaterialFromTemplate(submatName);

				if ( nSubMatId >= subMaterialCount )
					pMtl->SetSubMaterialCount(nSubMatId+1);
				pMtl->SetSubMaterial(nSubMatId,pSubMat);
			}

			if (pSubMat)
			{
				SInputShaderResources &sr=pSubMat->GetShaderResources();
				sr.m_Textures[EFTT_DIFFUSE].m_Name = CAutoGeneratorHelper::GetDefaultTextureName(0, nLod, bAlpha, exportPath, fileNameTemplate);
				sr.m_Textures[EFTT_NORMALS].m_Name = CAutoGeneratorHelper::GetDefaultTextureName(1, nLod, bAlpha, exportPath, fileNameTemplate);
				if ( bSaveSpec)
					sr.m_Textures[EFTT_SPECULAR].m_Name = CAutoGeneratorHelper::GetDefaultTextureName(2, nLod, bAlpha, exportPath, fileNameTemplate);
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
				CryMessageBox("The material file cannot be saved. The file is located in a PAK archive or access is denied", "Material File Save Failed", MB_OK | MB_APPLMODAL | MB_ICONWARNING);
			}

			pMtl->Reload();
		}
	}

	bool CAutoGeneratorTextureCreator::SaveTexture(ITexture* pTex,const bool bAlpha,const CString& fileName,const CString& texturePreset)
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


	bool CAutoGeneratorTextureCreator::VerifyNoOverlap(const int nSubMatIdx)
	{
		CAutoGeneratorDataBase* pAutoGeneratorDataBase = CAutoGeneratorDataBase::Instance();

		IStatObj *pCage = pAutoGeneratorDataBase->GetLoadedModel();
		if (pCage)
		{
			IRenderMesh *pRM = pCage->GetRenderMesh();
			if (pRM)
			{
				EVertexFormat fmt = pRM->GetVertexFormat();
				if (fmt==eVF_P3F_C4B_T2F || fmt==eVF_P3S_C4B_T2S || fmt==eVF_P3S_N4B_C4B_T2S)
				{
					std::vector<Vec2> uvList;
					pRM->LockForThreadAccess();
					const vtx_idx *pIndices = pRM->GetIndexPtr(FSL_READ);
					int32 uvStride;

					const byte *pUVs = pRM->GetUVPtr(uvStride, FSL_READ);
					if (pIndices && pUVs)
					{
						TRenderChunkArray& chunkList=pRM->GetChunks();
						for (int j=0; j<chunkList.size(); j++)
						{
							CRenderChunk &c = chunkList[j];
							if (c.m_nMatID == nSubMatIdx && c.nNumVerts>0)
							{
								for (int idx = c.nFirstIndexId; idx<c.nFirstIndexId+c.nNumIndices; idx++)
								{
									vtx_idx index = pIndices[idx];
									if (fmt == eVF_P3F_C4B_T2F)
									{
										const Vec2 *uv = (Vec2*)&pUVs[uvStride*index];
										uvList.push_back(*uv);
									}
									else
									{
										const Vec2f16 *uv16 = (Vec2f16*)&pUVs[uvStride*index];
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
						byte *buffer = new byte[width*width];
						memset(buffer, 0, sizeof(byte)*width*width);
						bool bOverlap=false;
						for (int i=0; i<uvList.size(); i+=3)
						{
							if (CAutoGeneratorHelper::RasteriseTriangle(&uvList[i], buffer, width, width))
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
}