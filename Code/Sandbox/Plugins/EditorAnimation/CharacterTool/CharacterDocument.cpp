// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "CharacterDocument.h"
#include <CryExtension/CryCreateClassInstance.h>
#include "Serialization.h"
#include <CrySystem/ISystem.h>
#include <CryAnimation/ICryAnimation.h>
#include <IEditor.h>
#include <CryInput/IInput.h>
#include <CryRenderer/IShaderParamCallback.h>
#include <CryAnimation/IVertexAnimation.h>
#include <CryPhysics/IPhysicsDebugRenderer.h>
#include <CryAction/IMaterialEffects.h>
#include "QViewport.h"
#include "QPropertyTree/ContextList.h"
#include <QViewportSettings.h>
#include <IResourceSelectorHost.h>
#include "DisplayParameters.h"
#include "AnimationList.h"
#include "CharacterList.h"
#include "Explorer/ExplorerPanel.h"
#include "SkeletonList.h"
#include "SkeletonContent.h"
#include "CharacterPhysics.h"
#include "CharacterRig.h"
#include "../Shared/AnimSettings.h"
#include "CompressionMachine.h"
#include "Expected.h"
#include <QApplication>
#include <CryMath/Cry_Color.h>
#include "FeatureTest.h"
#include "EffectPlayer.h"
#include "CharacterToolSystem.h"
#include "SceneContent.h"
#include "Util/EditorUtils.h" //strstri
#include "Controls/QuestionDialog.h"

#define IEntity IEntity_AfxOleOverride
#include <shlobj.h>
#include <shellapi.h>
#undef IEntity

namespace Private_CharacterDocument
{

static void DrawSkeletonBoundingBox(IRenderAuxGeom* pAuxGeom, const ICharacterInstance* pCharacter, const QuatT& location)
{
	if (!pCharacter)
		return;

	const OBB obb = OBB::CreateOBBfromAABB(location.q, pCharacter->GetAABB());
	pAuxGeom->DrawOBB(obb, location.t, 0, RGBA8(0xff, 0x00, 0x1f, 0xff), eBBD_Extremes_Color_Encoded);
}

static void DrawPhysicalEntities(IPhysicsDebugRenderer* pRenderer, ICharacterInstance* pCharacter, int iDrawHelpers)
{
	if (pCharacter == 0)
		return;

	ISkeletonPose& skeletonPose = *pCharacter->GetISkeletonPose();
	IPhysicalEntity* pBaseEntity = skeletonPose.GetCharacterPhysics();
	if (pBaseEntity)
		pRenderer->DrawEntityHelpers(pBaseEntity, iDrawHelpers);

	//draw rope proxies
	for (uint32 i = 0; 1; i++)
	{
		IPhysicalEntity* pEntity = skeletonPose.GetCharacterPhysics(i);
		if (pEntity == 0)
			break;
		pRenderer->DrawEntityHelpers(pEntity, iDrawHelpers);
	}

	IAttachmentManager* pIAttachmentManager = pCharacter->GetIAttachmentManager();
	if (pIAttachmentManager)
	{
		uint32 numAttachments = pIAttachmentManager->GetAttachmentCount();
		for (int32 i = 0; i < numAttachments; i++)
		{
			IAttachment* pAttachment = pIAttachmentManager->GetInterfaceByIndex(i);
			if (pAttachment == 0)
				continue;
			IAttachmentObject* pAttachmentObject = pAttachment->GetIAttachmentObject();
			if (pAttachmentObject == 0)
				continue;

			ICharacterInstance* pAttachedCharacter = pAttachmentObject->GetICharacterInstance();
			if (pAttachedCharacter)
				DrawPhysicalEntities(pRenderer, pAttachedCharacter, iDrawHelpers);
		}
	}

}

static void DrawJoint(IRenderAuxGeom* pAuxGeom, const Vec3& start, const Vec3& end, const uint8 opacity = 255)
{
	const ColorB colStart(255, 255, 0, opacity);//255,255
	const ColorB colEnd(150, 100, 0, opacity);//150,100

	Vec3 vBoneVec = end - start;
	float fBoneLength = vBoneVec.GetLength();
	const f32 t = fBoneLength * 0.03f;

	if (fBoneLength < 1e-4)
		return;

	QuatT qt(Quat::CreateRotationV0V1(Vec3(1, 0, 0), vBoneVec / fBoneLength), start);

	//bone points in x-direction
	Vec3 left = qt * Vec3(t, 0, +t);
	Vec3 right = qt * Vec3(t, 0, -t);

	pAuxGeom->DrawLine(start, colStart, left, colStart);
	pAuxGeom->DrawLine(start, colStart, right, colStart);
	pAuxGeom->DrawLine(end, colEnd, left, colStart);
	pAuxGeom->DrawLine(end, colEnd, right, colStart);
}

static ILINE ColorB shader(Vec3 n, Vec3 d0, Vec3 d1, ColorB c)
{
	f32 a = 0.5f * n | d0, b = 0.1f * n | d1, l = min(1.0f, fabs_tpl(a) - a + fabs_tpl(b) - b + 0.05f);
	return RGBA8(uint8(l * c.r), uint8(l * c.g), uint8(l * c.b), c.a);
}

static void DrawJoint(IRenderAuxGeom* pAuxGeom, const Vec3& p, const Vec3& c, ColorB clr, const Vec3& vdir)
{
	const f32 fAxisLen = (p - c).GetLength();
	if (fAxisLen < 0.001f)
		return;

	const Vec3 vAxisNormalized = (p - c).GetNormalized();
	QuatTS qt(Quat::CreateRotationV0V1(Vec3(1, 0, 0), vAxisNormalized), p, 1.0f);

	ColorB clr2 = clr;

	f32 cr = 0.01f + fAxisLen / 50.0f;
	uint32 subdiv = 4 * 4;
	f32 rad = gf_PI * 2.0f / f32(subdiv);
	const float cos15 = cos_tpl(rad), sin15 = sin_tpl(rad);

	Vec3 pt0, pt1, pt2, pt3;
	ColorB pclrlit0, pclrlit1, pclrlit2, pclrlit3;
	ColorB nclrlit0, nclrlit1, nclrlit2, nclrlit3;

	Matrix34 m34 = Matrix34(qt);
	Matrix33 m33 = Matrix33(m34);

	//	Matrix34 t34p; t34p.SetTranslationMat(Vec3( 0,0,0));

	Matrix34 t34m;
	t34m.SetTranslationMat(Vec3(-fAxisLen, 0, 0));
	Matrix34 m34p = m34;
	Matrix34 m34m = m34 * t34m;
	Matrix34 r34m = m34 * t34m;
	r34m.m00 = -r34m.m00;
	r34m.m10 = -r34m.m10;
	r34m.m20 = -r34m.m20;
	Matrix33 r33 = Matrix33(r34m);

	Vec3 ldir0 = vdir;
	ldir0.z = 0;
	(ldir0 = ldir0.normalized() * 0.5f).z = f32(sqrt3 * -0.5f);
	Vec3 ldir1 = Vec3(0, 0, 1);

	f32 x = cos15, y = sin15;
	pt0 = Vec3(0, 0, cr);
	pclrlit0 = shader(m33 * Vec3(0, 0, 1), ldir0, ldir1, clr2);
	f32 s = 0.20f;
	Vec3 _p[0x4000];
	ColorB _c[0x4000];
	uint32 m_dwNumFaceVertices = 0;
	// child part
	for (uint32 i = 0; i < subdiv; i++)
	{
		pt1 = Vec3(0, -y * cr, x * cr);
		pclrlit1 = shader(m33 * Vec3(0, -y, x), ldir0, ldir1, clr2);
		Vec3 x0 = m34m * Vec3(pt0.x * s, pt0.y * s, pt0.z * s);
		Vec3 x1 = m34p * pt0;
		Vec3 x3 = m34m * Vec3(pt1.x * s, pt1.y * s, pt1.z * s);
		Vec3 x2 = m34p * pt1;
		_p[m_dwNumFaceVertices] = x1;
		_c[m_dwNumFaceVertices++] = pclrlit0;
		_p[m_dwNumFaceVertices] = x2;
		_c[m_dwNumFaceVertices++] = pclrlit1;
		_p[m_dwNumFaceVertices] = x3;
		_c[m_dwNumFaceVertices++] = pclrlit1;

		_p[m_dwNumFaceVertices] = x3;
		_c[m_dwNumFaceVertices++] = pclrlit1;
		_p[m_dwNumFaceVertices] = x0;
		_c[m_dwNumFaceVertices++] = pclrlit0;
		_p[m_dwNumFaceVertices] = x1;
		_c[m_dwNumFaceVertices++] = pclrlit0;
		f32 t = x;
		x = x * cos15 - y * sin15;
		y = y * cos15 + t * sin15;
		pt0 = pt1, pclrlit0 = pclrlit1;
	}

	// parent part
	f32 cost = 1, sint = 0, costup = cos15, sintup = sin15;
	for (uint32 j = 0; j < subdiv / 4; j++)
	{
		Vec3 n = Vec3(sint, 0, cost);
		pt0 = n * cr;
		pclrlit0 = shader(m33 * n, ldir0, ldir1, clr);
		nclrlit0 = shader(r33 * n, ldir0, ldir1, clr);
		n = Vec3(sintup, 0, costup);
		pt2 = n * cr;
		pclrlit2 = shader(m33 * n, ldir0, ldir1, clr);
		nclrlit2 = shader(r33 * n, ldir0, ldir1, clr);

		x = cos15, y = sin15;
		for (uint32 i = 0; i < subdiv; i++)
		{
			n = Vec3(0, -y, x) * costup, n.x += sintup;
			pt3 = n * cr;
			pclrlit3 = shader(m33 * n, ldir0, ldir1, clr);
			nclrlit3 = shader(r33 * n, ldir0, ldir1, clr);
			n = Vec3(0, -y, x) * cost, n.x += sint;
			pt1 = n * cr;
			pclrlit1 = shader(m33 * n, ldir0, ldir1, clr);
			nclrlit1 = shader(r33 * n, ldir0, ldir1, clr);

			Vec3 v0 = m34p * pt0;
			Vec3 v1 = m34p * pt1;
			Vec3 v2 = m34p * pt2;
			Vec3 v3 = m34p * pt3;
			_p[m_dwNumFaceVertices] = v1;
			_c[m_dwNumFaceVertices++] = pclrlit1;
			_p[m_dwNumFaceVertices] = v0;
			_c[m_dwNumFaceVertices++] = pclrlit0;
			_p[m_dwNumFaceVertices] = v2;
			_c[m_dwNumFaceVertices++] = pclrlit2;
			_p[m_dwNumFaceVertices] = v3;
			_c[m_dwNumFaceVertices++] = pclrlit3;
			_p[m_dwNumFaceVertices] = v1;
			_c[m_dwNumFaceVertices++] = pclrlit1;
			_p[m_dwNumFaceVertices] = v2;
			_c[m_dwNumFaceVertices++] = pclrlit2;

			Vec3 w0 = r34m * (pt0 * s);
			Vec3 w1 = r34m * (pt1 * s);
			Vec3 w3 = r34m * (pt3 * s);
			Vec3 w2 = r34m * (pt2 * s);
			_p[m_dwNumFaceVertices] = w0;
			_c[m_dwNumFaceVertices++] = pclrlit0;
			_p[m_dwNumFaceVertices] = w1;
			_c[m_dwNumFaceVertices++] = pclrlit1;
			_p[m_dwNumFaceVertices] = w2;
			_c[m_dwNumFaceVertices++] = pclrlit2;
			_p[m_dwNumFaceVertices] = w1;
			_c[m_dwNumFaceVertices++] = pclrlit1;
			_p[m_dwNumFaceVertices] = w3;
			_c[m_dwNumFaceVertices++] = pclrlit3;
			_p[m_dwNumFaceVertices] = w2;
			_c[m_dwNumFaceVertices++] = pclrlit2;
			f32 t = x;
			x = x * cos15 - y * sin15;
			y = y * cos15 + t * sin15;
			pt0 = pt1, pt2 = pt3;
			pclrlit0 = pclrlit1, pclrlit2 = pclrlit3;
			nclrlit0 = nclrlit1, nclrlit2 = nclrlit3;
		}
		cost = costup;
		sint = sintup;
		costup = cost * cos15 - sint * sin15;
		sintup = sint * cos15 + cost * sin15;
	}

	pAuxGeom->SetRenderFlags(SAuxGeomRenderFlags(e_Mode3D | e_DrawInFrontOff | e_FillModeSolid | e_CullModeFront | e_DepthWriteOn | e_DepthTestOn));
	pAuxGeom->DrawTriangles(&_p[0], m_dwNumFaceVertices, &_c[0]);
	pAuxGeom->DrawLine(p, clr, c, clr);
}

static void DrawFrame(IRenderAuxGeom* pAuxGeom, const QuatT& location, const float scale = 0.02f, const uint8 opacity = 255)
{
	pAuxGeom->DrawLine(location.t, RGBA8(0xbf, 0x00, 0x00, opacity), location.t + location.GetColumn0() * scale, RGBA8(0xff, 0x00, 0x00, opacity));
	pAuxGeom->DrawLine(location.t, RGBA8(0x00, 0xbf, 0x00, opacity), location.t + location.GetColumn1() * scale, RGBA8(0x00, 0xff, 0x00, opacity));
	pAuxGeom->DrawLine(location.t, RGBA8(0x00, 0x00, 0xbf, opacity), location.t + location.GetColumn2() * scale, RGBA8(0x00, 0x00, 0xff, opacity));
}

static void DrawSkeleton(IRenderAuxGeom* pAuxGeom, IDefaultSkeleton* pDefaultSkeleton, ISkeletonPose* pPose, const QuatT& location, string filter, QuatT cameraFrame)
{
	pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags | e_DepthTestOff);

	uint32 jointCount = pDefaultSkeleton->GetJointCount();
	bool filtered = !filter.empty();

	for (uint32 i = 0; i < jointCount; ++i)
	{
		const uint8 opacity = (filtered && strstri(pDefaultSkeleton->GetJointNameByID(i), filter) == 0) ? 20 : 255;

		const int32 parentIndex = pDefaultSkeleton->GetJointParentIDByID(i);
		const QuatT jointFrame = location * pPose->GetAbsJointByID(i);
		const Diag33 jointScale = pPose->GetAbsJointScalingByID(i);

		if (parentIndex > -1)
		{
			DrawJoint(pAuxGeom, location * pPose->GetAbsJointByID(parentIndex).t, jointFrame.t, opacity);
		}

		DrawFrame(pAuxGeom, jointFrame, 0.02f * jointScale.x, opacity);
	}
}

}

namespace CharacterTool {

ViewportOptions::ViewportOptions()
	: lightMultiplier(4.0f)
	, lightSpecMultiplier(1.0f)
	, lightRadius(400.0f)
	, lightDiffuseColor0(0.70f, 0.70f, 0.70f)
	, lightDiffuseColor1(0.40f, 0.60f, 0.90f)
	, lightDiffuseColor2(0.40f, 0.14f, 0.00f)
	, lightOrbit(15.0f)
	, enableLighting(true)
	, animateLights(false)
{
}

void AnimationDrivenSamples::Reset()
{
	nextIndex = 0;
	count = 0;
}

void AnimationDrivenSamples::Add(const float fTime, const Vec3& pos)
{
	if (time >= updateTime)
	{
		time = 0.0f;

		count++;
		if (count > maxCount)
			count = maxCount;

		samples[nextIndex] = pos;

		nextIndex++;
		if (nextIndex >= maxCount)
			nextIndex = 0;
	}

	time += fTime;
}

void AnimationDrivenSamples::Draw(IRenderAuxGeom* aux)
{
	for (int i = 0; i < count; ++i)
		aux->DrawAABB(AABB(samples[i], 0.005f), true, RGBA8(0x00, 0x00, 0xff, 0x00), eBBD_Faceted);
}

// ---------------------------------------------------------------------------

static const char* GetExt(const char* path)
{
	const char* ext = strchr(path, '.');
	if (!ext)
		return path + strlen(path);
	return ext + 1;
}

static MotionParameters QueryMotionParametersFromCharacterInstance(const ICharacterInstance* character, const uint32 layer)
{
	assert(character);
	assert(character->GetISkeletonAnim());

	MotionParameters motionParameters;

	const auto animationsCount = character->GetISkeletonAnim()->GetNumAnimsInFIFO(layer);
	if (animationsCount > 0)
	{
		const CAnimation& animation = character->GetISkeletonAnim()->GetAnimFromFIFO(layer, animationsCount - 1);
		const SParametricSampler* sampler = animation.GetParametricSampler();
		if (sampler)
		{
			for (size_t i = 0; i < sampler->m_numDimensions; ++i)
			{
				const auto parameterId = sampler->m_MotionParameterID[i];
				if (parameterId < eMotionParamID_COUNT)
				{
					motionParameters.enabled[parameterId] = true;
					motionParameters.rangeMin[parameterId] = 0.0f;
					motionParameters.rangeMax[parameterId] = 1.0f;
					character->GetIAnimationSet()->GetMotionParameterRange(animation.GetAnimationId(), EMotionParamID(parameterId), motionParameters.rangeMin[parameterId], motionParameters.rangeMax[parameterId]);
				}
			}
		}
	}

	return motionParameters;
}

void TransferMotionParametersValues(const MotionParameters& from, MotionParameters& to)
{
	for (size_t i = 0; i < eMotionParamID_COUNT; ++i)
	{
		if (to.enabled[i])
		{
			to.values[i] = clamp_tpl(from.values[i], to.rangeMin[i], to.rangeMax[i]);
		}
	}
}

// ---------------------------------------------------------------------------

PlaybackLayer::PlaybackLayer()
	: enabled(true)
	, weight(1.0f)
	, layerId(-1)
	, type(-1)
{
}

void PlaybackLayer::Serialize(IArchive& ar)
{
	ar(animation, "animation");
	ar(AnimationPathWithId(path, layerId), "path", "<^");
	if (ar.isInput() && path.empty())
		animation.clear();
	ar(type, "type");
	ar(layerId, "layerId");

	bool weightEnabled = layerId != 0 && type != AnimationContent::AIMPOSE && type != AnimationContent::LOOKPOSE;
	ar(Serialization::Range(weight, 0.0f, 1.0f), "weight", weightEnabled ? "^>40>" : 0);
	if (ar.isInput())
		weight = clamp_tpl(weight, 0.0f, 1.0f);

	ar(enabled, "enabled", "^^");
	if (!ar.getFilter() || !ar.filter(SERIALIZE_LAYERS_ONLY))
	{
		if (type == AnimationContent::AIMPOSE || type == AnimationContent::LOOKPOSE)
			aim.Serialize(ar);
		else if ((type == AnimationContent::BLEND_SPACE || type == AnimationContent::COMBINED_BLEND_SPACE))
			motionParameters.Serialize(ar);
	}
}

// ---------------------------------------------------------------------------

PlaybackLayers::PlaybackLayers()
	: activeLayer(0)
	, bindPose(false)
	, allowRedirect(false)
{
	layers.resize(1);
	layers[0].layerId = 0;
}

void PlaybackLayers::Serialize(bool* activeLayerSetToEmpty, IArchive& ar)
{
	if (ar.openBlock("layers", bindPose ? "+Animation Layers" : "+Animation Layers"))
	{
		ar(layers, "layers", bindPose ? "!^[+]" : "^[+]");
		if (bindPose)
			ar.warning(layers, "Bind Pose overrides animation playback.");
		ar(bindPose, "bindPose", "^Bind Pose");
		ar.doc("Disable all animation and simulations and show only the model in its default pose");
		ar(allowRedirect, "allowRedirect", bindPose ? "^Allow Redirect" : 0);
		ar.doc("Show pose modifications created by redirected attachment sockets");

		if (ar.isInput())
		{
			*activeLayerSetToEmpty = false;
			for (size_t i = 0; i < layers.size(); ++i)
			{
				if (!*activeLayerSetToEmpty && (layers[i].layerId == -1 && layers[i].animation.empty()))
				{
					activeLayer = i;
					*activeLayerSetToEmpty = true;
				}
				layers[i].layerId = i;
			}
			if (!layers.empty() && size_t(activeLayer) >= layers.size())
				activeLayer = int(layers.size()) - 1;
		}
		ar.closeBlock();
	}
}

// ---------------------------------------------------------------------------

PlaybackOptions::PlaybackOptions()
	: loopAnimation(true)
	, restartOnAnimationSelection(true)
	, firstFrameAtEndOfTimeline(false)
	, wrapTimelineSlider(false)
	, smoothTimelineSlider(false)
	, playFromTheStart(false)
	, playbackSpeed(1.0f)
{
}

void PlaybackOptions::Serialize(IArchive& ar)
{
	ar(loopAnimation, "loopAnimation");
	ar(restartOnAnimationSelection, "restartOnAnimationSelection");
	ar(playFromTheStart, "playFromTheStart");
	ar(firstFrameAtEndOfTimeline, "firstFrameAtEndOfTimeline");
	ar(wrapTimelineSlider, "wrapTimelineSlider");
	ar(smoothTimelineSlider, "smoothTimelineSlider");
	ar(playbackSpeed, "playbackSpeed");
}

// ---------------------------------------------------------------------------

void ViewportOptions::Serialize(IArchive& ar)
{

}

// ---------------------------------------------------------------------------

CharacterDocument::CharacterDocument(System* system)
	: m_system(system)
	, m_bPaused(false)
	, m_camRadius(3.0f)
	, m_LocalEntityMat(IDENTITY)
	, m_PrevLocalEntityMat(IDENTITY)
	, m_lastCalculateRelativeMovement(IDENTITY)
	, m_NormalizedTime(0)
	, m_NormalizedTimeSmooth(0)
	, m_NormalizedTimeRate(0)
	, m_PhysicalLocation(IDENTITY)
	, m_characterManager(0)
	, m_pPhysicalEntity(0)
	, m_pDefaultMaterial(0)
	, m_AverageFrameTime(0.0f)
	, m_compressionMachine(new CompressionMachine())
	, m_playbackState(PLAYBACK_UNAVAILABLE)
	, m_playbackTime(0.0f)
	, m_playbackDuration(1.0f)
	, m_bindPoseEnabled(false)
	, m_displayOptions(new DisplayOptions())
	, m_updateCameraTarget(false)
	, m_viewportState(new SViewportState())
	, m_playbackBlockReason("")
	, m_loadedAnimationSetFilter(new AnimationSetFilter)
	, m_compressedCharacter()
	, m_uncompressedCharacter()
{
	EXPECTED(connect(m_compressionMachine.get(), SIGNAL(SignalAnimationStarted()), this, SLOT(OnCompressionMachineAnimationStarted())));

	m_characterManager = GetIEditor()->GetSystem()->GetIAnimationSystem();

	IConsole* console = GetIEditor()->GetSystem()->GetIConsole();
	m_cvar_drawEdges = console->GetCVar("ca_DrawWireframe");
	m_cvar_drawLocator = console->GetCVar("ca_DrawLocator");

}

void CharacterDocument::ConnectExternalSignals()
{
	EXPECTED(connect(m_system->scene.get(), &SceneContent::SignalAnimEventPlayerTypeChanged, this, &CharacterDocument::OnSceneAnimEventPlayerTypeChanged));
	EXPECTED(connect(m_system->scene.get(), SIGNAL(SignalCharacterChanged()), this, SLOT(OnSceneCharacterChanged())));
	EXPECTED(connect(m_system->scene.get(), SIGNAL(SignalPlaybackLayersChanged(bool)), this, SLOT(OnScenePlaybackLayersChanged(bool))));
	EXPECTED(connect(m_system->scene.get(), SIGNAL(SignalLayerActivated()), this, SLOT(OnSceneLayerActivated())));
	EXPECTED(connect(m_system->scene.get(), SIGNAL(SignalNewLayerActivated()), this, SLOT(OnSceneNewLayerActivated())));
	EXPECTED(connect(m_system->explorerData.get(), SIGNAL(SignalEntryModified(ExplorerEntryModifyEvent &)), this, SLOT(OnExplorerEntryModified(ExplorerEntryModifyEvent &))));
	EXPECTED(connect(m_system->explorerData.get(), &ExplorerData::SignalEntrySavedAs, this, &CharacterDocument::OnExplorerEntrySavedAs));
	EXPECTED(connect(m_system->characterList.get(), SIGNAL(SignalEntryModified(EntryModifiedEvent &)), this, SLOT(OnCharacterModified(EntryModifiedEvent &))));
	EXPECTED(connect(m_system->characterList.get(), &ExplorerFileList::SignalEntrySavedAs, this, &CharacterDocument::OnCharacterSavedAs));
	EXPECTED(connect(m_system->characterList.get(), &ExplorerFileList::SignalEntryDeleted, this, &CharacterDocument::OnCharacterDeleted));
	EXPECTED(connect(m_system->animationList.get(), SIGNAL(SignalForceRecompile(const char*)), this, SLOT(OnAnimationForceRecompile(const char*))));
}

CharacterDocument::~CharacterDocument()
{
}

void CharacterDocument::OnSceneAnimEventPlayerTypeChanged()
{
	m_system->contextList->Update<IAnimEventPlayer>(m_system->scene->animEventPlayer.get());
}

void CharacterDocument::OnSceneCharacterChanged()
{
	LoadCharacter(m_system->scene->characterPath.c_str());
}

void CharacterDocument::Physicalize()
{
	IPhysicalWorld* pPhysWorld = gEnv->pPhysicalWorld;
	if (!pPhysWorld)
		return;

	ICharacterInstance* character = m_compressedCharacter;

	if (m_pPhysicalEntity)
	{
		pPhysWorld->DestroyPhysicalEntity(m_pPhysicalEntity);
		if (character)
		{
			if (character->GetISkeletonPose()->GetCharacterPhysics(-1))
				pPhysWorld->DestroyPhysicalEntity(character->GetISkeletonPose()->GetCharacterPhysics(-1));
			character->GetISkeletonPose()->SetCharacterPhysics(NULL);
		}
	}

	if (character)
	{
		character->GetISkeletonPose()->DestroyCharacterPhysics();
		m_pPhysicalEntity = pPhysWorld->CreatePhysicalEntity(PE_LIVING);
		IPhysicalEntity* pCharPhys = character->GetISkeletonPose()->CreateCharacterPhysics(m_pPhysicalEntity, 80.0f, -1, 70.0f);
		character->GetISkeletonPose()->CreateAuxilaryPhysics(pCharPhys, Matrix34(IDENTITY));
		pe_player_dynamics pd;
		pd.bActive = 0;
		m_pPhysicalEntity->SetParams(&pd);
	}

	IMaterial* pMaterial = 0;
	if (m_compressedCharacter)
		pMaterial = m_compressedCharacter->GetIMaterial();

	if (pMaterial)
	{
		// Assign custom material to physics.
		int surfaceTypesId[MAX_SUB_MATERIALS];
		memset(surfaceTypesId, 0, sizeof(surfaceTypesId));
		int numIds = pMaterial->FillSurfaceTypeIds(surfaceTypesId);

		pe_params_part ppart;
		ppart.nMats = numIds;
		ppart.pMatMapping = surfaceTypesId;
		(character && character->GetISkeletonPose()->GetCharacterPhysics() ?
		 character->GetISkeletonPose()->GetCharacterPhysics() : m_pPhysicalEntity)->SetParams(&ppart);
	}
}

static string GetAnimSettingsFilename(const string& animationPath)
{
	const string gameFolderPath = PathUtil::AddSlash(PathUtil::GetGameFolder());
	const string animationFilePath = gameFolderPath + animationPath;

	const string animSettingsFilename = PathUtil::ReplaceExtension(animationFilePath, "animsettings");
	return animSettingsFilename;
}

struct SWaitCursor
{
	SWaitCursor()
	{
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	}

	~SWaitCursor()
	{
		QApplication::restoreOverrideCursor();
	}
};

void CharacterDocument::LoadCharacter(const char* filename)
{
	m_characterManager->ClearAllKeepInMemFlags();

	// make sure no one uses wrong ICharacterInstance or ICharacterModel during load notification calls
	m_system->contextList->Update<ICharacterInstance>(0);
	m_system->contextList->Update<IDefaultSkeleton>(0);

	SignalCharacterAboutToBeLoaded();
	SWaitCursor waitCursor;

	bool reload = false;
	if (m_loadedCharacterFilename == filename)
		reload = true;

	m_pPhysicalEntity = NULL;

	ReleaseObject();

	// Initialize these with some values
	m_AABB.min = Vec3(-1.0f, -1.0f, -1.0f);
	m_AABB.max = Vec3(1.0f, 1.0f, 1.0f);
	m_PhysicalLocation = QuatTS(IDENTITY);
	*m_loadedAnimationSetFilter = AnimationSetFilter();
	m_loadedAnimationEventsFilePath = string();
	m_loadedCharacterFilename = filename;
	m_loadedSkeleton.clear();
	m_camRadius = 3.0f;

	SEntry<CharacterContent>* characterEntry = 0;
	ExplorerEntry* activeCharacterEntry = 0;

	do
	{
		if (filename[0] == '\0')
			break;

		if (!gEnv->pCryPak->IsFileExist(filename))
		{
			QString message = "Character file is missing:\n    ";
			message += QString::fromLocal8Bit(filename);
			CQuestionDialog::SCritical("Error", message);
			break;
		}

		// make sure we have Explorer item to select before activating it
		if (m_system->characterList->GetEntryBaseByPath(m_loadedCharacterFilename.c_str()) == 0)
			m_system->characterList->AddEntry(m_loadedCharacterFilename.c_str(), true);
		activeCharacterEntry = GetActiveCharacterEntry();
		characterEntry = m_system->characterList->GetEntryByPath<CharacterContent>(filename);
		if (!activeCharacterEntry || !characterEntry)
		{
			QString message = "Failed to create asset entry for character:\n    ";
			message += QString::fromLocal8Bit(filename);
			CQuestionDialog::SCritical("Error", message);
			break;
		}
		m_system->characterList->LoadOrGetChangedEntry(activeCharacterEntry->id);

		string fileExt = GetExt(filename);

		bool IsCGA = (0 == stricmp(fileExt, "cga"));
		if (IsCGA)
		{
			m_compressedCharacter = m_characterManager->CreateInstance(filename, CA_CharEditModel);
			m_uncompressedCharacter = m_characterManager->CreateInstance(filename, CA_CharEditModel);
		}

		bool IsCDF = (0 == stricmp(fileExt, "cdf"));
		if (IsCDF)
		{
			if (characterEntry->content.cdf.skeleton.empty())
			{
				characterEntry->content.engineLoadState = CharacterContent::CHARACTER_INCOMPLETE;
				break;
			}

			m_characterManager->ClearCDFCache();
			vector<char> cdfContent;
			if (characterEntry)
			{
				characterEntry->content.engineLoadState = characterEntry->content.cdf.skeleton.empty() ? CharacterContent::CHARACTER_INCOMPLETE : CharacterContent::CHARACTER_NOT_LOADED;
				characterEntry->content.cdf.SaveToMemory(&cdfContent);
				m_characterManager->InjectCDF(filename, cdfContent.data(), cdfContent.size());
			}

			m_compressedCharacter = m_characterManager->CreateInstance(filename, CA_CharEditModel);
			if (m_compressedCharacter)
				m_characterManager->CreateDebugInstances(filename);
			m_uncompressedCharacter = m_characterManager->CreateInstance(filename, CA_CharEditModel);
			if (m_uncompressedCharacter)
				m_characterManager->CreateDebugInstances(filename);

			m_characterManager->ClearCDFCache();
		}

		bool IsSKEL = (0 == stricmp(fileExt, CRY_SKEL_FILE_EXT));
		bool IsSKIN = (0 == stricmp(fileExt, CRY_SKIN_FILE_EXT));
		if (IsSKEL || IsSKIN)
		{
			m_compressedCharacter = m_characterManager->CreateInstance(filename, CA_CharEditModel);
			if (m_compressedCharacter)
				m_characterManager->CreateDebugInstances(filename);
			m_uncompressedCharacter = m_characterManager->CreateInstance(filename, CA_CharEditModel);
			if (m_uncompressedCharacter)
				m_characterManager->CreateDebugInstances(filename);
		}

		if (!m_compressedCharacter || !m_uncompressedCharacter)
			characterEntry->content.engineLoadState = CharacterContent::CHARACTER_LOAD_FAILED;

	}
	while (false);

	//-------------------------------------------------------

	if (m_uncompressedCharacter)
	{
		m_uncompressedCharacter->SetCharEditMode(m_uncompressedCharacter->GetCharEditMode() | CA_CharacterTool);
	}

	if (m_compressedCharacter)
	{
		m_compressedCharacter->SetCharEditMode(m_compressedCharacter->GetCharEditMode() | CA_CharacterTool);
		m_compressedCharacter->GetISkeletonAnim()->SetEventCallback(AnimationEventCallback, this);

		m_PhysicalLocation.SetIdentity();

		f32 radius = m_compressedCharacter->GetAABB().GetRadius();
		Vec3 center = m_compressedCharacter->GetAABB().GetCenter();

		m_AABB.min = center - Vec3(radius, radius, radius);
		m_AABB.max = center + Vec3(radius, radius, radius);
		if (!reload)
			m_camRadius = center.z + radius;

		m_camRadius = (m_AABB.max - m_AABB.min).GetLength();

		Physicalize();

		IMaterial* pMtl = m_compressedCharacter->GetIMaterial();

		if (!m_groundAlignment)
		{
			::CryCreateClassInstanceForInterface<IAnimationGroundAlignment>(cryiidof<IAnimationGroundAlignment>(), m_groundAlignment);
		}

		CreateShaderParamCallbacks();
		m_characterManager->ClearCDFCache();

		if (characterEntry)
		{
			m_loadedSkeleton = PathUtil::ReplaceExtension(characterEntry->content.cdf.skeleton, ".chrparams");
			SEntry<SkeletonContent>* skeleton = m_system->skeletonList->GetEntryByPath<SkeletonContent>(m_loadedSkeleton.c_str());
			if (skeleton)
			{
				m_system->skeletonList->LoadOrGetChangedEntry(skeleton->id);

				skeleton->content.ComposeCompleteAnimationSetFilter(&*m_loadedAnimationSetFilter, &m_loadedAnimationEventsFilePath, m_system->skeletonList.get());
			}
		}
		else
			m_loadedSkeleton.clear();

		if (activeCharacterEntry && activeCharacterEntry->modified)
		{
			EntryModifiedEvent ev;
			ev.subtree = activeCharacterEntry->subtree;
			ev.id = activeCharacterEntry->id;

			OnCharacterModified(ev);
		}

		AnimationSetFilter filter;
		{
			string chrparamsFilename;
			if (stricmp(PathUtil::GetExt(filename), "cga") == 0)
			{
				string chrparamsFilenameForCga = PathUtil::ReplaceExtension(filename, ".chrparams");
				if (gEnv->pCryPak->IsFileExist(chrparamsFilenameForCga.c_str()))
					chrparamsFilename = chrparamsFilenameForCga;
				else
				{
					// CGA HACK!
					// fallback to animations prefixed with name of cga
					AnimationFilterFolder folder;
					folder.path = PathUtil::GetParentDirectory(m_loadedCharacterFilename);
					AnimationFilterWildcard w;
					w.fileWildcard = PathUtil::GetFileName(filename);
					w.fileWildcard += "*.caf";
					folder.wildcards.clear();
					folder.wildcards.push_back(w);
					filter.folders.clear();
					filter.folders.push_back(folder);
					// ^^^
				}
			}
			else if (CharacterDefinition* definition = GetLoadedCharacterDefinition())
			{
				string chrparamsFilenameForSkeleton = PathUtil::ReplaceExtension(definition->skeleton, ".chrparams");
				if (gEnv->pCryPak->IsFileExist(chrparamsFilenameForSkeleton.c_str()))
					chrparamsFilename = chrparamsFilenameForSkeleton;
			}

			if (!chrparamsFilename.empty())
			{
				if (SEntry<SkeletonContent>* skeleton = m_system->skeletonList->GetEntryByPath<SkeletonContent>(PathUtil::ReplaceExtension(chrparamsFilename, ".chrparams")))
				{
					m_system->skeletonList->LoadOrGetChangedEntry(skeleton->id);

					string dummy;
					skeleton->content.ComposeCompleteAnimationSetFilter(&filter, &dummy, m_system->skeletonList.get());
				}
			}
		}
		m_system->animationList->Populate(m_compressedCharacter, GetDefaultSkeletonAlias().c_str(), filter, m_compressedCharacter->GetModelAnimEventDatabase());

		if (characterEntry)
			characterEntry->content.engineLoadState = CharacterContent::CHARACTER_LOADED;
	}
	else
	{
		m_system->animationList->Populate(0, GetDefaultSkeletonAlias().c_str(), AnimationSetFilter(), "");
	}

	m_compressionMachine->SetCharacters(m_uncompressedCharacter, m_compressedCharacter);

	UpdateBlendShapeParameterList();

	m_system->contextList->Update(m_compressedCharacter.get());
	m_system->contextList->Update(m_compressedCharacter.get() ? &m_compressedCharacter->GetIDefaultSkeleton() : (IDefaultSkeleton*)0);

	SignalCharacterLoaded();
	OnSceneAnimEventPlayerTypeChanged();

	ExplorerEntries selectedEntries;
	GetSelectedExplorerEntries(&selectedEntries);
	if (activeCharacterEntry && (selectedEntries.empty() || (selectedEntries.size() == 1 && m_system->characterList->OwnsAssetEntry(selectedEntries[0]))))
	{
		SetSelectedExplorerEntries(ExplorerEntries(1, activeCharacterEntry), 0);
	}
	else
	{
		SetSelectedExplorerEntries(ExplorerEntries(), 0);
	}
}

void CharacterDocument::ReleaseObject()
{
	if (!m_loadedCharacterFilename.empty())
	{
		if (SEntry<CharacterContent>* characterEntry = m_system->characterList->GetEntryByPath<CharacterContent>(m_loadedCharacterFilename.c_str()))
			characterEntry->content.engineLoadState = CharacterContent::CHARACTER_NOT_LOADED;
	}

	if (m_compressedCharacter)
	{
		m_characterManager->DeleteDebugInstances();
		m_compressedCharacter.reset();
	}
	if (m_uncompressedCharacter)
	{
		m_uncompressedCharacter.reset();
	}
}

void CharacterDocument::PlaybackOptionsChanged()
{
	SignalPlaybackOptionsChanged();

	if (m_compressionMachine)
	{
		m_compressionMachine->SetLoop(m_playbackOptions.loopAnimation);
		m_compressionMachine->SetPlaybackSpeed(m_playbackOptions.playbackSpeed);
	}

	if (m_bPaused)
		ScrubTime(m_playbackTime, false);
}

void CharacterDocument::OnScenePlaybackLayersChanged(bool continuousChange)
{
	ExplorerEntry* activeAnimationEntry = GetActiveAnimationEntry();
	if (activeAnimationEntry && !IsExplorerEntrySelected(activeAnimationEntry))
		SetSelectedExplorerEntries(ExplorerEntries(1, activeAnimationEntry), 0);
	else
		TriggerAnimationPreview(PREVIEW_ALLOW_REWIND);

	SetBindPoseEnabled(m_system->scene->layers.bindPose);
}

void CharacterDocument::OnSceneLayerActivated()
{
	ExplorerEntry* activeAnimationEntry = GetActiveAnimationEntry();
	if (activeAnimationEntry && !IsExplorerEntrySelected(activeAnimationEntry))
		SetSelectedExplorerEntries(ExplorerEntries(1, activeAnimationEntry), 0);
}

void CharacterDocument::OnSceneNewLayerActivated()
{
	// gives opportunity to select animation into newly added layer
	SetSelectedExplorerEntries(ExplorerEntries(), 0);
}

void CharacterDocument::Serialize(IArchive& ar)
{
	ar(m_playbackOptions, "playbackOptions");
	ar(*m_displayOptions, "displayOptions");
	ar(m_playbackTime, "playbackTime");

	if (ar.isInput())
	{
		SignalPlaybackOptionsChanged();
		SignalDisplayOptionsChanged(*m_displayOptions);
		UpdateBlendShapeParameterList();
	}
}

//////////////////////////////////////////////////////////////////////////
void CharacterDocument::OnCharacterModified(EntryModifiedEvent& ev)
{
	SEntry<CharacterContent>* entry = m_system->characterList->GetEntryById<CharacterContent>(ev.id);
	if (!entry)
		return;

	if (stricmp(entry->path.c_str(), m_loadedCharacterFilename.c_str()) != 0)
		return;

	string skeletonChrparams = PathUtil::ReplaceExtension(entry->content.cdf.skeleton, ".chrparams");

	if (m_loadedSkeleton != skeletonChrparams)
	{
		LoadCharacter(m_loadedCharacterFilename.c_str());
		return;
	}

	bool skinSetChanged = false;
	if (m_compressedCharacter.get())
		entry->content.cdf.ApplyToCharacter(&skinSetChanged, m_compressedCharacter.get(), GetIEditor()->GetSystem()->GetIAnimationSystem(), true, !!strcmp(ev.reason, "Revert"));
	if (m_uncompressedCharacter.get())
		entry->content.cdf.ApplyToCharacter(&skinSetChanged, m_uncompressedCharacter.get(), GetIEditor()->GetSystem()->GetIAnimationSystem(), false);

	if (skinSetChanged)
		UpdateBlendShapeParameterList();

	if (entry->content.cdf.physics.empty())
	{
		SCharacterPhysicsContent emptyPhysics;
		if (m_compressedCharacter)
			emptyPhysics.ApplyToCharacter(m_compressedCharacter);
		if (m_uncompressedCharacter)
			emptyPhysics.ApplyToCharacter(m_uncompressedCharacter);
	}

	if (entry->content.cdf.rig.empty())
	{
		SCharacterRigContent emptyRig;
		if (m_compressedCharacter)
			emptyRig.ApplyToCharacter(m_compressedCharacter);
		if (m_uncompressedCharacter)
			emptyRig.ApplyToCharacter(m_uncompressedCharacter);
	}

	Physicalize();

	if (!ev.continuousChange)
		SignalActiveCharacterChanged();
}

void CharacterDocument::OnCharacterSavedAs(const char* oldName, const char* newName)
{
	if (stricmp(m_system->scene->characterPath.c_str(), oldName) == 0)
	{
		m_system->scene->characterPath = newName;
		m_system->scene->CharacterChanged();
	}
}

void CharacterDocument::OnCharacterDeleted(const char* path)
{
	if (stricmp(m_system->scene->characterPath.c_str(), path) == 0)
	{
		m_system->scene->characterPath.clear();
		m_system->scene->CharacterChanged();
	}
}

void CharacterDocument::GetEntriesActiveInDocument(ExplorerEntries* entries) const
{
	if (!entries)
		return;
	entries->clear();
	ExplorerEntry* characterEntry = GetActiveCharacterEntry();
	if (characterEntry)
		entries->push_back(characterEntry);
	ExplorerEntry* animationEntry = GetActiveAnimationEntry();
	if (animationEntry)
		entries->push_back(animationEntry);
}

void CharacterDocument::SetAuxRenderer(IRenderAuxGeom* pAuxRenderer)
{
	m_pAuxRenderer = pAuxRenderer;
}

ExplorerEntry* CharacterDocument::GetActivePhysicsEntry() const
{
	SEntry<CharacterContent>* character = m_system->characterList->GetEntryByPath<CharacterContent>(m_loadedCharacterFilename.c_str());
	if (!character)
		return 0;
	return m_system->explorerData->FindEntryByPath(&*m_system->physicsList, character->content.cdf.physics.c_str());
}

ExplorerEntry* CharacterDocument::GetActiveRigEntry() const
{
	SEntry<CharacterContent>* character = m_system->characterList->GetEntryByPath<CharacterContent>(m_loadedCharacterFilename.c_str());
	if (!character)
		return 0;
	return m_system->explorerData->FindEntryByPath(&*m_system->rigList, character->content.cdf.rig.c_str());
}

ExplorerEntry* CharacterDocument::GetActiveAnimationEntry() const
{
	if (m_system->scene->layers.layers.empty())
		return 0;

	if (size_t(m_system->scene->layers.activeLayer) >= m_system->scene->layers.layers.size())
		return 0;

	const PlaybackLayer& layer = m_system->scene->layers.layers[m_system->scene->layers.activeLayer];

	if (SEntry<AnimationContent>* anim = m_system->animationList->FindEntryByPath(layer.path.c_str()))
	{
		return m_system->explorerData->FindEntryById(&*m_system->animationList, anim->id);
	}

	return 0;
}

void CharacterDocument::OnExplorerEntrySavedAs(const char* oldPath, const char* newPath)
{
	std::vector<ExplorerEntry*> entries;
	m_system->explorerData->FindEntriesByPath(&entries, newPath);
	if (!entries.empty())
		SetSelectedExplorerEntries(ExplorerEntries(entries.begin(), entries.end()), 0);
}

void CharacterDocument::OnExplorerEntryModified(ExplorerEntryModifyEvent& ev)
{
	if ((ev.entryParts & ENTRY_PART_CONTENT) == 0)
		return;

	if (!ev.entry)
		return;

	ExplorerEntry* activePhysicsEntry = GetActivePhysicsEntry();
	if (ev.entry == activePhysicsEntry)
	{
		SEntry<SCharacterPhysicsContent>* characterPhysics = m_system->physicsList->GetEntryByPath<SCharacterPhysicsContent>(ev.entry->path.c_str());
		if (m_compressedCharacter)
			characterPhysics->content.ApplyToCharacter(m_compressedCharacter);
		if (m_uncompressedCharacter)
			characterPhysics->content.ApplyToCharacter(m_uncompressedCharacter);
	}

	ExplorerEntry* activeRigEntry = GetActivePhysicsEntry();
	if (ev.entry == activeRigEntry)
	{
		SEntry<SCharacterRigContent>* characterRig = m_system->rigList->GetEntryByPath<SCharacterRigContent>(ev.entry->path.c_str());
		if (m_compressedCharacter)
			characterRig->content.ApplyToCharacter(m_compressedCharacter);
		if (m_uncompressedCharacter)
			characterRig->content.ApplyToCharacter(m_uncompressedCharacter);
	}

	if (m_system->animationList->OwnsAssetEntry(ev.entry) && ev.entry == GetActiveAnimationEntry())
	{
		SEntry<AnimationContent>* animation = m_system->animationList->GetEntry(ev.entry->id);
		if (!animation)
			return;
		bool triggerPreview = false;

		// We need a name of temporary compressed file instead of animation asset
		// path, in case we are previewing animation compression at the same time.
		const char* animationPathConsideringCompression = m_compressionMachine->AnimationPathConsideringPreview(animation->path.c_str());
		animation->content.ApplyToCharacter(&triggerPreview, m_compressedCharacter, animationPathConsideringCompression, false);

		if (triggerPreview)
		{
			// these 2 lines are necessary for Scene Parameters panel to updated view immediately after a change
			m_system->scene->PlaybackLayersChanged(false);
			m_system->scene->SignalChanged(false);
			TriggerAnimationPreview(0);
		}
	}
	if (m_system->skeletonList->OwnsAssetEntry(ev.entry))
	{
		// One of the included CHRPARAMS changes.
		if (SEntry<SkeletonContent>* skeleton = m_system->skeletonList->GetEntryByPath<SkeletonContent>(m_loadedSkeleton.c_str()))
		{
			m_system->skeletonList->LoadOrGetChangedEntry(skeleton->id);
			if (std::find(ev.dependentEntries.begin(), ev.dependentEntries.end(), GetActiveCharacterEntry()) != ev.dependentEntries.end())
			{
				AnimationSetFilter filter;
				string animationEventsFilePath;
				skeleton->content.ComposeCompleteAnimationSetFilter(&filter, &animationEventsFilePath, m_system->skeletonList.get());
				const bool bFilterDiffers = (*m_loadedAnimationSetFilter != filter);
				assert(m_loadedAnimationEventsFilePath == m_compressedCharacter->GetModelAnimEventDatabase());
				const bool bAnimationEventsFilePathDiffers = (m_loadedAnimationEventsFilePath != animationEventsFilePath);
				if (bFilterDiffers || bAnimationEventsFilePathDiffers)
				{
					*m_loadedAnimationSetFilter = filter;
					m_loadedAnimationEventsFilePath = animationEventsFilePath;

					ReloadCHRPARAMS();

					if (bFilterDiffers)
					{
						m_system->animationList->SetAnimationFilterAndScan(filter);
					}

					assert(m_loadedAnimationEventsFilePath == m_compressedCharacter->GetModelAnimEventDatabase());
					if (bAnimationEventsFilePathDiffers)
					{
						m_system->animationList->SetAnimationEventsFilePath(m_loadedAnimationEventsFilePath);
					}
				}
			}
		}
	}
}

void CharacterDocument::ReloadCHRPARAMS()
{
	SWaitCursor waitCursor;
	CharacterDefinition* cdf = GetLoadedCharacterDefinition();
	if (!cdf)
		return;

	string chrparamsPath = PathUtil::ReplaceExtension(cdf->skeleton, ".chrparams");

	SEntry<SkeletonContent>* skeleton = m_system->skeletonList->GetEntryByPath<SkeletonContent>(chrparamsPath.c_str());
	if (!skeleton)
		return;

	vector<ExplorerEntry*> includedChrParams;

	if (ExplorerEntry* entry = m_system->explorerData->FindEntryByPath(&*m_system->skeletonList, chrparamsPath.c_str()))
	{
		includedChrParams.push_back(entry);
		m_system->explorerData->GetDependingEntries(&includedChrParams, entry);
	}

	for (size_t i = 0; i < includedChrParams.size(); ++i)
	{
		ExplorerEntry* entry = includedChrParams[i];
		if (!m_system->skeletonList->OwnsAssetEntry(entry))
			continue;
		SEntry<SkeletonContent>* skeleton = m_system->skeletonList->GetEntryById<SkeletonContent>(entry->id);
		vector<char> content;
		if (!skeleton->content.skeletonParameters.SaveToMemory(&content))
			continue;

		string chrparamsFilename = PathUtil::ReplaceExtension(entry->path, ".chrparams");
		m_characterManager->InjectCHRPARAMS(chrparamsFilename.c_str(), content.data(), content.size());
	}

	if (m_compressedCharacter)
		m_compressedCharacter->ReloadCHRPARAMS();
	if (m_uncompressedCharacter)
		m_uncompressedCharacter->ReloadCHRPARAMS();
	m_characterManager->ClearCHRPARAMSCache();
}

void CharacterDocument::TriggerAnimationPreview(int previewFlags)
{
	bool allowRewind = (previewFlags & PREVIEW_ALLOW_REWIND) != 0;
	bool forceRecompile = (previewFlags & PREVIEW_FORCE_RECOMPILE) != 0;

	SignalActiveAnimationSwitched();

	if (m_bindPoseEnabled)
		return;

	if (allowRewind && m_playbackOptions.restartOnAnimationSelection)
	{
		if (m_system->scene->animEventPlayer.get())
			m_system->scene->animEventPlayer->Reset();
		m_playbackTime = 0.0f;
		m_NormalizedTime = 0.0f;
		m_NormalizedTimeRate = 0.0f;
		m_NormalizedTimeSmooth = 0.0f;
		SignalPlaybackTimeChanged();
		PreviewAnimationEntry(forceRecompile);
		IdleUpdate();
		Play();
	}
	else
	{
		PreviewAnimationEntry(forceRecompile);
	}
}

void CharacterDocument::SetSelectedExplorerEntries(const ExplorerEntries& entries, int selectOptions)
{
	for (auto& panel : m_system->explorerPanels)
		panel->SetSelectedEntries(entries);
}

void CharacterDocument::OnExplorerSelectionChanged()
{
	// Prevent re-entrance
	static bool s_bHandlingExplorerSelectionChanged = false;
	if (s_bHandlingExplorerSelectionChanged)
		return;
	s_bHandlingExplorerSelectionChanged = true;

	ExplorerPanel* panel = static_cast<ExplorerPanel*>(sender());
	if (!panel)
		return;

	// Clear selection in all other panels
	for (auto* otherPanel : m_system->explorerPanels)
		if (otherPanel != panel)
			otherPanel->SetSelectedEntries(ExplorerEntries());

	ExplorerEntries entries;
	panel->GetSelectedEntries(&entries);
	m_system->explorerData->LoadEntries(entries);

	SignalExplorerSelectionChanged();

	bool playbackLayersChanged = false;

	if (!entries.empty())
	{
		ExplorerEntry* entry = entries[0];
		if (m_system->animationList->OwnsAssetEntry(entry))
		{
			if (SEntry<AnimationContent>* anim = m_system->animationList->GetEntry(entry->id))
			{
				if (m_system->scene->layers.layers.empty())
				{
					// add layer if all layers were cleared
					m_system->scene->layers.layers.resize(1);
					m_system->scene->layers.layers[0].layerId = 0;
					m_system->scene->layers.activeLayer = 0;
				}

				if (size_t(m_system->scene->layers.activeLayer) < m_system->scene->layers.layers.size())
				{
					PlaybackLayer& layer = m_system->scene->layers.layers[m_system->scene->layers.activeLayer];
					if (layer.animation != anim->name || layer.path != anim->path)
					{
						layer.animation = anim->name;
						layer.path = anim->path;
						layer.type = anim->content.type;
						playbackLayersChanged = true;
					}
				}
			}
		}
	}

	if (playbackLayersChanged)
	{
		m_system->scene->PlaybackLayersChanged(false);
		m_system->scene->SignalChanged(false);
	}

	s_bHandlingExplorerSelectionChanged = false;
}

void CharacterDocument::OnExplorerActivated(const ExplorerEntry* entry)
{
	if (m_system->characterList->OwnsAssetEntry(entry))
	{
		m_system->scene->characterPath = entry->path;
		m_system->scene->SignalCharacterChanged();
	}
}

void CharacterDocument::SetBindPoseEnabled(bool bindPoseEnabled)
{
	if (m_bindPoseEnabled != bindPoseEnabled)
	{
		m_bindPoseEnabled = bindPoseEnabled;

		if (m_bindPoseEnabled)
		{
			m_compressionMachine->Reset();
		}
		else
		{
			TriggerAnimationPreview(0);
		}

		SignalBindPoseModeChanged();
	}
}

void CharacterDocument::DisplayOptionsChanged()
{
	SignalDisplayOptionsChanged(*m_displayOptions);
}

void CharacterDocument::IdleUpdate()
{
	{
		PlaybackState playbackState;
		if (!m_compressionMachine->IsPlaying() || m_bindPoseEnabled)
		{
			playbackState = PLAYBACK_UNAVAILABLE;
		}
		else
		{
			if (m_bPaused)
				playbackState = PLAYBACK_PAUSE;
			else
				playbackState = PLAYBACK_PLAY;
		}

		if (m_playbackState != playbackState)
		{
			m_playbackState = playbackState;
			if (m_playbackState == PLAYBACK_UNAVAILABLE)
			{
				if (m_bindPoseEnabled)
					m_playbackBlockReason = "Playback Unavailable: Bind Pose Is Enabled";
				else
					m_playbackBlockReason = "Playback Unavailable";
			}
			else
			{
				m_playbackBlockReason = "";
			}
			SignalPlaybackStateChanged();
		}
	}

	if (m_compressedCharacter)
	{
		ISkeletonAnim& skeletonAnimation = *m_compressedCharacter->GetISkeletonAnim();
		int layer = 0;
		if (CAnimation* animation = &skeletonAnimation.GetAnimFromFIFO(layer, 0))
		{
			if (skeletonAnimation.GetNumAnimsInFIFO(layer) > 0)
			{
				float animationDurationSeconds = skeletonAnimation.CalculateCompleteBlendSpaceDuration(*animation);
				if (animationDurationSeconds < 0.0f)
					animationDurationSeconds = 0.0f;
				float animationTimeNormalized = skeletonAnimation.GetAnimationNormalizedTime(animation);
				float animationTimeSeconds = animationTimeNormalized * animationDurationSeconds;
				if (animationTimeSeconds < 0.0f)
					animationTimeSeconds = 0.0f;
				if (animationTimeSeconds != m_playbackTime ||
				    animationDurationSeconds != m_playbackDuration)
				{
					if (animationTimeSeconds < m_playbackTime)
					{
						if (m_system->scene->animEventPlayer.get())
							m_system->scene->animEventPlayer->Reset();
					}

					if (m_playbackState == PLAYBACK_PLAY)
						m_playbackTime = animationTimeSeconds;

					m_playbackDuration = animationDurationSeconds;
					SignalPlaybackTimeChanged();
				}

				bool animationEnded = animationTimeNormalized == 1.0f;
				if (m_playbackState == PLAYBACK_PLAY && animationEnded)
				{
					// For animations that were not looping we rewind and pause
					m_playbackState = PLAYBACK_PAUSE;
					if (m_playbackOptions.firstFrameAtEndOfTimeline)
					{
						m_compressionMachine->Play(0.0f);
						m_playbackTime = 0.0f;
						m_NormalizedTime = 0.0f;
						m_NormalizedTimeSmooth = 0.0f;
						m_NormalizedTimeRate = 0.0f;
					}
					Pause();
					SignalPlaybackStateChanged();
					SignalPlaybackTimeChanged();
				}
			}
		}
	}
}

static void DrawAnimationStateText(IRenderer* renderer, IRenderAuxGeom* pAuxRenderer,  const vector<StateText>& lines)
{
	int y = 30;

	bool singleLine = lines.size() == 1;
	for (size_t i = 0; i < lines.size(); ++i)
	{
		const StateText& l = lines[i];
		unsigned int color = l.type == l.PROGRESS ? 0xffffff00 :
		                     l.type == l.WARNING ? 0xff00ffff :
		                     l.type == l.FAIL ? 0xff0020ff : 0xffffffff;

		bool singleLine = l.animation.empty();
		if (!singleLine)
		{
			pAuxRenderer->Draw2dLabel(20, y, 1.5f, ColorF(color), false, "%s", l.animation.c_str());
			y += 18;
		}

		pAuxRenderer->Draw2dLabel(singleLine ? 20 : 40, y, singleLine ? 1.5f : 1.2f, ColorF(color), false, "%s", l.text.c_str());
		y += singleLine ? 18 : 16;
	}
}

int CharacterDocument::AnimationEventCallback(ICharacterInstance* instance, void* userData)
{
	((CharacterDocument*)userData)->OnAnimEvent(instance);
	return 0;
}

void CharacterDocument::OnAnimEvent(ICharacterInstance* character)
{
	if (!character)
		return;
	if (!m_bPaused)
	{
		ISkeletonAnim& skeletonAnim = *character->GetISkeletonAnim();
		ISkeletonPose& skeletonPose = *character->GetISkeletonPose();
		IDefaultSkeleton& defaultSkeleton = character->GetIDefaultSkeleton();

		AnimEventInstance event = skeletonAnim.GetLastAnimEvent();
		PlayAnimEvent(character, event);
	}
}

void CharacterDocument::PlayAnimEvent(ICharacterInstance* character, const AnimEventInstance& event)
{
	if (m_system->scene->runFeatureTest && m_system->scene->featureTest.get())
		if (m_system->scene->featureTest->AnimEvent(event, this))
			return;

	if (m_system->scene->animEventPlayer.get())
		m_system->scene->animEventPlayer->Play(character, event);
}

static void SetDesiredMotionParameters(ISkeletonAnim& skeletonAnimation, int layer, const MotionParameters& params)
{
	for (int i = 0; i < eMotionParamID_COUNT; ++i)
	{
		EMotionParamID id = EMotionParamID(i);
		float value = params.values[i];

		uint32 numAnimsLayer0 = skeletonAnimation.GetNumAnimsInFIFO(layer);
		for (uint32 a = 0; a < numAnimsLayer0; ++a)
		{
			int animCount = skeletonAnimation.GetNumAnimsInFIFO(layer);
			for (int i = 0; i < animCount; ++i)
			{
				CAnimation& anim = skeletonAnimation.GetAnimFromFIFO(layer, i);
				SParametricSampler* sampler = anim.GetParametricSampler();
				if (!sampler)
					continue;
				uint32 endOfCycle = anim.GetEndOfCycle();
				for (uint32 d = 0; d < sampler->m_numDimensions; d++)
				{
					if (sampler->m_MotionParameterID[d] == id)
					{
						uint32 locked = sampler->m_MotionParameterFlags[d] & CA_Dim_LockedParameter;
						uint32 init = sampler->m_MotionParameterFlags[d] & CA_Dim_Initialized;
						if (endOfCycle || init == 0 || locked == 0) // if already initialized and locked, then we can't change the parameter any more
						{
							sampler->m_MotionParameter[d] = value;
							sampler->m_MotionParameterFlags[d] |= CA_Dim_Initialized;
						}
					}
				}
			}
		}
	}
}

static void SetCharacterAnimationNormalizedTime(ICharacterInstance* character, float normalizedTime, int activeLayer = -1)
{
	if (!character)
		return;
	ISkeletonAnim& skeletonAnimation = *character->GetISkeletonAnim();
	for (int layerId = 0; layerId < ISkeletonAnim::LayerCount; ++layerId)
	{
		if (activeLayer == layerId)
			continue;
		CAnimation* animation = &skeletonAnimation.GetAnimFromFIFO(layerId, 0);
		if (!animation)
			return;
		const float oldTimeNormalized = skeletonAnimation.GetAnimationNormalizedTime(animation);
		const float newTimeNormalized = clamp_tpl(normalizedTime, 0.f, 1.f);
		skeletonAnimation.SetAnimationNormalizedTime(animation, newTimeNormalized);
	}
}

void CharacterDocument::PreRender(const SRenderContext& context)
{
	if (!m_compressedCharacter)
		return;

	m_cvar_drawEdges->Set(m_displayOptions->rendering.showEdges ? 1 : 0);
	m_cvar_drawLocator->Set(m_displayOptions->animation.showLocator ? 1 : 0);

	bool featureTestOverridesUpdate = false;
	if (m_system->scene->runFeatureTest && m_system->scene->featureTest)
		featureTestOverridesUpdate = m_system->scene->featureTest->OverrideUpdate();

	if (m_displayOptions->animation.resetCharacter)
	{
		m_PhysicalLocation.SetIdentity();
		m_system->scene->layers.layers.clear();
		m_system->scene->SignalPlaybackLayersChanged(false);
		m_system->scene->SignalChanged(false);
		m_displayOptions->animation.resetCharacter = 0;
	}

	ICharacterInstance* pInstanceBase = m_compressedCharacter;
	if (pInstanceBase && !featureTestOverridesUpdate)
	{
		f32 FrameTime = GetIEditor()->GetSystem()->GetITimer()->GetFrameTime();
		m_AverageFrameTime = pInstanceBase->GetAverageFrameTime();

		pInstanceBase->GetISkeletonPose()->SetForceSkeletonUpdate(1);

		uint32 flags = m_compressedCharacter->GetCharEditMode() & ~CA_DrawSocketLocation;
		if (m_displayOptions->attachmentGizmos || m_displayOptions->poseModifierGizmos)
			flags |= CA_DrawSocketLocation;
		m_compressedCharacter->SetCharEditMode(flags);

		ISkeletonAnim& skeletonAnim = *pInstanceBase->GetISkeletonAnim();
		ISkeletonPose& skeletonPose = *pInstanceBase->GetISkeletonPose();
		IDefaultSkeleton& defaultSkeleton = pInstanceBase->GetIDefaultSkeleton();

		float absCurrentSlope = 0.0f;
		{
			bool gotSlope = false;
			for (int layer = 0; layer < m_system->scene->layers.layers.size(); ++layer)
			{
				const MotionParameters& params = m_system->scene->layers.layers[layer].motionParameters;
				if (!gotSlope && params.enabled[eMotionParamID_TravelSlope])
					absCurrentSlope = params.values[eMotionParamID_TravelSlope];
				SetDesiredMotionParameters(skeletonAnim, layer, params);
				if (m_uncompressedCharacter)
					SetDesiredMotionParameters(*m_uncompressedCharacter->GetISkeletonAnim(), layer, params);
			}
		}

		auto updatePoseBlender = [&](IAnimationPoseBlenderDir* dir, const AimParameters& aimParameters, int layer, int fadeoutAngle)
		{
			if (dir)
			{
				dir->SetPolarCoordinatesOffset(Vec2(aimParameters.offsetX, aimParameters.offsetY));
				bool enabled = layer >= 0;
				dir->SetState(enabled);
				if (enabled)
				{
					dir->SetPolarCoordinatesSmoothTimeSeconds(aimParameters.smoothTime);
					dir->SetLayer(layer);
					if (aimParameters.direction == aimParameters.AIM_CAMERA)
						dir->SetTarget(context.camera->GetPosition());
					else if (aimParameters.direction == aimParameters.AIM_TARGET)
						dir->SetTarget(m_PhysicalLocation * aimParameters.targetPosition);
					else
						dir->SetTarget(m_PhysicalLocation * Vec3(0.0f, 100.0f, 1.75f));
					dir->SetFadeoutAngle(DEG2RAD(fadeoutAngle));
				}
			}
		};
		updatePoseBlender(skeletonPose.GetIPoseBlenderAim(), m_system->scene->GetAimParameters(), m_system->scene->aimLayer, 180);
		updatePoseBlender(skeletonPose.GetIPoseBlenderLook(), m_system->scene->GetLookParameters(), m_system->scene->lookLayer, 120);

		const CCamera& camera = *context.camera;
		float fDistance = (camera.GetPosition() - m_PhysicalLocation.t).GetLength();
		float fZoomFactor = 0.001f + 0.999f * (RAD2DEG(camera.GetFov()) / 60.f);

		SAnimationProcessParams params;
		params.locationAnimation = m_PhysicalLocation;
		params.bOnRender = 0;
		params.zoomAdjustedDistanceFromCamera = fDistance * fZoomFactor;

		pInstanceBase->StartAnimationProcessing(params);

		bool uncompressedCharacterUsed = m_uncompressedCharacter && m_displayOptions->animation.compressionPreview == COMPRESSION_PREVIEW_BOTH;
		if (uncompressedCharacterUsed)
			m_uncompressedCharacter->StartAnimationProcessing(params);

		const bool extractRootMotion = m_displayOptions->animation.movement == CHARACTER_MOVEMENT_CONTINUOUS || m_displayOptions->animation.movement == CHARACTER_MOVEMENT_INPLACE;
		skeletonAnim.SetAnimationDrivenMotion(extractRootMotion);
		if (uncompressedCharacterUsed)
			m_uncompressedCharacter->GetISkeletonAnim()->SetAnimationDrivenMotion(extractRootMotion);

		// get relative movement
		QuatT relMove = skeletonAnim.GetRelMovement();

		// Determine normalizedTime/relativeMovement by actual selected (active) layer
		const int activeLayerId = m_system->scene->layers.activeLayer;
		const uint32 activeLayerAnimationCount = skeletonAnim.GetNumAnimsInFIFO(activeLayerId);
		if (activeLayerAnimationCount == 1)
		{
			CAnimation* pAnimation = &skeletonAnim.GetAnimFromFIFO(activeLayerId, 0);
			if (pAnimation)
			{
				if (m_bPaused)
				{
					//At this point the animation pose was already fully updated by StartAnimationProcessing(params)
					relMove = m_lastCalculateRelativeMovement;  //if we set a new time with 'scrubbing' then we have to work with one frame delay (or we will see foot-sliding at low framerate)
					//Read back the new 'locator movement' immediately and use it next frame.
					m_lastCalculateRelativeMovement = skeletonAnim.CalculateRelativeMovement(-1.0f); //there is no "delta-time" in scrub-mode
				}
				else
				{
					m_NormalizedTime = skeletonAnim.GetAnimationNormalizedTime(pAnimation);
				}
			}
		}

		// In case of scrubbing [paused animation]: set normalized time on all layers 
		if (m_bPaused)
		{
			const f32 fSmoothTime = m_playbackOptions.smoothTimelineSlider ? 0.2f : 0.0f;
			SmoothCD(m_NormalizedTimeSmooth, m_NormalizedTimeRate, FrameTime, m_NormalizedTime, fSmoothTime);
			for (auto& it : m_system->scene->layers.layers)
			{
				const int& layerId = it.layerId;
				const uint32 animationCount = skeletonAnim.GetNumAnimsInFIFO(layerId);
				if (animationCount != 1) continue;

				CAnimation* pAnimation = &skeletonAnim.GetAnimFromFIFO(layerId, 0);
				if (!pAnimation) continue;

				// Set a new animation-time. We will see the effect one frame later.
				skeletonAnim.SetAnimationNormalizedTime(pAnimation, m_NormalizedTimeSmooth, 1);

				if (!uncompressedCharacterUsed) continue;
				CAnimation* pUncompressedAnimation = &m_uncompressedCharacter->GetISkeletonAnim()->GetAnimFromFIFO(layerId, 0);
				if (!pUncompressedAnimation) continue;
				m_uncompressedCharacter->GetISkeletonAnim()->SetAnimationNormalizedTime(pUncompressedAnimation, m_NormalizedTimeSmooth, 1);
			}
		}

		m_PhysicalLocation.s = 1.0f;  //uniform scaling is missing
		m_PhysicalLocation = m_PhysicalLocation * relMove;
		m_PhysicalLocation.q.Normalize();

		if (m_displayOptions->animation.movement == CHARACTER_MOVEMENT_CONTINUOUS)
			pInstanceBase->SetAttachmentLocation_DEPRECATED(m_PhysicalLocation);

		pInstanceBase->FinishAnimationComputations();
		if (uncompressedCharacterUsed)
			m_uncompressedCharacter->FinishAnimationComputations();

		*m_viewportState = context.viewport->GetState();

		// update grid spacing
		if (m_displayOptions->animation.resetGrid)
		{
			m_displayOptions->animation.resetGrid = false;
			SViewportState vpState;
			m_viewportState->gridCellOffset = vpState.gridCellOffset;
			m_viewportState->gridOrigin = vpState.gridOrigin;
		}
		else if (extractRootMotion)
		{
			Vec3 gridOffset = m_PhysicalLocation.q * -relMove.t;
			m_viewportState->gridCellOffset += gridOffset;
			m_viewportState->gridCellOffset.z = 0;

			if (m_viewportState->gridCellOffset.x >= m_displayOptions->viewport.grid.spacing || m_viewportState->gridCellOffset.x <= -m_displayOptions->viewport.grid.spacing)
				m_viewportState->gridCellOffset.x = 0;

			if (m_viewportState->gridCellOffset.y >= m_displayOptions->viewport.grid.spacing || m_viewportState->gridCellOffset.y <= -m_displayOptions->viewport.grid.spacing)
				m_viewportState->gridCellOffset.y = 0;
		}

		if (m_displayOptions->animation.movement == CHARACTER_MOVEMENT_INPLACE)
		{
			m_PhysicalLocation.t.zero();
		}

		// reset location if too far away from the origin
		if (m_PhysicalLocation.t.len() > 1000.0f)
			m_PhysicalLocation.t = Vec3(0);

		// set camera origin
		QuatT cameraOrigin(IDENTITY);
		int32 cameraTargetJointID = -1;

		if (m_displayOptions->followJoint.jointName.size())
		{
			cameraTargetJointID = defaultSkeleton.GetJointIDByName(m_displayOptions->followJoint.jointName);
			if (cameraTargetJointID > -1)
			{
				QuatT jointAbs = skeletonPose.GetAbsJointByID(cameraTargetJointID);
				if (m_displayOptions->followJoint.useRotation)
					cameraOrigin.q = jointAbs.q;

				if (m_displayOptions->followJoint.usePosition)
					cameraOrigin.t = jointAbs.t;
			}
		}

		Quat slope = Quat::CreateRotationAA(absCurrentSlope, m_PhysicalLocation.q.GetColumn0());
		m_viewportState->gridOrigin = QuatT(m_PhysicalLocation.t, slope);
		m_viewportState->cameraParentFrame = QuatT(m_viewportState->gridOrigin.t, IDENTITY) * cameraOrigin;

		// update viewport camera
		if (m_displayOptions->followJoint.lockAlign && cameraTargetJointID >= 0)
		{
			if (m_displayOptions->followJoint.usePosition)
				m_viewportState->cameraTarget.t = Vec3(0);

			if (m_displayOptions->followJoint.useRotation)
				m_viewportState->cameraTarget.q = Quat(IDENTITY);
		}

		// fix camera position offset on position toggle
		if (m_displayOptions->animation.updateCameraTarget)
		{
			m_displayOptions->animation.updateCameraTarget = false;

			QuatT oldCameraTargetAbsolute = context.viewport->GetState().cameraParentFrame * m_viewportState->cameraTarget;
			QuatT newCameraTarget = m_viewportState->cameraParentFrame.GetInverted() * oldCameraTargetAbsolute;

			m_viewportState->cameraTarget.t = newCameraTarget.t;
			m_viewportState->lastCameraTarget = m_viewportState->cameraTarget;
		}

		context.viewport->SetState(*m_viewportState);

		f32 fDistanceFromOrigin = m_PhysicalLocation.t.GetLength();

		m_LocalEntityMat = Matrix34(m_PhysicalLocation);

		if (m_Callbacks.size() > 0)
		{
			IMaterial* pMaterial = 0;
			if (m_compressedCharacter)
				pMaterial = m_compressedCharacter->GetIMaterial();

			TCallbackVector::iterator itEnd = m_Callbacks.end();
			for (TCallbackVector::iterator it = m_Callbacks.begin(); it != itEnd; ++it)
			{
				(*it)->SetupShaderParams(m_pShaderPublicParams, pMaterial);
			}
		}

		// update physics
		if (m_AverageFrameTime > 0)
		{
			pe_params_flags pf;
			pf.flagsOR = pef_update | pef_ignore_areas;

			IPhysicalEntity* pCharBasePhys = skeletonPose.GetCharacterPhysics();
			if (pCharBasePhys)
				pCharBasePhys->SetParams(&pf);
			for (int i = 0; 1; i++)
			{
				IPhysicalEntity* pCharPhys = skeletonPose.GetCharacterPhysics(i);
				if (pCharPhys == 0)
					break;
				pCharPhys->SetParams(&pf);
			}

			PhysicsVars* pVars = gEnv->pPhysicalWorld->GetPhysVars();
			pVars->helperOffset.zero();
			pVars->timeScalePlayers = 1.0f;

			pe_action_move am;
			am.dir.zero();
			am.iJump = 3;
			m_pPhysicalEntity->Action(&am);

			QuatT wlocation = QuatT(m_PhysicalLocation);
			pe_params_pos par_pos;
			par_pos.q = wlocation.q;
			par_pos.pos = wlocation.t;
			par_pos.scale = 1.0f;
			m_pPhysicalEntity->SetParams(&par_pos);

			int iOutOfBounds = pVars->iOutOfBounds;
			pVars->iOutOfBounds = 3;
			gEnv->pPhysicalWorld->TimeStep(m_AverageFrameTime, ent_independent | ent_living | ent_flagged_only);
			pVars->iOutOfBounds = iOutOfBounds;

			pf.flagsOR = 0;
			pf.flagsAND = ~pef_update;
			if (pCharBasePhys)
				pCharBasePhys->SetParams(&pf);
			for (int i = 0; skeletonPose.GetCharacterPhysics(i); i++)
				skeletonPose.GetCharacterPhysics(i)->SetParams(&pf);
		}

		//	pInstanceBase->SetAttachmentLocation_DEPRECATED( m_PhysicalLocation );

		IAttachmentManager* attachmentManager = m_compressedCharacter->GetIAttachmentManager();
		const BlendShapeOptions& blendShapeOptions = m_system->scene->blendShapeOptions;
		for (size_t i = 0; i < blendShapeOptions.skins.size(); ++i)
		{
			const BlendShapeSkin& skin = blendShapeOptions.skins[i];
			IAttachment* iattachment = attachmentManager->GetInterfaceByName(skin.name.c_str());
			IAttachmentSkin* attachment = iattachment ? iattachment->GetIAttachmentSkin() : 0;
			if (!attachment)
				continue;
			ISkin* iskin = attachment->GetISkin();
			if (!iskin)
				continue;
			IVertexAnimation* vertexAnimation = attachment->GetIVertexAnimation();
			if (!vertexAnimation)
				continue;

			vertexAnimation->OverrideFrameWeights(blendShapeOptions.overrideWeights);
			vertexAnimation->ClearAllFramesWeight();
			if (blendShapeOptions.overrideWeights)
			{
				for (size_t i = 0; i < skin.params.size(); ++i)
					vertexAnimation->OverrideFrameWeightByName(iskin, skin.params[i].name, skin.params[i].weight);
			}
		}
	}

	if (m_system->scene->runFeatureTest && m_system->scene->featureTest)
		m_system->scene->featureTest->Update(this);

}

void CharacterDocument::Render(const SRenderContext& context)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	if (!m_compressedCharacter)
		return;

	if (m_system->scene->animEventPlayer)
		m_system->scene->animEventPlayer->Render(QuatT(m_PhysicalLocation), *context.renderParams, *context.passInfo);

	DrawCharacter(m_compressedCharacter, context);

	if (m_system->scene->runFeatureTest && m_system->scene->featureTest.get())
		m_system->scene->featureTest->Render(context, this);

	ICharacterManager* pCharMan = gEnv->pCharacterManager;
	if (pCharMan)
		pCharMan->UpdateStreaming(-1, -1);

	m_compressionMachine->GetAnimationStateText(&m_compressedStateTextCache, true);
	if (m_bindPoseEnabled)
	{
		StateText t;
		t.text = "Bind Pose Enabled";
		t.type = StateText::WARNING;
		m_compressedStateTextCache.push_back(t);
	}

	IRenderer* renderer = gEnv->pRenderer;
	DrawAnimationStateText(renderer, m_pAuxRenderer, m_compressedStateTextCache);

	if (m_system->scene->animEventPlayer)
		m_system->scene->animEventPlayer->Update(m_compressedCharacter, QuatT(m_PhysicalLocation.q, m_PhysicalLocation.t), context.camera->GetMatrix());
}

void CharacterDocument::PreRenderOriginal(const SRenderContext& context)
{
	context.viewport->SetState(*m_viewportState);
}

void CharacterDocument::RenderOriginal(const SRenderContext& context)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	if (m_uncompressedCharacter)
		DrawCharacter(m_uncompressedCharacter, context);

	IRenderer* renderer = gEnv->pRenderer;

	m_compressionMachine->GetAnimationStateText(&m_uncompressedStateTextCache, false);
	DrawAnimationStateText(renderer, m_pAuxRenderer, m_uncompressedStateTextCache);
}

void CharacterDocument::DrawCharacter(ICharacterInstance* pInstanceBase, const SRenderContext& context)
{
	using namespace Private_CharacterDocument;
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	m_pAuxRenderer->SetRenderFlags(e_Def3DPublicRenderflags);

	f32 FrameTime = GetIEditor()->GetSystem()->GetITimer()->GetFrameTime();
	int yPos = 162;

	// draw joint names
	if (m_displayOptions->skeleton.showJointNames)
	{
		ISkeletonPose& skeletonPose = *pInstanceBase->GetISkeletonPose();
		IDefaultSkeleton& defaultSkeleton = pInstanceBase->GetIDefaultSkeleton();
		uint32 numJoints = defaultSkeleton.GetJointCount();
		bool filtered = !m_displayOptions->skeleton.jointFilter.empty();


		for (int j = 0; j < numJoints; j++)
		{
			const char* pJointName = defaultSkeleton.GetJointNameByID(j);
			if (filtered && (strstri(pJointName, m_displayOptions->skeleton.jointFilter) == 0))
				continue;

			const QuatT jointTM = QuatT(m_PhysicalLocation * skeletonPose.GetAbsJointByID(j));

			SDrawTextInfo ti;
			ti.scale = Vec2(m_displayOptions->skeleton.showJointNamesFontSize);
			ti.flags = EDrawTextFlags::eDrawText_FixedSize;

			m_pAuxRenderer->RenderTextQueued(jointTM.t, ti, pJointName);
		}
	}

	// draw last animation driven samples
	if (m_displayOptions->animation.showTrail && (m_displayOptions->animation.movement == CHARACTER_MOVEMENT_CONTINUOUS))
	{
		m_animDrivenSamples.Add(FrameTime, m_PhysicalLocation.t);
		m_animDrivenSamples.Draw(m_pAuxRenderer);
	}
	else
		m_animDrivenSamples.Reset();

	// draw character
	SRendParams rp = *context.renderParams;
	assert(rp.pMatrix);
	assert(rp.pPrevMatrix);

	rp.pMatrix = &m_LocalEntityMat;
	rp.pPrevMatrix = &m_PrevLocalEntityMat;
	rp.fDistance = (m_PhysicalLocation.t - context.camera->GetPosition()).GetLength();

	if (m_pShaderPublicParams)
		m_pShaderPublicParams->AssignToRenderParams(rp);

	const SRenderingPassInfo& passInfo = *context.passInfo;
	gEnv->p3DEngine->PrecacheCharacter(NULL, 1.f, pInstanceBase, pInstanceBase->GetIMaterial(), m_LocalEntityMat, 0, 1.f, 4, true, passInfo);
	pInstanceBase->SetViewdir(context.camera->GetViewdir());
	pInstanceBase->Render(rp, passInfo);

	// draw DCC tool origin
	if (m_displayOptions->animation.showDccToolOrigin)
	{
		const ISkeletonAnim& skeletonAnim = *pInstanceBase->GetISkeletonAnim();

		const int animCount = skeletonAnim.GetNumAnimsInFIFO(0);
		if (animCount > 0)
		{
			const IAnimationSet& animationSet = *pInstanceBase->GetIAnimationSet();

			const CAnimation& topAnimation = skeletonAnim.GetAnimFromFIFO(0, animCount - 1);

			QuatT dccToolOriginInv;
			animationSet.GetAnimationDCCWorldSpaceLocation(topAnimation.GetAnimationId(), dccToolOriginInv);

			const QuatT dccToolOrigin = dccToolOriginInv.GetInverted();

			DrawFrame(m_pAuxRenderer, dccToolOrigin, .3f);

			IRenderAuxText::Draw2dLabel(12, 50, 1.6f, Col_White, false, "DCC Tool Origin: rot = %f (%f %f %f)", dccToolOrigin.q.w, dccToolOrigin.q.v.x, dccToolOrigin.q.v.y, dccToolOrigin.q.v.z);
			IRenderAuxText::Draw2dLabel(12, 65, 1.6f, Col_White, false, "DCC Tool Origin: pos = %f %f %f", dccToolOrigin.t.x, dccToolOrigin.t.y, dccToolOrigin.t.z);
		}
	}

	// draw physics
	if (m_displayOptions->physics.showPhysicalProxies != DisplayPhysicsOptions::DISABLED || m_displayOptions->physics.showRagdollJointLimits != DisplayPhysicsOptions::NONE)
	{
		m_pAuxRenderer->SetRenderFlags(e_Mode3D | e_AlphaBlended | e_FillModeSolid | e_CullModeNone | e_DepthWriteOff | e_DepthTestOn);
		IPhysicsDebugRenderer* pPhysRender = GetIEditor()->GetSystem()->GetIPhysicsDebugRenderer();
		pPhysRender->UpdateCamera(*context.camera);
		int iDrawHelpers = 0;
		switch (m_displayOptions->physics.showPhysicalProxies)
		{
			case DisplayPhysicsOptions::TRANSLUCENT: iDrawHelpers |= 1 << 29;
			case DisplayPhysicsOptions::SOLID: iDrawHelpers |= 2;
		}
		switch (m_displayOptions->physics.showRagdollJointLimits)
		{
			case DisplayPhysicsOptions::SELECTED: iDrawHelpers |= 1 << 28 | m_displayOptions->physics.selectedBone << 16;
			case DisplayPhysicsOptions::ALL: iDrawHelpers |= 0x10;
		}
		DrawPhysicalEntities(pPhysRender, pInstanceBase, iDrawHelpers);
		pPhysRender->Flush(FrameTime);
	}

	uint32 flags = pInstanceBase->GetCharEditMode() & ~(CA_BindPose | CA_AllowRedirection);
	if (m_system->scene->layers.bindPose)
	{
		flags |= CA_BindPose;
		if (m_system->scene->layers.allowRedirect)
			flags |= CA_AllowRedirection; //sometimes we want to see how redirected transformations change the default pose
	}
	pInstanceBase->SetCharEditMode(flags);

	if (m_displayOptions->skeleton.showJoints)
		DrawSkeleton(m_pAuxRenderer, &pInstanceBase->GetIDefaultSkeleton(), pInstanceBase->GetISkeletonPose(), QuatT(m_PhysicalLocation), m_displayOptions->skeleton.jointFilter, m_viewportState->cameraTarget);

	if (pInstanceBase == m_compressedCharacter)
	{
		if (m_displayOptions->secondaryAnimation.showDynamicProxies)
			pInstanceBase->GetIAttachmentManager()->DrawProxies(1 << 0);
		else
			pInstanceBase->GetIAttachmentManager()->DrawProxies(1 ^ -1);

		if (m_displayOptions->secondaryAnimation.showAuxiliaryProxies)
			pInstanceBase->GetIAttachmentManager()->DrawProxies(1 << 1);
		else
			pInstanceBase->GetIAttachmentManager()->DrawProxies(2 ^ -1);

		if (m_displayOptions->secondaryAnimation.showClothProxies)
			pInstanceBase->GetIAttachmentManager()->DrawProxies(1 << 2);
		else
			pInstanceBase->GetIAttachmentManager()->DrawProxies(4 ^ -1);

		if (m_displayOptions->secondaryAnimation.showRagdollProxies)
			pInstanceBase->GetIAttachmentManager()->DrawProxies(1 << 3);
		else
			pInstanceBase->GetIAttachmentManager()->DrawProxies(8 ^ -1);
	}

	if (m_displayOptions->skeleton.showSkeletonBoundingBox)
		DrawSkeletonBoundingBox(m_pAuxRenderer, pInstanceBase, QuatT(m_PhysicalLocation));
}

void CharacterDocument::CreateShaderParamCallbacks()
{
	if (m_pShaderPublicParams == NULL)
	{
		m_pShaderPublicParams = gEnv->pRenderer->CreateShaderPublicParams();
	}

#if 0
	TBoolVarVector::const_iterator itEnd = mv_bShaderParamCallbackVars.end();
	for (TBoolVarVector::iterator it = mv_bShaderParamCallbackVars.begin(); it != itEnd; ++it)
	{
		m_vars.RemoveVariable(*it);
	}

	mv_bShaderParamCallbackVars.clear();
	m_pShaderParamCallbackArray.clear();
	m_Callbacks.clear();

	uint32 iSize = _countof(g_shaderParamCallbacks);

	m_pShaderParamCallbackArray.resize(iSize);
	mv_bShaderParamCallbackVars.resize(iSize);

	for (uint i = 0; i < iSize; ++i)
	{
		CreateShaderParamCallback(g_shaderParamCallbacks[i].first, g_shaderParamCallbacks[i].second, i);
	}
#endif
}

void CharacterDocument::CreateShaderParamCallback(const char* name, const char* UIname, uint32 iIndex)
{
#if 0
	if (CryCreateClassInstance(name, m_pShaderParamCallbackArray[iIndex]))
	{
		if (m_pAnimationBrowserPanel)
			m_pShaderParamCallbackArray[iIndex]->SetCharacterInstance(m_pAnimationBrowserPanel->GetSelected_SKEL());
		else
			m_pShaderParamCallbackArray[iIndex]->SetCharacterInstance(m_compressedCharacter);

		if (m_pShaderParamCallbackArray[iIndex]->Init())
		{
			m_Callbacks.push_back(m_pShaderParamCallbackArray[iIndex]);

			if (UIname != NULL)
			{
				assert(strlen(UIname) > 0);

				CVariable<bool>* pVariable = new CVariable<bool>;
				pVariable->Set(true);
				pVariable->SetUserData((void*) &m_pShaderParamCallbackArray[iIndex]);
				m_vars.AddVariable(*pVariable, _T(UIname), functor(*this, &CModelViewportCE::OnUseShaderParamCallback));

				mv_bShaderParamCallbackVars[iIndex] = pVariable;

				return;
			}
		}
	}
#endif
}

bool CharacterDocument::HasModifiedExporerEntriesSelected() const
{
	ExplorerEntries entries;
	GetSelectedExplorerEntries(&entries);

	for (size_t i = 0; i < entries.size(); ++i)
		if (entries[i]->modified)
			return true;

	return false;
}

void CharacterDocument::GetSelectedExplorerEntries(ExplorerEntries* entries) const
{
	entries->clear();
	for (auto& panel : m_system->explorerPanels)
		panel->GetSelectedEntries(entries, BMerge(1));
}

bool CharacterDocument::HasSelectedExplorerEntries() const
{
	ExplorerEntries entries;
	GetSelectedExplorerEntries(&entries);
	return !entries.empty();
}

bool CharacterDocument::IsExplorerEntrySelected(ExplorerEntry* entry) const
{
	for (auto& panel : m_system->explorerPanels)
		if (panel->IsEntrySelected(entry))
			return true;
	return false;
}

string CharacterDocument::GetDefaultSkeletonAlias()
{
	if (!m_compressedCharacter)
		return string();
	string skeletonPath = m_compressedCharacter->GetIDefaultSkeleton().GetModelFilePath();
	string alias = m_system->compressionSkeletonList->FindSkeletonNameByPath(skeletonPath.c_str());
	if (alias.empty())
		alias = m_system->compressionSkeletonList->FindSkeletonNameByPath(m_loadedCharacterFilename);
	return alias;
}

void CharacterDocument::PreviewAnimationEntry(bool forceRecompile)
{
	size_t numLayers = m_system->scene->layers.layers.size();
	vector<SAnimSettings> animSettings(numLayers);
	vector<bool> isModified(numLayers, false);
	bool expectChrParamsReload = false;
	for (size_t i = 0; i < numLayers; ++i)
	{
		const PlaybackLayer& layer = m_system->scene->layers.layers[i];
		if (SEntry<AnimationContent>* anim = m_system->animationList->FindEntryByPath(layer.path.c_str()))
		{
			if (anim->content.importState == AnimationContent::NEW_ANIMATION)
			{
				SAnimSettings settings;
				settings.build.compression.SetZeroCompression();
				settings.build.skeletonAlias = anim->content.newAnimationSkeleton;
				animSettings[i] = settings;
				isModified[i] = true;
			}
			else
			{
				if (anim->content.importState == AnimationContent::WAITING_FOR_CHRPARAMS_RELOAD)
					expectChrParamsReload = true;
				animSettings[i] = anim->content.settings;
				isModified[i] = anim->modified;
			}
		}
		else
			continue;

		if (animSettings[i].build.skeletonAlias.empty())
			animSettings[i].build.skeletonAlias = GetDefaultSkeletonAlias();
	}

	float normalizedTime = 0.0f;
	if (m_playbackDuration > 0.0f)
		normalizedTime = m_playbackTime / m_playbackDuration;

	if (!m_system->scene->layers.layers.empty())
	{
		// this is exposed to mannequin for animation selection
		GetIEditor()->GetResourceSelectorHost()->SetGlobalSelection("animation", m_system->scene->layers.layers[0].animation.c_str());
	}
	m_compressionMachine->PreviewAnimation(m_system->scene->layers, isModified, m_showOriginalAnimation,
	                                       animSettings, normalizedTime, forceRecompile, expectChrParamsReload);

}

void CharacterDocument::TriggerAnimEventsInRange(float timeFrom, float timeTo)
{
	ExplorerEntry* explorerEntry = GetActiveAnimationEntry();
	if (!explorerEntry)
		return;
	SEntry<AnimationContent>* animation = m_system->animationList->GetEntry(explorerEntry->id);
	if (!animation)
		return;

	float normalizedFrom = m_playbackDuration > 0.0f ? timeFrom / m_playbackDuration : 0.0f;
	float normalizedTo = m_playbackDuration > 0.0f ? timeTo / m_playbackDuration : 0.0f;
	const std::vector<AnimEvent>& events = animation->content.events;
	int numEvents = (int)events.size();
	for (int i = 0; i < numEvents; ++i)
	{
		if (events[i].startTime >= normalizedFrom && events[i].startTime < normalizedTo)
		{
			AnimEventInstance eventInstance;
			events[i].ToInstance(&eventInstance);

			PlayAnimEvent(m_compressedCharacter, eventInstance);
		}
	}
}

void CharacterDocument::ScrubTime(float time, bool scrubThrough)
{
	Pause();

	if (time != m_playbackTime || m_playbackOptions != m_lastScrubPlaybackOptions)
	{
		float normalizedTime = m_playbackDuration > 0.0f ? time / m_playbackDuration : 0.0f;
		if (m_playbackOptions.loopAnimation && !m_playbackOptions.firstFrameAtEndOfTimeline)
			normalizedTime = min(1.0f - FLT_EPSILON, normalizedTime);

		if (time > m_playbackTime && scrubThrough)
		{
			TriggerAnimEventsInRange(m_playbackTime, time);
		}

		m_NormalizedTime = normalizedTime;
		m_playbackTime = time;

		m_lastScrubPlaybackOptions = m_playbackOptions;
		SignalPlaybackTimeChanged();
	}
}

void CharacterDocument::Play()
{
	if (m_playbackState != PLAYBACK_UNAVAILABLE)
	{
		m_bPaused = false;
		if (m_playbackOptions.playFromTheStart || m_NormalizedTime == 1.0f)
		{
			m_NormalizedTime = 0.0f;
			m_NormalizedTimeSmooth = 0.0f;
			m_NormalizedTimeRate = 0.0f;
			m_playbackTime = 0.0f;
			SetCharacterAnimationNormalizedTime(m_compressedCharacter, 0);
			SetCharacterAnimationNormalizedTime(m_uncompressedCharacter, 0);
		}
		
		if (m_compressedCharacter)
			m_compressedCharacter->SetPlaybackScale(GetPlaybackOptions().playbackSpeed);
		if (m_uncompressedCharacter)
			m_uncompressedCharacter->SetPlaybackScale(GetPlaybackOptions().playbackSpeed);
	}
}

static void AllowAnimEventsToTriggerAgain(ICharacterInstance* characterInstance)
{
	ISkeletonAnim& skeletonAnim = *characterInstance->GetISkeletonAnim();
	for (int layerIndex = 0; layerIndex < ISkeletonAnim::LayerCount; ++layerIndex)
	{
		const int animCount = skeletonAnim.GetNumAnimsInFIFO(layerIndex);
		for (int animIndex = 0; animIndex < animCount; ++animIndex)
		{
			CAnimation& animation = skeletonAnim.GetAnimFromFIFO(layerIndex, animIndex);
			animation.ClearAnimEventsEvaluated();
		}
	}
}

void CharacterDocument::Pause()
{
	m_bPaused = true;
	if (m_compressedCharacter)
	{
		m_compressedCharacter->SetPlaybackScale(0.0f);
		AllowAnimEventsToTriggerAgain(m_compressedCharacter);
	}
	if (m_uncompressedCharacter)
		m_uncompressedCharacter->SetPlaybackScale(0.0f);
}

void CharacterDocument::EnableAudio(bool enable)
{
	if (m_system->scene->animEventPlayer)
		m_system->scene->animEventPlayer->EnableAudio(enable);
}

PlaybackState CharacterDocument::GetPlaybackState() const
{
	return m_playbackState;
}

ExplorerEntry* CharacterDocument::GetActiveCharacterEntry() const
{
	SEntry<CharacterContent>* entry = m_system->characterList->GetEntryByPath<CharacterContent>(m_loadedCharacterFilename.c_str());
	if (!entry)
		return 0;

	return m_system->explorerData->FindEntryById(&*m_system->characterList, entry->id);
}

CharacterDefinition* CharacterDocument::GetLoadedCharacterDefinition() const
{
	SEntry<CharacterContent>* entry = m_system->characterList->GetEntryByPath<CharacterContent>(m_loadedCharacterFilename.c_str());
	if (!entry)
		return 0;
	return &entry->content.cdf;
}

bool CharacterDocument::IsActiveInDocument(ExplorerEntry* entry) const
{
	return entry == GetActiveCharacterEntry() || entry == GetActiveAnimationEntry();
}

void CharacterDocument::EnableMotionParameters()
{
	bool playbackLayersChanged = false;

	const int numLayers = m_system->scene->layers.layers.size();
	for (int layer = 0; layer < numLayers; ++layer)
	{
		MotionParameters currentMotionParameters = QueryMotionParametersFromCharacterInstance(CompressedCharacter(), layer);
		MotionParameters& storedMotionParameters = m_system->scene->layers.layers[layer].motionParameters;

		for (size_t i = 0; i < eMotionParamID_COUNT; ++i)
		{
			if (currentMotionParameters.enabled[i] != storedMotionParameters.enabled[i]
			    || currentMotionParameters.rangeMin[i] != storedMotionParameters.rangeMin[i]
			    || currentMotionParameters.rangeMax[i] != storedMotionParameters.rangeMax[i])
			{
				TransferMotionParametersValues(storedMotionParameters, currentMotionParameters);
				storedMotionParameters = currentMotionParameters;
				playbackLayersChanged = true;
				break;
			}
		}
	}

	if (playbackLayersChanged)
		m_system->scene->CheckIfPlaybackLayersChanged(false);
}

void CharacterDocument::UpdateBlendShapeParameterList()
{
	BlendShapeOptions& options = m_system->scene->blendShapeOptions;
	BlendShapeSkins oldSkins;
	oldSkins.swap(options.skins);

	ICharacterInstance* character = CompressedCharacter();
	if (!character)
		return;
	IAttachmentManager* attachmentManager = character->GetIAttachmentManager();
	int count = attachmentManager->GetAttachmentCount();
	for (int i = 0; i < count; ++i)
	{
		IAttachment* baseAttachment = attachmentManager->GetInterfaceByIndex(i);
		IAttachmentSkin* attachment = baseAttachment ? baseAttachment->GetIAttachmentSkin() : 0;
		if (!attachment)
			continue;

		ISkin* skin = attachment->GetISkin();
		if (!skin)
			continue;
		const IVertexFrames* vertexFrames = skin->GetVertexFrames();
		if (!vertexFrames)
			continue;
		int numBlendShapes = vertexFrames->GetCount();
		if (numBlendShapes)
		{
			BlendShapeSkin skin;
			skin.name = baseAttachment->GetName();
			skin.blendShapesSupported = ((baseAttachment->GetFlags() & FLAGS_ATTACH_SW_SKINNING) != 0) || ((baseAttachment->GetFlags() & FLAGS_ATTACH_COMPUTE_SKINNING) != 0);

			for (int j = 0; j < numBlendShapes; ++j)
			{
				BlendShapeParameter param;
				param.name = vertexFrames->GetNameByIndex(j);
				param.weight = 0.0f;
				skin.params.push_back(param);
			}
			std::sort(skin.params.begin(), skin.params.end());
			options.skins.push_back(skin);
		}
	}

	std::sort(options.skins.begin(), options.skins.end());

	for (size_t i = 0; i < options.skins.size(); ++i)
	{
		BlendShapeSkin& skin = options.skins[i];
		const BlendShapeSkin* oldSkin = 0;
		for (size_t j = 0; j < oldSkins.size(); ++j)
			if (skin.name == oldSkins[j].name)
			{
				oldSkin = &oldSkins[j];
				break;
			}
		if (!oldSkin)
			continue;

		for (size_t j = 0; j < skin.params.size(); ++j)
		{
			BlendShapeParameter& param = skin.params[j];
			const BlendShapeParameter* oldParam = 0;
			for (size_t j = 0; j < oldSkin->params.size(); ++j)
				if (param.name == oldSkin->params[j].name)
				{
					oldParam = &oldSkin->params[j];
					break;
				}
			if (!oldParam)
				continue;
			param.weight = oldParam->weight;
		}
	}

	m_system->scene->SignalBlendShapeOptionsChanged();
}

void CharacterDocument::OnCompressionMachineAnimationStarted()
{
	EnableMotionParameters();

	// We need to apply animation event changes after animation is loaded in
	// case caf wasn't yet loaded when animevents were changed.
	if (ExplorerEntry* animationEntry = GetActiveAnimationEntry())
	{
		if (SEntry<AnimationContent>* animation = m_system->animationList->GetEntry(animationEntry->id))
		{
			const char* animationPathConsideringCompression = m_compressionMachine->AnimationPathConsideringPreview(animation->path.c_str());
			animation->content.ApplyToCharacter(0, m_compressedCharacter, animationPathConsideringCompression, true);
		}
	}

	SignalAnimationStarted();
}

void CharacterDocument::OnAnimationForceRecompile(const char* path)
{
	ExplorerEntries selectedEntries;
	GetSelectedExplorerEntries(&selectedEntries);

	string pathWithoutExtension = path;
	PathUtil::RemoveExtension(pathWithoutExtension);

	for (size_t i = 0; i < selectedEntries.size(); ++i)
	{
		string selectedPath = selectedEntries[i]->path;
		PathUtil::RemoveExtension(selectedPath);

		if (selectedPath == pathWithoutExtension)
			TriggerAnimationPreview(PREVIEW_FORCE_RECOMPILE | PREVIEW_ALLOW_REWIND);
	}
}

}

