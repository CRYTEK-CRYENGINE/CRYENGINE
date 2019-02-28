// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CubemapUtils.h"
#include "Objects\EntityObject.h"
#include "Objects\EnvironmentProbeObject.h"
#include "Util\ImageTIF.h"
#include "Controls/QuestionDialog.h"
#include "IEditorImpl.h"
#include "IBackgroundScheduleManager.h"
#include "FilePathUtil.h"
///////////////////////////////////////////////////////////////////////////////////
bool CubemapUtils::GenCubemapWithPathAndSize(string& filename, const int size, const bool dds)
{
	CBaseObject* pObject = GetIEditorImpl()->GetSelectedObject();
	return GenCubemapWithObjectPathAndSize(filename, pObject, size, dds);
}

///////////////////////////////////////////////////////////////////////////////////
bool CubemapUtils::GenCubemapWithObjectPathAndSize(string& filename, CBaseObject* pObject, const int size, const bool dds)
{
	if (!pObject)
	{
		Warning("Select One Entity/Brush to Generate Cubemap");
		return false;
	}

	if (pObject->GetType() != OBJTYPE_ENTITY && pObject->GetType() != OBJTYPE_BRUSH)
	{
		Warning("Only Entities and Brushes are allowed as a selected object. Please Select Entity or Brush objects");
		return false;
	}

	int res = 1;
	// Make size power of 2.
	for (int i = 0; i < 16; i++)
	{
		if (res * 2 > size)
			break;
		res *= 2;
	}
	if (res > 4096)
	{
		Warning("Bad texture resolution.\nMust be power of 2 and less or equal to 4096");
		return false;
	}

	IRenderNode* pRenderNode = pObject->GetEngineNode();
	IEntity* pIEnt = NULL;
	bool bIsHidden = false;
	// Hide object before Cubemap generation.
	if (pRenderNode)
	{
		bIsHidden = (pRenderNode->GetRndFlags() & ERF_HIDDEN) != 0;
		pRenderNode->SetRndFlags(ERF_HIDDEN, true);
	}
	else if (pObject->GetType() == OBJTYPE_ENTITY)
	{
		CEntityObject* pEnt = (CEntityObject*)pObject;
		pIEnt = pEnt->GetIEntity();
		if (pIEnt)
		{
			bIsHidden = pIEnt->IsHidden();
			pIEnt->Hide(true);
		}
	}

	// Items like helpers (CE-15190) and aux. geo. like gizmos (CE-15280) should not be rendered into cubemaps
	bool helpersOn = GetIEditorImpl()->IsHelpersDisplayed();
	if (helpersOn) 
	{
		GetIEditorImpl()->EnableHelpersDisplay(false);
		GetIEditorImpl()->GetObjectManager()->SendEvent(EVENT_HIDE_HELPER);
	} 

	int auxOn = gEnv->pConsole->GetCVar("r_auxGeom")->GetIVal();
	if(auxOn)
		gEnv->pConsole->GetCVar("r_auxGeom")->Set(0);

	string texname = PathUtil::GetFileName(filename);
	string path = PathUtil::GetPathWithoutFilename(filename);

	// Add _CM suffix if missing
	int32 nCMSufixCheck = texname.Find("_cm");
	texname = PathUtil::Make(path, texname + ((nCMSufixCheck == -1) ? "_cm.tif" : ".tif"));

	// Assign this texname to current material.
	texname = PathUtil::ToUnixPath(texname.GetString()).c_str();

	// Temporary solution to save both dds and tiff hdr cubemap
	AABB pObjAABB;
	pObject->GetBoundBox(pObjAABB);

	Vec3 pObjCenter = pObjAABB.GetCenter();

	GenHDRCubemapTiff(texname, res, pObjCenter);//pObject->GetWorldPos());

	// restore object's visibility
	if (pRenderNode)
	{
		pRenderNode->SetRndFlags(ERF_HIDDEN, bIsHidden);
	}
	else if (pIEnt)
	{
		pIEnt->Hide(bIsHidden);
	}
	// restore rendering of helpers and Aux. Geo.
	if (helpersOn) 
	{
		GetIEditorImpl()->EnableHelpersDisplay(true);
		GetIEditorImpl()->GetObjectManager()->SendEvent(EVENT_SHOW_HELPER);
	} 
	if (auxOn)
		gEnv->pConsole->GetCVar("r_auxGeom")->Set(auxOn);

	filename = PathUtil::ToUnixPath(texname.GetString()).c_str();
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CubemapUtils::GenHDRCubemapTiff(const string& fileName, std::size_t nDstSize, Vec3& pos)
{
	const auto nSrcSize = nDstSize * 4; // Render 16x bigger cubemap (4x4) - 16x SSAA
	const auto srcPitch = nSrcSize * 4;
	const auto dstPitch = nSrcSize;
	const auto srcSideSize = nSrcSize * srcPitch;
	const auto sides = 6;

	const auto& vecData = GetIEditorImpl()->GetRenderer()->EF_RenderEnvironmentCubeHDR(nSrcSize, pos);
	CRY_ASSERT_MESSAGE(vecData.size() == srcSideSize * sides, "TIFF data does not match expected size");
	if (vecData.size() < srcSideSize * sides)
		return;

	// todo: such big downsampling should be on gpu

	// save data to tiff
	// resample the image at the original size
	CWordImage img;
	img.Allocate(nSrcSize * sides, nDstSize);
	for (int side = 0; side < sides; ++side)
	{
		for (uint32 y = 0; y < nDstSize; ++y)
		{
			CryHalf4* pSrcSide = (CryHalf4*)&vecData[side * srcSideSize];
			CryHalf4* pDst = (CryHalf4*)&img.ValueAt(side * dstPitch, y);
			for (uint32 x = 0; x < nDstSize; ++x)
			{
				Vec4 cResampledColor(0.f, 0.f, 0.f, 0.f);

				// resample the image at the original size
				for (uint32 yres = 0; yres < 4; ++yres)
				{
					for (uint32 xres = 0; xres < 4; ++xres)
					{
						const CryHalf4& pSrc = pSrcSide[(y * 4 + yres) * nSrcSize + (x * 4 + xres)];
						cResampledColor += Vec4(CryConvertHalfToFloat(pSrc.x), CryConvertHalfToFloat(pSrc.y), CryConvertHalfToFloat(pSrc.z), CryConvertHalfToFloat(pSrc.w));
					}
				}

				cResampledColor /= 16.f;

				// Force alpha to 1.0, render output might contain other values which will only serve to corrupt the TIFF.
				*pDst++ = CryHalf4(cResampledColor.x, cResampledColor.y, cResampledColor.z, 1.0f);
			}
		}
	}

	assert(IsHeapValid());

	CImageTIF tif;
	const bool res = tif.SaveRAW(fileName, img.GetData(), nDstSize * 6, nDstSize, 2, 4, true, "HDRCubemap_highQ");
	assert(res);
}

//function will recurse all probes and generate a cubemap for each
void CubemapUtils::RegenerateAllEnvironmentProbeCubemaps()
{
	std::vector<CBaseObject*> results;
	GetIEditorImpl()->GetObjectManager()->FindObjectsOfType(RUNTIME_CLASS(CEnvironementProbeObject), results);

	IBackgroundSchedule* envmapTexSchedule = GetIEditorImpl()->GetBackgroundScheduleManager()->CreateSchedule("Cubemap Texgen Schedule");
	IBackgroundScheduleItem* scheduleItem = GetIEditorImpl()->GetBackgroundScheduleManager()->CreateScheduleItem("Cubemap Texgen ScheduleItem");

	for (std::vector<CBaseObject*>::iterator end = results.end(), item = results.begin(); item != end; ++item)
	{
		IBackgroundScheduleItemWork* workitem = ((CEnvironementProbeObject*)(*item))->GenerateCubemapTask();
		scheduleItem->AddWorkItem(workitem);
	}

	envmapTexSchedule->AddItem(scheduleItem);
	GetIEditorImpl()->GetBackgroundScheduleManager()->SubmitSchedule(envmapTexSchedule);
}

void CubemapUtils::GenerateCubemaps()
{
	const CSelectionGroup* pSelection = GetIEditorImpl()->GetObjectManager()->GetSelection();
	IBackgroundSchedule* envmapTexSchedule = GetIEditorImpl()->GetBackgroundScheduleManager()->CreateSchedule("Cubemap Texgen Schedule");
	IBackgroundScheduleItem* scheduleItem = GetIEditorImpl()->GetBackgroundScheduleManager()->CreateScheduleItem("Cubemap Texgen ScheduleItem");

	for (size_t i = 0, numObj = pSelection->GetCount(); i < numObj; ++i)
	{
		if (strcmp(pSelection->GetObject(i)->GetClassDesc()->ClassName(), "EnvironmentProbe") == 0)
		{
			IBackgroundScheduleItemWork* workitem = ((CEnvironementProbeObject*)pSelection->GetObject(i))->GenerateCubemapTask();
			scheduleItem->AddWorkItem(workitem);
		}
	}

	envmapTexSchedule->AddItem(scheduleItem);
	GetIEditorImpl()->GetBackgroundScheduleManager()->SubmitSchedule(envmapTexSchedule);
}

