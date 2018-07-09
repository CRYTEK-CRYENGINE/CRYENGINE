// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef physicalplaceholder_h
#define physicalplaceholder_h
#pragma once

#pragma pack(push)
#pragma pack(1)
// Packed and aligned structure ensures bitfield packs properly across
// 4 byte boundaries but stops the compiler implicitly doing byte copies
struct CRY_ALIGN(4) pe_gridthunk {
	uint64 inext : 20; // int64 for tighter packing
	uint64 iprev : 20;
	uint64 inextOwned : 20;
	uint64 iSimClass : 3;
	uint64 bFirstInCell : 1;
	unsigned char BBox[4];
	int BBoxZ0 : 16;
	int BBoxZ1 : 16;
	class CPhysicalPlaceholder *pent;
};
#pragma pack(pop)

class CPhysicalEntity;
const int NO_GRID_REG = -1<<14;
const int GRID_REG_PENDING = NO_GRID_REG+1;
const int GRID_REG_LAST = NO_GRID_REG+2;

class CPhysicalPlaceholder : public IPhysicalEntity {
public:
	CPhysicalPlaceholder()
		: m_pForeignData(nullptr)
		, m_iForeignData(0)
		, m_iForeignFlags(0)
		, m_iGThunk0(0)
		, m_pEntBuddy(nullptr)
		, m_bProcessed(0)
		, m_id(0)
		, m_bOBBThunks(0)
		, m_iSimClass(0)
		, m_lockUpdate(0)
	{ 
		static_assert(CRY_ARRAY_COUNT(m_BBox) == 2, "Invalid array size!");
		m_BBox[0].zero();
		m_BBox[1].zero();

		static_assert(CRY_ARRAY_COUNT(m_ig) == 2, "Invalid array size!");
		m_ig[0].x=m_ig[1].x=m_ig[0].y=m_ig[1].y = GRID_REG_PENDING;
	}

	virtual CPhysicalEntity *GetEntity();
	virtual CPhysicalEntity *GetEntityFast() { return (CPhysicalEntity*)m_pEntBuddy; }
	virtual bool IsPlaceholder() const { return true; };

	virtual int AddRef() { return 0; }
	virtual int Release() { return 0; }

	virtual pe_type GetType() const;
	virtual int SetParams(pe_params* params,int bThreadSafe=1);
	virtual int GetParams(pe_params* params) const;
	virtual int GetStatus(pe_status* status) const;
	virtual int Action(pe_action* action,int bThreadSafe=1);

	virtual int AddGeometry(phys_geometry *pgeom, pe_geomparams* params,int id=-1,int bThreadSafe=1);
	virtual void RemoveGeometry(int id,int bThreadSafe=1);

	virtual void *GetForeignData(int itype=0) const { return m_iForeignData==itype ? m_pForeignData : 0; }
	virtual int GetiForeignData() const { return m_iForeignData; }

	virtual int GetStateSnapshot(class CStream &stm, float time_back=0, int flags=0);
	virtual int GetStateSnapshot(TSerialize ser, float time_back=0, int flags=0);
	virtual int SetStateFromSnapshot(class CStream &stm, int flags=0);
	virtual int SetStateFromSnapshot(TSerialize ser, int flags=0);
	virtual int SetStateFromTypedSnapshot(TSerialize ser, int type, int flags=0);
	virtual int PostSetStateFromSnapshot();
	virtual int GetStateSnapshotTxt(char *txtbuf,int szbuf, float time_back=0);
	virtual void SetStateFromSnapshotTxt(const char *txtbuf,int szbuf);
	virtual unsigned int GetStateChecksum();
	virtual void SetNetworkAuthority(int authoritive, int paused);

	virtual void StartStep(float time_interval);
	virtual int Step(float time_interval);
	virtual int DoStep(float time_interval,int iCaller) { return 1; }
	virtual void StepBack(float time_interval);
	virtual IPhysicalWorld *GetWorld() const;

	virtual void GetMemoryStatistics(ICrySizer *pSizer) const {};

	Vec3 m_BBox[2];

	void *m_pForeignData;
	int m_iForeignData  : 16;
	int m_iForeignFlags : 16;

	struct vec2dpacked {
		int x : 16;
		int y : 16;
	};
	vec2dpacked m_ig[2];
	int m_iGThunk0;
#ifdef MULTI_GRID
	struct SEntityGrid *m_pGrid = nullptr;
#endif

	CPhysicalPlaceholder *m_pEntBuddy;
	volatile unsigned int m_bProcessed;
	int m_id : 23;
	int m_bOBBThunks : 1;
	int m_iSimClass : 8;
	mutable int m_lockUpdate;
};

#endif
