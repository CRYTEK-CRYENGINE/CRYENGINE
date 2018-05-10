// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef RESOURCE_COMPILER
struct GlobalAnimationHeader
{
	GlobalAnimationHeader()
		: m_nFlags(0)
		, m_FilePath()
		, m_FilePathCRC32(0)
	{}

	ILINE uint32      IsAimpose() const           { return m_nFlags & CA_AIMPOSE; }
	ILINE void        OnAimpose()                 { m_nFlags |= CA_AIMPOSE; }

	ILINE uint32      IsAimposeUnloaded() const   { return m_nFlags & CA_AIMPOSE_UNLOADED; }
	ILINE void        OnAimposeUnloaded()         { m_nFlags |= CA_AIMPOSE_UNLOADED; }

	ILINE uint32      IsAssetCreated() const      { return m_nFlags & CA_ASSET_CREATED; }
	ILINE void        OnAssetCreated()            { m_nFlags |= CA_ASSET_CREATED; }
	ILINE void        InvalidateAssetCreated()    { m_nFlags &= (CA_ASSET_CREATED ^ -1); }

	ILINE uint32      IsAssetAdditive() const     { return m_nFlags & CA_ASSET_ADDITIVE; }
	ILINE void        OnAssetAdditive()           { m_nFlags |= CA_ASSET_ADDITIVE; }

	ILINE uint32      IsAssetCycle() const        { return m_nFlags & CA_ASSET_CYCLE; }
	ILINE void        OnAssetCycle()              { m_nFlags |= CA_ASSET_CYCLE; }

	ILINE uint32      IsAssetLMG() const          { return m_nFlags & CA_ASSET_LMG; }
	ILINE void        OnAssetLMG()                { m_nFlags |= CA_ASSET_LMG; }

	ILINE uint32      IsAssetLMGValid() const     { return m_nFlags & CA_ASSET_LMG_VALID; }
	ILINE void        OnAssetLMGValid()           { m_nFlags |= CA_ASSET_LMG_VALID; }
	ILINE void        InvalidateAssetLMG()        { m_nFlags &= (CA_ASSET_LMG_VALID ^ -1); }

	ILINE uint32      IsAssetRequested() const    { return m_nFlags & CA_ASSET_REQUESTED; }
	ILINE void        OnAssetRequested()          { m_nFlags |= CA_ASSET_REQUESTED; }
	ILINE void        ClearAssetRequested()       { m_nFlags &= ~CA_ASSET_REQUESTED; }

	ILINE uint32      IsAssetNotFound() const     { return m_nFlags & CA_ASSET_NOT_FOUND; }
	ILINE void        OnAssetNotFound()           { m_nFlags |= CA_ASSET_NOT_FOUND; }
	ILINE void        ClearAssetNotFound()        { m_nFlags &= ~CA_ASSET_NOT_FOUND; }

	ILINE uint32      IsAssetTCB() const          { return m_nFlags & CA_ASSET_TCB; }

	ILINE uint32      IsAssetInternalType() const { return m_nFlags & CA_ASSET_INTERNALTYPE; }
	ILINE void        OnAssetInternalType()       { m_nFlags |= CA_ASSET_INTERNALTYPE; }

	ILINE uint32      GetFlags() const            { return m_nFlags; }
	ILINE void        SetFlags(uint32 nFlags)     { m_nFlags = nFlags; }

	ILINE const char* GetFilePath() const         { return m_FilePath.c_str(); }
	ILINE void        SetFilePath(const string& name);

	ILINE int         GetFilePathCRC32() const { return m_FilePathCRC32; }

protected:

	uint32 m_nFlags;
	string m_FilePath;        //low-case path-name - unique per animation asset
	uint32 m_FilePathCRC32;   //hash value for searching animation-paths (lower-case)
};

inline void GlobalAnimationHeader::SetFilePath(const string& name)
{
	m_FilePath = name;
	m_FilePathCRC32 = CCrc32::ComputeLowercase(name.c_str());
}
#endif