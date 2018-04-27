// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ModelViewport.h"

#include <CryMath/Cry_Vector3.h>
#include <CryGame/IGameFramework.h>
#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IShader.h>
#include <CryAudio/IListener.h>

#include <CryAnimation/ICryAnimation.h>
#include <CryAnimation/IAttachment.h>
#include <CryAnimation/CryCharMorphParams.h>

#include <CryAnimation/CryCharAnimationParams.h>
#include <CryAnimation/IFacialAnimation.h>

#include "Objects/DisplayContext.h"

#include <Cry3DEngine/I3DEngine.h>
#include <CryPhysics/IPhysics.h>
#include <CrySystem/ITimer.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include "Controls/QuestionDialog.h"
#include "FilePathUtil.h"

#include "RenderLock.h"
#include "IDataBaseItem.h"


//uint32 g_ypos = 0;

#define SKYBOX_NAME "InfoRedGal"

namespace
{
int s_varsPanelId = 0;

static const char* szSupportedExt[] =
{
	"chr",
	"chr(c)",
	"skin",
	"skin(c)",
	"cdf",
	"cdf(c)",
	"cgf",
	"cgf(c)",
	"cga",
	"cga(c)",
	"cid",
	"caf"
};

}

/////////////////////////////////////////////////////////////////////////////
// CModelViewport

CModelViewport::CModelViewport(const char* settingsPath)
{
	m_PhysicalLocation.SetIdentity();
	m_absCurrentSlope = 0.f;
	m_settingsPath = settingsPath;

	m_pCharacterBase = 0;
	m_bPaused = false;

	m_Camera.SetFrustum(800, 600, 3.14f / 4.0f, 0.02f, 10000);

	m_pAnimationSystem = GetIEditor()->GetSystem()->GetIAnimationSystem();

	m_bInRotateMode = false;
	m_bInMoveMode = false;

	m_object = 0;

	m_weaponModel = 0;
	m_attachedCharacter = 0;

	m_camRadius = 10;

	m_moveSpeed = 0.1;
	m_LightRotationRadian = 0.0f;

	m_weaponIK = false;

	m_pRESky = 0;
	m_pSkyboxName = 0;
	m_pSkyBoxShader = NULL;
	m_pPhysicalEntity = NULL;

	m_attachBone = "weapon_bone";

	// Init variable.
	mv_objectAmbientColor = Vec3(0.25f, 0.25f, 0.25f);
	mv_backgroundColor = Vec3(0.25f, 0.25f, 0.25f);

	m_VPLights.resize(3);

	Vec3 d0 = Vec3(0.70f, 0.70f, 0.70f);
	mv_lightDiffuseColor0 = d0;
	mv_lightDiffuseColor1 = d0;
	mv_lightDiffuseColor2 = d0;
	mv_lightMultiplier = 3.0f;
	mv_lightOrbit = 15.0f;
	mv_lightRadius = 400.0f;
	mv_lightSpecMultiplier = 1.0f;

	mv_showPhysics = false;

	m_GridOrigin = Vec3(ZERO);

	//--------------------------------------------------
	// Register variables.
	//--------------------------------------------------
	m_vars.AddVariable(mv_showPhysics, _T("Display Physics"));
	m_vars.AddVariable(mv_useCharPhysics, _T("Use Character Physics"), functor(*this, &CModelViewport::OnCharPhysics));
	mv_useCharPhysics = true;
	m_vars.AddVariable(mv_showGrid, _T("ShowGrid"));
	mv_showGrid = true;
	m_vars.AddVariable(mv_showBase, _T("ShowBase"));
	mv_showBase = false;
	m_vars.AddVariable(mv_showLocator, _T("ShowLocator"));
	mv_showLocator = 0;
	m_vars.AddVariable(mv_InPlaceMovement, _T("InPlaceMovement"));
	mv_InPlaceMovement = false;
	m_vars.AddVariable(mv_StrafingControl, _T("StrafingControl"));
	mv_StrafingControl = false;

	m_vars.AddVariable(mv_lighting, _T("Lighting"));
	mv_lighting = true;
	m_vars.AddVariable(mv_animateLights, _T("AnimLights"));

	m_vars.AddVariable(mv_backgroundColor, _T("BackgroundColor"), functor(*this, &CModelViewport::OnLightColor), IVariable::DT_COLOR);
	m_vars.AddVariable(mv_objectAmbientColor, _T("ObjectAmbient"), functor(*this, &CModelViewport::OnLightColor), IVariable::DT_COLOR);

	m_vars.AddVariable(mv_lightDiffuseColor0, _T("LightDiffuse1"), functor(*this, &CModelViewport::OnLightColor), IVariable::DT_COLOR);
	m_vars.AddVariable(mv_lightDiffuseColor1, _T("LightDiffuse2"), functor(*this, &CModelViewport::OnLightColor), IVariable::DT_COLOR);
	m_vars.AddVariable(mv_lightDiffuseColor2, _T("LightDiffuse3"), functor(*this, &CModelViewport::OnLightColor), IVariable::DT_COLOR);
	m_vars.AddVariable(mv_lightMultiplier, _T("Light Multiplier"), functor(*this, &CModelViewport::OnLightMultiplier), IVariable::DT_SIMPLE);
	m_vars.AddVariable(mv_lightSpecMultiplier, _T("Light Specular Multiplier"), functor(*this, &CModelViewport::OnLightMultiplier), IVariable::DT_SIMPLE);
	m_vars.AddVariable(mv_lightRadius, _T("Light Radius"), functor(*this, &CModelViewport::OnLightMultiplier), IVariable::DT_SIMPLE);
	m_vars.AddVariable(mv_lightOrbit, _T("Light Orbit"), functor(*this, &CModelViewport::OnLightMultiplier), IVariable::DT_SIMPLE);

	m_vars.AddVariable(mv_showWireframe1, _T("ShowWireframe1"));
	m_vars.AddVariable(mv_showWireframe2, _T("ShowWireframe2"));
	m_vars.AddVariable(mv_showTangents, _T("ShowTangents"));
	m_vars.AddVariable(mv_showBinormals, _T("ShowBinormals"));
	m_vars.AddVariable(mv_showNormals, _T("ShowNormals"));

	m_vars.AddVariable(mv_showSkeleton, _T("ShowSkeleton"));
	m_vars.AddVariable(mv_showJointNames, _T("ShowJointNames"));
	m_vars.AddVariable(mv_showJointsValues, _T("ShowJointsValues"));
	m_vars.AddVariable(mv_showStartLocation, _T("ShowInvStartLocation"));
	m_vars.AddVariable(mv_showMotionParam, _T("ShowMotionParam"));
	m_vars.AddVariable(mv_printDebugText, _T("PrintDebugText"));

	m_vars.AddVariable(mv_UniformScaling, _T("UniformScaling"));
	mv_UniformScaling = 1.0f;
	mv_UniformScaling.SetLimits(0.01f, 2.0f);
	m_vars.AddVariable(mv_forceLODNum, _T("ForceLODNum"));
	mv_forceLODNum = 0;
	mv_forceLODNum.SetLimits(0, 10);
	m_vars.AddVariable(mv_showShaders, _T("ShowShaders"), functor(*this, &CModelViewport::OnShowShaders));
	m_vars.AddVariable(mv_AttachCamera, _T("AttachCamera"));

	m_vars.AddVariable(mv_fov, _T("FOV"));
	mv_fov = 60;
	mv_fov.SetLimits(1, 120);

	RestoreDebugOptions();

	m_camRadius = 10;

	//YPR_Angle	=	Ang3(0,-1.0f,0);
	//SetViewTM( Matrix34(CCamera::CreateOrientationYPR(YPR_Angle), Vec3(0,-m_camRadius,0))  );
	Vec3 camPos = Vec3(10, 10, 10);
	Matrix34 tm = Matrix33::CreateRotationVDir((Vec3(0, 0, 0) - camPos).GetNormalized());
	tm.SetTranslation(camPos);
	SetViewTM(tm);

	if (GetIEditor()->IsInPreviewMode())
	{
		// In preview mode create a simple physical grid, so we can register physical entities.
		int nCellSize = 4;
		IPhysicalWorld* pPhysWorld = gEnv->pPhysicalWorld;
		if (pPhysWorld)
		{
			pPhysWorld->SetupEntityGrid(2, Vec3(0, 0, 0), 10, 10, 1, 1);
		}
	}

	m_RT = 0.0f;
	m_LTHUMB = Vec2(0, 0);
	m_RTHUMB = Vec2(0, 0);
	uint32 size = sizeof(m_arrLTHUMB) / sizeof(Vec2);
	for (uint32 i = 0; i < size; i++)
		m_arrLTHUMB[i] = Vec2(0, 0);

	if (gEnv->pInput)
	{
		gEnv->pInput->AddEventListener(this);
		uint32 test = gEnv->pInput->HasInputDeviceOfType(eIDT_Gamepad);
	}

	m_pIAudioListener = gEnv->pAudioSystem->CreateListener();
	m_AABB.Reset();
}

bool CModelViewport::IsPreviewableFileType(const char* szPath)
{
	const char* szExt = PathUtil::GetExt(szPath);

	for (unsigned int i = 0; i < CRY_ARRAY_COUNT(szSupportedExt); ++i)
	{
		if (stricmp(szExt, szSupportedExt[i]) == 0)
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::SaveDebugOptions() const
{
	//TODO save this in personalization
/*
	CXTRegistryManager regMgr;
	string str;
	const string strSection = _T(m_settingsPath);

	CVarBlock* vb = GetVarObject()->GetVarBlock();
	int32 vbCount = vb->GetNumVariables();

	regMgr.WriteProfileInt(strSection, _T("iDebugOptionCount"), vbCount);

	char keyType[64], keyValue[64];
	for (int32 i = 0; i < vbCount; ++i)
	{
		IVariable* var = vb->GetVariable(i);
		IVariable::EType vType = var->GetType();
		cry_sprintf(keyType, "DebugOption_%s_type", var->GetName());
		cry_sprintf(keyValue, "DebugOption_%s_value", var->GetName());
		switch (vType)
		{
		case IVariable::UNKNOWN:
			{
				break;
			}
		case IVariable::INT:
			{
				int32 value = 0;
				var->Get(value);
				regMgr.WriteProfileInt(strSection, _T(keyType), IVariable::INT);
				regMgr.WriteProfileInt(strSection, _T(keyValue), value);

				break;
			}
		case IVariable::BOOL:
			{
				BOOL value = 0;
				var->Get(value);
				regMgr.WriteProfileInt(strSection, _T(keyType), IVariable::BOOL);
				regMgr.WriteProfileInt(strSection, _T(keyValue), value);
				break;
			}
		case IVariable::FLOAT:
			{
				f32 value = 0;
				var->Get(value);
				regMgr.WriteProfileInt(strSection, _T(keyType), IVariable::FLOAT);
				regMgr.WriteProfileBinary(strSection, _T(keyValue), (LPBYTE)(&value), sizeof(f32));
				break;
			}
		case IVariable::VECTOR:
			{
				Vec3 value;
				var->Get(value);
				f32 valueArray[3];
				valueArray[0] = value.x;
				valueArray[1] = value.y;
				valueArray[2] = value.z;
				regMgr.WriteProfileInt(strSection, _T(keyType), IVariable::VECTOR);
				regMgr.WriteProfileBinary(strSection, _T(keyValue), (LPBYTE)(&value), 3 * sizeof(f32));

				break;
			}
		case IVariable::QUAT:
			{
				Quat value;
				var->Get(value);
				f32 valueArray[4];
				valueArray[0] = value.w;
				valueArray[1] = value.v.x;
				valueArray[2] = value.v.y;
				valueArray[3] = value.v.z;

				regMgr.WriteProfileInt(strSection, _T(keyType), IVariable::QUAT);
				regMgr.WriteProfileBinary(strSection, _T(keyValue), (LPBYTE)(&value), 4 * sizeof(f32));

				break;
			}
		case IVariable::STRING:
			{
				string value;
				var->Get(value);
				regMgr.WriteProfileInt(strSection, _T(keyType), IVariable::QUAT);
				regMgr.WriteProfileString(strSection, _T(keyValue), value);

				break;
			}
		case IVariable::ARRAY:
			{
				break;
			}
		default:
			break;
		}
	}*/
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::RestoreDebugOptions()
{
/*
	CXTRegistryManager regMgr;
	string str;
	const string strSection = _T(m_settingsPath);

	f32 floatValue = .0f;
	UINT byteNum = sizeof(f32);
	string strRead = "";
	int32 iRead = 0;
	BOOL bRead = FALSE;
	f32 fRead = .0f;
	LPBYTE pbtData = NULL;
	UINT bytes = 0;

	CVarBlock* vb = m_vars.GetVarBlock();
	int32 vbCount = vb->GetNumVariables();

	char keyType[64], keyValue[64];
	for (int32 i = 0; i < vbCount; ++i)
	{
		IVariable* var = vb->GetVariable(i);
		IVariable::EType vType = var->GetType();
		cry_sprintf(keyType, "DebugOption_%s_type", var->GetName());
		int32 iType = regMgr.GetProfileInt(strSection, _T(keyType), 0);

		cry_sprintf(keyValue, "DebugOption_%s_value", var->GetName());
		switch (iType)
		{
		case IVariable::UNKNOWN:
			{
				break;
			}
		case IVariable::INT:
			{
				iRead = regMgr.GetProfileInt(strSection, _T(keyValue), 0);
				var->Set(iRead);
				break;
			}
		case IVariable::BOOL:
			{
				bRead = regMgr.GetProfileInt(strSection, _T(keyValue), FALSE);
				var->Set(bRead);
				break;
			}
		case IVariable::FLOAT:
			{
				regMgr.GetProfileBinary(strSection, _T(keyValue), &pbtData, &bytes);
				fRead = *(f32*)(pbtData);
				var->Set(fRead);
				break;
			}
		case IVariable::VECTOR:
			{
				regMgr.GetProfileBinary(strSection, _T(keyValue), &pbtData, &bytes);
				assert(bytes == 3 * sizeof(f32));
				f32* pfRead = (f32*)(pbtData);

				Vec3 vecRead(pfRead[0], pfRead[1], pfRead[2]);
				var->Set(vecRead);
				break;
			}
		case IVariable::QUAT:
			{
				regMgr.GetProfileBinary(strSection, _T(keyValue), &pbtData, &bytes);
				assert(bytes == 4 * sizeof(f32));
				f32* pfRead = (f32*)(pbtData);

				Quat valueRead(pfRead[0], pfRead[1], pfRead[2], pfRead[3]);
				var->Set(valueRead);
				break;
			}
		case IVariable::STRING:
			{
				strRead = regMgr.GetProfileString(strSection, _T(keyValue), "");
				var->Set(strRead);
				break;
			}
		case IVariable::ARRAY:
			{
				break;
			}
		default:
			break;
		}
	}*/
}

//////////////////////////////////////////////////////////////////////////
CModelViewport::~CModelViewport()
{
	OnDestroy();
	ReleaseObject();

	GetIEditor()->GetIUndoManager()->Flush();

	SaveDebugOptions();

	if (m_pIAudioListener != nullptr)
	{
		gEnv->pAudioSystem->ReleaseListener(m_pIAudioListener);
		m_pIAudioListener = nullptr;
	}

	// Remove input event listener
	GetISystem()->GetIInput()->RemoveEventListener(this);

	gEnv->pPhysicalWorld->GetPhysVars()->helperOffset.zero();
	GetIEditor()->SetConsoleVar("ca_UsePhysics", 1);
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::ReleaseObject()
{
	if (m_object)
	{
		m_object->Release();
		m_object = NULL;
	}

	if (GetCharacterBase())
	{
		m_pCharacterBase = 0;
		m_pAnimationSystem->DeleteDebugInstances();
	}

	if (m_weaponModel)
	{
		m_weaponModel->Release();
		m_weaponModel = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CModelViewport::OnInputEvent(const SInputEvent& rInputEvent)
{
	if (rInputEvent.deviceType == eIDT_Gamepad)
	{
		uint32 nKeyID = rInputEvent.keyId;
		if (nKeyID == 0x210)
			m_LTHUMB.x = rInputEvent.value;
		if (nKeyID == 0x211)
			m_LTHUMB.y = rInputEvent.value;

		if (nKeyID == 0x216)
			m_RTHUMB.x = rInputEvent.value;
		if (nKeyID == 0x217)
			m_RTHUMB.y = rInputEvent.value;

		if (nKeyID == 0x20f)
			m_RT = rInputEvent.value;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::LoadObject(const string& fileName, float scale)
{
	m_bPaused = false;

	// Load object.
	string file = PathUtil::MakeGamePath(fileName.GetString()).c_str();

	bool reload = false;
	if (m_loadedFile == file)
		reload = true;
	m_loadedFile = file;

	SetName(string("Model View - ") + (const char*)file);

	ReleaseObject();

	if (IsPreviewableFileType(file))
	{
		// Try Load character.
		uint32 isSKEL = stricmp(PathUtil::GetExt(file), CRY_SKEL_FILE_EXT) == 0;
		uint32 isSKIN = stricmp(PathUtil::GetExt(file), CRY_SKIN_FILE_EXT) == 0;
		uint32 isCGA = stricmp(PathUtil::GetExt(file), CRY_ANIM_GEOMETRY_FILE_EXT) == 0;
		uint32 isCDF = stricmp(PathUtil::GetExt(file), CRY_CHARACTER_DEFINITION_FILE_EXT) == 0;
		if (isSKEL || isSKIN || isCGA || isCDF)
		{
			m_pCharacterBase = m_pAnimationSystem->CreateInstance(file, CA_CharEditModel);
			if (GetCharacterBase())
			{
				f32 radius = GetCharacterBase()->GetAABB().GetRadius();
				Vec3 center = GetCharacterBase()->GetAABB().GetCenter();

				m_AABB.min = center - Vec3(radius, radius, radius);
				m_AABB.max = center + Vec3(radius, radius, radius);

				if (!reload)
					m_camRadius = center.z + radius;
			}
		}
		else
			LoadStaticObject(file);
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "!Preview of this file type not supported", "Preview Error", MB_OK | MB_ICONEXCLAMATION);
		return;
	}

	if (!reload)
	{
		Vec3 v = m_AABB.max - m_AABB.min;
		float radius = v.GetLength() / 2.0f;
		m_camRadius = radius * 2;
	}

	if (GetIEditor()->IsInPreviewMode())
	{
		Physicalize();
	}
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::LoadStaticObject(const string& file)
{
	if (m_object)
		m_object->Release();

	// Load Static object.
	m_object = m_engine->LoadStatObj(file, 0, 0, false);

	if (!m_object)
	{
		CryLog("Loading of object failed.");
		return;
	}
	m_object->AddRef();

	m_AABB.min = m_object->GetBoxMin();
	m_AABB.max = m_object->GetBoxMax();
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::OnRender()
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	ProcessKeys();
	if (m_renderer)
	{
		SDisplayContextKey displayContextKey;
		displayContextKey.key.emplace<HWND>(static_cast<HWND>(GetSafeHwnd()));

		CRect rcClient;
		GetClientRect(&rcClient);

		m_Camera.SetFrustum(m_Camera.GetViewSurfaceX(), m_Camera.GetViewSurfaceZ(), m_Camera.GetFov(), 0.02f, 10000, m_Camera.GetPixelAspectRatio());
		int w = rcClient.right - rcClient.left;
		int h = rcClient.bottom - rcClient.top;
		m_Camera.SetFrustum(w, h, DEG2RAD(mv_fov), 0.0101f, 10000.0f);

		if (GetIEditor()->IsInPreviewMode())
		{
			GetISystem()->SetViewCamera(m_Camera);
		}

		Vec3 clearColor = mv_backgroundColor;

		SRenderingPassInfo passInfo = SRenderingPassInfo::CreateGeneralPassRenderingInfo(m_Camera, SRenderingPassInfo::DEFAULT_FLAGS, true, displayContextKey);
		passInfo.GetIRenderView()->SetTargetClearColor(ColorF(clearColor, 1.0f), true);

		{
			CScopedWireFrameMode scopedWireFrame(m_renderer, mv_showWireframe1 ? R_WIREFRAME_MODE : R_SOLID_MODE);
			DrawModel(passInfo);
		}

		ICharacterManager* pCharMan = gEnv->pCharacterManager;
		if (pCharMan)
		{
			pCharMan->UpdateStreaming(-1, -1);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::DrawSkyBox(const SRenderingPassInfo& passInfo)
{
	CRenderObject* pObj = passInfo.GetIRenderView()->AllocateTemporaryRenderObject();
	pObj->SetMatrix(Matrix34::CreateTranslationMat(GetViewTM().GetTranslation()), passInfo);

	if (m_pSkyboxName)
	{
		passInfo.GetIRenderView()->AddRenderObject(m_pRESky, SShaderItem(m_pSkyBoxShader), pObj, passInfo, EFSLIST_GENERAL, 1);
	}
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	// TODO: Add your message handler code here and/or call default

	Matrix34 tm;
	tm.SetIdentity();
	SetViewTM(tm);
}

//////////////////////////////////////////////////////////////////////////
bool CModelViewport::OnKeyDown(uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (GetCharacterBase())
	{
		int index = -1;
		if (nChar >= 0x30 && nChar <= 0x39)
		{
			if (nChar == 0x30)
				index = 10;
			else
				index = nChar - 0x31;
		}
		if (nChar >= VK_NUMPAD0 && nChar <= VK_NUMPAD9)
		{
			index = nChar - VK_NUMPAD0 + 10;
			if (nChar == VK_NUMPAD0)
				index = 20;
		}
		//if (nFlags& MK_CONTROL

		IAnimationSet* pAnimations = GetCharacterBase()->GetIAnimationSet();
		if (pAnimations)
		{
			uint32 numAnims = pAnimations->GetAnimationCount();
			if (index >= 0 && index < numAnims)
			{
				const char* animName = pAnimations->GetNameByAnimID(index);
				PlayAnimation(animName);
				return true;
			}
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::OnLightColor(IVariable* var)
{
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::OnShowNormals(IVariable* var)
{
	bool enable = mv_showNormals;
	GetIEditor()->SetConsoleVar("r_ShowNormals", (enable) ? 1 : 0);
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::OnShowTangents(IVariable* var)
{
	bool enable = mv_showTangents;
	GetIEditor()->SetConsoleVar("r_ShowTangents", (enable) ? 1 : 0);
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::OnCharPhysics(IVariable* var)
{
	bool enable = mv_useCharPhysics;
	GetIEditor()->SetConsoleVar("ca_UsePhysics", enable);
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::AttachObjectToBone(const string& model, const string& bone)
{
	if (!GetCharacterBase())
		return;

	IAttachmentObject* pBindable = NULL;
	m_attachedCharacter = m_pAnimationSystem->CreateInstance(model);
	if (m_attachedCharacter)
		m_attachedCharacter->AddRef();

	if (m_weaponModel)
	{
		SAFE_RELEASE(m_weaponModel);
	}

	if (!m_attachedCharacter)
	{
		m_weaponModel = m_engine->LoadStatObj(model, 0, 0, false);
		m_weaponModel->AddRef();
	}

	m_attachBone = bone;
	if (!pBindable)
	{
		string str;
		str.Format("Loading of weapon model %s failed.", (const char*)model);
		CQuestionDialog::SCritical(QObject::tr(""), QObject::tr(str));
		CryLog(str);
		return;
	}

	IAttachmentManager* pIAttachments = GetCharacterBase()->GetIAttachmentManager();
	if (!m_attachedCharacter)
	{
		IAttachment* pAttachment = pIAttachments->CreateAttachment("BoneAttachment", CA_BONE, bone);
		assert(pAttachment);
		pAttachment->AddBinding(pBindable);
	}
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::AttachObjectToFace(const string& model)
{
	if (!GetCharacterBase())
		return;

	if (m_weaponModel)
		m_weaponModel->Release();

	IAttachmentObject* pBindable = NULL;
	m_weaponModel = m_engine->LoadStatObj(model, 0, 0, false);
	m_weaponModel->AddRef();

	if (!pBindable)
	{
		string str;
		str.Format("Loading of weapon model %s failed.", (const char*)model);
		CQuestionDialog::SCritical(QObject::tr(""), QObject::tr(str));
		CryLog(str);
		return;
	}
	IAttachmentManager* pAttachments = GetCharacterBase()->GetIAttachmentManager();
	IAttachment* pIAttachment = pAttachments->CreateAttachment("FaceAttachment", CA_FACE);
	pIAttachment->AddBinding(pBindable);

}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::OnShowShaders(IVariable* var)
{
	bool bEnable = mv_showShaders;
	GetIEditor()->SetConsoleVar("r_ProfileShaders", bEnable);
}

void CModelViewport::OnDestroy()
{
	ReleaseObject();
	if (m_pRESky)
		m_pRESky->Release(false);
}


//////////////////////////////////////////////////////////////////////////
void CModelViewport::Update()
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	CRenderViewport::Update();

	if (m_pIAudioListener != nullptr)
	{
		m_pIAudioListener->SetTransformation(m_viewTM);
	}

	if (CScopedRenderLock lock = CScopedRenderLock())
	{
		if (GetIEditor()->Get3DEngine())
		{
			ICVar* pDisplayInfo = gEnv->pConsole->GetCVar("r_DisplayInfo");
			if (pDisplayInfo && pDisplayInfo->GetIVal() != 0)
			{
				const float x = float(IRenderAuxGeom::GetAux()->GetCamera().GetViewSurfaceX() - 5);
				const float y = float(IRenderAuxGeom::GetAux()->GetCamera().GetViewSurfaceZ());
				const float fps = gEnv->pTimer->GetFrameRate();

				gEnv->p3DEngine->DrawTextRightAligned(x, 1, "FPS: %.2f", fps);

				int nPolygons, nShadowVolPolys;
				gEnv->pRenderer->GetPolyCount(nPolygons, nShadowVolPolys);
				int nDrawCalls = gEnv->pRenderer->GetCurrentNumberOfDrawCalls();
				gEnv->p3DEngine->DrawTextRightAligned(x, 20, "Tris:%2d,%03d - DP:%d", nPolygons / 1000, nPolygons % 1000, nDrawCalls);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::SetCustomMaterial(IEditorMaterial* pMaterial)
{
	m_pCurrentMaterial = pMaterial;
}

//////////////////////////////////////////////////////////////////////////
IEditorMaterial* CModelViewport::GetMaterial()
{
	if (m_pCurrentMaterial)
		return m_pCurrentMaterial;
	else
	{
		IMaterial* pMtl = 0;
		if (m_object)
			pMtl = m_object->GetMaterial();
		else if (GetCharacterBase())
			pMtl = GetCharacterBase()->GetIMaterial();

		if (pMtl)
		{
			return (IEditorMaterial*)pMtl->GetUserData();
		}
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
bool CModelViewport::CanDrop(CPoint point, IDataBaseItem* pItem)
{
	if (!pItem)
		return false;

	if (pItem->GetType() == EDB_TYPE_MATERIAL)
	{
		SetCustomMaterial((IEditorMaterial*)pItem);
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::Drop(CPoint point, IDataBaseItem* pItem)
{
	if (!pItem)
	{
		SetCustomMaterial(NULL);
		return;
	}

	if (pItem->GetType() == EDB_TYPE_MATERIAL)
	{
		SetCustomMaterial((IEditorMaterial*)pItem);
	}
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::Physicalize()
{
	IPhysicalWorld* pPhysWorld = gEnv->pPhysicalWorld;
	if (!pPhysWorld)
		return;

	if (!m_object && !GetCharacterBase())
		return;

	if (m_pPhysicalEntity)
	{
		pPhysWorld->DestroyPhysicalEntity(m_pPhysicalEntity);
		if (GetCharacterBase())
		{
			if (GetCharacterBase()->GetISkeletonPose()->GetCharacterPhysics(-1))
				pPhysWorld->DestroyPhysicalEntity(GetCharacterBase()->GetISkeletonPose()->GetCharacterPhysics(-1));
			GetCharacterBase()->GetISkeletonPose()->SetCharacterPhysics(NULL);
		}
	}

	// Add geometries.
	if (m_object)
	{
		m_pPhysicalEntity = pPhysWorld->CreatePhysicalEntity(PE_STATIC, NULL, NULL, 0);
		if (!m_pPhysicalEntity)
			return;
		pe_geomparams params;
		params.flags = geom_colltype_ray;
		for (int i = 0; i < 4; i++)
		{
			if (m_object->GetPhysGeom(i))
				m_pPhysicalEntity->AddGeometry(m_object->GetPhysGeom(i), &params);
		}
		// Add all sub mesh geometries.
		for (int nobj = 0; nobj < m_object->GetSubObjectCount(); nobj++)
		{
			IStatObj* pStatObj = m_object->GetSubObject(nobj)->pStatObj;
			if (pStatObj)
			{
				params.pMtx3x4 = &m_object->GetSubObject(nobj)->tm;
				for (int i = 0; i < 4; i++)
				{
					if (pStatObj->GetPhysGeom(i))
						m_pPhysicalEntity->AddGeometry(pStatObj->GetPhysGeom(i), &params);
				}
			}
		}
	}
	else
	{
		if (GetCharacterBase())
		{
			GetCharacterBase()->GetISkeletonPose()->DestroyCharacterPhysics();
			m_pPhysicalEntity = pPhysWorld->CreatePhysicalEntity(PE_LIVING);
			IPhysicalEntity* pCharPhys = GetCharacterBase()->GetISkeletonPose()->CreateCharacterPhysics(m_pPhysicalEntity, 80.0f, -1, 70.0f);
			GetCharacterBase()->GetISkeletonPose()->CreateAuxilaryPhysics(pCharPhys, Matrix34(IDENTITY));
			if (pCharPhys)
			{
				// make sure the skeleton goes before the ropes in the list
				pe_params_pos pp;
				pp.iSimClass = 6;
				pCharPhys->SetParams(&pp);
				pp.iSimClass = 4;
				pCharPhys->SetParams(&pp);
			}
			pe_player_dynamics pd;
			pd.bActive = 0;
			m_pPhysicalEntity->SetParams(&pd);
		}
	}

	// Set materials.
	IEditorMaterial* pMaterial = GetMaterial();
	if (pMaterial)
	{
		// Assign custom material to physics.
		int surfaceTypesId[MAX_SUB_MATERIALS];
		memset(surfaceTypesId, 0, sizeof(surfaceTypesId));
		int numIds = pMaterial->GetMatInfo()->FillSurfaceTypeIds(surfaceTypesId);

		pe_params_part ppart;
		ppart.nMats = numIds;
		ppart.pMatMapping = surfaceTypesId;
		(GetCharacterBase() && GetCharacterBase()->GetISkeletonPose()->GetCharacterPhysics() ?
		 GetCharacterBase()->GetISkeletonPose()->GetCharacterPhysics() : m_pPhysicalEntity)->SetParams(&ppart);
	}
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::RePhysicalize()
{
	m_pPhysicalEntity = NULL;
	Physicalize();
}

//////////////////////////////////////////////////////////////////////////
namespace
{
void AllowAnimEventsToTriggerAgain(ICharacterInstance& characterInstance)
{
	ISkeletonAnim& skeletonAnim = *characterInstance.GetISkeletonAnim();
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
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::SetPaused(bool bPaused)
{
	//return;
	if (m_bPaused != bPaused)
	{
		if (bPaused)
		{
			if (GetCharacterBase())
			{
				GetCharacterBase()->SetPlaybackScale(0.0f);
				AllowAnimEventsToTriggerAgain(*GetCharacterBase());
			}
		}
		else
		{
			//const float val = m_pCharPanel_Animation ? m_pCharPanel_Animation->GetPlaybackScale() : 1.0f;
			//if (GetCharacterBase())
			//GetCharacterBase()->SetPlaybackScale(val);
		}

		m_bPaused = bPaused;
	}
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::DrawModel(const SRenderingPassInfo& passInfo)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	m_renderer->EF_StartEf(passInfo);

	//////////////////////////////////////////////////////////////////////////
	// Draw lights.
	//////////////////////////////////////////////////////////////////////////
	if (mv_lighting == true)
	{
		uint32 numLights = m_VPLights.size();
		for (int i = 0; i < numLights; i++)
		{
			pAuxGeom->DrawSphere(m_VPLights[i].m_Origin, 0.2f, ColorB(255, 255, 0, 255));
		}
	}

	gEnv->pConsole->GetCVar("ca_DrawWireframe")->Set(mv_showWireframe2);
	gEnv->pConsole->GetCVar("ca_DrawTangents")->Set(mv_showTangents);
	gEnv->pConsole->GetCVar("ca_DrawBinormals")->Set(mv_showBinormals);
	gEnv->pConsole->GetCVar("ca_DrawNormals")->Set(mv_showNormals);

	DrawLights(passInfo);

	CRect rcClient;
	GetClientRect(&rcClient);
	//-------------------------------------------------------------
	//------           Render physical Proxy                 ------
	//-------------------------------------------------------------
	pAuxGeom->SetOrthographicProjection(true, 0.0f, rcClient.right, rcClient.bottom, 0.0f);
	if (m_pPhysicalEntity)
	{
		CPoint mousePoint;
		GetCursorPos(&mousePoint);
		ScreenToClient(&mousePoint);
		Vec3 raySrc, rayDir;
		ViewToWorldRay(mousePoint, raySrc, rayDir);

		ray_hit hit;
		hit.pCollider = 0;
		int flags = rwi_any_hit | rwi_stop_at_pierceable;
		int col = gEnv->pPhysicalWorld->RayWorldIntersection(raySrc, rayDir * 1000.0f, ent_static, flags, &hit, 1);
		if (col > 0)
		{
			int nMatId = hit.idmatOrg;

			string sMaterial;
			if (GetMaterial())
			{
				sMaterial = GetMaterial()->GetMatInfo()->GetSafeSubMtl(nMatId)->GetName();
			}

			ISurfaceType* pSurfaceType = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceType(hit.surface_idx, m_loadedFile);
			if (pSurfaceType)
			{
				float color[4] = { 1, 1, 1, 1 };
				//m_renderer->Draw2dLabel( mousePoint.x+12,mousePoint.y+8,1.2f,color,false,"%s\n%s",sMaterial.c_str(),pSurfaceType->GetName() );
			}
		}
	}
	pAuxGeom->SetOrthographicProjection(false);

	//-----------------------------------------------------------------------------
	//-----            Render Static Object (handled by 3DEngine)              ----
	//-----------------------------------------------------------------------------
	// calculate LOD

	f32 fDistance = GetViewTM().GetTranslation().GetLength();
	SRendParams rp;
	rp.fDistance = fDistance;

	Matrix34 tm;
	tm.SetIdentity();
	rp.pMatrix = &tm;
	rp.pPrevMatrix = &tm;

	Vec3 vAmbient;
	mv_objectAmbientColor.Get(vAmbient);

	rp.AmbientColor.r = vAmbient.x * mv_lightMultiplier;
	rp.AmbientColor.g = vAmbient.y * mv_lightMultiplier;
	rp.AmbientColor.b = vAmbient.z * mv_lightMultiplier;
	rp.AmbientColor.a = 1;

	rp.dwFObjFlags = 0;
	rp.dwFObjFlags |= FOB_TRANS_MASK;
	if (m_pCurrentMaterial)
		rp.pMaterial = m_pCurrentMaterial->GetMatInfo();

	//-----------------------------------------------------------------------------
	//-----            Render Static Object (handled by 3DEngine)              ----
	//-----------------------------------------------------------------------------
	if (m_object)
	{
		m_object->Render(rp, passInfo);
		if (mv_showGrid)
		{
			const bool bInstantCommit = true;
			DrawFloorGrid(Quat(IDENTITY), Vec3(ZERO), Matrix33(IDENTITY), bInstantCommit);
		}

		if (mv_showBase)
			DrawCoordSystem(IDENTITY, 10.0f);
	}

	//-----------------------------------------------------------------------------
	//-----             Render Character (handled by CryAnimation)            ----
	//-----------------------------------------------------------------------------
	if (GetCharacterBase())
		DrawCharacter(GetCharacterBase(), rp, passInfo);

	m_renderer->EF_EndEf3D(SHDF_ALLOWHDR | SHDF_SECONDARY_VIEWPORT, -1, -1, passInfo);
}

void CModelViewport::DrawLights(const SRenderingPassInfo& passInfo)
{
	//feature currently disabled
	/*if (mv_animateLights)
		m_LightRotationRadian += m_AverageFrameTime;

	if (m_LightRotationRadian > gf_PI)
		m_LightRotationRadian = -gf_PI;*/

	Matrix33 LightRot33 = Matrix33::CreateRotationZ(m_LightRotationRadian);
	uint32 numLights = m_VPLights.size();

	Vec3 LPos0 = Vec3(-mv_lightOrbit, mv_lightOrbit, mv_lightOrbit);
	m_VPLights[0].SetPosition(LightRot33 * LPos0 + m_PhysicalLocation.t);
	Vec3 d0 = mv_lightDiffuseColor0;
	m_VPLights[0].m_Flags |= DLF_POINT;
	m_VPLights[0].SetLightColor(ColorF(d0.x * mv_lightMultiplier, d0.y * mv_lightMultiplier, d0.z * mv_lightMultiplier, 0));
	m_VPLights[0].SetSpecularMult(mv_lightSpecMultiplier);
	m_VPLights[0].SetRadius(mv_lightRadius);

	//-----------------------------------------------

	Vec3 LPos1 = Vec3(-mv_lightOrbit, -mv_lightOrbit, -mv_lightOrbit / 2);
	m_VPLights[1].SetPosition(LightRot33 * LPos1 + m_PhysicalLocation.t);
	Vec3 d1 = mv_lightDiffuseColor1;
	m_VPLights[1].m_Flags |= DLF_POINT;
	m_VPLights[1].SetLightColor(ColorF(d1.x * mv_lightMultiplier, d1.y * mv_lightMultiplier, d1.z * mv_lightMultiplier, 0));
	m_VPLights[1].SetSpecularMult(mv_lightSpecMultiplier);
	m_VPLights[1].SetRadius(mv_lightRadius);

	//---------------------------------------------

	Vec3 LPos2 = Vec3(mv_lightOrbit, -mv_lightOrbit, 0);
	m_VPLights[2].SetPosition(LightRot33 * LPos2 + m_PhysicalLocation.t);
	Vec3 d2 = mv_lightDiffuseColor2;
	m_VPLights[2].m_Flags |= DLF_POINT;
	m_VPLights[2].SetLightColor(ColorF(d2.x * mv_lightMultiplier, d2.y * mv_lightMultiplier, d2.z * mv_lightMultiplier, 0));
	m_VPLights[2].SetSpecularMult(mv_lightSpecMultiplier);
	m_VPLights[2].SetRadius(mv_lightRadius);

	if (mv_lighting == true)
	{
#if 0
		// Add lights.
		if (numLights == 1)
		{
			m_renderer->EF_ADDDlight(&m_VPLights[0], passInfo);
		}

		if (numLights == 2)
		{
			m_renderer->EF_ADDDlight(&m_VPLights[0], passInfo);
			m_renderer->EF_ADDDlight(&m_VPLights[1], passInfo);
		}

		if (numLights == 3)
		{
			m_renderer->EF_ADDDlight(&m_VPLights[0], passInfo);
			m_renderer->EF_ADDDlight(&m_VPLights[1], passInfo);
			m_renderer->EF_ADDDlight(&m_VPLights[2], passInfo);
		}
#else
		// Just one directional light until tiled forward shading is fully supported
		if (numLights >= 1)
		{
			m_VPLights[0].m_Flags = DLF_SUN | DLF_DIRECTIONAL;
			m_renderer->EF_ADDDlight(&m_VPLights[0], passInfo);
		}
#endif
	}
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::PlayAnimation(const char* szName)
{

}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::DrawFloorGrid(const Quat& m33, const Vec3& vPhysicalLocation, const Matrix33& rGridRot, bool bInstantSubmit)
{
	if (!m_renderer)
		return;

	const float XR = 45.0f;
	const float YR = 45.0f;
	const float step = 0.25f;

	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();

	pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags);

	Vec3 axis = m33.GetColumn0();

	Matrix33 SlopeMat33 = rGridRot;
	//uint32 GroundAlign = (m_pCharPanel_Animation ? m_pCharPanel_Animation->GetGroundAlign() : 1);
	//if (GroundAlign==0)
	//SlopeMat33= Matrix33::CreateRotationAA( m_absCurrentSlope, axis);

	m_GridOrigin = Vec3(floorf(vPhysicalLocation.x), floorf(vPhysicalLocation.y), vPhysicalLocation.z);

	Matrix33 ScaleMat33 = IDENTITY;
	Vec3 rh = Matrix33::CreateRotationY(m_absCurrentSlope) * Vec3(1.0f, 0.0f, 0.0f);
	if (rh.x)
	{
		Vec3 xback = SlopeMat33.GetRow(0);
		Vec3 yback = SlopeMat33.GetRow(1);
		f32 ratiox = 1.0f / Vec3(xback.x, xback.y, 0.0f).GetLength();
		f32 ratioy = 1.0f / Vec3(yback.x, yback.y, 0.0f).GetLength();

		f32 ratio = 1.0f / rh.x;
		//	Vec3 h=Vec3((m_GridOrigin.x-vPhysicalLocation.x)*ratiox,(m_GridOrigin.y-vPhysicalLocation.y)*ratioy,0.0f);
		Vec3 h = Vec3(m_GridOrigin.x - vPhysicalLocation.x, m_GridOrigin.y - vPhysicalLocation.y, 0.0f);
		Vec3 nh = SlopeMat33 * h;
		m_GridOrigin.z += nh.z * ratio;

		ScaleMat33 = Matrix33::CreateScale(Vec3(ratiox, ratioy, 0.0f));

		//	float color1[4] = {0,1,0,1};
		//	m_renderer->Draw2dLabel(12,g_ypos,1.6f,color1,false,"h: %f %f %f    h.z: %f   ratio: %f  ratiox: %f ratioy: %f",h.x,h.y,h.z,  nh.z,ratio,ratiox,ratioy);
		//	g_ypos+=18;
	}

	Matrix33 _m33;
	_m33.SetIdentity();
	AABB aabb1 = AABB(Vec3(-0.03f, -YR, -0.001f), Vec3(0.03f, YR, 0.001f));
	OBB _obb1 = OBB::CreateOBBfromAABB(SlopeMat33, aabb1);
	AABB aabb2 = AABB(Vec3(-XR, -0.03f, -0.001f), Vec3(XR, 0.03f, 0.001f));
	OBB _obb2 = OBB::CreateOBBfromAABB(SlopeMat33, aabb2);

	SlopeMat33 = SlopeMat33 * ScaleMat33;

	// Draw thick lines on x and y axis.
	pAuxGeom->DrawOBB(_obb1, SlopeMat33 * Vec3(0.0f) + m_GridOrigin, 1, RGBA8(0x9f, 0x9f, 0x9f, 0x00), eBBD_Faceted);
	pAuxGeom->DrawOBB(_obb2, SlopeMat33 * Vec3(0.0f) + m_GridOrigin, 1, RGBA8(0x9f, 0x9f, 0x9f, 0x00), eBBD_Faceted);

	// Draw grid.
	const size_t nLineVertices = ( ((int)(XR / step) * 2 + 1) + ((int)(YR / step) * 2 + 1) ) * 2;
	if (m_gridLineVertices.size() != nLineVertices) m_gridLineVertices.resize(nLineVertices);
	Vec3* p = &m_gridLineVertices[0];

	int idx = 0;
	for (float x = -XR; x < XR; x += step)
	{
		p[idx++] = SlopeMat33 * Vec3(x, -YR, 0) + m_GridOrigin;
		p[idx++] = SlopeMat33 * Vec3(x,  YR, 0) + m_GridOrigin;
	}

	for (float y = -YR; y < YR; y += step)
	{
		p[idx++] = SlopeMat33 * Vec3(-XR, y, 0) + m_GridOrigin;
		p[idx++] = SlopeMat33 * Vec3( XR, y, 0) + m_GridOrigin;
	}

	pAuxGeom->DrawLines( p, nLineVertices, RGBA8(0x7f, 0x7f, 0x7f, 0x00) );

	// TODO - the grid should probably be an IRenderNode at some point
	// flushing grid geometry now so it will not override transparent
	// objects later in the render pipeline.
	if (bInstantSubmit) pAuxGeom->Submit();
}

//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
void CModelViewport::DrawCoordSystem(const QuatT& location, f32 length)
{
	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
	pAuxGeom->SetRenderFlags(renderFlags);

	Vec3 absAxisX = location.q.GetColumn0();
	Vec3 absAxisY = location.q.GetColumn1();
	Vec3 absAxisZ = location.q.GetColumn2();

	const f32 scale = 3.0f;
	const f32 size = 0.009f;
	AABB xaabb = AABB(Vec3(-length * scale, -size * scale, -size * scale), Vec3(length * scale, size * scale, size * scale));
	AABB yaabb = AABB(Vec3(-size * scale, -length * scale, -size * scale), Vec3(size * scale, length * scale, size * scale));
	AABB zaabb = AABB(Vec3(-size * scale, -size * scale, -length * scale), Vec3(size * scale, size * scale, length * scale));

	OBB obb;
	obb = OBB::CreateOBBfromAABB(Matrix33(location.q), xaabb);
	pAuxGeom->DrawOBB(obb, location.t, 1, RGBA8(0xff, 0x00, 0x00, 0xff), eBBD_Extremes_Color_Encoded);
	pAuxGeom->DrawCone(location.t + absAxisX * length * scale, absAxisX, 0.03f * scale, 0.15f * scale, RGBA8(0xff, 0x00, 0x00, 0xff));

	obb = OBB::CreateOBBfromAABB(Matrix33(location.q), yaabb);
	pAuxGeom->DrawOBB(obb, location.t, 1, RGBA8(0x00, 0xff, 0x00, 0xff), eBBD_Extremes_Color_Encoded);
	pAuxGeom->DrawCone(location.t + absAxisY * length * scale, absAxisY, 0.03f * scale, 0.15f * scale, RGBA8(0x00, 0xff, 0x00, 0xff));

	obb = OBB::CreateOBBfromAABB(Matrix33(location.q), zaabb);
	pAuxGeom->DrawOBB(obb, location.t, 1, RGBA8(0x00, 0x00, 0xff, 0xff), eBBD_Extremes_Color_Encoded);
	pAuxGeom->DrawCone(location.t + absAxisZ * length * scale, absAxisZ, 0.03f * scale, 0.15f * scale, RGBA8(0x00, 0x00, 0xff, 0xff));
}

//////////////////////////////////////////////////////////////////////////
void CModelViewport::OnLightMultiplier(IVariable* var)
{

}

//////////////////////////////////////////////////////////////////////////
bool CModelViewport::UseAnimationDrivenMotion() const
{
	return false;
}

