// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "IEditorImpl.h"
#include "IObjectManager.h"
#include "Objects/DisplayContext.h"
#include "Objects/InspectorWidgetCreator.h"
#include "IBackgroundScheduleManager.h"
#include "EnvironmentProbeObject.h"
#include "Util/CubemapUtils.h"
#include "Util/Variable.h"
#include "FilePathUtil.h"
#include <Preferences/ViewportPreferences.h>
#include "GameEngine.h"
#include "../Cry3DEngine/StatObj.h"
#include "../Cry3DEngine/Material.h"
#include "../Cry3DEngine/MatMan.h"
#include <CryRenderer/IShader.h> // <> required for Interfuscator
#include <Cry3DEngine/ITimeOfDay.h>
#include <Serialization/Decorators/EditorActionButton.h>
#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h>
#include "Gizmos/AxisHelper.h"
#include "RenderLock.h"
#include "CrySystem/ISystem.h"
#include "CrySystem/File/ICryPak.h"

class CubeMapGenerationSchedule : public IBackgroundScheduleItemWork, public IAsyncTextureCompileListener
{
public:
	CubeMapGenerationSchedule(const string& filepath, const string& relFilepath, CryGUID cubeguid, int cubemapres)
		: m_cubemapguid(cubeguid)
		, m_filepath(filepath)
		, m_relFilepath(relFilepath)
		, m_texdone(0)
		, m_refCount(0)
		, m_bResult(false)
		, m_cubemapres(cubemapres)
	{
	}

	bool isCubemapLoaded() const
	{
		string ddsName = PathUtil::ReplaceExtension(m_relFilepath, "dds");
		const ITexture* const pTexture = GetIEditor()->GetSystem()->GetIRenderer()->EF_GetTextureByName(ddsName);
		return pTexture != nullptr;
	}

	virtual bool OnStart() override
	{
		// add as listener to the texture compiler
		gEnv->pRenderer->AddAsyncTextureCompileListener(this);

		CBaseObject* pObj = GetIEditorImpl()->GetObjectManager()->FindObject(m_cubemapguid);
		// our object could very well have been deleted in the meantime
		if (!pObj || m_cubemapres == 0)
		{
			// return true, we don't want to interrupt the whole scheduleitem
			m_texdone = 1;
			m_filepath = "";
			return true;
		}

		CEnvironementProbeObject* pProbe = static_cast<CEnvironementProbeObject*>(pObj);

		if (CScopedRenderLock lock = CScopedRenderLock())
		{
			// Do not wait for the TextureCompiler event if the generation fails or the cubemap is already exists.
			if (!CubemapUtils::GenCubemapWithObjectPathAndSize(m_filepath, pProbe, m_cubemapres, false) || isCubemapLoaded())
			{
				m_texdone = 1;
				m_filepath = "";
			}
		}

		return true;
	}

	virtual bool OnStop() override
	{
		gEnv->pRenderer->RemoveAsyncTextureCompileListener(this);
		return true;
	}

	virtual const char* GetDescription() const override
	{
		return "Update Cubemap Lights after Cubemap textures get generated";
	}

	virtual float GetProgress() const override
	{
		return m_texdone ? 1.0f : 0.0f;
	}

	virtual EScheduleWorkItemStatus OnUpdate() override
	{
		if (m_texdone > 0)
		{
			gEnv->pRenderer->RemoveAsyncTextureCompileListener(this);

			if (m_bResult)
			{
				// update environment probe synchronously with main thread here and exit
				CBaseObject* pObj = GetIEditorImpl()->GetObjectManager()->FindObject(m_cubemapguid);

				// our object could very well have been deleted in the meantime
				if (!pObj)
				{
					// return finished, we don't want to interrupt the whole scheduleitem
					return eScheduleWorkItemStatus_Finished;
				}

				CEnvironementProbeObject* pProbe = static_cast<CEnvironementProbeObject*>(pObj);

				auto probeProperties = pProbe->GetProperties();
				if (!probeProperties)
				{
					// return failed, as something is wrong with the fact
					// that the probe has no properties block
					// here it used to just plain crash the whole engine instead
					return eScheduleWorkItemStatus_Failed;
				}
				IVariable* pVar = probeProperties->FindVariable("texture_deferred_cubemap", true);
				if (!pVar)
				{
					// return finished, we don't want to interrupt the whole scheduleitem
					return eScheduleWorkItemStatus_Finished;
				}

				PathUtil::ToUnixPath(m_relFilepath.GetBuffer());
				if (GetIEditorImpl()->GetConsoleVar("ed_lowercasepaths"))
				{
					m_relFilepath = m_relFilepath.MakeLower();
				}

				pVar->Set(m_relFilepath);

				if (pProbe->GetVisualObject())
				{
					pProbe->GetVisualObject()->SetMaterial(pProbe->CreateMaterial());
				}
				pProbe->UpdateLinks();
				pProbe->UpdateUIVars();
			}
			else
			{
				return eScheduleWorkItemStatus_Finished;
			}

			return eScheduleWorkItemStatus_Finished;
		}
		else
		{
			return eScheduleWorkItemStatus_NotFinished;
		}
	}

	// Reference counting
	virtual void AddRef() override
	{
		CryInterlockedIncrement(&m_refCount);
	}

	virtual void Release() override
	{
		if (CryInterlockedDecrement(&m_refCount) == 0)
		{
			delete this;
		}
	}

	// For IAsyncTextureCompileListener
	virtual void OnCompilationStarted(const char* source, const char* target, int nPending) override {}

	virtual void OnCompilationFinished(const char* source, const char* target, ERcExitCode eReturnCode) override
	{
		if (m_relFilepath.CompareNoCase(PathUtil::ToGamePath(source)) == 0)
		{
			CryInterlockedIncrement(&m_texdone);
			m_bResult = (eReturnCode == ERcExitCode::eRcExitCode_Success);
		}
	}

	virtual void OnCompilationQueueTriggered(int nPending) override {}
	virtual void OnCompilationQueueDepleted() override              {}

protected:
	CryGUID      m_cubemapguid;
	string      m_filepath;
	string      m_relFilepath;
	volatile int m_texdone;
	volatile int m_refCount;
	bool         m_bResult;
	int          m_cubemapres;
};

REGISTER_CLASS_DESC(CEnvironmentProbeObjectClassDesc);

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CEnvironementProbeObject, CEntityObject)

//////////////////////////////////////////////////////////////////////////
CEnvironementProbeObject::CEnvironementProbeObject()
{
	m_entityClass = "EnvironmentLight";
	UseMaterialLayersMask(false);
}

//////////////////////////////////////////////////////////////////////////
bool CEnvironementProbeObject::Init(CBaseObject* prev, const string& file)
{
	bool res = CEntityObject::Init(prev, file);
	return res;
}

//////////////////////////////////////////////////////////////////////////
void CEnvironementProbeObject::InitVariables()
{
	CVarEnumList<int>* enumList = new CVarEnumList<int>;
	enumList->AddItem("skip update", 0);
	//enumList->AddItem( "32",32 );
	//enumList->AddItem( "64",64 );
	//enumList->AddItem( "128",128 );
	enumList->AddItem("256", 256);
	enumList->AddItem("512", 512);
	enumList->AddItem("1024", 1024);
	m_cubemap_resolution->SetEnumList(enumList);
	m_cubemap_resolution->SetDisplayValue("256");
	m_cubemap_resolution->SetFlags(m_cubemap_resolution->GetFlags() | IVariable::UI_UNSORTED);

	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_pVarObject->AddVariable(m_cubemap_resolution, string("cubemap_resolution"));
	m_pVarObject->AddVariable(m_preview_cubemap, string("preview_cubemap"), functor(*this, &CEnvironementProbeObject::OnPreviewCubemap), IVariable::DT_SIMPLE);

	m_preview_cubemap->Set(false);

	CEntityObject::InitVariables();
}

//////////////////////////////////////////////////////////////////////////
void CEnvironementProbeObject::Serialize(CObjectArchive& ar)
{
	m_preview_cubemap->Set(false);
	CEntityObject::Serialize(ar);
}

void CEnvironementProbeObject::SetScriptName(const string& file, CBaseObject* pPrev)
{
	// HACK: We override this function because environment probes need to manually set their light param
	// after the entity has set/initialized the script. This should be reconsidered when we move to
	// the entity component system since initialization order for entities is currently one big mess
	CEntityObject::SetScriptName(file, pPrev);

	m_bLight = 1;
}

//////////////////////////////////////////////////////////////////////////
void CEnvironementProbeObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CEntityObject::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CEnvironementProbeObject>("Cubemap", [](CEnvironementProbeObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		pObject->m_pVarObject->SerializeVariable(pObject->m_cubemap_resolution.GetVar(), ar);
		pObject->m_pVarObject->SerializeVariable(pObject->m_preview_cubemap.GetVar(), ar);

		if (ar.openBlock("generate", "<Generate"))
		{
		  ar(Serialization::ActionButton(&CubemapUtils::GenerateCubemaps), "generate_cubemap", "^Cubemaps");
		  ar.closeBlock();
		}
	});
}

/////////////////////////////////////////////////////////////////////////
void CEnvironementProbeObject::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();

	if (!gViewportDebugPreferences.showEnviromentProbeObjectHelper)
		return;

	Matrix34 wtm = GetWorldTM();
	Vec3 wp = wtm.GetTranslation();
	float fScale = GetHelperScale();

	if (IsHighlighted() && !IsFrozen())
	{
		dc.SetLineWidth(3);
	}

	if (IsSelected())
	{
		dc.SetSelectedColor();
	}
	else if (IsFrozen())
	{
		dc.SetFreezeColor();
	}
	else
	{
		dc.SetColor(GetColor());
	}

	if (!m_visualObject)
	{
		dc.PushMatrix(wtm);
		Vec3 sz(fScale * 0.5f, fScale * 0.5f, fScale * 0.5f);
		dc.DrawWireBox(-sz, sz);

		// Draw radiuses if present and object selected.
		if (gViewportPreferences.alwaysShowRadiuses || IsSelected())
		{
			if (m_bBoxProjectedCM)
			{
				// Draw helper for box projection.
				float fBoxWidth = m_fBoxWidth * 0.5f;
				float fBoxHeight = m_fBoxHeight * 0.5f;
				float fBoxLength = m_fBoxLength * 0.5f;

				dc.SetColor(0, 1, 0, 0.8f);
				dc.DrawWireBox(Vec3(-fBoxLength, -fBoxWidth, -fBoxHeight), Vec3(fBoxLength, fBoxWidth, fBoxHeight));
			}

			const Vec3& scale = GetScale();
			if (scale.x != 0.0f && scale.y != 0.0f && scale.z != 0.0f)
			{
				Vec3 size(m_boxSizeX * 0.5f / scale.x, m_boxSizeY * 0.5f / scale.y, m_boxSizeZ * 0.5f / scale.z);

				dc.SetColor(1, 1, 0, 0.8f);
				dc.DrawWireBox(Vec3(-size.x, -size.y, -size.z), Vec3(size.x, size.y, size.z));
			}
		}
		dc.PopMatrix();
	}

	if (IsHighlighted() && !IsFrozen())
	{
		dc.SetLineWidth(0);
	}

	if (!m_visualObject)
	{
		DrawDefault(dc);
	}

	if (m_visualObject)
	{
		Matrix34 tm(wtm);
		float sz = m_helperScale * gGizmoPreferences.helperScale;
		tm.ScaleColumn(Vec3(sz, sz, sz));

		SRendParams rp;
		rp.AmbientColor = ColorF(0.0f, 0.0f, 0.0f, 1);
		rp.dwFObjFlags |= FOB_TRANS_MASK;
		rp.fAlpha = 1;
		rp.pMatrix = &tm;
		rp.pMaterial = m_visualObject->GetMaterial();

		SRenderingPassInfo passInfo = SRenderingPassInfo::CreateGeneralPassRenderingInfo(gEnv->p3DEngine->GetRenderingCamera(), SRenderingPassInfo::DEFAULT_FLAGS, true, dc.GetDisplayContextKey());
		m_visualObject->Render(rp, passInfo);
	}
}

void CEnvironementProbeObject::GetDisplayBoundBox(AABB& box)
{
	// when object is selected, we also see the limits of the environment cube, so use that as a display bounding box
	if (CheckFlags(OBJFLAG_SELECTED))
	{
		const float fScale = GetHelperScale();
		const Vec3& position = GetWorldPos();
		const Vec3 halfsize(max(fScale, m_boxSizeX) * 0.5f, max(fScale, m_boxSizeY) * 0.5f, max(fScale, m_boxSizeZ) * 0.5f);

		box.max = position + halfsize;
		box.min = position - halfsize;
	}
	else
	{
		GetBoundBox(box);
	}
}

//////////////////////////////////////////////////////////////////////////
//! Get bounding box of object in world coordinate space.
void CEnvironementProbeObject::GetBoundBox(AABB& box)
{
	GetLocalBounds(box);

	const Vec3& position = GetWorldPos();
	box.max += position;
	box.min += position;
}

void CEnvironementProbeObject::GetLocalBounds(AABB& aabb)
{
	const float fScale = GetHelperScale();
	const Vec3 halfsize(fScale * 0.5f, fScale * 0.5f, fScale * 0.5f);

	aabb.max = halfsize;
	aabb.min = -halfsize;
}

//////////////////////////////////////////////////////////////////////////
IBackgroundScheduleItemWork* CEnvironementProbeObject::GenerateCubemapTask()
{
	const string levelfolder(GetIEditorImpl()->GetGameEngine()->GetLevelPath());
	const string levelname(PathUtil::GetFile(levelfolder).MakeLower());
	const string fullGameFolder(PathUtil::GetGameProjectAssetsPath());
	const string texturename(string().Format("%s_cm.tif", GetName().c_str()).MakeLower());

	const string relFolder(PathUtil::Make("textures/cubemaps", levelname));
	const string relFilename(PathUtil::Make(relFolder, texturename));
	const string fullFolder(PathUtil::Make(fullGameFolder, relFolder));
	const string fullFilename(PathUtil::Make(fullGameFolder, relFilename));
	GetISystem()->GetIPak()->MakeDir(fullFolder);

	int cubemapres = 256;
	m_cubemap_resolution->Get(cubemapres);

	// create a new workitem to listen for events
	return new CubeMapGenerationSchedule(fullFilename, relFilename, GetId(), cubemapres);
}

void CEnvironementProbeObject::GenerateCubemap()
{
	IBackgroundSchedule* envmapTexSchedule = GetIEditorImpl()->GetBackgroundScheduleManager()->CreateSchedule("Cubemap Texgen Schedule");
	IBackgroundScheduleItem* scheduleItem = GetIEditorImpl()->GetBackgroundScheduleManager()->CreateScheduleItem("Cubemap Texgen ScheduleItem");

	scheduleItem->AddWorkItem(GenerateCubemapTask());
	envmapTexSchedule->AddItem(scheduleItem);

	GetIEditorImpl()->GetBackgroundScheduleManager()->SubmitSchedule(envmapTexSchedule);
}
//////////////////////////////////////////////////////////////////////////
IMaterial* CEnvironementProbeObject::CreateMaterial()
{
	string deferredCubemapPath;
	string matName;
	if (!GetProperties())
		return NULL;

	IVariable* pVar = GetProperties()->FindVariable("texture_deferred_cubemap", true);
	if (!pVar)
		return NULL;

	pVar->Get(deferredCubemapPath);

	matName = PathUtil::GetFileName(deferredCubemapPath.GetBuffer());
	IMaterialManager* pMatMan = GetIEditorImpl()->Get3DEngine()->GetMaterialManager();
	IMaterial* pMatSrc = pMatMan->LoadMaterial("%EDITOR%/Objects/envcube", false, true);
	if (pMatSrc)
	{
		IMaterial* pMatDst = pMatMan->CreateMaterial((const char*)matName, pMatSrc->GetFlags() | MTL_FLAG_NON_REMOVABLE);
		if (pMatDst)
		{
			SShaderItem& si = pMatSrc->GetShaderItem();
			SInputShaderResourcesPtr isr = gEnv->pRenderer->EF_CreateInputShaderResource(si.m_pShaderResources);
			isr->m_Textures[EFTT_ENV].m_Name = deferredCubemapPath;
			SShaderItem siDst = GetIEditorImpl()->GetRenderer()->EF_LoadShaderItem(si.m_pShader->GetName(), true, 0, isr, si.m_pShader->GetGenerationMask());
			pMatDst->AssignShaderItem(siDst);
			return pMatDst;
		}
	}
	return NULL;
}
//////////////////////////////////////////////////////////////////////////
void CEnvironementProbeObject::OnPreviewCubemap(IVariable* piVariable)
{
	if (!m_pEntity)
		return;

	bool preview = false;
	piVariable->Get(preview);
	if (preview)
	{
		m_visualObject = GetIEditorImpl()->Get3DEngine()->LoadStatObj("%EDITOR%/Objects/envcube.cgf", nullptr, nullptr, false);
		m_visualObject->SetMaterial(CreateMaterial());
		m_visualObject->AddRef();
	}
	else
	{
		if (m_visualObject)
			m_visualObject->Release();
		m_visualObject = nullptr;
	}
}

int CEnvironementProbeObject::AddEntityLink(const string& name, CryGUID targetEntityId)
{
	int ret = CEntityObject::AddEntityLink(name, targetEntityId);
	if (ret != -1)
	{
		UpdateLinks();
	}
	return ret;
}

void CEnvironementProbeObject::UpdateLinks()
{
	int count = GetEntityLinkCount();
	for (int idx = 0; idx < count; idx++)
	{
		CEntityLink link = GetEntityLink(idx);
		CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(link.targetId);
		if (!pObject)
			continue;

		CEntityObject* pTarget = NULL;
		if (pObject && pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
			pTarget = (CEntityObject*)pObject;

		if (!pTarget)
			continue;

		CVarBlock* pVarBlock = NULL;
		string type = pObject->GetTypeDescription();
		if (stricmp(type, "Light") == 0)
		{
			pVarBlock = pTarget->GetProperties();
			if (!pVarBlock)
				continue;
		}
		else if (stricmp(type, "DestroyableLight") == 0 || stricmp(type, "RigidBodyLight") == 0)
		{
			pVarBlock = pTarget->GetProperties2();
			if (!pVarBlock)
				continue;
		}
		else
		{
			continue;
		}

		CVarBlock* pProperties = GetProperties();
		if (!pProperties || !pVarBlock)
			continue;

		IVariable* pDstDeferredCubemap = pVarBlock->FindVariable("texture_deferred_cubemap", true);
		IVariable* pDeferredCubemap = pProperties->FindVariable("texture_deferred_cubemap", true);

		if (!pDstDeferredCubemap || !pDeferredCubemap)
			continue;

		bool bDeferred = false;
		string strCubemap;

		pDeferredCubemap->Get(strCubemap);
		pDstDeferredCubemap->Set(strCubemap);
	}
}

void CEnvironementProbeObject::OnPropertyChanged(IVariable* pVar)
{
	UpdateLinks();

	if (stricmp(pVar->GetName(), "fHour") == 0)
	{
		float hour;
		pVar->Get(hour);
		if (hour < float(0.0))
			pVar->Set(float(0.0));
		else if (hour > float(24.0))
			pVar->Set(float(24.0));
	}

	CBaseObject::OnPropertyChanged(pVar);
}

void CEnvironementProbeObject::OnMultiSelPropertyChanged(IVariable* pVar)
{
	UpdateLinks();
	CBaseObject::OnMultiSelPropertyChanged(pVar);
}

