// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef STATOSCOPETEXTURESTREAMINGINTERVALGROUP_H
#define STATOSCOPETEXTURESTREAMINGINTERVALGROUP_H

#include "Statoscope.h"

#if ENABLE_STATOSCOPE

class CStatoscopeTextureStreamingIntervalGroup : public CStatoscopeIntervalGroup, public ITextureStreamListener
{
public:
	CStatoscopeTextureStreamingIntervalGroup();

	void Enable_Impl();
	void Disable_Impl();

public: // ITextureStreamListener Members
	virtual void OnCreatedStreamedTexture(void* pHandle, const char* name, int nMips, int nMinMipAvailable);
	virtual void OnUploadedStreamedTexture(void* pHandle);
	virtual void OnDestroyedStreamedTexture(void* pHandle);
	virtual void OnTextureWantsMip(void* pHandle, int nMinMip);
	virtual void OnTextureHasMip(void* pHandle, int nMinMip);
	virtual void OnBegunUsingTextures(void** pHandles, size_t numHandles);
	virtual void OnEndedUsingTextures(void** pHandle, size_t numHandles);

private:
	void OnChangedTextureUse(void** pHandles, size_t numHandles, int inUse);
	void OnChangedMip(void* pHandle, int field, int mip);
};

#endif

#endif
