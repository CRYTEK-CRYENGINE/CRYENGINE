// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Description: Stores all shared weapon parameters (shared by class)...
Allows for some memory savings...

-------------------------------------------------------------------------
History:
- 30:1:2008   10:54 : Benito G.R.

*************************************************************************/

#include "StdAfx.h"
#include "Game.h"
#include "WeaponSharedParams.h"
#include "WeaponSystem.h"
#include "ItemResourceCache.h"
#include "Melee.h"

#include "GameXmlParamReader.h"

#undef ReadOptionalParams
#define ReadOptionalParams(paramString, param) {										\
	CGameXmlParamReader optionaReader(rootNode);										\
	XmlNodeRef optionaParamsNode	= optionaReader.FindFilteredChild(paramString);		\
	if(optionaParamsNode) {																\
		if(!p##param) { p##param = new S##param; }										\
		Read##param(optionaParamsNode);													\
	} else { SAFE_DELETE(p##param); } }			



CWeaponSharedParams::~CWeaponSharedParams()
{
	ResetInternal();
}

//===================================================
void CWeaponSharedParams::ResetInternal()
{
	const int numFiremodes = firemodeParams.size();

	for (int i = 0; i < numFiremodes; i++)
	{
		firemodeParams[i].Release();
	}

	firemodeParams.clear();
	zoommodeParams.clear();

	const int numMeleeParams = meleeParams.size();
	for (int i = 0; i < numMeleeParams; ++i)
	{
		meleeParams[i].Release();
	}

	meleeParams.clear();

	SAFE_DELETE(pVehicleGuided);
	SAFE_DELETE(pPickAndThrowParams);
	SAFE_DELETE(pTurretParams);
	SAFE_DELETE(pMeleeModeParams);
	SAFE_DELETE(pC4Params);
}

//============================================================
void CWeaponSharedParams::GetMemoryUsage(ICrySizer *pSizer) const
{
	pSizer->AddObject(aiWeaponDescriptor);
	pSizer->AddObject(aiWeaponOffsets);
	pSizer->AddObject(hazardWeaponDescriptor);
	pSizer->AddObject(defaultMovementModifiers);
	pSizer->AddObject(zoomedMovementModifiers);

	//Reload Magazine Params
	pSizer->AddObject(reloadMagazineParams);

	//Bullet Belt Params
	pSizer->AddObject(bulletBeltParams);

	//Ammo Params
	pSizer->AddObject(ammoParams);

	//Zoom shared data
	pSizer->AddObject(zoommodeParams);


	//Fire shared data
	pSizer->AddObject(firemodeParams);

	pSizer->AddObject(pickupSound);

	pSizer->AddObject(pVehicleGuided);
	pSizer->AddObject(pPickAndThrowParams);
	pSizer->AddObject(pTurretParams);
	pSizer->AddObject(pMeleeModeParams);
	pSizer->AddObject(pC4Params);
}

void CWeaponSharedParams::ReadWeaponParams(const XmlNodeRef& rootNode, CItemSharedParams* pItemParams, const char* weaponClassName)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	CGameXmlParamReader rootReader(rootNode);

	XmlNodeRef paramsNode = rootReader.FindFilteredChild("params");
	CGameXmlParamReader paramsReader(paramsNode);

	pickupSound = paramsReader.ReadParamValue("ammo_pickup_sound");

	ReadAmmoParams(rootReader.FindFilteredChild("ammos"));
	ReadAIParams(rootReader.FindFilteredChild("ai_descriptor"));
	ReadHazardParams(rootReader.FindFilteredChild("hazard_descriptor"));
	ReadFireModeParams(rootReader.FindFilteredChild("firemodes"), pItemParams);
	ReadZoomModeParams(rootReader.FindFilteredChild("zoommodes"), pItemParams);
	ReadMovementModifierParams(rootReader.FindFilteredChild("MovementModifiers"));
	ReadReloadMagazineParams(rootReader.FindFilteredChild("ReloadMagazineParams"));
	ReadBulletBeltParams(rootReader.FindFilteredChild("BulletBelt"));
	
	ReadOptionalParams("PickAndThrowParams", PickAndThrowParams);
	ReadOptionalParams("vehicleGuided", VehicleGuided);
	ReadOptionalParams("C4Params", C4Params);

	XmlNodeRef turretNode = rootReader.FindFilteredChild("turret");

	if(turretNode)
	{
		if(!pTurretParams)
		{
			pTurretParams = new STurretParams();
		}

		ReadTurretParams(turretNode, paramsNode);
	}
	else
	{
		SAFE_DELETE(pTurretParams);
	}

	//Remove temp xmlrefs inside accessory params after parsing
	for (TAccessoryParamsVector::iterator accessoryIt = pItemParams->accessoryparams.begin(); accessoryIt != pItemParams->accessoryparams.end(); ++accessoryIt)
	{
		accessoryIt->tempAccessoryParams = XmlNodeRef(NULL);
	}
}

//------------------------------------------------------------------------
void CWeaponSharedParams::ReadZoomModeParams(const XmlNodeRef& paramsNode, CItemSharedParams* pItemParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	zoommodeParams.clear();

	if (paramsNode == NULL)
		return;

	CGameXmlParamReader reader(paramsNode);

	const int numZoommodes = reader.GetUnfilteredChildCount();

	//Find the default params
	XmlNodeRef defaultZMNode(NULL);
	for (int i = 0; i < numZoommodes; i++)
	{
		XmlNodeRef zmParamsNode = reader.GetFilteredChildAt(i);
		const char *typ = (zmParamsNode != NULL) ? zmParamsNode->getAttr("type") : "";

		if (typ!=0 && !strcmpi(typ, "default"))
		{
			defaultZMNode = zmParamsNode;
			break;
		}
	}

	zoommodeParams.reserve(numZoommodes);
	for (int i = 0; i < numZoommodes; i++)
	{
		XmlNodeRef zmParamsNode = reader.GetFilteredChildAt(i);

		if (zmParamsNode == NULL)
			continue;

		int enabled = 1;
		const char *name = zmParamsNode->getAttr("name");
		const char *typ = zmParamsNode->getAttr("type");
		zmParamsNode->getAttr("enabled", enabled);

		if (!typ || !typ[0])
		{
			GameWarning("Missing type for zoommode in weapon! Skipping...");
			continue;
		}

		if (!strcmpi(typ, "default"))
			continue;

		if (!name || !name[0])
		{
			GameWarning("Missing name for zoommode in weapon! Skipping...");
			continue;
		}

		SParentZoomModeParams newZoommode;

		newZoommode.initialiseParams.modeName = name;
		newZoommode.initialiseParams.modeType = typ;
		newZoommode.initialiseParams.enabled = (enabled != 0);
	
		ReadZoomMode(defaultZMNode, pItemParams, &newZoommode.baseZoomMode, true);
		ReadZoomMode(zmParamsNode, pItemParams, &newZoommode.baseZoomMode, false);

		const int numAccessoryParams = pItemParams->accessoryparams.size();

		// Pre-counting the number of params would be quite costly so take a decent guess instead
		newZoommode.accessoryChangedParams.reserve(numAccessoryParams * 2);
		for (int j = 0; j < numAccessoryParams; j++)
		{
			const SAccessoryParams* pAccessoryParams = &pItemParams->accessoryparams[j];

			XmlNodeRef defaultPatchNode = FindAccessoryPatchForMode("zoommodes", "default", pAccessoryParams);
			XmlNodeRef zmPatchNode = FindAccessoryPatchForMode("zoommodes", name, pAccessoryParams);

			if(defaultPatchNode || zmPatchNode)
			{	
				SAccessoryZoomModeParams accessoryZoommode;

				accessoryZoommode.pAccessories[0] = pAccessoryParams->pAccessoryClass;

				ReadZoomMode(defaultZMNode, pItemParams, &accessoryZoommode.alteredParams, true);

				if(defaultPatchNode)
				{
					ReadZoomMode(defaultPatchNode, pItemParams, &accessoryZoommode.alteredParams, false);
				}

				ReadZoomMode(zmParamsNode, pItemParams, &accessoryZoommode.alteredParams, false);

				if(zmPatchNode)
				{
					ReadZoomMode(zmPatchNode, pItemParams, &accessoryZoommode.alteredParams, false);
				}

				newZoommode.accessoryChangedParams.push_back(accessoryZoommode);

				for(int k = j; k < numAccessoryParams; k++)
				{
					const SAccessoryParams* pSecondParams = &pItemParams->accessoryparams[k];

					if(pSecondParams->category != pAccessoryParams->category) //category is an itemstring so only requires pointer comparison
					{
						XmlNodeRef secondDefaultPatchNode = FindAccessoryPatchForMode("zoommodes", "default", pSecondParams);
						XmlNodeRef secondZMPatchNode = FindAccessoryPatchForMode("zoommodes", name, pSecondParams);

						if(secondDefaultPatchNode || secondZMPatchNode)
						{
							SAccessoryZoomModeParams secondAccessoryZoommode;

							secondAccessoryZoommode.pAccessories[0] = pAccessoryParams->pAccessoryClass;
							secondAccessoryZoommode.pAccessories[1] = pSecondParams->pAccessoryClass;

							ReadZoomMode(defaultZMNode, pItemParams, &secondAccessoryZoommode.alteredParams, true);
							PatchZoomMode(&secondAccessoryZoommode.alteredParams, pItemParams, defaultPatchNode, secondDefaultPatchNode);
							ReadZoomMode(zmParamsNode, pItemParams, &secondAccessoryZoommode.alteredParams, false);
							PatchZoomMode(&secondAccessoryZoommode.alteredParams, pItemParams, zmPatchNode, secondZMPatchNode);

							newZoommode.accessoryChangedParams.push_back(secondAccessoryZoommode);

							for(int l = k; l < numAccessoryParams; l++)
							{
								const SAccessoryParams* pThirdParams = &pItemParams->accessoryparams[l];

								if(pThirdParams->category != pAccessoryParams->category && pThirdParams->category != pSecondParams->category)
								{
									XmlNodeRef thirdDefaultPatchNode = FindAccessoryPatchForMode("zoommodes", "default", pThirdParams);
									XmlNodeRef thirdZMPatchNode = FindAccessoryPatchForMode("zoommodes", name, pThirdParams);

									if(thirdDefaultPatchNode || thirdZMPatchNode)
									{
										SAccessoryZoomModeParams thirdAccessoryZoommode;

										thirdAccessoryZoommode.pAccessories[0] = pAccessoryParams->pAccessoryClass;
										thirdAccessoryZoommode.pAccessories[1] = pSecondParams->pAccessoryClass;
										thirdAccessoryZoommode.pAccessories[2] = pThirdParams->pAccessoryClass;

										ReadZoomMode(defaultZMNode, pItemParams, &thirdAccessoryZoommode.alteredParams, true);
										PatchZoomMode(&thirdAccessoryZoommode.alteredParams, pItemParams, defaultPatchNode, secondDefaultPatchNode, thirdDefaultPatchNode);
										ReadZoomMode(zmParamsNode, pItemParams, &thirdAccessoryZoommode.alteredParams, false);
										PatchZoomMode(&thirdAccessoryZoommode.alteredParams, pItemParams, zmPatchNode, secondZMPatchNode, thirdZMPatchNode);

										newZoommode.accessoryChangedParams.push_back(thirdAccessoryZoommode);
									}
								}
							}
						}
					}
				}
			}
		}

		zoommodeParams.push_back(newZoommode);
	}
}

void CWeaponSharedParams::PatchZoomMode(SZoomModeParams* pParams, CItemSharedParams* pItemParams, const XmlNodeRef& patchOne, const XmlNodeRef& patchTwo /*= XmlNodeRef(NULL)*/, const XmlNodeRef& patchThree /*= XmlNodeRef(NULL)*/)
{
	if(patchOne)
	{
		ReadZoomMode(patchOne, pItemParams, pParams, false);
	}

	if(patchTwo)
	{
		ReadZoomMode(patchTwo, pItemParams, pParams, false);
	}

	if(patchThree)
	{
		ReadZoomMode(patchThree, pItemParams, pParams, false);
	}
}

void CWeaponSharedParams::PatchMeleeMode(SMeleeModeParams* pParams, const XmlNodeRef& patchOne, const XmlNodeRef& patchTwo /*= XmlNodeRef(NULL)*/, const XmlNodeRef& patchThree /*= XmlNodeRef(NULL)*/)
{
	if(patchOne)
	{
		ReadMeleeMode(patchOne, pParams, false);
	}

	if(patchTwo)
	{
		ReadMeleeMode(patchTwo, pParams, false);
	}

	if(patchThree)
	{
		ReadMeleeMode(patchThree, pParams, false);
	}
}

void CWeaponSharedParams::ReadZoomMode(const XmlNodeRef& paramsNode, CItemSharedParams* pItemParams, SZoomModeParams* pZoomMode, bool defaultInit)
{
	XmlNodeRef aimParamsNode;
	XmlNodeRef zoomNode;
	XmlNodeRef spreadModNode;
	XmlNodeRef recoilModNode; 
	XmlNodeRef zoomSwayNode;
	XmlNodeRef scopeNode;
	XmlNodeRef stereoNode;

	if(paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		aimParamsNode = reader.FindFilteredChild("aimLookParams");
		zoomNode = reader.FindFilteredChild("zoom");
		spreadModNode = reader.FindFilteredChild("spreadMod");
		recoilModNode = reader.FindFilteredChild("recoilMod"); 
		zoomSwayNode = reader.FindFilteredChild("zoomSway");
		scopeNode = reader.FindFilteredChild("scope");
		stereoNode = reader.FindFilteredChild("stereo");
	}

	if (defaultInit)
		pZoomMode->aimLookParams = pItemParams->params.aimLookParams;
	else
		pZoomMode->aimLookParams.Read(aimParamsNode);
	pZoomMode->zoomParams.Reset(zoomNode, defaultInit);
	pZoomMode->spreadModParams.Reset(spreadModNode, defaultInit);
	pZoomMode->recoilModParams.Reset(recoilModNode, defaultInit);
	pZoomMode->zoomSway.Reset(zoomSwayNode, defaultInit);
	pZoomMode->scopeParams.Reset(scopeNode, defaultInit);
	pZoomMode->stereoParams.Reset(stereoNode, defaultInit);
}

//------------------------------------------------------------------------
void CWeaponSharedParams::ReadFireModeParams(const XmlNodeRef& paramsNode, CItemSharedParams* pItemParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	const int numFiremodeParams = firemodeParams.size();

	for (int i = 0; i < numFiremodeParams; i++)
	{
		firemodeParams[i].Release();
	}

	firemodeParams.clear();
	SAFE_DELETE(pMeleeModeParams);

	const int numMeleeParams = meleeParams.size();
	for (int i = 0; i < numMeleeParams; ++i)
	{
		meleeParams[i].Release();
	}

	meleeParams.clear();

	if (!paramsNode)
		return;

	bool meleeFound = false;

	CGameXmlParamReader reader(paramsNode);

	const int numFiremodes = reader.GetUnfilteredChildCount();

	// Find the default and melee params
	XmlNodeRef defaultFMNode(NULL);
	XmlNodeRef meleeFMNode(NULL);
	for (int i = 0; i < numFiremodes; i++)
	{
		XmlNodeRef fmParamsNode = reader.GetFilteredChildAt(i);
		const char* typ = (fmParamsNode != NULL) ? fmParamsNode->getAttr("type") : "";

		if (typ!=0 && !strcmpi(typ, "default"))
		{
			defaultFMNode = fmParamsNode;
			if(meleeFMNode)
			{
				break;
			}
		}
		else if (typ!=0 && !strcmpi(typ, "melee"))
		{
			meleeFMNode = fmParamsNode;
			if(defaultFMNode)
			{
				break;
			}
		}
	}

	firemodeParams.reserve(numFiremodes);
	for (int i = 0; i < numFiremodes; i++)
	{
		XmlNodeRef fmParamsNode = reader.GetFilteredChildAt(i);

		if (fmParamsNode == NULL)
			continue;

		bool isMelee = false;
		int enabled = 1;
			
		const char *name = fmParamsNode->getAttr("name");
		const char *typ = fmParamsNode->getAttr("type");
		
		fmParamsNode->getAttr("enabled", enabled);
	
		if (!typ || !typ[0])
		{
			GameWarning("Missing type for firemode in weapon! Skipping...");
			continue;
		}

		if (!strcmpi(typ, "default"))
			continue;

		if (!name || !name[0])
		{
			GameWarning("Missing name for firemode in weapon! Skipping...");
			continue;
		}


		if(!meleeFound && !strcmp(typ, CMelee::GetWeaponComponentType()))
		{
			pMeleeModeParams = new SMeleeModeParams();

			ReadMeleeMode(defaultFMNode, pMeleeModeParams, true);
			ReadMeleeMode(fmParamsNode, pMeleeModeParams, false);

			meleeFound = true;

			if(meleeFMNode)
			{
				ReadMeleeParams(meleeFMNode, pItemParams);
			}

			continue;
		}
#if !defined (_RELEASE)
		else if(!strcmp(typ, CMelee::GetWeaponComponentType()))
		{
			CRY_ASSERT_MESSAGE(0, "Multiple melee modes found, this isn't supported");
			GameWarning("Multiple melee modes found, this isn't supported");
		}
#endif

		SParentFireModeParams newFiremode;

		newFiremode.initialiseParams.modeName = name;
		newFiremode.initialiseParams.modeType = typ;
		newFiremode.initialiseParams.enabled = (enabled != 0);

		SFireModeParamsUnpacked baseFireMode;
		
		ReadFireMode(defaultFMNode, pItemParams, &baseFireMode, true);
		ReadFireMode(fmParamsNode, pItemParams, &baseFireMode, false);

		newFiremode.pBaseFireMode = new SFireModeParams(baseFireMode);

		const int numAccessoryParams = pItemParams->accessoryparams.size();

		// Pre-counting the number of params would be quite costly so take a decent guess instead
		newFiremode.accessoryChangedParams.reserve(numAccessoryParams * 2);
		for (int j = 0; j < numAccessoryParams; j++)
		{
			const SAccessoryParams* pAccessoryParams = &pItemParams->accessoryparams[j];

			XmlNodeRef defaultPatchNode = FindAccessoryPatchForMode("firemodes", "default", pAccessoryParams);
			XmlNodeRef fmPatchNode = FindAccessoryPatchForMode("firemodes", name, pAccessoryParams);

			if(defaultPatchNode || fmPatchNode)
			{
				SAccessoryFireModeParams accessoryFiremode;
				SFireModeParamsUnpacked accessoryFiremodeParams;

				accessoryFiremode.pAccessories[0] = pAccessoryParams->pAccessoryClass;

				ReadFireMode(defaultFMNode, pItemParams, &accessoryFiremodeParams, true);
				
				if(defaultPatchNode)
				{
					ReadFireMode(defaultPatchNode, pItemParams, &accessoryFiremodeParams, false);
				}
		
				ReadFireMode(fmParamsNode, pItemParams, &accessoryFiremodeParams, false);

				if(fmPatchNode)
				{
					ReadFireMode(fmPatchNode, pItemParams, &accessoryFiremodeParams, false);
				}

				accessoryFiremode.pAlteredParams = new SFireModeParams(accessoryFiremodeParams);

				newFiremode.accessoryChangedParams.push_back(accessoryFiremode);

				for(int k = j; k < numAccessoryParams; k++)
				{
					const SAccessoryParams* pSecondParams = &pItemParams->accessoryparams[k];

					if(pSecondParams->category != pAccessoryParams->category) //category is an itemstring so only requires pointer comparison
					{
						XmlNodeRef secondDefaultPatchNode = FindAccessoryPatchForMode("firemodes", "default", pSecondParams);
						XmlNodeRef secondFMPatchNode = FindAccessoryPatchForMode("firemodes", name, pSecondParams);

						if(secondDefaultPatchNode || secondFMPatchNode)
						{		
							SAccessoryFireModeParams secondAccessoryFiremode;
							SFireModeParamsUnpacked secondAccessoryFiremodeParams;

							secondAccessoryFiremode.pAccessories[0] = pAccessoryParams->pAccessoryClass;
							secondAccessoryFiremode.pAccessories[1] = pSecondParams->pAccessoryClass;

							ReadFireMode(defaultFMNode, pItemParams, &secondAccessoryFiremodeParams, true);
							PatchFireMode(&secondAccessoryFiremodeParams, pItemParams, defaultPatchNode, secondDefaultPatchNode);
							ReadFireMode(fmParamsNode, pItemParams, &secondAccessoryFiremodeParams, false);
							PatchFireMode(&secondAccessoryFiremodeParams, pItemParams, fmPatchNode, secondFMPatchNode);

							secondAccessoryFiremode.pAlteredParams = new SFireModeParams(secondAccessoryFiremodeParams);
							newFiremode.accessoryChangedParams.push_back(secondAccessoryFiremode);
						
							for(int l = k; l < numAccessoryParams; l++)
							{
								const SAccessoryParams* pThirdParams = &pItemParams->accessoryparams[l];

								if(pThirdParams->category != pAccessoryParams->category && pThirdParams->category != pSecondParams->category)
								{
									XmlNodeRef thirdDefaultPatchNode = FindAccessoryPatchForMode("firemodes", "default", pThirdParams);
									XmlNodeRef thirdFMPatchNode = FindAccessoryPatchForMode("firemodes", name, pThirdParams);

									if(thirdDefaultPatchNode || thirdFMPatchNode)
									{
										SAccessoryFireModeParams thirdAccessoryFiremode;
										SFireModeParamsUnpacked thirdAccessoryFiremodeParams;

										thirdAccessoryFiremode.pAccessories[0] = pAccessoryParams->pAccessoryClass;
										thirdAccessoryFiremode.pAccessories[1] = pSecondParams->pAccessoryClass;
										thirdAccessoryFiremode.pAccessories[2] = pThirdParams->pAccessoryClass;

										ReadFireMode(defaultFMNode, pItemParams, &thirdAccessoryFiremodeParams, true);
										PatchFireMode(&thirdAccessoryFiremodeParams, pItemParams, defaultPatchNode, secondDefaultPatchNode, thirdDefaultPatchNode);
										ReadFireMode(fmParamsNode, pItemParams, &thirdAccessoryFiremodeParams, false);
										PatchFireMode(&thirdAccessoryFiremodeParams, pItemParams, fmPatchNode, secondFMPatchNode, thirdFMPatchNode);
										
										thirdAccessoryFiremode.pAlteredParams = new SFireModeParams(thirdAccessoryFiremodeParams);
										newFiremode.accessoryChangedParams.push_back(thirdAccessoryFiremode);
									}
								}
							}
						}
					}
				}
			}
		}

		firemodeParams.push_back(newFiremode);
	}
}

void CWeaponSharedParams::ReadMeleeParams(const XmlNodeRef& meleeFireModeNode, CItemSharedParams* pItemParams)
{
	const int numAccessoryParams = pItemParams->accessoryparams.size();
	meleeParams.reserve(1);

	for (int i = 0; i < numAccessoryParams; ++i)
	{
		const SAccessoryParams* pAccessoryParams = &pItemParams->accessoryparams[i];

		XmlNodeRef meleePatchNode = FindAccessoryPatchForMode("firemodes", "melee", pAccessoryParams);
		if(meleePatchNode)
		{
			SAccessoryMeleeParams accessoryMelee;
			accessoryMelee.pAlteredMeleeParams = new SMeleeModeParams();
			accessoryMelee.pAccessories[0] = pAccessoryParams->pAccessoryClass;
			ReadMeleeMode(meleeFireModeNode, accessoryMelee.pAlteredMeleeParams, true);
			PatchMeleeMode(accessoryMelee.pAlteredMeleeParams, meleePatchNode);
			meleeParams.push_back(accessoryMelee);

			for(int j = i+1; j < numAccessoryParams; ++j)
			{
				const SAccessoryParams* pSecondParams = &pItemParams->accessoryparams[j];
				if(pSecondParams->category != pAccessoryParams->category)
				{
					XmlNodeRef secondMeleePatchNode = FindAccessoryPatchForMode("firemodes", "melee", pSecondParams);

					if(secondMeleePatchNode)
					{
						SAccessoryMeleeParams accessoryMeleeTwo;
						accessoryMeleeTwo.pAlteredMeleeParams = new SMeleeModeParams();
						accessoryMeleeTwo.pAccessories[0] = pAccessoryParams->pAccessoryClass;
						accessoryMeleeTwo.pAccessories[1] = pSecondParams->pAccessoryClass;
						ReadMeleeMode(meleeFireModeNode, accessoryMeleeTwo.pAlteredMeleeParams, true);
						PatchMeleeMode(accessoryMeleeTwo.pAlteredMeleeParams, meleePatchNode, secondMeleePatchNode);
						meleeParams.push_back(accessoryMeleeTwo);

						for(int k = j+1; k < numAccessoryParams; ++k)
						{
							const SAccessoryParams* pThirdParams = &pItemParams->accessoryparams[k];
							if(pThirdParams->category != pAccessoryParams->category && pThirdParams->category != pSecondParams->category)
							{
								XmlNodeRef thirdMeleePatchNode = FindAccessoryPatchForMode("firemodes", "melee", pThirdParams);
								if(thirdMeleePatchNode)
								{
									SAccessoryMeleeParams accessoryMeleeThree;
									accessoryMeleeThree.pAlteredMeleeParams = new SMeleeModeParams();
									accessoryMeleeThree.pAccessories[0] = pAccessoryParams->pAccessoryClass;
									accessoryMeleeThree.pAccessories[1] = pSecondParams->pAccessoryClass;
									accessoryMeleeThree.pAccessories[2] = pThirdParams->pAccessoryClass;
									ReadMeleeMode(meleeFireModeNode, accessoryMeleeThree.pAlteredMeleeParams, true);
									PatchMeleeMode(accessoryMeleeThree.pAlteredMeleeParams, meleePatchNode, secondMeleePatchNode, thirdMeleePatchNode);
									meleeParams.push_back(accessoryMeleeThree);
								}
							}
						}
					}
				}
			}
		}
	}
}

void CWeaponSharedParams::PatchFireMode(SFireModeParamsUnpacked* pParams, CItemSharedParams* pItemParams, const XmlNodeRef& patchOne, const XmlNodeRef& patchTwo /*= XmlNodeRef(NULL)*/, const XmlNodeRef& patchThree /*= XmlNodeRef(NULL)*/)
{
	if(patchOne)
	{
		ReadFireMode(patchOne, pItemParams, pParams, false);
	}

	if(patchTwo)
	{
		ReadFireMode(patchTwo, pItemParams, pParams, false);
	}

	if(patchThree)
	{
		ReadFireMode(patchThree, pItemParams, pParams, false);
	}
}

void CWeaponSharedParams::ReadMeleeMode(const XmlNodeRef& paramsNode, SMeleeModeParams* pMelee, bool defaultInit)
{
	XmlNodeRef meleeNode;
	XmlNodeRef actionsNode;
	XmlNodeRef tagsNode;

	if(paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		actionsNode = reader.FindFilteredChild("actions");
		meleeNode = reader.FindFilteredChild("melee");
		tagsNode = reader.FindFilteredChild("tags");
	}

	pMelee->meleeactions.Reset(actionsNode, defaultInit);
	pMelee->meleeparams.Reset(meleeNode, defaultInit);
	pMelee->meleetags.Reset(tagsNode, defaultInit);
}

void CWeaponSharedParams::ReadFireMode(const XmlNodeRef& paramsNode, CItemSharedParams* pItemParams, SFireModeParamsUnpacked* pFireMode, bool defaultInit)
{
	XmlNodeRef aimLookNode;
	XmlNodeRef fireNode;
	XmlNodeRef tracerNode;
	XmlNodeRef muzzleflashNode;
	XmlNodeRef muzzlesmokeNode;
	XmlNodeRef muzzlebeamNode;
	XmlNodeRef spinupNode;
	XmlNodeRef recoilNode;
	XmlNodeRef proceduralRecoilNode;
	XmlNodeRef spreadNode;
	XmlNodeRef bulletBeltNode;
	XmlNodeRef shotgunNode;
	XmlNodeRef burstParamsNode; 
	XmlNodeRef throwParamsNode;
	XmlNodeRef chargeNode;
	XmlNodeRef effectNode;
	XmlNodeRef rapidNode;
	XmlNodeRef grenadesNode;
	XmlNodeRef beamNode;
	XmlNodeRef hiteffectNode;
	XmlNodeRef meleeNode;
	XmlNodeRef plantNode;
	XmlNodeRef thermalVisionModNode;
	XmlNodeRef animationParamsNode;
	XmlNodeRef pStatChangesNode;
	XmlNodeRef spammerNode;
	XmlNodeRef bowNode;
	XmlNodeRef aiDescNode;
	XmlNodeRef hazardDescNode;
	
	if(paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);
		aimLookNode = reader.FindFilteredChild("aimLookParams");
		fireNode = reader.FindFilteredChild("fire");
		tracerNode = reader.FindFilteredChild("tracer");
		muzzleflashNode = reader.FindFilteredChild("muzzleflash");
		muzzlesmokeNode = reader.FindFilteredChild("muzzlesmoke");
		muzzlebeamNode = reader.FindFilteredChild("muzzlebeam");
		spinupNode = reader.FindFilteredChild("spinup");
		recoilNode = reader.FindFilteredChild("recoil");
		proceduralRecoilNode = reader.FindFilteredChild("proceduralRecoil");
		spreadNode = reader.FindFilteredChild("spread");
		bulletBeltNode = reader.FindFilteredChild("bulletBelt");
		shotgunNode = reader.FindFilteredChild("shotgun");
		burstParamsNode = reader.FindFilteredChild("burst");
		throwParamsNode = reader.FindFilteredChild("throw");
		chargeNode = reader.FindFilteredChild("charge");
		effectNode = reader.FindFilteredChild("effect");
		rapidNode = reader.FindFilteredChild("rapid");
		grenadesNode = reader.FindFilteredChild("grenades");
		beamNode = reader.FindFilteredChild("beam");
		hiteffectNode = reader.FindFilteredChild("hiteffect");
		plantNode = reader.FindFilteredChild("plant");
		thermalVisionModNode = reader.FindFilteredChild("thermalVision");
		animationParamsNode = reader.FindFilteredChild("animation");
		pStatChangesNode = reader.FindFilteredChild("stats_changes");
		spammerNode = reader.FindFilteredChild("spammer");
		bowNode = reader.FindFilteredChild("bow");
		aiDescNode = reader.FindFilteredChild("ai_descriptor");
		hazardDescNode = reader.FindFilteredChild("hazard_descriptor");		
	}

	pFireMode->aimLookParams = pItemParams->params.aimLookParams;
	if (!defaultInit)
		pFireMode->aimLookParams.Read(aimLookNode);
	pFireMode->fireparams.Reset(fireNode, defaultInit);
	pFireMode->tracerparams.Reset(tracerNode, defaultInit);
	pFireMode->recoilparamsCopy.Reset(recoilNode, defaultInit);
	pFireMode->recoilHints.Reset(recoilNode, defaultInit);
	pFireMode->proceduralRecoilParams.Reset(proceduralRecoilNode, defaultInit);
	pFireMode->spreadparamsCopy.Reset(spreadNode, defaultInit);
	pFireMode->muzzleflash.Reset(muzzleflashNode, defaultInit);
	pFireMode->muzzlesmoke.Reset(muzzlesmokeNode, defaultInit);
	pFireMode->muzzlebeam.Reset(muzzlebeamNode, defaultInit);
	pFireMode->spinup.Reset(spinupNode, defaultInit);
	pFireMode->shotgunparams.Reset(shotgunNode, defaultInit); //Only want this if type shotgun
	pFireMode->burstparams.Reset(burstParamsNode, defaultInit); //Only want this if type burst
	pFireMode->throwparams.Reset(throwParamsNode, defaultInit); //Only want this if type throw
	pFireMode->chargeparams.Reset(chargeNode, defaultInit); //only want this if type charge
	pFireMode->chargeeffect.Reset(effectNode, defaultInit); //only want this if type charge
	pFireMode->rapidparams.Reset(rapidNode, defaultInit); //only want this if type rapid
	pFireMode->lTagGrenades.Reset(grenadesNode, defaultInit); //only want this if type ltag
	pFireMode->plantparams.Reset(plantNode, defaultInit); //only want this if type plant
	pFireMode->thermalVisionParams.Reset(thermalVisionModNode, defaultInit);
	pFireMode->animationParams.Reset(animationParamsNode, defaultInit);
	pFireMode->weaponStatChangesParams.ReadStats(pStatChangesNode, defaultInit);
	pFireMode->spammerParams.Reset(spammerNode, defaultInit);
	pFireMode->bowParams.Reset(bowNode, defaultInit);
	pFireMode->aiDescriptor = aiWeaponDescriptor;
	pFireMode->hazardDescriptor = hazardWeaponDescriptor;
	if (!defaultInit)
	{
		pFireMode->aiDescriptor.Reset(aiDescNode, defaultInit);
		pFireMode->hazardDescriptor.Reset(hazardDescNode, defaultInit);		
	}
	
	ReadFireModePluginParams(paramsNode, pFireMode, defaultInit);
}


namespace
{

	template<typename PluginType>
	void ReadPlugin(const CGameXmlParamReader& reader, SFireModeParamsUnpacked* pFireMode, XmlNodeRef actionsNode, const char* nodeName, bool defaultInit)
	{
		XmlNodeRef pluginNode = reader.FindFilteredChild(nodeName);
		if(pluginNode)
		{
				bool overrideDefaultInit = defaultInit;
				PluginType* pParams = static_cast<PluginType*>(pFireMode->FindPluginOfType(PluginType::GetType()));
				if(!pParams)
				{
					overrideDefaultInit = true;
					pParams = new PluginType();
					pFireMode->pluginParams.push_back(pParams);
				}
				if(pParams)
				{
						pParams->Reset(pluginNode, actionsNode, overrideDefaultInit);
				}
		}
	}

}


//------------------------------------------------------------------------
void CWeaponSharedParams::ReadFireModePluginParams(const XmlNodeRef& paramsNode, SFireModeParamsUnpacked* pFireMode, bool defaultInit)
{
	XmlNodeRef actionsNode;

	if(paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		actionsNode = reader.FindFilteredChild("actions");

		ReadPlugin<SFireModePlugin_HeatingParams>(reader, pFireMode, actionsNode, "heating", defaultInit);
		ReadPlugin<SFireModePlugin_RejectParams>(reader, pFireMode, actionsNode, "reject", defaultInit);
		ReadPlugin<SFireModePlugin_AutoAimParams>(reader, pFireMode, actionsNode, "autoAim", defaultInit);
		ReadPlugin<SFireModePlugin_RecoilShakeParams>(reader, pFireMode, actionsNode, "recoilShake", defaultInit);
	}
}

//------------------------------------------------------------------------
void CWeaponSharedParams::ReadAmmoParams(const XmlNodeRef& paramsNode)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	ammoParams.ammo.resize(0);
	ammoParams.accessoryAmmo.resize(0);
	ammoParams.bonusAmmo.resize(0);
	ammoParams.minDroppedAmmo.resize(0);
	ammoParams.capacityAmmo.resize(0);
	ammoParams.extraItems = 0;
	
	if (!paramsNode)
		return;

	CGameXmlParamReader reader(paramsNode);

	const int numAmmo = reader.GetUnfilteredChildCount();

	ammoParams.ammo.reserve(numAmmo);
	ammoParams.accessoryAmmo.reserve(numAmmo);
	ammoParams.bonusAmmo.reserve(numAmmo);
	ammoParams.minDroppedAmmo.reserve(numAmmo);
	ammoParams.capacityAmmo.reserve(numAmmo);

	for (int i = 0; i < numAmmo; i++)
	{
		XmlNodeRef ammoNode = reader.GetFilteredChildAt(i);
		
		if (ammoNode == NULL)
			continue;

		if (!strcmpi(ammoNode->getTag(), "ammo"))
		{
			int extra = 0;
			int amount = 0;
			int accessoryAmmo = 0;
			int minAmmo = 0;
			int capacity = 0;

			const char* name = ammoNode->getAttr("name");
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
			CRY_ASSERT_TRACE(pClass, ("Ammo '%s' class not found in class registry, for weapon", name));

			ammoNode->getAttr("amount", amount);
			ammoNode->getAttr("extra", extra);
			ammoNode->getAttr("accessoryAmmo", accessoryAmmo);
			ammoNode->getAttr("minAmmo", minAmmo);
			ammoNode->getAttr("capacity", capacity);

			SWeaponAmmo ammoInfo(pClass, amount);

			if (accessoryAmmo)
			{
				ammoInfo.count = accessoryAmmo;
				ammoParams.ammo.push_back(ammoInfo);
				ammoParams.accessoryAmmo.push_back(ammoInfo);
			}
			else
			{
				ammoParams.ammo.push_back(ammoInfo);
			}

			if (extra)
			{
				ammoInfo.count = extra;
				ammoParams.bonusAmmo.push_back(ammoInfo);
			}

			if (minAmmo)
			{
				ammoInfo.count = minAmmo;
				ammoParams.minDroppedAmmo.push_back(ammoInfo);
			}

			if (capacity)
			{
				ammoInfo.count = capacity;
				ammoParams.capacityAmmo.push_back(ammoInfo);
			}
		}
		else if(!strcmpi(ammoNode->getTag(), "item"))
		{
			CRY_ASSERT(ammoParams.extraItems == 0);
			ammoNode->getAttr("extra", ammoParams.extraItems);
		}
	}
}

//----------------------------------------------------------------------
void CWeaponSharedParams::ReadReloadMagazineParams(const XmlNodeRef& paramsNode)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	if (!paramsNode)
		return;

	const char* magazineEvent = paramsNode->getAttr("magazineEvent");
	reloadMagazineParams.magazineEventCRC32 = (magazineEvent != NULL) ? CCrc32::ComputeLowercase(magazineEvent) : 0;
	reloadMagazineParams.magazineAttachment = paramsNode->getAttr("magazineAttachment");
}

//------------------------------------------------------------------------
void CWeaponSharedParams::ReadBulletBeltParams(const XmlNodeRef& paramsNode)
{
	if (paramsNode)
	{
		bulletBeltParams.jointName = paramsNode->getAttr("jointName");
		paramsNode->getAttr("beltRefillReloadFraction", bulletBeltParams.beltRefillReloadFraction);
		
		const char* pAmmoName = paramsNode->getAttr("ammoType");

		if(pAmmoName)
		{
			bulletBeltParams.pAmmoClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(pAmmoName);
		}
		
		int numOfBullets = 0;
		if (paramsNode->getAttr("numBullets", numOfBullets))
		{
			bulletBeltParams.numBullets = (uint16)numOfBullets;
		}
	}
}

//------------------------------------------------------------------------
void CWeaponSharedParams::ReadMovementModifierParams(const XmlNodeRef& paramsNode)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	const int numZoommodes = zoommodeParams.size();

	//Set default values for all zoom-modes
	zoomedMovementModifiers.reserve(numZoommodes);
	zoomedMovementModifiers.clear();

	for (int i = 0; i < numZoommodes; i++)
	{
		zoomedMovementModifiers.push_back(defaultMovementModifiers);
	}

	//Read actual values if present
	if (paramsNode)
	{
		float firingMovementScale = 1.f;
		float firingRotationScale = 1.f;

		//Get first the default/base ones
		paramsNode->getAttr("firingSpeedScale", firingMovementScale);
		paramsNode->getAttr("firingRotationScale", firingRotationScale);
	
		paramsNode->getAttr("speedScale", defaultMovementModifiers.movementSpeedScale);
		paramsNode->getAttr("rotationScale", defaultMovementModifiers.rotationSpeedScale);
		
		defaultMovementModifiers.mouseRotationSpeedScale = defaultMovementModifiers.rotationSpeedScale;
		paramsNode->getAttr("mouseRotationScale", defaultMovementModifiers.mouseRotationSpeedScale);
		
		defaultMovementModifiers.firingMovementSpeedScale = defaultMovementModifiers.movementSpeedScale * firingMovementScale;
		defaultMovementModifiers.firingMovementSpeedScale = defaultMovementModifiers.movementSpeedScale * firingMovementScale;
		defaultMovementModifiers.firingRotationSpeedScale = defaultMovementModifiers.rotationSpeedScale * firingRotationScale;
		defaultMovementModifiers.mouseFiringRotationSpeedScale = defaultMovementModifiers.mouseRotationSpeedScale * firingRotationScale;

		CGameXmlParamReader reader(paramsNode);

		//Patch defined zoom-modes with corresponding values
		for(int i = 0; i < numZoommodes; i++)
		{
			XmlNodeRef zoomModeModifierNode = reader.FindFilteredChild(zoommodeParams[i].initialiseParams.modeName.c_str());

			if (zoomModeModifierNode)
			{
				SPlayerMovementModifiers& zoomedModifiers = zoomedMovementModifiers[i];

				zoomModeModifierNode->getAttr("speedScale", zoomedModifiers.movementSpeedScale);
				zoomModeModifierNode->getAttr("rotationScale", zoomedModifiers.rotationSpeedScale);
				
				zoomedModifiers.mouseRotationSpeedScale = zoomedModifiers.rotationSpeedScale;
				zoomModeModifierNode->getAttr("mouseRotationScale", zoomedModifiers.mouseRotationSpeedScale);
				
				zoomedModifiers.firingMovementSpeedScale = zoomedModifiers.movementSpeedScale * firingMovementScale;
				zoomedModifiers.firingRotationSpeedScale = zoomedModifiers.rotationSpeedScale * firingRotationScale;
				zoomedModifiers.mouseFiringRotationSpeedScale = zoomedModifiers.mouseRotationSpeedScale * firingRotationScale;
			}
		}
	}
}

//------------------------------------------------------------------------
XmlNodeRef CWeaponSharedParams::FindAccessoryPatchForMode(const char* modeType, const char *modeName, const SAccessoryParams* pAccessory)
{
	CGameXmlParamReader reader(pAccessory->tempAccessoryParams);

	XmlNodeRef modesNode = reader.FindFilteredChild(modeType);
	if (modesNode)
	{
		CGameXmlParamReader modesReader(modesNode);

		const int numOfModes = modesReader.GetUnfilteredChildCount();
		for (int i = 0; i < numOfModes; i++)
		{
			XmlNodeRef modePatchNode = modesReader.GetFilteredChildAt(i);
			
			if (modePatchNode)
			{
				const char *name = modePatchNode->getAttr("name");

				if (name!=0 && !stricmp(name, modeName))
				{
					return modePatchNode;
				}
				const char *typ = modePatchNode->getAttr("type");
				if (typ!=0 && !stricmp(typ, modeName))
				{
					return modePatchNode;
				}
			}
		}		
	}
	
	return XmlNodeRef(NULL);
}

void CWeaponSharedParams::ReadPickAndThrowParams(const XmlNodeRef& paramsNode)
{
	CRY_ASSERT( paramsNode != NULL );
	
	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		pPickAndThrowParams->grabTypesParams.resize(0);

		const int numTypes = reader.GetUnfilteredChildCount();
		if (numTypes>0)
		{
			pPickAndThrowParams->grabTypesParams.reserve( numTypes );

			for (int i=0; i < numTypes; ++i)
			{
				XmlNodeRef nodeType = reader.GetFilteredChildAt( i );
				if (nodeType)
				{
					SPickAndThrowParams::SGrabTypeParams typeParams;;
					typeParams.Read( nodeType );
					pPickAndThrowParams->grabTypesParams.push_back(typeParams);
				}
			}
		}
	}
}


void SPickAndThrowParams::SGrabTypeParams::Read( const XmlNodeRef& paramsNode )
{
	if (!paramsNode)
		return;

	CGameXmlParamReader reader(paramsNode);

	helper = reader.ReadParamValue("helper");
	helper_3p = reader.ReadParamValue("helper_3p");
	attachment = reader.ReadParamValue("attachment");
	display_name = reader.ReadParamValue("display_name");
	grab_action_part1 = reader.ReadParamValue("grab_action_part1");
	grab_action_part1_alternate = reader.ReadParamValue("grab_action_part1_alternate"); // Used when picking up an environmental weapon that has been unrooted
	drop_action = reader.ReadParamValue("drop_action");
	melee_action = reader.ReadParamValue("melee_action");
	melee_recovery_action = reader.ReadParamValue("melee_recovery_action");
	melee_hit_action = reader.ReadParamValue("melee_hit_action");
	reader.ReadParamValue<Vec3>("melee_hit_dir", melee_hit_dir, Vec3(0.f,0.f,0.f));
	reader.ReadParamValue<float>("melee_hit_dir_min_ratio", melee_hit_dir_min_ratio, 0.f);
	reader.ReadParamValue<float>("melee_hit_dir_max_ratio", melee_hit_dir_max_ratio, 0.f);

	const char* pAudioSignalName = reader.ReadParamValue("audioSignalHitOnNPC");
	if (pAudioSignalName && pAudioSignalName[0]!=0)
		audioSignalHitOnNPC = g_pGame->GetGameAudio()->GetSignalID( pAudioSignalName );

	reader.ReadParamValue<float>( "melee_delay", melee_delay );
	reader.ReadParamValue<float>( "meleeAction_duration", meleeAction_duration );
	CRY_ASSERT( meleeAction_duration >= melee_delay );
	reader.ReadParamValue<float>( "melee_damage_scale", melee_damage_scale );
	reader.ReadParamValue<float>( "melee_impulse_scale", melee_impulse_scale );

	// Or alternatively forced to move to a specific point
	if(reader.ReadParamValue<Vec3>("grab_orientation_override_localOffset", grab_orientation_override_localOffset))
	{
		m_propertyFlags |= EPF_grab_orientation_override_localOffset_specified; 
	}
	
	// Melee simulation
	bool bTemp = false;
	reader.ReadParamValue<bool>( "use_physical_melee_system", bTemp);
	if(bTemp)
	{
		m_propertyFlags |= EPF_use_physical_melee_system;
	}

	melee_mfxLibrary = reader.ReadParamValue( "melee_mfxLibrary" );
	NPCtype = reader.ReadParamValue( "NPCtype" );
	animGraphPose = reader.ReadParamValue( "pose" );
	tag = reader.ReadParamValue( "tag", animGraphPose.c_str() );
	dbaFile = reader.ReadParamValue( "dbaFile", "");
	
	if(!NPCtype.empty())
	{
		m_propertyFlags |= EPF_isNPCType;
	}

	// Complex melee lerp 
	reader.ReadParamValue<float>( "complexMelee_snap_target_select_range", complexMelee_snap_target_select_range );
	reader.ReadParamValue<float>( "complexMelee_snap_end_position_range", complexMelee_snap_end_position_range ); 

	// Anim duration overrides Do we want this in release (could be nice for small gameplay tweaks)? 
	reader.ReadParamValue<float>( "anim_durationOverride_RootedGrab",		anim_durationOverride_RootedGrab );
	reader.ReadParamValue<float>( "anim_durationOverride_Grab",				anim_durationOverride_Grab );
	reader.ReadParamValue<float>( "anim_durationOverride_PrimaryAttack",	anim_durationOverride_PrimaryAttack );
	reader.ReadParamValue<float>( "anim_durationOverride_ComboAttack",	    anim_durationOverride_ComboAttack );

	// Read combo attacks (if any)
	// NOTE: probably not worth reserving with vec here if 99% of grab params don't make use of combos... 
	bool bContinue = true; 
	int i = 1; 
	CryFixedStringT<32> comboStringName;
	CryFixedStringT<32> comboRecoveryStringName;
	while(bContinue)
	{
		comboStringName.Format("melee_action_%d",i);
		comboRecoveryStringName.Format("melee_action_%d_recovery",i);
		m_meleeComboList.push_back( std::make_pair( ItemString(reader.ReadParamValue(comboStringName.c_str())), ItemString(reader.ReadParamValue(comboRecoveryStringName.c_str(), ""))) );

		// Abort when we fail
		if(m_meleeComboList[i-1].first.length() <= 0)
		{
			bContinue = false; 
			m_meleeComboList.pop_back(); 
		}
		else
		{
			// prepare to parse next
			++i;
		}
	}
	
	// Read shield params
	shieldsPlayer = false;
	reader.ReadParamValue<bool>( "shieldsPlayer", shieldsPlayer );

	// Read melee action cooldown
	reader.ReadParamValue<float>( "post_melee_action_cooldown_primary", post_melee_action_cooldown_primary );
	reader.ReadParamValue<float>( "post_melee_action_cooldown_combo", post_melee_action_cooldown_combo );

	// Read vehicle damage params
	reader.ReadParamValue<float>("vehicleMeleeDamage", vehicleMeleeDamage);
	reader.ReadParamValue<float>("vehicleThrowDamage", vehicleThrowDamage);
	reader.ReadParamValue<float>("vehicleThrowMinVelocity", vehicleThrowMinVelocity);
	reader.ReadParamValue<float>("vehicleThrowMaxVelocity", vehicleThrowMaxVelocity);
	CRY_ASSERT(vehicleThrowMinVelocity < vehicleThrowMaxVelocity);
	
	reader.ReadParamValue<float>("movementSpeedMultiplier", movementSpeedMultiplier);
	
	// default values (not for all fields)
	timePicking = 0.2f;
	timeToStickAtPicking = 0.1f;
	timeDropping = 0.5f;
	timeToFreeAtDropping = 0.1f;
	dropSpeedForward = 2.f;
	bool hideInteractionPrompts = false; 

	reader.ReadParamValue<float>("timePicking", timePicking );
	reader.ReadParamValue<float>("timeToStickAtPicking", timeToStickAtPicking );

	reader.ReadParamValue<float>("dropSpeedForward", dropSpeedForward );

	reader.ReadParamValue<float>("timeDropping", timeDropping );
	reader.ReadParamValue<float>("timeToFreeAtDropping", timeToFreeAtDropping );

	reader.ReadParamValue<float>("holdingEnergyConsumption", holdingEnergyConsumption );
	reader.ReadParamValue<int>("shieldingHits", shieldingHits );
	reader.ReadParamValue<int>("crossHairType", crossHairType, (int)eHCH_None);
	reader.ReadParamValue<bool>("hideInteractionPrompts", hideInteractionPrompts );
	if(hideInteractionPrompts)
	{
		m_propertyFlags |= EPF_hideInteractionPrompts; 
	}
	
	bool bUseSweepTestMelee = false; 
	reader.ReadParamValue<bool>("useSweepTestMelee", bUseSweepTestMelee );
	if(bUseSweepTestMelee)
	{
		m_propertyFlags |= EPF_useSweepTestMelee; 
	}
	
	killGrabbedNPCParams.Read( reader.FindFilteredChild("killGrabbedNPCParams") );
	throwParams.Read( reader.FindFilteredChild("throwParams") );
}

//////////////////////////////////////////////////////////////////////////
void SPickAndThrowParams::SGrabTypeParams::CacheResources()
{
	if (!dbaFile.empty())
	{
		IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
		mannequinSys.GetAnimationDatabaseManager().Load(dbaFile.c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
void SPickAndThrowParams::SGrabTypeParams::ReadCameraShake( const XmlNodeRef& params, SPickAndThrowParams::SCameraShakeParams* pCameraShake )
{
	if(pCameraShake)
	{
		params->getAttr("shift", pCameraShake->shift);
		params->getAttr("rotate", pCameraShake->rotate);
		params->getAttr("time", pCameraShake->time);
		params->getAttr("frequency", pCameraShake->frequency);
		int viewAngleAttenuation = 1;
		int randomDirection = 0;
		params->getAttr("viewAngleAttenuation", viewAngleAttenuation);
		params->getAttr("randomDirection", randomDirection);
		pCameraShake->viewAngleAttenuation = (viewAngleAttenuation != 0);
		pCameraShake->randomDirection = (randomDirection != 0);
	}
}

//////////////////////////////////////////////////////////////////////////
void SPickAndThrowParams::SThrowParams::SetDefaultValues()
{
	timeToFreeAtThrowing = 0.1f;
	timeToRestorePhysicsAfterFree = 0;
	throwSpeed = 12;
	autoThrowSpeed = 1;
	powerThrownDelay = 1.0f;
	chargedThrowEnabled = false; 
	maxChargedThrowSpeed = 15.0f;
}

void SPickAndThrowParams::SThrowParams::Read( const XmlNodeRef& paramsNode )
{
	SetDefaultValues();

	if (paramsNode)
	{
		CGameXmlParamReader localReader( paramsNode );
		throw_action = localReader.ReadParamValue( "throw_action", "throw" );
		localReader.ReadParamValue<float>( "timeToFreeAtThrowing", timeToFreeAtThrowing );
		localReader.ReadParamValue<float>( "timeToRestorePhysicsAfterFree", timeToRestorePhysicsAfterFree );
		localReader.ReadParamValue<float>( "throwSpeed", throwSpeed );
		localReader.ReadParamValue<float>( "autoThrowSpeed", autoThrowSpeed );
		localReader.ReadParamValue<float>( "powerThrownDelay", powerThrownDelay);

		// Charged throw setup
		localReader.ReadParamValue<bool>( "charged_throw_enabled", chargedThrowEnabled );
		chargedObjectThrowAction	= localReader.ReadParamValue( "charged_object_throw_action" );
		localReader.ReadParamValue<Vec3>( "charged_throw_initial_facing_dir", chargedThrowInitialFacingDir, Vec3(0.f,1.f,0.f));
		localReader.ReadParamValue<float>("maxChargedThrowSpeed", maxChargedThrowSpeed);
	}
}

//////////////////////////////////////////////////////////////////////////
void SPickAndThrowParams::SKillGrabbedNPCParams::Read( const XmlNodeRef& paramsNode )
{
	if (paramsNode)
	{
		CGameXmlParamReader localReader( paramsNode );
		action = localReader.ReadParamValue( "action" );
		localReader.ReadParamValue<bool>( "showKnife", showKnife );
	}
}

//------------------------------------------------------------------------
void CWeaponSharedParams::ReadAIParams(const XmlNodeRef& paramsNode)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	// Still set gravity descriptor even if we don't have it specified in the XML
	// the first firemode (which isn't melee) will be the default firemode entered

	const int numAmmoTypes = ammoParams.ammo.size();
	for (int i = 0; i < numAmmoTypes; i++)
	{
		if (const SAmmoParams *pAmmoParams = g_pGame->GetWeaponSystem()->GetAmmoParams(ammoParams.ammo[i].pAmmoClass))
		{
			if (pAmmoParams->pParticleParams && !is_unused(pAmmoParams->pParticleParams->gravity))
			{
				aiWeaponDescriptor.descriptor.projectileGravity = pAmmoParams->pParticleParams->gravity;
			}
			break;
		}
	}


	if (paramsNode)
	{
		aiWeaponDescriptor.Reset(paramsNode, false);

		CGameXmlParamReader reader(paramsNode);
		XmlNodeRef offsetNode = reader.FindFilteredChild("weaponOffset");
		if(offsetNode)
		{
			ReadAIOffsets(offsetNode);
		}
	}
}


//------------------------------------------------------------------------
void CWeaponSharedParams::ReadAIOffsets(const XmlNodeRef& paramsNode)
{
	CGameXmlParamReader reader(paramsNode);

	if(reader.FindFilteredChild("useEyeOffset"))
	{
		aiWeaponOffsets.useEyeOffset = true;
	}
	else
	{
		const int offsetCount = reader.GetUnfilteredChildCount();
		for (int i = 0; i < offsetCount; ++i)
		{
			XmlNodeRef offsetNode = reader.GetFilteredChildAt(i);

			if (offsetNode)
			{
				const char *stance = offsetNode->getAttr("stanceId");
				// retrieve enum value from scripts, using stance name
				int curStanceInt(STANCE_NULL);
				if(!gEnv->pScriptSystem->GetGlobalValue(stance,curStanceInt))
					continue;
				EStance	curStance((EStance)curStanceInt);
				if(curStance==STANCE_NULL)
					continue;
				Vec3	curOffset;
				if(!offsetNode->getAttr("weaponOffset", curOffset))
					continue;

				TStanceWeaponOffset::iterator itFind = aiWeaponOffsets.stanceWeponOffset.find(curStance);
				if (itFind == aiWeaponOffsets.stanceWeponOffset.end())
				{
					aiWeaponOffsets.stanceWeponOffset.insert(TStanceWeaponOffset::value_type(curStance, curOffset));
				}
				else
				{
					itFind->second = curOffset;
				}
					
				if(!offsetNode->getAttr("weaponOffsetLeanLeft", curOffset))
					continue;

				itFind = aiWeaponOffsets.stanceWeponOffsetLeanLeft.find(curStance);
				if (itFind == aiWeaponOffsets.stanceWeponOffsetLeanLeft.end())
				{
					aiWeaponOffsets.stanceWeponOffsetLeanLeft.insert(TStanceWeaponOffset::value_type(curStance, curOffset));
				}
				else
				{
					itFind->second = curOffset;
				}

				if(!offsetNode->getAttr("weaponOffsetLeanRight", curOffset))
					continue;

				itFind = aiWeaponOffsets.stanceWeponOffsetLeanRight.find(curStance);
				if (itFind == aiWeaponOffsets.stanceWeponOffsetLeanRight.end())
				{
					aiWeaponOffsets.stanceWeponOffsetLeanRight.insert(TStanceWeaponOffset::value_type(curStance, curOffset));
				}
				else
				{
					itFind->second = curOffset;
				}
			}
		}
	}
}


//------------------------------------------------------------------------
void CWeaponSharedParams::ReadHazardParams(const XmlNodeRef& paramsNode)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	if (paramsNode)
	{
		hazardWeaponDescriptor.Reset(paramsNode, false);		
	}
}


void CWeaponSharedParams::ReadTurretParams(const XmlNodeRef& turretNode, const XmlNodeRef& paramsNode)
{
	CGameXmlParamReader reader(paramsNode);

	pTurretParams->rocketFireMode = reader.ReadParamValue("rocket_firemode");
	pTurretParams->radarHelper = reader.ReadParamValue("radar_helper");
	pTurretParams->tripodHelper = reader.ReadParamValue("tripod_helper");
	pTurretParams->barrelHelper = reader.ReadParamValue("barrel_helper");
	pTurretParams->fireHelper = reader.ReadParamValue("fire_helper");
	pTurretParams->rocketHelper = reader.ReadParamValue("rocket_helper");
	reader.ReadParamValue<float>("HoverHeight", pTurretParams->desiredHoverHeight);
	
	CGameXmlParamReader turretReader(turretNode);

	XmlNodeRef searchNode = turretReader.FindFilteredChild("search");
	if (searchNode)
	{
		pTurretParams->searchParams.Reset(searchNode);
	}

	XmlNodeRef fireNode = turretReader.FindFilteredChild("fire");
	if (fireNode)
	{
		pTurretParams->fireParams.Reset(fireNode); 
	}
}

void CWeaponSharedParams::CacheResources()
{
	//Fire mode resources
	TParentFireModeParamsVector::iterator firemodesEndIt = firemodeParams.end();

	for (TParentFireModeParamsVector::iterator fmIt = firemodeParams.begin(); fmIt != firemodesEndIt; ++fmIt)
	{
		fmIt->CacheResources();
	}

	//Zoom mode resources
	TParentZoomModeParamsVector::iterator zoommodeEndIt = zoommodeParams.end();
	for (TParentZoomModeParamsVector::iterator zmIt = zoommodeParams.begin(); zmIt != zoommodeEndIt; ++zmIt)
	{
		zmIt->CacheResources();
	}

	if (pPickAndThrowParams)
	{
		const size_t grabParamsCount = pPickAndThrowParams->grabTypesParams.size();
		for (size_t i = 0; i < grabParamsCount; ++i)
		{
			pPickAndThrowParams->grabTypesParams[i].CacheResources();
		}
	}
}

void CWeaponSharedParams::ReadVehicleGuided(const XmlNodeRef& paramsNode)
{
	pVehicleGuided->Reset(paramsNode, false);
}

void CWeaponSharedParams::ReadC4Params(const XmlNodeRef& paramsNode)
{
	pC4Params->Reset(paramsNode, false);
}


void CWeaponSharedParams::ReleaseLevelResources()
{

}

SMeleeModeParams* CWeaponSharedParams::FindAccessoryMeleeParams( IEntityClass* pClass, IEntityClass* pClassTwo /*= NULL*/, IEntityClass* pClassThree /*= NULL*/, IEntityClass* pClassFour /*= NULL*/ ) const
{
	const int numAccessoryParams = meleeParams.size();

	for(int i = 0; i < numAccessoryParams; ++i)
	{
		const SAccessoryMeleeParams* pParams = &meleeParams[i];

		if(pClass == pParams->pAccessories[0] && pClassTwo == pParams->pAccessories[1] && pClassThree == pParams->pAccessories[2] && pClassFour == pParams->pAccessories[3])
		{
			return pParams->pAlteredMeleeParams;
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void SZoomModeParams::CacheResources()
{
	CItemMaterialAndTextureCache& textureCache = g_pGame->GetGameSharedParametersStorage()->GetItemResourceCache().GetMaterialsAndTextureCache();

	if (!zoomParams.blur_mask.empty())
	{
		textureCache.CacheTexture(zoomParams.blur_mask.c_str(), true);
	}

	if (!zoomParams.dof_mask.empty())
	{
		textureCache.CacheTexture(zoomParams.dof_mask.c_str(), true);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void STurretSearchParams::Reset( const XmlNodeRef& paramsNode, bool defaultInit /*= true*/ )
{
	if (defaultInit)
	{
		hints.resize(0);
		light_helper.clear();
		light_texture.clear();
		light_material.clear();
		light_color = Vec3(1,1,1);
		light_diffuse_mul = 1.f;
		light_hdr_dyn = 0.f;
	}

	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		light_helper = reader.ReadParamValue("light_helper", light_helper.c_str());

		XmlNodeRef hintsNode = reader.FindFilteredChild("hints");
		if (hintsNode && hintsNode->getChildCount())
		{
			CGameXmlParamReader hintsReader(hintsNode);

			Vec2 hintEntry;
			const int hintCount = hintsReader.GetUnfilteredChildCount();
			hints.reserve(hintCount);

			for (int i = 0; i < hintCount; ++i)
			{
				XmlNodeRef hintNode = hintsReader.GetFilteredChildAt(i);
				if (hintNode && hintNode->getAttr("x", hintEntry.x) && hintNode->getAttr("y", hintEntry.y))
				{
					Limit(hintEntry.x, -1.f, 1.f);
					Limit(hintEntry.y, -1.f, 1.f);
					hints.push_back(hintEntry);
				}
			}
		}    

		XmlNodeRef lightNode = reader.FindFilteredChild("light");
		if (lightNode)
		{
			light_helper = lightNode->getAttr("helper");
			light_texture = lightNode->getAttr("texture");
			light_material = lightNode->getAttr("material");
			lightNode->getAttr("color", light_color);
			lightNode->getAttr("diffuse_mul", light_diffuse_mul);
			lightNode->getAttr("hdr_dyn", light_hdr_dyn);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void STurretFireParams::Reset( const XmlNodeRef& paramsNode, bool defaultInit /*= true*/ )
{
	if (defaultInit)
	{
		hints.resize(0);  
		deviation_speed = 0.0f;
		deviation_amount = 0.0f;
		randomness = 0.0f;
	}

	if (paramsNode)
	{     
		CGameXmlParamReader reader(paramsNode);

		reader.ReadParamValue<float>("deviation_speed", deviation_speed);
		reader.ReadParamValue<float>("deviation_amount", deviation_amount);
		reader.ReadParamValue<float>("randomness", randomness);

		XmlNodeRef hintsNode = reader.FindFilteredChild("hints");
		if (hintsNode && hintsNode->getChildCount())
		{
			CGameXmlParamReader hintsReader(hintsNode);

			Vec2 hintEntry;
			const int hintCount = hintsReader.GetUnfilteredChildCount();
			hints.reserve(hintCount);

			for (int i = 0; i < hintCount; ++i)
			{
				XmlNodeRef hintNode = hintsReader.GetFilteredChildAt(i);
				if (hintsNode && hintsNode->getAttr("x", hintEntry.x) && hintsNode->getAttr("y", hintEntry.y))
				{ 
					hints.push_back(hintEntry);
				}
			}
		}   
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void SZoomParams::Reset( const XmlNodeRef& paramsNode, bool defaultInit /*= true*/ )
{
	if (defaultInit)
	{
		suffix = "ironsight";
		suffixAG = "";
		dof = false;
		dof_focusMin = 1.0f;
		dof_focusMax = 150.0f;
		dof_focusLimit = 150.0f;
		dof_shoulderMinZ = 0.0f;
		dof_shoulderMinZScale = 0.0f;
		dof_minZ = 0.0f;
		dof_minZScale = 0.0f;
		blur_amount = 0.0f;
		blur_mask = "";
		dof_mask = "";
		zoom_overlay = "";

		zoom_in_time = 0.35f;
		zoom_out_time = 0.35f;
		zoom_out_delay = 0.0f;
		stage_time = 0.055f;
		scope_mode = false;
		scope_nearFov = 55.0f;
		shoulderRotationAnimFactor = 1.0f;
		shoulderStrafeAnimFactor = 1.0f;
		shoulderMovementAnimFactor = 1.0f;
		ironsightRotationAnimFactor = 1.0f;
		ironsightStrafeAnimFactor = 1.0f;
		ironsightMovementAnimFactor = 1.0f;
		cameraShakeMultiplier = 1.0f;

		muzzle_flash_scale = 1.0f;

		fp_offset = ZERO;
		fp_rot_offset.SetRotationXYZ(Ang3(0.0f, 0.0f, 0.0f));

		hide_weapon = false;
		target_snap_enabled = true;

		hide_crosshair = true;
		near_fov_sync = false;

		armor_bonus_zoom = false;

		stages.resize(0);
		stages.push_back(SStageParams());
	}

	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		suffix = reader.ReadParamValue("suffix", suffix.c_str());
		suffixAG = reader.ReadParamValue("suffixAG", suffixAG.c_str());
		reader.ReadParamValue<bool>("dof", dof);
		reader.ReadParamValue<float>("dof_focusMin", dof_focusMin);
		reader.ReadParamValue<float>("dof_focusMax", dof_focusMax);
		reader.ReadParamValue<float>("dof_focusLimit", dof_focusLimit);
		reader.ReadParamValue<float>("dof_shoulderMinZ", dof_shoulderMinZ);
		reader.ReadParamValue<float>("dof_shoulderMinZScale", dof_shoulderMinZScale);
		reader.ReadParamValue<float>("dof_minZ", dof_minZ);
		reader.ReadParamValue<float>("dof_minZScale", dof_minZScale);
		blur_mask = reader.ReadParamValue("blur_mask", blur_mask.c_str());
		dof_mask = reader.ReadParamValue("dof_mask", dof_mask.c_str());
		zoom_overlay = reader.ReadParamValue("zoom_overlay", zoom_overlay.c_str());
		reader.ReadParamValue<float>("blur_amount", blur_amount);
		reader.ReadParamValue<float>("zoom_in_time", zoom_in_time);
		reader.ReadParamValue<float>("zoom_out_time", zoom_out_time);
		reader.ReadParamValue<float>("zoom_out_delay", zoom_out_delay);
		reader.ReadParamValue<float>("stage_time", stage_time);
		reader.ReadParamValue<bool>("scope_mode", scope_mode);
		reader.ReadParamValue<float>("scope_nearFov", scope_nearFov);
		reader.ReadParamValue<float>("shoulderRotationAnimFactor", shoulderRotationAnimFactor);
		reader.ReadParamValue<float>("shoulderStrafeAnimFactor", shoulderStrafeAnimFactor);
		reader.ReadParamValue<float>("shoulderMovementAnimFactor", shoulderMovementAnimFactor);
		reader.ReadParamValue<float>("ironsightRotationAnimFactor", ironsightRotationAnimFactor);
		reader.ReadParamValue<float>("ironsightStrafeAnimFactor", ironsightStrafeAnimFactor);
		reader.ReadParamValue<float>("ironsightMovementAnimFactor", ironsightMovementAnimFactor);
		reader.ReadParamValue<float>("cameraShakeMultiplier", cameraShakeMultiplier);

		reader.ReadParamValue<float>("muzzle_flash_scale", muzzle_flash_scale);

		reader.ReadParamValue<Vec3>("fp_offset", fp_offset);

		Vec3 rot_offset(0.0f, 0.0f, 0.0f);
		if (reader.ReadParamValue<Vec3>("fp_rot_offset", rot_offset))
		{
			fp_rot_offset.SetRotationXYZ(Ang3(DEG2RAD(rot_offset.x), DEG2RAD(rot_offset.y), DEG2RAD(rot_offset.z)));
		}

		reader.ReadParamValue<bool>("hide_weapon", hide_weapon);
		reader.ReadParamValue<bool>("target_snap_enabled", target_snap_enabled);

		reader.ReadParamValue<bool>("hide_crosshair", hide_crosshair);
		reader.ReadParamValue<bool>("near_fov_sync", near_fov_sync);

		reader.ReadParamValue<bool>("armor_bonus_zoom", armor_bonus_zoom);

		XmlNodeRef stagesNode = reader.FindFilteredChild("stages");
		if (stagesNode)
		{
			CGameXmlParamReader stagesReader(stagesNode);

			stages.resize(0); 
			const int stagesCount = stagesReader.GetUnfilteredChildCount();
			stages.reserve(stagesCount);
			for (int i = 0; i < stagesCount; i++)
			{
				SStageParams stageParams;
				XmlNodeRef stageNode = stagesReader.GetFilteredChildAt(i);
				if (stageNode)
				{
					stageNode->getAttr("rotationSpeedScale" , stageParams.rotationSpeedScale);
					stageNode->getAttr("movementSpeedScale" , stageParams.movementSpeedScale);
					stageNode->getAttr("value", stageParams.stage);
					int useDof = 1;
					stageNode->getAttr("dof", useDof);
					stageParams.useDof = (useDof != 0);

					stages.push_back(stageParams);
				}
			}

			//Make sure there is always at least one stage, if data is incorrect the game could crash
			if (stages.empty())
			{
				stages.push_back(SStageParams());
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void SZoomSway::Reset( const XmlNodeRef& paramsNode, bool defaultInit /*= true*/ )
{
	if (defaultInit)
	{
		maxX = 0.0f;
		maxY = 0.0f;
		stabilizeTime = 3.0f;
		holdBreathScale = 0.1f;
		holdBreathTime = 1.0f;
		minScale = 0.15f;
		scaleAfterFiring = 0.5f;
		crouchScale = 0.8f;
	}

	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		reader.ReadParamValue<float>("maxX", maxX);
		reader.ReadParamValue<float>("maxY", maxY);
		reader.ReadParamValue<float>("stabilizeTime", stabilizeTime);
		reader.ReadParamValue<float>("holdBreathScale", holdBreathScale);
		reader.ReadParamValue<float>("holdBreathTime", holdBreathTime);
		reader.ReadParamValue<float>("minScale", minScale);
		reader.ReadParamValue<float>("scaleAfterFiring", scaleAfterFiring);
		reader.ReadParamValue<float>("crouchScale", crouchScale);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void SVehicleGuided::Reset( const XmlNodeRef& paramsNode, bool defaultInit /*= true*/ )
{
	if (defaultInit)
	{
		postStateWaitTime = 0.0f;
		waitWhileFiring = false;
	}
	if (paramsNode)
	{
		animationName	= paramsNode->getAttr("anim");
		preState		= paramsNode->getAttr("preState");
		postState		= paramsNode->getAttr("postState");
		
		CGameXmlParamReader reader(paramsNode);

		reader.ReadParamValue<float>("postStateWaitTime", postStateWaitTime);
		reader.ReadParamValue<bool>("waitWhileFiring", waitWhileFiring);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void SC4Params::Reset(const XmlNodeRef& paramsNode, bool defaultInit)
{
	if(defaultInit)
	{
		armedLightMatIndex = -1;
		armedLightGlowAmount = 0.f;
		disarmedLightGlowAmount = 0.f;
		armedLightColour = ColorF(0.f, 1.f, 0.f);
		disarmedLightColour = ColorF(1.f, 0.f, 0.f);
	}

	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		Vec3 armColour, disarmColour;

		reader.ReadParamValue<int>("armed_light_material_index", armedLightMatIndex);
		reader.ReadParamValue<float>("armed_light_glow_amount", armedLightGlowAmount);
		reader.ReadParamValue<float>("disarmed_light_glow_amount", disarmedLightGlowAmount);
		reader.ReadParamValue<Vec3>("armed_light_colour", armColour);
		reader.ReadParamValue<Vec3>("disarmed_light_colour", disarmColour);

		armedLightColour = ColorF(armColour.x, armColour.y, armColour.z);
		disarmedLightColour = ColorF(disarmColour.x, disarmColour.y, disarmColour.z);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void SScopeParams::Reset( const XmlNodeRef& paramsNode , bool defaultInit /*= true*/)
{
	if (defaultInit)
	{
		helper_name = "";
		crosshair_sub_material = "";
		dark_out_time = 0.025f;
		dark_in_time = 0.15f;
		blink_frequency = 12.0f;
		techvisor = false;
	}

	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		helper_name = reader.ReadParamValue("helper_name", helper_name.c_str());
		crosshair_sub_material = reader.ReadParamValue("crosshair_sub_material", crosshair_sub_material.c_str());
		reader.ReadParamValue<float>("dark_out_time", dark_out_time);
		reader.ReadParamValue<float>("dark_in_time", dark_in_time);
		reader.ReadParamValue<float>("blink_frequency", blink_frequency);
		reader.ReadParamValue("techvisor", techvisor);
	}
}
