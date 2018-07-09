// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "LoaderCGA.h"

#include "Model.h"
#include "LoaderTCB.h"
#include "AnimEventLoader.h"

#include <CryString/CryPath.h>

#define ANIMATION_EXT  "anm"
#define CONT_EXTENSION (0x10)

//////////////////////////////////////////////////////////////////////////
// Loads animation object.
//////////////////////////////////////////////////////////////////////////
CDefaultSkeleton* CryCGALoader::LoadNewCGA(const char* OriginalGeomName, CharacterManager* pManager, uint32 nLoadingFlags)
{
	CRY_DEFINE_ASSET_SCOPE("CGA", OriginalGeomName);
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_CGA, 0, "%s", OriginalGeomName);

	Reset();
	//return 0;

	uint32 c = 0;
	char geomName[256];
	for (c = 0; c < 256; c++)
	{
		geomName[c] = OriginalGeomName[c];
		if (geomName[c] == 0)
			break;
	}

	if (c > 7)
		if (geomName[c - 8] == '_' && geomName[c - 7] == 'l' && geomName[c - 6] == 'o' && geomName[c - 5] == 'w')
		{
			geomName[c - 8] = '.';
			geomName[c - 7] = 'c';
			geomName[c - 6] = 'g';
			geomName[c - 5] = 'a';
			geomName[c - 4] = 0;
		}

	CLoaderTCB loader;

	loader.SetLoadOldChunks(Console::GetInst().ca_LoadUncompressedChunks > 0);
	CHeaderTCB* pSkinningInfo = loader.LoadTCB(OriginalGeomName, 0);

	if (pSkinningInfo == 0)
	{
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, OriginalGeomName, "Failed to load CGA-Object");
		return 0;
	}

	//CSkinningInfo* pSkinningInfo = pCGF->GetSkinningInfo();
	//if (pSkinningInfo==0)
	//	return 0;

	CDefaultSkeleton* pCGAModel = new CDefaultSkeleton(OriginalGeomName, CGA);
	pCGAModel->m_pAnimationSet = new CAnimationSet(OriginalGeomName);

	//we use the file-path to calculate a unique codeID for every controller
	uint32 nControllerJID = 0x1000;  //CCrc32::Compute(geomName);

	//-----------------------------------------------------------------------------
	//-----------------------------------------------------------------------------
	//-----------------------------------------------------------------------------

	m_DefaultNodeCount = 0;
	InitNodes(pSkinningInfo, pCGAModel, OriginalGeomName, "Default", 1, nControllerJID, nLoadingFlags);
	m_DefaultNodeCount = m_arrNodeAnims.size();
	//	g_pI3DEngine->ReleaseChunkFileContent( pCGF );

	LoadAnimations(geomName, pCGAModel, nControllerJID, nLoadingFlags);

	// the first step is for the root bone
	uint32 nRootCounter = 0;
	uint32 numJoints = uint32(pCGAModel->m_arrModelJoints.size());
	for (uint32 i = 0; i < numJoints; i++)
	{
		pCGAModel->m_poseDefaultData.GetJointsAbsolute()[i] = pCGAModel->m_poseDefaultData.GetJointsRelative()[i];
		int32 p = pCGAModel->m_arrModelJoints[i].m_idxParent;
		if (p < 0)
			nRootCounter++;
		if (p >= 0)
		{
			pCGAModel->m_poseDefaultData.GetJointsAbsolute()[i] = pCGAModel->m_poseDefaultData.GetJointsAbsolute()[p] * pCGAModel->m_poseDefaultData.GetJointsAbsolute()[i];
		}
	}

	//if (nRootCounter!=1)
	//	CryFatalError("CryAnimation error: multiple root CHR: %s",OriginalGeomName);
	if (nRootCounter != 1)
	{
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, OriginalGeomName, "Failed to load CGA-Object because it has multiple roots");
		delete pCGAModel;
		return 0;
	}

	pCGAModel->RebuildJointLookupCaches();

	//
	m_CtrlVec3.clear();
	m_CtrlQuat.clear();
	//
	return pCGAModel;
}

//////////////////////////////////////////////////////////////////////////
void CryCGALoader::LoadAnimations(const char* cgaFile, CDefaultSkeleton* pCGAModel, uint32 unique_model_id, uint32 nLoadingFlags)
{
	LOADING_TIME_PROFILE_SECTION;

	// Load all filename_***.anm files.
	char filter[_MAX_PATH];
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	char fname[_MAX_FNAME];
	char ext[_MAX_EXT];

	CryStringUtils::portable_splitpath(cgaFile, drive, dir, fname, ext);

	CryStringUtils::portable_makepath(filter, drive, dir, fname, ".chrparams");
	ICryPak* pack = gEnv->pCryPak;
	if (pack->IsFileExist(filter))
	{
		pCGAModel->LoadCHRPARAMS(filter);
	}
	else
	{
		cry_strcat(fname, "_*");
		CryStringUtils::portable_makepath(filter, drive, dir, fname, ".*");

		char fullpath[_MAX_PATH];
		string filename;
		CryStringUtils::portable_makepath(fullpath, drive, dir, NULL, NULL);

		// Search files that match filter specification.
		_finddata_t fd;
		int res;
		intptr_t handle;
		if ((handle = pack->FindFirst(filter, &fd)) != -1)
			if (handle != -1)
			{
				do
				{
					filename = fullpath;
					filename.append(fd.name);
					filename.MakeLower();

					const char* szFileExt = PathUtil::GetExt(filename.c_str());
					if (strcmp(szFileExt, "anm") == 0)
					{
						// ModelAnimationHeader file found, load it.
						LoadAnimationANM(filename.c_str(), pCGAModel, unique_model_id, nLoadingFlags);
					}
					else if (strcmp(szFileExt, "caf") == 0)
					{
						int prefixLen = strlen(fname) - 1;
						stack_string animName = fd.name;
						animName.erase(0, prefixLen);
						animName.erase(animName.length() - 4);
						animName.MakeLower();

						// ModelAnimationHeader file found, load it.
						pCGAModel->m_pAnimationSet->LoadFileCAF(filename.c_str(), animName.c_str());
					}

					res = pack->FindNext(handle, &fd);

					SLICE_AND_SLEEP();
				}
				while (res >= 0);

				pack->FindClose(handle);
			}

		pCGAModel->SetModelAnimEventDatabase(PathUtil::ReplaceExtension(cgaFile, "animevents"));
	}

	if (pack->IsFileExist(pCGAModel->GetModelAnimEventDatabaseCStr()))
	{
		AnimEventLoader::LoadAnimationEventDatabase(pCGAModel, pCGAModel->GetModelAnimEventDatabaseCStr());
	}
}

//////////////////////////////////////////////////////////////////////////
bool CryCGALoader::LoadAnimationANM(const char* animFile, CDefaultSkeleton* pCGAModel, uint32 unique_model_id, uint32 nLoadingFlags)
{
	// Get file name, this is a name of application.
	assert(strlen(animFile) < _MAX_PATH);
	char fname[_MAX_PATH];
	cry_strcpy(fname, animFile);
	PathUtil::RemoveExtension(fname);
	const char* sAnimName = PathUtil::GetFile(fname);

	const char* sName = strchr(sAnimName, '_');
	if (sName)
		sName += 1;
	else
		sName = sAnimName;

	//------------------------------------------------------------------------------

	CLoaderTCB loader;

	CHeaderTCB* pSkinningInfo = loader.LoadTCB(animFile, 0);
	;                                                             //g_pI3DEngine->LoadChunkFileContent( animFile );
	if (pSkinningInfo == 0)
	{
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, animFile, "Failed to load ANM-file: %s", animFile);
		return 0;
	}

	InitNodes(pSkinningInfo, pCGAModel, animFile, sName, 0, unique_model_id, nLoadingFlags);

	return true;
}

//////////////////////////////////////////////////////////////////////////
namespace CryCGALoaderHelper
{

static int GetLodIndex(const char* const pName)
{
	static const size_t lodNamePrefixLength = strlen(CGF_NODE_NAME_LOD_PREFIX);
	if (strnicmp(pName, CGF_NODE_NAME_LOD_PREFIX, lodNamePrefixLength))
	{
		return -1;
	}

	int value = 0;
	const char* p = pName + lodNamePrefixLength;
	while ((p[0] >= '0') && (p[0] <= '9'))
	{
		value = value * 10 + (p[0] - '0');
		++p;
	}

	return value;
}

} // namespace CryCGALoaderHelper

//////////////////////////////////////////////////////////////////////////
void CryCGALoader::InitNodes(CHeaderTCB* pSkinningInfo, CDefaultSkeleton* pCGAModel, const char* animFile, const string& strAnimationName, bool bMakeNodes, uint32 unique_model_id, uint32 nLoadingFlags)
{

	//-------------------------------------------------------------------------
	//----------        copy animation timing-values            ---------------
	//-------------------------------------------------------------------------
	m_arrNodeAnims.clear();
	m_start = pSkinningInfo->m_nStart;
	m_end = pSkinningInfo->m_nEnd;

	m_ModelAnimationHeader.SetAnimName(strAnimationName);

	//-------------------------------------------------------------------------
	//----------             copy animation tracks              ---------------
	//-------------------------------------------------------------------------
	m_CtrlVec3.clear();
	uint32 numTracksVec3 = pSkinningInfo->m_TrackVec3QQQ.size();
	m_CtrlVec3.reserve(numTracksVec3);
	for (uint32 t = 0; t < numTracksVec3; t++)
	{
		CControllerTCBVec3 Track;
		uint32 nkeys = pSkinningInfo->m_TrackVec3QQQ[t]->size();
		Track.resize(nkeys);
		for (uint32 i = 0; i < nkeys; i++)
		{
			const auto& inkey = (*pSkinningInfo->m_TrackVec3QQQ[t])[i];
			auto& outkey = Track.key(i);

			outkey.flags = 0;
			outkey.time = (f32)inkey.time / TICKS_CONVERT;
			outkey.value = inkey.val;
			outkey.tens = inkey.t;
			outkey.cont = inkey.c;
			outkey.bias = inkey.b;
			outkey.easefrom = inkey.eout;
			outkey.easeto = inkey.ein;
		}

		if (pSkinningInfo->m_TrackVec3FlagsQQQ[t].f0)
			Track.ORT(spline::TCBSpline<Vec3>::ORT_CYCLE);
		else if (pSkinningInfo->m_TrackVec3FlagsQQQ[t].f1)
			Track.ORT(spline::TCBSpline<Vec3>::ORT_LOOP);
		Track.update(); // Precompute spline tangents.
		m_CtrlVec3.push_back(Track);
	}

	m_CtrlQuat.clear();
	uint32 numTracksQuat = pSkinningInfo->m_TrackQuat.size();

	m_CtrlQuat.reserve(numTracksQuat);
	for (uint32 t = 0; t < numTracksQuat; t++)
	{
		spline::TCBAngleAxisSpline Track;
		uint32 nkeys = pSkinningInfo->m_TrackQuat[t]->size();
		Track.resize(nkeys);
		Quat rotLast(IDENTITY);
		for (uint32 i = 0; i < nkeys; i++)
		{
			auto const& inkey = (*pSkinningInfo->m_TrackQuat[t])[i];
			auto& outkey = Track.key(i);

			outkey.flags = 0;
			outkey.time = (float)inkey.time / TICKS_CONVERT;    // * secsPerTick;
			outkey.angle = inkey.val.w;
			outkey.axis = inkey.val.v;
			Quat rotRel = Quat::CreateRotationAA(inkey.val.w, inkey.val.v);
			rotLast = rotLast * rotRel;
			rotLast.Normalize();
			outkey.value = rotLast;
			outkey.tens = inkey.t;
			outkey.cont = inkey.c;
			outkey.bias = inkey.b;
			outkey.easefrom = inkey.eout;
			outkey.easeto = inkey.ein;
		}

		if (pSkinningInfo->m_TrackQuatFlagsQQQ[t].f0)
			Track.ORT(spline::TCBAngleAxisSpline::ORT_CYCLE);
		else if (pSkinningInfo->m_TrackQuatFlagsQQQ[t].f1)
			Track.ORT(spline::TCBAngleAxisSpline::ORT_LOOP);
		Track.update(); // Precompute spline tangents.
		m_CtrlQuat.push_back(Track);
	}

	m_arrControllers = pSkinningInfo->m_arrControllersTCB;

#ifdef _DEBUG
	uint32 numController = m_arrControllers.size();
#endif

	//-------------------------------------------------------------------------
	//-------------------------------------------------------------------------
	//-------------------------------------------------------------------------

	CContentCGF* pCGF = g_pI3DEngine->CreateChunkfileContent(animFile);
	bool bLoaded = g_pI3DEngine->LoadChunkFileContent(pCGF, animFile);
	if (!bLoaded)
	{
		//error
		return;
	}

	uint32 numChunks2 = pCGF->GetSkinningInfo()->m_numChunks;
	m_arrChunkNodes.resize(numChunks2 * CONT_EXTENSION);
	for (uint32 i = 0; i < numChunks2 * CONT_EXTENSION; i++)
		m_arrChunkNodes[i].active = 0;

	IStatObj* pRootStaticObj = g_pI3DEngine->LoadStatObj(pCGAModel->GetModelFilePath(), NULL, NULL, true, nLoadingFlags);

	pCGAModel->m_pCGA_Object = pRootStaticObj;

	uint32 numNodes2 = pCGF->GetNodeCount();
	assert(numNodes2);

	uint32 MeshNodeCounter = 0;
	for (uint32 n = 0; n < numNodes2; n++)
	{
		if (pCGF->GetNode(n)->type == CNodeCGF::NODE_MESH)
			MeshNodeCounter += (pCGF->GetNode(n) != 0);
	}

	DynArray<QuatT> jointsRelative;
	if (uint32 jointCount = pCGAModel->m_poseDefaultData.GetJointCount())
	{
		jointsRelative.resize(jointCount);
		for (uint32 i = 0; i < jointCount; ++i)
			jointsRelative[i] = pCGAModel->m_poseDefaultData.GetJointsRelative()[i];
	}

	CNodeCGF* pGFXNode2 = 0;
	//uint32 nodecounter=0;
	for (uint32 n = 0; n < numNodes2; n++)
	{
		uint32 MeshNode = pCGF->GetNode(n)->type == CNodeCGF::NODE_MESH;
		uint32 HelperNode = pCGF->GetNode(n)->type == CNodeCGF::NODE_HELPER;
		if (MeshNode || HelperNode)
		{
			pGFXNode2 = pCGF->GetNode(n);
			assert(pGFXNode2);

			// Try to create object.
			//			IStatObj::SSubObject *pSubObject = NULL;

			NodeDesc nd;
			nd.active = 1;
			nd.parentID = pGFXNode2->nParentChunkId;
			nd.pos_cont_id = pGFXNode2->pos_cont_id;
			nd.rot_cont_id = pGFXNode2->rot_cont_id;
			nd.scl_cont_id = pGFXNode2->scl_cont_id;

			int numChunks = (int)m_arrChunkNodes.size();

			if (nd.pos_cont_id != 0xffff)
				assert(nd.pos_cont_id < numChunks);
			if (nd.rot_cont_id != 0xffff)
				assert(nd.rot_cont_id < numChunks);
			if (nd.scl_cont_id != 0xffff)
				assert(nd.scl_cont_id < numChunks);

			assert(pGFXNode2->nChunkId < (int)numChunks);

			pCGAModel->m_ModelAABB = AABB(Vec3(-2, -2, -2), Vec3(+2, +2, +2));

			if (bMakeNodes)
			{
				// FindSubObject will only cut the characters before " ".
				// If there are any space in a joint name, it will fail. Use FindSubObject_StrStr instead.
				IStatObj::SSubObject* pSubObj = pRootStaticObj->FindSubObject_CGA(pGFXNode2->name);

				if (pSubObj == 0 && MeshNodeCounter != 1)
					continue;

				IStatObj* pStaticObj = 0;
				if (MeshNodeCounter == 1 && MeshNode)
					pStaticObj = pRootStaticObj;
				else if (pSubObj)
					pStaticObj = pSubObj->pStatObj;
				else
					continue;

				nd.node_idx = pCGAModel->m_arrModelJoints.size();

				uint16 ParentID = 0xffff;
				if (pGFXNode2->nParentChunkId != 0xffffffff)
				{
					assert(pGFXNode2->nParentChunkId < (int)numChunks);
					uint32 numJoints = pCGAModel->m_arrModelJoints.size();
					for (uint32 i = 0; i < numJoints; i++)
						if (pGFXNode2->nParentChunkId == pCGAModel->m_arrModelJoints[i].m_ObjectID)
							ParentID = i;
				}

				CDefaultSkeleton::SJoint mj;
				mj.m_strJointName = pGFXNode2->name;
				mj.m_nJointCRC32Lower = CCrc32::ComputeLowercase(pGFXNode2->name);
				mj.m_nJointCRC32 = CCrc32::Compute(pGFXNode2->name);
				mj.m_ObjectID = pGFXNode2->nChunkId;          //NOTE:: this is a place-holder to store the chunk-id
				mj.m_idxParent = ParentID;
				mj.m_CGAObject = pStaticObj;
				mj.m_NodeID = nd.node_idx;
				jointsRelative.push_back(QuatT(CryQuat(pGFXNode2->localTM), pGFXNode2->localTM.GetTranslation()));
				pCGAModel->m_arrModelJoints.push_back(mj);

				m_arrChunkNodes[pGFXNode2->nChunkId] = nd;
				//		assert(nodecounter==n);
				//		nodecounter++;
				//	g_nControllerJID++;
			}
			else
			{
				uint32 numJoints = pCGAModel->m_arrModelJoints.size();
				for (uint32 i = 0; i < numJoints; i++)
				{
					if (stricmp(pCGAModel->m_arrModelJoints[i].m_strJointName.c_str(), pGFXNode2->name) == 0)
					{
						nd.node_idx = i;
						break;
					}
				}
				m_arrChunkNodes[pGFXNode2->nChunkId] = nd;
			}
		}
	}

	if (uint32 jointCount = uint32(jointsRelative.size()))
	{
		if (pCGAModel->m_poseDefaultData.Initialize(jointCount))
		{
			for (uint32 i = 0; i < jointCount; ++i)
			{
				pCGAModel->m_poseDefaultData.SetJointRelative(i, jointsRelative[i]);
			}
		}
	}

	//------------------------------------------------------------------------
	//---    init nodes                                                    ---
	//------------------------------------------------------------------------
	uint32 numControllers0 = m_CtrlVec3.size();
	uint32 numControllers1 = m_CtrlQuat.size();

	uint32 numAktiveNodes = 0;
	uint32 numNodes = m_arrChunkNodes.size();
	for (uint32 i = 0; i < numNodes; i++)
		numAktiveNodes += m_arrChunkNodes[i].active;

	m_arrNodeAnims.clear();
	m_arrNodeAnims.resize(numAktiveNodes);

	if (numAktiveNodes < m_DefaultNodeCount)
		numAktiveNodes = m_DefaultNodeCount;

	m_arrNodeAnims.resize(numAktiveNodes);

	for (uint32 i = 0; i < numNodes; i++)
	{
		NodeDesc nd = m_arrChunkNodes[i];

		if (nd.active == 0)
			continue;

		if (nd.node_idx == 0xffff)
			continue;

		uint32 numAnims = m_arrNodeAnims.size();

		uint32 id = pCGAModel->m_arrModelJoints[nd.node_idx].m_NodeID;
		if (id >= numAnims)
			continue;

		m_arrNodeAnims[id].m_active = 0;

		// find controllers.
		if (nd.pos_cont_id != 0xffff)
		{
			CControllerType pTCB = m_arrControllers[nd.pos_cont_id];
			if (pTCB.m_controllertype == POSCHANNEL)
			{
				m_arrNodeAnims[id].m_active |= eJS_Position;
				m_arrNodeAnims[id].m_posTrack = m_CtrlVec3[pTCB.m_index];
			}
		}

		if (nd.rot_cont_id != 0xffff)
		{
			CControllerType pTCB = m_arrControllers[nd.rot_cont_id];
			if (pTCB.m_controllertype == ROTCHANNEL)
			{
				m_arrNodeAnims[id].m_active |= eJS_Orientation;
				m_arrNodeAnims[id].m_rotTrack = m_CtrlQuat[pTCB.m_index];
			}
		}

		if (nd.scl_cont_id != 0xffff)
		{
			CControllerType pTCB = m_arrControllers[nd.scl_cont_id];
			if (pTCB.m_controllertype == POSCHANNEL)
			{
				m_arrNodeAnims[id].m_active |= eJS_Scale;
				m_arrNodeAnims[id].m_sclTrack = m_CtrlVec3[pTCB.m_index];
			}
		}
	}

	//-------------------------------------------------------------------------

	if (!m_CtrlVec3.empty() || !m_CtrlQuat.empty())
		LoadANM(pCGAModel, animFile, strAnimationName, m_arrNodeAnims);

	uint32 numJoints2 = pCGAModel->m_arrModelJoints.size();
	for (uint32 a = 0; a < numJoints2; a++)
		pCGAModel->m_arrModelJoints[a].m_numChildren = 0;
	for (uint32 a = 0; a < numJoints2; a++)
	{
		for (uint32 b = 0; b < numJoints2; b++)
		{
			if (a == pCGAModel->m_arrModelJoints[b].m_idxParent)
				pCGAModel->m_arrModelJoints[a].m_numChildren++;
		}
	}
	g_pI3DEngine->ReleaseChunkfileContent(pCGF);
}

// loads the animations from the array: pre-allocates the necessary controller arrays
// the 0th animation is the default animation
uint32 CryCGALoader::LoadANM(CDefaultSkeleton* pDefaultSkeleton, const char* pFilePath, const char* pAnimName, DynArray<CControllerTCB>& m_LoadCurrAnimation)
{
	uint32 nAnimID = 0;
	pDefaultSkeleton->m_pAnimationSet->prepareLoadANMs(1);
	int32 rel = pDefaultSkeleton->m_pAnimationSet->LoadFileANM(pFilePath, pAnimName, m_LoadCurrAnimation, this, pDefaultSkeleton);
	if (rel >= 0)
		nAnimID++;
	else
		gEnv->pLog->LogError("Animation could not be read for Model: %s", pDefaultSkeleton->GetModelFilePath());

	return nAnimID;
}

//////////////////////////////////////////////////////////////////////////
void CryCGALoader::Reset()
{
	m_CtrlVec3.resize(0);
	m_CtrlQuat.resize(0);

	m_arrControllers.resize(0);
	m_arrChunkNodes.resize(0);

	m_arrNodeAnims.resize(0);
}
