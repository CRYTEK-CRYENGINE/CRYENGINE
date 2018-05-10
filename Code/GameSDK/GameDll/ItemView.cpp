// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 30:8:2005   12:52 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Item.h"
#include "Actor.h"
#include "Player.h"
#include "GameCVars.h"
#include <IViewSystem.h>
#include "ItemSharedParams.h"
#include "ReplayActor.h"


//------------------------------------------------------------------------
void CItem::UpdateFPView(float frameTime)
{ 
	if (m_stats.attachment == eIA_None && !m_stats.mounted)
		return;
	CheckViewChange();
}

//------------------------------------------------------------------------
bool CItem::FilterView(struct SViewParams &viewParams)
{
	return false;
}

//------------------------------------------------------------------------
void CItem::PostFilterView(struct SViewParams &viewParams)
{

}

//------------------------------------------------------------------------
bool CItem::IsOwnerFP()
{
	CActor *pOwner = GetOwnerActor();
	if (pOwner)
	{
		if (m_pGameFramework->GetClientActor() != pOwner)
			return false;

		return !pOwner->IsThirdPerson();
	}
	else if (gEnv->bMultiplayer)
	{
		CReplayActor* pReplayActor = CReplayActor::GetReplayActor(GetOwner());
		if (pReplayActor)
		{
			return !pReplayActor->IsThirdPerson();
		}
	}
	return false;
}

//------------------------------------------------------------------------
void CItem::UpdateMounted(float frameTime)
{
	CheckViewChange();
	
	if (CActor *pActor = GetOwnerActor())
	{
		SMovementState info;
		IMovementController* pMC = pActor->GetMovementController();		
		pMC->GetMovementState(info);
	
		Matrix34 tm = Matrix33::CreateRotationVDir(info.aimDirection.GetNormalized());
		Vec3 vGunXAxis = tm.GetColumn0();

		UpdateIKMounted(pActor, vGunXAxis*0.1f);
	}

	RequireUpdate(eIUS_General);  
}

//------------------------------------------------------------------------
void CItem::UpdateIKMounted(IActor* pActor, const Vec3& vGunXAxis)
{
	if (SMountParams* pMountParams = m_sharedparams->pMountParams)
	{
		if (!pMountParams->left_hand_helper.empty() && !pMountParams->right_hand_helper.empty())
		{ 
			const Vec3 lhpos = GetSlotHelperPos(eIGS_FirstPerson, pMountParams->left_hand_helper.c_str(), true);
			const Vec3 rhpos = GetSlotHelperPos(eIGS_FirstPerson, pMountParams->right_hand_helper.c_str(), true);
			pActor->SetIKPos("leftArm",		lhpos - vGunXAxis, 1);
			pActor->SetIKPos("rightArm",	rhpos + vGunXAxis, 1);

			//gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(lhpos, 0.075f, ColorB(255, 255, 255, 255));
			//gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(rhpos, 0.075f, ColorB(128, 128, 128, 255));
		}
	}
}

//------------------------------------------------------------------------


//------------------------------------------------------------------------
void CItem::CheckViewChange()
{
	bool firstPerson = IsOwnerFP();
  
  if (m_stats.mounted)
	{
    if (firstPerson!=m_stats.fp)
    {
      if (firstPerson)
				OnEnterFirstPerson();
			else
				OnEnterThirdPerson();
      //else if (!fp)
      //  AttachArms(false, false);
    }

    m_stats.fp = firstPerson;
 
		return;
	}

	if (firstPerson)
	{
		if (!m_stats.fp || !(m_stats.viewmode&eIVM_FirstPerson))
			OnEnterFirstPerson();
		m_stats.fp = true;
	}
	else
	{
		if (m_stats.fp || !(m_stats.viewmode&eIVM_ThirdPerson))
			OnEnterThirdPerson();
		m_stats.fp = false;
	}
}

//------------------------------------------------------------------------
void CItem::SetViewMode(int mode)
{
	m_stats.viewmode = mode;

	const int numAccessories = m_accessories.size();

	for (int i = 0; i < numAccessories; i++)
	{
		if (CItem* pItem = static_cast<CItem*>(m_pGameFramework->GetIItemSystem()->GetItem(m_accessories[i].accessoryId)))
		{
			pItem->SetViewMode(mode);
		}
	}

	if (mode & eIVM_FirstPerson)
	{
		SetHand(m_stats.hand);

		if (m_parentId)
		{
			DrawSlot(eIGS_FirstPerson, false, false);
		}
		else if (IsAttachedToBack())
		{
			//--- Remove back attachments on switch to 1P
			AttachToBack(false);
		}
		else if (IsAttachedToHand())
		{
			//--- Reattach to hand to switch model & binding style
			AttachToHand(true);
		}
	}
	else
	{
		SetGeometry(eIGS_FirstPerson, 0);
	}


	if (mode & eIVM_ThirdPerson)
	{
		//--- Reattach to hand to switch model & binding style
		if (IsAttachedToHand())
			AttachToHand(true);

		DrawSlot(eIGS_ThirdPerson, (m_stats.attachment != eIA_WeaponCharacter));
		if (!m_stats.mounted)
			CopyRenderFlags(GetOwner());
	}
	else
	{
		DrawSlot(eIGS_ThirdPerson, false);
		DrawSlot(eIGS_ThirdPersonAux, false);
	}

	AttachToShadowHand((mode&eIVM_FirstPerson) != 0);
}

//-----------------------------------------------------------------------
void CItem::AttachToShadowHand( bool attach )
{
	if (m_stats.mounted)
		return;

	//--- Shadow character for FP mode
	CActor *pOwner = GetOwnerActor();
	ICharacterInstance *pOwnerShadowCharacter = NULL;
	
	if (pOwner)
	{
		if (pOwner->IsPlayer())
		{
			CPlayer *ownerPlayer = (CPlayer*)pOwner;
			pOwnerShadowCharacter = ownerPlayer->GetShadowCharacter();
		}
	}
	else
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_owner.GetId());
		if (pEntity)
		{
			CReplayActor* pReplayActor = CReplayActor::GetReplayActor(pEntity);
			if (pReplayActor)
			{
				pOwnerShadowCharacter = pReplayActor->GetShadowCharacter();
			}
		}
	}

	if (pOwnerShadowCharacter == NULL)
		return;

	bool showShadowChar = g_pGameCVars->g_showShadowChar != 0;

	IAttachment *pAttachment = pOwnerShadowCharacter->GetIAttachmentManager()->GetInterfaceByName(m_sharedparams->params.attachment[m_stats.hand].c_str());

	if (pAttachment != NULL)
	{
		if (attach)
		{
			if (ICharacterInstance *pTPChar = GetEntity()->GetCharacter(eIGS_ThirdPerson))
			{
				CSKELAttachment *pChrAttachment = new CSKELAttachment();
				pChrAttachment->m_pCharInstance = pTPChar;
				pAttachment->AddBinding(pChrAttachment);
				pAttachment->HideAttachment(!showShadowChar);
				pAttachment->HideInShadow(0);
			}
			else if(IStatObj* pTPObject = GetEntity()->GetStatObj(eIGS_ThirdPerson))
			{
				CCGFAttachment* pCGFAttachment = new CCGFAttachment();
				pCGFAttachment->pObj = pTPObject;
				pAttachment->AddBinding(pCGFAttachment);
				pAttachment->HideAttachment(!showShadowChar);
				pAttachment->HideInShadow(0);
			}
		}
		else
		{
			if (m_stats.attachment == eIA_WeaponCharacter)
			{
				IAttachment *pAttachmentWeapon = GetOwnerAttachmentManager()->GetInterfaceByName(m_sharedparams->params.attachment[m_stats.hand].c_str());
				if (pAttachmentWeapon)
					pAttachmentWeapon->HideInShadow(false);
			}

			IAttachmentObject *pAttachmentObject = pAttachment->GetIAttachmentObject();
			if (pAttachmentObject)
			{
				bool isThisBinding = false;
				if (ICharacterInstance *pTPChar = GetEntity()->GetCharacter(eIGS_ThirdPerson))
				{
					isThisBinding = (pTPChar == pAttachmentObject->GetICharacterInstance());
				}
				else if (IStatObj* pTPObject = GetEntity()->GetStatObj(eIGS_ThirdPerson))
				{
					isThisBinding = (pTPObject == pAttachmentObject->GetIStatObj());
				}

				if (isThisBinding)
				{
					pAttachment->ClearBinding();
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CItem::CopyRenderFlags(IEntity *pOwner)
{
	if (!pOwner || !GetRenderProxy())
		return;

	{
		IEntityRender *pOwnerRenderProxy = (IEntityRender *)pOwner->GetRenderInterface();
		IRenderNode *pOwnerRenderNode = pOwnerRenderProxy?pOwnerRenderProxy->GetRenderNode():NULL;
		if (pOwnerRenderNode)
		{
			int viewDistRatio = pOwnerRenderNode->GetViewDistRatio();
			int lodRatio = pOwnerRenderNode->GetLodRatio();
			if (!gEnv->bMultiplayer)
			{
				viewDistRatio = (int)((float)viewDistRatio * g_pGameCVars->g_itemsViewDistanceRatioScale);
				lodRatio = (int)((float)lodRatio * g_pGameCVars->g_itemsLodRatioScale);
			}

			GetEntity()->SetViewDistRatio(viewDistRatio);
			GetEntity()->SetLodRatio(lodRatio);

			uint32 flags = pOwner->GetFlags()&(ENTITY_FLAG_CASTSHADOW);
			uint32 mflags = GetEntity()->GetFlags()&(~(ENTITY_FLAG_CASTSHADOW));
			GetEntity()->SetFlags(mflags|flags);
		}
	}
}


