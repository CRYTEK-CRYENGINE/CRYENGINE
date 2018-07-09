// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef ITEXTURESTREAMER_H
#define ITEXTURESTREAMER_H

struct STexStreamingInfo;
struct STexStreamPrepState;

typedef DynArray<CTexture*> TStreamerTextureVec;

class ITextureStreamer
{
public:
	enum EApplyScheduleFlags
	{
		eASF_InOut = 1,
		eASF_Prep  = 2,

		eASF_Full  = 3,
	};

public:
	ITextureStreamer();
	virtual ~ITextureStreamer() {}

public:
	virtual void  BeginUpdateSchedule();
	virtual void  ApplySchedule(EApplyScheduleFlags asf);

	virtual bool  BeginPrepare(CTexture* pTexture, const char* sFilename, uint32 nFlags) = 0;
	virtual void  EndPrepare(STexStreamPrepState*& pState) = 0;

	virtual void  Precache(CTexture* pTexture) = 0;
	virtual void  UpdateMip(CTexture* pTexture, const float fMipFactor, const int nFlags, const int nUpdateId, const int nCounter) = 0;

	virtual void  OnTextureDestroy(CTexture* pTexture) = 0;

	void          Relink(CTexture* pTexture);
	void          Unlink(CTexture* pTexture);

	virtual void  FlagOutOfMemory() = 0;
	virtual void  Flush() = 0;

	virtual bool  IsOverflowing() const = 0;
	virtual float GetBias() const { return 0.0f; }

	int           GetMinStreamableMip() const;
	int           GetMinStreamableMipWithSkip() const;

	void          StatsFetchTextures(std::vector<CTexture*>& out);
	bool          StatsWouldUnload(const CTexture* pTexture);

protected:
	void                 SyncTextureList();

	TStreamerTextureVec& GetTextures()
	{
		return m_textures;
	}

	CryCriticalSection& GetAccessLock() { return m_accessLock; }

private:
	size_t StatsComputeRequiredMipMemUsage();

private:
	TStreamerTextureVec m_pendingRelinks;
	TStreamerTextureVec m_pendingUnlinks;
	TStreamerTextureVec m_textures;
	
	// MT Locks access to m_pendingRelinks,m_pendingUnlinks,m_textures
	CryCriticalSection  m_accessLock;
};

#endif
