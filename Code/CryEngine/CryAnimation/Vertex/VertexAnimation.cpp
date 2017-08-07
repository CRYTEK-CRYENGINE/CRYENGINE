// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "VertexAnimation.h"

#include "Model.h"
#include "CharacterManager.h"
#include "VertexData.h"
#include "VertexCommand.h"
#include "VertexCommandBuffer.h"
#include "AttachmentSkin.h"
#include "AttachmentVCloth.h"

std::vector<CAttachmentSKIN*> CVertexAnimation::s_softwareRenderMeshes;
std::vector<CAttachmentSKIN*> CVertexAnimation::s_newSoftwareRenderMeshes;

void CVertexAnimation::RegisterSoftwareRenderMesh(CAttachmentSKIN* pAttachment)
{
	std::vector<CAttachmentSKIN*>::iterator itFound = std::find(s_softwareRenderMeshes.begin(), s_softwareRenderMeshes.end(), pAttachment);
	if (itFound != s_softwareRenderMeshes.end())
	{
		s_softwareRenderMeshes.erase(itFound);
	}
	s_newSoftwareRenderMeshes.push_back(pAttachment);
}

void CVertexAnimation::RemoveSoftwareRenderMesh(CAttachmentSKIN* pAttachment)
{
	std::vector<CAttachmentSKIN*>::iterator itFound;

	itFound = std::find(s_softwareRenderMeshes.begin(), s_softwareRenderMeshes.end(), pAttachment);
	while (itFound != s_softwareRenderMeshes.end())
	{
		s_softwareRenderMeshes.erase(itFound);
		itFound = std::find(s_softwareRenderMeshes.begin(), s_softwareRenderMeshes.end(), pAttachment);
	}

	itFound = std::find(s_newSoftwareRenderMeshes.begin(), s_newSoftwareRenderMeshes.end(), pAttachment);
	while (itFound != s_newSoftwareRenderMeshes.end())
	{
		s_newSoftwareRenderMeshes.erase(itFound);
		itFound = std::find(s_newSoftwareRenderMeshes.begin(), s_newSoftwareRenderMeshes.end(), pAttachment);
	}
}

std::vector<CAttachmentVCLOTH*> CVertexAnimation::s_softwareRenderMeshesVCloth;
std::vector<CAttachmentVCLOTH*> CVertexAnimation::s_newSoftwareRenderMeshesVCloth;

void CVertexAnimation::RegisterSoftwareRenderMesh(CAttachmentVCLOTH* pAttachment)
{
	std::vector<CAttachmentVCLOTH*>::iterator itFound = std::find(s_softwareRenderMeshesVCloth.begin(), s_softwareRenderMeshesVCloth.end(), pAttachment);
	if (itFound != s_softwareRenderMeshesVCloth.end())
	{
		s_softwareRenderMeshesVCloth.erase(itFound);
	}
	s_newSoftwareRenderMeshesVCloth.push_back(pAttachment);
}

void CVertexAnimation::RemoveSoftwareRenderMesh(CAttachmentVCLOTH* pAttachment)
{
	std::vector<CAttachmentVCLOTH*>::iterator itFound;

	itFound = std::find(s_softwareRenderMeshesVCloth.begin(), s_softwareRenderMeshesVCloth.end(), pAttachment);
	while (itFound != s_softwareRenderMeshesVCloth.end())
	{
		s_softwareRenderMeshesVCloth.erase(itFound);
		itFound = std::find(s_softwareRenderMeshesVCloth.begin(), s_softwareRenderMeshesVCloth.end(), pAttachment);
	}

	itFound = std::find(s_newSoftwareRenderMeshesVCloth.begin(), s_newSoftwareRenderMeshesVCloth.end(), pAttachment);
	while (itFound != s_newSoftwareRenderMeshesVCloth.end())
	{
		s_newSoftwareRenderMeshesVCloth.erase(itFound);
		itFound = std::find(s_newSoftwareRenderMeshesVCloth.begin(), s_newSoftwareRenderMeshesVCloth.end(), pAttachment);
	}
}

void CVertexAnimation::ClearSoftwareRenderMeshes()
{
	uint32 numMeshes = s_softwareRenderMeshes.size();
	for (uint i = 0; i < numMeshes; ++i)
		s_softwareRenderMeshes[i]->ReleaseSoftwareRenderMeshes();
	std::swap(s_softwareRenderMeshes, s_newSoftwareRenderMeshes);
	s_newSoftwareRenderMeshes.clear();

	uint32 numMeshesVCloth = s_softwareRenderMeshesVCloth.size();
	for (uint i = 0; i < numMeshesVCloth; ++i)
		s_softwareRenderMeshesVCloth[i]->ReleaseSoftwareRenderMeshes();
	std::swap(s_softwareRenderMeshesVCloth, s_newSoftwareRenderMeshesVCloth);
	s_newSoftwareRenderMeshesVCloth.clear();
}

//

CVertexAnimation::CVertexAnimation()
{
	m_skinData.vertexTransformCount = 0;

	m_pClothPiece = NULL;

	m_RenderMeshId = 0;
	m_skinningPoolID = 0;
	m_overridenWeights = false;
}

CVertexAnimation::~CVertexAnimation()
{
}

//

void CVertexAnimation::OverrideFrameWeight(const SSoftwareVertexFrame& frame, float weight)
{
	SVertexFrameState& frameState = m_frameStates[frame.index];

	frameState.weight = weight;
	frameState.flags = 0;
}

void CVertexAnimation::OverrideFrameWeightByIndex(const CSkin* pSkin, uint index, float weight)
{
	const CSoftwareMesh& softwareMesh = pSkin->GetModelMesh(0)->m_softwareMesh;
	const uint frameCount = softwareMesh.GetVertexFrames().GetCount();
	if (index >= frameCount)
		return;

	const SSoftwareVertexFrame& frame = softwareMesh.GetVertexFrames().GetFrames()[index];
	OverrideFrameWeight(frame, weight);
}

void CVertexAnimation::OverrideFrameWeightByName(const ISkin* pISkin, const char* name, float weight)
{
	CSkin* pSkin = (CSkin*)pISkin;
	const CSoftwareVertexFrames& vertexFrames = pSkin->GetModelMesh(0)->m_softwareMesh.GetVertexFrames();
	uint index = vertexFrames.GetIndexByName(name);
	if (index == -1)
		return;

	OverrideFrameWeightByIndex(pSkin, index, weight);
}

void CVertexAnimation::ClearAllFramesWeight()
{
	const uint stateCount = uint(m_frameStates.size());
	for (uint i = 0; i < stateCount; ++i)
	{
		m_frameStates[i].weight = 0.0f;
		m_frameStates[i].flags = 0;
	}
}

//

bool CVertexAnimation::CompileAdds(CVertexCommandBuffer& commandBuffer)
{
	if (Console::GetInst().ca_vaBlendEnable == 0)
	{
		return false;
	}

	const uint stateCount = uint(m_frameStates.size());

	// When ca_vaBlendCullingDebug is enabled we disable the culling every other frame to show the differences it causes.
	const bool bAllowCulling = Console::GetInst().ca_vaProfile != 1;
	const bool bDebugCulling = Console::GetInst().ca_vaBlendCullingDebug && gEnv->pRenderer->EF_GetSkinningPoolID() % 2;

	for (uint i = 0; i < stateCount; ++i)
	{
		const SVertexFrameState& frameState = m_frameStates[i];
		const SSoftwareVertexFrame* const pVertexFrame = frameState.pFrame;
		const bool bCullFrame = bAllowCulling && (frameState.flags & VA_FRAME_CULLED);
		const bool bUseFrame = std::abs(frameState.weight) > 0.0f && !bCullFrame;

		if (pVertexFrame && (bUseFrame || bDebugCulling))
		{
			if (VertexCommandAdd* pCommand = commandBuffer.AddCommand<VertexCommandAdd>())
			{
				pCommand->pVectors.data = &pVertexFrame->vertices[0].position;
				pCommand->pVectors.iStride = sizeof(pVertexFrame->vertices[0]);
				pCommand->pIndices.data = &pVertexFrame->vertices[0].index;
				pCommand->pIndices.iStride = sizeof(pVertexFrame->vertices[0]);
				pCommand->count = uint(pVertexFrame->vertices.size());
				pCommand->weight = frameState.weight * Console::GetInst().ca_vaScaleFactor;

#ifndef _RELEASE
				if (bUseFrame)
				{
					m_vertexAnimationStats.vertexBlendCount += uint(pVertexFrame->vertices.size());
					m_vertexAnimationStats.activeFrameCount++;
				}
#endif
			}
		}
	}

	return true;
}

bool CVertexAnimation::CompileCommands(CVertexCommandBuffer& commandBuffer)
{
	const bool bAddPostSkinning = Console::GetInst().ca_vaBlendPostSkinning != 0;

#ifndef _RELEASE
	m_vertexAnimationStats.activeFrameCount = 0;
	m_vertexAnimationStats.vertexBlendCount = 0;
#endif

	if (!m_skinData.vertexTransformCount)
		return false;

	uint stateCount = uint(m_frameStates.size());
	if (VertexCommandCopy* pCommand = commandBuffer.AddCommand<VertexCommandCopy>())
	{
		pCommand->pVertexPositions = m_skinData.pVertexPositions;
		pCommand->pVertexColors = m_skinData.pVertexColors;
		pCommand->pVertexCoords = m_skinData.pVertexCoords;

		pCommand->pIndices = m_skinData.pIndices;
	}

	if (m_pClothPiece)
		return m_pClothPiece->CompileCommand(m_skinData, commandBuffer);

	if (!bAddPostSkinning)
		CompileAdds(commandBuffer);

	if (VertexCommandSkin* pCommand = commandBuffer.AddCommand<VertexCommandSkin>())
	{
		pCommand->pTransformations = m_skinData.pTransformations;
		pCommand->pTransformationRemapTable = m_skinData.pTransformationRemapTable;
		pCommand->transformationCount = m_skinData.transformationCount;

		pCommand->pVertexPositions.data = NULL;
		pCommand->pVertexPositions.iStride = 0;
		pCommand->pVertexPositionsPrevious = m_skinData.pVertexPositionsPrevious;
		pCommand->pVertexQTangents = m_skinData.pVertexQTangents;
		pCommand->pVertexTransformIndices = m_skinData.pVertexTransformIndices;
		pCommand->pVertexTransformWeights = m_skinData.pVertexTransformWeights;
		pCommand->vertexTransformCount = m_skinData.vertexTransformCount;
	}

	if (bAddPostSkinning)
		CompileAdds(commandBuffer);

	if (Console::GetInst().ca_vaUpdateTangents && m_skinData.tangetUpdateTriCount)
		CompileTangents(commandBuffer);

	return true;
}

void CVertexAnimation::CompileTangents(CVertexCommandBuffer& commandBuffer)
{
	if (VertexCommandTangents* pCommand = commandBuffer.AddCommand<VertexCommandTangents>())
	{
		if (m_recTangents.size() != m_skinData.vertexCount)
		{
			m_recTangents.resize(m_skinData.vertexCount);
		}

		pCommand->pRecTangets = m_recTangents.begin();
		pCommand->pTangentUpdateData = m_skinData.pTangentUpdateTriangles;
		pCommand->tangetUpdateDataCount = m_skinData.tangetUpdateTriCount;
		pCommand->pTangentUpdateVertIds = m_skinData.pTangentUpdateVertIds;
		pCommand->tangetUpdateVertIdsCount = m_skinData.tangetUpdateVertIdsCount;
	}
}

uint FindVertexFrameIndex(const CSoftwareVertexFrames& vertexFrames, const char* name)
{
	uint frameCount = vertexFrames.GetCount();
	const SSoftwareVertexFrame* pFrames = vertexFrames.GetFrames();
	for (uint i = 0; i < frameCount; ++i)
	{
		if (stricmp(pFrames[i].name.c_str() + 1, name) == 0)
			return i;
	}

	return -1;
}

void CVertexAnimation::CreateFrameStates(const CSoftwareVertexFrames& vertexFrames, const CDefaultSkeleton& skeleton)
{
	static const char MORPH_TARGET_CONTROLLER_SUFFIX[] = "_blendWeightVertex";

	m_frameStates.reserve(vertexFrames.GetCount());

	for (uint32 i = 0, limit = vertexFrames.GetCount(); i < limit; ++i)
	{
		const auto& vertexFrame = vertexFrames.GetFrames()[i];
		const auto& expectedControllerName = stack_string(vertexFrame.name.c_str() + 1) + MORPH_TARGET_CONTROLLER_SUFFIX;

		m_frameStates.push_back();
		m_frameStates.back().pFrame = &vertexFrame;
		m_frameStates.back().jointIndex = skeleton.GetJointIDByName(expectedControllerName);
		m_frameStates.back().frameIndex = i;
		m_frameStates.back().flags = 0;
	}
}

void CVertexAnimation::UpdateFrameWeightsFromPose(const Skeleton::CPoseData& pose)
{
	if (m_overridenWeights)
		return;

	const uint numJoints = pose.GetJointCount();
	const QuatT* pWeights = pose.GetJointsRelative();
	const uint frameCount = uint(m_frameStates.size());
	SVertexFrameState* pFrames = m_frameStates.begin();
	for (uint i = 0; i < frameCount; ++i)
	{
		if (pFrames[i].jointIndex < numJoints)
		{
			pFrames[i].weight = pWeights[pFrames[i].jointIndex].t.x;
			pFrames[i].flags = 0;
		}
	}
}

#ifndef _RELEASE
CVertexAnimationProfiler g_vertexAnimationProfiler;

void CVertexAnimationProfiler::AddVertexAnimationStats(const SVertexAnimationStats& vertexAnimationStats)
{
	m_vertexAnimationStats.push_back(vertexAnimationStats);
}

void CVertexAnimationProfiler::Clear()
{
	m_vertexAnimationStats.clear();
}

void CVertexAnimationProfiler::DrawVertexAnimationStats(uint profileType)
{
	switch (profileType)
	{
	case 1:
	case 2:
		{
			if (profileType == 1)
			{
				g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.3f, ColorF(1.0f, 1.0f, 1.0f, 1.0f), false, "Vertex Animation Profile : detailed stats before culling");
			}
			else
			{
				g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.3f, ColorF(1.0f, 1.0f, 1.0f, 1.0f), false, "Vertex Animation Profile : detailed stats after culling");
			}
			g_YLine += 16;

			DynArray<bool> used(m_vertexAnimationStats.size(), false);

			for (uint i = 0; i < (uint)m_vertexAnimationStats.size(); ++i)
			{
				if (used[i])
				{
					continue;
				}

				const SVertexAnimationStats& stat = m_vertexAnimationStats[i];
				used[i] = true;

				g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.3f, ColorF(1.0f, 1.0f, 1.0f, 1.0f), false, "+ %s", stat.sCharInstanceName.c_str());
				g_YLine += 16;

				const uint vertexBlendMax = stat.vertexCount * 15;

				const ColorF color = stat.vertexBlendCount < vertexBlendMax ? ColorF(1.0f, 1.0f, 1.0f, 1.0f) : ColorF(1.0f, 0.0f, 0.0f, 1.0f);
				g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.3f, color, false,
				                        "| %s : %d skinned vertices, %d active frames, %d blended frames, %d blended vertices\n",
				                        stat.sAttachmentName.c_str(), stat.vertexCount, stat.activeFrameCount, stat.vertexBlendCount, vertexBlendMax);
				g_YLine += 16;

				// Find attachments using the same character instance.
				for (uint j = i + 1; j < (uint)m_vertexAnimationStats.size(); ++j)
				{
					if (stat.pCharInstance == m_vertexAnimationStats[j].pCharInstance)
					{
						const SVertexAnimationStats& stat2 = m_vertexAnimationStats[j];
						used[j] = true;

						const uint vertexBlendMax2 = stat2.vertexCount * 15;

						const ColorF color2 = stat2.vertexBlendCount < vertexBlendMax2 ? ColorF(1.0f, 1.0f, 1.0f, 1.0f) : ColorF(1.0f, 0.0f, 0.0f, 1.0f);
						g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.3f, color2, false,
						                        "| %s : %d skinned vertices, %d active frames, %d blended frames, %d blended vertices\n",
						                        stat2.sAttachmentName.c_str(), stat2.vertexCount, stat2.activeFrameCount, stat2.vertexBlendCount, vertexBlendMax2);
						g_YLine += 16;
					}
				}
			}
		}
		break;
	case 3:
		{
			uint characterCount = 0;
			uint meshCount = m_vertexAnimationStats.size();
			uint vertexCount = 0;
			uint activeFrameCount = 0;
			uint vertexBlendCount = 0;

			DynArray<bool> used(m_vertexAnimationStats.size(), false);

			for (uint i = 0; i < (uint)m_vertexAnimationStats.size(); ++i)
			{
				if (used[i])
				{
					continue;
				}

				const SVertexAnimationStats& stat = m_vertexAnimationStats[i];
				used[i] = true;

				characterCount++;
				vertexCount += stat.vertexCount;
				activeFrameCount += stat.activeFrameCount;
				vertexBlendCount += stat.vertexBlendCount;

				for (uint j = i + 1; j < (uint)m_vertexAnimationStats.size(); ++j)
				{
					if (stat.pCharInstance == m_vertexAnimationStats[j].pCharInstance)
					{
						const SVertexAnimationStats& stat2 = m_vertexAnimationStats[j];
						used[j] = true;

						vertexCount += stat2.vertexCount;
						activeFrameCount += stat2.activeFrameCount;
						vertexBlendCount += stat2.vertexBlendCount;
					}
				}
			}

			const ColorF color = ColorF(1.0f, 1.0f, 1.0f, 1.0f);
			g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.3f, color, false, "Vertex Animation Profile : global stats");
			g_YLine += 16;
			g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.3f, color, false, "%d character instances, %d meshes, %d vertices", characterCount, meshCount, vertexCount);
			g_YLine += 16;
			g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.3f, color, false, "%d blend shapes, %d blended vertices", activeFrameCount, vertexBlendCount);
			g_YLine += 16;
		}
		break;
	default:
		break;
	}
}
#endif
