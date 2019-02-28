// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CameraObject.h"
#include "Viewport.h"
#include "Controls/DynamicPopupMenu.h"
#include "RenderViewport.h"
#include "ViewManager.h"
#include "Objects/InspectorWidgetCreator.h"

REGISTER_CLASS_DESC(CCameraObjectClassDesc);
REGISTER_CLASS_DESC(CCameraObjectTargetClassDesc);

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CCameraObject, CEntityObject)
IMPLEMENT_DYNCREATE(CCameraObjectTarget, CEntityObject)

#define CAMERA_COLOR       RGB(0, 255, 255)
#define CAMERA_CONE_LENGTH 4
#define CAMERABOX_RADIUS   0.7f
#define MIN_FOV_IN_DEG     0.1f

namespace
{
const float kMinNearZ = 0.002f;
const float kMaxNearZ = 10.0f;
const float kMinFarZ = 11.0f;
const float kMaxFarZ = 100000.0f;
const float kMinCameraSeed = 0.0f;
const float kMaxCameraSeed = 100000.0f;
};

//////////////////////////////////////////////////////////////////////////
CCameraObject::CCameraObject() : m_listeners(1)
{
	mv_fov = DEG2RAD(60.0f);    // 60 degrees (to fit with current game)
	mv_nearZ = DEFAULT_NEAR;
	mv_farZ = DEFAULT_FAR;
	mv_amplitudeA = Vec3(1.0f, 1.0f, 1.0f);
	mv_amplitudeAMult = 0.0f;
	mv_frequencyA = Vec3(1.0f, 1.0f, 1.0f);
	mv_frequencyAMult = 0.0f;
	mv_noiseAAmpMult = 0.0f;
	mv_noiseAFreqMult = 0.0f;
	mv_timeOffsetA = 0.0f;
	mv_amplitudeB = Vec3(1.0f, 1.0f, 1.0f);
	mv_amplitudeBMult = 0.0f;
	mv_frequencyB = Vec3(1.0f, 1.0f, 1.0f);
	mv_frequencyBMult = 0.0f;
	mv_noiseBAmpMult = 0.0f;
	mv_noiseBFreqMult = 0.0f;
	mv_timeOffsetB = 0.0f;
	m_creationStep = 0;
	mv_cameraShakeSeed = 0;
	mv_omniCamera = 0;

	SetColor(CAMERA_COLOR);
	UseMaterialLayersMask(false);
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::Done()
{
	LOADING_TIME_PROFILE_SECTION_ARGS(GetName().c_str());
	CViewManager* pManager = GetIEditorImpl()->GetViewManager();
	if (pManager && pManager->GetCameraObjectId() == GetId())
	{
		CViewport* pViewport = GetIEditorImpl()->GetViewManager()->GetSelectedViewport();
		if (pViewport && pViewport->IsRenderViewport())
			((CRenderViewport*)pViewport)->SetDefaultCamera();
	}

	CEntityObject::Done();
}

//////////////////////////////////////////////////////////////////////////
bool CCameraObject::Init(CBaseObject* prev, const string& file)
{
	bool res = CEntityObject::Init(prev, file);

	m_entityClass = "CameraSource";

	if (prev)
	{
		CBaseObjectPtr prevLookat = prev->GetLookAt();
		if (prevLookat)
		{
			CBaseObjectPtr lookat = GetObjectManager()->NewObject(prevLookat->GetClassDesc(), prevLookat);
			if (lookat)
			{
				lookat->SetPos(prevLookat->GetPos() + Vec3(3, 0, 0));
				GetObjectManager()->ChangeObjectName(lookat, string(GetName()) + " Target");
				SetLookAt(lookat);
			}
		}
	}
	UpdateCameraEntity();

	return res;
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::InitVariables()
{
	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_pVarObject->AddVariable(mv_fov, "FOV", functor(*this, &CCameraObject::OnFovChange), IVariable::DT_ANGLE);
	m_pVarObject->AddVariable(mv_nearZ, "NearZ", functor(*this, &CCameraObject::OnNearZChange), IVariable::DT_SIMPLE);
	mv_nearZ.SetLimits(kMinNearZ, kMaxNearZ);
	m_pVarObject->AddVariable(mv_farZ, "FarZ", functor(*this, &CCameraObject::OnFarZChange), IVariable::DT_SIMPLE);
	mv_farZ.SetLimits(kMinFarZ, kMaxFarZ);
	mv_cameraShakeSeed.SetLimits(kMinCameraSeed, kMaxCameraSeed);
	m_pVarObject->AddVariable(mv_omniCamera, "OmniCamera", "Omnidirectional Camera", functor(*this, &CCameraObject::OnOmniCameraChange));

	mv_shakeParamsArray.DeleteAllVariables();

	m_pVarObject->AddVariable(mv_shakeParamsArray, "ShakeParams", "Shake Parameters");
	m_pVarObject->AddVariable(mv_shakeParamsArray, mv_amplitudeA, "AmplitudeA", "Amplitude A", functor(*this, &CCameraObject::OnShakeAmpAChange));
	m_pVarObject->AddVariable(mv_shakeParamsArray, mv_amplitudeAMult, "AmplitudeAMult", "Amplitude A Mult.", functor(*this, &CCameraObject::OnShakeMultChange));
	m_pVarObject->AddVariable(mv_shakeParamsArray, mv_frequencyA, "FrequencyA", "Frequency A", functor(*this, &CCameraObject::OnShakeFreqAChange));
	m_pVarObject->AddVariable(mv_shakeParamsArray, mv_frequencyAMult, "FrequencyAMult", "Frequency A Mult.", functor(*this, &CCameraObject::OnShakeMultChange));
	m_pVarObject->AddVariable(mv_shakeParamsArray, mv_noiseAAmpMult, "NoiseAAmpMult", "Noise A Amp. Mult.", functor(*this, &CCameraObject::OnShakeNoiseChange));
	m_pVarObject->AddVariable(mv_shakeParamsArray, mv_noiseAFreqMult, "NoiseAFreqMult", "Noise A Freq. Mult.", functor(*this, &CCameraObject::OnShakeNoiseChange));
	m_pVarObject->AddVariable(mv_shakeParamsArray, mv_timeOffsetA, "TimeOffsetA", "Time Offset A", functor(*this, &CCameraObject::OnShakeWorkingChange));
	m_pVarObject->AddVariable(mv_shakeParamsArray, mv_amplitudeB, "AmplitudeB", "Amplitude B", functor(*this, &CCameraObject::OnShakeAmpBChange));
	m_pVarObject->AddVariable(mv_shakeParamsArray, mv_amplitudeBMult, "AmplitudeBMult", "Amplitude B Mult.", functor(*this, &CCameraObject::OnShakeMultChange));
	m_pVarObject->AddVariable(mv_shakeParamsArray, mv_frequencyB, "FrequencyB", "Frequency B", functor(*this, &CCameraObject::OnShakeFreqBChange));
	m_pVarObject->AddVariable(mv_shakeParamsArray, mv_frequencyBMult, "FrequencyBMult", "Frequency B Mult.", functor(*this, &CCameraObject::OnShakeMultChange));
	m_pVarObject->AddVariable(mv_shakeParamsArray, mv_noiseBAmpMult, "NoiseBAmpMult", "Noise B Amp. Mult.", functor(*this, &CCameraObject::OnShakeNoiseChange));
	m_pVarObject->AddVariable(mv_shakeParamsArray, mv_noiseBFreqMult, "NoiseBFreqMult", "Noise B Freq. Mult.", functor(*this, &CCameraObject::OnShakeNoiseChange));
	m_pVarObject->AddVariable(mv_shakeParamsArray, mv_timeOffsetB, "TimeOffsetB", "Time Offset B", functor(*this, &CCameraObject::OnShakeWorkingChange));
	m_pVarObject->AddVariable(mv_shakeParamsArray, mv_cameraShakeSeed, "CameraShakeSeed", "Random Seed", functor(*this, &CCameraObject::OnCameraShakeSeedChange));
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CEntityObject::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CCameraObject>("Camera", [](CCameraObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_fov, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_farZ, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_nearZ, ar);

		pObject->m_pVarObject->SerializeVariable(&pObject->mv_omniCamera, ar);

		pObject->m_pVarObject->SerializeVariable(&pObject->mv_amplitudeA, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_amplitudeAMult, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_frequencyA, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_frequencyAMult, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_noiseAAmpMult, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_noiseAFreqMult, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_timeOffsetA, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_amplitudeB, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_amplitudeBMult, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_frequencyB, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_frequencyBMult, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_noiseBAmpMult, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_noiseBFreqMult, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_timeOffsetB, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_cameraShakeSeed, ar);
	});
}

void CCameraObject::Serialize(CObjectArchive& ar)
{
	CEntityObject::Serialize(ar);
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::SetName(const string& name)
{
	CEntityObject::SetName(name);
	if (GetLookAt())
	{
		GetLookAt()->SetName(string(GetName()) + " Target");
	}
}

//////////////////////////////////////////////////////////////////////////
int CCameraObject::MouseCreateCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	if (event == eMouseMove || event == eMouseLDown || event == eMouseLUp)
	{
		Vec3 pos = ((CViewport*)view)->MapViewToCP(point, 0, true, GetCreationOffsetFromTerrain());

		if (m_creationStep == 1)
		{
			if (GetLookAt())
			{
				GetLookAt()->SetPos(pos);
			}
		}
		else
		{
			SetPos(pos);
		}

		if (event == eMouseLDown && m_creationStep == 0)
		{
			m_creationStep = 1;
		}
		if (event == eMouseMove && 1 == m_creationStep && !GetLookAt())
		{
			float d = pos.GetDistance(GetPos());
			if (d * view->GetScreenScaleFactor(pos) > 1)
			{
				// Create LookAt Target.
				GetIEditorImpl()->GetIUndoManager()->Resume();
				CreateTarget();
				GetIEditorImpl()->GetIUndoManager()->Suspend();
			}
		}
		if (eMouseLUp == event && 1 == m_creationStep)
		{
			return MOUSECREATE_OK;
		}

		return MOUSECREATE_CONTINUE;
	}
	return CBaseObject::MouseCreateCallback(view, event, point, flags);
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::CreateTarget()
{
	// Create another camera object for target.
	CCameraObject* camTarget = (CCameraObject*)GetObjectManager()->NewObject("CameraTarget");
	if (camTarget)
	{
		camTarget->SetName(string(GetName()) + " Target");
		camTarget->SetPos(GetWorldPos() + Vec3(3, 0, 0));
		SetLookAt(camTarget);
	}
}

//////////////////////////////////////////////////////////////////////////
Vec3 CCameraObject::GetLookAtEntityPos() const
{
	Vec3 pos = GetWorldPos();
	if (GetLookAt())
	{
		pos = GetLookAt()->GetWorldPos();

		if (GetLookAt()->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			IEntity* pIEntity = ((CEntityObject*)GetLookAt())->GetIEntity();
			if (pIEntity)
			{
				pos = pIEntity->GetWorldPos();
			}
		}
	}
	return pos;
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	Matrix34 wtm = GetWorldTM();

	if (m_pEntity)
	{
		// Display the actual entity matrix.
		wtm = m_pEntity->GetWorldTM();
	}

	Vec3 wp = wtm.GetTranslation();

	//float fScale = dc.view->GetScreenScaleFactor(wp) * 0.03f;
	float fScale = GetHelperScale();

	if (IsHighlighted() && !IsFrozen())
		dc.SetLineWidth(3);

	if (GetLookAt())
	{
		// Look at camera.
		if (IsFrozen())
			dc.SetFreezeColor();
		else
			dc.SetColor(GetColor());

		bool bSelected = IsSelected();

		if (bSelected || GetLookAt()->IsSelected())
		{
			dc.SetSelectedColor();
		}

		// Line from source to target.
		dc.DrawLine(wp, GetLookAtEntityPos());

		if (bSelected)
		{
			dc.SetSelectedColor();
		}
		else if (IsFrozen())
			dc.SetFreezeColor();
		else
			dc.SetColor(GetColor());

		dc.PushMatrix(wtm);

		//Vec3 sz(0.2f*fScale,0.1f*fScale,0.2f*fScale);
		//dc.DrawWireBox( -sz,sz );

		if (bSelected)
		{
			float dist = GetLookAtEntityPos().GetDistance(wtm.GetTranslation());
			DrawCone(dc, dist);
		}
		else
		{
			float dist = CAMERA_CONE_LENGTH / 2.0f;
			DrawCone(dc, dist, fScale);
		}
		dc.PopMatrix();
	}
	else
	{
		// Free camera
		if (IsSelected())
			dc.SetSelectedColor();
		else if (IsFrozen())
			dc.SetFreezeColor();
		else
			dc.SetColor(GetColor());

		dc.PushMatrix(wtm);

		//Vec3 sz(0.2f*fScale,0.1f*fScale,0.2f*fScale);
		//dc.DrawWireBox( -sz,sz );

		float dist = CAMERA_CONE_LENGTH;
		DrawCone(dc, dist, fScale);
		dc.PopMatrix();
	}

	if (IsHighlighted() && !IsFrozen())
		dc.SetLineWidth(0);

	//dc.DrawIcon( ICON_QUAD,wp,0.1f*dc.view->GetScreenScaleFactor(wp) );

	DrawDefault(dc);
}

//////////////////////////////////////////////////////////////////////////
bool CCameraObject::HitTestRect(HitContext& hc)
{
	// transform all 8 vertices into world space
	CPoint p = hc.view->WorldToView(GetWorldPos());
	if (hc.rect.PtInRect(p))
	{
		hc.object = this;
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CCameraObject::HitHelperTest(HitContext& hc)
{
	if (hc.view && GetIEditorImpl()->GetViewManager()->GetCameraObjectId() == GetId())
		return false;
	return __super::HitHelperTest(hc);
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::GetBoundBox(AABB& box)
{
	Vec3 pos = GetWorldPos();
	float r;
	r = 1 * CAMERA_CONE_LENGTH * GetHelperScale();
	box.min = pos - Vec3(r, r, r);
	box.max = pos + Vec3(r, r, r);
	if (GetLookAt())
	{
		box.Add(GetLookAt()->GetWorldPos());
	}
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::GetLocalBounds(AABB& box)
{
	GetBoundBox(box);
	Matrix34 invTM(GetWorldTM());
	invTM.Invert();
	box.SetTransformedAABB(invTM, box);
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::OnMenuSetAsViewCamera()
{
	CViewport* pViewport = GetIEditorImpl()->GetViewManager()->GetSelectedViewport();
	if (pViewport && pViewport->IsRenderViewport())
	{
		CRenderViewport* pRenderViewport = static_cast<CRenderViewport*>(pViewport);
		pRenderViewport->SetCameraObject(this);
	}
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::OnContextMenu(CPopupMenuItem* menu)
{
	if (!menu->Empty())
		menu->AddSeparator();
	menu->Add("Set As View Camera", functor(*this, &CCameraObject::OnMenuSetAsViewCamera));

	__super::OnContextMenu(menu);
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::GetConePoints(Vec3 q[4], float dist, const float fAspectRatio)
{
	if (dist > 1e8f)
		dist = 1e8f;
	float ta = (float)tan(0.5f * GetFOV());
	float h = dist * ta;
	float w = h * fAspectRatio;

	q[0] = Vec3(w, dist, h);
	q[1] = Vec3(-w, dist, h);
	q[2] = Vec3(-w, dist, -h);
	q[3] = Vec3(w, dist, -h);
}

void CCameraObject::DrawCone(DisplayContext& dc, float dist, float fScale)
{
	Vec3 q[4];
	GetConePoints(q, dist, dc.view->GetAspectRatio());

	q[0] *= fScale;
	q[1] *= fScale;
	q[2] *= fScale;
	q[3] *= fScale;

	Vec3 org(0, 0, 0);
	dc.DrawLine(org, q[0]);
	dc.DrawLine(org, q[1]);
	dc.DrawLine(org, q[2]);
	dc.DrawLine(org, q[3]);

	// Draw quad.
	dc.DrawPolyLine(q, 4);
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::OnFovChange(IVariable* var)
{
	// No one will need a fov of less-than 0.1.
	if (mv_fov < DEG2RAD(MIN_FOV_IN_DEG))
		mv_fov = DEG2RAD(MIN_FOV_IN_DEG);

	for (CListenerSet<ICameraObjectListener*>::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
	{
		const float fov = mv_fov;
		notifier->OnFovChange(RAD2DEG(fov));
	}

	UpdateCameraEntity();
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::OnNearZChange(IVariable* var)
{
	for (CListenerSet<ICameraObjectListener*>::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
	{
		float nearZ = mv_nearZ;
		notifier->OnNearZChange(nearZ);
	}

	UpdateCameraEntity();
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::OnFarZChange(IVariable* var)
{
	for (CListenerSet<ICameraObjectListener*>::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
	{
		float farZ = mv_farZ;
		notifier->OnFarZChange(farZ);
	}

	UpdateCameraEntity();
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::UpdateCameraEntity()
{
	if (m_pEntity)
	{
		IEntityCameraComponent* pCameraProxy = (IEntityCameraComponent*)m_pEntity->GetProxy(ENTITY_PROXY_CAMERA);
		if (pCameraProxy)
		{
			float fov = mv_fov;
			float nearZ = mv_nearZ;
			float farZ = mv_farZ;
			CCamera& cam = pCameraProxy->GetCamera();
			cam.SetFrustum(cam.GetViewSurfaceX(), cam.GetViewSurfaceZ(), fov, nearZ, farZ, cam.GetPixelAspectRatio());
			pCameraProxy->SetCamera(cam);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::OnShakeAmpAChange(IVariable* var)
{
	for (CListenerSet<ICameraObjectListener*>::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnShakeAmpAChange(mv_amplitudeA);
	}
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::OnShakeAmpBChange(IVariable* var)
{
	for (CListenerSet<ICameraObjectListener*>::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnShakeAmpBChange(mv_amplitudeB);
	}
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::OnShakeFreqAChange(IVariable* var)
{
	for (CListenerSet<ICameraObjectListener*>::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnShakeFreqAChange(mv_frequencyA);
	}
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::OnShakeFreqBChange(IVariable* var)
{
	for (CListenerSet<ICameraObjectListener*>::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnShakeFreqBChange(mv_frequencyB);
	}
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::OnShakeMultChange(IVariable* var)
{
	for (CListenerSet<ICameraObjectListener*>::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnShakeMultChange(mv_amplitudeAMult, mv_amplitudeBMult, mv_frequencyAMult, mv_frequencyBMult);
	}
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::OnShakeNoiseChange(IVariable* var)
{
	for (CListenerSet<ICameraObjectListener*>::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnShakeNoiseChange(mv_noiseAAmpMult, mv_noiseBAmpMult, mv_noiseAFreqMult, mv_noiseBFreqMult);
	}
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::OnShakeWorkingChange(IVariable* var)
{
	for (CListenerSet<ICameraObjectListener*>::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnShakeWorkingChange(mv_timeOffsetA, mv_timeOffsetB);
	}
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::OnCameraShakeSeedChange(IVariable* var)
{
	for (CListenerSet<ICameraObjectListener*>::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnCameraShakeSeedChange(mv_cameraShakeSeed);
	}
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::OnOmniCameraChange(IVariable *var)
{
	for (CListenerSet<ICameraObjectListener*>::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnOmniCameraChange(mv_omniCamera);
	}
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::RegisterCameraListener(ICameraObjectListener* pListener)
{
	m_listeners.Add(pListener);
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::UnregisterCameraListener(ICameraObjectListener* pListener)
{
	m_listeners.Remove(pListener);
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::SetFOV(const float fov)
{
	mv_fov = std::max(MIN_FOV_IN_DEG, fov);
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::SetNearZ(const float nearZ)
{
	mv_nearZ = clamp_tpl(nearZ, kMinNearZ, kMaxNearZ);
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::SetAmplitudeAMult(const float amplitudeAMult)
{
	mv_amplitudeAMult = amplitudeAMult;
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::SetAmplitudeBMult(const float amplitudeBMult)
{
	mv_amplitudeBMult = amplitudeBMult;
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::SetFrequencyAMult(const float frequencyAMult)
{
	mv_frequencyAMult = frequencyAMult;
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::SetFrequencyBMult(const float frequencyBMult)
{
	mv_frequencyBMult = frequencyBMult;
}

//////////////////////////////////////////////////////////////////////////
//
// CCameraObjectTarget implementation.
//
//////////////////////////////////////////////////////////////////////////
CCameraObjectTarget::CCameraObjectTarget()
{
	SetColor(CAMERA_COLOR);
	UseMaterialLayersMask(false);
}

bool CCameraObjectTarget::Init(CBaseObject* prev, const string& file)
{
	SetColor(CAMERA_COLOR);
	bool res = CEntityObject::Init(prev, file);
	m_entityClass = "CameraTarget";
	return res;
}

//////////////////////////////////////////////////////////////////////////
void CCameraObjectTarget::InitVariables()
{

}

//////////////////////////////////////////////////////////////////////////
void CCameraObjectTarget::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	Vec3 wp = GetWorldPos();

	//float fScale = dc.view->GetScreenScaleFactor(wp) * 0.03f;
	float fScale = GetHelperScale();

	if (IsSelected())
		dc.SetSelectedColor();
	else if (IsFrozen())
		dc.SetFreezeColor();
	else
		dc.SetColor(GetColor());

	Vec3 sz(0.2f * fScale, 0.2f * fScale, 0.2f * fScale);
	dc.DrawWireBox(wp - sz, wp + sz);

	DrawDefault(dc);
}

//////////////////////////////////////////////////////////////////////////
bool CCameraObjectTarget::HitTest(HitContext& hc)
{
	Vec3 origin = GetWorldPos();
	float radius = CAMERABOX_RADIUS / 2.0f;

	//float fScale = hc.view->GetScreenScaleFactor(origin) * 0.03f;
	float fScale = GetHelperScale();

	Vec3 w = origin - hc.raySrc;
	w = hc.rayDir.Cross(w);
	float d = w.GetLength();

	if (d < radius * fScale + hc.distanceTolerance)
	{
		hc.dist = hc.raySrc.GetDistance(origin);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CCameraObjectTarget::GetBoundBox(AABB& box)
{
	Vec3 pos = GetWorldPos();
	float r = CAMERABOX_RADIUS;
	box.min = pos - Vec3(r, r, r);
	box.max = pos + Vec3(r, r, r);
}

void CCameraObjectTarget::Serialize(CObjectArchive& ar)
{
	CEntityObject::Serialize(ar);
}

