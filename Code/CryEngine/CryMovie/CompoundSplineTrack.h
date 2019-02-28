// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#define MAX_SUBTRACKS 4

class CCompoundSplineTrack : public IAnimTrack
{
public:
	CCompoundSplineTrack(const CAnimParamType& paramType, int nDims, EAnimValue inValueType, CAnimParamType subTrackParamTypes[MAX_SUBTRACKS]);

	virtual int                         GetSubTrackCount() const override { return m_nDimensions; }
	virtual IAnimTrack*                 GetSubTrack(int nIndex) const override;
	virtual const char*                 GetSubTrackName(int nIndex) const override;
	virtual void                        SetSubTrackName(int nIndex, const char* name) override;

	virtual EAnimValue                  GetValueType() override           { return m_valueType; }

	virtual CAnimParamType              GetParameterType() const override { return m_paramType; }

	virtual int                         GetNumKeys() const override;
	virtual bool                        HasKeys() const override;
	virtual void                        RemoveKey(int num) override;
	virtual void                        ClearKeys() override;

	virtual int                         CreateKey(SAnimTime time) override               { assert(0); return 0; };
	virtual const char*                 GetKeyType() const override                      { return STrackKey::GetType(); }
	virtual bool                        KeysDeriveTrackDurationKey() const override      { return false; }
	virtual _smart_ptr<IAnimKeyWrapper> GetWrappedKey(int key) override;
	virtual void                        GetKey(int index, STrackKey* key) const override { assert(0); };
	virtual SAnimTime                   GetKeyTime(int index) const override;
	virtual int                         FindKey(SAnimTime time) override                 { assert(0); return 0; };
	virtual void                        SetKey(int index, const STrackKey* key) override { assert(0); };

	virtual int                         GetFlags() override                              { return m_flags; };
	virtual bool                        IsMasked(const uint32 mask) const override       { return false; }
	virtual void                        SetFlags(int flags) override                     { m_flags = flags; };

	// Get track value at specified time. Interpolates keys if needed.
	virtual TMovieSystemValue GetValue(SAnimTime time) const override;

	// Get track default value
	virtual TMovieSystemValue GetDefaultValue() const override;

	// Set track default value
	virtual void SetDefaultValue(const TMovieSystemValue& value) override;

	virtual void SetTimeRange(TRange<SAnimTime> timeRange) override;

	virtual void Serialize(Serialization::IArchive& ar) override {}

	virtual bool Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks = true) override;

    virtual bool SerializeKeys(XmlNodeRef& xmlNode, bool bLoading, std::vector<SAnimTime>& keys, const SAnimTime time = SAnimTime(0)) override { return false; };

	virtual int  NextKeyByTime(int key) const override;

	void         SetSubTrackName(const int i, const string& name)          { assert(i < MAX_SUBTRACKS); m_subTrackNames[i] = name; }

	virtual void GetKeyValueRange(float& fMin, float& fMax) const override { if (GetSubTrackCount() > 0) { m_subTracks[0]->GetKeyValueRange(fMin, fMax); } }
	virtual void SetKeyValueRange(float fMin, float fMax) override
	{
		for (int i = 0; i < m_nDimensions; ++i)
		{
			m_subTracks[i]->SetKeyValueRange(fMin, fMax);
		}
	};

	virtual CryGUID GetGUID() const override { return m_guid; }

protected:
	CryGUID                m_guid;
	EAnimValue             m_valueType;
	int                    m_nDimensions;
	_smart_ptr<IAnimTrack> m_subTracks[MAX_SUBTRACKS];
	int                    m_flags;
	CAnimParamType         m_paramType;
	string                 m_subTrackNames[MAX_SUBTRACKS];

	void  PrepareNodeForSubTrackSerialization(XmlNodeRef& subTrackNode, XmlNodeRef& xmlNode, int i, bool bLoading);
	float PreferShortestRotPath(float degree, float degree0) const;
	int   GetSubTrackIndex(int& key) const;
};