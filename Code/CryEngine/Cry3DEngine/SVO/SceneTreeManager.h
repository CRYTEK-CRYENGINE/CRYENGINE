// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SCENETREEMANAGER_H__
#define __SCENETREEMANAGER_H__

#if defined(FEATURE_SVO_GI)

class CSvoManager : public Cry3DEngineBase
{
public:
	static void  Update(const SRenderingPassInfo& passInfo, CCamera& newCam);
	static void  UpdateSubSystems(const CCamera& _newCam, CCamera& newCam);
	static void  OnFrameStart(const SRenderingPassInfo& passInfo);
	static void  OnFrameComplete();
	static void  Render(bool bSyncUpdate);
	static char* GetStatusString(int nLine);
	static void  OnDisplayInfo(float& textPosX, float& textPosY, float& textStepY, float textScale);
	static bool  GetSvoStaticTextures(I3DEngine::SSvoStaticTexInfo& svoInfo, PodArray<I3DEngine::SLightTI>* pLightsTI_S, PodArray<I3DEngine::SLightTI>* pLightsTI_D);
	static void  GetSvoBricksForUpdate(PodArray<I3DEngine::SSvoNodeInfo>& arrNodeInfo, float nodeSize, PodArray<SVF_P3F_C4B_T2F>* pVertsOut);
	static int   ExportSvo(ICryArchive* pArchive);
	static void  RegisterMovement(const AABB& objBox);
	static void  CheckAllocateGlobalCloud();
	static void  Release();
};

#endif

#endif
