// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CrySystem/IStreamEngine.h>
#include <CryFlowGraph/IFlowBaseNode.h>
#include <CryEntitySystem/IEntityLayer.h>

class CFlowNode_PortalSwitch : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlowNode_PortalSwitch(SActivationInfo* pActInfo)
		: m_bActivated(false)
	{
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_PortalSwitch(pActInfo); }

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<SFlowSystemVoid>("Activate"),
			InputPortConfig<SFlowSystemVoid>("Deactivate"),
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.Value("activated", m_bActivated);

		if (ser.IsReading())
			ActivatePortal(pActInfo, m_bActivated);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_SetEntityId:
			{
				if (!pActInfo->pEntity)
					return;
				// don't disable here, because it will trigger on QL as well
				//gEnv->p3DEngine->ActivatePortal( pActInfo->pEntity->GetWorldPos(), false, pActInfo->pEntity->GetName() );
			}
			break;

		case eFE_Initialize:
			{
				ActivatePortal(pActInfo, false);    // i think this one is not needed anymore, but im keeping it just in case
			}
			break;
		case eFE_Activate:
			{
				if (!pActInfo->pEntity)
				{
					GameWarning("[flow] Trying to activate a portal at a non-existent entity");
					return;
				}
				bool doAnything = false;
				bool activate = false;
				if (IsPortActive(pActInfo, 0))
				{
					doAnything = true;
					activate = true;
				}
				else if (IsPortActive(pActInfo, 1))
				{
					doAnything = true;
					activate = false;
				}
				if (doAnything)
				{
					ActivatePortal(pActInfo, activate);
				}
			}
			break;
		}
	}

	void ActivatePortal(SActivationInfo* pActInfo, bool bActivate)
	{
		if (pActInfo->pEntity)
		{
			gEnv->p3DEngine->ActivatePortal(pActInfo->pEntity->GetWorldPos(), bActivate, pActInfo->pEntity->GetName());
			m_bActivated = bActivate;
		}
	}

private:
	bool m_bActivated;

};

class CFlowNode_OcclusionAreaSwitch : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlowNode_OcclusionAreaSwitch(SActivationInfo* pActInfo)
		: m_bActivated(false)
	{
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_OcclusionAreaSwitch(pActInfo); }

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<SFlowSystemVoid>("Activate"),
			InputPortConfig<SFlowSystemVoid>("Deactivate"),
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.Value("activated", m_bActivated);

		if (ser.IsReading())
			ActivateOcclusionAreas(pActInfo, m_bActivated);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_SetEntityId:
		{
			if (!pActInfo->pEntity)
				return;
			// don't disable here, because it will trigger on QL as well
			//gEnv->p3DEngine->ActivatePortal( pActInfo->pEntity->GetWorldPos(), false, pActInfo->pEntity->GetName() );
		}
		break;

		case eFE_Initialize:
		{
			ActivateOcclusionAreas(pActInfo, false);    // i think this one is not needed anymore, but im keeping it just in case
		}
		break;
		case eFE_Activate:
		{
			if (!pActInfo->pEntity)
			{
				GameWarning("[flow] Trying to activate a portal at a non-existent entity");
				return;
			}
			bool doAnything = false;
			bool activate = false;
			if (IsPortActive(pActInfo, 0))
			{
				doAnything = true;
				activate = true;
			}
			else if (IsPortActive(pActInfo, 1))
			{
				doAnything = true;
				activate = false;
			}
			if (doAnything)
			{
				ActivateOcclusionAreas(pActInfo, activate);
			}
		}
		break;
		}
	}

	struct AreaProxyTestCallback : public IVisAreaTestCallback
	{
		EntityId id;
		IEntityAreaComponent* proxy;
		AreaProxyTestCallback(EntityId id, IEntityAreaComponent* p) : id(id), proxy(p) {}
		virtual bool TestVisArea(IVisArea* pVisArea) const override
		{
			size_t nPoints;
			const Vec3* pPoints;
			pVisArea->GetShapePoints(pPoints, nPoints);

			for (size_t i = 0; i < nPoints; i++)
			{
				if (proxy->CalcPointWithin(id, pPoints[i]))
				{
					return true;
				}
			}
			return false;
		}
	};

	void ActivateOcclusionAreas(SActivationInfo* pActInfo, bool bActivate)
	{
		if (pActInfo->pEntity)
		{
			IEntityAreaComponent* pProxy = (IEntityAreaComponent*)pActInfo->pEntity->GetProxy(EEntityProxy::ENTITY_PROXY_AREA);

			if (pProxy)
			{
				const EntityId id = pActInfo->pEntity->GetId();
				AreaProxyTestCallback tester(id, pProxy);
				gEnv->p3DEngine->ActivateOcclusionAreas(&tester, bActivate);
			}

			m_bActivated = bActivate;
		}
	}

private:
	bool m_bActivated;

};

//////////////////////////////////////////////////////////////////////////////////////
// Node for ocean rendering management ///////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////

class CFlowNode_OceanSwitch : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_OceanSwitch(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool>("Enable", true, _HELP("Input for enabling/disabling ocean rendering")),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.sDescription = _HELP("Enabling ocean rendering on demand");
		config.SetCategory(EFLN_ADVANCED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, 0))
			{
				bool bEnabled = GetPortBool(pActInfo, 0);
				gEnv->p3DEngine->EnableOceanRendering(bEnabled);
			}
			break;
		}
	}
};

//////////////////////////////////////////////////////////////////////////////////////
//  Sky box asynchronous loading /////////////////////////////////////////////////////
//! [GDC09]: Async change SkyBox material[deprecated]	//////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
namespace SkyBoxLoading
{
class CSkyBoxLoadingCallback;
};

class CFlowNode_SkyboxSwitch : public CFlowBaseNode<eNCT_Instanced>
{
	friend class SkyBoxLoading::CSkyBoxLoadingCallback;
protected:
	IReadStreamPtr m_Job;
public:
	CFlowNode_SkyboxSwitch(SActivationInfo* pActInfo);
	virtual void         GetMemoryUsage(ICrySizer* s) const;
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_SkyboxSwitch(pActInfo); }
	void                 GetConfiguration(SFlowNodeConfig& config);
	void                 ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
};

namespace SkyBoxLoading
{
class CSkyBoxLoadingCallback : public IStreamCallback
{
	virtual void StreamAsyncOnComplete(IReadStream* pStream, unsigned nError);
	virtual void StreamOnComplete(IReadStream* pStream, unsigned nError);
};

struct SSkyboxLoadingInfo
{
	uint32                        nSelfSize;
	string                        sSkyTextureName;
	ITexture*                     pTextures[3];
	class CFlowNode_SkyboxSwitch* pParentNode;
	CSkyBoxLoadingCallback        pCallBack;

	SSkyboxLoadingInfo(class CFlowNode_SkyboxSwitch* _pParent)
	{
		pTextures[0] = NULL;
		pTextures[1] = NULL;
		pTextures[2] = NULL;
		pParentNode = _pParent;
		nSelfSize = sizeof(SSkyboxLoadingInfo);
	}
};

void CSkyBoxLoadingCallback::StreamAsyncOnComplete(IReadStream* pStream, unsigned nError)
{
	CRY_ASSERT(nError == 0);
	SSkyboxLoadingInfo* pSkyboxLoadingInfo = (SSkyboxLoadingInfo*)pStream->GetParams().dwUserData;
	if (!pSkyboxLoadingInfo || pSkyboxLoadingInfo->nSelfSize != sizeof(SSkyboxLoadingInfo))
	{
		CRY_ASSERT(0);
		return;
	}

	static const char* pNameSuffixes[] = { "box_12.dds", "box_34.dds", "box_5.dds" };
	string szNameBump;
	for (uint32 i = 0; i < 3; ++i)
	{
		szNameBump = PathUtil::Make(pSkyboxLoadingInfo->sSkyTextureName, pNameSuffixes[i]);
		// check if the texture exists
		if (gEnv->pCryPak->IsFileExist(szNameBump))
			pSkyboxLoadingInfo->pTextures[i] = gEnv->pRenderer->EF_LoadTexture(szNameBump, FT_DONT_STREAM);
		else
		{
			gEnv->pLog->LogError("Sky box texture not found: %s", szNameBump.c_str());
			for (uint32 j = i; j < 3; ++j)
			{
				SAFE_RELEASE(pSkyboxLoadingInfo->pTextures[i]);
			}
			return;
		}
		// in case of error release the rest textures
		if (!pSkyboxLoadingInfo->pTextures[i])
		{
			gEnv->pLog->LogError("Error loading sky box texture: %s", szNameBump.c_str());
			CRY_ASSERT(0);
			for (uint32 j = i; j < 3; ++j)
			{
				SAFE_RELEASE(pSkyboxLoadingInfo->pTextures[i]);
			}
			return;
		}
	}
}

void CSkyBoxLoadingCallback::StreamOnComplete(IReadStream* pStream, unsigned nError)
{
	CRY_ASSERT(nError == 0);
	SSkyboxLoadingInfo* pSkyboxLoadingInfo = (SSkyboxLoadingInfo*)pStream->GetParams().dwUserData;
	if (!pSkyboxLoadingInfo || pSkyboxLoadingInfo->nSelfSize != sizeof(SSkyboxLoadingInfo) || !pSkyboxLoadingInfo->pTextures[0])
	{
		CRY_ASSERT(0);
		gEnv->pLog->LogError("Error switching sky box");
		if (pSkyboxLoadingInfo != nullptr && pSkyboxLoadingInfo->pParentNode != nullptr)
		{
			pSkyboxLoadingInfo->pParentNode->m_Job = nullptr;
		}
		return;
	}

	IMaterial* pSkyMat = gEnv->p3DEngine->GetSkyMaterial();

	bool bSucceeded = false;
	if (pSkyMat && !(pSkyMat->GetFlags() & EF_SKY))
	{
		SShaderItem& rShaderItem = pSkyMat->GetShaderItem();
		if (rShaderItem.m_pShaderResources)
		{
			SSkyInfo* pSkyInfo = rShaderItem.m_pShaderResources->GetSkyInfo();

			if (pSkyInfo)
			{
				for (int i = 0; i < 3; ++i)
				{
					SAFE_RELEASE(pSkyInfo->m_SkyBox[i]);
					pSkyInfo->m_SkyBox[i] = pSkyboxLoadingInfo->pTextures[i];
				}
				bSucceeded = true;
			}
		}
	}

	// remove job
	pSkyboxLoadingInfo->pParentNode->m_Job = NULL;

	if (!bSucceeded)
	{
		gEnv->pLog->LogError("Error switching sky box: HDR sky box is not supported [deprecated]");
		for (int i = 0; i < 3; ++i)
			SAFE_RELEASE(pSkyboxLoadingInfo->pTextures[i]);
	}
	else
	{
		gEnv->pLog->Log("Sky box switched [deprecated]");
	}
}
}

CFlowNode_SkyboxSwitch::CFlowNode_SkyboxSwitch(SActivationInfo* pActInfo)
{
	m_Job = NULL;
}

void CFlowNode_SkyboxSwitch::GetMemoryUsage(ICrySizer* s) const
{
	s->Add(*this);
}

void CFlowNode_SkyboxSwitch::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig<string>("Texture",   "",    _HELP("Skybox texture name")),
		InputPortConfig<bool>("Start",       false, _HELP("Trigger to start the loading")),
		InputPortConfig<float>("Angle",      1.f,   _HELP("Sky box rotation")),
		InputPortConfig<float>("Stretching", 1.f,   _HELP("Sky box stretching")),
		{ 0 }
	};

	config.pInputPorts = in_config;
	config.sDescription = _HELP("Node for asynchronous sky box switching");
	config.SetCategory(EFLN_ADVANCED);
}

void CFlowNode_SkyboxSwitch::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Activate:
		// start loading signal
		if (IsPortActive(pActInfo, 1) && !m_Job)
		{
			// set up sky box size and angle
			gEnv->p3DEngine->SetGlobalParameter(E3DPARAM_SKY_SKYBOX_ANGLE, GetPortFloat(pActInfo, 2));
			gEnv->p3DEngine->SetGlobalParameter(E3DPARAM_SKY_SKYBOX_STRETCHING, GetPortFloat(pActInfo, 3));

			string sTextureName = GetPortString(pActInfo, 0);
			// start asynchronous job
			{
				// fill streaming params
				SkyBoxLoading::SSkyboxLoadingInfo* pSkyboxLoadingInfo = new SkyBoxLoading::SSkyboxLoadingInfo(this);
				pSkyboxLoadingInfo->sSkyTextureName = sTextureName;
				StreamReadParams pStreamParams;
				pStreamParams.nFlags = 0;
				pStreamParams.dwUserData = (DWORD_PTR)pSkyboxLoadingInfo;
				//				pStreamParams.nPriority = 0;
				pStreamParams.pBuffer = NULL;
				pStreamParams.nOffset = 0;
				pStreamParams.nSize = 0;
				m_Job = gEnv->pSystem->GetStreamEngine()->StartRead(eStreamTaskTypeTexture, sTextureName, &pSkyboxLoadingInfo->pCallBack, &pStreamParams);
			}
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////////////////
// Node for layer switching rendering management ///////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////

class CFlowNode_LayerSwitch : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_LayerSwitch(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	enum EInputs
	{
		eIN_LAYER_NAME = 0,
		eIN_HIDE,
		eIN_UNHIDE,
		eIN_SERIALIZE,
		eIN_DONT_SERIALIZE
	};

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<string>("Layer",             _HELP("Layer name")),
			InputPortConfig_Void("Hide",                 _HELP("Input for enabling objects in layer rendering")),
			InputPortConfig_Void("Unhide",               _HELP("Input for disabling objects in layer rendering")),
			InputPortConfig_Void("EnableSerialization",  _HELP("Input for enabling objects in this layer being saved/loaded.")),
			InputPortConfig_Void("DisableSerialization", _HELP("Input for disabling objects in this layer being saved/loaded.")),
			{ 0 }
		};

		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Hidden",   _HELP("Triggered if Boolean was False")),
			OutputPortConfig_Void("Unhidden", _HELP("Triggered if Boolean was True")),
			{ 0 }
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.sDescription = _HELP("Activate/Deactivate objects in layer");
		config.SetCategory(EFLN_ADVANCED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				const string& szLayerName = GetPortString(pActInfo, eIN_LAYER_NAME);
				const IEntityLayer* const pLayer = gEnv->pEntitySystem->FindLayer(szLayerName.c_str());

				if (IsPortActive(pActInfo, eIN_UNHIDE))
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
					if (pLayer)
					{
						EnableLayer(pActInfo, true, pLayer->IsSerialized());
					}
				}
				else
				{
					bool bEnabled = true;
					bool bSave = true;
					if (IsPortActive(pActInfo, eIN_HIDE))
					{
						bEnabled = false;
					}
					else if (pLayer)
					{
						bEnabled = pLayer->IsEnabled();
					}

					if (IsPortActive(pActInfo, eIN_SERIALIZE))
					{
						bSave = true;
					}
					else if (IsPortActive(pActInfo, eIN_DONT_SERIALIZE))
					{
						bSave = false;
					}
					else if (pLayer)
					{
						bSave = pLayer->IsSerialized();
					}

					EnableLayer(pActInfo, bEnabled, bSave);
				}
				break;
			}

		case eFE_Update:
			{
				EnableLayer(pActInfo, true, true);
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
				break;
			}
		}
	}

	void EnableLayer(SActivationInfo* pActInfo, bool bEnable, bool bSave)
	{
		stack_string layer = GetPortString(pActInfo, eIN_LAYER_NAME);

		if (gEnv->pEntitySystem)
		{
			gEnv->pEntitySystem->EnableLayer(layer, bEnable, bSave);
			if (bEnable && gEnv->pAISystem)
				gEnv->pAISystem->LayerEnabled(layer.c_str(), bEnable, bSave);
		}
		ActivateOutput(pActInfo, bEnable ? 1 : 0, true);
	}
};

//////////////////////////////////////////////////////////////////////////////////////
// FlowNode to set material on object in specified position  /////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////

class CFlowNode_SetObjectMaterial : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlowNode_SetObjectMaterial(SActivationInfo* pActInfo) :
		m_ppObjects(0),
		m_nObjType(0)
	{
		//STATIC_CHECK(eERType_TypesNum == 26, Sync_enum_in_GetConfiguration_ObjectType_port_with_new_entries_on_EERType);
	}

	~CFlowNode_SetObjectMaterial()
	{
		RestoreMaterials();
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_SetObjectMaterial(pActInfo); }

	virtual void         GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	enum EInputs
	{
		eIn_Material = 0,
		eIn_ObjectType,
		eIn_Position,
		eIn_Activate,
	};

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<string>("mat_Material", _HELP("Material name")),
			InputPortConfig<int>("ObjectType", 0, _HELP("Object type (render node type)"), NULL,
				_UICONFIG("enum_int:<choose>=0,Brush=1,Vegetation=2,Light=3,FogVolume=6,Decal=7,ParticleEmitter=8,WaterVolume=9,WaterWave=10,Road=11,DistanceCloud=12,Rope=15,MovableBrush=19,GameEffect=20,BreakableGlass=21,MergedMesh=23,GeomCache=24")),
			InputPortConfig<Vec3>("Position", _HELP("Position")),
			InputPortConfig_Void("Activate", _HELP("Activate set material event")),
			{ 0 }
		};

		static const SOutputPortConfig out_config[] = {
			{ 0 }
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.sDescription = _HELP("Set material to object (render node) at position");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				RestoreMaterials();
			}
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIn_Activate))
				{
					m_pos = GetPortVec3(pActInfo, eIn_Position);
					m_nObjType = GetPortInt(pActInfo, eIn_ObjectType);
					RestoreMaterials();
					SetMaterial(GetPortString(pActInfo, eIn_Material));
				}
			}
			break;
		}
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		if (ser.IsReading())
		{
			RestoreMaterials();
			string mtlName;
			ser.Value("mtlName", mtlName);
			if (!mtlName.empty())
			{
				SetMaterial(mtlName);
			}
		}
		else // saving
		{
			if (m_ppObjects && m_ppObjects[0] && m_ppObjects[0]->GetMaterial())
			{
				ser.Value("mtlName", m_ppObjects[0]->GetMaterial()->GetName());
			}
		}
	}

private:
	void SetMaterial(const string& mtlName)
	{
		_smart_ptr<IMaterial> pMtl = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(mtlName.c_str());
		if (pMtl && m_nObjType > 0)
		{
			AABB aabbPos(m_pos - Vec3(.1f, .1f, .1f), m_pos + Vec3(.1f, .1f, .1f));
			// Get count of render nodes first
			uint32 nCnt = gEnv->p3DEngine->GetObjectsByTypeInBox((EERType) m_nObjType, aabbPos);
			if (nCnt)
			{
				m_ppObjects = new IRenderNode*[nCnt];
				gEnv->p3DEngine->GetObjectsByTypeInBox((EERType) m_nObjType, aabbPos, m_ppObjects);
				for (uint32 i = 0; i < nCnt; ++i)
				{
					m_materials.push_back(m_ppObjects[i]->GetMaterial());
					m_ppObjects[i]->SetMaterial(pMtl);
				}
			}
		}
	}

	void RestoreMaterials()
	{
		if (m_ppObjects)
		{
			size_t size = m_materials.size();
			for (size_t i = 0; i < size; ++i)
			{
				m_ppObjects[i]->SetMaterial(m_materials[i]);
			}
			SAFE_DELETE_ARRAY(m_ppObjects)
			m_materials.clear();
		}
	}

private:
	IRenderNode**                      m_ppObjects;
	std::vector<_smart_ptr<IMaterial>> m_materials;
	Vec3                               m_pos;
	int                                m_nObjType;
};

//////////////////////////////////////////////////////////////////////////////////////
// FlowNode to precache area at specified position  ///////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////

class CFlowNode_PrecacheArea : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_PrecacheArea(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	enum EInputs
	{
		eIn_Position = 0,
		eIn_Timeout,
		eIn_Activate,
	};

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<Vec3>("Position", _HELP("Position")),
			InputPortConfig<float>("Timeout", 3.0f, _HELP("Time out in seconds")),
			InputPortConfig_Void("Activate",  _HELP("Activate precaching event")),
			{ 0 }
		};

		static const SOutputPortConfig out_config[] = {
			{ 0 }
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.sDescription = _HELP("Precache area at specified position");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIn_Activate))
				{
					gEnv->p3DEngine->OverrideCameraPrecachePoint(GetPortVec3(pActInfo, eIn_Position));
				}
			}
			break;
		}
	}
};

class CFlowNode_Viewport : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Viewport(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	enum EInputs
	{
		eIn_Get,
	};

	enum EOutputs
	{
		eOut_X,
		eOut_Y,
		eOut_Width,
		eOut_Height
	};

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Get", _HELP("Get current viewport information")),
			{ 0 }
		};

		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("x",      "Current top left x position of the viewport."),
			OutputPortConfig<int>("y",      "Current top left y position of the viewport."),
			OutputPortConfig<int>("width",  "Current width of the viewport."),
			OutputPortConfig<int>("height", "Current height of the viewport."),
			{ 0 }
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.sDescription = _HELP("Gets current rendering viewport information.");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIn_Get) && gEnv->pRenderer)
				{
					const int x = 0;
					const int y = 0;
					const int w = IRenderAuxGeom::GetAux()->GetCamera().GetViewSurfaceX();
					const int h = IRenderAuxGeom::GetAux()->GetCamera().GetViewSurfaceZ();

					ActivateOutput(pActInfo, eOut_X, x);
					ActivateOutput(pActInfo, eOut_Y, y);
					ActivateOutput(pActInfo, eOut_Width, w);
					ActivateOutput(pActInfo, eOut_Height, h);
				}
			}
			break;
		}
	}
};

class CFlowNode_ShadowCacheParams : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_ShadowCacheParams(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	enum EInputs
	{
		eIn_ShadowCachesCascade,
		eIn_ShadowCachesResolution,
		eIn_GsmLodsNum,
		eIn_GsmRange,
		eIn_GsmRangeStep,
		eIn_Apply
	};

	enum EOutputs
	{
		eOut_Done
	};

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<int>("ShadowCachesCascade", 4, _HELP("Sets what cascade we start using cached-shadows from (r_ShadowsCache)")),
			InputPortConfig<string>("ShadowCachesResolution", _HELP("Sets the shadow cache resolution per cascade (r_ShadowsCacheResolutions)")),
			InputPortConfig<int>("GsmLodsNum", 5, _HELP("Sets the number of Gsm Lods (minimum 1) (e_GsmLodsNum)")),
			InputPortConfig<float>("GsmRange", 10.f, _HELP("Sets the size of LOD 0 GSM area (in meters) (e_GsmRange)")),
			InputPortConfig<float>("GsmRangeStep", 1.f, _HELP("Sets the range of next GSM lod is previous range multiplied by step (minimum 1) (e_GsmRangeStep)")),
			InputPortConfig_AnyType("Apply", _HELP("Apply shadow cache parameters")),
			{ 0 }
		};

		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Done", _HELP("Changes were applied")),
			{ 0 }
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.sDescription = _HELP("Specify some shadow cache parameters");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIn_Apply))
				{
					if (ICVar* pShadowsCache = gEnv->pConsole->GetCVar("r_ShadowsCache"))
					{
						const int nGsmCache = GetPortInt(pActInfo, eIn_ShadowCachesCascade);
						pShadowsCache->Set(std::max(nGsmCache, 0));
					}

					if (ICVar* pSCR = gEnv->pConsole->GetCVar("r_ShadowsCacheResolutions"))
					{
						const string & resolution = GetPortString(pActInfo, eIn_ShadowCachesResolution);
						pSCR->Set(resolution.c_str());
					}

					if (ICVar* pLodsNum = gEnv->pConsole->GetCVar("e_GsmLodsNum"))
					{
						const int lods = GetPortInt(pActInfo, eIn_GsmLodsNum);
						pLodsNum->Set(std::max(lods, 1));
					}

					if (ICVar* pGsmRange = gEnv->pConsole->GetCVar("e_GsmRange"))
					{
						const float range = GetPortFloat(pActInfo, eIn_GsmRange);
						pGsmRange->Set(std::max(range, 0.f));
					}

					if (ICVar* pGsmRangeStep = gEnv->pConsole->GetCVar("e_GsmRangeStep"))
					{
						const float rangeStep = GetPortFloat(pActInfo, eIn_GsmRangeStep);
						pGsmRangeStep->Set(std::max(rangeStep, 1.f));
					}

					// Force one update of cached shadows whenever we modify the settings above
					ICVar* pShadowsCacheUpdate = gEnv->pConsole->GetCVar("e_ShadowsCacheUpdate");
					if (pShadowsCacheUpdate)
						pShadowsCacheUpdate->Set(1);

					ActivateOutput(pActInfo, eOut_Done, true);
				}
			}
			break;
		}
	}
};


//////////////////////////////////////////////////////////////////////////////////////

REGISTER_FLOW_NODE("Engine:PortalSwitch", CFlowNode_PortalSwitch);
REGISTER_FLOW_NODE("Engine:OcclusionAreaSwitch", CFlowNode_OcclusionAreaSwitch);
REGISTER_FLOW_NODE("Environment:OceanSwitch", CFlowNode_OceanSwitch);
REGISTER_FLOW_NODE("Environment:SkyboxSwitch", CFlowNode_SkyboxSwitch);
REGISTER_FLOW_NODE("Engine:LayerSwitch", CFlowNode_LayerSwitch);
REGISTER_FLOW_NODE("Material:SetObjectMaterial", CFlowNode_SetObjectMaterial);
REGISTER_FLOW_NODE("Engine:PrecacheArea", CFlowNode_PrecacheArea);
REGISTER_FLOW_NODE("Engine:Viewport", CFlowNode_Viewport);
REGISTER_FLOW_NODE("Engine:ShadowCacheParams", CFlowNode_ShadowCacheParams);
