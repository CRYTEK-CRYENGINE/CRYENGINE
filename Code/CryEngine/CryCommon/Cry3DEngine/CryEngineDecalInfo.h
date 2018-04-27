// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//! Structure containing common parameters that describe a decal.
struct SDecalOwnerInfo
{
	SDecalOwnerInfo() { memset(this, 0, sizeof(*this)); nRenderNodeSlotId = nRenderNodeSlotSubObjectId = -1; }
	struct IStatObj* GetOwner(Matrix34A& objMat);

	struct IRenderNode*       pRenderNode;                //!< Owner (decal will be attached to or wrapped around of this object).
	PodArray<struct SRNInfo>* pDecalReceivers;
	int                       nRenderNodeSlotId;          //!< Is set internally by 3dengine.
	int                       nRenderNodeSlotSubObjectId; //!< Is set internally by 3dengine.
	int                       nMatID;
};

struct CryEngineDecalInfo
{
	SDecalOwnerInfo  ownerInfo;
	Vec3             vPos;                          //!< Decal position (world coordinates).
	Vec3             vNormal;                       //!< Decal/face normal.
	float            fSize;                         //!< Decal size.
	float            fLifeTime;                     //!< Decal life time (in seconds).
	float            fAngle;                        //!< Angle of rotation.
	struct IStatObj* pIStatObj;                     //!< Decal geometry.
	Vec3             vHitDirection;                 //!< Direction from weapon/player position to decal position (bullet direction).
	float            fGrowTime, fGrowTimeAlpha;     //!< Used for blood pools.
	unsigned int     nGroupId;                      //!< Used for multi-component decals.
	bool             bSkipOverlappingTest;          //!< Always spawn decals even if there are a lot of other decals in same place.
	bool             bAssemble;                     //!< Assemble to bigger decals if more than 1 decal is on the same place.
	bool             bForceEdge;                    //!< Force the decal to the nearest edge of the owner mesh and project it accordingly.
	bool             bForceSingleOwner;             //!< Do not attempt to cast the decal into the environment even if it's large enough.
	bool             bDeferred;
	uint8            sortPrio;
	char             szMaterialName[_MAX_PATH]; //!< Name of material used for rendering the decal (in favor of szTextureName/nTid and the default decal shader).
	bool             preventDecalOnGround;      //!< Mainly for decal placement support.
	const Matrix33*  pExplicitRightUpFront;     //!< Mainly for decal placement support.

	void             GetMemoryUsage(ICrySizer* pSizer) const {}

	//! The constructor fills in some non-obligatory fields; the other fields must be filled in by the client.
	CryEngineDecalInfo()
	{
		memset(this, 0, sizeof(*this));
		ownerInfo.nRenderNodeSlotId = ownerInfo.nRenderNodeSlotSubObjectId = -1;
		sortPrio = 255;
	}
};