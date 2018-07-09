// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Cover.h"

class CoverUser : public ICoverUser
{
public:
	CoverUser();
	CoverUser(const Params& params);

	virtual ~CoverUser();

	virtual void           Reset() override;

	virtual void           SetCoverID(const CoverID& coverID) override;
	virtual const CoverID& GetCoverID() const override;

	virtual void           SetNextCoverID(const CoverID& coverID) override;
	virtual const CoverID& GetNextCoverID() const override;

	virtual void           SetParams(const Params& params) override;
	virtual const Params&  GetParams() const override;

	virtual StateFlags     GetState() const override { return m_state; }
	virtual void           SetState(const StateFlags& state) override { m_state = state; }

	virtual bool           IsCompromised() const override;
	virtual float          CalculateEffectiveHeightAt(const Vec3& position, const CoverID& coverId) const override;
	virtual float          GetLocationEffectiveHeight() const override;
	virtual Vec3           GetCoverNormal(const Vec3& position) const override;     // normal pointing out of the cover surface
	virtual Vec3           GetCoverLocation(const CoverID& coverID) const override;

	virtual void SetCoverBlacklisted(const CoverID& coverID, bool blacklist, float time) override;
	virtual bool IsCoverBlackListed(const CoverID& coverId) const override;

	virtual void UpdateCoverEyes() override;
	virtual const DynArray<Vec3>& GetCoverEyes() const override { return m_eyes; }

	void Update(float timeDelta);

	void          DebugDraw() const;

private:
	void          UpdateBlacklisted(float timeDelta);
	bool          UpdateCompromised(const Vec3& coverPos, const Vec3& coverNormal, float minEffectiveCoverHeight = 0.001f);
	void          UpdateNormal(const Vec3& pos);

	void ResetState();
	bool IsInCover(const Vec3& pos, float radius, const Vec3* eyes, uint32 eyeCount) const;

	typedef std::unordered_map<CoverID, float, stl::hash_uint32> CoverBlacklistMap;
	typedef DynArray<Vec3> CoverEyes;

	CoverID           m_coverID;
	CoverID           m_nextCoverID;

	StateFlags        m_state;

	float             m_locationEffectiveHeight;
	float             m_distanceFromCoverLocationSqr;

	bool              m_compromised;
	Params            m_params;

	CoverEyes         m_eyes;
	CoverBlacklistMap m_coverBlacklistMap;
};
