// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FacialPreviewDialog.h"
#include "FacialEdContext.h"
#include "ModelViewport.h"
#include "QMfcApp/qmfcviewporthost.h"
#include "Resource.h"
#include <CryAnimation/ICryAnimation.h>
#include "Util/Variable.h"
#include "IUndoObject.h"

IMPLEMENT_DYNAMIC(CFacialPreviewDialog, CToolbarDialog)

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CFacialPreviewDialog, CToolbarDialog)
ON_WM_SIZE()
ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()

enum ForcedRotationEntries
{
	FORCED_ROTATION_ENTRY_NECK,
	FORCED_ROTATION_ENTRY_EYE_LEFT,
	FORCED_ROTATION_ENTRY_EYE_RIGHT,

	FORCED_ROTATION_ENTRY_COUNT
};
static const char* boneNames[FORCED_ROTATION_ENTRY_COUNT] = { "Bip01 Neck", "eye_left_bone", "eye_right_bone" };

class CModelViewportFE : public CModelViewport
{
public:
	CModelViewportFE()
		: m_bLookIK(false),
		m_bLookIKEyesOnly(false),
		m_bShowEyeVectors(false),
		m_fLookIKOffsetX(0),
		m_fLookIKOffsetY(0),
		m_bProceduralAnimation(false),
		m_bAnimateCamera(false),
		m_pContext(0)
	{
		// Initialize the forced rotations.
		for (int i = 0; i < FORCED_ROTATION_ENTRY_COUNT; ++i)
			m_forcedRotations[i].rotation.SetIdentity();

		for (int i = 0; i < StoredCameraIndexCount; ++i)
			m_storedCamerasInitialized[i] = false;

		m_vCamPos = Vec3(0);
	}
	virtual void Update();

	Vec3         CalculateLookIKTarget();
	void         HandleAnimationSettingsSwitch();
	virtual void DrawCharacter(ICharacterInstance* pInstance, const SRendParams& rp, const SRenderingPassInfo& pass) override
	{
		//Not implemented
	}

	bool                           m_bLookIK;
	bool                           m_bLookIKEyesOnly;
	bool                           m_bShowEyeVectors;
	float                          m_fLookIKOffsetX;
	float                          m_fLookIKOffsetY;
	bool                           m_bProceduralAnimation;
	bool                           m_bAnimateCamera;
	CFacialEdContext*              m_pContext;
	Vec3						   m_vCamPos;

	CFacialAnimForcedRotationEntry m_forcedRotations[FORCED_ROTATION_ENTRY_COUNT];

	enum StoredCameraIndex
	{
		StoredCameraIndexBindPose,
		StoredCameraIndexAnimated,
		StoredCameraIndexCount
	};
	Matrix34 m_storedCameras[StoredCameraIndexCount];
	bool     m_storedCamerasInitialized[StoredCameraIndexCount];
};

//////////////////////////////////////////////////////////////////////////
// CFacialPreviewDialog
//////////////////////////////////////////////////////////////////////////
CFacialPreviewDialog::CFacialPreviewDialog()
	: CToolbarDialog(IDD_DATABASE, NULL)
	, m_pViewportHost(nullptr)
{
	m_pContext = 0;
	m_pModelViewport = 0;
	m_hAccelerators = 0;
}

//////////////////////////////////////////////////////////////////////////
CFacialPreviewDialog::~CFacialPreviewDialog()
{
	// Save values to registry.
	XmlNodeRef node = XmlHelpers::CreateXmlNode("Vars");
	m_vars.Serialize(node, false);
	AfxGetApp()->WriteProfileString("FEPreviewSettings", "Vars", node->getXML());
}

//////////////////////////////////////////////////////////////////////////
void CFacialPreviewDialog::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	CRect rcClient;
	GetClientRect(rcClient);

	CRect rctb = rcClient;
	rctb.bottom = rctb.top + 24;
	m_wndToolBar.MoveWindow(rctb);

	rcClient.top = rctb.bottom;

	if (m_pViewportHost)
	{
		m_pViewportHost->SetClientRect(rcClient);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialPreviewDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
}

//////////////////////////////////////////////////////////////////////////
BOOL CFacialPreviewDialog::OnInitDialog()
{
	m_hAccelerators = LoadAccelerators(AfxGetApp()->m_hInstance, MAKEINTRESOURCE(IDR_FACED_MENU));

	VERIFY(m_wndToolBar.CreateToolBar(WS_VISIBLE | WS_CHILD | CBRS_TOOLTIPS | CBRS_GRIPPER, this, AFX_IDW_TOOLBAR));
	VERIFY(m_wndToolBar.LoadToolBar(IDR_MATERIAL_BROWSER));
	m_wndToolBar.SetFlags(xtpFlagStretched);
	m_wndToolBar.EnableCustomization(FALSE);

	CRect rcClient;
	GetClientRect(rcClient);

	m_pModelViewport = new CModelViewportFE;

	CRect toolbarRect;
	m_wndToolBar.GetClientRect(toolbarRect);
	rcClient.top = toolbarRect.bottom;
	m_pViewportHost = new QMfcViewportHost(this, m_pModelViewport, rcClient);

	m_pModelViewport->SetType(ET_ViewportModel);

	m_vars.AddVariable(m_bLookIK, _T("LookIK"));
	m_vars.AddVariable(m_bLookIKEyesOnly, _T("LookIK Eyes Only"));
	m_vars.AddVariable(m_bShowEyeVectors, _T("Show Eye Vectors"));
	m_vars.AddVariable(m_fLookIKOffsetX, _T("Look IK Offset X"));
	m_vars.AddVariable(m_fLookIKOffsetY, _T("Look IK Offset Y"));
	m_vars.AddVariable(m_bProceduralAnimation, _T("Procedural Animation"));
	for (int i = 0; i < m_pModelViewport->GetVarObject()->GetNumVariables(); ++i)
	{
		IVariable* var = m_pModelViewport->GetVarObject()->GetVariable(i);
		IVariable* clone = var->Clone(true);
		clone->Wire(var);
		m_vars.AddVariable(*static_cast<CVariableBase*>(clone), clone->GetName());
	}

	m_bLookIK.AddOnSetCallback(functor(*this, &CFacialPreviewDialog::OnLookIKChanged));
	m_bLookIKEyesOnly.AddOnSetCallback(functor(*this, &CFacialPreviewDialog::OnLookIKEyesOnlyChanged));
	m_bShowEyeVectors.AddOnSetCallback(functor(*this, &CFacialPreviewDialog::OnShowEyeVectorsChanged));
	m_fLookIKOffsetX.AddOnSetCallback(functor(*this, &CFacialPreviewDialog::OnLookIKOffsetChanged));
	m_fLookIKOffsetX.SetLimits(-1.0f, 1.0f);
	m_fLookIKOffsetY.AddOnSetCallback(functor(*this, &CFacialPreviewDialog::OnLookIKOffsetChanged));
	m_fLookIKOffsetY.SetLimits(-1.0f, 1.0f);
	m_bProceduralAnimation.AddOnSetCallback(functor(*this, &CFacialPreviewDialog::OnProceduralAnimationChanged));
	m_bProceduralAnimation = false;

	CString xml = AfxGetApp()->GetProfileString("FEPreviewSettings", "Vars");
	if (!xml.IsEmpty())
	{
		XmlNodeRef node = XmlHelpers::LoadXmlFromBuffer(xml.GetBuffer(), xml.GetLength());
		if (node)
		{
			m_vars.Serialize(node, true);
		}
	}

	RecalcLayout();
	//MoveWindow(&rect);
	ShowWindow(SW_SHOW);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CFacialPreviewDialog::SetContext(CFacialEdContext* pContext)
{
	if (!m_pModelViewport)
		return;

	assert(pContext);
	m_pContext = pContext;
	m_pModelViewport->m_pContext = pContext;
	//m_pModelViewport->SetCharacter(pContext->pCharacter);
	if (pContext->pCharacter)
		m_pModelViewport->LoadObject(pContext->pCharacter->GetFilePath());
	OnCenterOnHead();
	if (m_pModelViewport->GetCharacterBase() && m_pModelViewport->GetCharacterBase()->GetISkeletonAnim())
	{
		CryCharAnimationParams params;
		params.m_nFlags |= CA_LOOP_ANIMATION;
		m_pModelViewport->GetCharacterBase()->GetISkeletonAnim()->StartAnimation("null", params);
	}
	if (m_pContext)
		m_pContext->RegisterListener(this);
}

//////////////////////////////////////////////////////////////////////////
void CFacialPreviewDialog::OnCenterOnHead()
{
	if (!m_pContext) return;
	if (m_pContext->pCharacter)
	{
		int16 id = m_pContext->pCharacter->GetIDefaultSkeleton().GetJointIDByName("Bip01 Head");
		if (id >= 0)
		{
			Vec3 vPos = m_pContext->pCharacter->GetISkeletonPose()->GetAbsJointByID(id).t;

			Matrix34 tm = Matrix34::CreateTranslationMat(vPos + Vec3(0, 0.5f, 0.0f));
			tm = tm * Matrix34::CreateRotationZ(gf_PI);

			m_pModelViewport->SetViewTM(tm);
		}
	}
	if (m_pModelViewport && m_pModelViewport->GetCharacterBase())
		m_pModelViewport->GetCharacterBase()->EnableProceduralFacialAnimation(m_bProceduralAnimation);
}

//////////////////////////////////////////////////////////////////////////
void CFacialPreviewDialog::RedrawPreview()
{
	//TODO : if necessary do this differently
	//GetIEditor()->GetViewManager()->OnEditorNotifyEvent(eNotify_OnIdleUpdate);
}

//////////////////////////////////////////////////////////////////////////
static const char* eyeBones[2] = { "eye_left_bone", "eye_right_bone" };
void CModelViewportFE::Update()
{
	if (m_bPaused)
		return;

	if (m_bAnimateCamera)
	{
		IFacialAnimSequence* pSequence = (m_pContext ? m_pContext->GetSequence() : 0);
		ISplineInterpolator* pPositionSpline = (pSequence ? pSequence->GetCameraPathPosition() : 0);
		ISplineInterpolator* pOrientationSpline = (pSequence ? pSequence->GetCameraPathOrientation() : 0);

		int positionSplineKeyCount = (pPositionSpline ? pPositionSpline->GetKeyCount() : 0);
		int orientationSplineKeyCount = (pOrientationSpline ? pOrientationSpline->GetKeyCount() : 0);

		float sequenceTime = (m_pContext ? m_pContext->GetSequenceTime() : 0);

		if (positionSplineKeyCount > 0 && orientationSplineKeyCount > 0)
		{
			Vec3 position(0.0f, 0.0f, 0.0f);
			if (pPositionSpline)
				pPositionSpline->Interpolate(sequenceTime, (ISplineInterpolator::ValueType&)position);

			Quat orientation(0.0f, 0.0f, 0.0f, 1.0f);
			if (pOrientationSpline)
				pOrientationSpline->Interpolate(sequenceTime, (ISplineInterpolator::ValueType&)orientation);
			orientation.Normalize();

			QuatT quat(position, orientation);
			Matrix34 transform(quat);
			m_Camera.SetMatrix(transform);
		}

		ISplineInterpolator* pFOVSpline = (pSequence ? pSequence->GetCameraPathFOV() : 0);
		int fovSplineKeyCount = (pFOVSpline ? pFOVSpline->GetKeyCount() : 0);
		if (pFOVSpline && fovSplineKeyCount)
		{
			float fov = 0.0f;
			pFOVSpline->InterpolateFloat(sequenceTime, fov);
			mv_fov = min(150.0f, max(5.0f, fov));
		}
	}

	ICharacterInstance* pCharacter = GetCharacterBase();
	if (pCharacter)
		pCharacter->EnableProceduralFacialAnimation(m_bProceduralAnimation);

	GetISystem()->GetIAnimationSystem()->DummyUpdate();

	if (pCharacter && pCharacter->GetISkeletonAnim())
	{
		float* customBlends = 0;
		if (this->m_bLookIKEyesOnly)
		{
			static float zeroCustomBlends[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
			customBlends = zeroCustomBlends;
		}

		IAnimationPoseBlenderDir* pIPoseBlenderLook = pCharacter->GetISkeletonPose()->GetIPoseBlenderLook();
		if (pIPoseBlenderLook)
		{
			pIPoseBlenderLook->SetState(m_bLookIK);
			pIPoseBlenderLook->SetTarget(CalculateLookIKTarget());
			pIPoseBlenderLook->SetFadeoutAngle(DEG2RAD(120));
		}

		// Render the eye bones.
		if (m_bShowEyeVectors)
		{
			static const float lineLength = 0.15f;
			for (int boneIndex = 0; boneIndex < sizeof(eyeBones) / sizeof(eyeBones[0]); ++boneIndex)
			{
				const char* bone = eyeBones[boneIndex];
				int boneID = pCharacter->GetIDefaultSkeleton().GetJointIDByName(bone);
				if (boneID >= 0)
				{
					Matrix34 abs34 = Matrix34(GetCharacterBase()->GetISkeletonPose()->GetAbsJointByID(boneID));
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(
					  abs34.GetTranslation(), RGBA8(0xFF, 0x00, 0x00, 0xFF),
					  abs34.GetTranslation() + abs34.GetColumn(1) * lineLength, RGBA8(0xFF, 0x00, 0x00, 0xFF));
				}
			}
		}

		// Update the forced rotations.
		for (int i = 0; i < FORCED_ROTATION_ENTRY_COUNT; ++i)
			m_forcedRotations[i].jointId = pCharacter->GetIDefaultSkeleton().GetJointIDByName(boneNames[i]);
		if (pCharacter->GetFacialInstance())
			pCharacter->GetFacialInstance()->SetForcedRotations(FORCED_ROTATION_ENTRY_COUNT, m_forcedRotations);

		// Force the skeleton to be updated - without this some cinematic sequences dont get rendered, because
		// the animation system thinks they are off screen.
		ISkeletonPose* pSkeletonPose = (pCharacter ? pCharacter->GetISkeletonPose() : 0);
		if (pSkeletonPose)
			pSkeletonPose->SetForceSkeletonUpdate(1);
	}

	__super::Update();

	// Backup the camera transforms so we can restore them later if we switch camera modes.
	StoredCameraIndex cameraIndex = StoredCameraIndexBindPose;
	if (m_pContext && (m_pContext->GetAnimateSkeleton() || m_pContext->GetAnimateCamera()))
		cameraIndex = StoredCameraIndexAnimated;
	m_storedCameras[cameraIndex] = m_Camera.GetMatrix();
	m_storedCamerasInitialized[cameraIndex] = true;
};

//////////////////////////////////////////////////////////////////////////
void CFacialPreviewDialog::OnFacialEdEvent(EFacialEdEvent event, IFacialEffector* pEffector, int nChannelCount, IFacialAnimChannel** ppChannels)
{
	switch (event)
	{
	case EFD_EVENT_ANIMATE_SKELETON_CHANGED:
	case EFD_EVENT_ANIMATE_CAMERA_CHANGED:
		{
			if (m_pModelViewport)
				m_pModelViewport->HandleAnimationSettingsSwitch();
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialPreviewDialog::OnLookIKChanged(IVariable* var)
{
	m_pModelViewport->m_bLookIK = this->m_bLookIK;
}

void CFacialPreviewDialog::OnLookIKEyesOnlyChanged(IVariable* var)
{
	m_pModelViewport->m_bLookIKEyesOnly = this->m_bLookIKEyesOnly;
}

void CFacialPreviewDialog::OnShowEyeVectorsChanged(IVariable* var)
{
	m_pModelViewport->m_bShowEyeVectors = this->m_bShowEyeVectors;
}

void CFacialPreviewDialog::OnLookIKOffsetChanged(IVariable* var)
{
	m_pModelViewport->m_fLookIKOffsetX = m_fLookIKOffsetX;
	m_pModelViewport->m_fLookIKOffsetY = m_fLookIKOffsetY;
}

void CFacialPreviewDialog::OnProceduralAnimationChanged(IVariable* var)
{
	m_pModelViewport->m_bProceduralAnimation = m_bProceduralAnimation;
}

CModelViewport* CFacialPreviewDialog::GetViewport()
{
	return m_pModelViewport;
}

void CFacialPreviewDialog::SetAnimateCamera(bool bAnimateCamera)
{
	m_pModelViewport->m_bAnimateCamera = bAnimateCamera;
}

bool CFacialPreviewDialog::GetAnimateCamera() const
{
	return m_pModelViewport->m_bAnimateCamera;
}

void CFacialPreviewDialog::SetForcedNeckRotation(const Quat& rotation)
{
	m_pModelViewport->m_forcedRotations[FORCED_ROTATION_ENTRY_NECK].rotation = rotation;
}

void CFacialPreviewDialog::SetForcedEyeRotation(const Quat& rotation, EyeType eye)
{
	if (eye >= 0 && eye < 2)
	{
		const int eyeEntries[] = { FORCED_ROTATION_ENTRY_EYE_LEFT, FORCED_ROTATION_ENTRY_EYE_RIGHT };
		m_pModelViewport->m_forcedRotations[eyeEntries[eye]].rotation = rotation;
	}
}

BOOL CFacialPreviewDialog::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST && m_hAccelerators)
		return ::TranslateAccelerator(m_hWnd, m_hAccelerators, pMsg);
	return CDialog::PreTranslateMessage(pMsg);
}

BOOL CFacialPreviewDialog::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	CFacialEdContext::MoveToKeyDirection direction = (zDelta < 0 ? CFacialEdContext::MoveToKeyDirectionBackward : CFacialEdContext::MoveToKeyDirectionForward);
	if (m_pContext)
		m_pContext->MoveToFrame(direction);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
Vec3 CModelViewportFE::CalculateLookIKTarget()
{
	// Find the position between the eye bones.
	Vec3 eyePos(0, 0, 0);
	for (int boneIndex = 0; boneIndex < sizeof(eyeBones) / sizeof(eyeBones[0]); ++boneIndex)
	{
		const char* bone = eyeBones[boneIndex];
		int boneID = GetCharacterBase()->GetIDefaultSkeleton().GetJointIDByName(bone);
		if (boneID >= 0)
		{
			Matrix34 abs34 = Matrix34(GetCharacterBase()->GetISkeletonPose()->GetAbsJointByID(boneID));
			eyePos += abs34.GetTranslation();
		}
	}
	eyePos /= sizeof(eyeBones) / sizeof(eyeBones[0]);

	Vec3 dir = m_vCamPos - eyePos;
	dir = Matrix34::CreateRotationZ(m_fLookIKOffsetX * gf_PI / 6) * dir;
	dir = Matrix34::CreateRotationX(m_fLookIKOffsetY * gf_PI / 6) * dir;
	return eyePos + dir;
}

//////////////////////////////////////////////////////////////////////////
void CModelViewportFE::HandleAnimationSettingsSwitch()
{
	CModelViewportFE::StoredCameraIndex cameraIndex = CModelViewportFE::StoredCameraIndexBindPose;
	if (m_pContext && (m_pContext->GetAnimateSkeleton() || m_pContext->GetAnimateCamera()))
		cameraIndex = CModelViewportFE::StoredCameraIndexAnimated;
	if (m_storedCamerasInitialized[cameraIndex])
		m_Camera.SetMatrix(m_storedCameras[cameraIndex]);
}

