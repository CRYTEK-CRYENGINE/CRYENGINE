// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FeatureCommon.h"
#include <CrySerialization/Decorators/Resources.h>

namespace pfx2
{

MakeDataType(EPDT_MeshGeometry, IMeshObj*, EDataDomain(EDD_PerParticle | EDD_NeedsClear)); // Submesh pointers must be cleared on edit to avoid referencing freed parent mesh

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

	CFeatureRenderMeshes()
		: m_scale(1.0f, 1.0f, 1.0f)
		, m_sizeMode(ESizeMode::Scale)
		, m_originMode(EOriginMode::Origin)
		, m_piecesMode(EPiecesMode::RandomPiece)
		, m_piecePlacement(EPiecePlacement::Standard)
	{}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(Serialization::ModelFilename(m_meshName), "Mesh", "Mesh");
		ar(m_scale, "Scale", "Scale");
		ar(m_sizeMode, "SizeMode", "Size Mode");
		ar(m_originMode, "OriginMode", "Origin Mode");
		ar(m_piecesMode, "PiecesMode", "Pieces Mode");
		ar(m_piecePlacement, "PiecePlacement", "Piece Placement");
	}

	virtual EFeatureType GetFeatureType() override { return EFT_Render; }

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		if (!(m_pStaticObject = Get3DEngine()->FindStatObjectByFilename(m_meshName)))
		{
			GetPSystem()->CheckFileAccess(m_meshName);
			m_pStaticObject = Get3DEngine()->LoadStatObj(m_meshName, NULL, NULL, m_piecesMode == EPiecesMode::Whole);
		}
		pParams->m_pMesh = m_pStaticObject;
		pParams->m_meshCentered = m_originMode == EOriginMode::Center;
		if (m_pStaticObject)
		{
			pComponent->RenderDeferred.add(this);
			pComponent->AddParticleData(EPVF_Position);
			pComponent->AddParticleData(EPQF_Orientation);

			m_aSubObjects.clear();
			float maxRadiusSqr = 0.0f;
			if (m_piecesMode != EPiecesMode::Whole)
			{
				int subObjectCount = m_pStaticObject->GetSubObjectCount();
				for (int i = 0; i < subObjectCount; ++i)
				{
					if (IStatObj::SSubObject* pSub = m_pStaticObject->GetSubObject(i))
						if (pSub->nType == STATIC_SUB_OBJECT_MESH && pSub->pStatObj && pSub->pStatObj->GetRenderMesh())
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
				pComponent->InitParticles.add(this);
				pComponent->AddParticleData(EPDT_MeshGeometry);
				if (m_piecesMode == EPiecesMode::AllPieces)
				{
					pComponent->AddParticleData(EPDT_SpawnId);
					pParams->m_scaleParticleCount *= m_aSubObjects.size();
				}
			}
			else
			{
				maxRadiusSqr = MeshRadiusSqr(m_pStaticObject);
			}
			if (m_sizeMode == ESizeMode::Scale)
				pParams->m_physicalSizeSlope.scale *= sqrt(maxRadiusSqr);
		}
	}

	float MeshRadiusSqr(IMeshObj* pMesh) const
	{
		AABB bb = pMesh->GetAABB();
		return m_originMode == EOriginMode::Center ? 
			bb.GetRadiusSqr() :
			max(bb.min.GetLengthSquared(), bb.max.GetLengthSquared());
	}

	virtual void InitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		CParticleContainer& container = runtime.GetContainer();
		TIOStream<IMeshObj*> meshes = container.IOStream(EPDT_MeshGeometry);
		TIStream<uint> spawnIds = container.IStream(EPDT_SpawnId);
		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
		IOQuatStream orientations = container.GetIOQuatStream(EPQF_Orientation);
		IFStream sizes = container.GetIFStream(EPDT_Size, 1.0f);
		uint pieceCount = m_aSubObjects.size();
		Vec3 center = m_pStaticObject->GetAABB().GetCenter();

		for (auto particleId : runtime.SpawnedRange())
		{
			uint piece;
			if (m_piecesMode == EPiecesMode::RandomPiece)
			{
				piece = runtime.Chaos().Rand();
			}
			else if (m_piecesMode == EPiecesMode::AllPieces)
			{
				piece = spawnIds.Load(particleId);
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

		if (!runtime.IsCPURuntime())
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

		IMeshObj* pMeshObj = m_pStaticObject;
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
	Vec3                               m_scale;
	ESizeMode                          m_sizeMode;
	EOriginMode                        m_originMode;
	EPiecesMode                        m_piecesMode;
	EPiecePlacement                    m_piecePlacement;

	_smart_ptr<IStatObj>               m_pStaticObject;
	std::vector<IStatObj::SSubObject*> m_aSubObjects;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureRenderMeshes, "Render", "Meshes", colorRender);

}
