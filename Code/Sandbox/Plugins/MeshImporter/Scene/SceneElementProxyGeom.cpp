// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SceneElementProxyGeom.h"
#include "SceneElementTypes.h"

#include <Cry3DEngine/I3DEngine.h>
#include <CryPhysics/physinterface.h>

#include <CrySerialization/Enum.h>
#include <CrySerialization/Math.h>

#include <CrySerialization/yasli/decorators/Range.h>

enum EPhysProxyType
{
	Default   = PHYS_GEOM_TYPE_DEFAULT,
	Obstruct  = PHYS_GEOM_TYPE_OBSTRUCT,
	NoCollide = PHYS_GEOM_TYPE_NO_COLLIDE,
};

SERIALIZATION_ENUM_BEGIN(EPhysProxyType, "ProxyType")
SERIALIZATION_ENUM(Default, "default", "Solid")
SERIALIZATION_ENUM(Obstruct, "obstr", "Obstruct")
SERIALIZATION_ENUM(NoCollide, "nocoll", "NoCollide")
SERIALIZATION_ENUM_END()

CSceneElementProxyGeom::CSceneElementProxyGeom(CScene* pScene, int id)
	: CSceneElementCommon(pScene, id)
	, m_pPhysGeom(nullptr)
{
}

void CSceneElementProxyGeom::SetPhysGeom(phys_geometry* pPhysGeom)
{
	m_pPhysGeom = pPhysGeom;

	if (m_pPhysGeom)
	{
		int type = m_pPhysGeom->pGeom->GetType();
		string name;
		switch(type)
		{
		case GEOM_BOX:
			name = "Box";
			break;
		case GEOM_CYLINDER:
			name = "Cylinder";
			break;
		case GEOM_CAPSULE:
			name = "Capsule";
			break;
		case GEOM_TRIMESH:
			name = "Mesh";
			break;
		default:
			name = "???";
		}
		SetName(name);
	}
}

phys_geometry* CSceneElementProxyGeom::GetPhysGeom()
{
	return m_pPhysGeom;
}

void CSceneElementProxyGeom::Serialize(Serialization::IArchive& ar)
{
	struct SPhysGeom
	{
		IGeometry* pGeom;
	};
	struct SBoxGeom : SPhysGeom
	{
		SBoxGeom(IGeometry* p) { pGeom = p; }
		void Serialize(Serialization::IArchive& ar)
		{
			primitives::box box = *(const primitives::box*)pGeom->GetData();
			ar(box.center, "center", "Center");
			Quat q = !Quat(box.Basis);
			ar(Serialization::AsAng3(q), "rot", "Rotation");
			ar(box.size, "size", "Half-size");
			if (ar.isInput())
			{
				box.Basis = Matrix33(!q);
				pGeom->SetData(&box);
			}
		}
	};
	struct SCylinderGeom : SPhysGeom
	{
		SCylinderGeom(IGeometry* p) { pGeom = p; }
		void Serialize(Serialization::IArchive& ar)
		{
			primitives::cylinder cyl = *(const primitives::cylinder*)pGeom->GetData();
			ar(cyl.center, "center", "Center");
			float phi = RAD2DEG(atan2(cyl.axis.y, cyl.axis.x)), theta = RAD2DEG(acos(cyl.axis.z));
			ar(yasli::Range(phi, -180.0f, 180.0f), "phi", "Phi(xy rotation)");
			ar(yasli::Range(theta, 0.0f, 180.0f), "theta", "Theta(z rotation)");
			ar(cyl.r, "r", "Radius");
			ar(cyl.hh, "hh", "Half-height");
			if (ar.isInput())
			{
				cyl.axis = Quat(Ang3(0, DEG2RAD(theta), DEG2RAD(phi))) * Vec3(0, 0, 1);
				pGeom->SetData(&cyl);
			}
		}
	};
	struct SMeshGeom : SPhysGeom
	{
		SMeshGeom(IGeometry* p) { pGeom = p; }
		void Serialize(Serialization::IArchive& ar)
		{
			const mesh_data* md = (const mesh_data*)pGeom->GetData();
			ar(md->nVertices, "nvtx", "!Vertex count");
			ar(md->nTris, "ntris", "!Triangle count");
		}
	};

	ar(m_pPhysGeom->surface_idx, "matid", "Submaterial index");
	ar((EPhysProxyType&)m_pPhysGeom->nMats, "type", "Proxy Type");
	IGeometry* const pGeom = m_pPhysGeom->pGeom;
	switch (pGeom->GetType())
	{
	case GEOM_BOX:
		ar(SBoxGeom(pGeom), "box", "Box");
		break;
	case GEOM_CAPSULE:
	case GEOM_CYLINDER:
		ar(SCylinderGeom(pGeom), "cyl", pGeom->GetType() == GEOM_CAPSULE ? "Capsule" : "Cylinder");
		break;
	case GEOM_TRIMESH:
		ar(SMeshGeom(pGeom), "mesh", "Mesh");
		if (ar.isInput())
		{
			mesh_data* pmd = (mesh_data*)pGeom->GetData();
			memset(pmd->pMats, m_pPhysGeom->surface_idx, pmd->pMats ? pmd->nTris : 0);
		}
		break;
	}
}

ESceneElementType CSceneElementProxyGeom::GetType() const
{
	return ESceneElementType::ProxyGeom;
}

