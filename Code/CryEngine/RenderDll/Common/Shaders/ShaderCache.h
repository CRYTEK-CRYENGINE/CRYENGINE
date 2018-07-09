// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   ShaderCache.h : Shaders cache related declarations.

   Revision history:
* Created by Honich Andrey

   =============================================================================*/

#ifndef __SHADERCACHE_H__
#define __SHADERCACHE_H__

#include "Shader.h"

struct SPreprocessMasks
{
	uint64 nRT, nRTSet;
	uint64 nGL, nGLSet;
	uint32 nLT, nLTSet;
	uint32 nMD, nMDSet;
	uint32 nMDV, nMDVSet;
};

typedef std::map<uint32, uint32>     MapPreprocessFlags;
typedef MapPreprocessFlags::iterator MapPreprocessFlagsItor;

struct SPreprocessNode
{
	TArray<uint32>                m_Expression;
	int                           m_nNode;
	uint64                        m_RTMask;
	uint64                        m_GLMask;
	uint32                        m_LTMask;
	uint32                        m_MDMask;
	uint32                        m_MDVMask;
	std::vector<SPreprocessNode*> m_Nodes[2];
	short                         m_nCode[2]; // 0 - No Code, 1 - Has Code, -1 - Depends
	SPreprocessNode()
	{
		m_nNode = 0;
		m_RTMask = 0;
		m_GLMask = 0;
		m_LTMask = 0;
		m_MDMask = 0;
		m_MDVMask = 0;
		m_nCode[0] = m_nCode[1] = 0;
	}
	~SPreprocessNode()
	{
		uint32 i;
		for (i = 0; i < m_Nodes[0].size(); i++)
		{
			SAFE_DELETE(m_Nodes[0][i]);
		}
		for (i = 0; i < m_Nodes[1].size(); i++)
		{
			SAFE_DELETE(m_Nodes[1][i]);
		}
	}
};

struct SPreprocessFlagDesc
{
	CCryNameR m_Name;
	uint64    m_nFlag;
	bool      m_bWasReferenced;
	SPreprocessFlagDesc() { m_nFlag = 0; m_bWasReferenced = true; }
};

struct SPreprocessTree
{
	static std::vector<SPreprocessFlagDesc> s_GLDescs;
	static std::vector<SPreprocessFlagDesc> s_RTDescs;
	static std::vector<SPreprocessFlagDesc> s_LTDescs;
	std::vector<SPreprocessFlagDesc>        m_GLDescsLocal;
	std::vector<SPreprocessFlagDesc>        m_MDDescs;
	std::vector<SPreprocessFlagDesc>        m_MDVDescs;

	static MapPreprocessFlags               s_MapFlags;
	MapPreprocessFlags                      m_MapFlagsLocal;

	std::vector<SPreprocessNode*>           m_Root;
	~SPreprocessTree()
	{
		uint32 i;
		for (i = 0; i < m_Root.size(); i++)
		{
			SAFE_DELETE(m_Root[i]);
		}
		m_Root.clear();
	}
	CShader* m_pSH;
};

//=======================================================================================================

union UPipelineState // Pipeline state relevant for shader instantiation
{
	union
	{
		struct
		{
			// GNM VS: Value must be 1TT00XXXb, where X determines the hardware stage to compile for (0: VS, 1: LS, 2: ES (ring), 3: ES (LDS), 4: CS/VS (DD), 5: CS/VS (DDI)). TT is the ISA selector.
			uint8 targetStage;
		} VS;
		struct
		{
			// GNM HS: Value must be 1TT00000b. TT is the ISA selector.
			uint8 targetStage;
		} HS;
		struct
		{
			// GNM DS: Value must be 1TT0000Xb, where X determines the hardware stage to compile for (0: VS, 1: ES). TT is the ISA selector.
			uint8 targetStage;
		} DS;
		struct
		{
			// GNM GS: Value must be 1TT0000Xb, where X determines the hardware stage to compile for (0: GS (ring), 1: GS (LDS)). TT is the ISA selector
			uint8 targetStage;
		} GS;
		struct
		{
			// GNM PS: depthStencilInfo value must be 1TT00..00DDSb, for D (0: not present, 1: 16-bit unorm, 2: 32-bit float), and for S (0: not present, 1: 8-bit uint). TT is the ISA selector.
			// targetFormats contains sce::Gnm::PsTargetOutputMode value (4-bits for each MRT), with MRT0 in the LSBs and MRT7 in the MSBs.
			uint32 targetFormats;
			uint32 depthStencilInfo;
		} PS;
		struct
		{
			// GNM CS: Value must be 1TT000XX, where X determines the hardware stage to compile for (0: CS, 1: CS (DD), 2: CS (DD instanced))
			uint8 targetStage;
		} CS;

		uint8 GetISA(EHWShaderClass type) const // Gets the HwISA field (value [0, 3]) given the type of shader
		{
			return (type == eHWSC_Pixel ? PS.depthStencilInfo >> 29 : VS.targetStage >> 5) & 0x3U;
		}

		void SetISA(EHWShaderClass type, uint8 isa) // Sets the HwISA field (value [0, 3]) given the type of shader
		{
			if (type == eHWSC_Pixel)
			{
				PS.depthStencilInfo = static_cast<uint32>(isa << 29) | (PS.depthStencilInfo & ~(3U << 29));
			}
			else
			{
				VS.targetStage = static_cast<uint32>(isa << 5) | (VS.targetStage & ~(3U << 5));
			}
		}
	} GNM;

	struct
	{
		uint64 resourceLayoutHash;
	} VULKAN;

	uint64 opaque;

	UPipelineState(uint64 opaqueValue = 0) : opaque(opaqueValue)
	{
	}

};
static_assert(sizeof(UPipelineState) == sizeof(uint64), "UPipelineState needs to be 64 bit");

struct SShaderCombIdent
{
	uint64         m_RTMask; // run-time mask
	uint64         m_GLMask; // global mask
	UPipelineState m_pipelineState;
	union
	{
		struct
		{
			uint32 m_LightMask;   // light mask
			uint32 m_MDMask;      // texture coordinates modifier mask
			uint32 m_MDVMask;     // vertex modifier mask | SF_PLATFORM
			uint32 m_nHash;
		};
		struct
		{
			uint64 m_FastCompare1;
			uint64 m_FastCompare2;
		};
	};

	SShaderCombIdent(uint64 nRT, uint64 nGL, uint32 nLT, uint32 nMD, uint32 nMDV)
	{
		m_RTMask = nRT;
		m_GLMask = nGL;
		m_LightMask = nLT;
		m_MDMask = nMD;
		m_MDVMask = nMDV;
		m_nHash = 0;
	}
	SShaderCombIdent(uint64 nGL, const SShaderCombIdent& Ident)
	{
		*this = Ident;
		m_GLMask = nGL;
	}

	SShaderCombIdent()
	{
		m_RTMask = 0;
		m_GLMask = 0;
		m_LightMask = 0;
		m_MDMask = 0;
		m_MDVMask = 0;
		m_nHash = 0;
	}
	bool   operator==(const SShaderCombIdent& other) const;

	uint32 PostCreate();
};

//==========================================================================================================================

class CResStreamDirCallback : public IStreamCallback
{
	virtual void StreamOnComplete(IReadStream* pStream, unsigned nError);
	virtual void StreamAsyncOnComplete(IReadStream* pStream, unsigned nError);
};
class CResStreamCallback : public IStreamCallback
{
	virtual void StreamOnComplete(IReadStream* pStream, unsigned nError);
	virtual void StreamAsyncOnComplete(IReadStream* pStream, unsigned nError);
};

struct SResStreamEntry
{
	struct SResStreamInfo* m_pParent;
	CCryNameTSCRC          m_Name;
	IReadStreamPtr         m_readStream;
};

struct SResStreamInfo
{
	CResFile*                     m_pRes;
	CResStreamCallback            m_Callback;
	CResStreamDirCallback         m_CallbackDir;

	CryCriticalSection            m_StreamLock;
	std::vector<SResStreamEntry*> m_EntriesQueue;

	std::vector<IReadStreamPtr>   m_dirReadStreams;
	int                           m_nDirRequestCount;

	SResStreamInfo()
	{
		m_pRes = NULL;
		m_nDirRequestCount = 0;
	}
	~SResStreamInfo()
	{
		assert(m_EntriesQueue.empty() && m_dirReadStreams.empty());
	}
	void AbortJobs()
	{
		using std::swap;

		CryAutoLock<CryCriticalSection> lock(m_StreamLock);

		// Copy the list here, as the streaming callback can modify the list
		std::vector<SResStreamEntry*> entries = m_EntriesQueue;

		for (size_t i = 0, c = entries.size(); i != c; ++i)
		{
			SResStreamEntry* pEntry = entries[i];
			if (pEntry->m_readStream)
				pEntry->m_readStream->Abort();
		}

		// Copy the list here, as the streaming callback can modify the list
		std::vector<IReadStreamPtr> dirStreams = m_dirReadStreams;

		for (size_t i = 0, c = dirStreams.size(); i != c; ++i)
		{
			IReadStream* pReadStream = dirStreams[i];
			if (pReadStream)
				pReadStream->Abort();
		}
	}
	SResStreamEntry* AddEntry(CCryNameTSCRC Name)
	{
		uint32 i;
		for (i = 0; i < m_EntriesQueue.size(); i++)
		{
			SResStreamEntry* pE = m_EntriesQueue[i];
			if (pE->m_Name == Name)
				break;
		}
		if (i == m_EntriesQueue.size())
		{
			SResStreamEntry* pEntry = new SResStreamEntry;
			pEntry->m_pParent = this;
			pEntry->m_Name = Name;
			m_EntriesQueue.push_back(pEntry);
			return pEntry;
		}
		return NULL;
	}
	void Release()
	{
		assert(m_EntriesQueue.empty());
		delete this;
	}

private:
	SResStreamInfo(const SResStreamInfo&);
	SResStreamInfo& operator=(const SResStreamInfo&);
};

struct SEmptyCombination
{
	uint64                                nRTOrg;
	uint64                                nGLOrg;
	uint32                                nLTOrg;
	uint64                                nRTNew;
	uint64                                nGLNew;
	uint32                                nLTNew;
	uint32                                nMD;
	uint32                                nMDV;
	class CHWShader*                      pShader;

	static std::vector<SEmptyCombination> s_Combinations;
};

//==========================================================================================================================

struct SShaderCombination
{
	uint64         m_RTMask;
	uint32         m_LTMask;
	uint32         m_MDMask;
	uint32         m_MDVMask;
	UPipelineState m_pipelineState;
	SShaderCombination()
	{
		m_RTMask = 0;
		m_LTMask = 0;
		m_MDMask = 0;
		m_MDVMask = 0;
	}
};

struct SCacheCombination
{
	CCryNameR        Name;
	CCryNameR        CacheName;
	SShaderCombIdent Ident;
	uint32           nCount;
	EHWShaderClass   eCL;
	SCacheCombination()
	{
		nCount = 0;
		eCL = eHWSC_Vertex;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const {}
};

struct SShaderGenComb
{
	CCryNameR   Name;
	SShaderGen* pGen;

	SShaderGenComb()
		: Name()
		, pGen(nullptr)
	{
	}

	~SShaderGenComb()
	{
		//SAFE_RELEASE(pGen);
	}

	inline SShaderGenComb(const SShaderGenComb& src)
		: Name(src.Name)
		, pGen(src.pGen)
	{
		if (pGen)
			pGen->m_nRefCount++;
	}

	inline SShaderGenComb& operator=(const SShaderGenComb& src)
	{
		this->~SShaderGenComb();
		new(this)SShaderGenComb(src);
		return *this;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(Name);
		pSizer->AddObject(pGen);
	}
};

typedef std::map<CCryNameR, SCacheCombination>              FXShaderCacheCombinations;
typedef FXShaderCacheCombinations::iterator                 FXShaderCacheCombinationsItor;
typedef std::map<CCryNameR, std::vector<SCacheCombination>> FXShaderCacheCombinationsList;
typedef FXShaderCacheCombinationsList::iterator             FXShaderCacheCombinationsListItor;

#endif
