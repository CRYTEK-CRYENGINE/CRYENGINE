// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a part for vehicles which is the an attachment
   of a parent Animated part

   -------------------------------------------------------------------------
   History:
   - 25:08:2005: Created by Mathieu Pinard

*************************************************************************/

#include "StdAfx.h"
#include <CryAnimation/ICryAnimation.h>
#include "IVehicleSystem.h"
#include "CryAction.h"
#include "Vehicle.h"
#include "VehiclePartBase.h"
#include "VehiclePartAnimated.h"
#include "VehiclePartSubPart.h"
#include "VehicleUtils.h"

//------------------------------------------------------------------------
CVehiclePartSubPart::CVehiclePartSubPart()
{
}

//------------------------------------------------------------------------
CVehiclePartSubPart::~CVehiclePartSubPart()
{
}

//------------------------------------------------------------------------
bool CVehiclePartSubPart::Init(IVehicle* pVehicle, const CVehicleParams& table, IVehiclePart* pParent, CVehicle::SPartInitInfo& initInfo, int partType)
{
	if (!CVehiclePartBase::Init(pVehicle, table, pParent, initInfo, eVPT_SubPart))
	{
		return false;
	}

	m_savedTM.SetIdentity();

	if (CVehicleParams subPartTable = table.findChild("SubPart"))
	{
		string fileName = subPartTable.getAttr("filename");
		const char* geoName = subPartTable.getAttr("geometryname");

		if (!fileName.empty())
		{
			string fileExtension;

			size_t pos = fileName.find_last_of("./\\");

			if ((pos != string::npos) && (fileName[pos] == '.'))
			{
				fileExtension = fileName.substr(pos);

				fileName.erase(pos);
			}

			IStatObj* pStatObjPrevState = NULL;

			for (EVehiclePartState state = eVGS_Default; state != eVGS_Last; state = static_cast<EVehiclePartState>(state + 1))
			{
				string stateFileName = fileName;

				stateFileName += CVehiclePartBase::GetDestroyedGeometrySuffix(state);
				stateFileName += fileExtension;

				IStatObj::SSubObject* pSubObject = NULL;

				IF_UNLIKELY (!gEnv->pCryPak->IsFileExist(stateFileName))
				{
					continue;
				}

				IStatObj* pStatObj = gEnv->p3DEngine->LoadStatObj(stateFileName, geoName, &pSubObject);

				// tell the streaming engine to stream destroyed version together with non destroyed
				if (pStatObjPrevState)
					pStatObjPrevState->SetStreamingDependencyFilePath(stateFileName);
				pStatObjPrevState = pStatObj;

				if (!pStatObj || pStatObj->IsDefaultObject())
				{
					RegisterStateGeom(state, NULL);
				}
				else
				{
					RegisterStateGeom(state, pStatObj);
				}
			}
		}
	}

	InitGeometry();

	return true;
}

//------------------------------------------------------------------------
void CVehiclePartSubPart::Reset()
{
	CVehiclePartBase::Reset();

	if (m_slot != -1)
	{
		GetEntity()->SetSlotLocalTM(m_slot, m_savedTM);
	}
}

//------------------------------------------------------------------------
void CVehiclePartSubPart::Release()
{
	if (m_slot != -1)
	{
		m_pVehicle->GetEntity()->FreeSlot(m_slot);
	}

	CVehiclePartBase::Release();
}

//------------------------------------------------------------------------
void CVehiclePartSubPart::OnEvent(const SVehiclePartEvent& event)
{
	CVehiclePartBase::OnEvent(event);
}

//------------------------------------------------------------------------
bool CVehiclePartSubPart::ChangeState(EVehiclePartState state, int flags)
{
	if (!CVehiclePartBase::ChangeState(state, flags))
	{
		return false;
	}

	CVehiclePartBase* pParentPartBase = (CVehiclePartBase*)m_pParentPart;

	if (!pParentPartBase)
	{
		return false;
	}

	IEntity* pEntity = GetEntity();

	bool swapped = false;

	for (TStateGeomVector::const_iterator iStateGeom = m_stateGeoms.begin(), iEnd = m_stateGeoms.end(); iStateGeom != iEnd; ++iStateGeom)
	{
		if (iStateGeom->state == state)
		{
			if (IStatObj* pStatObj = iStateGeom->pStatObj)
			{
				m_slot = pEntity->LoadGeometry(m_slot, pStatObj->GetFilePath(), pStatObj->GetGeoName());

				if (m_slot != -1)
				{
					pEntity->SetSlotMaterial(m_slot, pStatObj->GetMaterial());

					swapped = true;

					break;
				}
				else
				{
					CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CVehiclePartSubPart::ChangeState() : Failed to load geometry '%s'", pStatObj->GetFilePath());

					return false;
				}
			}
			else
			{
				swapped = true;

				break;
			}
		}
	}

	Matrix34 subTM(IDENTITY);

	if (IStatObj* pStatObj = pParentPartBase->GetSubGeometry(this, state, subTM, state == eVGS_Default))
	{
		if (!swapped)
		{
			m_slot = pEntity->LoadGeometry(m_slot, pStatObj->GetFilePath(), pStatObj->GetGeoName());

			if (m_slot != -1)
			{
				pEntity->SetSlotMaterial(m_slot, pStatObj->GetMaterial());

				swapped = true;
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CVehiclePartSubPart::ChangeState() : Failed to load geometry '%s'", pStatObj->GetFilePath());

				return false;
			}
		}
	}

	if (state == eVGS_Default)
	{
		Matrix34 localTM(IDENTITY);

		if (!m_pSharedParameters->m_position.IsZero())
		{
			localTM.SetTranslation(m_pSharedParameters->m_position);
		}
		else if (m_pSharedParameters->m_helperPosName.empty())
		{
			localTM = subTM;
		}
		else if (IVehicleHelper* pHelper = m_pVehicle->GetHelper(m_pSharedParameters->m_helperPosName.c_str()))
		{
			pHelper->GetVehicleTM(localTM);
		}

		pEntity->SetSlotLocalTM(m_slot, VALIDATE_MAT(localTM));

		m_savedTM = localTM;
	}

	if (m_slot > -1)
	{
		if (swapped && (m_hideCount == 0))
		{
			pEntity->SetSlotFlags(m_slot, GetEntity()->GetSlotFlags(m_slot) | ENTITY_SLOT_RENDER);
		}
		else
		{
			pEntity->SetSlotFlags(m_slot, GetEntity()->GetSlotFlags(m_slot) & ~(ENTITY_SLOT_RENDER | ENTITY_SLOT_RENDER_NEAREST));
		}
	}

	if (swapped && (flags & eVPSF_Physicalize))
	{
		Physicalize();
	}

	m_state = state;

	return true;
}

//------------------------------------------------------------------------
void CVehiclePartSubPart::Physicalize()
{
	if (m_slot != -1)
	{
		GetEntity()->UnphysicalizeSlot(m_slot);

		if (m_pSharedParameters->m_isPhysicalized)
		{
			SEntityPhysicalizeParams params;

			params.mass = m_pSharedParameters->m_mass;
			params.density = m_pSharedParameters->m_density;
			params.nSlot = m_slot;

			if (m_pSharedParameters->m_disableCollision)
			{
				params.nFlagsAND = ~(geom_collides | geom_floats);
			}

			m_physId = GetEntity()->PhysicalizeSlot(m_slot, params);

			if (GetEntity()->GetPhysics() && m_physId > -1)
			{
				const Matrix34& localTM = VALIDATE_MAT(GetEntity()->GetSlotLocalTM(m_slot, false));

				pe_params_part paramsPart;

				paramsPart.partid = m_physId;
				paramsPart.pos = localTM.GetTranslation();
				paramsPart.q = Quat(localTM);

				GetEntity()->GetPhysics()->SetParams(&paramsPart);

				CheckColltypeHeavy(m_physId);

				GetEntity()->SetSlotFlags(m_slot, GetEntity()->GetSlotFlags(m_slot) | ENTITY_SLOT_IGNORE_PHYSICS);
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehiclePartSubPart::Update(const float frameTime)
{
	CVehiclePartBase::Update(frameTime);
}

//------------------------------------------------------------------------
void CVehiclePartSubPart::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->Add(*this);

	GetMemoryUsageInternal(pSizer);
}

//------------------------------------------------------------------------
void CVehiclePartSubPart::GetMemoryUsageInternal(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));

	CVehiclePartBase::GetMemoryUsageInternal(pSizer);
}

//------------------------------------------------------------------------
Matrix34 CVehiclePartSubPart::GetLocalInitialTM()
{
	return m_savedTM;
}

//------------------------------------------------------------------------
void CVehiclePartSubPart::InitGeometry()
{
}

//------------------------------------------------------------------------
void CVehiclePartSubPart::RegisterStateGeom(EVehiclePartState state, IStatObj* pStatObj)
{
	m_stateGeoms.push_back(StateGeom(state, pStatObj));
}

DEFINE_VEHICLEOBJECT(CVehiclePartSubPart);
