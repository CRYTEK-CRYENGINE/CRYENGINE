// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "WaterShapeObject.h"
#include "Material\MaterialManager.h"
#include "Objects/ObjectLoader.h"
#include "Objects/InspectorWidgetCreator.h"
#include <Cry3DEngine/I3DEngine.h>

REGISTER_CLASS_DESC(CWaterShapeObjectClassDesc);

IMPLEMENT_DYNCREATE(CWaterShapeObject, CShapeObject)

//////////////////////////////////////////////////////////////////////////
CWaterShapeObject::CWaterShapeObject()
{
	m_pWVRN = 0;

	m_waterVolumeID = -1;
	m_fogPlane = Plane(Vec3(0, 0, 1), 0);
}

//////////////////////////////////////////////////////////////////////////
bool CWaterShapeObject::Init(CBaseObject* prev, const string& file)
{
	bool res = CShapeObject::Init(prev, file);
	return res;
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::InitVariables()
{
	__super::InitVariables();

	mv_waterVolumeDepth = 10.0f;
	mv_waterStreamSpeed = 0.0f;
	mv_waterFogDensity = 0.5f;
	mv_waterFogColor = Vec3(0.005f, 0.01f, 0.02f);
	mv_waterFogColorMultiplier = 0.5f;
	mv_waterFogColorAffectedBySun = true;
	mv_waterFogShadowing = 0.5f;
	mv_waterSurfaceUScale = 1.0f;
	mv_waterSurfaceVScale = 1.0f;
	mv_waterCapFogAtVolumeDepth = false;
	mv_ratioViewDist = 100;
	mv_waterCaustics = true;
	mv_waterCausticIntensity = 1.0f;
	mv_waterCausticTiling = 1.0f;
	mv_waterCausticHeight = 0.5f;
	mv_waterVolume = 0.0f;
	mv_accVolume = 0.001f;
	mv_borderPad = 0.0f;
	mv_convexBorder = false;
	mv_objVolThresh = 0.001f;
	mv_waterCell = 0.0f;
	mv_waveSpeed = 140.0f;
	mv_waveDamping = 0.2f;
	mv_waveTimestep = 0.02f;
	mv_simAreaGrowth = 0.0f;
	mv_minVel = 0.01f;
	mv_simDepth = 8;
	mv_velResistance = 1;
	mv_hlimit = 7;

	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_pVarObject->AddVariable(mv_waterVolumeDepth, "VolumeDepth", "Depth", functor(*this, &CWaterShapeObject::OnWaterPhysicsParamChange));
	m_pVarObject->AddVariable(mv_waterStreamSpeed, "StreamSpeed", "Speed", functor(*this, &CWaterShapeObject::OnWaterPhysicsParamChange));
	m_pVarObject->AddVariable(mv_waterFogDensity, "FogDensity", "FogDensity", functor(*this, &CWaterShapeObject::OnWaterParamChange));
	m_pVarObject->AddVariable(mv_waterFogColor, "FogColor", "FogColor", functor(*this, &CWaterShapeObject::OnWaterParamChange), IVariable::DT_COLOR);
	m_pVarObject->AddVariable(mv_waterFogColorMultiplier, "FogColorMultiplier", "FogColorMultiplier", functor(*this, &CWaterShapeObject::OnWaterParamChange));
	m_pVarObject->AddVariable(mv_waterFogColorAffectedBySun, "FogColorAffectedBySun", "FogColorAffectedBySun", functor(*this, &CWaterShapeObject::OnWaterParamChange));
	m_pVarObject->AddVariable(mv_waterFogShadowing, "FogShadowing", "FogShadowing", functor(*this, &CWaterShapeObject::OnWaterParamChange));
	m_pVarObject->AddVariable(mv_waterCapFogAtVolumeDepth, "CapFogAtVolumeDepth", "CapFogAtVolumeDepth", functor(*this, &CWaterShapeObject::OnWaterParamChange));
	m_pVarObject->AddVariable(mv_waterSurfaceUScale, "UScale", "UScale", functor(*this, &CWaterShapeObject::OnParamChange));
	m_pVarObject->AddVariable(mv_waterSurfaceVScale, "VScale", "VScale", functor(*this, &CWaterShapeObject::OnParamChange));
	m_pVarObject->AddVariable(mv_ratioViewDist, "ViewDistRatio", "ViewDistRatio", functor(*this, &CWaterShapeObject::OnParamChange));
	m_pVarObject->AddVariable(mv_waterCaustics, "Caustics", "Caustics", functor(*this, &CWaterShapeObject::OnWaterParamChange));
	m_pVarObject->AddVariable(mv_waterCausticIntensity, "CausticIntensity", "CausticIntensity", functor(*this, &CWaterShapeObject::OnWaterParamChange));
	m_pVarObject->AddVariable(mv_waterCausticTiling, "CausticTiling", "CausticTiling", functor(*this, &CWaterShapeObject::OnWaterParamChange));
	m_pVarObject->AddVariable(mv_waterCausticHeight, "CausticHeight", "CausticHeight", functor(*this, &CWaterShapeObject::OnWaterParamChange));
	
	m_pVarObject->AddVariable(mv_GroupAdv, mv_waterVolume, "Volume", "FixedVolume", functor(*this, &CWaterShapeObject::OnWaterParamChange));
	m_pVarObject->AddVariable(mv_GroupAdv, mv_accVolume, "VolumeAcc", "VolumeAccuracy", functor(*this, &CWaterShapeObject::OnWaterParamChange));
	m_pVarObject->AddVariable(mv_GroupAdv, mv_borderPad, "BorderPad", "ExtrudeBorder", functor(*this, &CWaterShapeObject::OnWaterParamChange));
	m_pVarObject->AddVariable(mv_GroupAdv, mv_convexBorder, "BorderConvex", "ConvexBorder", functor(*this, &CWaterShapeObject::OnWaterParamChange));
	m_pVarObject->AddVariable(mv_GroupAdv, mv_objVolThresh, "ObjVolThresh", "ObjectSizeLimit", functor(*this, &CWaterShapeObject::OnWaterParamChange));
	m_pVarObject->AddVariable(mv_GroupAdv, mv_waterCell, "SimCell", "WaveSimCell", functor(*this, &CWaterShapeObject::OnWaterParamChange));
	m_pVarObject->AddVariable(mv_GroupAdv, mv_waveSpeed, "WaveSpeed", "WaveSpeed", functor(*this, &CWaterShapeObject::OnWaterParamChange));
	m_pVarObject->AddVariable(mv_GroupAdv, mv_waveDamping, "WaveDamping", "WaveDamping", functor(*this, &CWaterShapeObject::OnWaterParamChange));
	m_pVarObject->AddVariable(mv_GroupAdv, mv_waveTimestep, "WaveTimestep", "WaveTimestep", functor(*this, &CWaterShapeObject::OnWaterParamChange));
	m_pVarObject->AddVariable(mv_GroupAdv, mv_minVel, "MinWaveVel", "MinWaveVel", functor(*this, &CWaterShapeObject::OnWaterParamChange));
	m_pVarObject->AddVariable(mv_GroupAdv, mv_simDepth, "DepthCells", "DepthCells", functor(*this, &CWaterShapeObject::OnWaterParamChange));
	m_pVarObject->AddVariable(mv_GroupAdv, mv_hlimit, "HeightLimit", "HeightLimit", functor(*this, &CWaterShapeObject::OnWaterParamChange));
	m_pVarObject->AddVariable(mv_GroupAdv, mv_velResistance, "Resistance", "Resistance", functor(*this, &CWaterShapeObject::OnWaterParamChange));
	m_pVarObject->AddVariable(mv_GroupAdv, mv_simAreaGrowth, "SimAreaGrowth", "SimAreaGrowth", functor(*this, &CWaterShapeObject::OnWaterParamChange));
	m_pVarObject->AddVariable(mv_GroupAdv, "Advanced");

	mv_ratioViewDist.SetLimits(0, 255);
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CShapeObject::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CWaterShapeObject>("Water Volume", [](CWaterShapeObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_waterVolumeDepth, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_waterStreamSpeed, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_waterFogDensity, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_waterFogColor, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_waterFogColorMultiplier, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_waterFogColorAffectedBySun, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_waterFogShadowing, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_waterCapFogAtVolumeDepth, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_waterSurfaceUScale, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_waterSurfaceVScale, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_waterCaustics, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_waterCausticIntensity, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_waterCausticTiling, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_waterCausticHeight, ar);

		if (ar.openBlock("Advanced", "Advanced"))
		{
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_waterVolume, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_accVolume, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_borderPad, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_convexBorder, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_objVolThresh, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_waterCell, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_waveSpeed, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_waveDamping, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_waveTimestep, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_minVel, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_simDepth, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_hlimit, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_velResistance, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_simAreaGrowth, ar);
			ar.closeBlock();
		}
	});
}

//////////////////////////////////////////////////////////////////////////
bool CWaterShapeObject::CreateGameObject()
{
	m_pWVRN = static_cast<IWaterVolumeRenderNode*>(GetIEditorImpl()->Get3DEngine()->CreateRenderNode(eERType_WaterVolume));
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::PostLoad(CObjectArchive& ar)
{
	__super::PostLoad(ar);
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::SetName(const string& name)
{
	CShapeObject::SetName(name);
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::Done()
{
	LOADING_TIME_PROFILE_SECTION_ARGS(GetName().c_str());
	if (m_pWVRN)
		GetIEditorImpl()->Get3DEngine()->DeleteRenderNode(m_pWVRN);
	m_pWVRN = NULL;

	CShapeObject::Done();
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::OnParamChange(IVariable* var)
{
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
Vec3 CWaterShapeObject::GetPremulFogColor() const
{
	return Vec3(mv_waterFogColor) * max(float(mv_waterFogColorMultiplier), 0.0f);
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::OnWaterParamChange(IVariable* var)
{
	if (m_pWVRN)
	{
		m_pWVRN->SetFogDensity(mv_waterFogDensity);
		m_pWVRN->SetFogColor(GetPremulFogColor());
		m_pWVRN->SetFogColorAffectedBySun(mv_waterFogColorAffectedBySun);
		m_pWVRN->SetFogShadowing(mv_waterFogShadowing);
		m_pWVRN->SetCapFogAtVolumeDepth(mv_waterCapFogAtVolumeDepth);
		m_pWVRN->SetCaustics(mv_waterCaustics);
		m_pWVRN->SetCausticIntensity(mv_waterCausticIntensity);
		m_pWVRN->SetCausticTiling(mv_waterCausticTiling);
		m_pWVRN->SetCausticHeight(mv_waterCausticHeight);
		pe_params_area pa;
		pa.volume = mv_waterVolume;
		pa.volumeAccuracy = mv_accVolume;
		pa.borderPad = mv_borderPad;
		pa.objectVolumeThreshold = mv_objVolThresh;
		pa.cellSize = mv_waterCell;
		pa.growthReserve = mv_simAreaGrowth;
		pa.bConvexBorder = mv_convexBorder ? 1 : 0;
		pa.waveSim.waveSpeed = mv_waveSpeed;
		pa.waveSim.dampingCenter = mv_waveDamping;
		pa.waveSim.timeStep = mv_waveTimestep;
		pa.waveSim.heightLimit = mv_hlimit;
		pa.waveSim.simDepth = mv_simDepth;
		pa.waveSim.minVel = mv_minVel;
		pa.waveSim.resistance = mv_velResistance;
		m_pWVRN->SetAuxPhysParams(&pa);
	}
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::Physicalize()
{
	if (m_pWVRN)
	{
		const Matrix34& wtm = GetWorldTM();

		std::vector<Vec3> points(GetPointCount());
		for (int i = 0; i < GetPointCount(); i++)
			points[i] = wtm.TransformPoint(GetPoint(i));

		m_pWVRN->Dephysicalize();
		m_pWVRN->SetVolumeDepth(mv_waterVolumeDepth);
		m_pWVRN->SetStreamSpeed(mv_waterStreamSpeed);
		m_pWVRN->SetAreaPhysicsArea(&points[0], points.size(), true);
		m_pWVRN->Physicalize();
	}
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::OnWaterPhysicsParamChange(IVariable* var)
{
	Physicalize();
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::SetMaterial(IEditorMaterial* mtl)
{
	CShapeObject::SetMaterial(mtl);

	if (m_pWVRN != nullptr)
	{
		if (mtl != nullptr)
		{
			const SShaderItem& si = mtl->GetMatInfo()->GetShaderItem();
			if (si.m_pShader && si.m_pShader->GetShaderType() != eST_Water)
			{
				CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_ERROR, "Incorrect shader set for water / water fog volume \"%s\"!", GetName());
			}

			m_pWVRN->SetMaterial(mtl->GetMatInfo());
		}
		else
		{
			m_pWVRN->SetMaterial(nullptr);
		}

		UpdateGameArea();
	}
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::UpdateGameArea()
{
	if (m_bIgnoreGameUpdate)
		return;

	if (m_waterVolumeID == -1)
	{
		m_waterVolumeID = GetId().hipart;
		assert(m_waterVolumeID);
	}

	if (m_pWVRN)
	{
		m_pWVRN->SetMinSpec(GetMinSpec());
		m_pWVRN->SetMaterialLayers(GetMaterialLayersMask());
		m_pWVRN->SetViewDistRatio(mv_ratioViewDist);

		if (GetPointCount() > 3)
		{
			const Matrix34& wtm = GetWorldTM();

			std::vector<Vec3> points(GetPointCount());
			for (int i = 0; i < GetPointCount(); i++)
				points[i] = wtm.TransformPoint(GetPoint(i));

			m_fogPlane.SetPlane(wtm.GetColumn2().GetNormalized(), points[0]);

			m_pWVRN->SetFogDensity(mv_waterFogDensity);
			m_pWVRN->SetFogColor(GetPremulFogColor());
			m_pWVRN->SetFogColorAffectedBySun(mv_waterFogColorAffectedBySun);
			m_pWVRN->SetFogShadowing(mv_waterFogShadowing);
			m_pWVRN->SetCapFogAtVolumeDepth(mv_waterCapFogAtVolumeDepth);
			m_pWVRN->SetCaustics(mv_waterCaustics);
			m_pWVRN->SetCausticIntensity(mv_waterCausticIntensity);
			m_pWVRN->SetCausticTiling(mv_waterCausticTiling);
			m_pWVRN->SetCausticHeight(mv_waterCausticHeight);
			pe_params_area pa;
			pa.volume = mv_waterVolume;
			pa.volumeAccuracy = mv_accVolume;
			pa.borderPad = mv_borderPad;
			pa.bConvexBorder = mv_convexBorder ? 1 : 0;
			pa.objectVolumeThreshold = mv_objVolThresh;
			pa.cellSize = mv_waterCell;
			pa.waveSim.waveSpeed = mv_waveSpeed;
			pa.waveSim.dampingCenter = mv_waveDamping;
			pa.waveSim.timeStep = mv_waveTimestep;
			pa.waveSim.heightLimit = mv_hlimit;
			pa.waveSim.simDepth = mv_simDepth;
			pa.waveSim.minVel = mv_minVel;
			pa.waveSim.resistance = mv_velResistance;
			pa.growthReserve = mv_simAreaGrowth;
			m_pWVRN->SetAuxPhysParams(&pa);

			m_pWVRN->CreateArea(m_waterVolumeID, &points[0], points.size(), Vec2(mv_waterSurfaceUScale, mv_waterSurfaceVScale), m_fogPlane, true);

			Physicalize();

			if (GetMaterial())
				m_pWVRN->SetMaterial(GetMaterial()->GetMatInfo());
			else
				m_pWVRN->SetMaterial(0);

			unsigned int renderFlags = m_pWVRN->GetRndFlags();
			if (CheckFlags(OBJFLAG_INVISIBLE) || IsHiddenBySpec())
				renderFlags |= ERF_HIDDEN;
			else
				renderFlags &= ~ERF_HIDDEN;
			m_pWVRN->SetRndFlags(renderFlags);
		}
	}

	m_bAreaModified = false;
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::Serialize(CObjectArchive& ar)
{
	XmlNodeRef xmlNode(ar.node);
	if (ar.bLoading)
	{
		float waterVolumeDepth(10.0f);
		xmlNode->getAttr("VolumeDepth", waterVolumeDepth);
		mv_waterVolumeDepth = waterVolumeDepth;

		float waterStreamSpeed(0.0f);
		xmlNode->getAttr("StreamSpeed", waterStreamSpeed);
		mv_waterStreamSpeed = waterStreamSpeed;

		float waterFogDensity(0.1f);
		xmlNode->getAttr("FogDensity", waterFogDensity);
		mv_waterFogDensity = waterFogDensity;

		Vec3 waterFogColor(0.2f, 0.5f, 0.7f);
		xmlNode->getAttr("FogColor", waterFogColor);
		mv_waterFogColor = waterFogColor;

		float waterFogColorMultiplier(1.0f);
		xmlNode->getAttr("FogColorMultiplier", waterFogColorMultiplier);
		mv_waterFogColorMultiplier = waterFogColorMultiplier;

		bool waterFogColorAffectedBySun(true);
		xmlNode->getAttr("FogColorAffectedBySun", waterFogColorAffectedBySun);
		mv_waterFogColorAffectedBySun = waterFogColorAffectedBySun;

		float waterFogShadowing(0.5f);
		xmlNode->getAttr("FogShadowing", waterFogShadowing);
		mv_waterFogShadowing = waterFogShadowing;

		float waterSurfaceUScale(1.0f);
		xmlNode->getAttr("UScale", waterSurfaceUScale);
		mv_waterSurfaceUScale = waterSurfaceUScale;

		float waterSurfaceVScale(1.0f);
		xmlNode->getAttr("VScale", waterSurfaceVScale);
		mv_waterSurfaceVScale = waterSurfaceVScale;

		bool waterCapFogAtVolumeDepth(false);
		xmlNode->getAttr("CapFogAtVolumeDepth", waterCapFogAtVolumeDepth);
		mv_waterCapFogAtVolumeDepth = waterCapFogAtVolumeDepth;

		bool waterCaustics(true);
		xmlNode->getAttr("Caustics", waterCaustics);
		mv_waterCaustics = waterCaustics;

		float waterCausticIntensity(1.0f);
		xmlNode->getAttr("CausticIntensity", waterCausticIntensity);
		mv_waterCausticIntensity = waterCausticIntensity;

		float waterCausticTiling(1.0f);
		xmlNode->getAttr("CausticTiling", waterCausticTiling);
		mv_waterCausticIntensity = waterCausticTiling;

		float waterCausticHeight(0.5f);
		xmlNode->getAttr("CausticHeight", waterCausticHeight);
		mv_waterCausticHeight = waterCausticHeight;
	}
	else
	{
		xmlNode->setAttr("VolumeDepth", mv_waterVolumeDepth);
		xmlNode->setAttr("StreamSpeed", mv_waterStreamSpeed);
		xmlNode->setAttr("FogDensity", mv_waterFogDensity);
		xmlNode->setAttr("FogColor", mv_waterFogColor);
		xmlNode->setAttr("FogColorMultiplier", mv_waterFogColorMultiplier);
		xmlNode->setAttr("FogColorAffectedBySun", mv_waterFogColorAffectedBySun);
		xmlNode->setAttr("FogShadowing", mv_waterFogShadowing);
		xmlNode->setAttr("UScale", mv_waterSurfaceUScale);
		xmlNode->setAttr("VScale", mv_waterSurfaceVScale);
		xmlNode->setAttr("CapFogAtVolumeDepth", mv_waterCapFogAtVolumeDepth);
		xmlNode->setAttr("Caustics", mv_waterCaustics);
		xmlNode->setAttr("CausticIntensity", mv_waterCausticIntensity);
		xmlNode->setAttr("CausticTiling", mv_waterCausticTiling);
		xmlNode->setAttr("CausticHeight", mv_waterCausticHeight);
	}

	__super::Serialize(ar);
}

//////////////////////////////////////////////////////////////////////////

XmlNodeRef CWaterShapeObject::Export(const string& levelPath, XmlNodeRef& xmlNode)
{
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::SetMinSpec(uint32 nSpec, bool bSetChildren)
{
	__super::SetMinSpec(nSpec, bSetChildren);
	if (m_pWVRN)
		m_pWVRN->SetMinSpec(nSpec);
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::SetMaterialLayersMask(uint32 nLayersMask)
{
	__super::SetMaterialLayersMask(nLayersMask);
	if (m_pWVRN)
		m_pWVRN->SetMaterialLayers(nLayersMask);
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::SetHidden(bool bHidden)
{
	__super::SetHidden(bHidden);
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
void CWaterShapeObject::UpdateVisibility(bool visible)
{
	if (visible == CheckFlags(OBJFLAG_INVISIBLE) ||
	    m_pWVRN && (bool(m_pWVRN->GetRndFlags() & ERF_HIDDEN) == (visible && !IsHiddenBySpec())) // force update if spec changed
	    )
	{
		__super::UpdateVisibility(visible);
		UpdateGameArea();
	}
}

