// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FeatureCommon.h"
#include <CrySerialization/Decorators/Resources.h>

namespace pfx2
{

MakeDataType(EPDT_MeshGeometry, IMeshObj*);

extern TDataType<float> EPDT_Alpha;
extern TDataType<UCol>  EPDT_Color;


SERIALIZATION_ENUM_DEFINE(ESizeMode, : uint8,
                          Size,
                          Scale
                          )

SERIALIZATION_ENUM_DEFINE(EOriginMode, : uint8,
                          Origin,
                          Center
                          )

SERIALIZATION_ENUM_DEFINE(EPiecesMode, : uint8,
                          Whole,
                          RandomPiece,
                          AllPieces
                          )

SERIALIZATION_ENUM_DEFINE(EPiecePlacement, : uint8,
                          Standard,
                          SubPlacement,
                          CenteredSubPlacement
                          )

class CFeatureRenderMeshes : public CParticleFeature, public Cry3DEngineBase
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(Serialization::ModelFilename(m_meshName), "Mesh", "Mesh");
		SERIALIZE_VAR(ar, m_scale);
		SERIALIZE_VAR(ar, m_sizeMode);
		SERIALIZE_VAR(ar, m_originMode);
		SERIALIZE_VAR(ar, m_piecesMode);
		SERIALIZE_VAR(ar, m_piecePlacement);
		SERIALIZE_VAR(ar, m_castShadows);
	}

	virtual EFeatureType GetFeatureType() override { return EFT_Render; }

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->LoadResources.add(this);
		LoadResources(*pComponent);
		if (pParams->m_pMesh)
		{
			pComponent->RenderDeferred.add(this);
			if (m_castShadows)
				pComponent->AddEnvironFlags(ENV_CAST_SHADOWS);
			pComponent->AddParticleData(EPVF_Position);
			pComponent->AddParticleData(EPQF_Orientation);
			if (m_aSubObjects.size() > 0)
			{
				// Require per-particle sub-objects
				assert(m_aSubObjects.size() < 256);
				pComponent->OnEdit.add(this);
				pComponent->InitParticles.add(this);
				pComponent->AddParticleData(EPDT_MeshGeometry);
			}
		}
	}

	virtual void LoadResources(CParticleComponent& component) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		SComponentParams& params = component.ComponentParams();
		if (!params.m_pMesh)
		{
			IStatObj* pMeshObj = nullptr;
			if (GetPSystem()->IsRuntime())
				pMeshObj = Get3DEngine()->FindStatObjectByFilename(m_meshName);
			if (!pMeshObj)
			{
				GetPSystem()->CheckFileAccess(m_meshName);
				pMeshObj = Get3DEngine()->LoadStatObj(m_meshName, NULL, NULL, m_piecesMode == EPiecesMode::Whole);
			}
			params.m_pMesh = pMeshObj;
			params.m_meshCentered = m_originMode == EOriginMode::Center;

			m_aSubObjects.clear();

			if (pMeshObj)
			{
				float maxRadiusSqr = 0.0f;
				if (m_piecesMode != EPiecesMode::Whole)
				{
					int subObjectCount = pMeshObj->GetSubObjectCount();
					for (int i = 0; i < subObjectCount; ++i)
					{
						if (IStatObj::SSubObject* pSub = pMeshObj->GetSubObject(i))
							if (pSub->nType == STATIC_SUB_OBJECT_MESH && pSub->pStatObj)
							{
								if (string(pSub->name).Right(5) == "_main")
									continue;
								m_aSubObjects.push_back(pSub);
								SetMax(maxRadiusSqr, MeshRadiusSqr(pSub->pStatObj));
							}
					}
				}

				if (m_aSubObjects.size() > 0)
				{
					// Require per-particle sub-objects
					assert(m_aSubObjects.size() < 256);
					component.OnEdit.add(this);
					component.InitParticles.add(this);
					component.AddParticleData(EPDT_MeshGeometry);
					if (m_piecesMode == EPiecesMode::AllPieces)
					{
						component.AddParticleData(EPDT_SpawnId);
						params.m_scaleParticleCount *= m_aSubObjects.size();
					}
				}
				else
				{
					maxRadiusSqr = MeshRadiusSqr(pMeshObj);
				}
				if (m_sizeMode == ESizeMode::Scale)
					params.m_scaleParticleSize *= sqrt(maxRadiusSqr);
			}
		}
		if (params.m_pMesh && GetCVars()->e_ParticlesPrecacheAssets)
		{
			m_pObjManager->PrecacheStatObj(static_cast<CStatObj*>(&*params.m_pMesh), 0,
				params.m_pMesh->GetMaterial(), 1.0f, 0.0f, true, true);
		}
	}

	float MeshRadiusSqr(IMeshObj* pMesh) const
	{
		AABB bb = pMesh->GetAABB();
		return m_originMode == EOriginMode::Center ? 
			bb.GetRadiusSqr() :
			max(bb.min.GetLengthSquared(), bb.max.GetLengthSquared());
	}

	virtual void OnEdit(CParticleComponentRuntime& runtime) override
	{
		// Submesh pointers must be cleared on edit to avoid referencing freed parent mesh
		runtime.GetContainer().FillData(EPDT_MeshGeometry, (IMeshObj*)nullptr, runtime.FullRange());
	}

	virtual void InitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		CParticleContainer& container = runtime.GetContainer();
		TIOStream<IMeshObj*> meshes = container.IOStream(EPDT_MeshGeometry);
		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
		IOQuatStream orientations = container.GetIOQuatStream(EPQF_Orientation);
		IFStream sizes = container.GetIFStream(EPDT_Size, 1.0f);
		const uint spawnIdOffset = container.GetSpawnIdOffset();
		const uint pieceCount = m_aSubObjects.size();
		Vec3 center = runtime.ComponentParams().m_pMesh->GetAABB().GetCenter();

		for (auto particleId : runtime.SpawnedRange())
		{
			uint piece;
			if (m_piecesMode == EPiecesMode::RandomPiece)
			{
				piece = runtime.Chaos().Rand();
			}
			else if (m_piecesMode == EPiecesMode::AllPieces)
			{
				piece = particleId + spawnIdOffset;
			}
			piece %= pieceCount;

			IMeshObj* mesh = m_aSubObjects[piece]->pStatObj;
			meshes.Store(particleId, mesh);

			if (m_piecePlacement != EPiecePlacement::Standard)
			{
				Vec3 position = positions.Load(particleId);
				Quat orientation = orientations.Load(particleId);
				const float size = sizes.Load(particleId);

				if (m_piecePlacement == EPiecePlacement::CenteredSubPlacement)
				{
					// Offset by main object center
					position -= orientation * center * size;
				}

				// Place pieces according to sub-transforms; scale is ignored
				Matrix34 const& localTM = m_aSubObjects[piece]->localTM;

				position += orientation * localTM.GetTranslation() * size;
				orientation = orientation * Quat(localTM);

				if (m_originMode == EOriginMode::Center)
				{
					Vec3 subCenter = m_aSubObjects[piece]->pStatObj->GetAABB().GetCenter();
					position += orientation * subCenter * size;
				}

				positions.Store(particleId, position);
				orientations.Store(particleId, orientation);
			}
		}
	}

	virtual void RenderDeferred(const CParticleComponentRuntime& runtime, const SRenderContext& renderContext) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		if (!runtime.IsCPURuntime() || !runtime.GetComponent()->IsVisible())
			return;
		auto& passInfo = renderContext.m_passInfo;
		SRendParams renderParams = renderContext.m_renderParams;

		const CParticleContainer& container = runtime.GetContainer();
		const IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
		const IQuatStream orientations = container.GetIQuatStream(EPQF_Orientation);
		const IFStream alphas = container.GetIFStream(EPDT_Alpha, 1.0f);
		const IFStream sizes = container.GetIFStream(EPDT_Size, 1.0f);
		const TIStream<IMeshObj*> meshes = container.IStream(EPDT_MeshGeometry);
		const Vec3 camPosition = passInfo.GetCamera().GetPosition();
		const bool hasAlphas = container.HasData(EPDT_Alpha);

		IMeshObj* pMeshObj = runtime.ComponentParams().m_pMesh;
		AABB bBox = pMeshObj->GetAABB();
		float sizeScale = m_sizeMode == ESizeMode::Size ? rsqrt_fast(MeshRadiusSqr(pMeshObj)) : 1.0f;

		renderParams.dwFObjFlags |= FOB_TRANS_MASK;

		for (auto particleId : runtime.FullRange())
		{
			if (m_aSubObjects.size())
			{
				pMeshObj = meshes.SafeLoad(particleId);
				if (!pMeshObj)
					continue;
				if (!pMeshObj->GetRenderMesh())
					continue;
				bBox = pMeshObj->GetAABB();
				sizeScale = m_sizeMode == ESizeMode::Size ? rsqrt_fast(MeshRadiusSqr(pMeshObj)) : 1.0f;
			}

			const Vec3 position = positions.Load(particleId);
			const Quat orientation = orientations.Load(particleId);
			float size = sizes.Load(particleId);

			const Vec3 scale = m_scale * size;
			Matrix34 wsMatrix(scale, orientation, position);
			if (m_originMode == EOriginMode::Center)
				wsMatrix.SetTranslation(wsMatrix * -bBox.GetCenter());

			renderParams.fAlpha = alphas.SafeLoad(particleId);

			// Trigger transparency rendering if any particles can be transparent.
			if (!passInfo.IsShadowPass() && hasAlphas)
				renderParams.fAlpha *= 0.999f;

			renderParams.fDistance = camPosition.GetDistance(position);
			renderParams.pMatrix = &wsMatrix;
			renderParams.pInstance = &non_const(*this);
			pMeshObj->Render(renderParams, passInfo);
		}
	}

	virtual uint GetNumResources() const override
	{
		return m_meshName.empty() ? 0 : 1;
	}

	virtual const char* GetResourceName(uint resourceId) const override
	{
		return m_meshName.c_str();
	}

private:
	string                             m_meshName;
	Vec3                               m_scale          {1};
	ESizeMode                          m_sizeMode       = ESizeMode::Scale;
	EOriginMode                        m_originMode     = EOriginMode::Origin;
	EPiecesMode                        m_piecesMode     = EPiecesMode::RandomPiece;
	EPiecePlacement                    m_piecePlacement = EPiecePlacement::Standard;
	bool                               m_castShadows    = true;

	std::vector<IStatObj::SSubObject*> m_aSubObjects;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureRenderMeshes, "Render", "Meshes", colorRender);

}
