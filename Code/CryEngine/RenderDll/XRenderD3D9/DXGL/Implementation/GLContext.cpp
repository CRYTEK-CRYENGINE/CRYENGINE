// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   GLContext.cpp
//  Version:     v1.00
//  Created:     25/04/2013 by Valerio Guagliumi.
//  Description: Implementation of the type CContext.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include <StdAfx.h>
#include "GLContext.hpp"
#include "GLDevice.hpp"
#include "GLShader.hpp"
#include "GLView.hpp"

#define DXGL_VALIDATE_PROGRAMS_ON_DRAW 0

namespace NCryOpenGL
{

////////////////////////////////////////////////////////////////////////////
// Global configuration variables
////////////////////////////////////////////////////////////////////////////

#if DXGL_STREAMING_CONSTANT_BUFFERS
int SGlobalConfig::iStreamingConstantBuffersMode = 0;
int SGlobalConfig::iStreamingConstantBuffersPersistentMap = 0;
int SGlobalConfig::iStreamingConstantBuffersGranularity = 0;
int SGlobalConfig::iStreamingConstantBuffersGrowth = 0;
int SGlobalConfig::iStreamingConstantBuffersMaxUnits = 0;
#endif
int SGlobalConfig::iMinFramePoolSize = 0;
int SGlobalConfig::iMaxFramePoolSize = 0;
int SGlobalConfig::iBufferUploadMode = 0;
#if DXGL_ENABLE_SHADER_TRACING
int SGlobalConfig::iShaderTracingMode = 0;
int SGlobalConfig::iShaderTracingHash = 0;
int SGlobalConfig::iVertexTracingID = 0;
int SGlobalConfig::iPixelTracingX = 0;
int SGlobalConfig::iPixelTracingY = 0;
int SGlobalConfig::iComputeTracingWorkGroupIDX = 0;
int SGlobalConfig::iComputeTracingWorkGroupIDY = 0;
int SGlobalConfig::iComputeTracingWorkGroupIDZ = 0;
int SGlobalConfig::iComputeTracingLocalInvocationIDX = 0;
int SGlobalConfig::iComputeTracingLocalInvocationIDY = 0;
int SGlobalConfig::iComputeTracingLocalInvocationIDZ = 0;
#endif

void SGlobalConfig::RegisterVariables()
{
#if DXGL_STREAMING_CONSTANT_BUFFERS
	RegisterConfigVariable("dxgl_streaming_constant_buffers_mode", &iStreamingConstantBuffersMode, 1);
	RegisterConfigVariable("dxgl_streaming_constant_buffer_persistent_map", &iStreamingConstantBuffersPersistentMap, 1);
	RegisterConfigVariable("dxgl_streaming_constant_buffers_granularity", &iStreamingConstantBuffersGranularity, 1024);
	RegisterConfigVariable("dxgl_streaming_constant_buffers_growth", &iStreamingConstantBuffersGrowth, 2);
	RegisterConfigVariable("dxgl_streaming_constant_buffers_max_units", &iStreamingConstantBuffersMaxUnits, 16 * 1024);
#endif
	RegisterConfigVariable("dxgl_min_frame_pool_size", &iMinFramePoolSize, 16);
	RegisterConfigVariable("dxgl_max_frame_pool_size", &iMaxFramePoolSize, 1024);
	RegisterConfigVariable("dxgl_buffer_upload_mode", &iBufferUploadMode, 1);
#if DXGL_ENABLE_SHADER_TRACING
	RegisterConfigVariable("dxgl_shader_tracing_mode", &iShaderTracingMode, 0);
	RegisterConfigVariable("dxgl_shader_tracing_hash", &iShaderTracingHash, 0);
	RegisterConfigVariable("dxgl_vertex_tracing_id", &iVertexTracingID, 0);
	RegisterConfigVariable("dxgl_pixel_tracing_x", &iPixelTracingX, 0);
	RegisterConfigVariable("dxgl_pixel_tracing_y", &iPixelTracingY, 0);
	RegisterConfigVariable("dxgl_compute_tracing_work_group_id_x", &iComputeTracingWorkGroupIDX, 0);
	RegisterConfigVariable("dxgl_compute_tracing_work_group_id_y", &iComputeTracingWorkGroupIDY, 0);
	RegisterConfigVariable("dxgl_compute_tracing_work_group_id_z", &iComputeTracingWorkGroupIDZ, 0);
	RegisterConfigVariable("dxgl_compute_tracing_local_invocation_id_x", &iComputeTracingLocalInvocationIDX, 0);
	RegisterConfigVariable("dxgl_compute_tracing_local_invocation_id_y", &iComputeTracingLocalInvocationIDY, 0);
	RegisterConfigVariable("dxgl_compute_tracing_local_invocation_id_z", &iComputeTracingLocalInvocationIDZ, 0);
#endif //DXGL_ENABLE_SHADER_TRACING
}

////////////////////////////////////////////////////////////////////////////
// State management helper functions
////////////////////////////////////////////////////////////////////////////

inline void SetEnabledState(GLenum eState, bool bEnabled)
{
	if (bEnabled)
		glEnable(eState);
	else
		glDisable(eState);
}

#if DXGL_SUPPORT_INDEXED_ENABLE_STATE
inline void SetEnabledStatei(GLenum eState, GLuint uIndex, bool bEnabled)
{
	if (bEnabled)
		glEnablei(eState, uIndex);
	else
		glDisablei(eState, uIndex);
}
#endif //DXGL_SUPPORT_INDEXED_ENABLE_STATE

inline void GetStateVar(GLenum eTarget, GLint* pData)                     { glGetIntegerv(eTarget, pData); }
inline void GetStateVar(GLenum eTarget, GLenum* pData)                    { glGetIntegerv(eTarget, reinterpret_cast<GLint*>(pData)); }
inline void GetStateVar(GLenum eTarget, GLboolean* pData)                 { glGetBooleanv(eTarget, pData); }
inline void GetStateVar(GLenum eTarget, GLfloat* pData)                   { glGetFloatv(eTarget, pData); }

inline void GetStateVari(GLenum eTarget, uint32 uIndex, GLint* pData)     { glGetIntegeri_v(eTarget, uIndex, pData); }
inline void GetStateVari(GLenum eTarget, uint32 uIndex, GLenum* pData)    { glGetIntegeri_v(eTarget, uIndex, reinterpret_cast<GLint*>(pData)); }
#if DXGL_SUPPORT_INDEXED_BOOL_STATE
inline void GetStateVari(GLenum eTarget, uint32 uIndex, GLboolean* pData) { glGetBooleani_v(eTarget, uIndex, pData); }
#endif //DXGL_SUPPORT_INDEXED_BOOL_STATE
#if DXGL_SUPPORT_INDEXED_FLOAT_STATE
inline void GetStateVari(GLenum eTarget, uint32 uIndex, GLfloat* pData)  { glGetFloati_v(eTarget, uIndex, pData); }
inline void GetStateVari(GLenum eTarget, uint32 uIndex, GLdouble* pData) { glGetDoublei_v(eTarget, uIndex, pData); }
#endif //DXGL_SUPPORT_INDEXED_FLOAT_STATE

////////////////////////////////////////////////////////////////////////////
// Cache of heavy weight objects indexed by configuration
////////////////////////////////////////////////////////////////////////////

struct SFrameBufferCache
{
	struct SHashCompare
	{
		size_t operator()(const SFrameBufferConfiguration& kConfig) const
		{
			return (size_t)NCryOpenGL::GetCRC32(kConfig.m_akAttachments, sizeof(kConfig.m_akAttachments));
		}

		bool operator()(const SFrameBufferConfiguration& kLeft, const SFrameBufferConfiguration& kRight) const
		{
			return memcmp(kLeft.m_akAttachments, kRight.m_akAttachments, sizeof(kLeft.m_akAttachments)) == 0;
		}
	};

	typedef std::unordered_map<SFrameBufferConfiguration, SFrameBufferPtr, SHashCompare, SHashCompare> TMap;

	TMap m_kMap;
};

struct SPipelineCache
{
	struct SHashCompare
	{
		size_t operator()(const SPipelineConfiguration& kConfig) const
		{
			return (size_t)
			       NCryOpenGL::GetCRC32(reinterpret_cast<const char*>(&kConfig.m_apShaders), sizeof(kConfig.m_apShaders))
#if DXGL_ENABLE_SHADER_TRACING
			       ^ kConfig.m_eStageTracing
#endif  //DXGL_ENABLE_SHADER_TRACING
#if !DXGL_SUPPORT_DEPTH_CLAMP
			       ^ ((uint32)kConfig.m_bEmulateDepthClamp)
#endif  //!DXGL_SUPPORT_DEPTH_CLAMP
			       ^ ((uint32)kConfig.m_eMode) << 2;
		}

		bool operator()(const SPipelineConfiguration& kLeft, const SPipelineConfiguration& kRight) const
		{
			int iCmp(memcmp(kLeft.m_apShaders, kRight.m_apShaders, sizeof(kLeft.m_apShaders)));
			if (iCmp != 0)
				return false;

#if DXGL_ENABLE_SHADER_TRACING
			iCmp = (int)kRight.m_eStageTracing - (int)kLeft.m_eStageTracing;
			if (iCmp != 0)
				return false;
#endif  //DXGL_ENABLE_SHADER_TRACING

			iCmp = (int)kRight.m_eMode - (int)kLeft.m_eMode;
			if (iCmp != 0)
				return false;

#if !DXGL_SUPPORT_DEPTH_CLAMP
			iCmp = (int)kRight.m_bEmulateDepthClamp - (int)kLeft.m_bEmulateDepthClamp;
			if (iCmp != 0)
				return false;
#endif  //!DXGL_SUPPORT_DEPTH_CLAMP

			return true;
		}
	};

	typedef std::unordered_map<SPipelineConfiguration, SPipelinePtr, SHashCompare, SHashCompare> TMap;

	TMap m_kMap;
};

struct SUnitMapCache
{
	struct SHashCompare
	{
		size_t operator()(const SUnitMapPtr& kMap) const
		{
			return (size_t)(kMap->m_uNumUnits ^ NCryOpenGL::GetCRC32(kMap->m_akUnits, sizeof(kMap->m_akUnits[0]) * kMap->m_uNumUnits));
		}

		bool operator()(const SUnitMapPtr& kLeft, const SUnitMapPtr& kRight) const
		{
			return kLeft->m_uNumUnits < kRight->m_uNumUnits ||
			       (kLeft->m_uNumUnits == kRight->m_uNumUnits &&
			        memcmp(kLeft->m_akUnits, kRight->m_akUnits, sizeof(kLeft->m_akUnits[0]) * kLeft->m_uNumUnits) < 0);
		}
	};

	typedef std::unordered_map<SUnitMapPtr, SUnitMapPtr, SHashCompare, SHashCompare> TMap;

	TMap m_kMap;
};

////////////////////////////////////////////////////////////////////////////
// Shader tracing helpers
////////////////////////////////////////////////////////////////////////////

#if DXGL_ENABLE_SHADER_TRACING

uint32 DumpShaderTrace(const uint32* puValues, const SShaderTraceIndex& kIndex, uint32 uStride, uint32 uSize, uint32 uCapacity)
{
	if (uSize > uCapacity)
	{
		DXGL_WARNING(
		  "The shader tracing buffer was not big enough to store all selected shader invocations. Only the first %d out of %d will be logged",
		  (uCapacity + uStride - 1) / uStride,
		  (uSize + uStride - 1) / uStride);
		uSize = uCapacity;
	}

	uint32 uNumElements((uSize + uStride - 1) / uStride);
	if (uNumElements > 0)
	{
		STraceFile kTraceFile;
		char acFileName[MAX_PATH];
		static LONG s_uTraceID = 0;
		LONG uTraceID = AtomicIncrement(&s_uTraceID);
		cry_sprintf(acFileName, "shader_trace_%d.txt", uTraceID);
		kTraceFile.Open(acFileName, false);

		for (uint32 uElement = 0; uElement < uNumElements; ++uElement)
		{
			uint32 uStep;
			const uint32* puValue(puValues + uElement * uStride);
			const uint32* puEnd(puValue + uStride);
			++puValue; // Skip first element which serves as dummy for unselected invocations
			while (puValue < puEnd && (uStep = *(puValue++)) < kIndex.m_kTraceStepSizes.size())
			{
				kTraceFile.Printf("Element %d - step %d\n", uElement, uStep);

				uint32 uStepVariablesBegin = kIndex.m_kTraceStepOffsets.at(uStep);

				for (uint32 uVariable = 0; puValue < puEnd && uVariable < kIndex.m_kTraceStepSizes.at(uStep); ++uVariable)
				{
					const VariableTraceInfo* pVariable(&kIndex.m_kTraceVariables.at(uStepVariablesBegin + uVariable));
					char cPrefix;
					switch (pVariable->eGroup)
					{
					case TRACE_VARIABLE_INPUT:
						cPrefix = 'v';
						break;
					case TRACE_VARIABLE_TEMP:
						cPrefix = 'r';
						break;
					case TRACE_VARIABLE_OUTPUT:
						cPrefix = 'o';
						break;
					default:
						DXGL_ERROR("Invalid trace variable group");
						return 0;
					}
					char cComponent("xyzw"[pVariable->ui8Component]);
					switch (pVariable->eType)
					{
					case TRACE_VARIABLE_UNKNOWN:
						kTraceFile.Printf("\t%c%d.%c = 0x%08X\n", cPrefix, pVariable->ui8Index, cComponent, *(puValue++));
						break;
					case TRACE_VARIABLE_UINT:
						kTraceFile.Printf("\t%c%d.%c = %u [0x%08X]\n", cPrefix, pVariable->ui8Index, cComponent, *puValue, *puValue);
						++puValue;
						break;
					case TRACE_VARIABLE_SINT:
						kTraceFile.Printf("\t%c%d.%c = %i [0x%08X]\n", cPrefix, pVariable->ui8Index, cComponent, *reinterpret_cast<const int32*>(puValue), *puValue);
						++puValue;
						break;
					case TRACE_VARIABLE_FLOAT:
						kTraceFile.Printf("\t%c%d.%c = %f [0x%08X]\n", cPrefix, pVariable->ui8Index, cComponent, *reinterpret_cast<const float*>(puValue), *puValue);
						++puValue;
						break;
					case TRACE_VARIABLE_DOUBLE:
						DXGL_NOT_IMPLEMENTED;
						break;
					default:
						DXGL_ERROR("Invalid trace variable type");
						return 0;
					}
				}
			}
			assert(puValue > puValues && *(puValue - 1) == 0xFFFFFFFF);
		}
	}
	return uNumElements;
}

struct SShaderTraceBufferCommon
{
	enum {CAPACITY = 0x100000};

	uint32 m_uSize;
	uint32 m_uStride;
	uint32 m_uCapacity;
};

template<typename StageHeader>
struct SShaderTraceBufferHeader : SShaderTraceBufferCommon
{
	StageHeader m_kStageHeader;
};

template<typename StageHeader>
struct SShaderTraceBuffer : SShaderTraceBufferHeader<StageHeader>
{
	uint32 m_auValues[SShaderTraceBufferCommon::CAPACITY];
};

template<typename StageHeader>
void BeginTraceInternal(uint32 uBufferName, const StageHeader& kStageHeader, uint32 uStride)
{
	typedef SShaderTraceBufferHeader<StageHeader> THeader;

	THeader* pHeader(static_cast<THeader*>(glMapNamedBufferRangeEXT(uBufferName, 0, sizeof(THeader), GL_MAP_WRITE_BIT)));
	pHeader->m_uSize = 0;
	pHeader->m_uStride = uStride;
	pHeader->m_uCapacity = SShaderTraceBufferCommon::CAPACITY;
	pHeader->m_kStageHeader = kStageHeader;
	glUnmapNamedBufferEXT(uBufferName);
}

template<typename StageHeader>
uint32 EndTraceInternal(uint32 uBufferName, EShaderType eShaderType, const SShaderTraceIndex& kIndex)
{
	typedef SShaderTraceBuffer<StageHeader> TBuffer;

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

	const TBuffer* pBuffer(static_cast<TBuffer*>(glMapNamedBufferRangeEXT(uBufferName, 0, sizeof(TBuffer), GL_MAP_READ_BIT)));
	uint32 uNumTraces = DumpShaderTrace(pBuffer->m_auValues, kIndex, pBuffer->m_uStride, pBuffer->m_uSize, pBuffer->m_uCapacity);
	glUnmapNamedBufferEXT(uBufferName);

	return uNumTraces;
}

#endif //DXGL_ENABLE_SHADER_TRACING

#if DXGL_STREAMING_CONSTANT_BUFFERS

////////////////////////////////////////////////////////////////////////////
// SStreamingBufferContext implementation
////////////////////////////////////////////////////////////////////////////

SStreamingBufferContext::SStreamingBufferContext()
	: m_uPreviousFramesBegin(0)
	, m_uPreviousFramesNum(0)
	, m_pFreeFramesHead(NULL)
	, m_uNumStreamingBuffersUnits(0)
	#if DXGL_SUPPORT_BUFFER_STORAGE
	, m_bFlushNeeded(false)
	#endif
{
}

SStreamingBufferContext::~SStreamingBufferContext()
{
	for (uint32 uFramePool = 0; uFramePool < m_kFramePools.size(); ++uFramePool)
		Free(m_kFramePools.at(uFramePool));
}

void SStreamingBufferContext::SwitchFrame(CDevice* pDevice)
{
	if (SGlobalConfig::iStreamingConstantBuffersMode <= 0)
		return;

	while (m_uPreviousFramesNum > 0)
	{
		GLint iResult(GL_UNSIGNALED);
		glGetSynciv(m_aspPreviousFrames[m_uPreviousFramesBegin]->m_pEndFence, GL_SYNC_STATUS, sizeof(iResult), NULL, &iResult);
		if (iResult != GL_SIGNALED)
			break;

		glDeleteSync(m_aspPreviousFrames[m_uPreviousFramesBegin]->m_pEndFence);
		m_aspPreviousFrames[m_uPreviousFramesBegin] = NULL;
		--m_uPreviousFramesNum;
		m_uPreviousFramesBegin = (m_uPreviousFramesBegin + 1) % MAX_PREVIOUS_FRAMES;
	}

	if (m_spCurrentFrame == NULL || m_uPreviousFramesNum < MAX_PREVIOUS_FRAMES)
	{
		if (m_spCurrentFrame != NULL)
		{
			m_spCurrentFrame->m_pEndFence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
			m_aspPreviousFrames[(m_uPreviousFramesBegin + m_uPreviousFramesNum) % MAX_PREVIOUS_FRAMES] = m_spCurrentFrame;
			++m_uPreviousFramesNum;
		}

		if (m_pFreeFramesHead == NULL)
		{
			uint32 uSize(min((uint32)(SGlobalConfig::iMinFramePoolSize << m_kFramePools.size()), (uint32)SGlobalConfig::iMaxFramePoolSize));
			SContextFrame* pNewPool(static_cast<SContextFrame*>(Malloc(sizeof(SContextFrame) * uSize)));
			m_kFramePools.push_back(pNewPool);

			m_pFreeFramesHead = pNewPool;
			uint32 uLast(uSize - 1);
			pNewPool[uLast].m_pNext = NULL;
			for (uint32 uElement = 0; uElement < uLast; ++uElement)
				pNewPool[uElement].m_pNext = pNewPool + uElement + 1;
		}

		SContextFrame* pNextFreeFrame(m_pFreeFramesHead->m_pNext);
		m_spCurrentFrame = new(m_pFreeFramesHead) SContextFrame(&m_pFreeFramesHead);
		m_pFreeFramesHead = pNextFreeFrame;
	}

	UpdateStreamingSizes(pDevice);
}

void SStreamingBufferContext::UpdateStreamingSizes(CDevice* pDevice)
{
	if (SGlobalConfig::iStreamingConstantBuffersMode <= 0)
		return;

	DXGL_TODO("Add some type of lazy constant buffer unit reclaiming scheme to release units that have not been used for several frames")

	SStreamingBuffer * pBuffersEnd(m_akStreamingBuffers + MAX_CONSTANT_BUFFER_SLOTS);
	for (SStreamingBuffer* pBuffer = m_akStreamingBuffers; pBuffer < pBuffersEnd; ++pBuffer)
	{
		if (pBuffer->m_uRequestedSectionSize > pBuffer->m_uSectionCapacity)
		{
			uint32 uGranularity((uint32)SGlobalConfig::iStreamingConstantBuffersGranularity);
			uint32 uMinUnits((pBuffer->m_uRequestedSectionSize + uGranularity - 1) / uGranularity);
			uint32 uOldUnits(pBuffer->m_uSectionCapacity / uGranularity);
			uint32 uDesiredUnits(uOldUnits);
			while (uDesiredUnits < uMinUnits)
			{
				uDesiredUnits = max(1u, uDesiredUnits * (uint32)SGlobalConfig::iStreamingConstantBuffersGrowth);
			}
			uint32 uMissingUnits(uDesiredUnits - uOldUnits);

			if (m_uNumStreamingBuffersUnits + uMissingUnits < SGlobalConfig::iStreamingConstantBuffersMaxUnits)
			{
				DXGL_TODO("Evaluate the possibility of pooling freed streaming buffer storages by size rather than deleting and creating a new one each time");

				GLuint uName;
				if (pBuffer->m_kName.IsValid())
				{
					uName = pBuffer->m_kName.GetName();
					glDeleteBuffers(1, &uName);
				}
				glGenBuffers(1, &uName);
				pBuffer->m_kName = pDevice->GetBufferNamePool().Create(uName);

				pBuffer->m_uSectionCapacity = uDesiredUnits * uGranularity;
				pBuffer->m_uNextPosition = 0;
				pBuffer->m_uEndPosition = pBuffer->m_uSectionCapacity;
				GLsizeiptr iNewSize(pBuffer->m_uSectionCapacity * MAX_PREVIOUS_FRAMES);
	#if DXGL_SUPPORT_BUFFER_STORAGE
				if (SupportBufferStorage() && SGlobalConfig::iStreamingConstantBuffersPersistentMap)
				{
					glNamedBufferStorageEXT(pBuffer->m_kName.GetName(), iNewSize, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
					pBuffer->m_pMappedData = glMapNamedBufferRangeEXT(pBuffer->m_kName.GetName(), 0, iNewSize, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
				}
				else
	#endif  //DXGL_SUPPORT_BUFFER_STORAGE
				{
					glNamedBufferDataEXT(pBuffer->m_kName.GetName(), iNewSize, NULL, GL_STREAM_DRAW);
				}
				m_uNumStreamingBuffersUnits += uMissingUnits;
			}
		}
		else if (pBuffer->m_uSectionCapacity > 0)
		{
			pBuffer->m_uNextPosition =
			  (pBuffer->m_uNextPosition + pBuffer->m_uSectionCapacity - 1 - (pBuffer->m_uNextPosition - 1) % pBuffer->m_uSectionCapacity) %
			  (pBuffer->m_uSectionCapacity * MAX_PREVIOUS_FRAMES);
			pBuffer->m_uEndPosition = pBuffer->m_uNextPosition + pBuffer->m_uSectionCapacity;
		}
		pBuffer->m_uRequestedSectionSize = 0;
	}
}

void SStreamingBufferContext::UploadAndBindUniformData(CContext* pContext, const SConstantBufferSlot& kSlot, uint32 uUnit)
{
	uint32 uStreamingAlignment(pContext->GetDevice()->GetAdapter()->m_kCapabilities.m_iUniformBufferOffsetAlignment);
	#if !OGL_SINGLE_CONTEXT
	uint32 uContextIndex(pContext->GetIndex());
	#endif

	if (kSlot.m_pBuffer != NULL)
	{
		SBuffer* pBuffer(kSlot.m_pBuffer);
		SStreamingBuffer* pStreaming(m_akStreamingBuffers + uUnit);
		uint32 uStreamingSize(kSlot.m_kRange.m_uSize + uStreamingAlignment - 1 - (kSlot.m_kRange.m_uSize - 1) % uStreamingAlignment);
	#if OGL_SINGLE_CONTEXT
		SBuffer::SContextCache* pContextCache(&pBuffer->m_kContextCache);
	#else
		SBuffer::SContextCache* pContextCache(pBuffer->m_akContextCaches + uContextIndex);
	#endif

		if (pBuffer->m_bStreaming)
		{
	#if OGL_SINGLE_CONTEXT
			bool bDataDirty(!pBuffer->m_bContextCacheValid);
	#else
			bool bDataDirty(!pBuffer->m_kContextCachesValid.Get(uContextIndex));
	#endif
			bool bCopyDirty(
			  (pContextCache->m_spFrame != NULL && (
			     pContextCache->m_spFrame != m_spCurrentFrame ||
			     pContextCache->m_uUnit != uUnit ||
			     pContextCache->m_kRange != kSlot.m_kRange)));
			if (bDataDirty || bCopyDirty)
			{
				if (pStreaming->m_uNextPosition + uStreamingSize < pStreaming->m_uEndPosition)
				{
					pContextCache->m_spFrame = m_spCurrentFrame;
					pContextCache->m_uUnit = uUnit;
					pContextCache->m_kRange = kSlot.m_kRange;
					pContextCache->m_uStreamOffset = pStreaming->m_uNextPosition;

					void* pSrcData(pBuffer->m_pSystemMemoryCopy + kSlot.m_kRange.m_uOffset);
					size_t uSize(kSlot.m_kRange.m_uSize);

	#if DXGL_SUPPORT_BUFFER_STORAGE
					if (pStreaming->m_pMappedData != NULL)
					{
						GLvoid* pData(static_cast<uint8*>(pStreaming->m_pMappedData) + pStreaming->m_uNextPosition);
						NCryOpenGL::Memcpy(pData, pSrcData, uSize);
						m_bFlushNeeded = true;
					}
					else
	#endif
					{
						GLintptr piDstOffset((GLintptr)pStreaming->m_uNextPosition);
						if (SGlobalConfig::iBufferUploadMode > 0)
						{
							glNamedBufferSubDataEXT(pStreaming->m_kName.GetName(), piDstOffset, (GLsizeiptr)uSize, pSrcData);
						}
						else
						{
							GLvoid* pData(
							  glMapNamedBufferRangeEXT(
							    pStreaming->m_kName.GetName(),
							    piDstOffset,
							    (GLsizeiptr)uStreamingSize,
							    GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT));
							NCryOpenGL::Memcpy(pData, pSrcData, uSize);
							glUnmapNamedBufferEXT(pStreaming->m_kName.GetName());
						}
					}

					pStreaming->m_uNextPosition += uStreamingSize;
				}
				else if (pBuffer->m_bStreaming)
				{
	#if OGL_SINGLE_CONTEXT
					pBuffer->m_kContextCache.m_spFrame = NULL;
	#else
					pBuffer->m_akContextCaches[uContextIndex].m_spFrame = NULL;
	#endif
					glNamedBufferDataEXT(pBuffer->m_kName.GetName(), pBuffer->m_uSize, pBuffer->m_pSystemMemoryCopy, pBuffer->m_eUsage);
				}

	#if OGL_SINGLE_CONTEXT
				pBuffer->m_bContextCacheValid = true;
	#else
				pBuffer->m_kContextCachesValid.Set(uContextIndex, true);
	#endif
				pStreaming->m_uRequestedSectionSize += uStreamingSize;
			}
		}

	#if OGL_SINGLE_CONTEXT
		if (pBuffer->m_kContextCache.m_spFrame == NULL)
	#else
		if (pBuffer->m_akContextCaches[uContextIndex].m_spFrame == NULL)
	#endif
		{
			pBuffer->m_kCreationFence.IssueWait(pContext);
			pContext->BindUniformBuffer(TIndexedBufferBinding(pBuffer->m_kName, kSlot.m_kRange), uUnit);
		}
		else
		{
	#if OGL_SINGLE_CONTEXT
			SBufferRange kStreamingRange(pBuffer->m_kContextCache.m_uStreamOffset, uStreamingSize);
	#else
			SBufferRange kStreamingRange(pBuffer->m_akContextCaches[uContextIndex].m_uStreamOffset, uStreamingSize);
	#endif
			pContext->BindUniformBuffer(TIndexedBufferBinding(pStreaming->m_kName, kStreamingRange), uUnit);
		}
	}
}

void SStreamingBufferContext::FlushUniformData()
{
	#if DXGL_SUPPORT_BUFFER_STORAGE
	if (m_bFlushNeeded)
		glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
	#endif //DXGL_SUPPORT_BUFFER_STORAGE
}

#endif //DXGL_STREAMING_CONSTANT_BUFFERS

////////////////////////////////////////////////////////////////////////////
// CContext implementation
////////////////////////////////////////////////////////////////////////////

CContext::CContext(CDevice* pDevice, const TRenderingContext& kRenderingContext, const TWindowContext& kDefaultWindowContext, uint32 uIndex)
	: m_uIndex(uIndex)
	, m_pReservedContext(nullptr)
	, m_pDevice(pDevice)
	, m_kRenderingContext(kRenderingContext)
	, m_kWindowContext(kDefaultWindowContext)
	, m_uReservationCount(0)
	, m_ePrimitiveTopologyMode(0)
	, m_eIndexType(GL_NONE)
	, m_uIndexStride(0)
	, m_uIndexOffset(0)
	, m_uGlobalVAO(0)
	, m_kTextureUnitContext()
	, m_pInputLayout(nullptr)
	, m_bFrameBufferStateDirty(false)
	, m_bPipelineDirty(false)
	, m_bInputLayoutDirty(false)
	, m_bInputAssemblerSlotsDirty(false)
#if DXGL_ENABLE_SHADER_TRACING
	, m_eStageTracing(eST_NUM)
	, m_uShaderTraceHash(0)
	, m_uShaderTraceCount(0)
#endif //DXGL_ENABLE_SHADER_TRACING
	, m_pFrameBufferCache(new SFrameBufferCache)
	, m_pPipelineCache(new SPipelineCache)
	, m_pUnitMapCache(new SUnitMapCache)
{
	ZeroStruct(m_kStateCache);

	for (uint32 uUnitType = 0; uUnitType < eRUT_NUM; ++uUnitType)
		m_abResourceUnitsDirty[uUnitType] = false;
	memset(m_apResourceUnitMaps, 0, sizeof(m_apResourceUnitMaps));
}

CContext::~CContext()
{
#if DXGL_ENABLE_SHADER_TRACING
	if (m_kShaderTracingBuffer.IsValid())
	{
		GLuint uName(m_kShaderTracingBuffer.GetName());
		glDeleteBuffers(1, &uName);
	}
#endif //DXGL_ENABLE_SHADER_TRACING

	glBindVertexArray(0);
	glDeleteVertexArrays(1, &m_uGlobalVAO);

#if !DXGL_SUPPORT_COPY_IMAGE
	GLuint uCopyPixelBufferName(m_kCopyPixelBuffer.GetName());
	glDeleteBuffers(1, &uCopyPixelBufferName);
#endif //!DXGL_SUPPORT_COPY_IMAGE

	delete m_pFrameBufferCache;
	delete m_pPipelineCache;
	delete m_pUnitMapCache;
}

bool CContext::Initialize()
{
	DXGL_SCOPED_PROFILE("CContext::Initialize")

	memset(&m_kStateCache, 0, sizeof(m_kStateCache));

#if DXGL_TRACE_CALLS
	char acTraceFileName[16];
	cry_sprintf(acTraceFileName, "Calls_%d", m_uIndex);
	m_kCallTrace.Open(acTraceFileName, false);
#endif

	DXGL_TODO("Try with separate cached VAOs for each input layout and vertex buffer to see if it improves performance");
	glGenVertexArrays(1, &m_uGlobalVAO);
	glBindVertexArray(m_uGlobalVAO);

#if !DXGL_SUPPORT_COPY_IMAGE
	GLuint uCopyPixelBufferName;
	glGenBuffers(1, &uCopyPixelBufferName);
	m_kCopyPixelBuffer = m_pDevice->GetBufferNamePool().Create(uCopyPixelBufferName);
#endif //!DXGL_SUPPORT_COPY_IMAGE

#if !DXGLES //Seamless cube map filtering is not optional in GL ES
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
#endif //!DXGLES

	const SCapabilities& kCapabilities(m_pDevice->GetAdapter()->m_kCapabilities);
	uint32 uNumTextureUnits(min((uint32)DXGL_ARRAY_SIZE(m_kStateCache.m_akTextureUnits), (uint32)kCapabilities.m_akResourceUnits[eRUT_Texture].m_aiMaxTotal));
#if DXGL_SUPPORT_SHADER_IMAGES
	uint32 uNumImageUnits(min((uint32)DXGL_ARRAY_SIZE(m_kStateCache.m_akImageUnits), (uint32)kCapabilities.m_akResourceUnits[eRUT_Image].m_aiMaxTotal));
#endif //DXGL_SUPPORT_SHADER_IMAGES

	for (uint32 uUnit = 0; uUnit < uNumTextureUnits; ++uUnit)
		GetTextureUnitCache(uUnit, m_kStateCache.m_akTextureUnits[uUnit]);
#if DXGL_SUPPORT_SHADER_IMAGES
	for (uint32 uUnit = 0; uUnit < uNumImageUnits; ++uUnit)
		GetImageUnitCache(uUnit, m_kStateCache.m_akImageUnits[uUnit]);
#endif //DXGL_SUPPORT_SHADER_IMAGES

	return GetImplicitStateCache(m_kStateCache) &&
	       GetBlendCache(m_kStateCache.m_kBlend) &&
	       GetDepthStencilCache(m_kStateCache.m_kDepthStencil) &&
	       GetRasterizerCache(m_kStateCache.m_kRasterizer) &&
	       GetInputAssemblerCache(m_kStateCache.m_kInputAssembler);
}

void CContext::SetWindowContext(const TWindowContext& kWindowContext)
{
	if (m_kWindowContext != kWindowContext)
	{
		m_kWindowContext = kWindowContext;

		// Update the current target window if this context is current
		if (m_pDevice->GetCurrentContext() == this)
			m_pDevice->SetCurrentContext(this);
	}
}

bool CContext::GetBlendCache(SBlendCache& kCache)
{
	DXGL_SCOPED_PROFILE("CContext::GetBlendCache")

	kCache.m_bAlphaToCoverageEnable = glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE) == GL_TRUE;

#if DXGL_SUPPORT_INDEPENDENT_BLEND_STATES
	kCache.m_bIndependentBlendEnable = false; // Enabled only if render target blend states are not compatible
	uint32 uTarget;
	for (uTarget = 0; uTarget < DXGL_ARRAY_SIZE(kCache.m_kTargets); ++uTarget)
	{
		STargetBlendState& kRTCache(kCache.m_kTargets[uTarget]);

	#if DXGL_SUPPORT_INDEXED_BOOL_STATE
		GetStateVari(GL_COLOR_WRITEMASK, uTarget, kRTCache.m_kWriteMask.m_abRGBA);
	#endif
		kRTCache.m_bEnable = glIsEnabledi(GL_BLEND, uTarget) == TRUE;
		if (kRTCache.m_bEnable)
		{
			GetStateVari(GL_BLEND_EQUATION_RGB, uTarget, &kRTCache.m_kRGB.m_eEquation);
			GetStateVari(GL_BLEND_EQUATION_ALPHA, uTarget, &kRTCache.m_kAlpha.m_eEquation);

			GetStateVari(GL_BLEND_SRC_RGB, uTarget, &kRTCache.m_kRGB.m_kFunction.m_eSrc);
			GetStateVari(GL_BLEND_SRC_ALPHA, uTarget, &kRTCache.m_kAlpha.m_kFunction.m_eSrc);

			GetStateVari(GL_BLEND_DST_RGB, uTarget, &kRTCache.m_kRGB.m_kFunction.m_eDst);
			GetStateVari(GL_BLEND_DST_ALPHA, uTarget, &kRTCache.m_kAlpha.m_kFunction.m_eDst);

			kRTCache.m_bSeparateAlpha = kRTCache.m_kRGB == kRTCache.m_kAlpha; // Disabled if the rgb and alpha parameters are the same

			if (uTarget > 0 && kCache.m_kTargets[0].m_bEnable)
			{
				// Check if the parameters for this target are compatible with the default ones (target 0)
				if (kCache.m_kTargets[0] != kRTCache)
					kCache.m_bIndependentBlendEnable = true;
			}
		}
		else
		{
			if (uTarget == 0)
				kCache.m_bIndependentBlendEnable = true;  // Can't use unique blending parameters as the default ones (target 0) are not present
		}
	}
#else
	STargetBlendState& kRTCache(kCache.m_kTargets[0]);

	GetStateVar(GL_COLOR_WRITEMASK, kRTCache.m_kWriteMask.m_abRGBA);
	kRTCache.m_bEnable = glIsEnabled(GL_BLEND) == TRUE;
	if (kRTCache.m_bEnable)
	{
		GetStateVar(GL_BLEND_EQUATION_RGB, &kRTCache.m_kRGB.m_eEquation);
		GetStateVar(GL_BLEND_EQUATION_ALPHA, &kRTCache.m_kAlpha.m_eEquation);

		GetStateVar(GL_BLEND_SRC_RGB, &kRTCache.m_kRGB.m_kFunction.m_eSrc);
		GetStateVar(GL_BLEND_SRC_ALPHA, &kRTCache.m_kAlpha.m_kFunction.m_eSrc);

		GetStateVar(GL_BLEND_DST_RGB, &kRTCache.m_kRGB.m_kFunction.m_eDst);
		GetStateVar(GL_BLEND_DST_ALPHA, &kRTCache.m_kAlpha.m_kFunction.m_eDst);

		kRTCache.m_bSeparateAlpha = kRTCache.m_kRGB == kRTCache.m_kAlpha; // Disabled if the rgb and alpha parameters are the same
	}

	for (uint32 uOverriddenTarget = 1; uOverriddenTarget < DXGL_ARRAY_SIZE(kCache.m_kTargets); ++uOverriddenTarget)
		kCache.m_kTargets[uOverriddenTarget] = kCache.m_kTargets[0];
#endif

	return true;
}

bool CContext::GetDepthStencilCache(
  SDepthStencilCache& kCache)
{
	DXGL_SCOPED_PROFILE("CContext::GetDepthStencilCache")

	kCache.m_bDepthTestingEnabled = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
	GetStateVar(GL_DEPTH_FUNC, reinterpret_cast<GLint*>(&kCache.m_eDepthTestFunc));
	GetStateVar(GL_DEPTH_WRITEMASK, &kCache.m_bDepthWriteMask);
	kCache.m_bStencilTestingEnabled = glIsEnabled(GL_STENCIL_TEST) == GL_TRUE;

	GetStateVar(GL_STENCIL_FUNC, &kCache.m_kStencilFrontFaces.m_eFunction);
	GetStateVar(GL_STENCIL_FAIL, &kCache.m_kStencilFrontFaces.m_eStencilFailOperation);
	GetStateVar(GL_STENCIL_PASS_DEPTH_FAIL, &kCache.m_kStencilFrontFaces.m_eDepthFailOperation);
	GetStateVar(GL_STENCIL_PASS_DEPTH_PASS, &kCache.m_kStencilFrontFaces.m_eDepthPassOperation);
	GetStateVar(GL_STENCIL_WRITEMASK, &kCache.m_kStencilFrontFaces.m_uStencilWriteMask);
	GetStateVar(GL_STENCIL_VALUE_MASK, &kCache.m_kStencilFrontFaces.m_uStencilReadMask);
	GetStateVar(GL_STENCIL_REF, &kCache.m_kStencilRef.m_iFrontFacesReference);

	GetStateVar(GL_STENCIL_BACK_FUNC, &kCache.m_kStencilBackFaces.m_eFunction);
	GetStateVar(GL_STENCIL_BACK_FAIL, &kCache.m_kStencilBackFaces.m_eStencilFailOperation);
	GetStateVar(GL_STENCIL_BACK_PASS_DEPTH_FAIL, &kCache.m_kStencilBackFaces.m_eDepthFailOperation);
	GetStateVar(GL_STENCIL_BACK_PASS_DEPTH_PASS, &kCache.m_kStencilBackFaces.m_eDepthPassOperation);
	GetStateVar(GL_STENCIL_BACK_WRITEMASK, &kCache.m_kStencilBackFaces.m_uStencilWriteMask);
	GetStateVar(GL_STENCIL_BACK_VALUE_MASK, &kCache.m_kStencilBackFaces.m_uStencilReadMask);
	GetStateVar(GL_STENCIL_BACK_REF, &kCache.m_kStencilRef.m_iBackFacesReference);

	return true;
}

bool CContext::GetRasterizerCache(SRasterizerCache& kCache)
{
	DXGL_SCOPED_PROFILE("CContext::GetRasterizerCache")

	GetStateVar(GL_FRONT_FACE, reinterpret_cast<GLint*>(&kCache.m_eFrontFaceMode));
	GetStateVar(GL_CULL_FACE_MODE, reinterpret_cast<GLint*>(&kCache.m_eCullFaceMode));
	GetStateVar(GL_POLYGON_OFFSET_UNITS, &kCache.m_fPolygonOffsetUnits);
	GetStateVar(GL_POLYGON_OFFSET_FACTOR, &kCache.m_fPolygonOffsetFactor);
#if !DXGLES
	GetStateVar(GL_POLYGON_MODE, reinterpret_cast<GLint*>(&kCache.m_ePolygonMode));
#endif //!DXGLES

	kCache.m_bCullingEnabled = glIsEnabled(GL_CULL_FACE) == GL_TRUE;
	kCache.m_bPolygonOffsetFillEnabled = glIsEnabled(GL_POLYGON_OFFSET_FILL) == GL_TRUE;
	kCache.m_bScissorEnabled = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;
#if DXGL_SUPPORT_DEPTH_CLAMP
	kCache.m_bDepthClipEnabled = glIsEnabled(GL_DEPTH_CLAMP) == GL_FALSE;
#else
	kCache.m_bDepthClipEnabled = false;
#endif
#if !DXGLES
	kCache.m_bLineSmoothEnabled = glIsEnabled(GL_LINE_SMOOTH) == GL_TRUE;
	kCache.m_bPolygonOffsetLineEnabled = glIsEnabled(GL_POLYGON_OFFSET_LINE) == GL_TRUE;
	kCache.m_bMultisampleEnabled = glIsEnabled(GL_MULTISAMPLE) == GL_TRUE;
#endif //!DXGLES

#if !DXGLES
	switch (kCache.m_ePolygonMode)
	{
	case GL_LINE:
		kCache.m_bPolygonOffsetEnabled = kCache.m_bPolygonOffsetLineEnabled;
		break;
	case GL_FILL:
		kCache.m_bPolygonOffsetEnabled = kCache.m_bPolygonOffsetFillEnabled;
		break;
	default:
		DXGL_WARNING("Unexpected value for GL_POLYGON_MODE - should be GL_LINE or GL_FILL");
		break;
	}
#else
	kCache.m_bPolygonOffsetEnabled = kCache.m_bPolygonOffsetFillEnabled;
#endif

	// There is only polygon offset fill in OpenGL ES and no support for custom
	// clip planes
#if !DXGLES
	if ((glIsEnabled(GL_POLYGON_OFFSET_LINE) == GL_TRUE) != kCache.m_bPolygonOffsetEnabled)
	{
		DXGL_WARNING("GL_POLYGON_OFFSET_LINE is required to have the same state of GL_POLYGON_OFFSET_FILL for cache coherence - overriding it now.");
		SetEnabledState(GL_POLYGON_OFFSET_LINE, kCache.m_bPolygonOffsetEnabled);
	}

	GLenum eUserClipPlane(GL_CLIP_DISTANCE1);
	GLenum eLastUserClipPlane(GL_CLIP_DISTANCE5);
	while (eUserClipPlane <= eLastUserClipPlane)
	{
		if (glIsEnabled(eUserClipPlane))
		{
			DXGL_WARNING("User clip planes are not exposed to Direct3D (deprecated in DX10) - disabling it now for coherence");
			glDisable(eUserClipPlane);
		}
		++eUserClipPlane;
	}
#endif //!DXGLES

	return true;
}

bool CContext::GetTextureUnitCache(uint32 uUnit, STextureUnitCache& kCache)
{
	DXGL_SCOPED_PROFILE("CContext::GetTextureUnitCache")

	struct STarget
	{
		GLenum m_eName;
		GLenum m_eBinding;
	};
	static STarget s_akTargets[] =
	{
		{ GL_TEXTURE_2D,             GL_TEXTURE_BINDING_2D             },
		{ GL_TEXTURE_2D_ARRAY,       GL_TEXTURE_BINDING_2D_ARRAY       },
		{ GL_TEXTURE_CUBE_MAP,       GL_TEXTURE_BINDING_CUBE_MAP       },
		{ GL_TEXTURE_3D,             GL_TEXTURE_BINDING_3D             },
#if !DXGLES
		{ GL_TEXTURE_1D,             GL_TEXTURE_BINDING_1D             },
		{ GL_TEXTURE_1D_ARRAY,       GL_TEXTURE_BINDING_1D_ARRAY       },
		{ GL_TEXTURE_RECTANGLE,      GL_TEXTURE_BINDING_RECTANGLE      },
#endif //!DXGLES
#if DXGL_SUPPORT_TEXTURE_BUFFERS
		{ GL_TEXTURE_BUFFER,         GL_TEXTURE_BINDING_BUFFER         },
#endif //DXGL_SUPPORT_TEXTURE_BUFFERS
#if DXGL_SUPPORT_CUBEMAP_ARRAYS
		{ GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_BINDING_CUBE_MAP_ARRAY },
#endif //DXGL_SUPPORT_CUBEMAP_ARRAYS
	};

	glActiveTexture(GL_TEXTURE0 + uUnit);

	GLint iSamplerBinding;
	GetStateVar(GL_SAMPLER_BINDING, &iSamplerBinding);

	kCache.m_kTextureName = CResourceName();
	kCache.m_eTextureTarget = 0;
	kCache.m_uSampler = static_cast<GLuint>(iSamplerBinding);

	uint32 uTarget;
	for (uTarget = 0; uTarget < DXGL_ARRAY_SIZE(s_akTargets); ++uTarget)
	{
		GLint iTargetTexture;
		GetStateVar(s_akTargets[uTarget].m_eBinding, &iTargetTexture);

		if (iTargetTexture != 0)
		{
			if (kCache.m_eTextureTarget != 0)
			{
				DXGL_ERROR("At most one resource binding per texture unit supported");
				return false;
			}

			kCache.m_kTextureName = m_pDevice->GetTextureNamePool().Create((GLuint)iTargetTexture);
			kCache.m_eTextureTarget = s_akTargets[uTarget].m_eName;
		}
	}

	return true;
}

#if DXGL_SUPPORT_SHADER_IMAGES

bool CContext::GetImageUnitCache(uint32 uUnit, SImageUnitCache& kCache)
{
	DXGL_SCOPED_PROFILE("CContext::GetImageUnitCache")

	GLint iTexture;
	GLboolean bLayered;
	GLint iLayer;
	GLint iFormat;
	GetStateVari(GL_IMAGE_BINDING_NAME, uUnit, &iTexture);
	GetStateVari(GL_IMAGE_BINDING_LEVEL, uUnit, &kCache.m_kConfiguration.m_iLevel);
	GetStateVari(GL_IMAGE_BINDING_LAYERED, uUnit, &bLayered);
	GetStateVari(GL_IMAGE_BINDING_LAYER, uUnit, &iLayer);
	GetStateVari(GL_IMAGE_BINDING_ACCESS, uUnit, &kCache.m_kConfiguration.m_eAccess);
	GetStateVari(GL_IMAGE_BINDING_FORMAT, uUnit, &iFormat);

	kCache.m_kConfiguration.m_eFormat = (GLenum)iFormat;
	kCache.m_kConfiguration.m_iLayer = bLayered ? iLayer : -1;
	;
	kCache.m_kTextureName = iTexture == 0 ? CResourceName() : m_pDevice->GetTextureNamePool().Create((GLuint)iTexture);

	return true;
}

#endif //DXGL_SUPPORT_SHADER_IMAGES

template<typename T>
void GetVertexAttrib(GLuint uSlot, GLenum eParameter, T* pValue)
{
	GLint iValue;
	glGetVertexAttribiv(uSlot, eParameter, &iValue);
	*pValue = (T)iValue;
}

bool CContext::GetInputAssemblerCache(SInputAssemblerCache& kCache)
{
	DXGL_SCOPED_PROFILE("CContext::GetInputAssemblerCache")

	GLint iNumVertexAttribs(m_pDevice->GetAdapter()->m_kCapabilities.m_iMaxVertexAttribs);
	if (iNumVertexAttribs > sizeof(kCache.m_uVertexAttribsEnabled) * 8)
	{
		DXGL_WARNING("Currently %d vertex attribute slots supported at most, additional attributes will not be used", (int)sizeof(kCache.m_uVertexAttribsEnabled) * 8);
		iNumVertexAttribs = sizeof(kCache.m_uVertexAttribsEnabled) * 8;
	}

	kCache.m_uVertexAttribsEnabled = 0;
	uint32 uSlot;
	for (uSlot = 0; uSlot < iNumVertexAttribs; ++uSlot)
	{
		GLint iEnabled;
		glGetVertexAttribiv(uSlot, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &iEnabled);
		if (iEnabled != GL_FALSE)
			kCache.m_uVertexAttribsEnabled |= 1 << uSlot;

		GetVertexAttrib(uSlot, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, &kCache.m_auVertexAttribDivisors[uSlot]);
	}

#if DXGL_SUPPORT_VERTEX_ATTRIB_BINDING
	if (SupportVertexAttribBinding())
	{
		for (uint32 uSlot = 0; uSlot < iNumVertexAttribs; ++uSlot)
		{
			GetVertexAttrib(uSlot, GL_VERTEX_ATTRIB_ARRAY_SIZE, &kCache.m_aVertexAttribFormats[uSlot].m_iSize);
			GetVertexAttrib(uSlot, GL_VERTEX_ATTRIB_RELATIVE_OFFSET, &kCache.m_aVertexAttribFormats[uSlot].m_uRelativeOffset);
			GetVertexAttrib(uSlot, GL_VERTEX_ATTRIB_ARRAY_TYPE, &kCache.m_aVertexAttribFormats[uSlot].m_eType);
			GetVertexAttrib(uSlot, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &kCache.m_aVertexAttribFormats[uSlot].m_bNormalized);
			GetVertexAttrib(uSlot, GL_VERTEX_ATTRIB_ARRAY_INTEGER, &kCache.m_aVertexAttribFormats[uSlot].m_bInteger);

			GetVertexAttrib(uSlot, GL_VERTEX_ATTRIB_BINDING, &kCache.m_auVertexBindingIndices[uSlot]);
		}

		const GLint iNumVertexAttribBindings(m_pDevice->GetAdapter()->m_kCapabilities.m_iMaxVertexAttribBindings);

		for (uint32 uBinding = 0; uBinding < iNumVertexAttribBindings; ++uBinding)
		{
			GetStateVari(GL_VERTEX_BINDING_DIVISOR, uBinding, kCache.m_auVertexBindingDivisors + uBinding);
		}
		kCache.m_iLastNonZeroBindingSlot = -1;
	}
#endif //DXGL_SUPPORT_VERTEX_ATTRIB_BINDING

	return true;
}

bool CContext::GetImplicitStateCache(SImplicitStateCache& kCache)
{
	DXGL_SCOPED_PROFILE("CContext::GetImplicitStateCache")

#if DXGL_SUPPORT_MULTISAMPLED_TEXTURES
	// Direct3D11 supports at most 32 bit sample mask
	GetStateVari(GL_SAMPLE_MASK_VALUE, 0, &kCache.m_aSampleMask);
	kCache.m_bSampleMaskEnabled = glIsEnabled(GL_SAMPLE_MASK) == GL_TRUE;
#endif //DXGL_SUPPORT_MULTISAMPLED_TEXTURES

	GLuint uDrawFrameBuffer;
	GLuint uReadFrameBuffer;
#if CRY_PLATFORM_IOS
	uDrawFrameBuffer = 1;
	uReadFrameBuffer = 0;
#else
	GetStateVar(GL_DRAW_FRAMEBUFFER_BINDING, &uDrawFrameBuffer);
	GetStateVar(GL_READ_FRAMEBUFFER_BINDING, &uReadFrameBuffer);
#endif
	kCache.m_kDrawFrameBuffer = m_pDevice->GetFrameBufferNamePool().Create(uDrawFrameBuffer);
	kCache.m_kReadFrameBuffer = m_pDevice->GetFrameBufferNamePool().Create(uReadFrameBuffer);

	GetStateVar(GL_BLEND_COLOR, kCache.m_akBlendColor.m_afRGBA);

	GetStateVar(GL_UNPACK_ROW_LENGTH, &kCache.m_iUnpackRowLength);
	GetStateVar(GL_UNPACK_IMAGE_HEIGHT, &kCache.m_iUnpackImageHeight);
	GetStateVar(GL_UNPACK_ALIGNMENT, &kCache.m_iUnpackAlignment);
	GetStateVar(GL_PACK_ROW_LENGTH, &kCache.m_iPackRowLength);
#if !DXGLES
	GetStateVar(GL_PACK_IMAGE_HEIGHT, &kCache.m_iPackImageHeight);
#endif //!DXGLES
	GetStateVar(GL_PACK_ALIGNMENT, &kCache.m_iPackAlignment);

#if DXGL_SUPPORT_TESSELLATION
	GetStateVar(GL_PATCH_VERTICES, &kCache.m_iNumPatchControlPoints);
#endif //DXGL_SUPPORT_TESSELLATION

#if !DXGLES
	kCache.m_bFrameBufferSRGBEnabled = glIsEnabled(GL_FRAMEBUFFER_SRGB) == GL_TRUE;
#endif //!DXGLES

#if DXGL_SUPPORT_VIEWPORT_ARRAY
	for (uint32 uViewport = 0; uViewport < DXGL_NUM_SUPPORTED_VIEWPORTS; ++uViewport)
	{
		GetStateVari(GL_VIEWPORT, uViewport, kCache.m_akViewportData + 4 * uViewport);
		GetStateVari(GL_DEPTH_RANGE, uViewport, kCache.m_akDepthRangeData + 2 * uViewport);
	}
#else
	GetStateVar(GL_VIEWPORT, kCache.m_akViewportData);
	GetStateVar(GL_DEPTH_RANGE, kCache.m_akDepthRangeData);
#endif

	for (uint32 uBufferBinding = 0; uBufferBinding < eBB_NUM; ++uBufferBinding)
	{
		GLuint uBufferBound(0);
		GetStateVar(GetBufferBindingPoint((EBufferBinding)uBufferBinding), &uBufferBound);
		kCache.m_akBuffersBound[uBufferBinding] = uBufferBound == 0 ? CResourceName() : m_pDevice->GetBufferNamePool().Create(uBufferBound);
	}

	const SCapabilities& kCapabilities = m_pDevice->GetAdapter()->m_kCapabilities;

	uint32 uNumUniformBuffers = kCapabilities.m_akResourceUnits[eRUT_UniformBuffer].m_aiMaxTotal;
	for (uint32 uIndex = 0; uIndex < uNumUniformBuffers; ++uIndex)
	{
		GLuint uBufferBound(0);
		GetStateVari(GL_UNIFORM_BUFFER_BINDING, uIndex, &uBufferBound);
		kCache.m_akUniformBuffersBound[uIndex] = uBufferBound == 0 ? CResourceName() : m_pDevice->GetBufferNamePool().Create(uBufferBound);
	}

#if DXGL_SUPPORT_SHADER_STORAGE_BLOCKS
	uint32 uNumStorageBuffers = kCapabilities.m_akResourceUnits[eRUT_StorageBuffer].m_aiMaxTotal;
	for (uint32 uIndex = 0; uIndex < uNumStorageBuffers; ++uIndex)
	{
		GLuint uBufferBound(0);
		GetStateVari(GL_SHADER_STORAGE_BUFFER_BINDING, uIndex, &uBufferBound);
		kCache.m_akStorageBuffersBound[uIndex] = uBufferBound == 0 ? CResourceName() : m_pDevice->GetBufferNamePool().Create(uBufferBound);
	}
#endif //DXGL_SUPPORT_SHADER_STORAGE_BLOCKS

	return true;
}

template<typename T>
inline bool RefreshCache(T& kCache, T kState)
{
	bool bDirty(kCache != kState);
	kCache = kState;
	return bDirty;
}

#define CACHE_VAR(_Cache, _State)            RefreshCache(_Cache, _State)
#define CACHE_FIELD(_Cache, _State, _Member) RefreshCache((_Cache)._Member, (_State)._Member)

struct SSetTargetIndependentBlendState
{
	static inline void SetBlendEnable(bool bEnable, GLuint)                                 { SetEnabledState(GL_BLEND, bEnable); }
	static inline void SetBlendEquation(GLenum eRGBA, GLuint)                               { glBlendEquation(eRGBA); }
	static inline void SetBlendFunction(SBlendFunction kRGBA, GLuint)                       { glBlendFunc(kRGBA.m_eSrc, kRGBA.m_eDst); }
	static inline void SetBlendEquation(GLenum eRGB, GLenum eAlpha, GLuint)                 { glBlendEquationSeparate(eRGB, eAlpha); }
	static inline void SetBlendFunction(SBlendFunction kRGB, SBlendFunction kAlpha, GLuint) { glBlendFuncSeparate(kRGB.m_eSrc, kRGB.m_eDst, kAlpha.m_eSrc, kAlpha.m_eDst); }
	static inline void SetWriteMask(const GLboolean abRGBA[4], GLuint)                      { glColorMask(abRGBA[0], abRGBA[1], abRGBA[2], abRGBA[3]); }
};
#if DXGL_SUPPORT_INDEPENDENT_BLEND_STATES
struct SSetTargetDependentBlendState
{
	static inline void SetBlendEnable(bool bEnable, GLuint uTarget)                                 { SetEnabledStatei(GL_BLEND, uTarget, bEnable); }
	static inline void SetBlendEquation(GLenum eRGBA, GLuint uTarget)                               { glBlendEquationi(uTarget, eRGBA); }
	static inline void SetBlendFunction(SBlendFunction kRGBA, GLuint uTarget)                       { glBlendFunci(uTarget, kRGBA.m_eSrc, kRGBA.m_eDst); }
	static inline void SetBlendEquation(GLenum eRGB, GLenum eAlpha, GLuint uTarget)                 { glBlendEquationSeparatei(uTarget, eRGB, eAlpha); }
	static inline void SetBlendFunction(SBlendFunction kRGB, SBlendFunction kAlpha, GLuint uTarget) { glBlendFuncSeparatei(uTarget, kRGB.m_eSrc, kRGB.m_eDst, kAlpha.m_eSrc, kAlpha.m_eDst); }
	static inline void SetWriteMask(const GLboolean abRGBA[4], GLuint uTarget)                      { glColorMaski(uTarget, abRGBA[0], abRGBA[1], abRGBA[2], abRGBA[3]); }
};
#endif //DXGL_SUPPORT_INDEPENDENT_BLEND_STATES
template<typename TSetBlendState>
void SetChannelsBlendState(STargetBlendState& kRTCache, const STargetBlendState& kRTState, GLuint uTargetIndex)
{
	if (CACHE_FIELD(kRTCache, kRTState, m_kRGB.m_eEquation))
		TSetBlendState::SetBlendEquation(kRTState.m_kRGB.m_eEquation, uTargetIndex);

	if (CACHE_FIELD(kRTCache, kRTState, m_kRGB.m_kFunction))
		TSetBlendState::SetBlendFunction(kRTState.m_kRGB.m_kFunction, uTargetIndex);
}

template<typename TSetBlendState>
void SetChannelsBlendStateSeparate(STargetBlendState& kRTCache, const STargetBlendState& kRTState, GLuint uTargetIndex)
{
	bool bNewRGBEquation(CACHE_FIELD(kRTCache, kRTState, m_kRGB.m_eEquation));
	bool bNewAlphaEquation(CACHE_FIELD(kRTCache, kRTState, m_kAlpha.m_eEquation));
	if (bNewRGBEquation || bNewAlphaEquation)
		TSetBlendState::SetBlendEquation(kRTState.m_kRGB.m_eEquation, kRTState.m_kAlpha.m_eEquation, uTargetIndex);

	bool bNewRGBFunction(CACHE_FIELD(kRTCache, kRTState, m_kRGB.m_kFunction));
	bool bNewAlphaFunction(CACHE_FIELD(kRTCache, kRTState, m_kAlpha.m_kFunction));
	if (bNewRGBFunction || bNewAlphaFunction)
		TSetBlendState::SetBlendFunction(kRTState.m_kRGB.m_kFunction, kRTState.m_kAlpha.m_kFunction, uTargetIndex);
}

template<typename TSetBlendState>
void SetTargetsBlendState(STargetBlendState* pRTCache, const STargetBlendState* pRTState, uint32 uNumTargets)
{
	uint32 uTarget;
	for (uTarget = 0; uTarget < uNumTargets; ++uTarget)
	{
		if (CACHE_FIELD(*pRTCache, *pRTState, m_bEnable))
			TSetBlendState::SetBlendEnable(pRTState->m_bEnable, uTarget);

		pRTCache->m_bSeparateAlpha = pRTState->m_bSeparateAlpha;

		if (CACHE_FIELD(*pRTCache, *pRTState, m_kWriteMask))
			TSetBlendState::SetWriteMask(pRTState->m_kWriteMask.m_abRGBA, uTarget);

		if (pRTState->m_bEnable)
		{
			if (pRTState->m_bSeparateAlpha)
				SetChannelsBlendStateSeparate<TSetBlendState>(*pRTCache, *pRTState, uTarget);
			else
				SetChannelsBlendState<TSetBlendState>(*pRTCache, *pRTState, uTarget);
		}

		++pRTCache;
		++pRTState;
	}
}

bool CContext::SetBlendState(const SBlendState& kState)
{
	DXGL_SCOPED_PROFILE("CContext::SetBlendState")

	if (CACHE_FIELD(m_kStateCache.m_kBlend, kState, m_bAlphaToCoverageEnable))
		SetEnabledState(GL_SAMPLE_ALPHA_TO_COVERAGE, kState.m_bAlphaToCoverageEnable);

#if DXGL_SUPPORT_INDEPENDENT_BLEND_STATES
	if (kState.m_bIndependentBlendEnable)
		SetTargetsBlendState<SSetTargetDependentBlendState>(m_kStateCache.m_kBlend.m_kTargets, kState.m_kTargets, DXGL_ARRAY_SIZE(kState.m_kTargets));
	else
#endif //DXGL_SUPPORT_INDEPENDENT_BLEND_STATES
	{
		SetTargetsBlendState<SSetTargetIndependentBlendState>(m_kStateCache.m_kBlend.m_kTargets, kState.m_kTargets, 1);
		uint32 uOverriddenTarget;
		for (uOverriddenTarget = 1; uOverriddenTarget < DXGL_ARRAY_SIZE(kState.m_kTargets); ++uOverriddenTarget)
		{
			m_kStateCache.m_kBlend.m_kTargets[uOverriddenTarget] = m_kStateCache.m_kBlend.m_kTargets[0];
		}
	}

	return true;
}

void CContext::SetSampleMask(GLbitfield aSampleMask)
{
	DXGL_SCOPED_PROFILE("CContext::SetSampleMask")

#if DXGL_SUPPORT_MULTISAMPLED_TEXTURES
	// Automatically enable/disable sample masking according to the mask value
	bool bSampleMaskEnabled(aSampleMask != ~(GLuint)0);
	if (CACHE_VAR(m_kStateCache.m_bSampleMaskEnabled, bSampleMaskEnabled))
		SetEnabledState(GL_SAMPLE_MASK, bSampleMaskEnabled);

	if (bSampleMaskEnabled)
	{
		if (CACHE_VAR(m_kStateCache.m_aSampleMask, aSampleMask))
			glSampleMaski(0, aSampleMask);
	}
#endif //DXGL_SUPPORT_MULTISAMPLED_TEXTURES
}

void CContext::SetBlendColor(GLfloat fRed, GLfloat fGreen, GLfloat fBlue, GLfloat fAlpha)
{
	DXGL_SCOPED_PROFILE("CContext::SetBlendColor")

	SColor kBlendColor;
	kBlendColor.m_afRGBA[0] = fRed;
	kBlendColor.m_afRGBA[1] = fGreen;
	kBlendColor.m_afRGBA[2] = fBlue;
	kBlendColor.m_afRGBA[3] = fAlpha;
	if (CACHE_VAR(m_kStateCache.m_akBlendColor, kBlendColor))
		glBlendColor(fRed, fGreen, fBlue, fAlpha);
}

void SetStencilState(
  GLenum eFace,
  SDepthStencilState::SFace& kDSCache, const SDepthStencilState::SFace& kDSState,
  GLint& iRefCache, GLint iRefState)
{
	if (CACHE_FIELD(kDSCache, kDSState, m_uStencilWriteMask))
		glStencilMaskSeparate(eFace, kDSState.m_uStencilWriteMask);

	DXGL_TODO("Verify that glStencilFuncSeparate works as intended: specification is (face, func...) , while glew declares as (frontfunc, backfunc...)");
	bool bNewStencilFunc(CACHE_FIELD(kDSCache, kDSState, m_eFunction));
	bool bNewRef(CACHE_VAR(iRefCache, iRefState));
	bool bNewStencilReadMask(CACHE_FIELD(kDSCache, kDSState, m_uStencilReadMask));
	if (bNewStencilFunc || bNewRef || bNewStencilReadMask)
		glStencilFuncSeparate(eFace, kDSState.m_eFunction, iRefState, kDSState.m_uStencilReadMask);

	bool bNewStencilFailOperation(CACHE_FIELD(kDSCache, kDSState, m_eStencilFailOperation));
	bool bNewDepthFailOperation(CACHE_FIELD(kDSCache, kDSState, m_eDepthFailOperation));
	bool bNewDepthPassOperation(CACHE_FIELD(kDSCache, kDSState, m_eDepthPassOperation));
	if (bNewStencilFailOperation || bNewDepthFailOperation || bNewDepthPassOperation)
		glStencilOpSeparate(eFace, kDSState.m_eStencilFailOperation, kDSState.m_eDepthFailOperation, kDSState.m_eDepthPassOperation);
}

bool CContext::SetDepthStencilState(
  const SDepthStencilState& kDSState,
  GLint iStencilRef)
{
	DXGL_SCOPED_PROFILE("CContext::SetDepthStencilState")

	SDepthStencilCache & kDSCache(m_kStateCache.m_kDepthStencil);
	SStencilRefCache& kRefCache(m_kStateCache.m_kStencilRef);

	if (CACHE_FIELD(kDSCache, kDSState, m_bDepthTestingEnabled))
		SetEnabledState(GL_DEPTH_TEST, kDSState.m_bDepthTestingEnabled);

	if (kDSState.m_bDepthTestingEnabled)
	{
		if (CACHE_FIELD(kDSCache, kDSState, m_eDepthTestFunc))
			glDepthFunc(kDSState.m_eDepthTestFunc);
	}

	if (CACHE_FIELD(kDSCache, kDSState, m_bDepthWriteMask))
		glDepthMask(kDSState.m_bDepthWriteMask);

	if (CACHE_FIELD(kDSCache, kDSState, m_bStencilTestingEnabled))
		SetEnabledState(GL_STENCIL_TEST, kDSState.m_bStencilTestingEnabled);

	if (kDSState.m_bStencilTestingEnabled)
	{
		SetStencilState(GL_BACK, kDSCache.m_kStencilBackFaces, kDSState.m_kStencilBackFaces, kRefCache.m_iBackFacesReference, iStencilRef);
		SetStencilState(GL_FRONT, kDSCache.m_kStencilFrontFaces, kDSState.m_kStencilFrontFaces, kRefCache.m_iFrontFacesReference, iStencilRef);
	}

	return true;
}

bool CContext::SetRasterizerState(const SRasterizerState& kState)
{
	DXGL_SCOPED_PROFILE("CContext::SetRasterizerState")

	SRasterizerCache & kCache(m_kStateCache.m_kRasterizer);

	if (CACHE_FIELD(kCache, kState, m_bCullingEnabled))
		SetEnabledState(GL_CULL_FACE, kState.m_bCullingEnabled);

	if (kState.m_bCullingEnabled)
	{
		if (CACHE_FIELD(kCache, kState, m_eCullFaceMode))
			glCullFace(kState.m_eCullFaceMode);
	}

	if (CACHE_FIELD(kCache, kState, m_eFrontFaceMode))
		glFrontFace(kState.m_eFrontFaceMode);

	kCache.m_bPolygonOffsetEnabled = kState.m_bPolygonOffsetEnabled;
#if !DXGLES
	switch (kState.m_ePolygonMode)
	{
	case GL_FILL:
		if (CACHE_VAR(kCache.m_bPolygonOffsetFillEnabled, kState.m_bPolygonOffsetEnabled))
			SetEnabledState(GL_POLYGON_OFFSET_FILL, kState.m_bPolygonOffsetEnabled);
		break;
	case GL_LINE:
		if (CACHE_VAR(kCache.m_bPolygonOffsetLineEnabled, kState.m_bPolygonOffsetEnabled))
			SetEnabledState(GL_POLYGON_OFFSET_LINE, kState.m_bPolygonOffsetEnabled);
		break;
	default:
		DXGL_WARNING("Unexpected value for GL_POLYGON_MODE - should be GL_LINE or GL_FILL");
		break;
	}
#else
	if (CACHE_VAR(kCache.m_bPolygonOffsetFillEnabled, kState.m_bPolygonOffsetEnabled))
		SetEnabledState(GL_POLYGON_OFFSET_FILL, kState.m_bPolygonOffsetEnabled);
#endif

	if (kState.m_bPolygonOffsetEnabled)
	{
		bool bNewUnits(CACHE_FIELD(kCache, kState, m_fPolygonOffsetUnits));
		bool bNewFactor(CACHE_FIELD(kCache, kState, m_fPolygonOffsetFactor));
		if (bNewUnits || bNewFactor)
			glPolygonOffset(kState.m_fPolygonOffsetFactor, kState.m_fPolygonOffsetUnits);
	}

	if (CACHE_FIELD(kCache, kState, m_bScissorEnabled))
		SetEnabledState(GL_SCISSOR_TEST, kState.m_bScissorEnabled);

	bool bDepthClamp(!kState.m_bDepthClipEnabled);
#if !DXGLES
	if (CACHE_FIELD(kCache, kState, m_bMultisampleEnabled))
		SetEnabledState(GL_MULTISAMPLE, kState.m_bMultisampleEnabled);
#endif

#if DXGL_SUPPORT_DEPTH_CLAMP
	if (CACHE_FIELD(kCache, kState, m_bDepthClipEnabled))
		SetEnabledState(GL_DEPTH_CLAMP, bDepthClamp);
#else
	if (CACHE_VAR(m_kPipelineConfiguration.m_bEmulateDepthClamp, bDepthClamp))
		m_bPipelineDirty = true;
#endif

#if !DXGLES && !defined(DXGL_ANDROID_GL)
	if (CACHE_FIELD(kCache, kState, m_ePolygonMode))
		glPolygonMode(GL_FRONT_AND_BACK, kState.m_ePolygonMode);

	if (CACHE_FIELD(kCache, kState, m_bLineSmoothEnabled))
		SetEnabledState(GL_LINE_SMOOTH, kState.m_bLineSmoothEnabled);
#endif //!DXGLES && !defined(DXGL_ANDROID_GL)

	return true;
}

void CContext::FlushTextureUnits()
{
	DXGL_SCOPED_PROFILE("CContext::FlushTextureUnits");

	const SUnitMap* pTextureUnitMap = m_apResourceUnitMaps[eRUT_Texture];

	if (m_abResourceUnitsDirty[eRUT_Texture] && pTextureUnitMap != NULL)
	{
		m_kTextureUnitContext.m_kModifiedTextures.clear();
		uint32 uNumMapElements(pTextureUnitMap->m_uNumUnits);

		for (uint32 uMapElement = 0; uMapElement < uNumMapElements; ++uMapElement)
		{
			SUnitMap::SElement kMapElement(pTextureUnitMap->m_akUnits[uMapElement]);
			uint32 uUnit(kMapElement.GetTextureUnit());
			uint32 uTextureSlot(kMapElement.GetTextureSlot());

			assert(uTextureSlot < MAX_TEXTURE_SLOTS);
			assert(uUnit < MAX_TEXTURE_UNITS);

			STextureUnitCache& kCache(m_kStateCache.m_akTextureUnits[uUnit]);
			SShaderTextureBasedView* pTextureView(m_akTextureSlots[uTextureSlot].m_pView);

			SSamplerState* pSamplerState(NULL);
			if (kMapElement.HasSampler())
			{
				uint32 uSamplerSlot(kMapElement.GetSamplerSlot());
				assert(uSamplerSlot < MAX_SAMPLER_SLOTS);
				pSamplerState = m_akSamplerSlots[uSamplerSlot].m_pSampler;
			}

			SSamplerState kNullSamplerState;
			if (pSamplerState == NULL)
			{
				kNullSamplerState.m_uSamplerObjectMip = 0;
				kNullSamplerState.m_uSamplerObjectNoMip = 0;
				pSamplerState = &kNullSamplerState;
			}

			if (pTextureView != NULL &&
			    pTextureView->BindTextureUnit(pSamplerState, m_kTextureUnitContext, this))
			{
				bool bNewTexture(CACHE_FIELD(kCache, m_kTextureUnitContext.m_kCurrentUnitState, m_kTextureName));
				bool bNewTarget(CACHE_FIELD(kCache, m_kTextureUnitContext.m_kCurrentUnitState, m_eTextureTarget));
				if (bNewTexture || bNewTarget)
				{
					glActiveTexture(GL_TEXTURE0 + uUnit);
					glBindTexture(kCache.m_eTextureTarget, kCache.m_kTextureName.GetName());
				}

				if (CACHE_FIELD(kCache, m_kTextureUnitContext.m_kCurrentUnitState, m_uSampler))
					glBindSampler(uUnit, kCache.m_uSampler);
			}
			else
			{
				GLenum eOldTarget(kCache.m_eTextureTarget);
				if (CACHE_VAR(kCache.m_kTextureName, CResourceName()) && eOldTarget != 0)
				{
					glActiveTexture(GL_TEXTURE0 + uUnit);
					glBindTexture(eOldTarget, 0);
				}
			}
		}

		uint32 uTexReset;
		for (uTexReset = 0; uTexReset < m_kTextureUnitContext.m_kModifiedTextures.size(); ++uTexReset)
		{
			m_kTextureUnitContext.m_kModifiedTextures.at(uTexReset)->m_pBoundModifier = NULL;
		}

		m_abResourceUnitsDirty[eRUT_Texture] = false;
	}
}

void CContext::FlushUniformBufferUnits()
{
	DXGL_SCOPED_PROFILE("CContext::FlushUniformBufferUnits");

	bool bUniformBuffersDirty = m_abResourceUnitsDirty[eRUT_UniformBuffer];
#if DXGL_STREAMING_CONSTANT_BUFFERS
	bUniformBuffersDirty |= SGlobalConfig::iStreamingConstantBuffersMode > 0;
#endif

	const SUnitMap* pUniformBufferUnitMap = m_apResourceUnitMaps[eRUT_UniformBuffer];
	if (bUniformBuffersDirty && pUniformBufferUnitMap != NULL)
	{
		uint32 uNumUnits = pUniformBufferUnitMap->m_uNumUnits;
		const SUnitMap::SElement* pUnitsBegin = pUniformBufferUnitMap->m_akUnits;
		const SUnitMap::SElement* pUnitsEnd = pUnitsBegin + uNumUnits;
		for (const SUnitMap::SElement* pUnit = pUnitsBegin; pUnit < pUnitsEnd; ++pUnit)
		{
			SUnitMap::SElement kUnit = *pUnit;
			uint16 uSlot = kUnit.GetResourceSlot();
			uint16 uUnit = kUnit.GetResourceUnit();

			assert(uSlot < MAX_CONSTANT_BUFFER_SLOTS);
			assert(uUnit < MAX_UNIFORM_BUFFER_UNITS);

#if DXGL_STREAMING_CONSTANT_BUFFERS
			if (SGlobalConfig::iStreamingConstantBuffersMode > 0)
				m_kStreamingBuffers.UploadAndBindUniformData(this, m_akConstantBufferSlots[uSlot], uUnit);
			else
			{
				SBuffer* pBuffer = m_akConstantBufferSlots[uSlot].m_pBuffer;
				if (pBuffer == NULL)
					BindUniformBuffer(TIndexedBufferBinding(), uUnit);
				else
				{
					pBuffer->m_kCreationFence.IssueWait(this);
					BindUniformBuffer(TIndexedBufferBinding(pBuffer->m_kName, m_akConstantBufferSlots[uSlot].m_kRange), uUnit);
				}
			}
#else
			BindUniformBuffer(m_akConstantBufferSlots[uSlot], uUnit);
#endif
		}
	}

#if DXGL_STREAMING_CONSTANT_BUFFERS
	if (SGlobalConfig::iStreamingConstantBuffersMode > 0)
		m_kStreamingBuffers.FlushUniformData();
#endif
}

#if DXGL_SUPPORT_SHADER_STORAGE_BLOCKS

void CContext::FlushStorageBufferUnits()
{
	DXGL_SCOPED_PROFILE("CContext::FlushStorageBufferUnits");

	const SUnitMap* pStorageBufferUnitMap = m_apResourceUnitMaps[eRUT_StorageBuffer];

	if (m_abResourceUnitsDirty[eRUT_StorageBuffer] && pStorageBufferUnitMap != NULL)
	{
		uint32 uNumUnits = pStorageBufferUnitMap->m_uNumUnits;
		const SUnitMap::SElement* pUnitsBegin = pStorageBufferUnitMap->m_akUnits;
		const SUnitMap::SElement* pUnitsEnd = pUnitsBegin + uNumUnits;
		for (const SUnitMap::SElement* pUnit = pUnitsBegin; pUnit < pUnitsEnd; ++pUnit)
		{
			SUnitMap::SElement kUnit = *pUnit;
			uint16 uSlot = kUnit.GetResourceSlot();
			uint16 uUnit = kUnit.GetResourceUnit();

			assert(uSlot < MAX_STORAGE_BUFFER_SLOTS);
			assert(uUnit < MAX_STORAGE_BUFFER_UNITS);

			BindStorageBuffer(m_akStorageBufferSlots[uSlot], uUnit);
		}
	}
}

#endif //DXGL_SUPPORT_SHADER_STORAGE_BLOCKS

#if DXGL_SUPPORT_SHADER_IMAGES

void CContext::FlushImageUnits()
{
	DXGL_SCOPED_PROFILE("CContext::FlushImageUnits");

	const SUnitMap* pImageUnitMap = m_apResourceUnitMaps[eRUT_Image];

	if (m_abResourceUnitsDirty[eRUT_Image] && pImageUnitMap != NULL)
	{
		uint32 uNumUnits = pImageUnitMap->m_uNumUnits;
		const SUnitMap::SElement* pUnitsBegin = pImageUnitMap->m_akUnits;
		const SUnitMap::SElement* pUnitsEnd = pUnitsBegin + uNumUnits;
		for (const SUnitMap::SElement* pUnit = pUnitsBegin; pUnit < pUnitsEnd; ++pUnit)
		{
			SUnitMap::SElement kUnit = *pUnit;
			uint16 uSlot = kUnit.GetResourceSlot();
			uint16 uUnit = kUnit.GetResourceUnit();

			assert(uSlot < MAX_IMAGE_SLOTS);
			assert(uUnit < MAX_IMAGE_UNITS);

			const SImageUnitCache& kSlot(m_akImageSlots[uSlot]);
			BindImage(kSlot.m_kTextureName, kSlot.m_kConfiguration, uUnit);
		}
	}
}

#endif //DXGL_SUPPORT_SHADER_IMAGES

#if DXGL_SUPPORT_VERTEX_ATTRIB_BINDING

void CContext::FlushInputAssemblerStateVab()
{
	DXGL_SCOPED_PROFILE("CContext::FlushInputAssemblerStateVab")

	if (m_bInputLayoutDirty)
	{
		if (m_pInputLayout != NULL)
		{
			SInputAssemblerCache::TAttributeBitMask uAttribsEnabled(0);

			GLuint auUpdatedDivisors[MAX_VERTEX_ATTRIB_BINDINGS] = { 0 };

			SInputAssemblerCache::SVertexAttribFormat aUpdatedVertexAttribFormats[MAX_VERTEX_ATTRIBUTES] = {
				{ 0 }
			};
			GLuint aUpdatedVertexBindingIndices[MAX_VERTEX_ATTRIBUTES] = { 0 };

			for (uint32 uSlot = 0; uSlot < m_pInputLayout->m_akVertexAttribFormats.size(); ++uSlot)
			{
				const SInputLayout::SAttributeFormats& kAttrib(m_pInputLayout->m_akVertexAttribFormats[uSlot]);
				SInputAssemblerCache::SVertexAttribFormat& kUpdatedVertexAttribFormat(aUpdatedVertexAttribFormats[kAttrib.m_uAttributeIndex]);

				uAttribsEnabled |= (1 << kAttrib.m_uAttributeIndex);

				kUpdatedVertexAttribFormat.m_bInteger = kAttrib.m_bInteger;
				kUpdatedVertexAttribFormat.m_bNormalized = kAttrib.m_bNormalized;
				kUpdatedVertexAttribFormat.m_uRelativeOffset = kAttrib.m_uPointerOffset;
				kUpdatedVertexAttribFormat.m_eType = kAttrib.m_eType;
				kUpdatedVertexAttribFormat.m_iSize = kAttrib.m_iDimension;

				aUpdatedVertexBindingIndices[kAttrib.m_uAttributeIndex] = kAttrib.m_uBindingIndex;

				auUpdatedDivisors[kAttrib.m_uBindingIndex] = kAttrib.m_uVertexAttribDivisor;
			}

			SInputAssemblerCache& kCache(m_kStateCache.m_kInputAssembler);

			// loop through all bindings since we don't know how many actually will have VBs bound to them later on.
			{
				const GLint iNumVertexAttribBindings(m_pDevice->GetAdapter()->m_kCapabilities.m_iMaxVertexAttribBindings);
				for (uint32 uBindingSlot = 0; uBindingSlot < iNumVertexAttribBindings; ++uBindingSlot)
				{
					if (CACHE_VAR(kCache.m_auVertexBindingDivisors[uBindingSlot], auUpdatedDivisors[uBindingSlot]))
					{
						glVertexBindingDivisor(uBindingSlot, auUpdatedDivisors[uBindingSlot]);
					}
				}
			}

			// toggle attribute enables/disables for those that we changed
			SInputAssemblerCache::TAttributeBitMask uAttribEnabledChanged(kCache.m_uVertexAttribsEnabled ^ uAttribsEnabled);

			if (uAttribEnabledChanged)
			{
				const uint32 uMaxChangedVertexAttribIndex = m_pDevice->GetAdapter()->m_kCapabilities.m_iMaxVertexAttribs;

				for (uint32 uAttribIndex = 0; uAttribIndex < uMaxChangedVertexAttribIndex; ++uAttribIndex)
				{
					const uint32 uAttribEnabledBit = 1 << uAttribIndex;

					if (uAttribEnabledChanged & uAttribEnabledBit)
					{
						if (uAttribsEnabled & uAttribEnabledBit)
							glEnableVertexAttribArray(uAttribIndex);
						else
							glDisableVertexAttribArray(uAttribIndex);
					}
				}
				kCache.m_uVertexAttribsEnabled = uAttribsEnabled;
			}

			// now update the format and bindings for *all* enabled vertex attribs (and not just those that we might just have enabled)
			{
				const GLint iNumVertexAttribs(m_pDevice->GetAdapter()->m_kCapabilities.m_iMaxVertexAttribs);
				for (uint32 uAttribIndex = 0; uAttribIndex < iNumVertexAttribs; ++uAttribIndex)
				{
					const SInputAssemblerCache::SVertexAttribFormat& kUpdatedVertexAttribFormat = aUpdatedVertexAttribFormats[uAttribIndex];
					const uint32 uAttribEnabledBit = 1 << uAttribIndex;
					const SInputAssemblerCache::TAttributeBitMask uEnabledAttribs = kCache.m_uVertexAttribsEnabled;
					if (uEnabledAttribs & uAttribEnabledBit)
					{
						if (CACHE_VAR(kCache.m_aVertexAttribFormats[uAttribIndex], kUpdatedVertexAttribFormat))
						{
							if (kUpdatedVertexAttribFormat.m_bInteger)
								glVertexAttribIFormat(uAttribIndex, kUpdatedVertexAttribFormat.m_iSize, kUpdatedVertexAttribFormat.m_eType, kUpdatedVertexAttribFormat.m_uRelativeOffset);
							else
								glVertexAttribFormat(uAttribIndex, kUpdatedVertexAttribFormat.m_iSize, kUpdatedVertexAttribFormat.m_eType, kUpdatedVertexAttribFormat.m_bNormalized, kUpdatedVertexAttribFormat.m_uRelativeOffset);
						}

						if (CACHE_VAR(kCache.m_auVertexBindingIndices[uAttribIndex], aUpdatedVertexBindingIndices[uAttribIndex]))
						{
							glVertexAttribBinding(uAttribIndex, aUpdatedVertexBindingIndices[uAttribIndex]);
						}
					}
				}
			}
		}

		m_bInputLayoutDirty = false;
	}

	if (m_bInputAssemblerSlotsDirty)
	{
		GLuint auUpdatedVertexBindingBuffers[MAX_VERTEX_ATTRIB_BINDINGS] = { 0 };
		GLintptr auUpdatedVertexBindingOffsets[MAX_VERTEX_ATTRIB_BINDINGS] = { 0 };
		GLsizei auUpdatedVertexBindingStrides[MAX_VERTEX_ATTRIB_BINDINGS] = { 0 };

		GLint iLastNonZeroBindingSlot = -1;

		for (int uSlot = 0; uSlot < MAX_VERTEX_ATTRIB_BINDINGS; ++uSlot)
		{
			SInputAssemblerSlot& kSlot(m_akInputAssemblerSlots[uSlot]);

			if (kSlot.m_pVertexBuffer)
			{
				auUpdatedVertexBindingBuffers[uSlot] = kSlot.m_pVertexBuffer->m_kName.GetName();
				auUpdatedVertexBindingOffsets[uSlot] = GLintptr(kSlot.m_uOffset);
				auUpdatedVertexBindingStrides[uSlot] = kSlot.m_uStride;
				iLastNonZeroBindingSlot = uSlot;
			}
		}

		SInputAssemblerCache& kCache(m_kStateCache.m_kInputAssembler);

		const GLuint uFirstSlotToUpdate = 0;
		GLuint uBindingSlotsToUpdate = std::max<GLint>(iLastNonZeroBindingSlot, kCache.m_iLastNonZeroBindingSlot) + 1;
		kCache.m_iLastNonZeroBindingSlot = iLastNonZeroBindingSlot;

		if (uBindingSlotsToUpdate)
		{
			if (SupportMultiBind())
				glBindVertexBuffers(uFirstSlotToUpdate, uBindingSlotsToUpdate, auUpdatedVertexBindingBuffers, auUpdatedVertexBindingOffsets, auUpdatedVertexBindingStrides);
			else
			{
				for (GLuint uSlot = 0; uSlot < uBindingSlotsToUpdate; ++uSlot)
				{
					glBindVertexBuffer(uFirstSlotToUpdate + uSlot, auUpdatedVertexBindingBuffers[uSlot], auUpdatedVertexBindingOffsets[uSlot], auUpdatedVertexBindingStrides[uSlot]);
				}
			}
		}
		m_bInputAssemblerSlotsDirty = false;
	}
}

#endif //DXGL_SUPPORT_VERTEX_ATTRIB_BINDING

void CContext::FlushInputAssemblerState()
{
	DXGL_SCOPED_PROFILE("CContext::FlushInputAssemblerState")

	if (m_bInputLayoutDirty || m_bInputAssemblerSlotsDirty)
	{
		SInputAssemblerCache& kCache(m_kStateCache.m_kInputAssembler);

		uint32 uAttribute(0);
		uint32 uSlot;
		SInputAssemblerCache::TAttributeBitMask uAttribsEnabled(0);
		if (m_pInputLayout != NULL)
		{
			for (uSlot = 0; uSlot < DXGL_ARRAY_SIZE(m_akInputAssemblerSlots); ++uSlot)
			{
				if (m_akInputAssemblerSlots[uSlot].m_pVertexBuffer != NULL)
				{
					BindBuffer(m_akInputAssemblerSlots[uSlot].m_pVertexBuffer, eBB_Array);

					SInputLayout::TAttributes::const_iterator kAttrIter(m_pInputLayout->m_akSlotAttributes[uSlot].begin());
					const SInputLayout::TAttributes::const_iterator kAttrEnd(m_pInputLayout->m_akSlotAttributes[uSlot].end());
					for (; kAttrIter != kAttrEnd; ++kAttrIter)
					{
						SInputAssemblerCache::TAttributeBitMask uFlag(1 << kAttrIter->m_uAttributeIndex);
						uAttribsEnabled |= uFlag;
						if ((kCache.m_uVertexAttribsEnabled & uFlag) == 0)
							glEnableVertexAttribArray(kAttrIter->m_uAttributeIndex);

						if (CACHE_VAR(kCache.m_auVertexAttribDivisors[kAttrIter->m_uAttributeIndex], kAttrIter->m_uVertexAttribDivisor))
							glVertexAttribDivisor(kAttrIter->m_uAttributeIndex, kAttrIter->m_uVertexAttribDivisor);

						GLuint uStride(m_akInputAssemblerSlots[uSlot].m_uStride);
						GLvoid* pPointer(reinterpret_cast<GLvoid*>(static_cast<uintptr_t>(m_akInputAssemblerSlots[uSlot].m_uOffset + kAttrIter->m_uPointerOffset)));
#if !DXGL_SUPPORT_DRAW_WITH_BASE_VERTEX
						pPointer = reinterpret_cast<GLbyte*>(pPointer) + m_uVertexOffset * uStride;
#endif        //!DXGL_SUPPORT_DRAW_WITH_BASE_VERTEX
						if (!kAttrIter->m_bInteger)
							glVertexAttribPointer(
							  kAttrIter->m_uAttributeIndex,
							  kAttrIter->m_iDimension,
							  kAttrIter->m_eType,
							  kAttrIter->m_bNormalized,
							  uStride,
							  pPointer);
						else
							glVertexAttribIPointer(
							  kAttrIter->m_uAttributeIndex,
							  kAttrIter->m_iDimension,
							  kAttrIter->m_eType,
							  uStride,
							  pPointer);
					}
				}
			}
		}

		SInputAssemblerCache::TAttributeBitMask uToDisable(kCache.m_uVertexAttribsEnabled & ~uAttribsEnabled);

		for (uSlot = 0; uToDisable != 0; ++uSlot)
		{
			if ((uToDisable & 1) != 0)
				glDisableVertexAttribArray(uSlot);
			uToDisable = uToDisable >> 1;
		}

		kCache.m_uVertexAttribsEnabled = uAttribsEnabled;

		m_bInputLayoutDirty = false;
		m_bInputAssemblerSlotsDirty = false;
	}
}

void CContext::FlushFrameBufferState()
{
	DXGL_SCOPED_PROFILE("CContext::FlushFrameBufferState")

	if (m_bFrameBufferStateDirty)
	{
		m_spFrameBuffer = AllocateFrameBuffer(m_kFrameBufferConfig);

		if (m_spFrameBuffer != NULL)
		{
			BindDrawFrameBuffer(m_spFrameBuffer->m_kObject.m_kName);
#if !DXGLES
			if (CACHE_VAR(m_kStateCache.m_bFrameBufferSRGBEnabled, m_spFrameBuffer->m_kObject.m_bUsesSRGB))
				SetEnabledState(GL_FRAMEBUFFER_SRGB, m_spFrameBuffer->m_kObject.m_bUsesSRGB);
#endif  //!DXGLES
			if (CACHE_VAR(m_spFrameBuffer->m_kObject.m_kDrawMaskCache, m_spFrameBuffer->m_kDrawMask))
				glDrawBuffers(m_spFrameBuffer->m_uNumDrawBuffers, m_spFrameBuffer->m_aeDrawBuffers);

			uint32 uAttachment;
			for (uAttachment = 0; uAttachment < SFrameBufferConfiguration::MAX_ATTACHMENTS; ++uAttachment)
			{
				SOutputMergerView* pAttachedView(m_spFrameBuffer->m_kConfiguration.m_akAttachments[uAttachment]);
				if (pAttachedView != NULL)
					pAttachedView->Bind(*m_spFrameBuffer, this);
			}

			m_bFrameBufferStateDirty = false;
		}
	}
}

void CContext::FlushPipelineState()
{
	DXGL_SCOPED_PROFILE("CContext::FlushPipelineState")

	if (m_bPipelineDirty)
	{
		m_spPipeline = AllocatePipeline(m_kPipelineConfiguration);
		if (m_spPipeline != NULL)
		{
#if DXGL_VALIDATE_PROGRAMS_ON_DRAW
			if (SupportSeparablePrograms())
			{
				glValidateProgramPipeline(m_spPipeline->m_uName);
				VerifyProgramPipelineStatus(m_spPipeline->m_uName, GL_VALIDATE_STATUS);
			}
			else
			{
				glValidateProgram(m_spPipeline->m_uName);
				VerifyProgramStatus(m_spPipeline->m_uName, GL_VALIDATE_STATUS);
			}
#endif  //DXGL_VALIDATE_PROGRAMS_ON_DRAW

			if (SupportSeparablePrograms())
				glBindProgramPipeline(m_spPipeline->m_uName);
			else
				glUseProgram(m_spPipeline->m_uName);

			for (uint32 uUnitType = 0; uUnitType < eRUT_NUM; ++uUnitType)
			{
				SUnitMap* pUnitMap = m_spPipeline->m_aspResourceUnitMaps[uUnitType];
				if (pUnitMap != NULL)
					m_abResourceUnitsDirty[uUnitType] |= CACHE_VAR(m_apResourceUnitMaps[uUnitType], pUnitMap);
			}
		}
		else
		{
			for (uint32 uUnitType = 0; uUnitType < eRUT_NUM; ++uUnitType)
				m_apResourceUnitMaps[uUnitType] = NULL;
		}

		m_bPipelineDirty = false;
	}
}

void CContext::FlushDrawState()
{
	DXGL_SCOPED_PROFILE("CContext::FlushDrawState")

	if (m_pDevice->FlushFrameFence(m_uIndex))
	{
#if DXGL_STREAMING_CONSTANT_BUFFERS
		if (SGlobalConfig::iStreamingConstantBuffersMode > 0)
		{
			m_kStreamingBuffers.SwitchFrame(m_pDevice);
		}
#endif //DXGL_STREAMING_CONSTANT_BUFFERS
#if DXGL_ENABLE_SHADER_TRACING
		bool bAutoVertexTracing = SGlobalConfig::iShaderTracingMode == 1 && m_uShaderTraceCount == 0;
		bool bAutoPixelTracing = SGlobalConfig::iShaderTracingMode == 2 && m_uShaderTraceCount == 0;
		bool bAutoComputeTracing = SGlobalConfig::iShaderTracingMode == 3 && m_uShaderTraceCount == 0;
		ToggleVertexTracing(
		  bAutoVertexTracing, SGlobalConfig::iShaderTracingHash,
		  SGlobalConfig::iVertexTracingID);
		TogglePixelTracing(
		  bAutoPixelTracing, SGlobalConfig::iShaderTracingHash,
		  SGlobalConfig::iPixelTracingX, SGlobalConfig::iPixelTracingY);
		ToggleComputeTracing(
		  bAutoComputeTracing, SGlobalConfig::iShaderTracingHash,
		  SGlobalConfig::iComputeTracingWorkGroupIDX, SGlobalConfig::iComputeTracingWorkGroupIDY, SGlobalConfig::iComputeTracingWorkGroupIDZ,
		  SGlobalConfig::iComputeTracingLocalInvocationIDX, SGlobalConfig::iComputeTracingLocalInvocationIDY, SGlobalConfig::iComputeTracingLocalInvocationIDZ);
#endif //DXGL_ENABLE_SHADER_TRACING
	}

#if DXGL_ENABLE_SHADER_TRACING
	FlushShaderTracingState();
#endif

	if (CACHE_VAR(m_kPipelineConfiguration.m_eMode, ePM_Graphics))
		m_bPipelineDirty = true;

#if DXGL_SUPPORT_VERTEX_ATTRIB_BINDING
	if (SupportVertexAttribBinding())
		FlushInputAssemblerStateVab();
	else
#endif //DXGL_SUPPORT_VERTEX_ATTRIB_BINDING
	FlushInputAssemblerState();

	FlushPipelineState();
	FlushTextureUnits();
	FlushUniformBufferUnits();
#if DXGL_SUPPORT_SHADER_STORAGE_BLOCKS
	FlushStorageBufferUnits();
#endif
#if DXGL_SUPPORT_SHADER_IMAGES
	FlushImageUnits();
#endif
	FlushFrameBufferState();
}

#if DXGL_SUPPORT_COMPUTE

void CContext::FlushDispatchState()
{
	DXGL_SCOPED_PROFILE("CContext::FlushDispatchState")

	#if DXGL_ENABLE_SHADER_TRACING
	FlushShaderTracingState();
	#endif

	if (CACHE_VAR(m_kPipelineConfiguration.m_eMode, ePM_Compute))
		m_bPipelineDirty = true;

	FlushPipelineState();
	FlushTextureUnits();
	FlushUniformBufferUnits();
	#if DXGL_SUPPORT_SHADER_STORAGE_BLOCKS
	FlushStorageBufferUnits();
	#endif
	#if DXGL_SUPPORT_SHADER_IMAGES
	FlushImageUnits();
	#endif
}

#endif //DXGL_SUPPORT_COMPUTE

void CContext::SwitchFrame()
{
#if DXGL_STREAMING_CONSTANT_BUFFERS
	if (SGlobalConfig::iStreamingConstantBuffersMode > 0)
	{
		m_kStreamingBuffers.SwitchFrame(m_pDevice);
	}
#endif //DXGL_STREAMING_CONSTANT_BUFFERS
}

void CContext::BindDrawFrameBuffer(const CResourceName& kName)
{
	DXGL_SCOPED_PROFILE("CContext::BindDrawFrameBuffer")

	if (CACHE_VAR(m_kStateCache.m_kDrawFrameBuffer, kName))
	{
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, kName.GetName());
		m_bFrameBufferStateDirty = true;
	}
}

void CContext::BindReadFrameBuffer(const CResourceName& kName)
{
	DXGL_SCOPED_PROFILE("CContext::BindReadFrameBuffer")
	if (CACHE_VAR(m_kStateCache.m_kReadFrameBuffer, kName))
		glBindFramebuffer(GL_READ_FRAMEBUFFER, kName.GetName());
}

void CContext::SetNumPatchControlPoints(GLint iNumPatchControlPoints)
{
	DXGL_SCOPED_PROFILE("CContext::SetNumPatchControlPoints");

#if DXGL_SUPPORT_TESSELLATION
	if (CACHE_VAR(m_kStateCache.m_iNumPatchControlPoints, iNumPatchControlPoints))
		glPatchParameteri(GL_PATCH_VERTICES, iNumPatchControlPoints);
#else
	DXGL_WARNING("CContext::SetNumPatchControlPoints - OpenGL(ES) Version does not support tesselation");
#endif
}

#if !DXGL_SUPPORT_DRAW_WITH_BASE_VERTEX

void CContext::SetVertexOffset(uint32 uVertexOffset)
{
	if (CACHE_VAR(m_uVertexOffset, uVertexOffset))
		m_bInputLayoutDirty = true;
}

#endif //!DXGL_SUPPORT_DRAW_WITH_BASE_VERTEX

GLenum CContext::BindBuffer(const CResourceName& kBufferName, EBufferBinding eBinding)
{
	DXGL_SCOPED_PROFILE("CContext::BindBuffer")

	GLenum eTarget(GetBufferBindingTarget(eBinding));
	assert(eTarget != 0);
	if (CACHE_VAR(m_kStateCache.m_akBuffersBound[eBinding], kBufferName))
		glBindBuffer(eTarget, kBufferName.GetName());
	return eTarget;
}

GLenum CContext::BindBuffer(SBuffer* pBuffer, EBufferBinding eBinding)
{
	DXGL_SCOPED_PROFILE("CContext::BindBuffer")

	if (pBuffer == NULL)
		return BindBuffer(CResourceName(), eBinding);
	else
	{
		pBuffer->m_kCreationFence.IssueWait(this);
		return BindBuffer(pBuffer->m_kName, eBinding);
	}
}

void CContext::BindUniformBuffer(const TIndexedBufferBinding& kBinding, uint32 uUnit)
{
	if (CACHE_VAR(m_kStateCache.m_akUniformBuffersBound[uUnit], kBinding))
	{
		//Indexed glBindBufferBase/glBindBufferRange also internally changes the general GL_UNIFORM_BUFFER binding
		m_kStateCache.m_akBuffersBound[eBB_UniformBuffer] = kBinding.m_kName;

		if (kBinding.m_kRange.m_uOffset == 0 && kBinding.m_kRange.m_uSize == 0)
			glBindBufferBase(GL_UNIFORM_BUFFER, uUnit, kBinding.m_kName.GetName());
		else
			glBindBufferRange(GL_UNIFORM_BUFFER, uUnit, kBinding.m_kName.GetName(), kBinding.m_kRange.m_uOffset, kBinding.m_kRange.m_uSize);
	}
}

#if DXGL_SUPPORT_SHADER_STORAGE_BLOCKS

void CContext::BindStorageBuffer(const TIndexedBufferBinding& kBinding, uint32 uUnit)
{
	if (CACHE_VAR(m_kStateCache.m_akStorageBuffersBound[uUnit], kBinding))
	{
		//Indexed glBindBufferBase/glBindBufferRange also internally changes the general GL_SHADER_STORAGE_BUFFER binding
		m_kStateCache.m_akBuffersBound[eBB_ShaderStorage] = kBinding.m_kName;

		if (kBinding.m_kRange.m_uOffset == 0 && kBinding.m_kRange.m_uSize == 0)
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, uUnit, kBinding.m_kName.GetName());
		else
			glBindBufferRange(GL_SHADER_STORAGE_BUFFER, uUnit, kBinding.m_kName.GetName(), kBinding.m_kRange.m_uOffset, kBinding.m_kRange.m_uSize);
	}
}

#endif //DXGL_SUPPORT_SHADER_STORAGE_BLOCKS

#if DXGL_SUPPORT_SHADER_IMAGES

void CContext::BindImage(const CResourceName& kName, SShaderImageViewConfiguration kConfiguration, uint32 uUnit)
{
	SImageUnitCache& kImageUnit(m_kStateCache.m_akImageUnits[uUnit]);
	bool bNewTexture = CACHE_VAR(kImageUnit.m_kTextureName, kName);
	bool bNewConfig = CACHE_VAR(kImageUnit.m_kConfiguration, kConfiguration);
	if (bNewTexture || bNewConfig)
	{
		glBindImageTexture(
		  uUnit,
		  kName.GetName(),
		  kConfiguration.m_iLevel,
		  kConfiguration.m_iLayer >= 0,
		  kConfiguration.m_iLayer >= 0 ? kConfiguration.m_iLayer : 0,
		  kConfiguration.m_eAccess,
		  kConfiguration.m_eFormat);
	}
}

#endif //DXGL_SUPPORT_SHADER_STORAGE_BLOCKS

void CContext::SetUnpackRowLength(GLint iValue)
{
	if (CACHE_VAR(m_kStateCache.m_iUnpackRowLength, iValue))
		glPixelStorei(GL_UNPACK_ROW_LENGTH, iValue);
}

void CContext::SetUnpackImageHeight(GLint iValue)
{
	if (CACHE_VAR(m_kStateCache.m_iUnpackImageHeight, iValue))
		glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, iValue);
}

void CContext::SetUnpackAlignment(GLint iValue)
{
	if (CACHE_VAR(m_kStateCache.m_iUnpackAlignment, iValue))
		glPixelStorei(GL_UNPACK_ALIGNMENT, iValue);
}

void CContext::SetPackRowLength(GLint iValue)
{
	if (CACHE_VAR(m_kStateCache.m_iPackRowLength, iValue))
		glPixelStorei(GL_PACK_ROW_LENGTH, iValue);
}

void CContext::SetPackImageHeight(GLint iValue)
{
#if !DXGLES
	if (CACHE_VAR(m_kStateCache.m_iPackImageHeight, iValue))
		glPixelStorei(GL_PACK_IMAGE_HEIGHT, iValue);
#else
	DXGL_WARNING("OpenGL ES does not support GL_PACK_IMAGE_HEIGHT");
#endif
}

void CContext::SetPackAlignment(GLint iValue)
{
	if (CACHE_VAR(m_kStateCache.m_iPackAlignment, iValue))
		glPixelStorei(GL_PACK_ALIGNMENT, iValue);
}

void CContext::SetViewports(uint32 uNumViewports, const D3D11_VIEWPORT* pViewports)
{
	DXGL_SCOPED_PROFILE("CContext::SetViewports")

	if (uNumViewports > DXGL_NUM_SUPPORTED_VIEWPORTS)
	{
		DXGL_WARNING("Setting more viewports than supported (" DXGL_QUOTE(NUM_SUPPORTED_VIEWPORTS) "), additional viewports are ignored");
		uNumViewports = DXGL_NUM_SUPPORTED_VIEWPORTS;
	}

	TViewportValue* pGLViewportIter(m_kStateCache.m_akViewportData);
	TDepthRangeValue* pGLDepthRangeIter(m_kStateCache.m_akDepthRangeData);
	const D3D11_VIEWPORT* pViewport;
	for (pViewport = pViewports; pViewport < pViewports + uNumViewports; ++pViewport)
	{
		*(pGLViewportIter++) = (TViewportValue)pViewport->TopLeftX;
		*(pGLViewportIter++) = (TViewportValue)pViewports->TopLeftY;
		*(pGLViewportIter++) = (TViewportValue)pViewport->Width;
		*(pGLViewportIter++) = (TViewportValue)pViewport->Height;
		*(pGLDepthRangeIter++) = pViewport->MinDepth;
		*(pGLDepthRangeIter++) = pViewport->MaxDepth;
	}

	assert(pGLViewportIter < m_kStateCache.m_akViewportData + sizeof(m_kStateCache.m_akViewportData));
	assert(pGLDepthRangeIter < m_kStateCache.m_akDepthRangeData + sizeof(m_kStateCache.m_akDepthRangeData));
#if DXGL_SUPPORT_VIEWPORT_ARRAY
	glViewportArrayv(0, uNumViewports, m_kStateCache.m_akViewportData);
	glDepthRangeArrayv(0, uNumViewports, m_kStateCache.m_akDepthRangeData);
#else
	glViewport(
	  m_kStateCache.m_akViewportData[0],
	  m_kStateCache.m_akViewportData[1],
	  m_kStateCache.m_akViewportData[2],
	  m_kStateCache.m_akViewportData[3]);
	glDepthRangef(
	  m_kStateCache.m_akDepthRangeData[0],
	  m_kStateCache.m_akDepthRangeData[1]);
#endif
}

void CContext::SetScissorRects(uint32 uNumRects, const D3D11_RECT* pRects)
{
	DXGL_SCOPED_PROFILE("CContext::SetScissorRects")

	GLint aGLScissorData[DXGL_NUM_SUPPORTED_SCISSOR_RECTS * 4];

	if (uNumRects > DXGL_NUM_SUPPORTED_SCISSOR_RECTS)
	{
		DXGL_WARNING("Setting more scissor rectangles than supported (" DXGL_QUOTE(DXGL_NUM_SUPPORTED_SCISSOR_RECTS) "), additional scissor rectangles are ignored");
		uNumRects = DXGL_NUM_SUPPORTED_SCISSOR_RECTS;
	}

	GLint* piScissorIter(aGLScissorData);
	const D3D11_RECT* pRect;
	for (pRect = pRects; pRect < pRects + uNumRects; ++pRect)
	{
		*(piScissorIter++) = pRect->left;
		*(piScissorIter++) = pRect->top;
		*(piScissorIter++) = pRect->right - pRect->left;
		*(piScissorIter++) = pRect->bottom - pRect->top;
	}
	assert(piScissorIter < aGLScissorData + sizeof(aGLScissorData));
#if DXGL_SUPPORT_SCISSOR_RECT_ARRAY
	glScissorArrayv(0, uNumRects, aGLScissorData);
#else
	glScissor(aGLScissorData[0], aGLScissorData[1], aGLScissorData[2], aGLScissorData[3]);
#endif
}

uint32 MatchColorAttachmentIndex(SOutputMergerView* pView, SFrameBufferConfiguration& kFrameBufferConfig)
{
	uint32 uAttachment(0);
	while (uAttachment < SFrameBufferConfiguration::MAX_COLOR_ATTACHMENTS &&
	       kFrameBufferConfig.m_akAttachments[uAttachment] != pView)
		++uAttachment;
	return uAttachment;
}

bool MatchDepthStencilAttachment(SOutputMergerView* pView, bool bDepth, bool bStencil, SFrameBufferConfiguration& kFrameBufferConfig)
{
	return (!bDepth || kFrameBufferConfig.m_akAttachments[SFrameBufferConfiguration::DEPTH_ATTACHMENT_INDEX] == pView) &&
	       (!bStencil || kFrameBufferConfig.m_akAttachments[SFrameBufferConfiguration::STENCIL_ATTACHMENT_INDEX] == pView);
}

SFrameBufferPtr GetCompatibleColorAttachmentFrameBuffer(SOutputMergerView* pOMView, uint32& uAttachment, CContext* pContext)
{
	// Check if there is a suitable cached frame buffer with this view attached at the requested point
#if OGL_SINGLE_CONTEXT
	SOutputMergerView::SContextData* pOMContextData(&pOMView->m_kContextData);
#else
	SOutputMergerView::SContextData* pOMContextData(pOMView->m_kContextMap[pContext->GetIndex()]);
	if (pOMContextData != NULL)
#endif
	{
		uint32 uTexFrameBuffer;
		for (uTexFrameBuffer = 0; uTexFrameBuffer < pOMContextData->m_kBoundFrameBuffers.size(); ++uTexFrameBuffer)
		{
			SFrameBuffer* pTexFrameBuffer(pOMContextData->m_kBoundFrameBuffers.at(uTexFrameBuffer).m_spFrameBuffer);
			uAttachment = MatchColorAttachmentIndex(pOMView, pTexFrameBuffer->m_kConfiguration);
			if (uAttachment != SFrameBufferConfiguration::MAX_COLOR_ATTACHMENTS)
			{
				return pTexFrameBuffer;
			}
		}
	}

	// If not then create a new one specifically for the purpose
	SFrameBufferConfiguration kCustomConfig;
	uAttachment = SFrameBufferConfiguration::FIRST_COLOR_ATTACHMENT_INDEX;
	kCustomConfig.m_akAttachments[uAttachment] = pOMView;
	return pContext->AllocateFrameBuffer(kCustomConfig);
}

SFrameBufferPtr GetCompatibleDepthStencilAttachmentFrameBuffer(SOutputMergerView* pOMView, bool bDepth, bool bStencil, CContext* pContext)
{
	// Check if there is a suitable cached frame buffer with this view attached at the requested point
#if OGL_SINGLE_CONTEXT
	SOutputMergerView::SContextData* pOMContextData(&pOMView->m_kContextData);
#else
	SOutputMergerView::SContextData* pOMContextData(pOMView->m_kContextMap[pContext->GetIndex()]);
	if (pOMContextData != NULL)
#endif
	{
		uint32 uTexFrameBuffer;
		for (uTexFrameBuffer = 0; uTexFrameBuffer < pOMContextData->m_kBoundFrameBuffers.size(); ++uTexFrameBuffer)
		{
			SFrameBuffer* pTexFrameBuffer(pOMContextData->m_kBoundFrameBuffers.at(uTexFrameBuffer).m_spFrameBuffer);
			if (MatchDepthStencilAttachment(pOMView, bDepth, bStencil, pTexFrameBuffer->m_kConfiguration))
			{
				return pTexFrameBuffer;
			}
		}
	}

	// If not then create a new one specifically for the purpose
	SFrameBufferConfiguration kCustomConfig;
	if (bDepth)
		kCustomConfig.m_akAttachments[SFrameBufferConfiguration::DEPTH_ATTACHMENT_INDEX] = pOMView;
	if (bStencil)
		kCustomConfig.m_akAttachments[SFrameBufferConfiguration::STENCIL_ATTACHMENT_INDEX] = pOMView;
	return pContext->AllocateFrameBuffer(kCustomConfig);
}

void CContext::ClearRenderTarget(SOutputMergerView* pRenderTargetView, const float afColor[4])
{
	DXGL_SCOPED_PROFILE("CContext::ClearRenderTarget")

	pRenderTargetView->m_kCreationFence.IssueWait(this);

	// First see if the view is in the current frame buffer configuration color attachments
	uint32 uAttachment(MatchColorAttachmentIndex(pRenderTargetView, m_kFrameBufferConfig));
	if (uAttachment != SFrameBufferConfiguration::MAX_COLOR_ATTACHMENTS)
		FlushFrameBufferState();
	else
	{
		SFrameBufferPtr spClearFrameBuffer(GetCompatibleColorAttachmentFrameBuffer(pRenderTargetView, uAttachment, this));

		// Finally bind the suitable frame buffer
#if !DXGLES
		if (CACHE_VAR(m_kStateCache.m_bFrameBufferSRGBEnabled, spClearFrameBuffer->m_kObject.m_bUsesSRGB))
			SetEnabledState(GL_FRAMEBUFFER_SRGB, spClearFrameBuffer->m_kObject.m_bUsesSRGB);
#endif //DXGLES
		BindDrawFrameBuffer(spClearFrameBuffer->m_kObject.m_kName);
	}

	uint32 uDrawBufferIndex((uint32)(SFrameBufferConfiguration::AttachmentIndexToID(uAttachment) - GL_COLOR_ATTACHMENT0));

	// Make sure the color mask includes all channels as glClearBufferfv is masked as well
	SColorMask kRequiredColorMask = {
		{ true, true, true, true }
	};
	SColorMask kOriginalColorMask(m_kStateCache.m_kBlend.m_kTargets[uDrawBufferIndex].m_kWriteMask);
	if (kRequiredColorMask != kOriginalColorMask)
#if DXGL_SUPPORT_INDEPENDENT_BLEND_STATES
		SSetTargetDependentBlendState::SetWriteMask(kRequiredColorMask.m_abRGBA, uDrawBufferIndex);
#else
		SSetTargetIndependentBlendState::SetWriteMask(kRequiredColorMask.m_abRGBA, 0);
#endif

	// Make sure that scissor test is disabled as glClearBufferfv is affected as well
	if (m_kStateCache.m_kRasterizer.m_bScissorEnabled)
		SetEnabledState(GL_SCISSOR_TEST, false);

	glClearBufferfv(GL_COLOR, (GLint)uDrawBufferIndex, afColor);

	// Restore the color mask as specified by the blend state
	if (kRequiredColorMask != kOriginalColorMask)
#if DXGL_SUPPORT_INDEPENDENT_BLEND_STATES
		SSetTargetDependentBlendState::SetWriteMask(kOriginalColorMask.m_abRGBA, uDrawBufferIndex);
#else
		SSetTargetIndependentBlendState::SetWriteMask(kOriginalColorMask.m_abRGBA, 0);
#endif

	// Restore that scissor test switch as specified by the rasterizer state
	if (m_kStateCache.m_kRasterizer.m_bScissorEnabled)
		SetEnabledState(GL_SCISSOR_TEST, true);
}

void CContext::ClearDepthStencil(SOutputMergerView* pDepthStencilView, bool bClearDepth, bool bClearStencil, float fDepthValue, uint8 uStencilValue)
{
	DXGL_SCOPED_PROFILE("CContext::ClearDepthStencil")

	pDepthStencilView->m_kCreationFence.IssueWait(this);

	// bClearDepth/bClearStencil could be true even if the format is not depth/stencil-renderable, verify that before
	const SGIFormatInfo* pGIFormat(GetGIFormatInfo(pDepthStencilView->m_eFormat));
	if (pGIFormat == NULL || pGIFormat->m_pUncompressed == NULL)
	{
		DXGL_ERROR("Depth-stencil view to be cleared does not have a valid format");
		return;
	}
	bool bDepthRenderable(pGIFormat->m_pUncompressed->m_eDepthType != eGICT_UNUSED);
	bool bStencilRenderable(pGIFormat->m_pUncompressed->m_eStencilType != eGICT_UNUSED);
	if (!bDepthRenderable && !bStencilRenderable)
	{
		DXGL_ERROR("Depth-stencil view to be cleared is neither depth-renderable nor stencil-renderable");
		return;
	}

	if (!bDepthRenderable)
		bClearDepth = false;
	if (!bStencilRenderable)
		bClearStencil = false;

	// First see if the view is in the current frame buffer configuration depth/stencil attachments
	if (MatchDepthStencilAttachment(pDepthStencilView, bClearDepth, bClearStencil, m_kFrameBufferConfig))
		FlushFrameBufferState();
	else
	{
		SFrameBufferPtr spClearFrameBuffer(GetCompatibleDepthStencilAttachmentFrameBuffer(pDepthStencilView, bClearDepth, bClearStencil, this));

#if !DXGLES
		if (CACHE_VAR(m_kStateCache.m_bFrameBufferSRGBEnabled, false))
			SetEnabledState(GL_FRAMEBUFFER_SRGB, false);
#endif //!DXGLES
		BindDrawFrameBuffer(spClearFrameBuffer->m_kObject.m_kName);
	}

	if (bClearDepth)
	{
		// Make sure the depth mask includes depth writing as glClearBufferf[i|v] are masked as well
		if (m_kStateCache.m_kDepthStencil.m_bDepthWriteMask != GL_TRUE)
			glDepthMask(GL_TRUE);

		// Make sure that the depth range for viewport 0 is [0.0f, 1.0f] as glClearBufferf[i|v] clamp depth values to that range in case of fixed point target
		if (m_kStateCache.m_akDepthRangeData[0] != 0.0f || m_kStateCache.m_akDepthRangeData[1] != 1.0f)
			glDepthRangef(0.0f, 1.0f);
	}

	// Make sure the stencil mask includes depth writing as glClearBufferf[i|v] are masked as well
	if (bClearStencil)
	{
		if (m_kStateCache.m_kDepthStencil.m_kStencilFrontFaces.m_uStencilWriteMask != GL_TRUE)
			glStencilMaskSeparate(GL_FRONT, ~0);
		if (m_kStateCache.m_kDepthStencil.m_kStencilBackFaces.m_uStencilWriteMask != GL_TRUE)
			glStencilMaskSeparate(GL_BACK, ~0);
	}

	// Make sure that scissor test is disabled as glClearBufferf[i|v] is affected as well
	if (m_kStateCache.m_kRasterizer.m_bScissorEnabled)
		SetEnabledState(GL_SCISSOR_TEST, false);

	GLenum eBuffer(0);
	if (bClearDepth && bClearStencil)
		glClearBufferfi(GL_DEPTH_STENCIL, 0, fDepthValue, static_cast<GLint>(uStencilValue));
	else if (bClearDepth)
		glClearBufferfv(GL_DEPTH, 0, &fDepthValue);
	else if (bClearStencil)
	{
		GLint iStencilValue(uStencilValue);
		glClearBufferiv(GL_STENCIL, 0, &iStencilValue);
	}

	if (bClearDepth)
	{
		// Restore the depth mask as specified by the depth stencil state
		if (m_kStateCache.m_kDepthStencil.m_bDepthWriteMask != GL_TRUE)
			glDepthMask(m_kStateCache.m_kDepthStencil.m_bDepthWriteMask);

		// Restore the depth range for viewport 0 as specified by the viewport state
		if (m_kStateCache.m_akDepthRangeData[0] != 0.0f || m_kStateCache.m_akDepthRangeData[1] != 1.0f)
#if DXGL_SUPPORT_VIEWPORT_ARRAY
			glDepthRangeIndexed(0, m_kStateCache.m_akDepthRangeData[0], m_kStateCache.m_akDepthRangeData[1]);
#else
			glDepthRangef(m_kStateCache.m_akDepthRangeData[0], m_kStateCache.m_akDepthRangeData[1]);
#endif
	}

	// Restore the stencil mask as specified by the depth stencil state
	if (bClearStencil)
	{
		if (m_kStateCache.m_kDepthStencil.m_kStencilFrontFaces.m_uStencilWriteMask != GL_TRUE)
			glStencilMaskSeparate(GL_FRONT, m_kStateCache.m_kDepthStencil.m_kStencilFrontFaces.m_uStencilWriteMask);
		if (m_kStateCache.m_kDepthStencil.m_kStencilBackFaces.m_uStencilWriteMask != GL_TRUE)
			glStencilMaskSeparate(GL_BACK, m_kStateCache.m_kDepthStencil.m_kStencilBackFaces.m_uStencilWriteMask);
	}

	// Restore that scissor test switch as specified by the rasterizer state
	if (m_kStateCache.m_kRasterizer.m_bScissorEnabled)
		SetEnabledState(GL_SCISSOR_TEST, false);
}

void CContext::SetRenderTargets(uint32 uNumRTViews, SOutputMergerView** apRenderTargetViews, SOutputMergerView* pDepthStencilView)
{
	DXGL_SCOPED_PROFILE("CContext::SetRenderTargets")

	m_kFrameBufferConfig = SFrameBufferConfiguration();
	m_bFrameBufferStateDirty = true;

	uint32 uColorView;
	for (uColorView = 0; uColorView < uNumRTViews; ++uColorView)
	{
		if (apRenderTargetViews[uColorView] != NULL)
		{
			SOutputMergerView* pOMView(apRenderTargetViews[uColorView]);
			pOMView->m_kCreationFence.IssueWait(this);
			m_kFrameBufferConfig.m_akAttachments[SFrameBufferConfiguration::FIRST_COLOR_ATTACHMENT_INDEX + uColorView] = pOMView;
		}
	}

	if (pDepthStencilView != NULL)
	{
		pDepthStencilView->m_kCreationFence.IssueWait(this);

		uint32 uAttachmentIndex(SFrameBufferConfiguration::MAX_ATTACHMENTS);
		const SGIFormatInfo* pFormatInfo(GetGIFormatInfo(pDepthStencilView->m_eFormat));
		GLenum eBaseFormat(GL_NONE);
		if (pFormatInfo != NULL && pFormatInfo->m_pTexture != NULL)
		{
			eBaseFormat = pFormatInfo->m_pTexture->m_eBaseFormat;
		}

		switch (eBaseFormat)
		{
		default:
			DXGL_WARNING("Invalid format for depth stencil view - using it as depth attachment");
		case GL_DEPTH_COMPONENT:
			m_kFrameBufferConfig.m_akAttachments[SFrameBufferConfiguration::DEPTH_ATTACHMENT_INDEX] = pDepthStencilView;
			break;
#if DXGL_SUPPORT_STENCIL_ONLY_FORMAT
		case GL_STENCIL_INDEX:
			m_kFrameBufferConfig.m_akAttachments[SFrameBufferConfiguration::STENCIL_ATTACHMENT_INDEX] = pDepthStencilView;
			break;
#endif  //DXGL_SUPPORT_STENCIL_ONLY_FORMAT
		case GL_DEPTH_STENCIL:
			m_kFrameBufferConfig.m_akAttachments[SFrameBufferConfiguration::DEPTH_ATTACHMENT_INDEX] = pDepthStencilView;
			m_kFrameBufferConfig.m_akAttachments[SFrameBufferConfiguration::STENCIL_ATTACHMENT_INDEX] = pDepthStencilView;
			break;
		}
	}
}

void CContext::SetShader(SShader* pShader, uint32 uStage)
{
	assert(uStage < DXGL_ARRAY_SIZE(m_kPipelineConfiguration.m_apShaders));

	m_bPipelineDirty |= (m_kPipelineConfiguration.m_apShaders[uStage] != pShader);
	m_kPipelineConfiguration.m_apShaders[uStage] = pShader;
}

void CContext::SetShaderTexture(SShaderTextureBasedView* pView, uint32 uStage, uint32 uIndex)
{
	uint32 uSlot = TextureSlot((EShaderType)uStage, uIndex);
	if (uSlot >= MAX_TEXTURE_SLOTS)
	{
		if (pView != NULL)
		{
			DXGL_WARNING("Texture %d not available for stage %d - setting ignored", uIndex, uStage);
		}
		return;
	}

	if (pView != NULL)
		pView->m_kCreationFence.IssueWait(this);
	m_abResourceUnitsDirty[eRUT_Texture] |= CACHE_VAR(m_akTextureSlots[uSlot].m_pView, pView);
}

#if DXGL_SUPPORT_SHADER_IMAGES

void CContext::SetShaderImage(SShaderImageView* pView, uint32 uStage, uint32 uIndex)
{
	uint32 uSlot = ImageSlot((EShaderType)uStage, uIndex);
	if (uSlot >= MAX_IMAGE_SLOTS)
	{
		if (pView != NULL)
		{
			DXGL_WARNING("Image %d not available for stage %d - setting ignored", uIndex, uStage);
		}
		return;
	}

	SImageUnitCache& kSlot(m_akImageSlots[uSlot]);
	if (pView == NULL)
		m_abResourceUnitsDirty[eRUT_Image] |= CACHE_VAR(kSlot.m_kTextureName, CResourceName());
	else
	{
		pView->m_kCreationFence.IssueWait(this);
		bool bNewTexture = CACHE_VAR(kSlot.m_kTextureName, pView->m_kName);
		bool bNewConfig = CACHE_VAR(kSlot.m_kConfiguration, pView->m_kConfiguration);
		m_abResourceUnitsDirty[eRUT_Image] |= bNewTexture || bNewConfig;
	}
}

#endif //DXGL_SUPPORT_SHADER_IMAGES

#if DXGL_SUPPORT_SHADER_STORAGE_BLOCKS

void CContext::SetShaderBuffer(SShaderBufferView* pView, uint32 uStage, uint32 uIndex)
{
	uint32 uSlot = StorageBufferSlot((EShaderType)uStage, uIndex);
	if (uSlot >= MAX_STORAGE_BUFFER_SLOTS)
	{
		if (pView != NULL)
		{
			DXGL_WARNING("Shader storage buffer %d not available for stage %d - setting ignored", uIndex, uStage);
		}
		return;
	}

	TIndexedBufferBinding& kSlot(m_akStorageBufferSlots[uSlot]);
	if (pView == NULL)
		m_abResourceUnitsDirty[eRUT_StorageBuffer] |= CACHE_VAR(kSlot, TIndexedBufferBinding());
	else
	{
		pView->m_kCreationFence.IssueWait(this);
		m_abResourceUnitsDirty[eRUT_StorageBuffer] |= CACHE_VAR(kSlot, TIndexedBufferBinding(pView->m_kName, pView->m_kRange));
	}
}

#endif //DXGL_SUPPORT_SHADER_STORAGE_BLOCKS

void CContext::SetShaderResourceView(SShaderView* pView, uint32 uStage, uint32 uSlot)
{
	if (pView == NULL)
	{
#if DXGL_SUPPORT_SHADER_STORAGE_BLOCKS
		SetShaderBuffer(NULL, uStage, uSlot);
#endif //DXGL_SUPPORT_SHADER_STORAGE_BLOCKS
		SetShaderTexture(NULL, uStage, uSlot);
	}
	else
	{
		pView->m_kCreationFence.IssueWait(this);
		switch (pView->m_eType)
		{
		case SShaderView::eSVT_Texture:
			SetShaderTexture(static_cast<SShaderTextureBasedView*>(pView), uStage, uSlot);
#if DXGL_SUPPORT_SHADER_STORAGE_BLOCKS
			SetShaderBuffer(NULL, uStage, uSlot);
#endif  //DXGL_SUPPORT_SHADER_STORAGE_BLOCKS
			break;
#if DXGL_SUPPORT_SHADER_STORAGE_BLOCKS
		case SShaderView::eSVT_Buffer:
			SetShaderBuffer(static_cast<SShaderBufferView*>(pView), uStage, uSlot);
			SetShaderTexture(NULL, uStage, uSlot);
			break;
#endif  //DXGL_SUPPORT_SHADER_STORAGE_BLOCKS
		default:
			DXGL_ERROR("Cannot bind view of this type as shader resource");
			break;
		}
	}
}

void CContext::SetUnorderedAccessView(SShaderView* pView, uint32 uStage, uint32 uSlot)
{
#if DXGL_SUPPORT_SHADER_STORAGE_BLOCKS && DXGL_SUPPORT_SHADER_IMAGES
	if (pView == NULL)
	{
		SetShaderBuffer(NULL, uStage, uSlot);
		SetShaderImage(NULL, uStage, uSlot);
	}
	else
	{
		pView->m_kCreationFence.IssueWait(this);
		switch (pView->m_eType)
		{
		case SShaderView::eSVT_Image:
			SetShaderImage(static_cast<SShaderImageView*>(pView), uStage, uSlot);
			SetShaderBuffer(NULL, uStage, uSlot);
			break;
		case SShaderView::eSVT_Buffer:
			SetShaderBuffer(static_cast<SShaderBufferView*>(pView), uStage, uSlot);
			SetShaderImage(NULL, uStage, uSlot);
			break;
		default:
			DXGL_ERROR("Cannot bind view of this type as shader resource");
			break;
		}
	}
#else
	DXGL_ERROR("CContext::SetUnorderedAccessView is not supported in this configuration");
#endif
}

void CContext::SetSampler(SSamplerState* pSampler, uint32 uStage, uint32 uIndex)
{
	uint32 uSlot = SamplerSlot((EShaderType)uStage, uIndex);
	if (uSlot >= MAX_SAMPLER_SLOTS)
	{
		DXGL_WARNING("Sampler %d not available for stage %d - setting ignored", uIndex, uStage);
		return;
	}

	m_abResourceUnitsDirty[eRUT_Texture] |= CACHE_VAR(m_akSamplerSlots[uSlot].m_pSampler, pSampler);
}

void CContext::SetConstantBuffer(SBuffer* pConstantBuffer, SBufferRange kRange, uint32 uStage, uint32 uIndex)
{
	uint32 uSlot = ConstantBufferSlot((EShaderType)uStage, uIndex);
	if (uSlot >= MAX_CONSTANT_BUFFER_SLOTS)
	{
		DXGL_WARNING("Constant buffer %d not available for stage %d - setting ignored", uIndex, uStage);
		return;
	}

	TConstantBufferSlot& kSlot(m_akConstantBufferSlots[uSlot]);
#if DXGL_STREAMING_CONSTANT_BUFFERS
	kSlot.m_kRange = kRange;
	kSlot.m_pBuffer = pConstantBuffer;
#else
	if (pConstantBuffer == NULL)
		m_abResourceUnitsDirty[eRUT_UniformBuffer] |= CACHE_VAR(kSlot, TIndexedBufferBinding());
	else
	{
		pConstantBuffer->m_kCreationFence.IssueWait(this);
		m_abResourceUnitsDirty[eRUT_UniformBuffer] |= CACHE_VAR(kSlot, TIndexedBufferBinding(pConstantBuffer->m_kName, kRange));
	}
#endif
}

void CContext::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY eTopology)
{
	switch (eTopology)
	{
#define _CASE(_D3DValue, _GLMode, _AdditionalInstr) \
  case _D3DValue:                                   \
    m_ePrimitiveTopologyMode = _GLMode;             \
    _AdditionalInstr                                \
    break;
#define CASE_PRIM(_D3DValue, _GLMode)                 _CASE(_D3DValue, _GLMode, )
#define CASE_CTRL(_D3DValue, _GLMode, _NumCtrlPoints) _CASE(_D3DValue, _GLMode, SetNumPatchControlPoints(_NumCtrlPoints); )
		CASE_PRIM(D3D_PRIMITIVE_TOPOLOGY_UNDEFINED, GL_NONE)
		CASE_PRIM(D3D_PRIMITIVE_TOPOLOGY_POINTLIST, GL_POINTS)
		CASE_PRIM(D3D_PRIMITIVE_TOPOLOGY_LINELIST, GL_LINES)
		CASE_PRIM(D3D_PRIMITIVE_TOPOLOGY_LINESTRIP, GL_LINE_STRIP)
		CASE_PRIM(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST, GL_TRIANGLES)
		CASE_PRIM(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, GL_TRIANGLE_STRIP)
		CASE_PRIM(D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ, GL_LINE_STRIP)
		CASE_PRIM(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ, GL_TRIANGLE_STRIP)
#if DXGL_SUPPORT_GEOMETRY_SHADERS
		CASE_PRIM(D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ, GL_LINES_ADJACENCY)
		CASE_PRIM(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ, GL_TRIANGLES_ADJACENCY)
#endif //DXGL_SUPPORT_GEOMETRY_SHADERS
#if DXGL_SUPPORT_TESSELLATION
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST, GL_PATCHES, 1)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST, GL_PATCHES, 2)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST, GL_PATCHES, 3)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST, GL_PATCHES, 4)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST, GL_PATCHES, 5)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST, GL_PATCHES, 6)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST, GL_PATCHES, 7)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST, GL_PATCHES, 8)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST, GL_PATCHES, 9)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST, GL_PATCHES, 10)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST, GL_PATCHES, 11)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST, GL_PATCHES, 12)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST, GL_PATCHES, 13)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST, GL_PATCHES, 14)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST, GL_PATCHES, 15)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST, GL_PATCHES, 16)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST, GL_PATCHES, 17)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST, GL_PATCHES, 18)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST, GL_PATCHES, 19)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST, GL_PATCHES, 20)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST, GL_PATCHES, 21)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST, GL_PATCHES, 22)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST, GL_PATCHES, 23)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST, GL_PATCHES, 24)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST, GL_PATCHES, 25)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST, GL_PATCHES, 26)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST, GL_PATCHES, 27)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST, GL_PATCHES, 28)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST, GL_PATCHES, 29)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST, GL_PATCHES, 30)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST, GL_PATCHES, 31)
		CASE_CTRL(D3D_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST, GL_PATCHES, 32)
#endif //DXGL_SUPPORT_TESSELLATION
#undef _CASE
#undef CASE_CTRL
#undef CASE_PRIM
	default:
		DXGL_ERROR("Invalid primitive topology");
		break;
	}
}

void CContext::SetInputLayout(SInputLayout* pInputLayout)
{
	m_pInputLayout = pInputLayout;
	m_bInputLayoutDirty = true;
}

void CContext::SetVertexBuffer(uint32 uSlot, SBuffer* pVertexBuffer, uint32 uStride, uint32 uOffset)
{
	SInputAssemblerSlot& kSlot(m_akInputAssemblerSlots[uSlot]);
	kSlot.m_pVertexBuffer = pVertexBuffer;
	kSlot.m_uStride = uStride;
	kSlot.m_uOffset = uOffset;
	m_bInputAssemblerSlotsDirty = true;
}

void CContext::SetIndexBuffer(SBuffer* pIndexBuffer, GLenum eIndexType, GLuint uIndexStride, GLuint uOffset)
{
	BindBuffer(pIndexBuffer, eBB_ElementArray);
	m_eIndexType = eIndexType;
	m_uIndexStride = uIndexStride;
	m_uIndexOffset = uOffset;
}

#if DXGL_ENABLE_SHADER_TRACING

void CContext::PrepareTraceHeader(uint32 uFirstVertex, uint32 uFirstIndex)
{
	switch (m_eStageTracing)
	{
	case eST_Vertex:
		{
			uint32 uIndex = uFirstIndex + m_kStageTracingInfo.m_kVertex.m_uVertexIndex;

			const CResourceName& kIndexBuffer(m_kStateCache.m_akBuffersBound[eBB_ElementArray]);
			uint32 uVertexID = ~0;
			if (kIndexBuffer.IsValid())
			{
				GLvoid* pIndex = glMapNamedBufferRangeEXT(kIndexBuffer.GetName(), m_uIndexOffset + m_uIndexStride * uIndex, m_uIndexStride, GL_MAP_READ_BIT);
				if (pIndex == NULL)
				{
					DXGL_ERROR("Could not read back vertex ID for index %d", uIndex);
				}
				else
				{
					switch (m_eIndexType)
					{
					case GL_UNSIGNED_INT:
						uVertexID = (uint32) * static_cast<GLuint*>(pIndex);
						break;
					case GL_UNSIGNED_SHORT:
						uVertexID = (uint32) * static_cast<GLushort*>(pIndex);
						break;
					case GL_UNSIGNED_BYTE:
						uVertexID = (uint32) * static_cast<GLubyte*>(pIndex);
						break;
					default:
						DXGL_ERROR("Unsupported index type 0x%X", m_eIndexType);
						uVertexID = ~0;
					}

					glUnmapNamedBufferEXT(kIndexBuffer.GetName());
				}
			}
			else
				uVertexID = uIndex;

			m_kStageTracingInfo.m_kVertex.m_kHeader.m_uVertexID = uFirstVertex + uVertexID;
		}
		break;
	case eST_Fragment:
		{
			uint32 uFragmentX = m_kStageTracingInfo.m_kFragment.m_uFragmentCoordX;
			uint32 uFragmentY = m_kStageTracingInfo.m_kFragment.m_uFragmentCoordY;

			m_kStageTracingInfo.m_kFragment.m_kHeader.m_afFragmentCoordX = 0.5f + (float)uFragmentX;
			m_kStageTracingInfo.m_kFragment.m_kHeader.m_afFragmentCoordY = 0.5f + (float)uFragmentY;
		}
		break;
	case eST_Compute:
		{
			GLuint uComputeProgram;
			if (SupportSeparablePrograms())
			{
				GLint iValue;
				glGetProgramPipelineiv(m_spPipeline->m_uName, GL_COMPUTE_SHADER, &iValue);
				uComputeProgram = (GLuint)iValue;
			}
			else
				uComputeProgram = m_spPipeline->m_uName;

			GLint aiWorkGroupSize[3];
			glGetProgramiv(uComputeProgram, GL_COMPUTE_WORK_GROUP_SIZE, aiWorkGroupSize);

			SComputeShaderTraceInfo::ID kWorkGroupID = m_kStageTracingInfo.m_kCompute.m_kWorkGroupID;
			SComputeShaderTraceInfo::ID kLocalInvocationID = m_kStageTracingInfo.m_kCompute.m_kLocalInvocationID;

			m_kStageTracingInfo.m_kCompute.m_kHeader.m_uGlobalInvocationIDX = kWorkGroupID.x * aiWorkGroupSize[0] + kLocalInvocationID.x;
			m_kStageTracingInfo.m_kCompute.m_kHeader.m_uGlobalInvocationIDY = kWorkGroupID.y * aiWorkGroupSize[1] + kLocalInvocationID.y;
			m_kStageTracingInfo.m_kCompute.m_kHeader.m_uGlobalInvocationIDZ = kWorkGroupID.z * aiWorkGroupSize[2] + kLocalInvocationID.z;
		}
		break;
	default:
		DXGL_NOT_IMPLEMENTED;
		break;
	}
}

void CContext::FlushShaderTracingState()
{
	if (m_eStageTracing != eST_NUM && m_kPipelineConfiguration.m_apShaders[m_eStageTracing] != NULL)
	{
		uint32 uCurrentConfigHash(m_kPipelineConfiguration.m_apShaders[m_eStageTracing]->m_kReflection.m_uInputHash);
		if (uCurrentConfigHash == m_uShaderTraceHash)
		{
			m_kPipelineConfiguration.m_eStageTracing = m_eStageTracing;
			m_bPipelineDirty = true;
		}
	}
}

void CContext::BeginTrace(uint32 uFirstVertex, uint32 uFirstIndex)
{
	enum
	{
		MAX_TRACED_VERTEX_INVOCATONS   = 0x04,
		MAX_TRACED_FRAGMENT_INVOCATONS = 0x10,
		MAX_TRACED_COMPUTE_INVOCATONS  = 0x01,
		VERTEX_TRACE_STRIDE            = SShaderTraceBufferCommon::CAPACITY / MAX_TRACED_VERTEX_INVOCATONS,
		FRAGMENT_TRACE_STRIDE          = SShaderTraceBufferCommon::CAPACITY / MAX_TRACED_FRAGMENT_INVOCATONS,
		COMPUTE_TRACE_STRIDE           = SShaderTraceBufferCommon::CAPACITY / MAX_TRACED_COMPUTE_INVOCATONS,
	};

	union UShaderTraceBufferStorage
	{
		SShaderTraceBuffer<SVertexShaderTraceHeader>   m_kVertex;
		SShaderTraceBuffer<SFragmentShaderTraceHeader> m_kPixel;
		SShaderTraceBuffer<SComputeShaderTraceHeader>  m_kCompute;
	};

	if (m_eStageTracing != eST_NUM && m_spPipeline->m_kConfiguration.m_apShaders[m_eStageTracing] != NULL)
	{
		uint32 uCurrentPipelineHash(m_spPipeline->m_kConfiguration.m_apShaders[m_eStageTracing]->m_kReflection.m_uInputHash);
		if (uCurrentPipelineHash != m_uShaderTraceHash)
			return;

		PrepareTraceHeader(uFirstVertex, uFirstIndex);

		GLuint uBufferName;
		if (!m_kShaderTracingBuffer.IsValid())
		{
			glGenBuffers(1, &uBufferName);
			glNamedBufferDataEXT(uBufferName, sizeof(UShaderTraceBufferStorage), NULL, GL_DYNAMIC_READ);
			m_kShaderTracingBuffer = m_pDevice->GetBufferNamePool().Create(uBufferName);
		}
		else
			uBufferName = m_kShaderTracingBuffer.GetName();

		switch (m_eStageTracing)
		{
		case eST_Vertex:
			BeginTraceInternal(uBufferName, m_kStageTracingInfo.m_kVertex.m_kHeader, VERTEX_TRACE_STRIDE);
			break;
		case eST_Fragment:
			BeginTraceInternal(uBufferName, m_kStageTracingInfo.m_kFragment.m_kHeader, FRAGMENT_TRACE_STRIDE);
			break;
		case eST_Compute:
			BeginTraceInternal(uBufferName, m_kStageTracingInfo.m_kCompute.m_kHeader, COMPUTE_TRACE_STRIDE);
			break;
		default:
			DXGL_NOT_IMPLEMENTED;
			break;
		}

		uint32 uTraceBufferUnit = m_spPipeline->m_uTraceBufferUnit;
		m_kReplacedStorageBuffer = m_kStateCache.m_akStorageBuffersBound[uTraceBufferUnit];
		BindStorageBuffer(TIndexedBufferBinding(m_kShaderTracingBuffer, SBufferRange()), uTraceBufferUnit);
	}
}

void CContext::EndTrace()
{
	if (m_eStageTracing != eST_NUM && m_spPipeline->m_kConfiguration.m_apShaders[m_eStageTracing])
	{
		uint32 uCurrentPipelineHash(m_spPipeline->m_kConfiguration.m_apShaders[m_eStageTracing]->m_kReflection.m_uInputHash);
		if (uCurrentPipelineHash != m_uShaderTraceHash)
			return;

		BindStorageBuffer(m_kReplacedStorageBuffer, m_spPipeline->m_uTraceBufferUnit);

		GLuint uBufferName(m_kShaderTracingBuffer.GetName());
		const SShaderTraceIndex& kTraceIndex(m_kPipelineConfiguration.m_apShaders[m_eStageTracing]->m_kTraceIndex);

		switch (m_eStageTracing)
		{
		case eST_Vertex:
			m_uShaderTraceCount += EndTraceInternal<SVertexShaderTraceHeader>(uBufferName, eST_Vertex, kTraceIndex);
			break;
		case eST_Fragment:
			m_uShaderTraceCount += EndTraceInternal<SFragmentShaderTraceHeader>(uBufferName, eST_Fragment, kTraceIndex);
			break;
		case eST_Compute:
			m_uShaderTraceCount += EndTraceInternal<SComputeShaderTraceHeader>(uBufferName, eST_Compute, kTraceIndex);
			break;
		default:
			DXGL_NOT_IMPLEMENTED;
			break;
		}

		m_kPipelineConfiguration.m_eStageTracing = eST_NUM;
	}
}

	#define BEGIN_TRACE(_FirstVertex, _FirstIndex) BeginTrace(_FirstVertex, _FirstIndex)
	#define END_TRACE()                            EndTrace()

#else

	#define BEGIN_TRACE(_FirstVertex, _FirstIndex)
	#define END_TRACE()

#endif //DXGL_ENABLE_SHADER_TRACING

void CContext::DrawIndexed(uint32 uIndexCount, uint32 uStartIndexLocation, uint32 uBaseVertexLocation)
{
	DXGL_SCOPED_PROFILE("CContext::DrawIndexed")

#if !DXGL_SUPPORT_DRAW_WITH_BASE_VERTEX
	SetVertexOffset(uBaseVertexLocation);
#endif //!DXGL_SUPPORT_DRAW_WITH_BASE_VERTEX

	FlushDrawState();

	BEGIN_TRACE(uBaseVertexLocation, uStartIndexLocation);

	GLvoid* pvOffset(reinterpret_cast<GLvoid*>(static_cast<uintptr_t>(m_uIndexOffset + uStartIndexLocation * m_uIndexStride)));
	assert(m_ePrimitiveTopologyMode != GL_NONE);
#if DXGL_SUPPORT_DRAW_WITH_BASE_VERTEX
	glDrawElementsBaseVertex(m_ePrimitiveTopologyMode, uIndexCount, m_eIndexType, pvOffset, uBaseVertexLocation);
#else
	glDrawElements(m_ePrimitiveTopologyMode, uIndexCount, m_eIndexType, pvOffset);
#endif

	END_TRACE();
}

void CContext::Draw(uint32 uVertexCount, uint32 uStartVertexLocation)
{
	DXGL_SCOPED_PROFILE("CContext::Draw")

	FlushDrawState();

	BEGIN_TRACE(uStartVertexLocation, 0);

	assert(m_ePrimitiveTopologyMode != GL_NONE);
	glDrawArrays(m_ePrimitiveTopologyMode, uStartVertexLocation, uVertexCount);

	END_TRACE();
}

void CContext::DrawIndexedInstanced(uint32 uIndexCountPerInstance, uint32 uInstanceCount, uint32 uStartIndexLocation, uint32 uBaseVertexLocation, uint32 uStartInstanceLocation)
{
	DXGL_SCOPED_PROFILE("CContext::DrawIndexedInstanced")

#if !DXGL_SUPPORT_DRAW_WITH_BASE_VERTEX
	SetVertexOffset(uBaseVertexLocation);
#endif //!DXGL_SUPPORT_DRAW_WITH_BASE_VERTEX

	FlushDrawState();

	BEGIN_TRACE(uBaseVertexLocation, uStartIndexLocation);

	GLvoid* pvOffset(reinterpret_cast<GLvoid*>(static_cast<uintptr_t>(m_uIndexOffset + uStartIndexLocation * m_uIndexStride)));
	assert(m_ePrimitiveTopologyMode != GL_NONE);
#if DXGL_SUPPORT_DRAW_WITH_BASE_VERTEX
	glDrawElementsInstancedBaseVertexBaseInstance(m_ePrimitiveTopologyMode, uIndexCountPerInstance, m_eIndexType, pvOffset, uInstanceCount, uBaseVertexLocation, uStartInstanceLocation);
#else
	if (uStartInstanceLocation == 0)
	{
		glDrawElementsInstanced(m_ePrimitiveTopologyMode, uIndexCountPerInstance, m_eIndexType, pvOffset, uInstanceCount);
	}
	else
	{
		DXGL_NOT_IMPLEMENTED
	}
#endif

	END_TRACE();
}

void CContext::DrawInstanced(uint32 uVertexCountPerInstance, uint32 uInstanceCount, uint32 uStartVertexLocation, uint32 uStartInstanceLocation)
{
	DXGL_SCOPED_PROFILE("CContext::DrawInstanced")

	FlushDrawState();

	BEGIN_TRACE(uStartVertexLocation, 0);

	assert(m_ePrimitiveTopologyMode != GL_NONE);
#if DXGL_SUPPORT_DRAW_WITH_BASE_VERTEX
	glDrawArraysInstancedBaseInstance(m_ePrimitiveTopologyMode, uStartVertexLocation, uVertexCountPerInstance, uInstanceCount, uStartInstanceLocation);
#else
	glDrawArraysInstanced(m_ePrimitiveTopologyMode, uStartVertexLocation, uVertexCountPerInstance, uInstanceCount);
#endif

	END_TRACE();
}

#if DXGL_SUPPORT_COMPUTE

void CContext::Dispatch(uint32 uGroupX, uint32 uGroupY, uint32 uGroupZ)
{
	DXGL_SCOPED_PROFILE("CContext::Dispatch")

	FlushDispatchState();

	BEGIN_TRACE(0, 0);

	glDispatchCompute(uGroupX, uGroupY, uGroupZ);

	END_TRACE();
}

void CContext::DispatchIndirect(uint32 uIndirectOffset)
{
	DXGL_SCOPED_PROFILE("CContext::DispatchIndirect")

	FlushDispatchState();

	BEGIN_TRACE(0, 0);

	glDispatchComputeIndirect(uIndirectOffset);

	END_TRACE();
}

#endif //DXGL_SUPPORT_COMPUTE

void CContext::Flush()
{
	DXGL_SCOPED_PROFILE("CContext::Flush")

	glFlush();
}

SFrameBufferPtr CContext::AllocateFrameBuffer(const SFrameBufferConfiguration& kConfiguration)
{
	DXGL_SCOPED_PROFILE("CContext::AllocateFrameBuffer")

	// First see if there is an equivalent frame buffer in the cache
	SFrameBufferCache::TMap::iterator kFound(m_pFrameBufferCache->m_kMap.find(kConfiguration));
	if (kFound != m_pFrameBufferCache->m_kMap.end())
		return kFound->second;

	DXGL_TODO("Add the possibility of using the default frame buffer (0) if the configuration only contains the default fb texture");

	// Create a new one and cache it
	GLuint uFrameBufferName;
	glGenFramebuffers(1, &uFrameBufferName);
	SFrameBufferPtr spFrameBuffer(new SFrameBuffer(kConfiguration));
	spFrameBuffer->m_uNumDrawBuffers = 0;
	spFrameBuffer->m_kDrawMask.SetZero();
	spFrameBuffer->m_kObject.m_bUsesSRGB = false;
	spFrameBuffer->m_pContext = this;
	spFrameBuffer->m_kObject.m_kName = m_pDevice->GetFrameBufferNamePool().Create(uFrameBufferName);

	bool bDepthStencilAttached(false);

	bool bSuccess(true);
	uint32 uAttachment;
	for (uAttachment = 0; uAttachment < SFrameBufferConfiguration::MAX_ATTACHMENTS; ++uAttachment)
	{
		SOutputMergerView* pAttachedView(kConfiguration.m_akAttachments[uAttachment]);
		if (pAttachedView != NULL)
		{
			GLenum eAttachmentID(SFrameBufferConfiguration::AttachmentIndexToID(uAttachment));
			assert(eAttachmentID != GL_NONE);

			const SGIFormatInfo* pFormatInfo(GetGIFormatInfo(pAttachedView->m_eFormat));
			if (pFormatInfo != NULL && pFormatInfo->m_pTexture != NULL && pFormatInfo->m_pTexture->m_bSRGB)
				spFrameBuffer->m_kObject.m_bUsesSRGB = true;

			if (uAttachment >= SFrameBufferConfiguration::FIRST_COLOR_ATTACHMENT_INDEX && uAttachment < SFrameBufferConfiguration::MAX_COLOR_ATTACHMENTS)
			{
				spFrameBuffer->m_aeDrawBuffers[spFrameBuffer->m_uNumDrawBuffers] = eAttachmentID;
				spFrameBuffer->m_kDrawMask.Set(uAttachment - SFrameBufferConfiguration::FIRST_COLOR_ATTACHMENT_INDEX, true);
				++spFrameBuffer->m_uNumDrawBuffers;
			}

#if !DXGL_SUPPORT_STENCIL_ONLY_FORMAT
			if (eAttachmentID == GL_DEPTH_ATTACHMENT || eAttachmentID == GL_STENCIL_ATTACHMENT)
			{
				if (bDepthStencilAttached)
					continue;
				GLenum eOtherID(eAttachmentID == GL_DEPTH_ATTACHMENT ? GL_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT);
				if (kConfiguration.m_akAttachments[SFrameBufferConfiguration::AttachmentIDToIndex(eOtherID)] == pAttachedView)
				{
					eAttachmentID = GL_DEPTH_STENCIL_ATTACHMENT;
					bDepthStencilAttached = true;
				}
			}
#endif  //!DXGL_SUPPORT_STENCIL_ONLY_FORMAT

			if (!pAttachedView->AttachFrameBuffer(spFrameBuffer, eAttachmentID, this))
				bSuccess = false;
		}
	}
	glFramebufferDrawBuffersEXT(spFrameBuffer->m_kObject.m_kName.GetName(), spFrameBuffer->m_uNumDrawBuffers, spFrameBuffer->m_aeDrawBuffers);
	spFrameBuffer->m_kObject.m_kDrawMaskCache = spFrameBuffer->m_kDrawMask;

	bSuccess = spFrameBuffer->Validate() && bSuccess;

	m_pFrameBufferCache->m_kMap.insert(SFrameBufferCache::TMap::value_type(kConfiguration, spFrameBuffer));

	if (!bSuccess)
	{
		RemoveFrameBuffer(spFrameBuffer, NULL);
		return NULL;
	}

	return spFrameBuffer;
}

void CContext::RemoveFrameBuffer(SFrameBuffer* pFrameBuffer, SOutputMergerView* pInvalidView)
{
	DXGL_SCOPED_PROFILE("CContext::RemoveFrameBuffer")

	SFrameBufferCache::TMap::iterator kFound(m_pFrameBufferCache->m_kMap.find(pFrameBuffer->m_kConfiguration));
	if (kFound == m_pFrameBufferCache->m_kMap.end())
	{
		DXGL_ERROR("Frame buffer to remove was not found in the cache map");
		return;
	}

	// Remove all references of the frame buffer from the attached textures, except pInvalidView which is being destroyed
	uint32 uAttachment;
	for (uAttachment = 0; uAttachment < SFrameBufferConfiguration::MAX_ATTACHMENTS; ++uAttachment)
	{
		SOutputMergerView* pAttachedView(pFrameBuffer->m_kConfiguration.m_akAttachments[uAttachment]);
		if (pAttachedView != pInvalidView && pAttachedView != NULL)
			pAttachedView->DetachFrameBuffer(pFrameBuffer);
	}

	m_pFrameBufferCache->m_kMap.erase(kFound);
}

SPipelinePtr CContext::AllocatePipeline(const SPipelineConfiguration& kConfiguration)
{
	DXGL_SCOPED_PROFILE("CContext::AllocatePipeline")

	// First see if there is an equivalent pipeline in the cache
	SPipelineCache::TMap::iterator kFound(m_pPipelineCache->m_kMap.find(kConfiguration));
	if (kFound != m_pPipelineCache->m_kMap.end())
		return kFound->second;

	// Create a new one and cache it
	SPipelinePtr spPipeline(new SPipeline(kConfiguration, this));

	if (!InitializePipeline(spPipeline))
		return NULL;

	m_pPipelineCache->m_kMap.insert(SPipelineCache::TMap::value_type(kConfiguration, spPipeline));
	for (uint32 uShader = 0; uShader < eST_NUM; ++uShader)
	{
		SShader* pShader(kConfiguration.m_apShaders[uShader]);
		if (IsPipelineStageUsed(kConfiguration.m_eMode, (EShaderType)uShader) && pShader != NULL)
			pShader->AttachPipeline(spPipeline);
	}

	return spPipeline;
}

void CContext::RemovePipeline(SPipeline* pPipeline, SShader* pInvalidShader)
{
	DXGL_SCOPED_PROFILE("CContext::RemovePipeline")

	SPipelineCache::TMap::iterator kFound(m_pPipelineCache->m_kMap.find(pPipeline->m_kConfiguration));
	if (kFound == m_pPipelineCache->m_kMap.end())
	{
		DXGL_ERROR("Pipeline to remove was not found in the cache map");
		return;
	}

	// Remove all references of the pipeline from the attached shaders, except pInvalidShader which is being destroyed
	for (uint32 uShader = 0; uShader < eST_NUM; ++uShader)
	{
		SShader* pAttachedShader(pPipeline->m_kConfiguration.m_apShaders[uShader]);
		if (IsPipelineStageUsed(pPipeline->m_kConfiguration.m_eMode, (EShaderType)uShader) &&
		    pAttachedShader != pInvalidShader &&
		    pAttachedShader != NULL)
			pAttachedShader->DetachPipeline(pPipeline);
	}

	m_pPipelineCache->m_kMap.erase(kFound);
}

SUnitMapPtr CContext::AllocateUnitMap(SUnitMapPtr spConfiguration)
{
	SUnitMapCache::TMap::iterator kUnitMapFound(m_pUnitMapCache->m_kMap.find(spConfiguration));
	if (kUnitMapFound != m_pUnitMapCache->m_kMap.end())
		return kUnitMapFound->second;

	m_pUnitMapCache->m_kMap[spConfiguration] = spConfiguration;
	return spConfiguration;
}

bool CContext::InitializePipeline(SPipeline* pPipeline)
{
	DXGL_SCOPED_PROFILE("CContext::InitializePipeline")

	if (!CompilePipeline(pPipeline, &m_kPipelineCompilationBuffer, m_pDevice))
		return false;

	return InitializePipelineResources(pPipeline, this);
}

void CContext::BlitFrameBuffer(
  SFrameBufferObject& kSrcFBO, SFrameBufferObject& kDstFBO,
  GLenum eSrcColorBuffer, GLenum eDstColorBuffer,
  GLint iSrcXMin, GLint iSrcYMin, GLint iSrcXMax, GLint iSrcYMax,
  GLint iDstXMin, GLint iDstYMin, GLint iDstXMax, GLint iDstYMax,
  GLbitfield uMask, GLenum eFilter)
{
	DXGL_SCOPED_PROFILE("CContext::BlitFrameBuffer")

	BindReadFrameBuffer(kSrcFBO.m_kName);
	glReadBuffer(eSrcColorBuffer);

	BindDrawFrameBuffer(kDstFBO.m_kName);
	SFrameBufferObject::TColorAttachmentMask kDstDrawMask(false);
	if (GL_COLOR_ATTACHMENT0 <= eDstColorBuffer && (eDstColorBuffer - GL_COLOR_ATTACHMENT0) < SFrameBufferConfiguration::MAX_COLOR_ATTACHMENTS)
		kDstDrawMask.Set(eDstColorBuffer - GL_COLOR_ATTACHMENT0, true);
	if (CACHE_VAR(kDstFBO.m_kDrawMaskCache, kDstDrawMask))
		glDrawBuffers(1, &eDstColorBuffer);

#if !DXGLES
	bool bEnableFrameBufferSRGB(kSrcFBO.m_bUsesSRGB || kDstFBO.m_bUsesSRGB);
	if (CACHE_VAR(m_kStateCache.m_bFrameBufferSRGBEnabled, bEnableFrameBufferSRGB))
		SetEnabledState(GL_FRAMEBUFFER_SRGB, bEnableFrameBufferSRGB);
#endif //!DXGLES

	// Make sure that scissor test is disabled as glBlitFramebuffer is affected as well
	if (m_kStateCache.m_kRasterizer.m_bScissorEnabled)
		SetEnabledState(GL_SCISSOR_TEST, false);

	glBlitFramebuffer(iSrcXMin, iSrcYMin, iSrcXMax, iSrcYMax, iDstXMin, iDstYMin, iDstXMax, iDstYMax, uMask, eFilter);

	// Restore that scissor test switch as specified by the rasterizer state
	if (m_kStateCache.m_kRasterizer.m_bScissorEnabled)
		SetEnabledState(GL_SCISSOR_TEST, true);

	m_bFrameBufferStateDirty = true;
}

bool CContext::BlitOutputMergerView(
  SOutputMergerView* pSrcView, SOutputMergerView* pDstView,
  GLint iSrcXMin, GLint iSrcYMin, GLint iSrcXMax, GLint iSrcYMax,
  GLint iDstXMin, GLint iDstYMin, GLint iDstXMax, GLint iDstYMax,
  GLbitfield uMask, GLenum eFilter)
{
	const SGIFormatInfo* pSrcFormatInfo(GetGIFormatInfo(pSrcView->m_eFormat));
	const SGIFormatInfo* pDstFormatInfo(GetGIFormatInfo(pDstView->m_eFormat));

	if (pSrcFormatInfo->m_pUncompressed == NULL || pDstFormatInfo->m_pUncompressed == NULL)
		return false;

	bool bColor((uMask & GL_COLOR_BUFFER_BIT) != 0 && pSrcFormatInfo->m_pUncompressed->m_eColorType != eGICT_UNUSED);
	bool bDepth((uMask & GL_DEPTH_BUFFER_BIT) != 0 && pSrcFormatInfo->m_pUncompressed->m_eDepthType != eGICT_UNUSED);
	bool bStencil((uMask & GL_STENCIL_BUFFER_BIT) != 0 && pSrcFormatInfo->m_pUncompressed->m_eStencilType != eGICT_UNUSED);

	if (bColor)
	{
		uint32 uSrcAttachment, uDstAttachment;
		SFrameBufferPtr spSrcFrameBuffer(GetCompatibleColorAttachmentFrameBuffer(pSrcView, uSrcAttachment, this));
		SFrameBufferPtr spDstFrameBuffer(GetCompatibleColorAttachmentFrameBuffer(pDstView, uDstAttachment, this));

		if (spSrcFrameBuffer == NULL || spDstFrameBuffer == NULL)
			return false;

		BlitFrameBuffer(
		  spSrcFrameBuffer->m_kObject, spDstFrameBuffer->m_kObject,
		  SFrameBufferConfiguration::AttachmentIndexToID(uSrcAttachment),
		  SFrameBufferConfiguration::AttachmentIndexToID(uDstAttachment),
		  iSrcXMin, iSrcYMin, iSrcXMax, iSrcYMax,
		  iDstXMin, iDstYMin, iDstXMax, iDstYMax,
		  GL_COLOR_BUFFER_BIT, eFilter);
	}

	if (bDepth || bStencil)
	{
		SFrameBufferPtr spSrcFrameBuffer(GetCompatibleDepthStencilAttachmentFrameBuffer(pSrcView, bDepth, bStencil, this));
		SFrameBufferPtr spDstFrameBuffer(GetCompatibleDepthStencilAttachmentFrameBuffer(pDstView, bDepth, bStencil, this));

		if (spSrcFrameBuffer == NULL || spDstFrameBuffer == NULL)
			return false;

		GLenum eDSMask((bDepth ? GL_DEPTH_BUFFER_BIT : 0) | (bStencil ? GL_STENCIL_BUFFER_BIT : 0));
		BlitFrameBuffer(
		  spSrcFrameBuffer->m_kObject, spDstFrameBuffer->m_kObject,
		  0, 0,
		  iSrcXMin, iSrcYMin, iSrcXMax, iSrcYMax,
		  iDstXMin, iDstYMin, iDstXMax, iDstYMax,
		  eDSMask, eFilter);
	}

	return true;
}

void CContext::ReadbackFrameBufferAttachment(
  SFrameBufferObject& kFBO, GLenum eColorBuffer,
  GLint iXMin, GLint iYMin, GLsizei iWidth, GLint iHeight,
  GLenum eBaseFormat, GLenum eDataType,
  GLvoid* pvData)
{
	BindReadFrameBuffer(kFBO.m_kName);
	glReadBuffer(eColorBuffer);

#if !DXGLES
	if (CACHE_VAR(m_kStateCache.m_bFrameBufferSRGBEnabled, kFBO.m_bUsesSRGB))
		SetEnabledState(GL_FRAMEBUFFER_SRGB, kFBO.m_bUsesSRGB);
#endif //!DXGLES

	glReadPixels(iXMin, iYMin, iWidth, iHeight, eBaseFormat, eDataType, pvData);
}

bool CContext::ReadBackOutputMergerView(SOutputMergerView* pView, GLint iXMin, GLint iYMin, GLsizei iWidth, GLint iHeight, GLvoid* pvData)
{
	const SGIFormatInfo* pFormatInfo(GetGIFormatInfo(pView->m_eFormat));

	if (pFormatInfo->m_pUncompressed == NULL)
		return false;

	bool bDepth(pFormatInfo->m_pUncompressed->m_eDepthType != eGICT_UNUSED);
	bool bStencil(pFormatInfo->m_pUncompressed->m_eStencilType != eGICT_UNUSED);
	if (bDepth || bStencil)
		return false;

	uint32 uAttachment;
	SFrameBufferPtr spFrameBuffer(GetCompatibleColorAttachmentFrameBuffer(pView, uAttachment, this));

	if (spFrameBuffer == NULL)
		return false;

	ReadbackFrameBufferAttachment(
	  spFrameBuffer->m_kObject,
	  SFrameBufferConfiguration::AttachmentIndexToID(uAttachment),
	  iXMin, iYMin, iWidth, iHeight,
	  pFormatInfo->m_pTexture->m_eBaseFormat, pFormatInfo->m_pTexture->m_eDataType,
	  pvData);

	return true;
}

#if DXGL_ENABLE_SHADER_TRACING

void CContext::TogglePixelTracing(bool bEnable, uint32 uShaderHash, uint32 uPixelX, uint32 uPixelY)
{
	if (bEnable)
	{
		m_eStageTracing = eST_Fragment;
		m_uShaderTraceHash = uShaderHash;
		m_kStageTracingInfo.m_kFragment.m_uFragmentCoordX = uPixelX;
		m_kStageTracingInfo.m_kFragment.m_uFragmentCoordY = uPixelY;
	}
	else if (m_eStageTracing == eST_Fragment)
		m_eStageTracing = eST_NUM;
}

void CContext::ToggleVertexTracing(bool bEnable, uint32 uShaderHash, uint32 uVertexIndex)
{
	if (bEnable)
	{
		m_eStageTracing = eST_Vertex;
		m_uShaderTraceHash = uShaderHash;
		m_kStageTracingInfo.m_kVertex.m_uVertexIndex = uVertexIndex;
	}
	else if (m_eStageTracing == eST_Vertex)
		m_eStageTracing = eST_NUM;
}

void CContext::ToggleComputeTracing(
  bool bEnable, uint32 uShaderHash,
  uint32 uWorkGroupIDX, uint32 uWorkGroupIDY, uint32 uWorkGroupIDZ,
  uint32 uLocalInvocationIDX, uint32 uLocalInvocationIDY, uint32 uLocalInvocationIDZ)
{
	if (bEnable)
	{
		m_eStageTracing = eST_Compute;
		m_uShaderTraceHash = uShaderHash;
		m_kStageTracingInfo.m_kCompute.m_kWorkGroupID.x = uWorkGroupIDX;
		m_kStageTracingInfo.m_kCompute.m_kWorkGroupID.y = uWorkGroupIDY;
		m_kStageTracingInfo.m_kCompute.m_kWorkGroupID.z = uWorkGroupIDZ;
		m_kStageTracingInfo.m_kCompute.m_kLocalInvocationID.x = uLocalInvocationIDX;
		m_kStageTracingInfo.m_kCompute.m_kLocalInvocationID.y = uLocalInvocationIDY;
		m_kStageTracingInfo.m_kCompute.m_kLocalInvocationID.z = uLocalInvocationIDZ;
	}
	else if (m_eStageTracing == eST_Compute)
		m_eStageTracing = eST_NUM;
}

#endif //DXGL_ENABLE_SHADER_TRACING

#if DXGL_TRACE_CALLS

void CContext::CallTraceWrite(const char* szTrace)
{
	fprintf(m_kCallTrace.m_pFile, szTrace);
}

void CContext::CallTraceFlush()
{
	fflush(m_kCallTrace.m_pFile);
}

#endif //DXGL_TRACE_CALLS

#undef CACHE_VAR
#undef CACHE_FIELD

}
