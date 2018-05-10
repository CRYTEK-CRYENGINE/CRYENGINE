// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Command_Buffer.h"

#include "Command_Commands.h"
#include "CharacterInstance.h"
#include "SkeletonAnim.h"
#include "SkeletonPose.h"

using namespace Command;

/*
   CState
 */

bool CState::Initialize(CCharInstance* pInstance, const QuatTS& location)
{
	m_pInstance = pInstance;
	m_pDefaultSkeleton = pInstance->m_pDefaultSkeleton;

	m_location = location;

	m_pFallbackPoseData = Console::GetInst().ca_ResetCulledJointsToBindPose ? &pInstance->m_SkeletonPose.GetPoseDataDefault() : &pInstance->m_SkeletonPose.GetPoseData();
	m_pPoseData = pInstance->m_SkeletonPose.GetPoseDataWriteable();

	m_jointCount = pInstance->m_pDefaultSkeleton->GetJointCount();

	m_lod = m_pInstance->GetAnimationLOD();

	m_originalTimeDelta = m_pInstance->m_fOriginalDeltaTime;

	m_pJointMask = NULL;
	m_jointMaskCount = 0;

	return true;
}

/*
   CBuffer
 */

bool CBuffer::Initialize(CCharInstance* pInstance, const QuatTS& location)
{
	m_pCommands = m_pBuffer;
	m_commandCount = 0;

	m_state.Initialize(pInstance, location);

	m_pInstance = pInstance;
	return true;
}

void CBuffer::Execute()
{
	DEFINE_PROFILER_FUNCTION();

	if (!m_pInstance->m_SkeletonPose.m_physics.m_bPhysicsRelinquished)
		m_state.m_pPoseData->ResetToDefault(*m_pInstance->m_pDefaultSkeleton);

	// Temporary (stack-local) buffers
	PREFAST_SUPPRESS_WARNING(6255)
	QuatT * pJointsTemp = static_cast<QuatT*>(alloca(m_state.m_jointCount * sizeof(QuatT)));
	PREFAST_SUPPRESS_WARNING(6255)
	float* pScalingTemp = static_cast<float*>(alloca(m_state.m_jointCount * sizeof(float)));
	std::fill(pScalingTemp, pScalingTemp + m_state.m_jointCount, 1.0f);
	PREFAST_SUPPRESS_WARNING(6255)
	Vec3 * pJWeightSumTemp = static_cast<Vec3*>(alloca(m_state.m_jointCount * sizeof(Vec3)));
	PREFAST_SUPPRESS_WARNING(6255)
	JointState * pJointsStatusTemp = static_cast<JointState*>(alloca(m_state.m_jointCount * sizeof(JointState)));
	::memset(pJointsStatusTemp, 0, m_state.m_jointCount * sizeof(JointState));

	// Base buffers
	QuatT* const __restrict pJointsRelative = m_state.m_pPoseData->GetJointsRelative();
	PREFAST_SUPPRESS_WARNING(6255)
	float* const pScalingRelativeDummy = static_cast<float*>(alloca(m_state.m_jointCount * sizeof(float)));
	std::fill(pScalingRelativeDummy, pScalingRelativeDummy + m_state.m_jointCount, 1.0f);
	JointState* const __restrict pJointsStatusBase = m_state.m_pPoseData->GetJointsStatus();
	::memset(pJointsStatusBase, 0, m_state.m_jointCount * sizeof(JointState));

	m_state.m_pJointMask = NULL;
	m_state.m_jointMaskCount = 0;

	CEvaluationContext evalContext = CEvaluationContext();
	evalContext.m_buffers[TmpBuffer + 0] = pJointsTemp;
	evalContext.m_buffers[TmpBuffer + 1] = pJointsStatusTemp;
	evalContext.m_buffers[TmpBuffer + 2] = pJWeightSumTemp;
	evalContext.m_buffers[TmpBuffer + 3] = pScalingTemp;
	evalContext.m_buffers[TargetBuffer + 0] = pJointsRelative;
	evalContext.m_buffers[TargetBuffer + 1] = pJointsStatusBase;
	evalContext.m_buffers[TargetBuffer + 2] = nullptr;
	evalContext.m_buffers[TargetBuffer + 3] = pScalingRelativeDummy;

	uint8* pCommands = m_pBuffer;
	uint32 commandOffset = 0;
	for (uint32 i = 0; i < m_commandCount; ++i)
	{
		uint8& command = pCommands[commandOffset];
		switch (command)
		{
		case ClearPoseBuffer::ID:
			{
				((ClearPoseBuffer&)command).Execute(m_state, evalContext);
				commandOffset += sizeof(ClearPoseBuffer);
				break;
			}

		case PerJointBlending::ID:
			{
				((PerJointBlending&)command).Execute(m_state, evalContext);
				commandOffset += sizeof(PerJointBlending);
				break;
			}

		case SampleAddAnimFull::ID:
			{
				((SampleAddAnimFull&)command).Execute(m_state, evalContext);
				commandOffset += sizeof(SampleAddAnimFull);
				break;
			}

		case AddPoseBuffer::ID:
			{
				((AddPoseBuffer&)command).Execute(m_state, evalContext);
				commandOffset += sizeof(AddPoseBuffer);
				break;
			}

		case NormalizeFull::ID:
			{
				((NormalizeFull&)command).Execute(m_state, evalContext);
				commandOffset += sizeof(NormalizeFull);
				break;
			}

		case ScaleUniformFull::ID:
			{
				((ScaleUniformFull&)command).Execute(m_state, evalContext);
				commandOffset += sizeof(ScaleUniformFull);
				break;
			}
		case PoseModifier::ID:
			{
				((PoseModifier&)command).Execute(m_state, evalContext);
				commandOffset += sizeof(PoseModifier);
				break;
			}

		case SampleAddAnimPart::ID:
			{
				((SampleAddAnimPart&)command).Execute(m_state, evalContext);
				commandOffset += sizeof(SampleAddAnimPart);
				break;
			}

		case JointMask::ID:
			{
				if (Console::GetInst().ca_UseJointMasking)
				{
					m_state.m_pJointMask = ((JointMask&)command).m_pMask;
					m_state.m_jointMaskCount = ((JointMask&)command).m_count;
				}
				commandOffset += sizeof(JointMask);
				break;
			}

		case SampleAddPoseFull::ID:
			{
				((SampleAddPoseFull&)command).Execute(m_state, evalContext);
				commandOffset += sizeof(SampleAddPoseFull);
				break;
			}

		case UpdateRedirectedJoint::ID:
			{
				((UpdateRedirectedJoint&)command).Execute(m_state, evalContext);
				commandOffset += sizeof(UpdateRedirectedJoint);
				break;
			}

		case UpdatePendulumRow::ID:
			{
				((UpdatePendulumRow&)command).Execute(m_state, evalContext);
				commandOffset += sizeof(UpdatePendulumRow);
				break;
			}

		case PrepareAllRedirectedTransformations::ID:
			{
				((PrepareAllRedirectedTransformations&)command).Execute(m_state, evalContext);
				commandOffset += sizeof(PrepareAllRedirectedTransformations);
				break;
			}

		case GenerateProxyModelRelativeTransformations::ID:
			{
				((GenerateProxyModelRelativeTransformations&)command).Execute(m_state, evalContext);
				commandOffset += sizeof(GenerateProxyModelRelativeTransformations);
				break;
			}

		case ComputeAbsolutePose::ID:
			{
				((ComputeAbsolutePose&)command).Execute(m_state, evalContext);
				commandOffset += sizeof(ComputeAbsolutePose);
				break;
			}

		case ProcessAnimationDrivenIk::ID:
			{
				((ProcessAnimationDrivenIk&)command).Execute(m_state, evalContext);
				commandOffset += sizeof(ProcessAnimationDrivenIk);
				break;
			}

		case PhysicsSync::ID:
			{
				((PhysicsSync&)command).Execute(m_state, evalContext);
				commandOffset += sizeof(PhysicsSync);
				break;
			}

#ifdef _DEBUG
		case VerifyFull::ID:
			((VerifyFull&)command).Execute(m_state, evalContext);
			commandOffset += sizeof(VerifyFull);
			break;
#endif
		default:
			CryFatalError("CryAnimation: Command-Buffer Invalid: Command %u  Model: %s", command, m_state.m_pDefaultSkeleton->GetModelFilePath());
			break;
		}

		// In case any of the scaling tracks have been updated, we want to swap the dummy scaling
		// output buffer with the actual lazily-allocated Skeleton::CPose one (a one-time operation).
		if (evalContext.m_isScalingPresent && evalContext.m_buffers[TargetBuffer + 3] == pScalingRelativeDummy)
		{
			if (float* pScalingRelative = m_state.m_pPoseData->GetScalingRelative())
			{
				std::copy(pScalingRelativeDummy, pScalingRelativeDummy + m_state.m_jointCount, pScalingRelative);
				evalContext.m_buffers[TargetBuffer + 3] = pScalingRelative;
			}
		}
	}

	if (Console::GetInst().ca_DebugCommandBuffer)
		DebugDraw();
}

void CBuffer::DebugDraw()
{
	if (Console::GetInst().ca_DebugCommandBufferFilter)
	{
		if ((strcmp(Console::GetInst().ca_DebugCommandBufferFilter, "") != 0) && (strstr(m_state.m_pInstance->m_strFilePath, Console::GetInst().ca_DebugCommandBufferFilter) == nullptr))
		{
			return;
		}
	}

	float charsize = 1.4f;
	uint32 yscan = 14;
	float fColor2[4] = { 1, 0, 1, 1 };
	g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.3f, fColor2, false,
	                        "m_CommandBufferCounter %d  Commands: %d", GetLengthUsed(), m_commandCount);
	g_YLine += 10;

	const IAnimationSet* pAnimationSet = m_state.m_pDefaultSkeleton->GetIAnimationSet();

	uint8* pCommands = m_pBuffer;
	uint32 commandOffset = 0;
	for (uint32 c = 0; c < m_commandCount; c++)
	{
		int32 anim_command = pCommands[commandOffset];
		switch (anim_command)
		{
		case ClearPoseBuffer::ID:
			g_pAuxGeom->Draw2dLabel(1, g_YLine, charsize, fColor2, false, "ClearPoseBuffer");
			g_YLine += yscan;
			commandOffset += sizeof(ClearPoseBuffer);
			break;

		case SampleAddAnimFull::ID:
			{
				const SampleAddAnimFull* pCommand = reinterpret_cast<const SampleAddAnimFull*>(&pCommands[commandOffset]);
				g_pAuxGeom->Draw2dLabel(1, g_YLine, charsize, fColor2, false, "SampleAddAnimFull: w:%f t:%f %s", pCommand->m_fWeight, pCommand->m_fETimeNew, pAnimationSet->GetNameByAnimID(pCommand->m_nEAnimID));
				g_YLine += yscan;
				commandOffset += sizeof(SampleAddAnimFull);
				break;
			}

		case AddPoseBuffer::ID:
			g_pAuxGeom->Draw2dLabel(1, g_YLine, charsize, fColor2, false, "AddPoseBuffer");
			g_YLine += yscan;
			commandOffset += sizeof(AddPoseBuffer);
			break;

		case NormalizeFull::ID:
			g_pAuxGeom->Draw2dLabel(1, g_YLine, charsize, fColor2, false, "NormalizeFull");
			g_YLine += yscan;
			commandOffset += sizeof(NormalizeFull);
			break;

		case ScaleUniformFull::ID:
			g_pAuxGeom->Draw2dLabel(1, g_YLine, charsize, fColor2, false, "ScaleUniformFull");
			g_YLine += yscan;
			commandOffset += sizeof(ScaleUniformFull);
			break;

		case SampleAddAnimPart::ID:
			{
				const SampleAddAnimPart* pCommand = reinterpret_cast<const SampleAddAnimPart*>(&pCommands[commandOffset]);
				g_pAuxGeom->Draw2dLabel(1, g_YLine, charsize, fColor2, false, "SampleAddAnimPart: w:%f t:%f %s", pCommand->m_fWeight, pCommand->m_fAnimTime, pAnimationSet->GetNameByAnimID(pCommand->m_nEAnimID));
				g_YLine += yscan;
				commandOffset += sizeof(SampleAddAnimPart);
				break;
			}

		case PerJointBlending::ID:
			g_pAuxGeom->Draw2dLabel(1, g_YLine, charsize, fColor2, false, "PerJointBlending");
			g_YLine += yscan;
			commandOffset += sizeof(PerJointBlending);
			break;

#ifdef _DEBUG
		case VerifyFull::ID:
			g_pAuxGeom->Draw2dLabel(1, g_YLine, charsize, fColor2, false, "VerifyFull");
			g_YLine += yscan;
			commandOffset += sizeof(VerifyFull);
			break;
#endif

		case PoseModifier::ID:
			g_pAuxGeom->Draw2dLabel(1, g_YLine, charsize, fColor2, false, "PoseModifier: %s", ((PoseModifier*)&pCommands[commandOffset])->m_pPoseModifier->GetFactory()->GetName());
			g_YLine += yscan;
			commandOffset += sizeof(PoseModifier);
			break;

		case JointMask::ID:
			g_pAuxGeom->Draw2dLabel(1, g_YLine, charsize, fColor2, false, "State_JOINTMASK");
			g_YLine += yscan;
			commandOffset += sizeof(JointMask);
			break;

		//just for aim-files
		case SampleAddPoseFull::ID:
			{
				const SampleAddPoseFull* pCommand = reinterpret_cast<const SampleAddPoseFull*>(&pCommands[commandOffset]);
				g_pAuxGeom->Draw2dLabel(1, g_YLine, charsize, fColor2, false, "SampleAddPoseFull: w:%f t:%f %s", pCommand->m_fWeight, pCommand->m_fETimeNew, pAnimationSet->GetNameByAnimID(pCommand->m_nEAnimID));
				g_YLine += yscan;
				commandOffset += sizeof(SampleAddPoseFull);
				break;
			}

		case UpdateRedirectedJoint::ID:
			{
				const UpdateRedirectedJoint* pCommand = reinterpret_cast<const UpdateRedirectedJoint*>(&pCommands[commandOffset]);
				g_pAuxGeom->Draw2dLabel(1, g_YLine, charsize, fColor2, false, "UpdateRedirectedJoint");
				g_YLine += yscan;
				commandOffset += sizeof(UpdateRedirectedJoint);
				break;
			}

		case UpdatePendulumRow::ID:
			{
				const UpdatePendulumRow* pCommand = reinterpret_cast<const UpdatePendulumRow*>(&pCommands[commandOffset]);
				g_pAuxGeom->Draw2dLabel(1, g_YLine, charsize, fColor2, false, "UpdatePendulumRow");
				g_YLine += yscan;
				commandOffset += sizeof(UpdatePendulumRow);
				break;
			}

		case PrepareAllRedirectedTransformations::ID:
			{
				const PrepareAllRedirectedTransformations* pCommand = reinterpret_cast<const PrepareAllRedirectedTransformations*>(&pCommands[commandOffset]);
				g_pAuxGeom->Draw2dLabel(1, g_YLine, charsize, fColor2, false, "PrepareAllRedirectedTransformations");
				g_YLine += yscan;
				commandOffset += sizeof(PrepareAllRedirectedTransformations);
				break;
			}

		case GenerateProxyModelRelativeTransformations::ID:
			{
				const GenerateProxyModelRelativeTransformations* pCommand = reinterpret_cast<const GenerateProxyModelRelativeTransformations*>(&pCommands[commandOffset]);
				g_pAuxGeom->Draw2dLabel(1, g_YLine, charsize, fColor2, false, "GenerateProxyModelRelativeTransformations");
				g_YLine += yscan;
				commandOffset += sizeof(GenerateProxyModelRelativeTransformations);
				break;
			}

		case ComputeAbsolutePose::ID:
			{
				const ComputeAbsolutePose* pCommand = reinterpret_cast<const ComputeAbsolutePose*>(&pCommands[commandOffset]);
				g_pAuxGeom->Draw2dLabel(1, g_YLine, charsize, fColor2, false, "ComputeAbsolutePose");
				g_YLine += yscan;
				commandOffset += sizeof(ComputeAbsolutePose);
				break;
			}

		case ProcessAnimationDrivenIk::ID:
			{
				const ProcessAnimationDrivenIk* pCommand = reinterpret_cast<const ProcessAnimationDrivenIk*>(&pCommands[commandOffset]);
				g_pAuxGeom->Draw2dLabel(1, g_YLine, charsize, fColor2, false, "ProcessAnimationDrivenIk");
				g_YLine += yscan;
				commandOffset += sizeof(ProcessAnimationDrivenIk);
				break;
			}

		case PhysicsSync::ID:
			{
				const PhysicsSync* pCommand = reinterpret_cast<const PhysicsSync*>(&pCommands[commandOffset]);
				g_pAuxGeom->Draw2dLabel(1, g_YLine, charsize, fColor2, false, "PhysicsSync");
				g_YLine += yscan;
				commandOffset += sizeof(PhysicsSync);
				break;
			}

		default:
			CryFatalError("CryAnimation: Command-Buffer Invalid: Command %d  Model: %s", anim_command, m_state.m_pDefaultSkeleton->GetModelFilePath());
			break;
		}
	}
}
