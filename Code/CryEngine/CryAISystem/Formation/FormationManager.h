// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Formation/AIFormationDescriptor.h"

class CFormation;
struct IAIObject;

class CFormationManager
{
public:
	typedef std::map<CWeakRef<CAIObject>, CFormation*> FormationMap;
	typedef std::map<string, CFormationDescriptor>     FormationDescriptorMap;

public:

	CFormationManager();
	~CFormationManager();

	void Reset();
	void Serialize(TSerialize ser);
	void DebugDraw() const;
	void GetMemoryStatistics(ICrySizer* pSizer) const;

	void OnAIObjectRemoved(IAIObject* pObject);
	const FormationMap& GetActiveFormations() const { return m_mapActiveFormations; }

	CFormation* CreateFormation(CWeakRef<CAIObject> refOwner, const char* szFormationName, Vec3 vTargetPos = ZERO);
	void ReleaseFormation(CWeakRef<CAIObject> refOwner, bool bDelete);
	CFormation* GetFormation(CFormation::TFormationID id) const;

	bool ChangeFormation(IAIObject* pOwner, const char* szFormationName, float fScale);
	bool ScaleFormation(IAIObject* pOwner, float fScale);
	bool SetFormationUpdate(IAIObject* pOwner, bool bUpdate);
	
	void FreeFormationPoint(CWeakRef<CAIObject> refOwner);	
	void ReleaseFormationPoint(CAIObject* pReserved);
	IAIObject* GetFormationPoint(IAIObject* pObject) const;
	int GetFormationPointClass(const char* descriptorName, int position) const;

	bool SameFormation(const CPuppet* pHuman, const CAIVehicle* pVehicle) const;

    string GetFormationNameFromCRC32(uint32 nCrc32ForFormationName) const;
	void CreateFormationDescriptor(const char* szName);
	void AddFormationPoint(const char* szName, const FormationNode& nodeDescriptor);
	bool IsFormationDescriptorExistent(const char* szName) const;

	void EnumerateFormationNames(unsigned int maxNames, const char** outNames, uint32* outNameCount) const;

private:
	FormationMap           m_mapActiveFormations;
	FormationDescriptorMap m_mapFormationDescriptors;
};