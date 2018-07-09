// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameEngine.h"
#include "AICoverSurface.h"
#include "AI/AIManager.h"
#include "AI/CoverSurfaceManager.h"
#include "AI/NavDataGeneration/Navigation.h"
#include "IIconManager.h"
#include "Gizmos/AxisHelper.h"
#include "Viewport.h"
#include "Objects/InspectorWidgetCreator.h"

#include <CrySystem/ICryLink.h>

IMPLEMENT_DYNCREATE(CAICoverSurface, CBaseObject);

REGISTER_CLASS_DESC(CAICoverSurfaceClassDesc);

CAICoverSurface::CAICoverSurface()
	: m_sampler(0)
	, m_surfaceID(0)
	, m_helperScale(1.0f)
	, m_aabb(AABB::RESET)
	, m_aabbLocal(AABB::RESET)
	, m_samplerResult(None)
{
	m_aabbLocal.Reset();
	UseMaterialLayersMask(false);
	SetColor(RGB(70, 130, 180));
}

CAICoverSurface::~CAICoverSurface()
{
}

bool CAICoverSurface::Init(CBaseObject* prev, const string& file)
{
	if (CBaseObject::Init(prev, file))
	{
		CreatePropertyVars();

		if (prev)
		{
			CAICoverSurface* original = static_cast<CAICoverSurface*>(prev);
			m_propertyValues = original->m_propertyValues;
		}
		else
			SetPropertyVarsFromParams(ICoverSampler::Params()); // defaults

		return true;
	}

	return false;
}

bool CAICoverSurface::CreateGameObject()
{
	if (CBaseObject::CreateGameObject())
	{
		GetIEditorImpl()->GetAI()->GetCoverSurfaceManager()->AddSurfaceObject(this);

		return true;
	}

	return false;
}

void CAICoverSurface::Done()
{
	LOADING_TIME_PROFILE_SECTION_ARGS(GetName().c_str());
	if (m_sampler)
	{
		m_sampler->Release();
		m_sampler = 0;
	}

	ClearSurface();

	CAIManager* pAIManager(GetIEditorImpl()->GetAI());
	CCoverSurfaceManager* pCoverSurfaceManager = pAIManager->GetCoverSurfaceManager();

	pCoverSurfaceManager->RemoveSurfaceObject(this);

	CBaseObject::Done();
}

void CAICoverSurface::DeleteThis()
{
	delete this;
}

void CAICoverSurface::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& disp = objRenderHelper.GetDisplayContextRef();
	if (IsFrozen())
		disp.SetFreezeColor();
	else
		disp.SetColor(GetColor());

	Matrix34 scale(Matrix34::CreateScale(Vec3(gGizmoPreferences.helperScale * GetHelperScale())));

	objRenderHelper.Render(GetWorldTM() * scale);

	if (m_sampler)
	{
		switch (m_sampler->Update(0.025f, 2.0f))
		{
		case ICoverSampler::Finished:
			{
				CommitSurface();
				ReleaseSampler();
				m_samplerResult = Success;
			}
			break;
		case ICoverSampler::Error:
			{
				ClearSurface();
				ReleaseSampler();
				m_samplerResult = Error;
			}
			break;
		default:
			break;
		}
	}

	if (m_surfaceID && IsSelected() && (GetIEditorImpl()->GetSelection()->GetCount() < 15))
	{
		gEnv->pAISystem->GetCoverSystem()->DrawSurface(m_surfaceID);
	}

	if (m_samplerResult == Error)
	{
		DisplayBadCoverSurfaceObject(disp);
	}

	DrawDefault(disp);
}

bool CAICoverSurface::HitTest(HitContext& hitContext)
{
	Vec3 origin = GetWorldPos();
	float radius = GetHelperSize() * GetHelperScale();

	Vec3 w = origin - hitContext.raySrc;
	w = hitContext.rayDir.Cross(w);

	float d = w.GetLengthSquared();
	if (d < (radius * radius) + hitContext.distanceTolerance)
	{
		Vec3 i0;
		if (Intersect::Ray_SphereFirst(Ray(hitContext.raySrc, hitContext.rayDir), Sphere(origin, radius), i0))
		{
			hitContext.dist = hitContext.raySrc.GetDistance(i0);

			return true;
		}

		hitContext.dist = hitContext.raySrc.GetDistance(origin);

		return true;
	}

	return false;
}

void CAICoverSurface::GetBoundBox(AABB& aabb)
{
	aabb = m_aabb;

	float extent = GetHelperSize() * GetHelperScale();
	Vec3 world = GetWorldPos();

	aabb.Add(world - Vec3(extent));
	aabb.Add(world + Vec3(extent));
}

void CAICoverSurface::GetLocalBounds(AABB& aabb)
{
	aabb = m_aabbLocal;

	float extent = GetHelperSize() * GetHelperScale();
	aabb.Add(Vec3(-extent));
	aabb.Add(Vec3(extent));
}

void CAICoverSurface::SetHelperScale(float scale)
{
	m_helperScale = scale;
}

float CAICoverSurface::GetHelperScale()
{
	return m_helperScale;
}

float CAICoverSurface::GetHelperSize() const
{
	return 0.5f * gGizmoPreferences.helperScale;
}

const CoverSurfaceID& CAICoverSurface::GetSurfaceID() const
{
	return m_surfaceID;
}

void CAICoverSurface::SetSurfaceID(const CoverSurfaceID& coverSurfaceID)
{
	m_surfaceID = coverSurfaceID;
}

void CAICoverSurface::Generate()
{
	CreateSampler();
	StartSampling();

	while (m_sampler->Update(2.0f) == ICoverSampler::InProgress)
		;

	if ((m_sampler->GetState() != ICoverSampler::Error) && (m_sampler->GetSampleCount() > 1))
		CommitSurface();

	ReleaseSampler();
}

void CAICoverSurface::ValidateGenerated()
{
	ICoverSystem::SurfaceInfo surfaceInfo;
	// Check surface is not empty
	if (!m_surfaceID ||	!gEnv->pAISystem->GetCoverSystem()->GetSurfaceInfo(m_surfaceID, &surfaceInfo) || (surfaceInfo.sampleCount < 2))
	{
		Vec3 pos = GetWorldPos();

		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "AI Cover Surface '%s' at (%.2f, %.2f, %.2f) is empty! %s", (const char*)GetName(), pos.x, pos.y, pos.z,
			CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "general.select_and_go_to_object %s", GetName()));
	}
}

void CAICoverSurface::CreateSampler()
{
	if (!gEnv->pAISystem)
		return;

	if (m_sampler)
		m_sampler->Release();

	m_sampler = gEnv->pAISystem->GetCoverSystem()->CreateCoverSampler();
}

void CAICoverSurface::ReleaseSampler()
{
	if (m_sampler)
	{
		m_sampler->Release();
		m_sampler = 0;
	}
}

void CAICoverSurface::StartSampling()
{
	if (!gEnv->pAISystem)
		return;

	ICoverSampler::Params params(GetParamsFromPropertyVars());

	Matrix34 worldTM = GetWorldTM();
	params.position = worldTM.GetTranslation();
	params.direction = worldTM.GetColumn1();

	m_sampler->StartSampling(params);
}

void CAICoverSurface::CommitSurface()
{
	uint32 sampleCount = m_sampler->GetSampleCount();
	if (sampleCount)
	{
		ICoverSystem::SurfaceInfo surfaceInfo;
		surfaceInfo.sampleCount = sampleCount;
		surfaceInfo.samples = m_sampler->GetSamples();
		surfaceInfo.flags = m_sampler->GetSurfaceFlags();

		if (!m_surfaceID)
			m_surfaceID = gEnv->pAISystem->GetCoverSystem()->AddSurface(surfaceInfo);
		else
			gEnv->pAISystem->GetCoverSystem()->UpdateSurface(m_surfaceID, surfaceInfo);

		Matrix34 invWorldTM = GetWorldTM().GetInverted();
		m_aabbLocal = AABB(AABB::RESET);

		for (uint32 i = 0; i < surfaceInfo.sampleCount; ++i)
		{
			const ICoverSampler::Sample& sample = surfaceInfo.samples[i];

			Vec3 position = invWorldTM.TransformPoint(sample.position);

			m_aabbLocal.Add(position);
			m_aabbLocal.Add(position + Vec3(0.0f, 0.0f, sample.GetHeight()));
		}

		m_aabb = m_sampler->GetAABB();
	}
	else
		ClearSurface();
}

void CAICoverSurface::ClearSurface()
{
	if (m_surfaceID)
	{
		if (gEnv->pAISystem)
			gEnv->pAISystem->GetCoverSystem()->RemoveSurface(m_surfaceID);
		m_surfaceID = CoverSurfaceID(0);
	}

	m_aabb = AABB(AABB::RESET);
	m_aabbLocal = AABB(AABB::RESET);
}

void CAICoverSurface::SetPropertyVarsFromParams(const ICoverSampler::Params& params)
{
	m_propertyValues.limitLeft = params.limitLeft;
	m_propertyValues.limitRight = params.limitRight;

	m_propertyValues.limitDepth = params.limitDepth;
	m_propertyValues.limitHeight = params.limitHeight;

	m_propertyValues.widthInterval = params.widthSamplerInterval;
	m_propertyValues.heightInterval = params.heightSamplerInterval;

	m_propertyValues.maxStartHeight = params.maxStartHeight;
	m_propertyValues.simplifyThreshold = params.simplifyThreshold;
}

ICoverSampler::Params CAICoverSurface::GetParamsFromPropertyVars()
{
	ICoverSampler::Params params;

	params.limitDepth = m_propertyValues.limitDepth;
	params.limitHeight = m_propertyValues.limitHeight;

	params.limitLeft = m_propertyValues.limitLeft;
	params.limitRight = m_propertyValues.limitRight;

	params.widthSamplerInterval = m_propertyValues.widthInterval;
	params.heightSamplerInterval = m_propertyValues.heightInterval;

	params.maxStartHeight = m_propertyValues.maxStartHeight;
	params.simplifyThreshold = m_propertyValues.simplifyThreshold;

	return params;
}

void CAICoverSurface::Serialize(CObjectArchive& archive)
{
	CBaseObject::Serialize(archive);

	// just do a dummy-serialization for backwards compatibility (CommitSurface() will allocate a fresh CoverSurfaceID if necessary)
	CoverSurfaceID dummyID(0);
	SerializeValue(archive, "SurfaceID", dummyID);

	m_propertyValues.Serialize(*this, archive);
}

XmlNodeRef CAICoverSurface::Export(const string& levelPath, XmlNodeRef& xmlNode)
{
	return 0;
}

void CAICoverSurface::InvalidateTM(int whyFlags)
{
	CBaseObject::InvalidateTM(whyFlags);

	if (!m_sampler)
		CreateSampler();

	StartSampling();
}

void CAICoverSurface::SetSelected(bool bSelected)
{
	CBaseObject::SetSelected(bSelected);

	if (bSelected)
	{
		ClearSurface();
		ReleaseSampler();

		CreateSampler();
		StartSampling();
	}
}

void CAICoverSurface::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CBaseObject::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CAICoverSurface>("AI Cover Surface", [](CAICoverSurface* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		pObject->m_propertyVars->Serialize(ar);
	});
}

void CAICoverSurface::OnPropertyVarChange(IVariable* var)
{
	CBaseObject::OnPropertyChanged(var);

	ReleaseSampler();
	CreateSampler();

	StartSampling();
}

void CAICoverSurface::CreatePropertyVars()
{
	m_propertyVars.reset(new CVarBlock());

	m_propertyVars->AddVariable(m_propertyValues.sampler, "Sampler");
	m_propertyVars->AddVariable(m_propertyValues.limitLeft, "Limit Left");
	m_propertyVars->AddVariable(m_propertyValues.limitRight, "Limit Right");
	m_propertyVars->AddVariable(m_propertyValues.limitDepth, "Limit Depth");
	m_propertyVars->AddVariable(m_propertyValues.limitHeight, "Limit Height");

	m_propertyVars->AddVariable(m_propertyValues.widthInterval, "Sample Width");
	m_propertyVars->AddVariable(m_propertyValues.heightInterval, "Sample Height");

	m_propertyVars->AddVariable(m_propertyValues.maxStartHeight, "Max Start Height");

	m_propertyVars->AddVariable(m_propertyValues.simplifyThreshold, "Simplification Threshold");

	m_propertyValues.sampler->AddEnumItem(string("Default"), string("Default"));
	m_propertyValues.sampler->Set(string("Default"));

	m_propertyValues.limitLeft->SetLimits(0.0f, 50.0f, 0.05f);
	m_propertyValues.limitRight->SetLimits(0.0f, 50.0f, 0.05f);
	m_propertyValues.limitDepth->SetLimits(0.0f, 10.0f, 0.05f);
	m_propertyValues.limitHeight->SetLimits(0.05f, 50.0f, 0.05f);
	m_propertyValues.widthInterval->SetLimits(0.05f, 1.0f, 0.05f);
	m_propertyValues.heightInterval->SetLimits(0.05f, 1.0f, 0.05f);
	m_propertyValues.maxStartHeight->SetLimits(0.0f, 2.0f, 0.05f);
	m_propertyValues.simplifyThreshold->SetLimits(0.0f, 1.0f, 0.005f);

	m_propertyVars->AddOnSetCallback(functor(*this, &CAICoverSurface::OnPropertyVarChange));
}

void CAICoverSurface::DisplayBadCoverSurfaceObject(DisplayContext& disp)
{
	IRenderAuxGeom* pRenderAux = gEnv->pRenderer->GetIRenderAuxGeom();

	const SAuxGeomRenderFlags oldFlags = pRenderAux->GetRenderFlags();
	SAuxGeomRenderFlags renderFlags(oldFlags);

	renderFlags.SetAlphaBlendMode(e_AlphaBlended);

	pRenderAux->SetRenderFlags(renderFlags);

	const Matrix34& objectTM = GetWorldTM();

	AABB objectLocalBBox;
	GetLocalBounds(objectLocalBBox);
	objectLocalBBox.Expand(Vec3(0.15f, 0.15f, 0.15f));

	bool bDisplay2D = ((disp.flags & DISPLAY_2D) != 0);
	float alpha;

	if (bDisplay2D)
	{
		// 2D viewport is not animated, therefore always show fully opaque
		alpha = 1.0f;
	}
	else
	{
		const float time = gEnv->pTimer->GetAsyncCurTime();

		alpha = clamp_tpl((1.0f + sinf(time * 2.5f)) * 0.5f, 0.25f, 0.7f);
	}

	const ColorB color(255, 0, 0, (uint8)(alpha * 255));

	const float height = 1.5f;
	const float halfHeight = height * 0.5f;
	const float objectRadius = objectLocalBBox.GetRadius();
	const Vec3 objectCenter = objectTM.TransformPoint(objectLocalBBox.GetCenter());

	disp.SetColor(color);
	disp.DrawBall(objectCenter, objectRadius);

	Vec3 p1 = objectCenter + (objectTM.GetColumn0() * (halfHeight + objectRadius));
	Vec3 p2 = p1 + objectTM.GetColumn0();
	disp.DrawCylinder(p1, p2, 0.25f, height);

	p1 = objectCenter + (objectTM.GetColumn1() * (halfHeight + objectRadius));
	p2 = p1 + objectTM.GetColumn1();
	disp.DrawCylinder(p1, p2, 0.25f, height);

	p1 = objectCenter + (objectTM.GetColumn2() * (halfHeight + objectRadius));
	p2 = p1 + objectTM.GetColumn2();
	disp.DrawCylinder(p1, p2, 0.25f, height);

	pRenderAux->SetRenderFlags(oldFlags);
}

