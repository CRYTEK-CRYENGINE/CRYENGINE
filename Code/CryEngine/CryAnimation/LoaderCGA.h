// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ModelAnimationSet.h"

struct CHeaderTCB;
class CChunkFileReader;

typedef spline::TCBSpline<Vec3> CControllerTCBVec3;

// Internal description of node.
struct NodeDesc
{
	NodeDesc()
	{
		active = 0;
		node_idx = 0xffff;    //index of joint
		parentID = 0xffff;    //if of parent-chunk
		pos_cont_id = 0xffff; // position controller chunk id
		rot_cont_id = 0xffff; // rotation controller chunk id
		scl_cont_id = 0xffff; // scale		controller chunk id
	};

	uint16 active;
	uint16 node_idx;    //index of joint
	uint16 parentID;
	uint16 pos_cont_id; // position controller chunk id
	uint16 rot_cont_id; // rotation controller chunk id
	uint16 scl_cont_id; // scale		controller chunk id
};

//////////////////////////////////////////////////////////////////////////
// Loads AnimObject from CGF/CAF files.
//////////////////////////////////////////////////////////////////////////
class CryCGALoader
{
public:

	CryCGALoader() : m_start(0), m_end(0)
	{}

	// Load animation object from cgf or caf.
	CDefaultSkeleton* LoadNewCGA(const char* geomName, CharacterManager* pManager, uint32 nLoadingFlags);

	//private:
public:
	void InitNodes(CHeaderTCB* pSkinningInfo, CDefaultSkeleton* pCGAModel, const char* animFile, const string& strName, bool bMakeNodes, uint32 unique_model_id, uint32 nLoadingFlags);

	// Load all animations for this object.
	void   LoadAnimations(const char* cgaFile, CDefaultSkeleton* pCGAModel, uint32 unique_model_id, uint32 nLoadingFlags);
	bool   LoadAnimationANM(const char* animFile, CDefaultSkeleton* pCGAModel, uint32 unique_model_id, uint32 nLoadingFlags);
	uint32 LoadANM(CDefaultSkeleton* pDefaultSkeleton, const char* pFilePath, const char* pAnimName, DynArray<CControllerTCB>& m_LoadCurrAnimation);

	void   Reset();

	DynArray<CControllerTCBVec3>         m_CtrlVec3;
	DynArray<spline::TCBAngleAxisSpline> m_CtrlQuat;

	// Array of controllers.
	DynArray<CControllerType> m_arrControllers;

	DynArray<NodeDesc>        m_arrChunkNodes;

	int                       m_start; // Start time of the animation, expressed in frames (constant frame rate of 30fps is assumed).
	int                       m_end;   // End time of the animation, expressed in frames (constant frame rate of 30fps is assumed).

	uint32                    m_DefaultNodeCount;

	//the controllers for CGA are in this array
	DynArray<CControllerTCB> m_arrNodeAnims;

	// Created animation object
	ModelAnimationHeader m_ModelAnimationHeader;
};
