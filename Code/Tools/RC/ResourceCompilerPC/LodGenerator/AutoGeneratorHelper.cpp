// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AutoGeneratorHelper.h"
#include "ISourceControl.h"
#include "IEditor.h"
#include "Material/MaterialManager.h"
#include <Cry3DEngine/IIndexedMesh.h>

namespace LODGenerator 
{
	CString CAutoGeneratorHelper::m_changeId = "";

	CAutoGeneratorHelper::CAutoGeneratorHelper()
	{

	}

	CAutoGeneratorHelper::~CAutoGeneratorHelper()
	{

	}

	bool CAutoGeneratorHelper::CheckoutOrExtract(const char * filename)
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
			CString path(filename);
			return CFileUtil::ExtractFile(path,false);
		}
		return true;
	}

	bool CAutoGeneratorHelper::CheckoutInSourceControl(const CString& filename)
	{
		//try from source control
		bool bCheckedOut = false;
		ISourceControl * pSControl = GetIEditor()->GetSourceControl();
		if(pSControl)
		{
			CString path(filename);
			int eFileAttribs = pSControl->GetFileAttributes(path);
			if (!(eFileAttribs & SCC_FILE_ATTRIBUTE_MANAGED) && (eFileAttribs & SCC_FILE_ATTRIBUTE_NORMAL))
			{
				char * changeId = m_changeId.GetBuffer();
				bCheckedOut = pSControl->Add(path, "LOD Tool Generation", ADD_WITHOUT_SUBMIT|ADD_CHANGELIST, changeId);
			}
			else if (!(eFileAttribs & SCC_FILE_ATTRIBUTE_BYANOTHER) && !(eFileAttribs & SCC_FILE_ATTRIBUTE_CHECKEDOUT) && (eFileAttribs & SCC_FILE_ATTRIBUTE_MANAGED))
			{
				if (pSControl->GetLatestVersion(path))
				{
					char * changeId = m_changeId.GetBuffer();
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

	CMaterial * CAutoGeneratorHelper::CreateMaterialFromTemplate(const CString& matName)
	{
		CMaterial * templateMaterial = GetIEditor()->GetMaterialManager()->LoadMaterial("Editor/Materials/lodgen_template.mtl");
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

	void CAutoGeneratorHelper::SetLODMaterialId(IStatObj * pStatObj, int matId)
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

	CString CAutoGeneratorHelper::GetDefaultTextureName(const int i, const int nLod, const bool bAlpha, const CString& exportPath, const CString& fileNameTemplate)
	{
		char *defaultSubName[]={"diff", "ddna", "spec"};

		CString strLodId;
		strLodId.Format("%d",nLod);

		CString outName;
		outName=Path::AddSlash(exportPath+"textures")+fileNameTemplate;
		outName.Replace("[BakedTextureType]", defaultSubName[i]);
		outName.Replace("[LodId]", strLodId);
		outName.Replace("[AlphaBake]", !bAlpha ? "" : "alpha_");
		outName=Path::MakeGamePath(outName, true);
		return outName;
	}


	bool CAutoGeneratorHelper::RasteriseTriangle(Vec2 *tri, byte *buffer, const int width, const int height)
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
}