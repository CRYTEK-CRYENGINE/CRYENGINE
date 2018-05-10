// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "HUDUtils.h"
#include "UI/UIManager.h"
#include "UI/UICVars.h"
#include "UI/Utils/ScreenLayoutManager.h"

#include <CryString/StringUtils.h>
#include <CryString/StringUtils.h>
#include <CrySystem/Scaleform/IFlashPlayer.h>

#include "GameRules.h"
#include "GameRulesModules/IGameRulesRoundsModule.h"

#include "Network/Lobby/GameLobby.h"
#include "Network/Squad/SquadManager.h"

#include "Player.h"


static const float MIN_ASPECT_RATIO = (16.0f / 9.0f);

namespace CHUDUtils
{
////////////////////////////////////////////////////////////////////////
static const ColorF s_hUDColor(0.6015625f, 0.83203125f, 0.71484375f, 1.0f);

static const char* g_subtitleCharacters[] =
{
	"PSYCHO",
	"RASCH",
	"PROPHET",
	"CLAIRE",
	"NAX",
	"HIVEMIND",
	"SUIT_VOICE",
};

static const size_t g_subtitleCharactersCount = (CRY_ARRAY_COUNT(g_subtitleCharacters) );

static const char* g_subtitleColors[] =
{
	"92D050", //PSYCHO
	"00B0F0", //RASCH
	"8F68AC", //PROPHET
	"FFCCFF", //CLAIRE
	"F79646", //NAX
	"FF0000", //HIVEMIND
	"FFFF00", //SUIT_VOICE
};


const size_t GetNumSubtitleCharacters()
{
	return g_subtitleCharactersCount;
}

const char* GetSubtitleCharacter(const size_t index)
{
	assert(index>=0 && index<g_subtitleCharactersCount);
	return g_subtitleCharacters[index];
}

const char* GetSubtitleColor(const size_t index)
{
	assert(index>=0 && index<g_subtitleCharactersCount);
	return g_subtitleColors[index];
}



//////////////////////////////////////////////////////////////////////////
void LocalizeString( string &out, const char *text, const char *arg1, const char *arg2, const char *arg3, const char *arg4)
{
#if ENABLE_HUD_EXTRA_DEBUG
	const int numberOfWs = g_pGame->GetHUD()->GetCVars()->hud_localize_ws_instead;
	if( numberOfWs>0 )
	{
		static int lastNumberOfWs=0;
		if( lastNumberOfWs!=numberOfWs )
		{
			for(int i=0; i<numberOfWs; i++)
			{
				out.append("W");
			}

			lastNumberOfWs = numberOfWs;
		}
		return;
	}
#endif

	if(!text)
	{
		out = "";
		return;
	}

	string localizedString, param1, param2, param3, param4;
	ILocalizationManager* pLocMgr = gEnv->pSystem->GetLocalizationManager();

	if(text[0]=='@')
		pLocMgr->LocalizeString(text, localizedString);
	else
		localizedString = text;

	if(arg1)
	{
		if(arg1[0]=='@')
			pLocMgr->LocalizeString(arg1, param1);
		else
			param1 = arg1;
	}

	if(arg2)
	{
		if(arg2[0]=='@')
			pLocMgr->LocalizeString(arg2, param2);
		else
			param2 = arg2;
	}

	if(arg3)
	{
		if(arg3[0]=='@')
			pLocMgr->LocalizeString(arg3, param3);
		else
			param3 = arg3;
	}

	if(arg4)
	{
		if(arg4[0]=='@')
			pLocMgr->LocalizeString(arg4, param4);
		else
			param4 = arg4;
	}

	out.resize(0);
	pLocMgr->FormatStringMessage(out, localizedString, param1.c_str(), param2.c_str(), param3.c_str(), param4.c_str());
}
//////////////////////////////////////////////////////////////////////////
const char * LocalizeString( const char *text, const char *arg1, const char *arg2, const char *arg3, const char *arg4 )
{
	static string charstr;
	LocalizeString( charstr, text, arg1, arg2, arg3, arg4 );

	return charstr.c_str();
}
//////////////////////////////////////////////////////////////////////////
void LocalizeStringn( char* dest, size_t bufferSizeInBytes, const char *text, const char *arg1 /*= NULL*/, const char *arg2 /*= NULL*/, const char *arg3 /*= NULL*/, const char *arg4 /*= NULL*/ )
{
	cry_strcpy( dest, bufferSizeInBytes, LocalizeString(text, arg1, arg2, arg3, arg4) );
}
//////////////////////////////////////////////////////////////////////////
const char* LocalizeNumber(const int number)
{
	ILocalizationManager* pLocMgr = gEnv->pSystem->GetLocalizationManager();

	static string charstr;
	pLocMgr->LocalizeNumber(number, charstr);

	return charstr.c_str();
}
//////////////////////////////////////////////////////////////////////////
void LocalizeNumber(string& out, const int number)
{
	ILocalizationManager* pLocMgr = gEnv->pSystem->GetLocalizationManager();

	pLocMgr->LocalizeNumber(number, out);
}
//////////////////////////////////////////////////////////////////////////
void LocalizeNumbern(char* dest, size_t bufferSizeInBytes, const int number)
{
	cry_strcpy(dest, bufferSizeInBytes, LocalizeNumber(number));
}
//////////////////////////////////////////////////////////////////////////
const char* LocalizeNumber(const float number, int decimals)
{
	ILocalizationManager* pLocMgr = gEnv->pSystem->GetLocalizationManager();

	static string charstr;
	pLocMgr->LocalizeNumber(number, decimals, charstr);

	return charstr.c_str();
}
//////////////////////////////////////////////////////////////////////////
void LocalizeNumber(string& out, const float number, int decimals)
{
	ILocalizationManager* pLocMgr = gEnv->pSystem->GetLocalizationManager();

	pLocMgr->LocalizeNumber(number, decimals, out);
}
//////////////////////////////////////////////////////////////////////////
void LocalizeNumbern(char* dest, size_t bufferSizeInBytes, const float number, int decimals)
{
	cry_strcpy(dest, bufferSizeInBytes, LocalizeNumber(number, decimals));
}
//////////////////////////////////////////////////////////////////////////

void ConvertSecondsToTimerString( const int s, string* in_out_string, const bool stripZeroElements/*=false*/, bool keepMinutes/*=false*/, const char* const hex_colour/*=NULL*/ )
{
	int hours=0, mins=0, secs=0;
	secs = s;
	hours = (int)floor(((float)secs)*(1/60.0f)*(1/60.0f));
	secs -= hours*60*60;
	mins = (int)floor(((float)secs)*(1/60.0f));
	secs -= mins*60;
	string& l_time = (*in_out_string);
	if (stripZeroElements)
	{
		if (hours > 0)
		{
			l_time.Format( "%.2d:%.2d:%.2d", hours, mins, secs );
		}
		else if (mins > 0 || keepMinutes )
		{
			l_time.Format( "%.2d:%.2d", mins, secs );
		}
		else
		{
			l_time.Format( "%.2d", secs );
		}
	}
	else
	{
		l_time.Format( "%.2d:%.2d:%.2d", hours, mins, secs );
	}

	if (hex_colour)
	{
		string formatted_time;
		formatted_time.Format("<FONT color=\"%s\">%s</FONT>", hex_colour, l_time.c_str());
		l_time = formatted_time;
	}
	
	return;
}

////////////////////////////////////////////////////////////////////////

const char* GetFriendlyStateColour( EFriendState friendState )
{
	if (CUICVars* pHUDCvars = g_pGame->GetUI()->GetCVars())
	{
		switch(friendState)
		{
		case eFS_Friendly:
			return pHUDCvars->hud_colour_friend->GetString();
		case eFS_Enemy:
			return pHUDCvars->hud_colour_enemy->GetString();
		case eFS_LocalPlayer:
			return pHUDCvars->hud_colour_localclient->GetString();
		case eFS_Squaddie:
			return pHUDCvars->hud_colour_squaddie->GetString();
		case eFS_Server:
			return pHUDCvars->hud_colour_localclient->GetString();
		};
	}

	return "FFFFFF";
}

const EFriendState GetFriendlyState( const EntityId entityId, CActor* pLocalActor )
{
	if( pLocalActor )
	{
		if (pLocalActor->GetEntityId()==entityId)
		{
			return eFS_LocalPlayer;
		}
		else if( pLocalActor->IsFriendlyEntity( entityId ) )
		{
			if(CGameLobby* pLobby = g_pGame->GetGameLobby())
			{
				if(CSquadManager* pSM = g_pGame->GetSquadManager())
				{
					IActor* pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(entityId);
					if(pActor)
					{
						uint16 channelId = pActor->GetChannelId();
						CryUserID userId = pLobby->GetUserIDFromChannelID(channelId);
						const bool isSquadMember = pSM->IsSquadMateByUserId( userId );
						if (isSquadMember)
						{
							return eFS_Squaddie;
						}
					}
				}
			}

			return eFS_Friendly;
		}
		else
		{
			return eFS_Enemy;
		}
	}

	return eFS_Unknown;
}

IFlashPlayer* GetFlashPlayerFromMaterial( IMaterial* pMaterial, bool bGetTemporary )
{
	IFlashPlayer* pRetFlashPlayer(NULL);
	const SShaderItem& shaderItem(pMaterial->GetShaderItem());
	if (shaderItem.m_pShaderResources)
	{
		SEfResTexture* pTex = shaderItem.m_pShaderResources->GetTexture(EEfResTextures(0));
		if (pTex)
		{
			IDynTextureSource* pDynTexSrc = pTex->m_Sampler.m_pDynTexSource;
			if (pDynTexSrc)
			{
				if (bGetTemporary)
				{
					pRetFlashPlayer = (IFlashPlayer*) pDynTexSrc->GetSourceTemp(IDynTextureSource::DTS_I_FLASHPLAYER);
				}
				else
				{
					pRetFlashPlayer = (IFlashPlayer*) pDynTexSrc->GetSourcePerm(IDynTextureSource::DTS_I_FLASHPLAYER);
				}
			}
		}
	}

	return pRetFlashPlayer;
}

// assume the cgf is loaded at slot 0
// assume that each cgf has only one dynamic flash material
// assume that the dynamic texture is texture 0 on teh shader
IFlashPlayer* GetFlashPlayerFromCgfMaterial( IEntity* pCgfEntity, bool bGetTemporary/*, const char* materialName*/ )
{
	IFlashPlayer* pRetFlashPlayer(NULL);

	// First get the Material from slot 0 on the entity
	assert(pCgfEntity); // should never be NULL

	//IMaterial* pSrcMat = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial( materialName, false );

	IStatObj* pStatObj = pCgfEntity->GetStatObj(0);	

	//const int subObjCount = pStatObj->GetSubObjectCount();

	//for( int subObjId=0; subObjId<subObjCount; subObjId++ )
	{
		//IStatObj::SSubObject* pSubObject = pStatObj->GetSubObject( subObjId );
		//IStatObj* pSubStatObj = pSubObject->pStatObj;

		IMaterial* pMaterial = pStatObj->GetMaterial();
		if (!pMaterial)
		{
			GameWarning( "HUD: Static object '%s' does not have a material!", pStatObj->GetGeoName() );
			return NULL;
		}

		pRetFlashPlayer = CHUDUtils::GetFlashPlayerFromMaterialIncludingSubMaterials(pMaterial, bGetTemporary);
	}
	return pRetFlashPlayer;
}


IFlashPlayer* GetFlashPlayerFromMaterialIncludingSubMaterials( IMaterial* pMaterial, bool bGetTemporary )
{
	IFlashPlayer* pRetFlashPlayer = CHUDUtils::GetFlashPlayerFromMaterial( pMaterial, bGetTemporary );

	const int subMtlCount = pMaterial->GetSubMtlCount();
	for (int i = 0; i != subMtlCount; ++i)
	{
		IMaterial* pSubMat = pMaterial->GetSubMtl(i);
		if (!pSubMat)
		{
			GameWarning( "HUD: Failed to get unified asset 3D submaterial #%d.", i );
			continue;
		}

		IFlashPlayer* pFlashPlayer = CHUDUtils::GetFlashPlayerFromMaterial( pSubMat, bGetTemporary );
		if(pFlashPlayer)
		{
			if(pRetFlashPlayer)
			{
				GameWarning( "HUD: Multiple flash assets in texture!");
			}

			pRetFlashPlayer = pFlashPlayer;
		}
	}

	return pRetFlashPlayer;
}

void UpdateFlashPlayerViewport(IFlashPlayer* pFlashPlayer, int iWidth, int iHeight)
{
	if (pFlashPlayer)
	{
		const float assetWidth = (float)pFlashPlayer->GetWidth();
		const float assetHeight = (float)pFlashPlayer->GetHeight();

		const float renderWidth = (float)iWidth;
		const float renderHeight = (float)iHeight;

		if( (renderWidth * __fres(renderHeight)) > MIN_ASPECT_RATIO )
		{
			const int proposedWidth = int_round( (renderHeight * __fres(assetHeight)) * assetWidth);
			const int offset = int_round(0.5f * (float)(iWidth - proposedWidth));
			pFlashPlayer->SetViewport(offset, 0, proposedWidth, iHeight);
		}
		else
		{
			const int proposedHeight = int_round( (renderWidth / assetWidth) * assetHeight);
			const int offset = int_round(0.5f * (float)(iHeight - proposedHeight));
			pFlashPlayer->SetViewport(0, offset, iWidth, proposedHeight);
		}
	}
}


const ColorF& GetHUDColor()
{
	return s_hUDColor;
}

const float GetIconDepth(const float distance)
{
	// Made into a separate function in case the calculation becomes more complicated at any point
	CUICVars* pCVars = g_pGame->GetUI()->GetCVars();
	return max(distance * pCVars->hud_stereo_icon_depth_multiplier, pCVars->hud_stereo_minDist);
}

int GetBetweenRoundsTimer( int previousTimer )
{
	int roundTime = previousTimer;

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		IGameRulesRoundsModule *pRoundsModule = pGameRules->GetRoundsModule();
		if (pRoundsModule)
		{
			if (pRoundsModule->IsRestarting() && (pRoundsModule->GetRoundEndHUDState() == IGameRulesRoundsModule::eREHS_HUDMessage))
			{
				if (pRoundsModule->GetPreviousRoundWinReason() == EGOR_TimeLimitReached)
				{
					// Round finished by the time limit being hit, force the timer to 0 (may not have reached 0 on clients yet due to network lag)
					roundTime = 0;
				}
			}
			else
			{
				roundTime = static_cast<int>(60.0f*pGameRules->GetTimeLimit());
			}
		}
	}

	return roundTime;
}

bool IsSensorPackageActive()
{
	return false;
}

static CHUDUtils::TCenterSortArray m_helperArray;
TCenterSortArray& GetCenterSortHelper()
{
	return m_helperArray;
}

void* GetNearestToCenter()
{
	return GetNearestToCenter(20.0f);
}

void* GetNearestToCenter(const float maxValidDistance)
{
	return GetNearestTo(Vec2(50.0f, 50.0f), maxValidDistance);
}

void* GetNearestTo(const Vec2& center, const float maxValidDistance)
{
	return GetNearestTo(GetCenterSortHelper(), center, maxValidDistance);
}

void* GetNearestTo(const TCenterSortArray& array, const Vec2& center, const float maxValidDistance)
{
	int nearest = -1;
	float nearestDistSq = sqr(maxValidDistance*1.1f);

	Vec2 renderSize = Vec2(800.0f, 600.0f);

	ScreenLayoutManager* pLayoutMgr = g_pGame->GetUI()->GetLayoutManager();
	if(pLayoutMgr)
	{
		renderSize.x = pLayoutMgr->GetRenderWidth();
		renderSize.y = pLayoutMgr->GetRenderHeight();
	}
	else if(gEnv->pRenderer)
	{
		renderSize.x = (float)gEnv->pRenderer->GetOverlayWidth();
		renderSize.y = (float)gEnv->pRenderer->GetOverlayHeight();
	}

	float xCompression = 1.0f;
	if(renderSize.y>0.0f)
	{
		xCompression = renderSize.x / renderSize.y;
	}

	const int numPoints = array.size();
	for(int i=0; i<numPoints; ++i)
	{
		const SCenterSortPoint& point = array[i];

		Vec2 dir = (point.m_screenPos - center);
		dir.x *= xCompression;

		const float distanceSq = dir.GetLength2();

		if(distanceSq>nearestDistSq)
		{
			continue;
		}

		nearest = i;
		nearestDistSq = distanceSq;
	}

	if(numPoints>0 && nearest<0)
	{
		int a=1;
	}

	if(nearest>=0)
	{
		return array[nearest].m_pData;
	}
	return NULL;
}

uint32 ConverToSilhouetteParamValue(ColorF color, bool bEnable/* = true*/)
{
		return	ConverToSilhouetteParamValue(color.r, color.g, color.b, color.a);
}

uint32 ConverToSilhouetteParamValue(float r, float g, float b, float a, bool bEnable/* = true*/)
{
	if (bEnable && fabsf(1.f - a) < FLT_EPSILON)
	{
		return	(uint32)(int_round(r * 255.0f) << 24) |
						(int_round(g * 255.0f) << 16) |
						(int_round(b * 255.0f) << 8) |
						(int_round(a * 255.0f));
	}

	return 0;
}

}
