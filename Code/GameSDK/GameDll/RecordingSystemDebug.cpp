// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RecordingSystemDebug.h"

#ifdef RECSYS_DEBUG
#include "RecordingSystem.h"


CRecordingSystemDebug::CRecordingSystemDebug(CRecordingSystem& system)
	: m_system(system)
{
	m_system.RegisterListener(*this);
}

CRecordingSystemDebug::~CRecordingSystemDebug()
{
	m_system.UnregisterListener(*this);
}

void CRecordingSystemDebug::PrintFirstPersonPacketData( uint8* data, size_t size, const char* const msg ) const
{
	if( g_pGameCVars->kc_debugPacketData )
	{
		RecSysLog(": FIRST PERSON KILL CAM DATA START [%s] :", msg);
		uint8* pPos = data;
		while (pPos < data+size)
		{
			SRecording_Packet& packet = *((SRecording_Packet*)pPos);
			PrintFirstPersonPacketData(packet);
			pPos += packet.size;
		}
		RecSysLog(": FIRST PERSON KILL CAM DATA END [%s] :", msg);
	}
}

void CRecordingSystemDebug::PrintFirstPersonPacketData( CRecordingBuffer& buffer, const char* const msg ) const
{
	if( g_pGameCVars->kc_debugPacketData )
	{
		RecSysLog(": FIRST PERSON KILL CAM DATA START [%s] :", msg);
		int size = (int)buffer.size();
		int offset = 0;
		while( offset < size )
		{
			const SRecording_Packet& packet = *((SRecording_Packet*)buffer.at(offset));
			PrintFirstPersonPacketData(packet);
			offset += packet.size;
		}
		RecSysLog(": FIRST PERSON KILL CAM DATA END [%s] :", msg);
	}
}

void CRecordingSystemDebug::PrintThirdPersonPacketData( uint8* data, size_t size, const char* const msg ) const
{
	if( g_pGameCVars->kc_debugPacketData )
	{
		RecSysLog(": THIRD PERSON KILL CAM DATA START [%s] :", msg);
		uint8* pPos = data;
		float frameTime = 0.f;
		while( pPos < data+size )
		{
			const SRecording_Packet& packet = *((SRecording_Packet*)pPos);
			PrintThirdPersonPacketData(packet,frameTime);
			pPos += packet.size;
		}
		RecSysLog(": THIRD PERSON KILL CAM DATA END [%s] :", msg);
	}
}

void CRecordingSystemDebug::PrintThirdPersonPacketData( CRecordingBuffer& buffer, const char* const msg ) const
{
	if( g_pGameCVars->kc_debugPacketData )
	{
		RecSysLog(": THIRD PERSON KILL CAM DATA START [%s] :", msg);
		int size = (int)buffer.size();
		int offset = 0;
		float frameTime = 0.f;
		while( offset < size )
		{
			const SRecording_Packet& packet = *((SRecording_Packet*)buffer.at(offset));
			PrintThirdPersonPacketData(packet,frameTime);
			offset += packet.size;
		}
		RecSysLog(": THIRD PERSON KILL CAM DATA END [%s] :", msg);
	}
}

void CRecordingSystemDebug::PrintFirstPersonPacketData( const SRecording_Packet& packet ) const
{
	switch(packet.type)
	{
	case eFPP_FPChar:
		{
			const SRecording_FPChar& rFPChar = (const SRecording_FPChar&)packet;
			const Vec3 playerpos(rFPChar.relativePosition.t+rFPChar.camlocation.t);
			RecSysLog("[%4.4f] [FPChar] CAMPOS[%.3f,%.3f,%.3f] CAMROT[%.3f,%.3f,%.3f,%.3f] FOV[%.2f] PLAYERPOS[%.3f,%.3f,%.3f] FLAGS[%s|%s|%s|%s|%s|%s]"
				, rFPChar.frametime
				, rFPChar.camlocation.t.x, rFPChar.camlocation.t.y, rFPChar.camlocation.t.z
				, rFPChar.camlocation.q.v.x, rFPChar.camlocation.q.v.y, rFPChar.camlocation.q.v.z, rFPChar.camlocation.q.w
				, rFPChar.fov
				, playerpos.x, playerpos.y, playerpos.z
				, (rFPChar.playerFlags&eFPF_OnGround)?"GRND":""
				, (rFPChar.playerFlags&eFPF_Sprinting)?"SPNT":""
				, (rFPChar.playerFlags&eFPF_StartZoom)?"ZOOM":""
				, (rFPChar.playerFlags&eFPF_ThirdPerson)?"THRD":""
				, (rFPChar.playerFlags&eFPF_FiredShot)?"FIRE":""
				, (rFPChar.playerFlags&eFPF_NightVision)?"NGHT":""
				);
		}
		break;
	case eFPP_Flashed:
		{
			const SRecording_Flashed& rFlashed = (const SRecording_Flashed&)packet;
			RecSysLog("[%4.4f] [Flashd] DURATION[%.3f] BLIND[%.3f]"
				, rFlashed.frametime
				, rFlashed.duration
				, rFlashed.blindAmount
				);
		}
		break;
	case eFPP_VictimPosition:
		{
			const SRecording_VictimPosition& rVictim = (const SRecording_VictimPosition&)packet;
			RecSysLog("[%4.4f] [Victim] POS[%.3f,%.3f,%.3f]"
				, rVictim.frametime
				, rVictim.victimPosition.x, rVictim.victimPosition.y, rVictim.victimPosition.z
				);
		}
		break;
	case eFPP_KillHitPosition:
		{
			const SRecording_KillHitPosition& rKillHit = (const SRecording_KillHitPosition&)packet;
			RecSysLog("[xxx.xxxx] [KilHit] VICTIMID[%x] HITRELPOS[%.3f,%.3f,%.3f]"
				, rKillHit.victimId
				, rKillHit.hitRelativePos.x, rKillHit.hitRelativePos.y, rKillHit.hitRelativePos.z
				);
		}
		break;
	case eFPP_BattleChatter:
		{
			const SRecording_BattleChatter& rBatChat = (const SRecording_BattleChatter&)packet;
			RecSysLog("[%4.4f] [Chattr] TYPE[%d] CHTRVAR[%d] NETID[%x] ENTID[%x]"
				, rBatChat.frametime
				, rBatChat.chatterType
				, rBatChat.chatterVariation
				, rBatChat.entityNetId
				, CRecordingSystem::NetIdToEntityId(rBatChat.entityNetId)
				);
		}
		break;
	case eFPP_RenderNearest:
		{
			const SRecording_RenderNearest& rRenderNearest = (const SRecording_RenderNearest&)packet;
			RecSysLog("[%4.4f] [RdNear] RENDERNEAREST[%s]"
				, rRenderNearest.frametime
				, rRenderNearest.renderNearest ? "TRUE" : "FALSE"
				);
		}
		break;
	case eFPP_PlayerHealthEffect:
		{
			const SRecording_PlayerHealthEffect& rHealthEffect = (const SRecording_PlayerHealthEffect&)packet;
			RecSysLog("[%4.4f] [HlthFX] STR[%.3f] SPD[%.3f] DIR[%.3f,%.3f,%.3f]"
				, rHealthEffect.frametime
				, rHealthEffect.hitStrength
				, rHealthEffect.hitSpeed
				, rHealthEffect.hitDirection.x, rHealthEffect.hitDirection.y, rHealthEffect.hitDirection.z
				);
		}
		break;
	case eFPP_PlaybackTimeOffset:
		{
			const SRecording_PlaybackTimeOffset& rTimeOffset = (const SRecording_PlaybackTimeOffset&)packet;
			RecSysLog("[xxx.xxxx] [PbTmOf] TIMEOFFSET[%.3f]"
				, rTimeOffset.timeOffset
				);
		}
		break;
	}
}

void CRecordingSystemDebug::PrintThirdPersonPacketData( const SRecording_Packet& packet, float& frameTime ) const
{
	//RecSysLog("%d",packet.type);
	switch(packet.type)
	{
	case eRBPT_FrameData:
		{
			const SRecording_FrameData& rFrame = (const SRecording_FrameData&)packet;
			frameTime = rFrame.frametime;
		}
		break;
	case eTPP_TPChar:
		{
			const SRecording_TPChar& rTPChar = (const SRecording_TPChar&)packet;
			RecSysLog("[%4.4f] [TPChar] ENTID[%x] ENTLOC[%.3f,%.3f,%.3f] ENTDIR[%.3f,%.3f,%.3f,%.3f] VEL[%.3f,%.3f,%.3f] AIMDIR[%.3f,%.3f,%.3f] HLTH[%d] LYRFX[%x] FLAGS[%s|%s|%s|%s|%s]", frameTime
				, rTPChar.eid
				, rTPChar.entitylocation.t.x, rTPChar.entitylocation.t.y, rTPChar.entitylocation.t.z
				, rTPChar.entitylocation.q.v.x, rTPChar.entitylocation.q.v.y, rTPChar.entitylocation.q.v.z, rTPChar.entitylocation.q.w
				, rTPChar.velocity.x, rTPChar.velocity.y, rTPChar.velocity.z
				, rTPChar.aimdir.x, rTPChar.aimdir.y, rTPChar.aimdir.z
				, rTPChar.healthPercentage
				, rTPChar.layerEffectParams
				, (rTPChar.playerFlags&eTPF_Dead)?"DEAD":""
				, (rTPChar.playerFlags&eTPF_Cloaked)?"CLKD":""
				, (rTPChar.playerFlags&eTPF_AimIk)?"AMIK":""
				, (rTPChar.playerFlags&eTPF_OnGround)?"GRND":""
				, (rTPChar.playerFlags&eTPF_Invisible)?"INVI":""
				);
		}
		break;
	case eTPP_SpawnCustomParticle:
		{
			const SRecording_SpawnCustomParticle& rSpawn = (const SRecording_SpawnCustomParticle&)packet;
			const Vec3 pos(rSpawn.location.GetTranslation());
			RecSysLog("[%4.4f] [ParSpC] NAME[%s] POS[%.3f,%.3f,%.3f]", frameTime
				, rSpawn.pParticleEffect ? rSpawn.pParticleEffect->GetName() : "UNKOWN_PARTICLE_EFFECT"
				, pos.x, pos.y, pos.z
				);
		}
		break;
	case eTPP_ParticleCreated:
		{
			const SRecording_ParticleCreated& rCreate = (const SRecording_ParticleCreated&)packet;
			RecSysLog("[%4.4f] [ParCre] NAME[%s] POS[%.3f,%.3f,%.3f]", frameTime
				, rCreate.pParticleEffect ? rCreate.pParticleEffect->GetName() : "UNKOWN_PARTICLE_EFFECT"
				, rCreate.location.t.x, rCreate.location.t.y, rCreate.location.t.z
				);
		}
		break;
	case eTPP_ParticleDeleted:
		{
			//const SRecording_ParticleDeleted& rDeleted = (const SRecording_ParticleDeleted&)packet;
			RecSysLog("[%4.4f] [ParDel]", frameTime
				);
		}
		break;
	case eTPP_ParticleLocation:
		{
			//const SRecording_ParticleLocation& rParticleLocation = (const SRecording_ParticleLocation&)packet;
			RecSysLog("[%4.4f] [ParLoc]", frameTime 
				);
		}
		break;
	case eTPP_ParticleTarget:
		{
			//const SRecording_ParticleTarget& rParticleTarget = (const SRecording_ParticleTarget&)packet;
			RecSysLog("[%4.4f] [ParTrg]", frameTime 
				);
		}
		break;
	case eTPP_WeaponAccessories:
		{
			const SRecording_WeaponAccessories& rWeaponAcc = (const SRecording_WeaponAccessories&)packet;
			RecSysLog("[%4.4f] [WpnAcc] WPNID[%x]", frameTime
				, rWeaponAcc.weaponId
				);
		}
		break;
	case eTPP_WeaponSelect:
		{
			const SRecording_WeaponSelect& rWeaponSel = (const SRecording_WeaponSelect&)packet;
			RecSysLog("[%4.4f] [WpnSel] ONWERID[%x] WPNID[%x] SELECTED[%s] RIPPEDOFF[%s]", frameTime 
				, rWeaponSel.ownerId
				, rWeaponSel.weaponId
				, rWeaponSel.isSelected ? "TRUE" : "FALSE"
				, rWeaponSel.isRippedOff ? "TRUE" : "FALSE"
				);
		}
		break;
	case eTPP_FiremodeChanged:
		{
			const SRecording_FiremodeChanged& rFiremodeChanged = (const SRecording_FiremodeChanged&)packet;
			RecSysLog("[%4.4f] [FMCh] ONWERID[%x] WPNID[%x] FIREMODE[%x]", frameTime 
				, rFiremodeChanged.ownerId
				, rFiremodeChanged.weaponId
				, rFiremodeChanged.firemode
				);
		}
		break;
	case eTPP_MannEvent:
		{
			const SRecording_MannEvent& rMannEvent = (const SRecording_MannEvent&)packet;

			stack_string tagnames;
			const SControllerDef* pControllerDef = gEnv->pGameFramework->GetMannequinInterface().GetAnimationDatabaseManager().LoadControllerDef("Animations/Mannequin/ADB/PlayerControllerDefs.xml");
			pControllerDef->m_tags.FlagsToTagList(rMannEvent.historyItem.tagState, tagnames);

			if(rMannEvent.historyItem.type==SMannHistoryItem::Fragment)
			{
				RecSysLog("[%4.4f] [MnEvnt] ENTID[%x] TYPE[FRAGMENT] TIME[%4.4f] FRAG[%s] FRTAG[%s] SCOPEMASK[%x] OPTIONIDX[%d]", frameTime
					, rMannEvent.eid
					, rMannEvent.historyItem.time
					, pControllerDef->m_fragmentIDs.GetTagName(rMannEvent.historyItem.fragment)
					, tagnames.c_str()
					, rMannEvent.historyItem.scopeMask
					, rMannEvent.historyItem.optionIdx
					);
			}
			else if(rMannEvent.historyItem.type==SMannHistoryItem::Tag)
			{
				RecSysLog("[%4.4f] [MnEvnt] ENTID[%x] TYPE[TAG] TIME[%4.4f] TAG[%s]", frameTime
					, rMannEvent.eid
					, rMannEvent.historyItem.time
					, tagnames.c_str()
					);
			}
		}
		break;
	case eTPP_MannSetSlaveController:
		{
			//const SRecording_MannSetSlaveController& rMannSetSlaveCont = (const SRecording_MannSetSlaveController&)packet;
			RecSysLog("[%4.4f] [MnSlCt]", frameTime 
				);
		}
		break;
	case eTPP_MannSetParam:
		{
			//const SRecording_MannSetParam& rMannSetParam = (const SRecording_MannSetParam&)packet;
			RecSysLog("[%4.4f] [MnSPrm]", frameTime 
				);
		}
		break;
	case eTPP_MannSetParamFloat:
		{
			//const SRecording_MannSetParamFloat& rMannSetParam = (const SRecording_MannSetParamFloat&)packet;
			RecSysLog("[%4.4f] [MnSPrF]", frameTime 
				);
		}
		break;
	case eTPP_MountedGunAnimation:
		{
			//const SRecording_MountedGunAnimation& rMGAnim = (const SRecording_MountedGunAnimation&)packet;
			RecSysLog("[%4.4f] [MGunAn]", frameTime 
				);
		}
		break;
	case eTPP_MountedGunRotate:
		{
			//const SRecording_MountedGunRotate& rMGRot = (const SRecording_MountedGunRotate&)packet;
			RecSysLog("[%4.4f] [MGunRo]", frameTime 
				);
		}
		break;
	case eTPP_MountedGunEnter:
		{
			//const SRecording_MountedGunEnter& rMGEnter = (const SRecording_MountedGunEnter&)packet;
			RecSysLog("[%4.4f] [MGunEn]", frameTime 
				);
		}
		break;
	case eTPP_MountedGunLeave:
		{
			//const SRecording_MountedGunLeave& rMGExit = (const SRecording_MountedGunLeave&)packet;
			RecSysLog("[%4.4f] [MGunEx]", frameTime 
				);
		}
		break;
	case eTPP_OnShoot:
		{
			const SRecording_OnShoot& rOnShoot = (const SRecording_OnShoot&)packet;
			RecSysLog("[%4.4f] [OnShot] WPNID[%x]", frameTime
				, rOnShoot.weaponId
				);
		}
		break;
	case eTPP_EntitySpawn:
		{
			const SRecording_EntitySpawn& rEntSpawned = (const SRecording_EntitySpawn&)packet;
			RecSysLog("[%4.4f] [EntSpa] ENTID[%x] CLASS[%s] POS[%.3f,%.3f,%.3f]", frameTime
				, rEntSpawned.entityId
				, rEntSpawned.pClass ? rEntSpawned.pClass->GetName() : "No Class Type"
				, rEntSpawned.entitylocation.t.x, rEntSpawned.entitylocation.t.y, rEntSpawned.entitylocation.t.z
				);
		}
		break;
	case eTPP_EntityRemoved:
		{
			const SRecording_EntityRemoved& rEntRemoved = (const SRecording_EntityRemoved&)packet;
			RecSysLog("[%4.4f] [EntRem] ENTID[%x]", frameTime
				, rEntRemoved.entityId
				);
		}
		break;
	case eTPP_EntityLocation:
		{
			const SRecording_EntityLocation& rEntLoc = (const SRecording_EntityLocation&)packet;
			RecSysLog("[%4.4f] [EntLoc] ENTID[%x] POS[%.3f,%.3f,%.3f]", frameTime 
				, rEntLoc.entityId
				, rEntLoc.entitylocation.t.x, rEntLoc.entitylocation.t.y, rEntLoc.entitylocation.t.z
				);
		}
		break;
	case eTPP_EntityHide:
		{
			const SRecording_EntityHide& rEntHide = (const SRecording_EntityHide&)packet;
			RecSysLog("[%4.4f] [EntHid] ENTID[%x] HIDE[%s]", frameTime 
				, rEntHide.entityId
				, rEntHide.hide ? "TRUE" : "FALSE"
				);
		}
		break;
	case eTPP_PlaySound:
		{
			REINST("do we still need this?");
			/*const SRecording_PlaySound& rPlaySound = (const SRecording_PlaySound&)packet;
			RecSysLog("[%4.4f] [PlySnd] NAME[%s] ENTID[%x] SNDID[%d]", frameTime 
				, rPlaySound.szName
				, rPlaySound.entityId
				, rPlaySound.soundId
				);*/
		}
		break;
	case eTPP_StopSound:
		{
			REINST("do we still need this?");
			/*const SRecording_StopSound& rStopSound = (const SRecording_StopSound&)packet;
			RecSysLog("[%4.4f] [StpSnd] SNDID[%d]", frameTime 
				, rStopSound.soundId
				);*/
		}
		break;
	case eTPP_SoundParameter:
		{
			REINST("do we still need this?");
			/*const SRecording_SoundParameter& rSoundParam = (const SRecording_SoundParameter&)packet;
			RecSysLog("[%4.4f] [SndPrm] SNDID[%d] INDEX[%d] VAL[%.3f]", frameTime 
				, rSoundParam.soundId
				, rSoundParam.index
				, rSoundParam.value
				);*/
		}
		break;
	case eTPP_BulletTrail:
		{
			const SRecording_BulletTrail& rBulletTrail = (const SRecording_BulletTrail&)packet;
			RecSysLog("[%4.4f] [BltTrl] FRIENDLY[%s] FROM[%.3f,%.3f,%.3f] TO[%.3f,%.3f,%.3f]", frameTime 
				, rBulletTrail.friendly ? "TRUE" : "FALSE"
				, rBulletTrail.start.x, rBulletTrail.start.y, rBulletTrail.start.z
				, rBulletTrail.end.x, rBulletTrail.end.y, rBulletTrail.end.z
				);
		}
		break;
	case eTPP_ProceduralBreakHappened:
		{
			const SRecording_ProceduralBreakHappened& rProcBreak = (const SRecording_ProceduralBreakHappened&)packet;
			RecSysLog("[%4.4f] [PrcBrk]", frameTime 
				);
		}
		break;
	case eTPP_DrawSlotChange:
		{
			const SRecording_DrawSlotChange& rDrawSlotChange = (const SRecording_DrawSlotChange&)packet;
			RecSysLog("[%4.4f] [DrSlCh]", frameTime 
				);
		}
		break;
	case eTPP_StatObjChange:
		{
			const SRecording_StatObjChange& rStatObjChange = (const SRecording_StatObjChange&)packet;
			RecSysLog("[%4.4f] [StObCh]", frameTime 
				);
		}
		break;
	case eTPP_SubObjHideMask:
		{
			const SRecording_SubObjHideMask& rSubObjHideMask = (const SRecording_SubObjHideMask&)packet;
			RecSysLog("[%4.4f] [SbObHi]", frameTime 
				);
		}
		break;
	case eTPP_TeamChange:
		{
			const SRecording_TeamChange& rTeamChange = (const SRecording_TeamChange&)packet;
			RecSysLog("[%4.4f] [TeamCh]", frameTime 
				);
		}
		break;
	case eTPP_ItemSwitchHand:
		{
			const SRecording_ItemSwitchHand& rItemSwitchHand = (const SRecording_ItemSwitchHand&)packet;
			RecSysLog("[%4.4f] [ItmHnd]", frameTime 
				);
		}
		break;
	case eTPP_EntityAttached:
		{
			const SRecording_EntityAttached& rEntityAttached = (const SRecording_EntityAttached&)packet;
			RecSysLog("[%4.4f] [EntAtt]", frameTime 
				);
		}
		break;
	case eTPP_EntityDetached:
		{
			const SRecording_EntityDetached& rEntityDetached = (const SRecording_EntityDetached&)packet;
			RecSysLog("[%4.4f] [EntDet]", frameTime 
				);
		}
		break;
	case eTPP_PlayerJoined:
		{
			const SRecording_PlayerJoined& rPlayerJoined = (const SRecording_PlayerJoined&)packet;
			RecSysLog("[%4.4f] [PlrJnd]", frameTime 
				);
		}
		break;
	case eTPP_PlayerLeft:
		{
			const SRecording_PlayerLeft& rPlayerLeft = (const SRecording_PlayerLeft&)packet;
			RecSysLog("[%4.4f] [PlrLft]", frameTime 
				);
		}
		break;
	case eTPP_PlayerChangedModel:
		{
			const SRecording_PlayerChangedModel& rPlayerChangedModel = (const SRecording_PlayerChangedModel&)packet;
			RecSysLog("[%4.4f] [PlChMd]", frameTime 
				);
		}
		break;
	case eTPP_CorpseSpawned:
		{
			const SRecording_CorpseSpawned& rCorpseSpawned = (const SRecording_CorpseSpawned&)packet;
			RecSysLog("[%4.4f] [CrpsSp]", frameTime 
				);
		}
		break;
	case eTPP_CorpseRemoved:
		{
			const SRecording_CorpseRemoved& rCorpseRemoved = (const SRecording_CorpseRemoved&)packet;
			RecSysLog("[%4.4f] [CrpsRm]", frameTime 
				);
		}
		break;
	case eTPP_AnimObjectUpdated:
		{
			const SRecording_AnimObjectUpdated& rAnimObjectUpdate = (const SRecording_AnimObjectUpdated&)packet;
			RecSysLog("[%4.4f] [AnObUp]", frameTime 
				);
		}
		break;
	case eTPP_ObjectCloakSync:
		{
			const SRecording_ObjectCloakSync& rObjectCloakSync = (const SRecording_ObjectCloakSync&)packet;
			RecSysLog("[%4.4f] [OClkSy]", frameTime 
				);
		}
		break;
	case eTPP_PickAndThrowUsed:
		{
			const SRecording_PickAndThrowUsed& rPickAndThrowUsed = (const SRecording_PickAndThrowUsed&)packet;
			RecSysLog("[%4.4f] [PkThrw]", frameTime 
				);
		}
		break;
	case eTPP_InteractiveObjectFinishedUse:
		{
			const SRecording_InteractiveObjectFinishedUse& rInteractiveObjectFinishedUse = (const SRecording_InteractiveObjectFinishedUse&)packet;
			RecSysLog("[%4.4f] [IOFinU]", frameTime 
				);
		}
		break;
	case eTPP_ForcedRagdollAndImpulse:
		{
			const SRecording_ForcedRagdollAndImpulse& rForcedRagdollAndImpulse = (const SRecording_ForcedRagdollAndImpulse&)packet;
			RecSysLog("[%4.4f] [FrRgIm]", frameTime 
				);
		}
		break;
	case eTPP_RagdollImpulse:
		{
			const SRecording_RagdollImpulse& rRagdollImpulse = (const SRecording_RagdollImpulse&)packet;
			RecSysLog("[%4.4f] [RagImp]", frameTime 
				);
		}
		break;
	}
}

#endif //RECSYS_DEBUG
