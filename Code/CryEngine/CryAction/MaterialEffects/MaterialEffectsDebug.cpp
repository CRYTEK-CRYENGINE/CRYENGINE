// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MaterialEffectsDebug.h"

#include "MaterialEffects.h"
#include "MaterialEffectsCVars.h"
#include "MFXContainer.h"

namespace MaterialEffectsUtils
{
#ifdef MATERIAL_EFFECTS_DEBUG

	#define DEFAULT_DEBUG_VISUAL_MFX_LIFETIME 12.0f

void CVisualDebug::AddEffectDebugVisual(const TMFXEffectId effectId, const SMFXRunTimeEffectParams& runtimeParams)
{
	//Only add, if hint search matches (this allows to filter effects invoked by name from game, and get all info we need to display)
	if (effectId == m_lastSearchHint.fxId)
	{
		if (m_nextHit >= kMaxDebugVisualMfxEntries)
		{
			m_nextHit = 0;
		}

		const char* debugFilter = CMaterialEffectsCVars::Get().mfx_DebugVisualFilter->GetString();
		assert(debugFilter);
		bool ignoreFilter = (strlen(debugFilter) == 0) || (strcmp(debugFilter, "0") == 0);
		bool addToDebugList = ignoreFilter || (stricmp(debugFilter, m_lastSearchHint.materialName1.c_str()) == 0);

		if (addToDebugList)
		{
			m_effectList[m_nextHit].fxPosition = runtimeParams.pos;
			m_effectList[m_nextHit].fxDirection = (runtimeParams.normal.IsZero() == false) ? runtimeParams.normal : Vec3(0.0f, 0.0f, 1.0f);
			m_effectList[m_nextHit].lifeTime = DEFAULT_DEBUG_VISUAL_MFX_LIFETIME;
			m_effectList[m_nextHit].fxId = effectId;
			m_effectList[m_nextHit].materialName1 = m_lastSearchHint.materialName1.c_str();
			m_effectList[m_nextHit].materialName2 = m_lastSearchHint.materialName2.c_str();

			m_nextHit++;
		}
	}
}

void CVisualDebug::AddLastSearchHint(const TMFXEffectId effectId, const char* customName, const int surfaceIndex2)
{
	m_lastSearchHint.Reset();

	ISurfaceTypeManager* pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
	assert(pSurfaceTypeManager);

	m_lastSearchHint.materialName1 = customName;
	m_lastSearchHint.materialName2 = pSurfaceTypeManager->GetSurfaceType(surfaceIndex2)->GetName();
	m_lastSearchHint.fxId = effectId;
}

void CVisualDebug::AddLastSearchHint(const TMFXEffectId effectId, const IEntityClass* pEntityClass, const int surfaceIndex2)
{
	m_lastSearchHint.Reset();

	ISurfaceTypeManager* pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
	assert(pSurfaceTypeManager);
	assert(pEntityClass);

	m_lastSearchHint.materialName1 = pEntityClass->GetName();
	m_lastSearchHint.materialName2 = pSurfaceTypeManager->GetSurfaceType(surfaceIndex2)->GetName();
	m_lastSearchHint.fxId = effectId;
}

void CVisualDebug::AddLastSearchHint(const TMFXEffectId effectId, const int surfaceIndex1, const int surfaceIndex2)
{
	m_lastSearchHint.Reset();

	ISurfaceTypeManager* pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
	assert(pSurfaceTypeManager);

	m_lastSearchHint.materialName1 = pSurfaceTypeManager->GetSurfaceType(surfaceIndex1)->GetName();
	m_lastSearchHint.materialName2 = pSurfaceTypeManager->GetSurfaceType(surfaceIndex2)->GetName();
	m_lastSearchHint.fxId = effectId;
}

void CVisualDebug::Update(const CMaterialEffects& materialEffects, const float frameTime)
{
	IRenderAuxGeom* pRenderAux = gEnv->pRenderer->GetIRenderAuxGeom();

	SAuxGeomRenderFlags oldFlags = pRenderAux->GetRenderFlags();
	SAuxGeomRenderFlags newFlags = e_Def3DPublicRenderflags;
	newFlags.SetAlphaBlendMode(e_AlphaBlended);
	newFlags.SetDepthTestFlag(e_DepthTestOff);
	newFlags.SetCullMode(e_CullModeNone);
	pRenderAux->SetRenderFlags(newFlags);

	const float baseDebugTimeOut = DEFAULT_DEBUG_VISUAL_MFX_LIFETIME;

	bool extendedDebugInfo = (CMaterialEffectsCVars::Get().mfx_DebugVisual == 2);

	for (int i = 0; i < kMaxDebugVisualMfxEntries; ++i)
	{
		SDebugVisualEntry& currentFX = m_effectList[i];

		if (currentFX.lifeTime <= 0.0f)
		{
			continue;
		}

		currentFX.lifeTime -= frameTime;

		TMFXContainerPtr pEffectContainer = materialEffects.InternalGetEffect(currentFX.fxId);
		if (pEffectContainer)
		{
			const float alpha = clamp_tpl(powf(((currentFX.lifeTime + 2.0f) / baseDebugTimeOut), 3.0f), 0.0f, 1.0f);
			const ColorB blue(0, 0, 255, (uint8)(192 * alpha));
			const Vec3 coneBase = currentFX.fxPosition + (currentFX.fxDirection * 0.4f);
			const Vec3 lineEnd = currentFX.fxPosition;
			pRenderAux->DrawCone(coneBase, currentFX.fxDirection, 0.12f, 0.2f, blue);
			pRenderAux->DrawLine(coneBase, blue, lineEnd, blue, 3.0f);

			const Vec3 baseText = coneBase + (0.2f * currentFX.fxDirection);
			const Vec3 textLineOffset(0.0f, 0.0f, 0.14f);
			const float textColorOk[4] = { 1.0f, 1.0f, 1.0f, alpha };
			const float textColorError[4] = { 1.0f, 0.0f, 0.0f, alpha };
			const float titleColor[4] = { 1.0f, 1.0f, 0.0f, alpha };

			bool matDefaultDetected = ((stricmp(currentFX.materialName1.c_str(), "mat_default") == 0) ||
			                           (stricmp(currentFX.materialName2.c_str(), "mat_default") == 0));

			const float* textColor = matDefaultDetected ? textColorError : textColorOk;

			if (matDefaultDetected)
				IRenderAuxText::DrawLabelEx(baseText, 1.75f, textColor, true, false, "FIX ME (mat_default)!");

			IRenderAuxText::DrawLabelExF(baseText - (textLineOffset * 2.0f), 1.25f, textColor, true, false, "%s / %s", currentFX.materialName1.c_str(), currentFX.materialName2.c_str());
			IRenderAuxText::DrawLabelExF(baseText - (textLineOffset * 3.0f), 1.25f, textColor, true, false, "Lib: %s, FX: %s", pEffectContainer->GetParams().libraryName.c_str(), pEffectContainer->GetParams().name.c_str());

			if (extendedDebugInfo)
			{
				float textOffsetCount = 5.0f;
				SMFXResourceListPtr pFxResources = materialEffects.GetResources(currentFX.fxId);

				//Particles
				SMFXParticleListNode* pParticlesNode = pFxResources->m_particleList;
				if (pParticlesNode)
				{
					IRenderAuxText::DrawLabelEx(baseText - (textLineOffset * textOffsetCount), 1.35f, titleColor, true, false, "** Particles **");
					while (pParticlesNode)
					{
						textOffsetCount += 1.0f;
						IRenderAuxText::DrawLabelExF(baseText - (textLineOffset * textOffsetCount), 1.25f, textColor, true, false, "  %s", pParticlesNode->m_particleParams.name);
						pParticlesNode = pParticlesNode->pNext;
					}
				}

				//Audio
				SMFXAudioListNode* pAudioNode = pFxResources->m_audioList;
				if (pAudioNode)
				{
					textOffsetCount += 1.0f;
					stack_string audioDebugLine;
					IRenderAuxText::DrawLabelEx(baseText - (textLineOffset * textOffsetCount), 1.35f, titleColor, true, false, "** Audio **");
					while (pAudioNode)
					{
						textOffsetCount += 1.0f;

						audioDebugLine.Format("Trigger:  %s ", pAudioNode->m_audioParams.triggerName);
						for (uint32 switchIdx = 0; switchIdx < pAudioNode->m_audioParams.triggerSwitches.size(); ++switchIdx)
						{
							const IMFXAudioParams::SSwitchData& switchData = pAudioNode->m_audioParams.triggerSwitches[switchIdx];
							audioDebugLine.append(stack_string().Format("| '%s'='%s' ", switchData.switchName, switchData.switchStateName).c_str());
						}

						IRenderAuxText::DrawLabelEx(baseText - (textLineOffset * textOffsetCount), 1.25f, textColor, true, false, audioDebugLine.c_str());
						pAudioNode = pAudioNode->pNext;
					}
				}

				//Decals
				SMFXDecalListNode* pDecalNode = pFxResources->m_decalList;
				if (pDecalNode)
				{
					textOffsetCount += 1.0f;
					IRenderAuxText::DrawLabelEx(baseText - (textLineOffset * textOffsetCount), 1.35f, titleColor, true, false, "** Decals **");
					while (pDecalNode)
					{
						textOffsetCount += 1.0f;
						IRenderAuxText::DrawLabelExF(baseText - (textLineOffset * textOffsetCount), 1.25f, textColor, true, false, "  Mat: %s / Tex: %s", pDecalNode->m_decalParams.material, pDecalNode->m_decalParams.filename);
						pDecalNode = pDecalNode->pNext;
					}
				}

				//Flow graphs
				SMFXFlowGraphListNode* pFlowgraphNode = pFxResources->m_flowGraphList;
				if (pFlowgraphNode)
				{
					textOffsetCount += 1.0f;
					IRenderAuxText::DrawLabelEx(baseText - (textLineOffset * textOffsetCount), 1.35f, titleColor, true, false, "** Flow graphs **");
					while (pFlowgraphNode)
					{
						textOffsetCount += 1.0f;
						IRenderAuxText::DrawLabelExF(baseText - (textLineOffset * textOffsetCount), 1.25f, textColor, true, false, "  %s", pFlowgraphNode->m_flowGraphParams.name);
						pFlowgraphNode = pFlowgraphNode->pNext;
					}
				}

				//Force feedback
				SMFXForceFeedbackListNode* pForceFeedbackNode = pFxResources->m_forceFeedbackList;
				if (pForceFeedbackNode)
				{
					textOffsetCount += 1.0f;
					IRenderAuxText::DrawLabelEx(baseText - (textLineOffset * textOffsetCount), 1.35f, titleColor, true, false, "** Force feedback **");
					while (pForceFeedbackNode)
					{
						textOffsetCount += 1.0f;
						IRenderAuxText::DrawLabelExF(baseText - (textLineOffset * textOffsetCount), 1.25f, textColor, true, false, "  %s", pForceFeedbackNode->m_forceFeedbackParams.forceFeedbackEventName);
						pForceFeedbackNode = pForceFeedbackNode->pNext;
					}
				}
			}
		}
	}

	pRenderAux->SetRenderFlags(oldFlags);
}

#endif //MATERIAL_EFFECTS_DEBUG
}
