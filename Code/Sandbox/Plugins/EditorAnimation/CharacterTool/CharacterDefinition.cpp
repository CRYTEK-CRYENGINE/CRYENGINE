// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "CharacterDefinition.h"
#include "Serialization.h"
#include "Expected.h"
#include <CryAnimation/ICryAnimation.h>
#include <CryExtension/ICryFactory.h>
#include <CrySerialization/CryExtension.h>
#include <CrySerialization/CryExtensionImpl.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/CryName.h>
#include <CrySerialization/DynArray.h>
#include <CrySerialization/Math.h>
#include <CryExtension/CryCreateClassInstance.h>
#include <Cry3DEngine/IIndexedMesh.h>

#include <IEditor.h>

bool ProxyDimFromGeom(IGeometry *pGeom, QuatT& trans, Vec4& dim)
{
	dim.zero();
	trans.SetIdentity();
	switch (pGeom->GetType())
	{
		case GEOM_BOX: 
		{
			const primitives::box *pbox = (const primitives::box*)pGeom->GetData();
			trans = QuatT(!Quat(pbox->Basis), pbox->center); 
			(Vec3&)dim = pbox->size; 
			return true;
		}
		case GEOM_CAPSULE:
		case GEOM_CYLINDER:
		{
			const primitives::cylinder *pcyl = (const primitives::cylinder*)pGeom->GetData();
			trans = QuatT(Quat::CreateRotationV0V1(Vec3(0,0,1), pcyl->axis), pcyl->center);
			dim.z = pcyl->hh; dim.w = pcyl->r;
			return true;
		}
		case GEOM_SPHERE:
		{
			const primitives::sphere *psph = (const primitives::sphere*)pGeom->GetData();
			trans = QuatT(Quat(IDENTITY), psph->center);
			dim.w = psph->r;
			return true;
		}
		default: return false;
	}
}


namespace CharacterTool
{

// TODO: Replace this by 2 proper structs, one for Rope and one for Cloth settings. Each with
// a custom serialize method.
// Currently we use this function to get the property names, types and some default values.
// This function is a copy-paste from the exact same function in CryAnimation.
static DynArray<SJointProperty> GetPhysInfoProperties_ROPE(const CryBonePhysics& pi, int32 nRopeOrGrid)
{
	DynArray<SJointProperty> res;
	if (nRopeOrGrid == 0)
	{
		res.push_back(SJointProperty("Type", "Rope"));
		res.push_back(SJointProperty("Gravity", pi.framemtx[1][0]));

		float jl = pi.spring_tension[0];
		if (pi.min[0] != 0) jl = RAD2DEG(fabs_tpl(pi.min[0]));
		res.push_back(SJointProperty("JointLimit", jl));
		res.push_back(SJointProperty("JointLimitIncrease", pi.spring_angle[2]));

		float jli = pi.spring_tension[1];
		if (jli <= 0 || jli >= 1) jli = 0.02f;
		res.push_back(SJointProperty("MaxTimestep", jli));
		res.push_back(SJointProperty("Stiffness", max(0.001f, RAD2DEG(pi.max[0]))));
		res.push_back(SJointProperty("StiffnessDecay", RAD2DEG(pi.max[1])));
		res.push_back(SJointProperty("Damping", RAD2DEG(pi.max[2])));
		res.push_back(SJointProperty("Friction", pi.spring_tension[2]));
		res.push_back(SJointProperty("SimpleBlending", !(pi.flags & 4)));
		res.push_back(SJointProperty("Mass", RAD2DEG(fabs_tpl(pi.min[1]))));
		res.push_back(SJointProperty("Thickness", RAD2DEG(fabs_tpl(pi.min[2]))));
		res.push_back(SJointProperty("SleepSpeed", pi.damping[0] - 1.0f));
		res.push_back(SJointProperty("HingeY", (pi.flags & 8) != 0));
		res.push_back(SJointProperty("HingeZ", (pi.flags & 16) != 0));
		res.push_back(SJointProperty("StiffnessControlBone", (float)FtoI(pi.framemtx[0][1] - 1.0f) * (pi.framemtx[0][1] >= 2.0f && pi.framemtx[0][1] < 100.0f)));
		res.push_back(SJointProperty("EnvCollisions", !(pi.flags & 1)));
		res.push_back(SJointProperty("BodyCollisions", !(pi.flags & 2)));
	}

	if (nRopeOrGrid > 0)
	{
		res.push_back(SJointProperty("Type", "Cloth"));
		res.push_back(SJointProperty("MaxTimestep", pi.damping[0]));
		res.push_back(SJointProperty("MaxStretch", pi.damping[1]));
		res.push_back(SJointProperty("Stiffness", RAD2DEG(pi.max[2])));
		res.push_back(SJointProperty("Thickness", pi.damping[2]));
		res.push_back(SJointProperty("Friction", pi.spring_tension[2]));
		res.push_back(SJointProperty("StiffnessNorm", RAD2DEG(pi.max[0])));
		res.push_back(SJointProperty("StiffnessTang", RAD2DEG(pi.max[1])));
		res.push_back(SJointProperty("Damping", pi.spring_tension[0]));
		res.push_back(SJointProperty("AirResistance", pi.spring_tension[1]));
		res.push_back(SJointProperty("StiffnessAnim", pi.min[0]));
		res.push_back(SJointProperty("StiffnessDecayAnim", pi.min[1]));
		res.push_back(SJointProperty("DampingAnim", pi.min[2]));
		res.push_back(SJointProperty("MaxIters", (float)FtoI(pi.framemtx[0][2])));
		res.push_back(SJointProperty("MaxDistAnim", pi.spring_angle[2]));
		res.push_back(SJointProperty("CharacterSpace", pi.framemtx[0][1]));

		res.push_back(SJointProperty("EnvCollisions", !(pi.flags & 1)));
		res.push_back(SJointProperty("BodyCollisions", !(pi.flags & 2)));
	}
	return res;
}

SERIALIZATION_ENUM_BEGIN(AttachmentTypes, "Attachment Type")
SERIALIZATION_ENUM(CA_BONE, "bone", "Joint Attachment")
SERIALIZATION_ENUM(CA_FACE, "face", "Face Attachment")
SERIALIZATION_ENUM(CA_SKIN, "skin", "Skin Attachment")
SERIALIZATION_ENUM(CA_PROX, "prox", "Proxy Attachment")
SERIALIZATION_ENUM(CA_PROW, "prow", "PRow Attachment")
SERIALIZATION_ENUM(CA_VCLOTH, "vcloth", "VCloth 2.0 Attachment")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(SimulationParams, ClampType, "Clamp Mode")
SERIALIZATION_ENUM(SimulationParams::DISABLED, "disabled", "Disabled")
SERIALIZATION_ENUM(SimulationParams::PENDULUM_CONE, "cone", "Pendulum Cone")
SERIALIZATION_ENUM(SimulationParams::PENDULUM_HINGE_PLANE, "hinge", "Pendulum Hinge")
SERIALIZATION_ENUM(SimulationParams::PENDULUM_HALF_CONE, "halfCone", "Pendulum Half Cone")
SERIALIZATION_ENUM(SimulationParams::SPRING_ELLIPSOID, "ellipsoid", "Spring Ellipsoid")
SERIALIZATION_ENUM(SimulationParams::TRANSLATIONAL_PROJECTION, "projection", "Translational Projection")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(RowSimulationParams, ClampMode, "Clamp Mode")
SERIALIZATION_ENUM(RowSimulationParams::PENDULUM_CONE, "cone", "Pendulum Cone")
SERIALIZATION_ENUM(RowSimulationParams::PENDULUM_HINGE_PLANE, "hinge", "Pendulum Hinge")
SERIALIZATION_ENUM(RowSimulationParams::PENDULUM_HALF_CONE, "halfCone", "Pendulum Half Cone")
SERIALIZATION_ENUM(RowSimulationParams::TRANSLATIONAL_PROJECTION, "projection", "Translational Projection")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(CharacterAttachment, ProxyPurpose, "Proxy Pupose")
SERIALIZATION_ENUM(CharacterAttachment::AUXILIARY, "auxiliary", "Auxiliary")
SERIALIZATION_ENUM(CharacterAttachment::CLOTH, "cloth", "Cloth")
SERIALIZATION_ENUM(CharacterAttachment::RAGDOLL, "ragdoll", "Rag Doll")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(SJointPhysics, EType, "JointPhysics Type")
SERIALIZATION_ENUM(SJointPhysics::EType::None, "none", "None")
SERIALIZATION_ENUM(SJointPhysics::EType::Rope, "rope", "Rope")
SERIALIZATION_ENUM(SJointPhysics::EType::Cloth, "cloth", "Cloth")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN(geomtypes, "Geometry Type")
SERIALIZATION_ENUM(GEOM_BOX, "box", "Box")
SERIALIZATION_ENUM(GEOM_SPHERE, "sphere", "Sphere")
SERIALIZATION_ENUM(GEOM_CAPSULE, "capsule", "Capsule")
SERIALIZATION_ENUM(GEOM_CYLINDER, "cylinder", "Cylinder")
SERIALIZATION_ENUM(GEOM_TRIMESH, "mesh", "Mesh")
SERIALIZATION_ENUM_END()

enum primtypes
{
	GEOM_CYLINDER = primitives::cylinder::type,
	GEOM_CAPSULE  = primitives::capsule::type, 
	GEOM_SPHERE   = primitives::sphere::type,
	GEOM_BOX      = primitives::box::type
};

SERIALIZATION_ENUM_BEGIN(primtypes, "Geometry Type")
SERIALIZATION_ENUM(GEOM_BOX, "box", "Box")
SERIALIZATION_ENUM(GEOM_SPHERE, "sphere", "Sphere")
SERIALIZATION_ENUM(GEOM_CAPSULE, "capsule", "Capsule")
SERIALIZATION_ENUM(GEOM_CYLINDER, "cylinder", "Cylinder")
SERIALIZATION_ENUM_END()

enum ProjectionSelection1
{
	PS1_NoProjection,
	PS1_ShortarcRotation,
};

enum ProjectionSelection2
{
	PS2_NoProjection,
	PS2_ShortarcRotation,
	PS2_DirectedRotation,
};

enum ProjectionSelection3
{
	PS3_NoProjection,
	PS3_ShortvecTranslation,
};

enum ProjectionSelection4
{
	PS4_NoProjection,
	PS4_ShortvecTranslation,
	PS4_DirectedTranslation
};

SERIALIZATION_ENUM_BEGIN(ProjectionSelection1, "projectionType")
SERIALIZATION_ENUM(PS1_NoProjection, "no_projection", "No Projection")
SERIALIZATION_ENUM(PS1_ShortarcRotation, "shortarc_rotation", "Shortarc Rotation")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN(ProjectionSelection2, "projectionType")
SERIALIZATION_ENUM(PS2_NoProjection, "no_projection", "No Projection")
SERIALIZATION_ENUM(PS2_ShortarcRotation, "shortarc_rotation", "Shortarc Rotation")
SERIALIZATION_ENUM(PS2_DirectedRotation, "directed_rotation", "Directed Rotation")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN(ProjectionSelection3, "projectionType")
SERIALIZATION_ENUM(PS3_NoProjection, "no_projection", "No Projection")
SERIALIZATION_ENUM(PS3_ShortvecTranslation, "shortvec_translation", "Shortvec Translation")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN(ProjectionSelection4, "projectionType")
SERIALIZATION_ENUM(PS4_NoProjection, "no_projection", "No Projection")
SERIALIZATION_ENUM(PS4_ShortvecTranslation, "shortvec_translation", "Shortvec Translation")
SERIALIZATION_ENUM(PS4_DirectedTranslation, "directed_translation", "Directed Translation")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(CharacterAttachment, TransformSpace, "Position Space")
SERIALIZATION_ENUM(CharacterAttachment::SPACE_CHARACTER, "character", "Relative to Character")
SERIALIZATION_ENUM(CharacterAttachment::SPACE_JOINT, "joint", "Relative to Joint")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN(AttachmentFlags, "Attachment Flags")
SERIALIZATION_ENUM(FLAGS_ATTACH_COMPUTE_SKINNING, "compute_skinning", "Compute Skinning")
SERIALIZATION_ENUM(FLAGS_ATTACH_COMPUTE_SKINNING_PREMORPHS, "compute_premorphs", "Apply morphs pre-skinning")
SERIALIZATION_ENUM(FLAGS_ATTACH_COMPUTE_SKINNING_TANGENTS, "compute_tangents", "Recompute Normals")
SERIALIZATION_ENUM(FLAGS_ATTACH_HIDE_ATTACHMENT, "hide", "Hidden                                                                     ")
SERIALIZATION_ENUM(FLAGS_ATTACH_EXCLUDE_FROM_NEAREST, "exclude_nearest", "Exclude from nearest camera range                                                                     ")
SERIALIZATION_ENUM(FLAGS_ATTACH_PHYSICALIZED_RAYS, "physicalized_rays", "Physicalized Rays                                                          ")
SERIALIZATION_ENUM(FLAGS_ATTACH_PHYSICALIZED_COLLISIONS, "physicalized_collisions", "Physicalized Collisions                                                    ")
SERIALIZATION_ENUM(FLAGS_ATTACH_SW_SKINNING, "cpu_skinning", "CPU Skinning")
SERIALIZATION_ENUM_END()

enum SkinningMethod
{
	SKINNING_VERTEX_SHADER,
	SKINNING_CPU,
	SKINNING_COMPUTE_SHADER
};

SERIALIZATION_ENUM_BEGIN(SkinningMethod, "Skinning Method")
SERIALIZATION_ENUM(SKINNING_VERTEX_SHADER, "vertex_shader", "Vertex Shader (4 weights)")
SERIALIZATION_ENUM(SKINNING_CPU, "cpu", "CPU (slow) (8 weights, morphs, normals)")
SERIALIZATION_ENUM(SKINNING_COMPUTE_SHADER, "compute_shader", "Compute Shader (8 weights, morphs, normals)")
SERIALIZATION_ENUM_END()

// ---------------------------------------------------------------------------

struct ProxySet
{
	DynArray<CCryName>&  names;
	CharacterDefinition* definition;
	ProxySet(DynArray<CCryName>& names, CharacterDefinition* definition)
		: names(names)
		, definition(definition)
	{}
};

bool Serialize(Serialization::IArchive& ar, ProxySet& set, const char* name, const char* label)
{
	if (ar.isEdit() && set.definition)
	{
		if (ar.openBlock(name, label))
		{
			DynArray<CCryName> allProxyNames;
			uint32 numAttachments = set.definition->attachments.size();
			allProxyNames.reserve(numAttachments);
			for (uint32 i = 0; i < numAttachments; i++)
			{
				const CharacterAttachment& att = set.definition->attachments[i];
				const char* strSocketName = att.m_strSocketName.c_str();
				if (att.m_attachmentType == CA_PROX)
					allProxyNames.push_back(CCryName(strSocketName));
			}

			uint32 numProxyNames = allProxyNames.size();
			DynArray<bool> arrUsedProxies(numProxyNames, false);

			arrUsedProxies.resize(numProxyNames);
			uint32 numNames = set.names.size();
			for (uint32 crcIndex = 0; crcIndex < numNames; crcIndex++)
			{
				uint32 nCRC32lower = CCrc32::ComputeLowercase(set.names[crcIndex].c_str());
				for (uint32 proxyIndex = 0; proxyIndex < numProxyNames; ++proxyIndex)
				{
					uint32 nCRC32 = CCrc32::ComputeLowercase(allProxyNames[proxyIndex].c_str());
					if (nCRC32lower == nCRC32)
						arrUsedProxies[proxyIndex] = true;
				}
			}

			// make sure names are preserved even if proxies are not there at the moment
			ar(set.names, "__allUsedNames", 0);

			static std::set<string> staticNames;
			for (uint32 proxyIndex = 0; proxyIndex < numProxyNames; ++proxyIndex)
			{
				const char* proxyName = allProxyNames[proxyIndex].c_str();
				const string& name = *staticNames.insert(proxyName).first;
				ar(arrUsedProxies[proxyIndex], name.c_str(), name.c_str());
			}

			for (uint32 proxyIndex = 0; proxyIndex < numProxyNames; proxyIndex++)
			{
				int index = -1;
				for (int j = 0; j < set.names.size(); ++j)
					if (set.names[j] == allProxyNames[proxyIndex])
						index = j;

				if (arrUsedProxies[proxyIndex])
				{
					if (index < 0)
						set.names.push_back(allProxyNames[proxyIndex]);
				}
				else if (index >= 0)
					set.names.erase(index);
			}
			ar.closeBlock();
			return true;
		}
		return false;
	}
	else
		return ar(set.names, name, label);
}

// ---------------------------------------------------------------------------

struct SimulationParamsSerializer
{
	SimulationParams& p;
	SimulationParamsSerializer(SimulationParams& p) : p(p) {}

	void Serialize(Serialization::IArchive& ar)
	{
		bool inJoint = false;
		bool hasGeometry = false;
		CharacterAttachment* attachment = ar.context<CharacterAttachment>();
		CharacterDefinition* definition = 0;

		if (attachment)
		{
			inJoint = attachment->m_attachmentType == CA_BONE;
			hasGeometry = !attachment->m_strGeometryFilepath.empty();
			definition = attachment->m_definition;
			if (!definition)
				if (CharacterDefinition* definitionFromContext = ar.context<CharacterDefinition>())
					definition = definitionFromContext;
		}

		SimulationParams::ClampType ct = p.m_nClampType;
		ar(ct, "clampType", "^");
		p.m_nClampType = ct;
		if (ct == SimulationParams::PENDULUM_CONE || ct == SimulationParams::PENDULUM_HINGE_PLANE || ct == SimulationParams::PENDULUM_HALF_CONE)
		{
			ar(p.m_useRedirect, "useRedirect", inJoint ? "Redirect to Joint                                                " : 0);
			if (!inJoint || p.m_useRedirect || hasGeometry)
			{
				ar(p.m_useDebugSetup, "useDebug", "Debug Setup                                                           ");
				ar(p.m_useDebugText, "useDebugText", "Debug Text                                                            ");
				ar(p.m_useSimulation, "useSimulation", "Activate Simulation                                                   ");
				ar(Serialization::Range(p.m_nSimFPS, uint8(10), uint8(255)), "simulationFPS", "Simulation FPS");

				ar(p.m_vSimulationAxis, "simulationAxis", "Simulation Axis");
				ar(Serialization::Range(p.m_fMass, 0.0001f, 10.0f), "mass", "Mass");
				ar(Serialization::Range(p.m_fGravity, -90.0f, 90.0f), "gravity", "Gravity");
				ar(Serialization::Range(p.m_fDamping, 0.0f, 10.0f), "damping", "Damping");
				ar(Serialization::Range(p.m_fStiffness, 0.0f, 999.0f), "jointSpring", "Joint Spring");          //in the case of a pendulum its a joint-based force
				Vec2& vStiffnessTarget = (Vec2&)p.m_vStiffnessTarget;
				ar(vStiffnessTarget, "stiffnessTarget", "Spring Target");

				ar(Serialization::Range(p.m_fMaxAngle, 0.0f, 179.0f), "coneAngle", "Cone Angle");
				ar(Serialization::Range(p.m_vDiskRotation.x, 0.0f, 359.0f), "hRotation", "Hinge Rotation");
				ar(p.m_vPivotOffset, "pivotOffset", "Pivot Offset");

				ar(p.m_vCapsule, "capsule", "Capsule");
				p.m_vCapsule.x = clamp_tpl(p.m_vCapsule.x, 0.0f, 2.0f);
				p.m_vCapsule.y = clamp_tpl(p.m_vCapsule.y, 0.0f, 0.5f);

				if (ct == SimulationParams::PENDULUM_CONE || ct == SimulationParams::PENDULUM_HALF_CONE)
					ar((ProjectionSelection1&)p.m_nProjectionType, "projectionType", "Projection Type");
				else
					ar((ProjectionSelection2&)p.m_nProjectionType, "projectionType", "Projection Type");

				if (p.m_nProjectionType)
				{
					ar(ProxySet(p.m_arrProxyNames, definition), "proxyNames", "Available Collision Proxies");
				}
			}
			else
			{
				ar.warning(p.m_useRedirect, "Either Redirection or Geometry should be specified for simulation to function.\n(ignore this warning if you left geometry empty on purpose, e.g. inventory slots)");
			}
		}

		if (ct == SimulationParams::SPRING_ELLIPSOID)
		{
			ar(p.m_useRedirect, "useRedirect", inJoint ? "Redirect to Joint                                               " : 0);
			bool hasGeomtryOrRedirect = inJoint ? (hasGeometry || p.m_useRedirect) : hasGeometry;
			if (hasGeomtryOrRedirect)
			{
				ar(p.m_useDebugSetup, "useDebug", "Debug Setup                                                               ");
				ar(p.m_useDebugText, "useDebugText", "Debug Text                                                                ");
				ar(p.m_useSimulation, "useSimulation", "Activate Simulation                                                       ");
				ar(Serialization::Range(p.m_nSimFPS, uint8(10), uint8(255)), "simulationFPS", "Simulation FPS");

				ar(Serialization::Range(p.m_fMass, 0.0001f, 10.0f), "mass", "Mass");
				ar(Serialization::Range(p.m_fGravity, 0.0001f, 99.0f), "gravity", "Gravity");
				ar(Serialization::Range(p.m_fDamping, 0.0f, 10.0f), "damping", "Damping");
				ar(Serialization::Range(p.m_fStiffness, 0.0f, 999.0f), "Stiffness", "Stiffness");            //in the case of a spring its a position-based force
				ar(p.m_vStiffnessTarget, "stiffnessTarget", "Spring Target");

				ar(Serialization::Range(p.m_fRadius, 0.0f, 10.0f), "diskRadius", "Disk Radius");
				ar(p.m_vSphereScale, "sphereScale", "Sphere Scale");
				p.m_vSphereScale.x = clamp_tpl(p.m_vSphereScale.x, 0.0f, 99.0f);
				p.m_vSphereScale.y = clamp_tpl(p.m_vSphereScale.y, 0.0f, 99.0f);
				ar(p.m_vDiskRotation, "diskRotation", "Disk Rotation");
				p.m_vDiskRotation.x = clamp_tpl(p.m_vDiskRotation.x, 0.0f, 359.0f);
				p.m_vDiskRotation.y = clamp_tpl(p.m_vDiskRotation.y, 0.0f, 359.0f);
				ar(p.m_vPivotOffset, "pivotOffset", "Pivot Offset");

				ar((ProjectionSelection3&)p.m_nProjectionType, "projectionType", "Projection Type");
				if (p.m_nProjectionType)
				{
					if (p.m_nProjectionType == ProjectionSelection3::PS3_ShortvecTranslation)
					{
						ar(p.m_vCapsule.y, "radius", "Radius");
						p.m_vCapsule.x = 0;
						p.m_vCapsule.y = clamp_tpl(p.m_vCapsule.y, 0.0f, 0.5f);
					}
					if (p.m_nProjectionType)
						ar(ProxySet(p.m_arrProxyNames, definition), "proxyNames", "Available Collision Proxies");
				}
			}
			else
			{
				ar.warning(p.m_useRedirect, "Either Redirection or Geometry should be specified for simulation to function.\n(ignore this warning if you left geometry empty on purpose, e.g. inventory slots)");
			}
		}

		if (ct == SimulationParams::TRANSLATIONAL_PROJECTION)
		{
			ar(p.m_useDebugSetup, "useDebug", "Debug Setup                                                           ");
			ar(p.m_useDebugText, "useDebugText", "Debug Text                                                            ");
			ar(p.m_useSimulation, "useProjection", "Activate Projection                                                   ");
			ar(p.m_vPivotOffset, "pivotOffset", "Pivot Offset");

			ar((ProjectionSelection4&)p.m_nProjectionType, "projectionType", "Projection Type");
			if (p.m_nProjectionType)
			{
				if (p.m_nProjectionType == ProjectionSelection4::PS4_ShortvecTranslation)
				{
					ar(p.m_vCapsule.y, "radius", "Radius");
					p.m_vCapsule.x = clamp_tpl(p.m_vCapsule.x, 0.0f, 2.0f);
					p.m_vCapsule.y = clamp_tpl(p.m_vCapsule.y, 0.0f, 0.5f);
				}
				else
				{
					ar(JointName(p.m_strDirTransJoint), "dirTransJoint", "Directional Translation Joint");
					uint32 hasJointName = p.m_strDirTransJoint.length();
					if (hasJointName == 0)
						ar(p.m_vSimulationAxis, "translationAxis", "Translation Axis");
					ar(p.m_vCapsule, "capsule", "Capsule");
					p.m_vCapsule.x = clamp_tpl(p.m_vCapsule.x, 0.0f, 2.0f);
					p.m_vCapsule.y = clamp_tpl(p.m_vCapsule.y, 0.0f, 0.5f);
				}
				if (p.m_nProjectionType)
					ar(ProxySet(p.m_arrProxyNames, definition), "proxyNames", "Available Collision Proxies");
			}

			if (inJoint)
			{
				ICharacterInstance* pICharacterInstance = ar.context<ICharacterInstance>();
				if (inJoint && pICharacterInstance)
				{
					IAttachmentManager* pIAttachmentManager = pICharacterInstance->GetIAttachmentManager();
					Serialization::StringList types;
					types.push_back("none");
					uint32 numFunctions = pIAttachmentManager->GetProcFunctionCount();
					for (uint32 i = 0; i < numFunctions; i++)
					{
						const char* pFunctionName = pIAttachmentManager->GetProcFunctionName(i);
						if (pFunctionName)
							types.push_back(pFunctionName);
					}

					Serialization::StringListValue strProcFunction(types, p.m_strProcFunction.c_str());
					ar(strProcFunction, "procFunction", "Proc Function");
					if (ar.isInput())
					{
						if (strProcFunction.index() <= 0)
							p.m_strProcFunction.reset();
						else
							p.m_strProcFunction = strProcFunction.c_str();
					}
				}
				else
				{
					ar(p.m_strProcFunction, "procFunction", "!Proc Function");
				}
			}
			else
				ar(p.m_strProcFunction, "procFunction", 0);

		}
		//----------------------------------------------
	}
};
}

bool Serialize(Serialization::IArchive& ar, SimulationParams& params, const char* name, const char* label)
{
	CharacterTool::SimulationParamsSerializer s(params);
	return ar(s, name, label);
}

namespace CharacterTool
{

void SJointPhysics::Serialize(Serialization::IArchive& ar)
{
	ar(type, "type", "Type");

	if (type == EType::None)
		return;

	auto& jointProperties = (type == EType::Rope) ? ropeProperties : clothProperties;
	for (int setIndex = 0; setIndex < ePropertySetIndex_COUNT; ++setIndex)
	{
		if (setIndex == ePropertySetIndex_Alive)
			ar.openBlock("aliveProperties", "Properties When Alive");
		else
			ar.openBlock("ragdollProperties", "Properties When Ragdollized");

		if (jointProperties[setIndex].empty())
		{
			CryBonePhysics dummyBonePhysics;
			memset(&dummyBonePhysics, 0, sizeof(dummyBonePhysics));
			jointProperties[setIndex] = GetPhysInfoProperties_ROPE(dummyBonePhysics, (int32)type); // get the names & default values into the jointProperties
		}

		for (int propertyIndex = 1; propertyIndex < jointProperties[setIndex].size(); ++propertyIndex)
		{
			auto& property = jointProperties[setIndex][propertyIndex];

			switch (property.type)
			{
			case 0:
				{
					ar(property.fval, property.name, property.name);
					break;
				}
			case 1:
				{
					ar(property.bval, property.name, property.name);
					break;
				}
			case 2:
				{
					assert(false);
					break;
				}
			}
		}
		ar.closeBlock();
	}
}

void SJointPhysics::LoadFromXml(const XmlNodeRef& node)
{
	if (!node->haveAttr("PhysPropType"))
		return;

	stack_string propType = node->getAttr("PhysPropType");

	type =
	  !strcmp(propType, "Rope")
	  ? EType::Rope
	  : (!strcmp(propType, "Cloth") ? EType::Cloth : EType::None);

	auto& jointProperties = (type == EType::Rope) ? ropeProperties : clothProperties;

	CryBonePhysics dummyBonePhysics;
	memset(&dummyBonePhysics, 0, sizeof(dummyBonePhysics));
	jointProperties[0] = jointProperties[1] = GetPhysInfoProperties_ROPE(dummyBonePhysics, (int32)type); // get the names & default values into the jointProperties

	char buf[32];
	for (int setIndex = 0; setIndex < ePropertySetIndex_COUNT; ++setIndex)
	{
		for (uint32 propertyIndex = 1; propertyIndex < jointProperties[setIndex].size(); ++propertyIndex)
		{
			auto& property = jointProperties[setIndex][propertyIndex];

			cry_sprintf(buf, "lod%d_%s", setIndex, property.name);
			if (property.type == 0)
				node->getAttr(buf, property.fval);
			else
				node->getAttr(buf, property.bval);
		}
	}
}

void SJointPhysics::SaveToXml(XmlNodeRef node) const
{
	if (type == EType::None)
		return;

	node->setAttr("PhysPropType", type == EType::Rope ? "Rope" : "Cloth");

	const auto& jointProperties = (type == EType::Rope) ? ropeProperties : clothProperties;

	char buf[32];
	for (int setIndex = 0; setIndex < ePropertySetIndex_COUNT; ++setIndex)
	{
		for (int propertyIndex = 1; propertyIndex < jointProperties[setIndex].size(); ++propertyIndex)
		{
			const auto& property = jointProperties[setIndex][propertyIndex];
			cry_sprintf(buf, "lod%d_%s", setIndex, property.name);
			if (property.type == 0)
				node->setAttr(buf, property.fval);
			else
				node->setAttr(buf, property.bval);
		}
	}
}

IGeometry *CharacterAttachment::CreateProxyGeom() const
{
	IGeometry *pGeom = nullptr;
	IGeomManager *pGeoman = gEnv->pPhysicalWorld->GetGeomManager();
	switch (m_ProxyType)
	{
		case GEOM_BOX: {
			primitives::box box;
			box.Basis = Matrix33(!m_jointSpacePosition.q);
			box.center = m_jointSpacePosition.t;
			box.size = (Vec3&)m_ProxyParams;
			pGeom = pGeoman->CreatePrimitive(primitives::box::type, &box);
			break;
		}
		case GEOM_SPHERE: {
			primitives::sphere sph;
			sph.center = m_jointSpacePosition.t;
			sph.r = m_ProxyParams.w;
			pGeom = pGeoman->CreatePrimitive(primitives::sphere::type, &sph);
			break;
		}
		case GEOM_CAPSULE: case GEOM_CYLINDER: {
			primitives::cylinder cyl;
			cyl.axis = m_jointSpacePosition.q * Vec3(0,0,1);
			cyl.center = m_jointSpacePosition.t;
			cyl.hh = m_ProxyParams.z;
			cyl.r = m_ProxyParams.w;
			pGeom = pGeoman->CreatePrimitive(m_ProxyType == GEOM_CYLINDER ? primitives::cylinder::type : primitives::capsule::type, &cyl);
			break;
		}
		case GEOM_TRIMESH: {
			if (!m_proxySrc || !m_proxySrc->m_proxyMesh)
				return nullptr;
			if ((m_jointSpacePosition.q | Quat(IDENTITY)) > 0.999f && m_jointSpacePosition.t.len2() < sqr(0.001f))
			{
				m_proxySrc->m_proxyMesh->AddRef();
				return m_proxySrc->m_proxyMesh;
			}
			mesh_data& md = *(mesh_data*)m_proxySrc->m_proxyMesh->GetData();
			Vec3 *vtx = new Vec3[md.nVertices];
			for(int i = 0; i < md.nVertices; i++)
				vtx[i] = m_jointSpacePosition * md.pVertices[i];
			pGeom = pGeoman->CreateMesh(vtx, md.pIndices, md.pMats, md.pForeignIdx, md.nTris, md.flags, 0.0f);
			delete[] vtx;
		}
	}
	return pGeom;
}

void CharacterAttachment::ChangeProxyType()
{
	IGeometry *mesh = m_proxySrc ? (m_proxySrc->m_hullMesh ? m_proxySrc->m_hullMesh : m_proxySrc->m_proxyMesh) : nullptr;
	if (mesh)
	{
		if (m_ProxyType != GEOM_TRIMESH)
		{
			mesh_data& md = *(mesh_data*)mesh->GetData();
			int flags = mesh_approx_box;
			switch (m_ProxyType)
			{
				case GEOM_CYLINDER: flags = mesh_approx_cylinder; break;
				case GEOM_CAPSULE: flags = mesh_approx_capsule; break;
				case GEOM_SPHERE: flags = mesh_approx_sphere; break;
			}
			IGeometry *pPrim = gEnv->pPhysicalWorld->GetGeomManager()->CreateMesh(md.pVertices, md.pIndices, nullptr, nullptr, md.nTris, flags, 1e10f);
			ProxyDimFromGeom(pPrim, m_jointSpacePosition, m_ProxyParams);
			m_ProxyParams *= min(1.0f, (1 + pow(mesh->GetVolume()/pPrim->GetVolume(), 1.0f/3)) * 0.5f);
			pPrim->Release();
		} 
		else
			m_jointSpacePosition.SetIdentity();
	}
	else
	{
		Vec3 size = (Vec3&)m_prevProxyParams, axis(ZERO);
		float r = m_prevProxyParams.w, hh = m_prevProxyParams.z;
		int i;
		#define GSWITCH(a,b) ((int)(a) * 16 + (int)(b))
		switch (GSWITCH(m_prevProxyType, m_ProxyType))
		{
			case GSWITCH(GEOM_BOX, GEOM_CYLINDER):
			case GSWITCH(GEOM_BOX, GEOM_CAPSULE):
				i = max(max(size.x,size.y),size.z)*2.5f > size.x + size.y + size.z ? idxmax3(size) : idxmin3(size);
				hh = size[i];
				r = sqrt(size.GetVolume() * 4 / (hh * gf_PI));
				axis[i] = 1;
				m_jointSpacePosition.q = m_jointSpacePosition.q * Quat::CreateRotationV0V1(Vec3(0,0,1), axis);
				if (m_ProxyType == GEOM_CAPSULE)
					hh = max(0.0f, hh - r * (2.0f/3));
				m_ProxyParams = Vec4(0, 0, hh, r);
				break;
			case GSWITCH(GEOM_BOX, GEOM_SPHERE):
				m_ProxyParams = Vec4(0, 0, 0, pow(size.GetVolume() * (6/gf_PI), 1.0f/3));
				break;

			case GSWITCH(GEOM_CAPSULE, GEOM_BOX):
				m_ProxyParams.z += r * (2.0f/3);
			case GSWITCH(GEOM_CYLINDER, GEOM_BOX):
				m_ProxyParams.x = m_ProxyParams.y = r * sqrt(gf_PI)*0.5f;
				m_ProxyParams.w = 0;
				break;
			case GSWITCH(GEOM_CYLINDER, GEOM_CAPSULE):
				m_ProxyParams.z = max(0.0f, hh - r * (2.0f/3));
				break;
			case GSWITCH(GEOM_CYLINDER, GEOM_SPHERE):
				m_ProxyParams = Vec4(0, 0, 0, pow(sqr(r)*hh*1.5f, 1.0f/3));
				break;

			case GSWITCH(GEOM_CAPSULE, GEOM_CYLINDER):
				m_ProxyParams.z += r * (2.0f/3);
				break;
			case GSWITCH(GEOM_CAPSULE, GEOM_SPHERE):
				m_ProxyParams = Vec4(0, 0, 0, pow((hh*1.5f + r)*sqr(r), 1.0f/3));
				break;

			case GSWITCH(GEOM_SPHERE, GEOM_BOX):
				m_ProxyParams.x = m_ProxyParams.y = m_ProxyParams.z = r * pow(4*gf_PI/3, 1.0f/3) * 0.5f;
				m_ProxyParams.w = 0;
				break;
			case GSWITCH(GEOM_SPHERE, GEOM_CYLINDER):
				m_ProxyParams.z = r * (2.0f/3);
				break;
		}
		#undef GSWITCH			
	}
	m_prevProxyType = m_ProxyType;
	m_prevProxyParams = m_ProxyParams;
	m_positionSpace = m_rotationSpace = SPACE_JOINT;
}

void CharacterAttachment::GenerateMesh()
{
	if (!m_proxySrc)
		return;
	if (m_proxySrc->m_hullMesh && m_meshSmooth > 0)
	{
		IGeometry** pGeoms;
		IGeometry::SProxifyParams pp;
		pp.ncells = 40;
		pp.findPrimLines = 0;
		pp.findPrimSurfaces = 0;
		pp.surfMaxAndMinNorms = 1;
		pp.reuseVox = m_proxySrc->m_proxyMesh && m_proxySrc->m_proxyMesh != m_proxySrc->m_hullMesh ? 1 : 0;
		pp.storeVox = pp.reuseVox ^ 1;
		pp.surfMeshIters = m_meshSmooth;
		if (int ngeoms = m_proxySrc->m_hullMesh->Proxify(pGeoms, &pp))
		{
			for(int i = 1; i < ngeoms; i++)
				if (pGeoms[i]->GetVolume() > pGeoms[0]->GetVolume())
					std::swap(pGeoms[0], pGeoms[i]);
			IPhysUtils *pUtils = gEnv->pPhysicalWorld->GetPhysUtils();
			if (m_meshSmooth > 2)
			{
				mesh_data& md = *(mesh_data*)pGeoms[0]->GetData();
				index_t* pTris = nullptr;
				int nTris = pUtils->qhull(md.pVertices, md.nVertices, pTris);
				m_proxySrc->m_proxyMesh = gEnv->pPhysicalWorld->GetGeomManager()->CreateMesh(md.pVertices, pTris, nullptr,nullptr, nTris, mesh_AABB);
				pUtils->DeletePointer(pTris);
				m_proxySrc->m_proxyMesh->Release();
			}
			else
				m_proxySrc->m_proxyMesh = pGeoms[0];
			for(int i = 0; i < ngeoms; i++)
				pGeoms[i]->Release();
			pUtils->DeletePointer(pGeoms);
		}
	} 
	else
		m_proxySrc->m_proxyMesh = m_proxySrc->m_hullMesh;
	m_prevMeshSmooth = m_meshSmooth;
}

void CharacterAttachment::UpdateMirrorInfo(int idBone, const IDefaultSkeleton& skel)
{
	m_boneTransMirror.SetIdentity();
	int idc = skel.GetJointChildIDAtIndexByID(idBone, 0);
	m_dirChild = idc >= 0 ? skel.GetDefaultRelJointByID(idc).t : Vec3(ZERO);

	// find mirror bones by left-right substring matching ('#' means string start or end)
	const char* mirrorNames[] = { " l ", " r ",  "_l_", "_r_",  "#l ", "#r ",  "#l_", "#r_",  " l#", " r#",  "_l#", "_r#",  "left", "right" };
	string jname0 = m_strJointName, jname1; 
	jname0.MakeLower();
	for(int j = 0, id1 = -1; j < CRY_ARRAY_COUNT(mirrorNames) && id1 < 0; j++)
	{
		int l = strlen(mirrorNames[j]) - 1;
		if (*mirrorNames[j] == '#' && !strncmp(jname0, mirrorNames[j] + 1, l))
			(jname1 = mirrorNames[j ^ 1] + 1).append(jname0.c_str() + l);
		else if (mirrorNames[j][l] == '#' && !strncmp(jname0.c_str() - l, mirrorNames[j], l))
			(jname1 = jname0.substr(0, jname0.length() - l)).append(mirrorNames[j ^ 1], l);
		else if (const char *p = strstr(jname0, mirrorNames[j]))
			jname1 = jname0.substr(0, p - jname0.c_str()) + string(mirrorNames[j ^ 1]) + string(p + l + 1);
		else
			continue;
		m_strJointNameMirror = skel.GetJointNameByID(id1 = skel.GetJointIDByName(jname1));
		m_boneTransMirror = skel.GetDefaultAbsJointByID(id1);
	}
	m_updateMirror = false;
}

void MirrorAttachment(const CharacterAttachment& att0, CharacterAttachment& att1)
{
	Vec3 axes[2], dir = att0.m_dirChild;
	QuatT trans[] = { att0.m_boneTrans, att0.m_boneTransMirror };
	if (!dir.len2()) // use the principal direction of the phys proxy primitive if child direction not available
		dir = Quat(att0.m_jointSpacePosition.q).GetColumn(att0.m_ProxyType == GEOM_BOX ? idxmax3(&att0.m_ProxyParams.x) : 2);
	for(int j = 0; j < 2; j++)
		axes[j] = trans[j].q.GetColumn(idxmax3(dir.abs()));
	Vec3 axis = ((axes[0] - axes[1]).len2() > 0.001f ? axes[0] - axes[1] : trans[0].t - trans[1].t).normalized();
	axis *= sgnnz(axis[idxmax3(axis.abs())]);

	att1.m_strJointName = att0.m_strJointNameMirror;
	att1.m_strJointNameMirror = att0.m_strJointName;
	att1.m_strSocketName = string("$") + att1.m_strJointName;
	att1.m_attachmentType = CA_PROX;
	att1.m_ProxyPurpose = CharacterAttachment::RAGDOLL;
	att1.m_rotationSpace = CharacterAttachment::SPACE_JOINT;
	att1.m_positionSpace = CharacterAttachment::SPACE_JOINT;
	att1.m_boneTrans = att0.m_boneTransMirror;
	att1.m_boneTransMirror = att0.m_boneTrans;
	auto Mirror = [axis](const Vec3& v) { return v - axis * ((axis * v) * 2); };
	att1.m_dirChild = Mirror(trans[0].q * att0.m_dirChild) * trans[1].q;
	Matrix33 mtx;
	for(int j = 0; j < 2; j++)
		mtx.SetColumn(j + 1, !trans[1].q * Mirror(trans[0].q * Quat(att0.m_jointSpacePosition.q).GetColumn(j + 1)));
	mtx.SetColumn(0, mtx.GetColumn1() ^ mtx.GetColumn2());
	att1.m_jointSpacePosition.q = Quat(mtx);
	att1.m_ProxyParams = att1.m_prevProxyParams = att0.m_ProxyParams;
	att1.m_jointSpacePosition.t = Mirror(trans[0].q * att0.m_jointSpacePosition.t) * trans[1].q;
	att1.m_ProxyType = att1.m_prevProxyType = att0.m_ProxyType;
	att1.m_proxySrc = nullptr;
	att1.m_meshSmooth = att1.m_prevMeshSmooth = 0;
	att1.m_characterSpacePosition = trans[1] * att1.m_jointSpacePosition;
	att1.m_limits[0] = -att0.m_limits[1]; att1.m_limits[1] = -att0.m_limits[0];
	mtx = Matrix33(trans[0].q * Quat(att0.m_frame0));
	Vec3 axProj, axNew;
	for(int j = 0; j < 3; j++)
	{
		axProj[j] = (axes[1] * (axNew = Mirror(mtx.GetColumn(j))));
		mtx.SetColumn(j, axNew);
	}
	int icorr = idxmax3(axProj.abs()), iter = 0;
	do 
	{	// we'll have to flip one axis of q0, choose it so that q0 (global and mirrored) is close to trans[1].q
		icorr = inc_mod3[icorr];
		axNew = mtx.GetColumn(inc_mod3[icorr]) ^ mtx.GetColumn(dec_mod3[icorr]);
	} while	(++iter < 2 && (axNew * trans[1].q.GetColumn(icorr)) < 0);
	if (mtx.GetColumn(icorr) * axNew < 0)
		std::swap(att1.m_limits[0][icorr] *= -1, att1.m_limits[1][icorr] *= -1);
	mtx.SetColumn(icorr, axNew);
	att1.m_frame0 = Ang3(Matrix33(!trans[1].q) * mtx);
	for(int j = 0; j < 3; j++)
		att1.m_frame0[j] = ((int)fabs(att1.m_frame0[j] * 10) * 0.1f) * sgnnz(att1.m_frame0[j]);
	att1.m_damping = att0.m_damping;
	att1.m_tension = att0.m_tension;
	att1.m_updateMirror = false;
}

std::map<INT_PTR,_smart_ptr<CharacterAttachment::ProxySource>> CharacterDefinition::g_meshArchive;
int CharacterDefinition::g_meshArchiveUsed = 0;

void CharacterAttachment::Serialize(Serialization::IArchive& ar)
{
	using Serialization::Range;
	using Serialization::LocalToJoint;
	using Serialization::LocalToEntity;
	using Serialization::LocalFrame;

	Serialization::SContext attachmentContext(ar, this);

	if (ar.isEdit() && ar.isOutput())
	{
		auto inlineName = m_strSocketName;
		ar(inlineName, "inlineName", "!^");
		auto inlineType = m_attachmentType;
		ar(inlineType, "inlineType", "!>118>^");
	}

	stack_string oldName = m_strSocketName;
	ar(m_strSocketName, "name", "Name");
	if (m_strSocketName.empty())
		ar.warning(m_strSocketName, "Missing attachment name.");

	ar(m_attachmentType, "type", "Type");

	stack_string oldGeometry = m_strGeometryFilepath;
	if (m_attachmentType == CA_BONE)
	{
		ar(JointName(m_strJointName), "jointName", "Joint");

		ar(ResourceFilePath(m_strGeometryFilepath, "Attachment Geometry (cgf, cga, chr, cdf)|*.cgf;*.cga;*chr;*.cdf", "Objects"), "geometry", "<Geometry");
		ar(Serialization::MaterialPicker(m_strMaterial), "material", "<Material");
		ar(m_viewDistanceMultiplier, "viewDistanceMultiplier", "View Distance Multiplier");

		ar(m_positionSpace, "positionSpace", "Store Position");
		ar(m_rotationSpace, "rotationSpace", "Store Rotation");
		if (ar.isEdit())
		{
			const char* transformNamesBySpaces[] = { "transform_jj", "transform_jc", "transform_cj", "transform_cc" };
			const char* transformName = transformNamesBySpaces[(int)m_rotationSpace * 2 + (int)m_positionSpace];
			QuatT& abs = m_characterSpacePosition;
			QuatT& rel = m_jointSpacePosition;
			bool rotJointSpace = m_rotationSpace == SPACE_JOINT;
			bool posJointSpace = m_positionSpace == SPACE_JOINT;
			ar(LocalFrame(rotJointSpace ? &rel.q : &abs.q, rotJointSpace ? Serialization::SPACE_SOCKET_RELATIVE_TO_JOINT : Serialization::SPACE_SOCKET_RELATIVE_TO_BINDPOSE,
			              posJointSpace ? &rel.t : &abs.t, posJointSpace ? Serialization::SPACE_SOCKET_RELATIVE_TO_JOINT : Serialization::SPACE_SOCKET_RELATIVE_TO_BINDPOSE,
			              m_strSocketName.c_str(), this),
			   transformName,
			   "+Transform"
			   );
			abs.q.Normalize();
			rel.q.Normalize();
		}
		else
		{
			if (m_rotationSpace == SPACE_JOINT)
				ar(m_jointSpacePosition.q, "jointRotation", 0);
			else
				ar(m_characterSpacePosition.q, "characterRotation", 0);
			if (m_positionSpace == SPACE_JOINT)
				ar(m_jointSpacePosition.t, "jointPosition", 0);
			else
				ar(m_characterSpacePosition.t, "characterPosition", 0);
		}

		ar(m_simulationParams, "simulation", "+Simulation");

		int availableFlags = FLAGS_ATTACH_HIDE_ATTACHMENT | FLAGS_ATTACH_PHYSICALIZED_RAYS | FLAGS_ATTACH_PHYSICALIZED_COLLISIONS | FLAGS_ATTACH_EXCLUDE_FROM_NEAREST;
		BitFlags<AttachmentFlags>(m_nFlags, availableFlags).Serialize(ar);
		if ((m_nFlags & FLAGS_ATTACH_HIDE_ATTACHMENT) != 0)
			ar.warning(*this, "Hidden by default.");

		ar(m_jointPhysics, "jointPhysics", "Rope/Cloth Physics");
	}

	if (m_attachmentType == CA_FACE)
	{
		ar(ResourceFilePath(m_strGeometryFilepath, "Attachment Geometry (cgf, cga, chr, cdf)|*.cgf;*.cga;*chr;*.cdf", "Objects"), "geometry", "<Geometry");
		ar(ResourceFilePath(m_strMaterial, "Materials (mtl)|*.mtl", "Materials"), "material", "<Material");
		ar(m_viewDistanceMultiplier, "viewDistanceMultiplier", "View Distance Multiplier");

		if (ar.isInput())
		{
			m_positionSpace = SPACE_CHARACTER;
			m_rotationSpace = SPACE_CHARACTER;
		}
		QuatT& abs = m_characterSpacePosition;
		ar(LocalFrame(&abs.q, Serialization::SPACE_SOCKET_RELATIVE_TO_BINDPOSE, &abs.t, Serialization::SPACE_SOCKET_RELATIVE_TO_BINDPOSE, m_strSocketName.c_str(), this),
		   "characterPosition",
		   "+Transform"
		   );
		abs.q.Normalize();

		if (ar.isEdit())
			ar(m_jointSpacePosition, "jointPosition", 0);

		ar(m_simulationParams, "simulation", "+Simulation");
		if (m_simulationParams.m_nClampType == SimulationParams::TRANSLATIONAL_PROJECTION)
			m_simulationParams.m_nClampType = SimulationParams::DISABLED; //you can't choose this mode on face attachments

		int availableFlags = FLAGS_ATTACH_HIDE_ATTACHMENT | FLAGS_ATTACH_PHYSICALIZED_RAYS | FLAGS_ATTACH_PHYSICALIZED_COLLISIONS | FLAGS_ATTACH_EXCLUDE_FROM_NEAREST;
		BitFlags<AttachmentFlags>(m_nFlags, availableFlags).Serialize(ar);
		if ((m_nFlags & FLAGS_ATTACH_HIDE_ATTACHMENT) != 0)
			ar.warning(*this, "Hidden by default.");
	}

	if (m_attachmentType == CA_SKIN)
	{
		ar(ResourceFilePath(m_strGeometryFilepath, "Attachment Geometry (skin)|*.skin", "Objects"), "geometry", "<Geometry");
		ar(ResourceFilePath(m_strMaterial, "Materials (mtl)|*.mtl", "Materials"), "material", "<Material");
		ar(m_viewDistanceMultiplier, "viewDistanceMultiplier", "View Distance Multiplier");
				
		int availableFlags = FLAGS_ATTACH_HIDE_ATTACHMENT | FLAGS_ATTACH_EXCLUDE_FROM_NEAREST;
		{
			SkinningMethod skinningMethod = SKINNING_VERTEX_SHADER;
			if (m_nFlags & FLAGS_ATTACH_SW_SKINNING)
				skinningMethod = SKINNING_CPU;
			else if (m_nFlags & FLAGS_ATTACH_COMPUTE_SKINNING)
			{
				skinningMethod = SKINNING_COMPUTE_SHADER;
				availableFlags |= FLAGS_ATTACH_COMPUTE_SKINNING_PREMORPHS | FLAGS_ATTACH_COMPUTE_SKINNING_TANGENTS;
			}

			ar(skinningMethod, "skinningMethod", "Skinning Method");

			m_nFlags &= ~(FLAGS_ATTACH_SW_SKINNING | FLAGS_ATTACH_COMPUTE_SKINNING);
			switch (skinningMethod)
			{
			case SKINNING_VERTEX_SHADER:
				break;
			case SKINNING_CPU:
				m_nFlags |= FLAGS_ATTACH_SW_SKINNING;
				break;
			case SKINNING_COMPUTE_SHADER:
				m_nFlags |= FLAGS_ATTACH_COMPUTE_SKINNING;
				break;
			}
		}

		BitFlags<AttachmentFlags>(m_nFlags, availableFlags).Serialize(ar);

		if ((m_nFlags & FLAGS_ATTACH_HIDE_ATTACHMENT) != 0)
			ar.warning(*this, "Hidden by default.");

		ar(m_positionSpace, "positionSpace", 0);
		ar(m_rotationSpace, "rotationSpace", 0);
		ar(m_jointSpacePosition, "relativeJointPosition", 0);
		ar(m_characterSpacePosition, "relativeCharacterPosition", 0);
	}

	if (m_attachmentType == CA_PROX)
	{
		ar(JointName(m_strJointName), "jointName", "Joint");

		ar(m_positionSpace, "positionSpace", "Store Position");
		ar(m_rotationSpace, "rotationSpace", "Store Rotation");
		if (ar.isEdit())
		{
			const char* transformNamesBySpaces[] = { "transform_jj", "transform_jc", "transform_cj", "transform_cc" };
			const char* transformName = transformNamesBySpaces[(int)m_rotationSpace * 2 + (int)m_positionSpace];
			QuatT& abs = m_characterSpacePosition;
			QuatT& rel = m_jointSpacePosition;
			bool rotJointSpace = m_rotationSpace == SPACE_JOINT;
			bool posJointSpace = m_positionSpace == SPACE_JOINT;
			ar(LocalFrame(rotJointSpace ? &rel.q : &abs.q, rotJointSpace ? Serialization::SPACE_JOINT : Serialization::SPACE_SOCKET_RELATIVE_TO_BINDPOSE,
			              posJointSpace ? &rel.t : &abs.t, posJointSpace ? Serialization::SPACE_JOINT : Serialization::SPACE_SOCKET_RELATIVE_TO_BINDPOSE,
			              m_strSocketName.c_str(), this),
			   transformName,
			   "+Transform"
			   );
			abs.q.Normalize();
			rel.q.Normalize();
		}
		else
		{
			if (m_rotationSpace == SPACE_JOINT)
				ar(m_jointSpacePosition.q, "jointRotation", 0);
			else
				ar(m_characterSpacePosition.q, "characterRotation", 0);
			if (m_positionSpace == SPACE_JOINT)
				ar(m_jointSpacePosition.t, "jointPosition", 0);
			else
				ar(m_characterSpacePosition.t, "characterPosition", 0);
		}

		ar(m_ProxyPurpose, "proxyPurpose", "Purpose");

		if (m_ProxyPurpose == CharacterAttachment::RAGDOLL)
		{
			INT_PTR ptr = (INT_PTR)(ProxySource*)m_proxySrc;
			ar(ptr, "src");
			if (ar.isInput())
				m_proxySrc = ptr ? CharacterDefinition::g_meshArchive.find(ptr)->second : nullptr;
			else
				CharacterDefinition::g_meshArchive.insert(std::pair<INT_PTR, _smart_ptr<ProxySource>>(ptr, m_proxySrc));
			if (ar.isOutput() && !m_proxySrc)
				ar((primtypes&)m_ProxyType, "geomtype", "Type");
			else
				ar(m_ProxyType, "geomtype", "Type");
			ar(m_prevProxyType, "prev_geomtype");
			if (m_ProxyType != m_prevProxyType)
				ChangeProxyType();
			else switch (m_ProxyType)
			{
				case GEOM_BOX: 
					ar.openBlock("size", "Half-size");
					ar(Serialization::Range(m_ProxyParams.x, 0.0f, 10.0f, 0.01f), "x", "^");
					ar(Serialization::Range(m_ProxyParams.y, 0.0f, 10.0f, 0.01f), "y", "^");
					ar(Serialization::Range(m_ProxyParams.z, 0.0f, 10.0f, 0.01f), "z", "^");
					ar.closeBlock(); 
					break;
				case GEOM_CAPSULE: case GEOM_CYLINDER:
					ar(Serialization::Range(m_ProxyParams.z, 0.0f, 10.0f, 0.01f), "height", "Half-height");
				case GEOM_SPHERE:
					ar(Serialization::Range(m_ProxyParams.w, 0.0f, 10.0f, 0.01f), "r", "Radius");
					break;
				case GEOM_TRIMESH:
					ar(Serialization::Range(m_meshSmooth, 0, 5), "smooth", m_proxySrc && m_proxySrc->m_hullMesh ? "Mesh Simplification" : nullptr);
					if (m_meshSmooth != m_prevMeshSmooth)
						GenerateMesh();
			}
			for(int i = 0; i < 2; i++) 
			{
				ar.openBlock(&("mina\0maxa"[i * 5]), &("Min Angle\0Max Angle"[i * 10]));
				for(int j = 0; j < 3; j++)
					ar(Serialization::Range(m_limits[i][j], -180, 180, 5), &("x\0y\0z"[j * 2]), "^");
				ar.closeBlock();
			}
			ar(Serialization::RadiansAsDeg(m_frame0), "frame0", "Rotation0");
			ar(m_tension, "tension", "Spring Tension");
			ar(m_damping, "damping", "Spring Damping");
			ar(m_strJointNameMirror, "mirrorName");
			ar(m_boneTrans, "boneTrans");
			ar(m_boneTransMirror, "boneTransMirror");
			ar(m_dirChild, "dirChild");
			ar(m_prevProxyParams, "prevDim");
			if (m_ProxyType != GEOM_TRIMESH && *m_strJointNameMirror)
				ar(Serialization::ToggleButton(m_updateMirror), "mirrorUpd", "Update L/R Mirror");
		}	
		else if (ar.openBlock("lozenge", "+Lozenge"))
		{
			ar.doc("The Lozenge collision primitive is defined by 4 numbers.\n"
			       "With these 4 numbers, points, spheres, capsules, line-segments,\n"
			       "rectangles, boxes, 2D-Lozenges and 3D-Lozenges can be created.\n"
			       "That is 8 different shapes (and everything in-between) that can be\n"
			       "used to approximate the shape of arms, legs and the torso of a human body.\n"
			       "If all numbers are non-zero, the shape is a box with edge lengths defined\n"
			       "by \"Size/X/Y/Z\" and a rounded border with a width defined by \"Border Width\".");
			ar(Serialization::Range(m_ProxyParams.w, 0.0f, 1.0f), "borderWidth", "Border Width");
			ar.doc("The width of the rounded border around the box of extends defined below. "
			       "If no XYZ is set, this is the radius of a sphere.");
			ar(Serialization::RadiusWithRangeAsDiameter(m_ProxyParams.x, 0.0f, 1.0f), "sizeX", "Size X");
			ar.doc("The size of the box in the X dimension without the border.");
			ar(Serialization::RadiusWithRangeAsDiameter(m_ProxyParams.y, 0.0f, 1.0f), "sizeY", "Size Y");
			ar.doc("The size of the box in the Y dimension without the border.");
			ar(Serialization::RadiusWithRangeAsDiameter(m_ProxyParams.z, 0.0f, 1.0f), "sizeZ", "Size Z");
			ar.doc("The size of the box in the Z dimension without the border.");
			ar.closeBlock();
		}
	}

	if (m_attachmentType == CA_PROW)
	{
		ar(JointName(m_strRowJointName), "rowJointName", "Row Joint Name");

		ar(m_rowSimulationParams.m_nClampMode, "clampMode", "Clamp Mode");
		ar(m_rowSimulationParams.m_useDebugSetup, "useDebug", "Debug Setup                                                           ");
		ar(m_rowSimulationParams.m_useDebugText, "useDebugText", "Debug Text                                                            ");
		ar(m_rowSimulationParams.m_useSimulation, "useSimulation", "Activate Simulation                                                   ");

		RowSimulationParams::ClampMode ct = m_rowSimulationParams.m_nClampMode;
		if (ct == RowSimulationParams::TRANSLATIONAL_PROJECTION)
		{
			ar((ProjectionSelection4&)m_rowSimulationParams.m_nProjectionType, "projectionType", "Projection Type");
			if (m_rowSimulationParams.m_nProjectionType)
			{
				if (m_rowSimulationParams.m_nProjectionType == ProjectionSelection4::PS4_ShortvecTranslation)
				{
					ar(m_rowSimulationParams.m_vCapsule.y, "radius", "Radius");
					m_rowSimulationParams.m_vCapsule.x = 0;
					m_rowSimulationParams.m_vCapsule.y = clamp_tpl(m_rowSimulationParams.m_vCapsule.y, 0.0f, 0.5f);
				}
				else
				{
					ar(JointName(m_rowSimulationParams.m_strDirTransJoint), "dirTransJoint", "Directional Translation Joint");
					uint32 hasJointName = m_rowSimulationParams.m_strDirTransJoint.length();
					if (hasJointName == 0)
						ar(m_rowSimulationParams.m_vTranslationAxis, "translationAxis", "Translation Axis");
					ar(m_rowSimulationParams.m_vCapsule, "capsule", "Capsule");
					m_rowSimulationParams.m_vCapsule.x = clamp_tpl(m_rowSimulationParams.m_vCapsule.x, 0.0f, 2.0f);
					m_rowSimulationParams.m_vCapsule.y = clamp_tpl(m_rowSimulationParams.m_vCapsule.y, 0.0f, 0.5f);
				}

				if (m_rowSimulationParams.m_nProjectionType)
					ar(ProxySet(m_rowSimulationParams.m_arrProxyNames, m_definition), "proxyNames", "Available Collision Proxies");
			}
		}
		else
		{
			ar(Serialization::Range(m_rowSimulationParams.m_nSimFPS, uint8(10), uint8(255)), "simulationFPS", "Simulation FPS");

			ar(Serialization::Range(m_rowSimulationParams.m_fMass, 0.0001f, 10.0f), "mass", "Mass");
			ar(Serialization::Range(m_rowSimulationParams.m_fGravity, -90.0f, 90.0f), "gravity", "Gravity");
			ar(Serialization::Range(m_rowSimulationParams.m_fDamping, 0.0f, 10.0f), "damping", "Damping");
			ar(Serialization::Range(m_rowSimulationParams.m_fJointSpring, 0.0f, 999.0f), "jointSpring", "Joint Spring");          //in the case of a pendulum its a joint-based force

			ar(Serialization::Range(m_rowSimulationParams.m_fConeAngle, 0.0f, 179.0f), "coneAngle", "Cone Angle");
			ar(m_rowSimulationParams.m_vConeRotation, "coneRotation", "Cone Rotation");
			m_rowSimulationParams.m_vConeRotation.x = clamp_tpl(m_rowSimulationParams.m_vConeRotation.x, -179.0f, +179.0f);
			m_rowSimulationParams.m_vConeRotation.y = clamp_tpl(m_rowSimulationParams.m_vConeRotation.y, -179.0f, +179.0f);
			m_rowSimulationParams.m_vConeRotation.z = clamp_tpl(m_rowSimulationParams.m_vConeRotation.z, -179.0f, +179.0f);
			ar(Serialization::Range(m_rowSimulationParams.m_fRodLength, 0.1f, 5.0f), "rodLength", "Rod Length");
			ar(m_rowSimulationParams.m_vStiffnessTarget, "stiffnessTarget", "Spring Target");
			m_rowSimulationParams.m_vStiffnessTarget.x = clamp_tpl(m_rowSimulationParams.m_vStiffnessTarget.x, -179.0f, +179.0f);
			m_rowSimulationParams.m_vStiffnessTarget.y = clamp_tpl(m_rowSimulationParams.m_vStiffnessTarget.y, -179.0f, +179.0f);
			ar(m_rowSimulationParams.m_vTurbulence, "turbulenceTarget", "Turbulence");
			m_rowSimulationParams.m_vTurbulence.x = clamp_tpl(m_rowSimulationParams.m_vTurbulence.x, 0.0f, 2.0f);
			m_rowSimulationParams.m_vTurbulence.y = clamp_tpl(m_rowSimulationParams.m_vTurbulence.y, 0.0f, 9.0f);
			ar(Serialization::Range(m_rowSimulationParams.m_fMaxVelocity, 0.1f, 100.0f), "maxVelocity", "Max Velocity");

			ar(m_rowSimulationParams.m_cycle, "cycle", "Cycle");
			ar(Serialization::Range(m_rowSimulationParams.m_fStretch, 0.00f, 0.9f), "stretch", "Stretch");
			ar(Serialization::Range(m_rowSimulationParams.m_relaxationLoops, 0u, 20u), "relaxationLoops", "Relax Loops");

			ar(m_rowSimulationParams.m_vCapsule, "capsule", "Capsule");
			m_rowSimulationParams.m_vCapsule.x = clamp_tpl(m_rowSimulationParams.m_vCapsule.x, 0.0f, 2.0f);
			m_rowSimulationParams.m_vCapsule.y = clamp_tpl(m_rowSimulationParams.m_vCapsule.y, 0.0f, 0.5f);

			if (ct == RowSimulationParams::PENDULUM_CONE || ct == RowSimulationParams::PENDULUM_HALF_CONE)
				ar((ProjectionSelection1&)m_rowSimulationParams.m_nProjectionType, "projectionType", "Projection Type");
			else
				ar((ProjectionSelection2&)m_rowSimulationParams.m_nProjectionType, "projectionType", "Projection Type");

			if (m_rowSimulationParams.m_nProjectionType)
				ar(ProxySet(m_rowSimulationParams.m_arrProxyNames, m_definition), "proxyNames", "Available Collision Proxies");
		}

	}

	if (m_attachmentType == CA_VCLOTH)
	{
		if (ar.openBlock("vcloth-parameter", "+Parameter"))
		{
			// be careful: first parameter of parameter widget should not be within an 'openBlock()', otherwise following attachments would not be shown in the GUI
			if (ar.openBlock("animation", "+Animation Control"))
			{
				ar(m_vclothParams.hide, "hide", "Hide");
				ar(m_vclothParams.forceSkinning, "forceSkinning", "Force Skinning");
				ar.doc("If enabled, simulation is skipped and skinning is always enforced.");
				ar(Serialization::Range(m_vclothParams.forceSkinningFpsThreshold, 5.0f, std::numeric_limits<float>::max()), "forceSkinningFpsThreshold", "Force Skinning FPS Thresh");
				ar.doc("If the framerate drops under the provided FPS, simulation is skipped and skinning is enforced.");
				ar(Serialization::Range(m_vclothParams.forceSkinningTranslateThreshold, 0.0f, std::numeric_limits<float>::max()), "forceSkinningTranslateThreshold", "Force Skinning Translate Thresh");
				ar.doc("If the translation exceeds the provided threshold, simulation is skipped and skinning is enforced.");
				ar(m_vclothParams.checkAnimationRewind, "checkAnimationRewind", "Check Animation Rewind");
				ar.doc("Reset particle positions to skinned positions, if a time-jump occurs in the animation (e.g., in case of animation rewind).");
				ar(Serialization::Range(m_vclothParams.disableSimulationAtDistance, 0.0f, std::numeric_limits<float>::max()), "disableSimulationAtDistance", "Disable Simulation At Distance");
				ar.doc("Disable simulation/enable skinning in dependance of camera distance.");
				ar(Serialization::Range(m_vclothParams.disableSimulationTimeRange, 0.0f, 1.0f), "disableSimulationTimeRange", "Disable Simulation Time Range");
				ar.doc("Defines the physical time [in seconds] which is used for fading between simulation and skinning, e.g., 0.5 implies half a second for fading.");
				ar.closeBlock();
			}

			if (ar.openBlock("simulation", "+Simulation and Collision"))
			{
				ar(Serialization::Range(m_vclothParams.timeStep, 0.001f, 0.05f), "timeStep", "Time Step");
				ar.doc("Simulation timestep.");
				ar(Serialization::Range(m_vclothParams.timeStepsMax, 3, 999), "timeStepsMax", "Time Step Max Iterations");
				ar.doc("Maximum number of iterations for the time discretization between two frames.");
				ar(Serialization::Range(m_vclothParams.numIterations, 1, 100), "numIterations", "Coll./Stiffness Iterations");
				ar.doc("Number of stiffness/collision iterations per timestep.");
				ar(Serialization::Range(m_vclothParams.collideEveryNthStep, 0, 10), "collideEveryNthStep", "Collide Every n-th Step");
				ar.doc("During stiffness/collision iterations, only execute collision resolve every n-th step. '0' disables collisions.");
				ar(Serialization::Range(m_vclothParams.collisionMultipleShiftFactor, 0.0f, 1.0f), "collisionMultipleShiftFactor", "Multi-Collisions Shift Factor");
				ar.doc("In case of multiple collisions at the same time, the particle is shifted by this factor into the average direction. '0.0' disables multiple-collision handling. At the moment a maximum of 2 simultaneous collisions is handled.");

				ar(Serialization::Range(m_vclothParams.gravityFactor, -16.0f, 16.0f), "gravityFactor", "Gravity Factor");
				ar.doc("Defines intensity of gravity.");

				ar.closeBlock();
			}

			if (ar.openBlock("stiffness", "+Stiffness and Elasticity"))
			{
				ar(Serialization::Range(m_vclothParams.stretchStiffness, 0.0f, 10.0f), "stretchStiffness", "Stretch Stiffness");
				ar.doc("Smaller values indicate less stiffness, larger values indicate stronger stiffness. (default value: 1.0)");
				ar(Serialization::Range(m_vclothParams.shearStiffness, 0.0f, 1.0f), "shearStiffness", "Shear Stiffness");
				ar.doc("Smaller values indicate less stiffness, larger values indicate stronger stiffness. (value: 0..1)");
				ar(Serialization::Range(m_vclothParams.bendStiffness, 0.0f, 1.0f), "bendStiffness", "Bend Stiffness");
				ar.doc("Smaller values indicate less stiffness, larger values indicate stronger stiffness. (value: 0..1)");

				ar(Serialization::Range(m_vclothParams.bendStiffnessByTrianglesAngle, 0.0f, 1.0f), "bendStiffnessByTrianglesAngle", "Bend Stiffness By Angle");
				ar.doc("Bend stiffness depending on triangle angles, thus, the stiffness is not affecting elasticity.");

				ar(Serialization::Range(m_vclothParams.pullStiffness, 0.0f, 4.0f), "pullStiffness", "Pull Stiffness");
				ar.doc("Strength of pulling pinched vertices towards skinned position. Also used for 'Max Skin Distance' (see below, value: 0..1)");
				ar.closeBlock();
			}

			if (ar.openBlock("damping", "+Friction and Damping"))
			{
				ar(Serialization::Range(m_vclothParams.friction, 0.0f, 2.0f), "friction", "Friction");
				ar.doc("Global friction. Recommended are values between 0.0 and 0.1");
				ar(Serialization::Range(m_vclothParams.rigidDamping, 0.0f, 1.0f), "rigidDamping", "Rigid Damping");
				ar.doc("Damping stiffness into rigid-body/stiff cloth. 0.0 represents no damping, 1.0 represents rigid cloth.");

				// commented out for now, to keep GUI simple, ensure zeros for compatibility with old stored files
				m_vclothParams.springDamping = 0;
				m_vclothParams.springDampingPerSubstep = false;
				m_vclothParams.collisionDampingTangential = 0;
				//ar(Serialization::Range(m_vclothParams.springDamping, 0.0f, 1.0f), "springDamping", "Spring Damping");
				//ar.doc("Damping factor of springs.");
				//ar(m_vclothParams.springDampingPerSubstep, "springDampingPerSubstep", "Spring Damping Per Substep");
				//ar.doc("Enable spring damping for each substep of the collision/stiffness solver.");
				//ar(m_vclothParams.collisionDampingTangential, "collisionDampingTangential", "Collision Friction Tangential");
				//ar.doc("Tangential friction for collided particles. Normal direction is preserved, if not pointing inwards.");
				ar.closeBlock();
			}

			if (ar.openBlock("longrangeattachments", "+Long Range Attachments"))
			{
				ar(m_vclothParams.longRangeAttachments, "longRangeAttachments", "Long Range Attachments");
				ar.doc("Enables LRA for improved stiffness, while reducing elasticity.");
				ar(Serialization::Range(m_vclothParams.longRangeAttachmentsMaximumShiftFactor, 0.0f, 0.8f), "longRangeAttachmentsMaximumShiftFactor", "LRA maximum shift factor");
				ar.doc("Scales maximum shift per iteration in direction of closest neighbor, e.g. 0.5 -> half way to closest neighbor. Smaller values result in higher stability.");
				ar(Serialization::Range(m_vclothParams.longRangeAttachmentsShiftCollisionFactor, -2.0f, 2.0f), "longRangeAttachmentsShiftCollisionFactor", "LRA shift collision factor");
				ar.doc("Scales in case of shift the velocity, 0.0=no shift, 1.0=no velocity change, -1=increase velocity by change.");
				ar(Serialization::Range(m_vclothParams.longRangeAttachmentsAllowedExtension, 0.0f, 1.0f), "longRangeAttachmentsAllowedExtension", "LRA allowed extension");
				ar.doc("Allowed extension for Long Range Attachments, e.g. 0.1 = 10%");
				ar.closeBlock();
			}

			if (ar.openBlock("other", "+Additional"))
			{
				// commented out for now, to keep GUI simple, ensure zeros for compatibility with old stored files
				m_vclothParams.translationBlend = 0;
				m_vclothParams.rotationBlend = 0;
				m_vclothParams.externalBlend = 0;
				//ar(m_vclothParams.translationBlend, "translationBlend", "Translation Blend");
				//ar.doc("Blending factor between local and world space translation.");
				//ar(m_vclothParams.rotationBlend, "rotationBlend", "Rotation Blend");
				//ar.doc("Blending factor between local and world space rotation.");
				//ar(m_vclothParams.externalBlend, "externalBlend", "External Blend");
				//ar.doc("Blending factor of world transformation.");
				ar(m_vclothParams.maxAnimDistance, "maxAnimDistance", "Max Skin Distance");
				ar.doc("Maximum allowed particle distance from skinned position.");
				ar(Serialization::Range(m_vclothParams.filterLaplace, 0.0f, 1.0f), "filterLaplace", "Mesh Filter Laplace");
				ar.doc("Enables post-process laplace filter for the simulation mesh. Accept values between 0 and 1, whereas 0.0 means no filtering and 1.0 full filtering.");
				ar.closeBlock();
			}

			if (ar.openBlock("ResetDamping", "+Reset Damping"))
			{
				ar(Serialization::Range(m_vclothParams.resetDampingRange, 0, 999), "resetDampingRange", "Reset Damping N Frames");
				ar.doc("Damps particles velocity for N frames after reset, to prevent cloth from strong fluttering.");
				ar(Serialization::Range(m_vclothParams.resetDampingFactor, 0.0f, 1.0f), "resetDampingFactor", "Reset Damping Factor");
				ar.doc("Strength of initial damping.");
				ar.closeBlock();
			}

			if (ar.openBlock("files", "+Files"))
			{
				ar(m_vclothParams.isMainCharacter, "isMainCharacter", "isMainCharacter");
				ar(m_vclothParams.renderMeshName, "renderMeshName", "renderMeshName");
				ar(ResourceFilePath(m_vclothParams.renderBinding, "Render Skin (.skin)|*.skin", "Characters"), "Binding", "Binding");
				ar(m_vclothParams.simMeshName, "simMeshName", "simMeshName");
				ar(ResourceFilePath(m_vclothParams.simBinding, "Simulation Skin (.skin)|*.skin", "Characters"), "simBinding", "simBinding");
				ar(ResourceFilePath(m_vclothParams.material, "Materials (mtl)|*.mtl", "Materials"), "material", "material");
				ar.closeBlock();
			}

			if (ar.openBlock("debug", "+Debug"))
			{
				ar(Serialization::Range(m_vclothParams.debugDrawVerticesRadius, 0.0f, 1.0f), "debugDrawVerticesRadius", "Draw Vertices Radius");
				ar(Serialization::Range(m_vclothParams.debugDrawCloth,0, std::numeric_limits<int>::max()), "debugDrawCloth", "Draw Cloth");
				ar(Serialization::Range(m_vclothParams.debugDrawLRA,0, std::numeric_limits<int>::max()), "debugDrawLRA", "Draw Long Range Attachments");
				ar(Serialization::Range(m_vclothParams.debugPrint,0, std::numeric_limits<int>::max()), "debugPrint", "Debug");
				ar.closeBlock();
			}

			ar.closeBlock(); // close "vcloth-parameter"
		}
	}

	// Keep character/joint-space positions in sync. Important to run this with
	// ar.isOutput() as this is the only place when conversion occurs before
	// applying definition to character.
	if (ICharacterInstance* characterInstance = ar.context<ICharacterInstance>())
	{
		const IDefaultSkeleton& defaultSkeleton = characterInstance->GetIDefaultSkeleton();
		int id = defaultSkeleton.GetJointIDByName(m_strJointName.c_str());
		QuatT defaultJointTransform = IDENTITY;
		if (id != -1)
			defaultJointTransform = defaultSkeleton.GetDefaultAbsJointByID(id);
		if (m_rotationSpace == SPACE_JOINT)
			m_characterSpacePosition.q = (defaultJointTransform * m_jointSpacePosition).q;
		else
			m_jointSpacePosition.q = (defaultJointTransform.GetInverted() * m_characterSpacePosition).q;

		m_characterSpacePosition.q.Normalize();
		m_jointSpacePosition.q.Normalize();

		if (m_positionSpace == SPACE_JOINT)
			m_characterSpacePosition.t = (defaultJointTransform * m_jointSpacePosition).t;
		else
			m_jointSpacePosition.t = (defaultJointTransform.GetInverted() * m_characterSpacePosition).t;

		if (m_simulationParams.m_nClampType == SimulationParams::TRANSLATIONAL_PROJECTION)
		{
			m_characterSpacePosition = defaultJointTransform;
			m_jointSpacePosition.SetIdentity();
		}
	}

	if (ar.isInput())
	{
		// assign attachment name based on geometry file path when geometry is changing, by the name is empty
		if (m_strSocketName.empty() && oldName.empty() && oldGeometry.empty() && !m_strGeometryFilepath.empty())
			m_strSocketName = PathUtil::GetFileName(m_strGeometryFilepath.c_str());
	}
}

// ---------------------------------------------------------------------------

void CharacterDefinition::Serialize(Serialization::IArchive& ar)
{
	Serialization::SContext definitionContext(ar, this);

	ar(SkeletonOrCgaPath(skeleton), "skeleton", "Skeleton");
	ar.doc("The main skeleton of this character. (it can optionally be extended using attachments of type 'skin')");
	if (skeleton.empty())
		ar.warning(skeleton, "A skeleton is required for every character.");
	ar(MaterialPicker(materialPath), "material", "Skeleton Material");
	ar.doc("Optional material to use on the geometry of the skeleton.");
	// evgenya: .rig and .phys references are temporarily disabled
	ar(CharacterPhysicsPath(physics), "physics", 0);
	ar(CharacterRigPath(rig), "rig", 0);
	bool physEditPrev = m_physEdit;
	ar(m_physEdit, "physedit");
	ar(attachments, "attachments", "Attachments");
	m_initialized = ar.context<ICharacterInstance>() != 0;

	if (ar.isInput() && !m_physEdit && physEditPrev)
	{
		attachments.insert(attachments.end(), origBonePhysAtt.begin(), origBonePhysAtt.end());
		m_physEdit = m_physNeedsApply = true;
	}

	if (ar.isInput())
	{
		for (size_t i = 0; i < attachments.size(); ++i)
		{
			CharacterAttachment& att = attachments[i];
			att.m_definition = this;
			if (att.m_attachmentType == CA_PROX && att.m_ProxyPurpose == CharacterAttachment::RAGDOLL && att.m_updateMirror && att.m_ProxyType != GEOM_TRIMESH && *att.m_strJointNameMirror)
			{
				att.m_updateMirror = false;
				auto mirror = std::find_if(attachments.begin(), attachments.end(), 
					[att](auto &att1) { return !strcmp(att.m_strJointNameMirror, att1.m_strSocketName.c_str() + 1) && att1.m_strSocketName[0] == '$'; });
				MirrorAttachment(att, mirror == attachments.end() ? (attachments.push_back(CharacterAttachment()), attachments.back()) : *mirror);
			}
		}
	}

	if (attachments.empty())
		ar.warning(attachments, "Add attachments to the skeleton to create character geometry.\nIf you want to see the skeleton: go to 'Display Options', 'Skeleton' and select 'Joints'.");

	if (ar.isInput() && !modifiers)
		CryCreateClassInstanceForInterface(cryiidof<IAnimationPoseModifierSetup>(), modifiers);
	if (modifiers)
		modifiers->Serialize(ar);
}

bool CharacterDefinition::LoadFromXmlFile(const char* filename)
{
	XmlNodeRef root = GetIEditor()->GetSystem()->LoadXmlFromFile(filename);
	if (root == 0)
		return false;

	return LoadFromXml(root);
}

bool CharacterDefinition::LoadFromXml(const XmlNodeRef& root)
{
	m_initialized = false;
	uint32 numChilds = root->getChildCount();
	if (numChilds == 0)
		return false;

	struct tagname
	{
		uint32          m_crc32;
		AttachmentTypes m_type;
		bool            m_dsetup;
		bool            m_dtext;
		bool            m_active;
		tagname(uint32 crc32, AttachmentTypes type, bool dsetup, bool dtext, bool active)
		{
			m_crc32 = crc32;
			;
			m_type = type;
			m_dsetup = dsetup;
			m_dtext = dtext;
			m_active = active;
		}
	};
	DynArray<tagname> arrDebugSetup;
	uint32 numAttachments = attachments.size();
	for (uint32 i = 0; i < numAttachments; i++)
	{
		bool dsetup = attachments[i].m_simulationParams.m_useDebugSetup;
		bool dtext = attachments[i].m_simulationParams.m_useDebugText;
		bool dsim = attachments[i].m_simulationParams.m_useSimulation;
		if (dsetup || dtext || dsim == 0)
		{
			uint32 crc32 = CCrc32::ComputeLowercase(attachments[i].m_strSocketName.c_str());
			AttachmentTypes type = attachments[i].m_attachmentType;
			arrDebugSetup.push_back(tagname(crc32, type, dsetup, dtext, dsim));
		}
	}

	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------

	for (uint32 xmlnode = 0; xmlnode < numChilds; xmlnode++)
	{
		XmlNodeRef node = root->getChild(xmlnode);
		const char* tag = node->getTag();

		//-----------------------------------------------------------
		//load base model
		//-----------------------------------------------------------
		if (strcmp(tag, "Model") == 0)
		{
			skeleton = node->getAttr("File");
			materialPath = node->getAttr("Material");
			physics = node->getAttr("Physics");
			rig = node->getAttr("Rig File");
		}

		//-----------------------------------------------------------
		//load attachment-list
		//-----------------------------------------------------------
		if (strcmp(tag, "AttachmentList") == 0)
		{
			XmlNodeRef nodeAttachements = node;
			uint32 num = nodeAttachements->getChildCount();
			attachments.clear();
			attachments.reserve(num);

			for (uint32 i = 0; i < num; i++)
			{
				CharacterAttachment attach;
				XmlNodeRef nodeAttach = nodeAttachements->getChild(i);
				const char* AttachTag = nodeAttach->getTag();
				if (strcmp(AttachTag, "Attachment"))
					continue; //invalid

				stack_string Type = nodeAttach->getAttr("Type");
				attach.m_attachmentType = CA_Invalid;
				if (Type == "CA_BONE")
					attach.m_attachmentType = CA_BONE;
				if (Type == "CA_FACE")
					attach.m_attachmentType = CA_FACE;
				if (Type == "CA_SKIN")
					attach.m_attachmentType = CA_SKIN;
				if (Type == "CA_PROX")
					attach.m_attachmentType = CA_PROX;
				if (Type == "CA_PROW")
					attach.m_attachmentType = CA_PROW;
				if (Type == "CA_VCLOTH")
					attach.m_attachmentType = CA_VCLOTH;

				attach.m_strSocketName = nodeAttach->getAttr("AName");

				QuatT& abstransform = attach.m_characterSpacePosition;
				nodeAttach->getAttr("Rotation", abstransform.q);
				nodeAttach->getAttr("Position", abstransform.t);

				QuatT& reltransform = attach.m_jointSpacePosition;
				attach.m_rotationSpace = nodeAttach->getAttr("RelRotation", reltransform.q) ? attach.SPACE_JOINT : attach.SPACE_CHARACTER;
				attach.m_positionSpace = nodeAttach->getAttr("RelPosition", reltransform.t) ? attach.SPACE_JOINT : attach.SPACE_CHARACTER;

				attach.m_strJointName = nodeAttach->getAttr("BoneName");
				attach.m_strGeometryFilepath = nodeAttach->getAttr("Binding");
				attach.m_strMaterial = nodeAttach->getAttr("Material");

				nodeAttach->getAttr("ProxyParams", attach.m_ProxyParams);
				uint32 nProxyPurpose = 0;
				nodeAttach->getAttr("ProxyPurpose", nProxyPurpose), attach.m_ProxyPurpose = CharacterAttachment::ProxyPurpose(nProxyPurpose);

				SimulationParams::ClampType ct = SimulationParams::DISABLED;
				nodeAttach->getAttr("PA_PendulumType", (int&)ct);
				if (ct == SimulationParams::PENDULUM_CONE || ct == SimulationParams::PENDULUM_HINGE_PLANE || ct == SimulationParams::PENDULUM_HALF_CONE)
				{
					attach.m_simulationParams.m_nClampType = ct;
					nodeAttach->getAttr("PA_FPS", attach.m_simulationParams.m_nSimFPS);
					nodeAttach->getAttr("PA_Redirect", attach.m_simulationParams.m_useRedirect);

					nodeAttach->getAttr("PA_MaxAngle", attach.m_simulationParams.m_fMaxAngle);
					nodeAttach->getAttr("PA_HRotation", attach.m_simulationParams.m_vDiskRotation.x);

					nodeAttach->getAttr("PA_Mass", attach.m_simulationParams.m_fMass);
					nodeAttach->getAttr("PA_Gravity", attach.m_simulationParams.m_fGravity);
					nodeAttach->getAttr("PA_Damping", attach.m_simulationParams.m_fDamping);
					nodeAttach->getAttr("PA_Stiffness", attach.m_simulationParams.m_fStiffness);

					nodeAttach->getAttr("PA_PivotOffset", attach.m_simulationParams.m_vPivotOffset);
					nodeAttach->getAttr("PA_SimulationAxis", attach.m_simulationParams.m_vSimulationAxis);
					nodeAttach->getAttr("PA_StiffnessTarget", attach.m_simulationParams.m_vStiffnessTarget);

					nodeAttach->getAttr("PA_ProjectionType", attach.m_simulationParams.m_nProjectionType);
					nodeAttach->getAttr("PA_CapsuleX", attach.m_simulationParams.m_vCapsule.x);
					nodeAttach->getAttr("PA_CapsuleY", attach.m_simulationParams.m_vCapsule.y);
					attach.m_simulationParams.m_strDirTransJoint = nodeAttach->getAttr("PA_DirTransJointName");
					uint32 IsIdentical = stricmp(attach.m_simulationParams.m_strDirTransJoint.c_str(), attach.m_strJointName.c_str()) == 0;
					if (attach.m_simulationParams.m_strDirTransJoint.length() && IsIdentical)
						attach.m_simulationParams.m_strDirTransJoint.reset();

					char proxytag[] = "PA_Proxy00";
					for (uint32 i = 0; i < SimulationParams::MaxCollisionProxies; i++)
					{
						CCryName strProxyName = CCryName(nodeAttach->getAttr(proxytag));
						proxytag[9]++;
						if (strProxyName.empty())
							continue;
						attach.m_simulationParams.m_arrProxyNames.push_back(strProxyName);
					}
				}

				uint32 isSpring = 0;
				nodeAttach->getAttr("SA_SpringType", isSpring);
				if (isSpring)
				{
					attach.m_simulationParams.m_nClampType = SimulationParams::SPRING_ELLIPSOID;
					nodeAttach->getAttr("SA_FPS", attach.m_simulationParams.m_nSimFPS);

					nodeAttach->getAttr("SA_Radius", attach.m_simulationParams.m_fRadius);
					nodeAttach->getAttr("SA_ScaleZP", attach.m_simulationParams.m_vSphereScale.x);
					nodeAttach->getAttr("SA_ScaleZN", attach.m_simulationParams.m_vSphereScale.y);
					nodeAttach->getAttr("SA_DiskRotX", attach.m_simulationParams.m_vDiskRotation.x);
					nodeAttach->getAttr("SA_DiskRotZ", attach.m_simulationParams.m_vDiskRotation.y);
					nodeAttach->getAttr("SA_HRotation", attach.m_simulationParams.m_vDiskRotation.x);         //just for backwards compatibility

					nodeAttach->getAttr("SA_Redirect", attach.m_simulationParams.m_useRedirect);

					nodeAttach->getAttr("SA_Mass", attach.m_simulationParams.m_fMass);
					nodeAttach->getAttr("SA_Gravity", attach.m_simulationParams.m_fGravity);
					nodeAttach->getAttr("SA_Damping", attach.m_simulationParams.m_fDamping);
					nodeAttach->getAttr("SA_Stiffness", attach.m_simulationParams.m_fStiffness);

					nodeAttach->getAttr("SA_PivotOffset", attach.m_simulationParams.m_vPivotOffset);
					nodeAttach->getAttr("SA_StiffnessTarget", attach.m_simulationParams.m_vStiffnessTarget);

					nodeAttach->getAttr("SA_ProjectionType", attach.m_simulationParams.m_nProjectionType);
					attach.m_simulationParams.m_vCapsule.x = 0;
					nodeAttach->getAttr("SA_CapsuleY", attach.m_simulationParams.m_vCapsule.y);
					char proxytag[] = "SA_Proxy00";
					for (uint32 i = 0; i < SimulationParams::MaxCollisionProxies; i++)
					{
						CCryName strProxyName = CCryName(nodeAttach->getAttr(proxytag));
						proxytag[9]++;
						if (strProxyName.empty())
							continue;
						attach.m_simulationParams.m_arrProxyNames.push_back(strProxyName);
					}
				}

				uint32 isProjection = 0;
				nodeAttach->getAttr("P_Projection", isProjection);
				if (isProjection)
				{
					attach.m_simulationParams.m_nClampType = SimulationParams::TRANSLATIONAL_PROJECTION;
					attach.m_simulationParams.m_useRedirect = 1;
					nodeAttach->getAttr("P_ProjectionType", attach.m_simulationParams.m_nProjectionType);

					attach.m_simulationParams.m_strDirTransJoint = nodeAttach->getAttr("P_DirTransJointName");
					uint32 IsIdentical = stricmp(attach.m_simulationParams.m_strDirTransJoint.c_str(), attach.m_strJointName.c_str()) == 0;
					if (attach.m_simulationParams.m_strDirTransJoint.length() && IsIdentical)
						attach.m_simulationParams.m_strDirTransJoint.reset();
					nodeAttach->getAttr("P_TranslationAxis", attach.m_simulationParams.m_vSimulationAxis);

					nodeAttach->getAttr("P_CapsuleX", attach.m_simulationParams.m_vCapsule.x);
					nodeAttach->getAttr("P_CapsuleY", attach.m_simulationParams.m_vCapsule.y);

					nodeAttach->getAttr("P_PivotOffset", attach.m_simulationParams.m_vPivotOffset);

					char proxytag[] = "P_Proxy00";
					for (uint32 i = 0; i < SimulationParams::MaxCollisionProxies; i++)
					{
						CCryName strProxyName = CCryName(nodeAttach->getAttr(proxytag));
						proxytag[8]++;
						if (strProxyName.empty())
							continue;
						attach.m_simulationParams.m_arrProxyNames.push_back(strProxyName);
					}
				}

				const char* strProcFunction = nodeAttach->getAttr("ProcFunction");
				if (strProcFunction && strProcFunction[0])
					attach.m_simulationParams.m_strProcFunction = strProcFunction;

				if (Type == "CA_PROW")
				{
					attach.m_strRowJointName = nodeAttach->getAttr("RowJointName");

					uint32 nPendulumClampMode = 0;
					uint32 isPendulum = nodeAttach->getAttr("ROW_ClampMode", nPendulumClampMode);
					if (isPendulum)
					{
						attach.m_rowSimulationParams.m_nClampMode = RowSimulationParams::ClampMode(nPendulumClampMode);

						nodeAttach->getAttr("ROW_FPS", attach.m_rowSimulationParams.m_nSimFPS);
						nodeAttach->getAttr("ROW_ConeAngle", attach.m_rowSimulationParams.m_fConeAngle);
						nodeAttach->getAttr("ROW_ConeRotation", attach.m_rowSimulationParams.m_vConeRotation);

						nodeAttach->getAttr("ROW_Mass", attach.m_rowSimulationParams.m_fMass);
						nodeAttach->getAttr("ROW_Gravity", attach.m_rowSimulationParams.m_fGravity);
						nodeAttach->getAttr("ROW_Damping", attach.m_rowSimulationParams.m_fDamping);
						nodeAttach->getAttr("ROW_JointSpring", attach.m_rowSimulationParams.m_fJointSpring);
						nodeAttach->getAttr("ROW_RodLength", attach.m_rowSimulationParams.m_fRodLength);
						nodeAttach->getAttr("ROW_StiffnessTarget", attach.m_rowSimulationParams.m_vStiffnessTarget);
						nodeAttach->getAttr("ROW_Turbulence", attach.m_rowSimulationParams.m_vTurbulence);
						nodeAttach->getAttr("ROW_MaxVelocity", attach.m_rowSimulationParams.m_fMaxVelocity);

						nodeAttach->getAttr("ROW_Cycle", attach.m_rowSimulationParams.m_cycle);
						nodeAttach->getAttr("ROW_RelaxLoops", attach.m_rowSimulationParams.m_relaxationLoops);
						nodeAttach->getAttr("ROW_Stretch", attach.m_rowSimulationParams.m_fStretch);

						nodeAttach->getAttr("ROW_ProjectionType", attach.m_rowSimulationParams.m_nProjectionType);
						nodeAttach->getAttr("ROW_CapsuleX", attach.m_rowSimulationParams.m_vCapsule.x);
						nodeAttach->getAttr("ROW_CapsuleY", attach.m_rowSimulationParams.m_vCapsule.y);

						char proxytag[] = "ROW_Proxy00";
						for (uint32 i = 0; i < SimulationParams::MaxCollisionProxies; i++)
						{
							CCryName strProxyName = CCryName(nodeAttach->getAttr(proxytag));
							proxytag[10]++;
							if (strProxyName.empty())
								continue;
							attach.m_rowSimulationParams.m_arrProxyNames.push_back(strProxyName);
						}
					}
				}

				if (Type == "CA_VCLOTH")
				{
					// Animation Control
					nodeAttach->getAttr("hide", attach.m_vclothParams.hide);
					nodeAttach->getAttr("forceSkinning", attach.m_vclothParams.forceSkinning);
					nodeAttach->getAttr("forceSkinningFpsThreshold", attach.m_vclothParams.forceSkinningFpsThreshold);
					nodeAttach->getAttr("forceSkinningTranslateThreshold", attach.m_vclothParams.forceSkinningTranslateThreshold);
					nodeAttach->getAttr("checkAnimationRewind", attach.m_vclothParams.checkAnimationRewind);
					nodeAttach->getAttr("disableSimulationAtDistance", attach.m_vclothParams.disableSimulationAtDistance);
					nodeAttach->getAttr("disableSimulationTimeRange", attach.m_vclothParams.disableSimulationTimeRange);

					// Simulation and Collision
					nodeAttach->getAttr("timeStep", attach.m_vclothParams.timeStep);
					nodeAttach->getAttr("timeStepMax", attach.m_vclothParams.timeStepsMax);
					nodeAttach->getAttr("numIterations", attach.m_vclothParams.numIterations);
					nodeAttach->getAttr("collideEveryNthStep", attach.m_vclothParams.collideEveryNthStep);
					nodeAttach->getAttr("collisionMultipleShiftFactor", attach.m_vclothParams.collisionMultipleShiftFactor);
					nodeAttach->getAttr("gravityFactor", attach.m_vclothParams.gravityFactor);

					// Stiffness and Elasticity
					nodeAttach->getAttr("stretchStiffness", attach.m_vclothParams.stretchStiffness);
					nodeAttach->getAttr("shearStiffness", attach.m_vclothParams.shearStiffness);
					nodeAttach->getAttr("bendStiffness", attach.m_vclothParams.bendStiffness);
					nodeAttach->getAttr("bendStiffnessByTrianglesAngle", attach.m_vclothParams.bendStiffnessByTrianglesAngle);
					nodeAttach->getAttr("pullStiffness", attach.m_vclothParams.pullStiffness);

					// Friction and Damping
					nodeAttach->getAttr("friction", attach.m_vclothParams.friction);
					nodeAttach->getAttr("rigidDamping", attach.m_vclothParams.rigidDamping);
					nodeAttach->getAttr("springDamping", attach.m_vclothParams.springDamping);
					nodeAttach->getAttr("springDampingPerSubstep", attach.m_vclothParams.springDampingPerSubstep);
					nodeAttach->getAttr("collisionDampingTangential", attach.m_vclothParams.collisionDampingTangential);

					// Long Range Attachments
					nodeAttach->getAttr("longRangeAttachments", attach.m_vclothParams.longRangeAttachments);
					nodeAttach->getAttr("longRangeAttachmentsAllowedExtension", attach.m_vclothParams.longRangeAttachmentsAllowedExtension);
					nodeAttach->getAttr("longRangeAttachmentsMaximumShiftFactor", attach.m_vclothParams.longRangeAttachmentsMaximumShiftFactor);
					nodeAttach->getAttr("longRangeAttachmentsShiftCollisionFactor", attach.m_vclothParams.longRangeAttachmentsShiftCollisionFactor);

					// Test Reset Damping
					nodeAttach->getAttr("resetDampingFactor", attach.m_vclothParams.resetDampingFactor);
					nodeAttach->getAttr("resetDampingRange", attach.m_vclothParams.resetDampingRange);

					// Additional
					nodeAttach->getAttr("translationBlend", attach.m_vclothParams.translationBlend);
					nodeAttach->getAttr("rotationBlend", attach.m_vclothParams.rotationBlend);
					nodeAttach->getAttr("externalBlend", attach.m_vclothParams.externalBlend);
					nodeAttach->getAttr("filterLaplace", attach.m_vclothParams.filterLaplace);
					nodeAttach->getAttr("maxAnimDistance", attach.m_vclothParams.maxAnimDistance);

					// Debug
					nodeAttach->getAttr("isMainCharacter", attach.m_vclothParams.isMainCharacter);
					attach.m_vclothParams.simMeshName = nodeAttach->getAttr("simMeshName");
					attach.m_vclothParams.renderMeshName = nodeAttach->getAttr("renderMeshName");
					attach.m_vclothParams.simBinding = nodeAttach->getAttr("simBinding");
					attach.m_vclothParams.renderBinding = nodeAttach->getAttr("Binding");
					attach.m_vclothParams.material = nodeAttach->getAttr("Material");
					nodeAttach->getAttr("debugDrawVerticesRadius", attach.m_vclothParams.debugDrawVerticesRadius);
					nodeAttach->getAttr("debugDrawCloth", attach.m_vclothParams.debugDrawCloth);
					nodeAttach->getAttr("debugDrawLRA", attach.m_vclothParams.debugDrawLRA);
					nodeAttach->getAttr("debugPrint", attach.m_vclothParams.debugPrint);
				}

				uint32 flags;
				if (nodeAttach->getAttr("Flags", flags))
					attach.m_nFlags = flags;

				nodeAttach->getAttr("ViewDistRatio", attach.m_viewDistanceMultiplier);

				attach.m_jointPhysics.LoadFromXml(nodeAttach);

				attach.m_definition = this;
				attachments.push_back(attach);
			}

		}
	}

	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	uint32 numDebugSetup = arrDebugSetup.size();
	for (uint32 d = 0; d < numDebugSetup; d++)
	{
		uint32 _crc32 = arrDebugSetup[d].m_crc32;
		AttachmentTypes _type = arrDebugSetup[d].m_type;
		uint32 numAttachments = attachments.size();
		for (uint32 i = 0; i < numAttachments; i++)
		{
			uint32 crc32 = CCrc32::ComputeLowercase(attachments[i].m_strSocketName.c_str());
			AttachmentTypes type = attachments[i].m_attachmentType;
			if (crc32 == _crc32 && type == _type)
			{
				attachments[i].m_simulationParams.m_useDebugSetup = arrDebugSetup[d].m_dsetup;
				attachments[i].m_simulationParams.m_useDebugText = arrDebugSetup[d].m_dtext;
				attachments[i].m_simulationParams.m_useSimulation = arrDebugSetup[d].m_active;
			}
		}
	}

	if (CryCreateClassInstanceForInterface(cryiidof<IAnimationPoseModifierSetup>(), modifiers))
	{
		if (!Serialization::LoadXmlNode(*modifiers, root))
			return false;
	}

	return true;
}

XmlNodeRef CharacterDefinition::SaveToXml()
{
	XmlNodeRef root = GetIEditor()->GetSystem()->CreateXmlNode("CharacterDefinition");

	{
		XmlNodeRef node = root->newChild("Model");
		//-----------------------------------------------------------
		// load base model
		//-----------------------------------------------------------
		node->setAttr("File", skeleton);

		if (!materialPath.empty())
			node->setAttr("Material", materialPath);
		else
			node->delAttr("Material");

		if (!physics.empty())
			node->setAttr("Physics", physics.c_str());
		else
			node->delAttr("Physics");

		if (!rig.empty())
			node->setAttr("Rig", rig.c_str());
		else
			node->delAttr("Rig");

		// We don't currently modify these in the editor...

		// node->getAttr( "KeepModelsInMemory",def.m_nKeepModelsInMemory );
	}

	//-----------------------------------------------------------
	// load attachment-list
	//-----------------------------------------------------------
	XmlNodeRef nodeAttachments = root->newChild("AttachmentList");
	const uint32 numAttachments = nodeAttachments->getChildCount();

	// clear out the existing attachments
	while (XmlNodeRef childNode = nodeAttachments->findChild("Attachment"))
	{
		nodeAttachments->removeChild(childNode);
	}

	//don't save names for not existing proxies
	uint32 numAttachmentsInList = attachments.size();
	for (uint32 x = 0; x < numAttachmentsInList; x++)
	{

		DynArray<uint8> arrProxies;
		arrProxies.resize(numAttachmentsInList);
		memset(&arrProxies[0], 0, numAttachmentsInList);
		for (uint32 pn = 0; pn < attachments[x].m_simulationParams.m_arrProxyNames.size(); pn++)
		{
			const char* strProxyName = attachments[x].m_simulationParams.m_arrProxyNames[pn].c_str();
			for (uint32 a = 0; a < numAttachmentsInList; a++)
			{
				const char* strSocketName = attachments[a].m_strSocketName.c_str();
				if (attachments[a].m_attachmentType == CA_PROX && stricmp(strSocketName, strProxyName) == 0)
				{
					arrProxies[a] = 1;
					break;
				}
			}
		}
		attachments[x].m_simulationParams.m_arrProxyNames.resize(0);
		for (uint32 n = 0; n < numAttachmentsInList; n++)
		{
			if (arrProxies[n])
				attachments[x].m_simulationParams.m_arrProxyNames.push_back(CCryName(attachments[n].m_strSocketName.c_str()));
		}

		memset(&arrProxies[0], 0, numAttachmentsInList);
		for (uint32 pn = 0; pn < attachments[x].m_rowSimulationParams.m_arrProxyNames.size(); pn++)
		{
			const char* strProxyName = attachments[x].m_rowSimulationParams.m_arrProxyNames[pn].c_str();
			for (uint32 a = 0; a < numAttachmentsInList; a++)
			{
				const char* strSocketName = attachments[a].m_strSocketName.c_str();
				if (attachments[a].m_attachmentType == CA_PROX && stricmp(strSocketName, strProxyName) == 0)
				{
					arrProxies[a] = 1;
					break;
				}
			}
		}
		attachments[x].m_rowSimulationParams.m_arrProxyNames.resize(0);
		for (uint32 n = 0; n < numAttachmentsInList; n++)
		{
			if (arrProxies[n])
				attachments[x].m_rowSimulationParams.m_arrProxyNames.push_back(CCryName(attachments[n].m_strSocketName.c_str()));
		}
	}

	for (vector<CharacterAttachment>::const_iterator iter = attachments.begin(), itEnd = attachments.end(); iter != itEnd; ++iter)
	{
		const CharacterAttachment& attach = (*iter);
		if (attach.m_attachmentType == CA_BONE)
			ExportBoneAttachment(attach, nodeAttachments);
		if (attach.m_attachmentType == CA_FACE)
			ExportFaceAttachment(attach, nodeAttachments);
		if (attach.m_attachmentType == CA_SKIN)
			ExportSkinAttachment(attach, nodeAttachments);
		if (attach.m_attachmentType == CA_PROX)
			ExportProxyAttachment(attach, nodeAttachments);
		if (attach.m_attachmentType == CA_PROW)
			ExportPClothAttachment(attach, nodeAttachments);
		if (attach.m_attachmentType == CA_VCLOTH)
			ExportVClothAttachment(attach, nodeAttachments);
	}

	if (XmlNodeRef node = root->findChild("Modifiers"))
	{
		root->removeChild(node);
	}

	if (modifiers)
	{
		Serialization::SaveXmlNode(root, *modifiers);
	}

	// make sure we don't leave empty modifiers block
	XmlNodeRef modifiersNode = root->findChild("Modifiers");
	if (modifiersNode && modifiersNode->getChildCount() == 0 && modifiersNode->getNumAttributes() == 0)
		root->removeChild(modifiersNode);

	return root;
}

void CharacterDefinition::ExportBoneAttachment(const CharacterAttachment& attach, XmlNodeRef nodeAttachments)
{
	XmlNodeRef nodeAttach = gEnv->pSystem->CreateXmlNode("Attachment");
	nodeAttachments->addChild(nodeAttach);

	nodeAttach->setAttr("Type", "CA_BONE");

	if (!attach.m_strSocketName.empty())
		nodeAttach->setAttr("AName", attach.m_strSocketName);

	if (!attach.m_strGeometryFilepath.empty())
		nodeAttach->setAttr("Binding", attach.m_strGeometryFilepath);
	else
		nodeAttach->delAttr("Binding");

	if (!attach.m_strMaterial.empty())
		nodeAttach->setAttr("Material", attach.m_strMaterial);

	const QuatT& reltransform = attach.m_jointSpacePosition;
	const QuatT& abstransform = attach.m_characterSpacePosition;

	if (attach.m_rotationSpace == CharacterAttachment::SPACE_JOINT)
	{
		Quat identity(IDENTITY);
		if (fabs_tpl(reltransform.q.w) > 0.99999f)
			nodeAttach->setAttr("RelRotation", identity);
		else
			nodeAttach->setAttr("RelRotation", reltransform.q);
	}
	else
		nodeAttach->setAttr("Rotation", abstransform.q);

	if (attach.m_positionSpace == CharacterAttachment::SPACE_JOINT)
		nodeAttach->setAttr("RelPosition", reltransform.t);
	else
		nodeAttach->setAttr("Position", abstransform.t);

	if (!attach.m_strJointName.empty())
		nodeAttach->setAttr("BoneName", attach.m_strJointName);
	else
		nodeAttach->delAttr("BoneName");

	ExportSimulation(attach, nodeAttach);

	if (attach.m_simulationParams.m_strProcFunction.length())
		nodeAttach->setAttr("ProcFunction", attach.m_simulationParams.m_strProcFunction.c_str());

	nodeAttach->setAttr("Flags", attach.m_nFlags);

	if (attach.m_viewDistanceMultiplier != 1.0f)
		nodeAttach->setAttr("ViewDistRatio", attach.m_viewDistanceMultiplier);
	else
		nodeAttach->delAttr("ViewDistRatio");

	attach.m_jointPhysics.SaveToXml(nodeAttach);
}

void CharacterDefinition::ExportFaceAttachment(const CharacterAttachment& attach, XmlNodeRef nodeAttachments)
{
	XmlNodeRef nodeAttach = gEnv->pSystem->CreateXmlNode("Attachment");
	nodeAttachments->addChild(nodeAttach);

	nodeAttach->setAttr("Type", "CA_FACE");

	if (!attach.m_strSocketName.empty())
		nodeAttach->setAttr("AName", attach.m_strSocketName);

	if (!attach.m_strGeometryFilepath.empty())
		nodeAttach->setAttr("Binding", attach.m_strGeometryFilepath);
	else
		nodeAttach->delAttr("Binding");

	if (!attach.m_strMaterial.empty())
		nodeAttach->setAttr("Material", attach.m_strMaterial);

	const QuatT& transform = attach.m_characterSpacePosition;
	nodeAttach->setAttr("Rotation", transform.q);
	nodeAttach->setAttr("Position", transform.t);

	ExportSimulation(attach, nodeAttach);

	nodeAttach->setAttr("Flags", attach.m_nFlags);

	if (attach.m_viewDistanceMultiplier != 1.0f)
		nodeAttach->setAttr("ViewDistRatio", attach.m_viewDistanceMultiplier);
	else
		nodeAttach->delAttr("ViewDistRatio");
}

void CharacterDefinition::ExportSkinAttachment(const CharacterAttachment& attach, XmlNodeRef nodeAttachments)
{
	XmlNodeRef nodeAttach = gEnv->pSystem->CreateXmlNode("Attachment");
	nodeAttachments->addChild(nodeAttach);

	nodeAttach->setAttr("Type", "CA_SKIN");

	if (!attach.m_strSocketName.empty())
		nodeAttach->setAttr("AName", attach.m_strSocketName);

	if (!attach.m_strGeometryFilepath.empty())
		nodeAttach->setAttr("Binding", attach.m_strGeometryFilepath);
	else
		nodeAttach->delAttr("Binding");

	if (!attach.m_strMaterial.empty())
		nodeAttach->setAttr("Material", attach.m_strMaterial);

	nodeAttach->setAttr("Flags", attach.m_nFlags);

	if (attach.m_viewDistanceMultiplier != 1.0f)
		nodeAttach->setAttr("ViewDistRatio", attach.m_viewDistanceMultiplier);
	else
		nodeAttach->delAttr("ViewDistRatio");
}

void CharacterDefinition::ExportProxyAttachment(const CharacterAttachment& attach, XmlNodeRef nodeAttachments)
{
	if (attach.m_ProxyPurpose == CharacterAttachment::RAGDOLL)
		return;

	XmlNodeRef nodeAttach = gEnv->pSystem->CreateXmlNode("Attachment");
	nodeAttachments->addChild(nodeAttach);

	nodeAttach->setAttr("Type", "CA_PROX");

	if (!attach.m_strSocketName.empty())
		nodeAttach->setAttr("AName", attach.m_strSocketName);

	const QuatT& reltransform = attach.m_jointSpacePosition;
	const QuatT& abstransform = attach.m_characterSpacePosition;
	if (attach.m_rotationSpace == CharacterAttachment::SPACE_JOINT)
		nodeAttach->setAttr("RelRotation", reltransform.q);
	else
		nodeAttach->setAttr("Rotation", abstransform.q);
	if (attach.m_positionSpace == CharacterAttachment::SPACE_JOINT)
		nodeAttach->setAttr("RelPosition", reltransform.t);
	else
		nodeAttach->setAttr("Position", abstransform.t);

	if (!attach.m_strJointName.empty())
		nodeAttach->setAttr("BoneName", attach.m_strJointName);
	else
		nodeAttach->delAttr("BoneName");

	nodeAttach->setAttr("ProxyParams", attach.m_ProxyParams);
	nodeAttach->setAttr("ProxyPurpose", attach.m_ProxyPurpose);
}

void CharacterDefinition::ExportPClothAttachment(const CharacterAttachment& attach, XmlNodeRef nodeAttachments)
{
	XmlNodeRef nodeAttach = gEnv->pSystem->CreateXmlNode("Attachment");
	nodeAttachments->addChild(nodeAttach);
	nodeAttach->setAttr("Type", "CA_PROW");
	if (!attach.m_strSocketName.empty())
		nodeAttach->setAttr("AName", attach.m_strSocketName);

	nodeAttach->setAttr("rowJointName", attach.m_strRowJointName);
	nodeAttach->setAttr("ROW_ClampMode", (int&)attach.m_rowSimulationParams.m_nClampMode);

	RowSimulationParams::ClampMode ct = attach.m_rowSimulationParams.m_nClampMode;
	if (ct == RowSimulationParams::TRANSLATIONAL_PROJECTION)
	{
		//only store values in XML if they are not identical with the default-values
		if (attach.m_rowSimulationParams.m_nProjectionType)       nodeAttach->setAttr("ROW_ProjectionType", attach.m_rowSimulationParams.m_nProjectionType);
		if (attach.m_rowSimulationParams.m_nProjectionType == ProjectionSelection4::PS4_DirectedTranslation)
		{
			uint32 IsIdentical = stricmp(attach.m_rowSimulationParams.m_strDirTransJoint.c_str(), attach.m_strJointName.c_str()) == 0;
			if (attach.m_rowSimulationParams.m_strDirTransJoint.length() && IsIdentical == 0)
				nodeAttach->setAttr("ROW_DirTransJointName", attach.m_rowSimulationParams.m_strDirTransJoint.c_str());
			else
				nodeAttach->setAttr("ROW_TranslationAxis", attach.m_rowSimulationParams.m_vTranslationAxis);

			if (attach.m_rowSimulationParams.m_vCapsule.x) nodeAttach->setAttr("ROW_CapsuleX", attach.m_rowSimulationParams.m_vCapsule.x);
			if (attach.m_rowSimulationParams.m_vCapsule.y) nodeAttach->setAttr("ROW_CapsuleY", attach.m_rowSimulationParams.m_vCapsule.y);
		}
		if (attach.m_rowSimulationParams.m_nProjectionType == ProjectionSelection4::PS4_ShortvecTranslation)
		{
			if (attach.m_rowSimulationParams.m_vCapsule.y) nodeAttach->setAttr("ROW_CapsuleY", attach.m_rowSimulationParams.m_vCapsule.y);
		}

		uint32 numProxiesName = std::min((uint32)SimulationParams::MaxCollisionProxies, (uint32)attach.m_rowSimulationParams.m_arrProxyNames.size());
		char proxytag[] = "ROW_Proxy00";
		for (uint32 i = 0; i < numProxiesName; i++)
		{
			const char* pname = attach.m_rowSimulationParams.m_arrProxyNames[i].c_str();
			nodeAttach->setAttr(proxytag, pname);
			proxytag[10]++;
		}
	}
	else
	{
		//only store values in XML if they are not identical with the default-values
		if (attach.m_rowSimulationParams.m_nSimFPS != 10)         nodeAttach->setAttr("ROW_FPS", attach.m_rowSimulationParams.m_nSimFPS);
		if (attach.m_rowSimulationParams.m_fConeAngle != 45.0f)     nodeAttach->setAttr("ROW_ConeAngle", attach.m_rowSimulationParams.m_fConeAngle);
		if (sqr(attach.m_rowSimulationParams.m_vConeRotation))    nodeAttach->setAttr("ROW_ConeRotation", attach.m_rowSimulationParams.m_vConeRotation);

		if (attach.m_rowSimulationParams.m_fMass != 1.00f)      nodeAttach->setAttr("ROW_Mass", attach.m_rowSimulationParams.m_fMass);
		if (attach.m_rowSimulationParams.m_fGravity != 9.81f)      nodeAttach->setAttr("ROW_Gravity", attach.m_rowSimulationParams.m_fGravity);
		if (attach.m_rowSimulationParams.m_fDamping != 1.00f)      nodeAttach->setAttr("ROW_Damping", attach.m_rowSimulationParams.m_fDamping);
		if (attach.m_rowSimulationParams.m_fJointSpring)          nodeAttach->setAttr("ROW_JointSpring", attach.m_rowSimulationParams.m_fJointSpring);
		if (attach.m_rowSimulationParams.m_fRodLength > 0.1)        nodeAttach->setAttr("ROW_RodLength", attach.m_rowSimulationParams.m_fRodLength);
		if (sqr(attach.m_rowSimulationParams.m_vStiffnessTarget)) nodeAttach->setAttr("ROW_StiffnessTarget", attach.m_rowSimulationParams.m_vStiffnessTarget);
		if (sqr(attach.m_rowSimulationParams.m_vTurbulence))      nodeAttach->setAttr("ROW_Turbulence", attach.m_rowSimulationParams.m_vTurbulence);
		if (attach.m_rowSimulationParams.m_fMaxVelocity)          nodeAttach->setAttr("ROW_MaxVelocity", attach.m_rowSimulationParams.m_fMaxVelocity);

		if (attach.m_rowSimulationParams.m_cycle)                 nodeAttach->setAttr("ROW_Cycle", attach.m_rowSimulationParams.m_cycle);
		if (attach.m_rowSimulationParams.m_relaxationLoops)       nodeAttach->setAttr("ROW_RelaxLoops", attach.m_rowSimulationParams.m_relaxationLoops);
		if (attach.m_rowSimulationParams.m_fStretch != 0.10f)      nodeAttach->setAttr("ROW_Stretch", attach.m_rowSimulationParams.m_fStretch);

		if (attach.m_rowSimulationParams.m_vCapsule.x)            nodeAttach->setAttr("ROW_CapsuleX", attach.m_rowSimulationParams.m_vCapsule.x);
		if (attach.m_rowSimulationParams.m_vCapsule.y)            nodeAttach->setAttr("ROW_CapsuleY", attach.m_rowSimulationParams.m_vCapsule.y);
		if (attach.m_rowSimulationParams.m_nProjectionType)       nodeAttach->setAttr("ROW_ProjectionType", attach.m_rowSimulationParams.m_nProjectionType);

		uint32 numProxiesName = std::min((uint32)SimulationParams::MaxCollisionProxies, (uint32)attach.m_rowSimulationParams.m_arrProxyNames.size());
		char proxytag[] = "ROW_Proxy00";
		for (uint32 i = 0; i < numProxiesName; i++)
		{
			const char* pname = attach.m_rowSimulationParams.m_arrProxyNames[i].c_str();
			nodeAttach->setAttr(proxytag, pname);
			proxytag[10]++;
		}
	}

}

void CharacterDefinition::ExportVClothAttachment(const CharacterAttachment& attach, XmlNodeRef nodeAttachments)
{
	XmlNodeRef nodeAttach = gEnv->pSystem->CreateXmlNode("Attachment");
	nodeAttachments->addChild(nodeAttach);

	nodeAttach->setAttr("Type", "CA_VCLOTH");

	if (!attach.m_strSocketName.empty())
		nodeAttach->setAttr("AName", attach.m_strSocketName);

	// Animation Control
	nodeAttach->setAttr("hide", attach.m_vclothParams.hide);
	nodeAttach->setAttr("forceSkinning", attach.m_vclothParams.forceSkinning);
	nodeAttach->setAttr("forceSkinningFpsThreshold", attach.m_vclothParams.forceSkinningFpsThreshold);
	nodeAttach->setAttr("forceSkinningTranslateThreshold", attach.m_vclothParams.forceSkinningTranslateThreshold);
	nodeAttach->setAttr("checkAnimationRewind", attach.m_vclothParams.checkAnimationRewind);
	nodeAttach->setAttr("disableSimulationAtDistance", attach.m_vclothParams.disableSimulationAtDistance);
	nodeAttach->setAttr("disableSimulationTimeRange", attach.m_vclothParams.disableSimulationTimeRange);

	// Simulation and Collision
	nodeAttach->setAttr("timeStep", attach.m_vclothParams.timeStep);
	nodeAttach->setAttr("timeStepMax", attach.m_vclothParams.timeStepsMax);
	nodeAttach->setAttr("numIterations", attach.m_vclothParams.numIterations);
	nodeAttach->setAttr("collideEveryNthStep", attach.m_vclothParams.collideEveryNthStep);
	nodeAttach->setAttr("collisionMultipleShiftFactor", attach.m_vclothParams.collisionMultipleShiftFactor);
	nodeAttach->setAttr("gravityFactor", attach.m_vclothParams.gravityFactor);

	// Stiffness and Elasticity
	nodeAttach->setAttr("stretchStiffness", attach.m_vclothParams.stretchStiffness);
	nodeAttach->setAttr("shearStiffness", attach.m_vclothParams.shearStiffness);
	nodeAttach->setAttr("bendStiffness", attach.m_vclothParams.bendStiffness);
	nodeAttach->setAttr("bendStiffnessByTrianglesAngle", attach.m_vclothParams.bendStiffnessByTrianglesAngle);
	nodeAttach->setAttr("pullStiffness", attach.m_vclothParams.pullStiffness);

	// Friction and Damping
	nodeAttach->setAttr("friction", attach.m_vclothParams.friction);
	nodeAttach->setAttr("rigidDamping", attach.m_vclothParams.rigidDamping);
	nodeAttach->setAttr("springDamping", attach.m_vclothParams.springDamping);
	nodeAttach->setAttr("springDampingPerSubstep", attach.m_vclothParams.springDampingPerSubstep);
	nodeAttach->setAttr("collisionDampingTangential", attach.m_vclothParams.collisionDampingTangential);

	// Long Range Attachments
	nodeAttach->setAttr("longRangeAttachments", attach.m_vclothParams.longRangeAttachments);
	nodeAttach->setAttr("longRangeAttachmentsAllowedExtension", attach.m_vclothParams.longRangeAttachmentsAllowedExtension);
	nodeAttach->setAttr("longRangeAttachmentsMaximumShiftFactor", attach.m_vclothParams.longRangeAttachmentsMaximumShiftFactor);
	nodeAttach->setAttr("longRangeAttachmentsShiftCollisionFactor", attach.m_vclothParams.longRangeAttachmentsShiftCollisionFactor);

	// Test Reset Damping
	nodeAttach->setAttr("resetDampingFactor", attach.m_vclothParams.resetDampingFactor);
	nodeAttach->setAttr("resetDampingRange", attach.m_vclothParams.resetDampingRange);

	// Additional
	nodeAttach->setAttr("translationBlend", attach.m_vclothParams.translationBlend);
	nodeAttach->setAttr("rotationBlend", attach.m_vclothParams.rotationBlend);
	nodeAttach->setAttr("externalBlend", attach.m_vclothParams.externalBlend);
	nodeAttach->setAttr("maxAnimDistance", attach.m_vclothParams.maxAnimDistance);
	nodeAttach->setAttr("filterLaplace", attach.m_vclothParams.filterLaplace);

	// Debug
	nodeAttach->setAttr("isMainCharacter", attach.m_vclothParams.isMainCharacter);
	nodeAttach->setAttr("renderMeshName", attach.m_vclothParams.renderMeshName);
	nodeAttach->setAttr("Binding", attach.m_vclothParams.renderBinding);
	nodeAttach->setAttr("simMeshName", attach.m_vclothParams.simMeshName);
	nodeAttach->setAttr("simBinding", attach.m_vclothParams.simBinding);
	nodeAttach->setAttr("Material", attach.m_vclothParams.material); // also store material in vCloth-node - otherwise material would be lost, while saving
	nodeAttach->setAttr("debugDrawVerticesRadius", attach.m_vclothParams.debugDrawVerticesRadius);
	nodeAttach->setAttr("debugDrawCloth", attach.m_vclothParams.debugDrawCloth);
	nodeAttach->setAttr("debugDrawLRA", attach.m_vclothParams.debugDrawLRA);
	nodeAttach->setAttr("debugPrint", attach.m_vclothParams.debugPrint);
}

void CharacterDefinition::ExportSimulation(const CharacterAttachment& attach, XmlNodeRef nodeAttach)
{
	if (SimulationParams::DISABLED != attach.m_simulationParams.m_nClampType)
	{
		uint32 ct = attach.m_simulationParams.m_nClampType;
		if (ct == SimulationParams::PENDULUM_CONE || ct == SimulationParams::PENDULUM_HINGE_PLANE || ct == SimulationParams::PENDULUM_HALF_CONE)
		{
			nodeAttach->setAttr("PA_PendulumType", (int&)attach.m_simulationParams.m_nClampType);

			//only store values in XML if they are not identical with the default-values
			if (attach.m_simulationParams.m_nSimFPS != 10)         nodeAttach->setAttr("PA_FPS", attach.m_simulationParams.m_nSimFPS);
			if (attach.m_simulationParams.m_fMaxAngle != 45.0f)      nodeAttach->setAttr("PA_MaxAngle", attach.m_simulationParams.m_fMaxAngle);
			if (attach.m_simulationParams.m_vDiskRotation.x)       nodeAttach->setAttr("PA_HRotation", attach.m_simulationParams.m_vDiskRotation.x);
			if (attach.m_simulationParams.m_useRedirect)           nodeAttach->setAttr("PA_Redirect", attach.m_simulationParams.m_useRedirect);

			if (attach.m_simulationParams.m_fMass != 1.00f)      nodeAttach->setAttr("PA_Mass", attach.m_simulationParams.m_fMass);
			if (attach.m_simulationParams.m_fGravity != 9.81f)      nodeAttach->setAttr("PA_Gravity", attach.m_simulationParams.m_fGravity);
			if (attach.m_simulationParams.m_fDamping != 1.00f)      nodeAttach->setAttr("PA_Damping", attach.m_simulationParams.m_fDamping);
			if (attach.m_simulationParams.m_fStiffness)            nodeAttach->setAttr("PA_Stiffness", attach.m_simulationParams.m_fStiffness);

			if (sqr(attach.m_simulationParams.m_vPivotOffset))     nodeAttach->setAttr("PA_PivotOffset", attach.m_simulationParams.m_vPivotOffset);
			if (sqr(attach.m_simulationParams.m_vSimulationAxis))  nodeAttach->setAttr("PA_SimulationAxis", attach.m_simulationParams.m_vSimulationAxis);
			if (sqr(attach.m_simulationParams.m_vStiffnessTarget)) nodeAttach->setAttr("PA_StiffnessTarget", attach.m_simulationParams.m_vStiffnessTarget);

			if (attach.m_simulationParams.m_vCapsule.x)            nodeAttach->setAttr("PA_CapsuleX", attach.m_simulationParams.m_vCapsule.x);
			if (attach.m_simulationParams.m_vCapsule.y)            nodeAttach->setAttr("PA_CapsuleY", attach.m_simulationParams.m_vCapsule.y);
			if (attach.m_simulationParams.m_nProjectionType)       nodeAttach->setAttr("PA_ProjectionType", attach.m_simulationParams.m_nProjectionType);

			if (attach.m_simulationParams.m_nProjectionType == 3)
			{
				uint32 IsIdentical = stricmp(attach.m_simulationParams.m_strDirTransJoint.c_str(), attach.m_strJointName.c_str()) == 0;
				if (attach.m_simulationParams.m_strDirTransJoint.length() && IsIdentical == 0)
					nodeAttach->setAttr("PA_DirTransJointName", attach.m_simulationParams.m_strDirTransJoint.c_str());
			}

			uint32 numProxiesName = std::min((uint32)SimulationParams::MaxCollisionProxies, (uint32)attach.m_simulationParams.m_arrProxyNames.size());
			char proxytag[] = "PA_Proxy00";
			for (uint32 i = 0; i < numProxiesName; i++)
			{
				const char* pname = attach.m_simulationParams.m_arrProxyNames[i].c_str();
				nodeAttach->setAttr(proxytag, pname);
				proxytag[9]++;
			}
		}

		if (ct == SimulationParams::SPRING_ELLIPSOID)
		{
			nodeAttach->setAttr("SA_SpringType", (int&)attach.m_simulationParams.m_nClampType);

			//only store values in XML if they are not identical with the default-values
			if (attach.m_simulationParams.m_nSimFPS != 10)         nodeAttach->setAttr("SA_FPS", attach.m_simulationParams.m_nSimFPS);

			if (attach.m_simulationParams.m_fRadius != 0.5f)       nodeAttach->setAttr("SA_Radius", attach.m_simulationParams.m_fRadius);
			if (attach.m_simulationParams.m_vSphereScale.x != 1)    nodeAttach->setAttr("SA_ScaleZP", attach.m_simulationParams.m_vSphereScale.x);
			if (attach.m_simulationParams.m_vSphereScale.y != 1)    nodeAttach->setAttr("SA_ScaleZN", attach.m_simulationParams.m_vSphereScale.y);
			if (attach.m_simulationParams.m_vDiskRotation.x)       nodeAttach->setAttr("SA_DiskRotX", attach.m_simulationParams.m_vDiskRotation.x);
			if (attach.m_simulationParams.m_vDiskRotation.y)       nodeAttach->setAttr("SA_DiskRotZ", attach.m_simulationParams.m_vDiskRotation.y);
			if (attach.m_simulationParams.m_useRedirect)           nodeAttach->setAttr("SA_Redirect", attach.m_simulationParams.m_useRedirect);

			if (attach.m_simulationParams.m_fMass != 1.00f)      nodeAttach->setAttr("SA_Mass", attach.m_simulationParams.m_fMass);
			if (attach.m_simulationParams.m_fGravity != 9.81f)      nodeAttach->setAttr("SA_Gravity", attach.m_simulationParams.m_fGravity);
			if (attach.m_simulationParams.m_fDamping != 1.00f)      nodeAttach->setAttr("SA_Damping", attach.m_simulationParams.m_fDamping);
			if (attach.m_simulationParams.m_fStiffness)            nodeAttach->setAttr("SA_Stiffness", attach.m_simulationParams.m_fStiffness);

			if (sqr(attach.m_simulationParams.m_vStiffnessTarget)) nodeAttach->setAttr("SA_StiffnessTarget", attach.m_simulationParams.m_vStiffnessTarget);

			//only store values in XML if they are not identical with the default-values
			if (attach.m_simulationParams.m_nProjectionType)       nodeAttach->setAttr("SA_ProjectionType", attach.m_simulationParams.m_nProjectionType);
			if (attach.m_simulationParams.m_nProjectionType)
				if (attach.m_simulationParams.m_vCapsule.y)          nodeAttach->setAttr("SA_CapsuleY", attach.m_simulationParams.m_vCapsule.y);
			if (sqr(attach.m_simulationParams.m_vPivotOffset))     nodeAttach->setAttr("SA_PivotOffset", attach.m_simulationParams.m_vPivotOffset);

			uint32 numProxiesName = std::min((uint32)SimulationParams::MaxCollisionProxies, (uint32)attach.m_simulationParams.m_arrProxyNames.size());
			char proxytag[] = "SA_Proxy00";
			for (uint32 i = 0; i < numProxiesName; i++)
			{
				const char* pname = attach.m_simulationParams.m_arrProxyNames[i].c_str();
				nodeAttach->setAttr(proxytag, pname);
				proxytag[9]++;
			}
		}

		if (ct == SimulationParams::TRANSLATIONAL_PROJECTION)
		{
			nodeAttach->setAttr("P_Projection", (int&)attach.m_simulationParams.m_nClampType);

			//only store values in XML if they are not identical with the default-values
			if (attach.m_simulationParams.m_nProjectionType)       nodeAttach->setAttr("P_ProjectionType", attach.m_simulationParams.m_nProjectionType);
			if (attach.m_simulationParams.m_nProjectionType == ProjectionSelection4::PS4_DirectedTranslation)
			{
				uint32 IsIdentical = stricmp(attach.m_simulationParams.m_strDirTransJoint.c_str(), attach.m_strJointName.c_str()) == 0;
				if (attach.m_simulationParams.m_strDirTransJoint.length() && IsIdentical == 0)
					nodeAttach->setAttr("P_DirTransJointName", attach.m_simulationParams.m_strDirTransJoint.c_str());
				else
					nodeAttach->setAttr("P_TranslationAxis", attach.m_simulationParams.m_vSimulationAxis);
			}
			if (attach.m_simulationParams.m_nProjectionType == ProjectionSelection4::PS4_DirectedTranslation)
			{
				if (attach.m_simulationParams.m_vCapsule.x)            nodeAttach->setAttr("P_CapsuleX", attach.m_simulationParams.m_vCapsule.x);
				if (attach.m_simulationParams.m_vCapsule.y)            nodeAttach->setAttr("P_CapsuleY", attach.m_simulationParams.m_vCapsule.y);
			}
			else
			{
				if (attach.m_simulationParams.m_vCapsule.y)            nodeAttach->setAttr("P_CapsuleY", attach.m_simulationParams.m_vCapsule.y);
			}

			if (sqr(attach.m_simulationParams.m_vPivotOffset))     nodeAttach->setAttr("P_PivotOffset", attach.m_simulationParams.m_vPivotOffset);

			uint32 numProxiesName = std::min((uint32)SimulationParams::MaxCollisionProxies, (uint32)attach.m_simulationParams.m_arrProxyNames.size());
			char proxytag[] = "P_Proxy00";
			for (uint32 i = 0; i < numProxiesName; i++)
			{
				const char* pname = attach.m_simulationParams.m_arrProxyNames[i].c_str();
				nodeAttach->setAttr(proxytag, pname);
				proxytag[8]++;
			}
		}

	}
}

bool CharacterDefinition::Save(const char* filename)
{
	XmlNodeRef root = SaveToXml();
	if (!root)
		return false;
	bool result = root->saveToFile(filename);
	SavePhysProxiesToCGF(PathUtil::GetGameFolder() + "/" + skeleton + ".cgf");
	if (result)
	{
		GetIEditor()->RequestScriptReload(eReloadScriptsType_Entity);
	}
	return result;
}

bool CharacterDefinition::SaveToMemory(vector<char>* buffer)
{
	XmlNodeRef root = SaveToXml();
	if (!root)
		return false;
	_smart_ptr<IXmlStringData> str(root->getXMLData());
	buffer->assign(str->GetString(), str->GetString() + str->GetStringLength());
	return true;
}

static string NormalizeMaterialName(const char* materialName)
{
	string result = materialName;
	result.MakeLower();
	PathUtil::RemoveExtension(result);
	return result;
}

void CharacterDefinition::ApplyPhysAttachments(ICharacterInstance* pICharacterInstance)
{
	IDefaultSkeleton& skel = pICharacterInstance->GetIDefaultSkeleton();
	IGeomManager *pGeoman = gEnv->pPhysicalWorld->GetGeomManager();
	for(int i = 0; i < skel.GetJointCount(); i++)
		if (skel.GetJointPhysInfo(i)->pPhysGeom)
		{
			pGeoman->UnregisterGeometry(skel.GetJointPhysInfo(i)->pPhysGeom);
			skel.GetJointPhysInfo(i)->pPhysGeom = nullptr;
		}

	int numAttachments = attachments.size();
	for (int i = 0; i < numAttachments; ++i)
	{
		const CharacterAttachment& attachment = attachments[i];
		if (attachment.m_attachmentType == CA_PROX && attachment.m_ProxyPurpose == CharacterAttachment::RAGDOLL)
		{
			if (IGeometry *pGeom = attachment.CreateProxyGeom())
			{
				CryBonePhysics& phys = *skel.GetJointPhysInfo(skel.GetJointIDByName(attachment.m_strJointName));
				phys.pPhysGeom = pGeoman->RegisterGeometry(pGeom);
				pGeom->Release();
				phys.flags = joint_no_gravity | joint_isolated_accelerations;
				*(Vec3*)phys.min = DEG2RAD(Vec3(attachment.m_limits[0]));
				*(Vec3*)phys.max = DEG2RAD(Vec3(attachment.m_limits[1]));
				for(int j = 0; j < 3; j++)
				{
					phys.flags |= (angle0_locked << j) * isneg(phys.max[j] - phys.min[j] - 0.01f);
					float unlim = 1 + isneg(gf_PI*1.999f - phys.max[j] + phys.min[j]);
					phys.max[j] *= unlim; phys.min[j] *= unlim;
				}
				*(Vec3*)phys.damping = attachment.m_damping;
				*(Vec3*)phys.spring_tension = attachment.m_tension;
			}
		}
	}

	int nRoots = 0, idummyRoot = -1;
	for(int i = 0, j; i < skel.GetJointCount(); i++)	if (skel.GetJointPhysGeom(i)) 
	{
		for(j = skel.GetJointParentIDByID(i); j >= 0 && !skel.GetJointPhysGeom(j); j = skel.GetJointParentIDByID(j))
			skel.GetJointPhysInfo(j)->flags = 0;
		nRoots -= j >> 31;
	}
	// for skeleton consistency, if multiple phys roots are present, create a temp phys root at their closest common ancestor
	if (nRoots > 1)
	{
		for(int i = 0, j; i < skel.GetJointCount(); i++)	if (skel.GetJointPhysGeom(i)) 
		{
			for(j = skel.GetJointParentIDByID(i); j >= 0 && !skel.GetJointPhysGeom(j); j = skel.GetJointParentIDByID(j))
				;
			if (j < 0) for(j = skel.GetJointParentIDByID(i); j >= 0; j = skel.GetJointParentIDByID(j))
				if (++skel.GetJointPhysInfo(j)->flags == nRoots)
				{
					idummyRoot = j; break;
				}
			if (idummyRoot >= 0)
				break;
		}
		CryBonePhysics& phys = *skel.GetJointPhysInfo(idummyRoot);
		memset(&phys, 0, sizeof(CryBonePhysics));
		((Matrix33*)phys.framemtx)->SetIdentity();
		IGeomManager *pGeoman = gEnv->pPhysicalWorld->GetGeomManager();
		primitives::sphere sph;
		sph.center.zero(); sph.r = 0.01f;
		phys.pPhysGeom = pGeoman->RegisterGeometry(pGeoman->CreatePrimitive(primitives::sphere::type, &sph));
		phys.pPhysGeom->pGeom->SetForeignData(nullptr, -1);
		phys.pPhysGeom->pGeom->Release();
	}

	for (int i = 0, j; i < numAttachments; ++i)
	{	// apply 0-angles rotations last since they need full parent-child hierarchy with physicalization
		const CharacterAttachment& att = attachments[i];
		if (att.m_attachmentType == CA_PROX && att.m_ProxyPurpose == CharacterAttachment::RAGDOLL)
		{	
			int idx = skel.GetJointIDByName(att.m_strJointName);
			for(j = skel.GetJointParentIDByID(idx); j >= 0 && !skel.GetJointPhysGeom(j); j = skel.GetJointParentIDByID(j))
				;
			*(Matrix33*)skel.GetJointPhysInfo(idx)->framemtx = Matrix33((j >= 0 ? !skel.GetDefaultAbsJointByID(j).q * skel.GetDefaultAbsJointByID(idx).q : Quat(IDENTITY)) * Quat(DEG2RAD(att.m_frame0)));
		}
	}
	m_physNeedsApply = false;
}

void CharacterDefinition::ApplyToCharacter(bool* skinSetChanged, ICharacterInstance* pICharacterInstance, ICharacterManager* characterManager, bool showDebug, bool applyPhys)
{
	if (m_initialized == 0)
		return; //no serialization, no update
	applyPhys = applyPhys && m_physEdit;

	IAttachmentManager* pIAttachmentManager = pICharacterInstance->GetIAttachmentManager();

	vector<string> names;
	uint32 attachmentCount = pIAttachmentManager->GetAttachmentCount();
	for (size_t i = 0; i < attachmentCount; ++i)
	{
		IAttachment* attachment = pIAttachmentManager->GetInterfaceByIndex(i);
		names.push_back(attachment->GetName());
	}

	vector<string> proxynames;
	uint32 proxyCount = pIAttachmentManager->GetProxyCount();
	for (size_t i = 0; i < proxyCount; ++i)
	{
		IProxy* proxy = pIAttachmentManager->GetProxyInterfaceByIndex(i);
		proxynames.push_back(proxy->GetName());
	}

	uint32 numAttachments = attachments.size();
	for (uint32 i = 0; i < numAttachments; ++i)
	{
		const CharacterAttachment& attachment = attachments[i];
		const char* strSocketName = attachment.m_strSocketName.c_str();
		int index = pIAttachmentManager->GetIndexByName(strSocketName);

		if (attachment.m_attachmentType == CA_PROX && attachment.m_ProxyPurpose == CharacterAttachment::RAGDOLL)
			continue;

		IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByIndex(index);
		if (index < 0)
		{
			if (attachment.m_attachmentType == CA_BONE || attachment.m_attachmentType == CA_FACE || attachment.m_attachmentType == CA_SKIN || attachment.m_attachmentType == CA_PROW)
			{
				if (attachment.m_strSocketName.length())
				{
					pIAttachment = pIAttachmentManager->CreateAttachment(attachment.m_strSocketName.c_str(), attachment.m_attachmentType, attachment.m_strJointName.c_str());
					if (pIAttachment)
						index = pIAttachmentManager->GetIndexByName(pIAttachment->GetName());
				}
			}
		}

		if (pIAttachment && index >= 0)
		{
			if (attachment.m_attachmentType != CA_PROX)
			{
				if (pIAttachment->GetType() != attachment.m_attachmentType)
				{
					pIAttachmentManager->RemoveAttachmentByInterface(pIAttachment, CA_CharEditModel);
					pIAttachment = pIAttachmentManager->CreateAttachment(attachment.m_strSocketName.c_str(), attachment.m_attachmentType, attachment.m_strJointName.c_str());
					if (pIAttachment)
						index = pIAttachmentManager->GetIndexByName(pIAttachment->GetName());
				}
			}
		}

		if (pIAttachment == 0)
		{
			if (index < 0 && attachment.m_attachmentType == CA_PROX)
			{
				IProxy* pIProxy = pIAttachmentManager->GetProxyInterfaceByName(strSocketName);
				if (pIProxy == 0)
				{
					uint32 IsEmpty = attachment.m_strJointName.empty();
					if (IsEmpty == 0)
						pIAttachmentManager->CreateProxy(strSocketName, attachment.m_strJointName);
				}

				//physics-proxies are not in the regular attachment-list
				pIProxy = pIAttachmentManager->GetProxyInterfaceByName(strSocketName);
				if (pIProxy)
				{
					pIProxy->SetProxyParams(attachment.m_ProxyParams);
					pIProxy->SetProxyPurpose(int8(attachment.m_ProxyPurpose));
					QuatT engineLocation = pIProxy->GetProxyAbsoluteDefault();
					QuatT editorLocation = attachment.m_characterSpacePosition;
					if (engineLocation.t != editorLocation.t || engineLocation.q != editorLocation.q)
						pIProxy->SetProxyAbsoluteDefault(editorLocation);
					string lowercaseAttachmentName(attachment.m_strSocketName);
					lowercaseAttachmentName.MakeLower(); // lowercase because the names in the list coming from the engine are lowercase
					proxynames.erase(std::remove(proxynames.begin(), proxynames.end(), lowercaseAttachmentName.c_str()), proxynames.end());
				}
			}
			continue;
		}

		if (pIAttachment->GetType() != attachment.m_attachmentType)
			continue;

		switch (attachment.m_attachmentType)
		{
		case CA_BONE:
			{
				ApplyBoneAttachment(pIAttachment, characterManager, attachment, pICharacterInstance, showDebug);
				break;
			}

		case CA_FACE:
			{
				ApplyFaceAttachment(pIAttachment, characterManager, attachment, showDebug);
				break;
			}

		case CA_SKIN:
			{
				ApplySkinAttachment(pIAttachment, characterManager, attachment, pICharacterInstance, skinSetChanged);
				break;
			}

		case CA_VCLOTH:
			{
				pIAttachment->SetVClothParams(attachment.m_vclothParams);
				if (showDebug)
				{
					pIAttachment->PostUpdateSimulationParams(0, 0);
				}
				break;
			}
		case CA_PROW:
			{
				pIAttachment->SetJointName(attachment.m_strRowJointName.c_str());
				if (showDebug)
				{
					pIAttachment->GetRowParams() = attachment.m_rowSimulationParams;
					pIAttachment->PostUpdateSimulationParams(0, 0);
				}
				break;
			}

		}

		//make sure we don't loose the dyanamic flags
		uint32 nMaskForDynamicFlags = 0;
		nMaskForDynamicFlags |= FLAGS_ATTACH_VISIBLE;
		nMaskForDynamicFlags |= FLAGS_ATTACH_PROJECTED;
		nMaskForDynamicFlags |= FLAGS_ATTACH_WAS_PHYSICALIZED;
		nMaskForDynamicFlags |= FLAGS_ATTACH_HIDE_MAIN_PASS;
		nMaskForDynamicFlags |= FLAGS_ATTACH_HIDE_SHADOW_PASS;
		nMaskForDynamicFlags |= FLAGS_ATTACH_HIDE_RECURSION;
		nMaskForDynamicFlags |= FLAGS_ATTACH_NEAREST_NOFOV;
		nMaskForDynamicFlags |= FLAGS_ATTACH_NO_BBOX_INFLUENCE;
		nMaskForDynamicFlags |= FLAGS_ATTACH_COMBINEATTACHMENT;
		nMaskForDynamicFlags |= FLAGS_ATTACH_MERGED_FOR_SHADOWS;
		uint32 flags = pIAttachment->GetFlags() & nMaskForDynamicFlags;
		pIAttachment->SetFlags(attachment.m_nFlags | flags);
		pIAttachment->HideAttachment((attachment.m_nFlags & FLAGS_ATTACH_HIDE_ATTACHMENT) != 0);

		string lowercaseAttachmentName(attachment.m_strSocketName);
		lowercaseAttachmentName.MakeLower(); // lowercase because the names in the list coming from the engine are lowercase
		names.erase(std::remove(names.begin(), names.end(), lowercaseAttachmentName.c_str()), names.end());
	}

	if (applyPhys)
		ApplyPhysAttachments(pICharacterInstance);

	// remove attachments that weren't updated
	for (size_t i = 0; i < names.size(); ++i)
	{
		pIAttachmentManager->RemoveAttachmentByName(names[i].c_str(), CA_CharEditModel);
		if (skinSetChanged)
			*skinSetChanged = true;
	}
	for (size_t i = 0; i < proxynames.size(); ++i)
		pIAttachmentManager->RemoveProxyByName(proxynames[i].c_str());

	if (skinSetChanged)
	{
		//if the a skin-attachment was changed on the character, then probaly the whole skeleton changed
		//that means we have to set all bone-attachments again to the correct absolute location
		for (uint32 i = 0; i < numAttachments; ++i)
		{
			CharacterAttachment& attachment = attachments[i];
			if (attachment.m_attachmentType == CA_BONE)
			{
				const char* strSocketName = attachment.m_strSocketName.c_str();
				int index = pIAttachmentManager->GetIndexByName(strSocketName);
				IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByIndex(index);
				if (pIAttachment)
				{
					const IDefaultSkeleton& rIDefaultSkeleton = pICharacterInstance->GetIDefaultSkeleton();
					int id = rIDefaultSkeleton.GetJointIDByName(attachment.m_strJointName.c_str());
					if (id == -1)
						continue;

					const QuatT& defaultJointTransform = rIDefaultSkeleton.GetDefaultAbsJointByID(id);
					if (attachment.m_rotationSpace == CharacterAttachment::SPACE_JOINT)
						attachment.m_characterSpacePosition.q = (defaultJointTransform * attachment.m_jointSpacePosition).q.GetNormalized();
					if (attachment.m_positionSpace == CharacterAttachment::SPACE_JOINT)
						attachment.m_characterSpacePosition.t = (defaultJointTransform * attachment.m_jointSpacePosition).t;

					if (attachment.m_simulationParams.m_nClampType == SimulationParams::TRANSLATIONAL_PROJECTION)
					{
						attachment.m_characterSpacePosition = defaultJointTransform;
						attachment.m_jointSpacePosition.SetIdentity();
					}

					QuatT engineLocation = pIAttachment->GetAttAbsoluteDefault();
					QuatT editorLocation = attachment.m_characterSpacePosition;
					if (engineLocation.t != editorLocation.t || engineLocation.q != editorLocation.q)
						pIAttachment->SetAttAbsoluteDefault(editorLocation);
				}
			}
		}
	}

	SynchModifiers(*pICharacterInstance);
	m_initialized = true; //only the serialization code can set this to true.
}

void CharacterDefinition::LoadPhysProxiesFromCharacter(ICharacterInstance *character)
{
	IDefaultSkeleton& skel = character->GetIDefaultSkeleton();
	origBonePhysAtt.clear();
	for(int i = 0; i < skel.GetJointCount(); i++)
		if (const phys_geometry *pPhys = skel.GetJointPhysGeom(i))
		{
			CharacterAttachment att;
			att.m_strJointName = skel.GetJointNameByID(i);
			att.m_strSocketName = string("$") + att.m_strJointName;
			att.m_attachmentType = CA_PROX;
			att.m_ProxyPurpose = CharacterAttachment::RAGDOLL;
			att.m_rotationSpace = CharacterAttachment::SPACE_JOINT;
			att.m_positionSpace = CharacterAttachment::SPACE_JOINT;
			ProxyDimFromGeom(pPhys->pGeom, att.m_jointSpacePosition, att.m_ProxyParams);
			att.m_prevProxyParams = att.m_ProxyParams;
			att.m_ProxyType = att.m_prevProxyType = (geomtypes)pPhys->pGeom->GetType();
			if (att.m_ProxyType == GEOM_TRIMESH)
				(att.m_proxySrc = new CharacterAttachment::ProxySource)->m_proxyMesh = pPhys->pGeom;
			att.m_meshSmooth = att.m_prevMeshSmooth = 0;
			att.m_characterSpacePosition = (att.m_boneTrans = skel.GetDefaultAbsJointByID(i)) * att.m_jointSpacePosition;
			const CryBonePhysics& bp = *skel.GetJointPhysInfo(i);
			for(int l = 0; l < 2; l++) for(int j = 0; j < 3; j++)
				att.m_limits[l][j] = (int)(min(180.0f, max(-180.0f, RAD2DEG(bp.min[l * 3 + j] * (~bp.flags >> j & 1)))) + 180) - 180;
			att.m_damping = *(Vec3*)bp.damping;
			att.m_tension = *(Vec3*)bp.spring_tension;
			Quat q0 = Quat((Matrix33&)bp.framemtx[0][0]);
			int j;
			for(j = skel.GetJointParentIDByID(i); j >= 0 && !skel.GetJointPhysGeom(j); j = skel.GetJointParentIDByID(j))
				;
			if (j >= 0)
				q0 = !skel.GetDefaultAbsJointByID(i).q * skel.GetDefaultAbsJointByID(j).q * q0;
			att.m_frame0 = Ang3(q0);
			for(j = 0; j < 3; j++)
				att.m_frame0[j] = ((int)fabs(att.m_frame0[j] * 10) * 0.1f) * sgnnz(att.m_frame0[j]);
			att.UpdateMirrorInfo(i, skel);

			attachments.push_back(att);
			origBonePhysAtt.push_back(att);
		}
	m_physEdit = true;
}

void CharacterDefinition::SavePhysProxiesToCGF(const char* fname)
{
	IStatObj* pSkelObj = gEnv->p3DEngine->CreateStatObj();
	pSkelObj->FreeIndexedMesh();
	for(int i = 0; i < attachments.size(); i++)
	{
		const CharacterAttachment& att = attachments[i];
		if (att.m_attachmentType != CA_PROX || att.m_ProxyPurpose != CharacterAttachment::RAGDOLL)
			continue;
		IStatObj::SSubObject& subObj = pSkelObj->AddSubObject(gEnv->p3DEngine->CreateStatObj());
		phys_geometry *pGeom = gEnv->pPhysicalWorld->GetGeomManager()->RegisterGeometry(att.CreateProxyGeom());
		pGeom->pGeom->Release();
		subObj.pStatObj->SetPhysGeom(pGeom);
		subObj.pStatObj->SetBBoxMax(Vec3(0.1f));
		subObj.name = att.m_strJointName;
		subObj.properties.Format("%d %d %d %d %d %d %.3f %.3f %.3f %.3f %.3f %.3f %.2f %.2f %.2f",
			att.m_limits[0].x, att.m_limits[0].y, att.m_limits[0].z, att.m_limits[1].x, att.m_limits[1].y, att.m_limits[1].z, 
			att.m_tension.x, att.m_tension.y, att.m_tension.z, att.m_damping.x, att.m_damping.y, att.m_damping.z,
			att.m_frame0.x, att.m_frame0.y, att.m_frame0.z);
		subObj.pStatObj->SetGeoName(subObj.name);
		subObj.pStatObj->SetProperties(subObj.properties);
		subObj.bIdentityMatrix = 0;
		subObj.tm = subObj.localTM = Matrix34(att.m_characterSpacePosition * att.m_jointSpacePosition.GetInverted());
	}
	if (pSkelObj->GetSubObjectCount())
	{
		string dir = fname;
		dir.Truncate(min(dir.length(), dir.find_last_of('/')));
		gEnv->pCryPak->MakeDir(dir);
		if (!pSkelObj->SaveToCGF(fname))
			gEnv->pLog->LogWarning("Failed to write character phys proxies to %s", fname);
	}
	pSkelObj->Release();
	origBonePhysAtt.clear();
	m_physEdit = false;
}

void CharacterDefinition::ApplyBoneAttachment(IAttachment* pIAttachment, ICharacterManager* characterManager, const CharacterAttachment& desc, ICharacterInstance* pICharacterInstance, bool showDebug) const
{
	if (pIAttachment == 0)
		return;

	uint32 type = pIAttachment->GetType();
	if (type == CA_BONE)
	{
		const char* strJointname = desc.m_strJointName.c_str();
		int32 nJointID1 = pIAttachment->GetJointID();
		int32 nJointID2 = pICharacterInstance->GetIDefaultSkeleton().GetJointIDByName(strJointname);
		if (nJointID1 != nJointID2 && nJointID2 != -1)
			pIAttachment->SetJointName(strJointname);

		IAttachmentObject* pIAttachmentObject = pIAttachment->GetIAttachmentObject();
		string existingBindingFilename;
		if (pIAttachmentObject)
		{
			if (IStatObj* statObj = pIAttachmentObject->GetIStatObj())
			{
				existingBindingFilename = statObj->GetFilePath();
			}
			else if (ICharacterInstance* attachedCharacter = pIAttachmentObject->GetICharacterInstance())
			{
				existingBindingFilename = attachedCharacter->GetFilePath();
			}
			else if (IAttachmentSkin* attachmentSkin = pIAttachmentObject->GetIAttachmentSkin())
			{
				if (ISkin* skin = attachmentSkin->GetISkin())
					existingBindingFilename = skin->GetModelFilePath();
			}
		}

		if (stricmp(existingBindingFilename.c_str(), desc.m_strGeometryFilepath.c_str()) != 0)
		{
			if (!desc.m_strGeometryFilepath.empty())
			{
				string fileExt = PathUtil::GetExt(desc.m_strGeometryFilepath.c_str());

				bool IsCDF = (0 == stricmp(fileExt, "cdf"));
				bool IsCHR = (0 == stricmp(fileExt, "chr"));
				bool IsCGA = (0 == stricmp(fileExt, "cga"));
				bool IsCGF = (0 == stricmp(fileExt, "cgf"));
				if (IsCDF || IsCHR || IsCGA)
				{
					ICharacterInstance* pIChildCharacter = characterManager->CreateInstance(desc.m_strGeometryFilepath.c_str(), CA_CharEditModel);
					if (pIChildCharacter)
					{
						CSKELAttachment* pCharacterAttachment = new CSKELAttachment();
						pCharacterAttachment->m_pCharInstance = pIChildCharacter;
						pIAttachmentObject = (IAttachmentObject*)pCharacterAttachment;
					}
				}
				if (IsCGF)
				{
					IStatObj* pIStatObj = gEnv->p3DEngine->LoadStatObj(desc.m_strGeometryFilepath.c_str(), 0, 0, false);
					if (pIStatObj)
					{
						CCGFAttachment* pStatAttachment = new CCGFAttachment();
						pStatAttachment->pObj = pIStatObj;
						pIAttachmentObject = (IAttachmentObject*)pStatAttachment;
					}
				}
				if (pIAttachmentObject != pIAttachment->GetIAttachmentObject())
					pIAttachment->AddBinding(pIAttachmentObject, nullptr, CA_CharEditModel);
			}
			else
			{
				pIAttachment->ClearBinding(CA_CharEditModel);
			}
		}

		SimulationParams& ap = pIAttachment->GetSimulationParams();
		bool bAttachmentSortingRequired = ap.m_nClampType != desc.m_simulationParams.m_nClampType;
		bAttachmentSortingRequired |= uint8(ap.m_useRedirect) != desc.m_simulationParams.m_useRedirect;

		ap.m_nClampType = desc.m_simulationParams.m_nClampType;
		if (showDebug && (ap.m_nClampType == SimulationParams::PENDULUM_CONE || ap.m_nClampType == SimulationParams::PENDULUM_HINGE_PLANE || ap.m_nClampType == SimulationParams::PENDULUM_HALF_CONE))
		{
			ap.m_useDebugSetup = desc.m_simulationParams.m_useDebugSetup;
			ap.m_useDebugText = desc.m_simulationParams.m_useDebugText;
			ap.m_useSimulation = desc.m_simulationParams.m_useSimulation;
			ap.m_useRedirect = desc.m_simulationParams.m_useRedirect;

			ap.m_nSimFPS = desc.m_simulationParams.m_nSimFPS;
			ap.m_fMaxAngle = desc.m_simulationParams.m_fMaxAngle;
			ap.m_vDiskRotation.x = desc.m_simulationParams.m_vDiskRotation.x;

			ap.m_fMass = desc.m_simulationParams.m_fMass;
			ap.m_fGravity = desc.m_simulationParams.m_fGravity;
			ap.m_fDamping = desc.m_simulationParams.m_fDamping;
			ap.m_fStiffness = desc.m_simulationParams.m_fStiffness;

			ap.m_vPivotOffset = desc.m_simulationParams.m_vPivotOffset;
			ap.m_vSimulationAxis = desc.m_simulationParams.m_vSimulationAxis;
			ap.m_vStiffnessTarget = desc.m_simulationParams.m_vStiffnessTarget;
			ap.m_strProcFunction = desc.m_simulationParams.m_strProcFunction;

			ap.m_vCapsule = desc.m_simulationParams.m_vCapsule;
			ap.m_nProjectionType = desc.m_simulationParams.m_nProjectionType;
			ap.m_arrProxyNames = desc.m_simulationParams.m_arrProxyNames;
		}
		if (showDebug && ap.m_nClampType == SimulationParams::SPRING_ELLIPSOID)
		{
			ap.m_useDebugSetup = desc.m_simulationParams.m_useDebugSetup;
			ap.m_useDebugText = desc.m_simulationParams.m_useDebugText;
			ap.m_useSimulation = desc.m_simulationParams.m_useSimulation;
			ap.m_useRedirect = desc.m_simulationParams.m_useRedirect;
			ap.m_nSimFPS = desc.m_simulationParams.m_nSimFPS;

			ap.m_fRadius = desc.m_simulationParams.m_fRadius;
			ap.m_vSphereScale = desc.m_simulationParams.m_vSphereScale;
			ap.m_vDiskRotation = desc.m_simulationParams.m_vDiskRotation;

			ap.m_fMass = desc.m_simulationParams.m_fMass;
			ap.m_fGravity = desc.m_simulationParams.m_fGravity;
			ap.m_fDamping = desc.m_simulationParams.m_fDamping;
			ap.m_fStiffness = desc.m_simulationParams.m_fStiffness;

			ap.m_vStiffnessTarget = desc.m_simulationParams.m_vStiffnessTarget;
			ap.m_strProcFunction = desc.m_simulationParams.m_strProcFunction;

			ap.m_nProjectionType = desc.m_simulationParams.m_nProjectionType;
			ap.m_vCapsule = desc.m_simulationParams.m_vCapsule;
			ap.m_vCapsule.x = 0;
			ap.m_vPivotOffset = desc.m_simulationParams.m_vPivotOffset;
			ap.m_arrProxyNames = desc.m_simulationParams.m_arrProxyNames;
		}
		if (showDebug && ap.m_nClampType == SimulationParams::TRANSLATIONAL_PROJECTION)
		{
			ap.m_useDebugSetup = desc.m_simulationParams.m_useDebugSetup;
			ap.m_useDebugText = desc.m_simulationParams.m_useDebugText;
			ap.m_useSimulation = desc.m_simulationParams.m_useSimulation;
			ap.m_useRedirect = 1;

			ap.m_nProjectionType = desc.m_simulationParams.m_nProjectionType;
			ap.m_strDirTransJoint = desc.m_simulationParams.m_strDirTransJoint;
			ap.m_vSimulationAxis = desc.m_simulationParams.m_vSimulationAxis;

			ap.m_vCapsule = desc.m_simulationParams.m_vCapsule;
			if (ap.m_nProjectionType == ProjectionSelection4::PS4_ShortvecTranslation)
				ap.m_vCapsule.x = 0;
			ap.m_vPivotOffset = desc.m_simulationParams.m_vPivotOffset;
			ap.m_arrProxyNames = desc.m_simulationParams.m_arrProxyNames;
		}
		pIAttachment->PostUpdateSimulationParams(bAttachmentSortingRequired, desc.m_strJointName.c_str());
	}

	QuatT engineLocation = pIAttachment->GetAttAbsoluteDefault();
	QuatT editorLocation = desc.m_characterSpacePosition;
	if (engineLocation.t != editorLocation.t || engineLocation.q != editorLocation.q)
		pIAttachment->SetAttAbsoluteDefault(editorLocation);
}

void CharacterDefinition::ApplyFaceAttachment(IAttachment* pIAttachment, ICharacterManager* characterManager, const CharacterAttachment& desc, bool showDebug) const
{
	if (pIAttachment == 0)
		return;

	uint32 type = pIAttachment->GetType();
	if (type == CA_FACE)
	{
		IAttachmentObject* pIAttachmentObject = pIAttachment->GetIAttachmentObject();
		string existingBindingFilename;
		if (pIAttachmentObject)
		{
			if (IStatObj* statObj = pIAttachmentObject->GetIStatObj())
			{
				existingBindingFilename = statObj->GetFilePath();
			}
			else if (ICharacterInstance* attachedCharacter = pIAttachmentObject->GetICharacterInstance())
			{
				existingBindingFilename = attachedCharacter->GetFilePath();
			}
			else if (IAttachmentSkin* attachmentSkin = pIAttachmentObject->GetIAttachmentSkin())
			{
				if (ISkin* skin = attachmentSkin->GetISkin())
					existingBindingFilename = skin->GetModelFilePath();
			}
		}

		if (stricmp(existingBindingFilename.c_str(), desc.m_strGeometryFilepath.c_str()) != 0)
		{
			if (!desc.m_strGeometryFilepath.empty())
			{
				string fileExt = PathUtil::GetExt(desc.m_strGeometryFilepath.c_str());

				bool IsCDF = (0 == stricmp(fileExt, "cdf"));
				bool IsCHR = (0 == stricmp(fileExt, "chr"));
				bool IsCGA = (0 == stricmp(fileExt, "cga"));
				bool IsCGF = (0 == stricmp(fileExt, "cgf"));
				if (IsCDF || IsCHR || IsCGA)
				{
					ICharacterInstance* pIChildCharacter = characterManager->CreateInstance(desc.m_strGeometryFilepath.c_str(), CA_CharEditModel);
					if (pIChildCharacter)
					{
						CSKELAttachment* pCharacterAttachment = new CSKELAttachment();
						pCharacterAttachment->m_pCharInstance = pIChildCharacter;
						pIAttachmentObject = (IAttachmentObject*)pCharacterAttachment;
					}
				}
				if (IsCGF)
				{
					IStatObj* pIStatObj = gEnv->p3DEngine->LoadStatObj(desc.m_strGeometryFilepath.c_str(), 0, 0, false);
					if (pIStatObj)
					{
						CCGFAttachment* pStatAttachment = new CCGFAttachment();
						pStatAttachment->pObj = pIStatObj;
						pIAttachmentObject = (IAttachmentObject*)pStatAttachment;
					}
				}
				if (pIAttachmentObject != pIAttachment->GetIAttachmentObject())
					pIAttachment->AddBinding(pIAttachmentObject, nullptr, CA_CharEditModel);
			}
			else
			{
				pIAttachment->ClearBinding(CA_CharEditModel);
			}
		}

		SimulationParams& ap = pIAttachment->GetSimulationParams();
		bool bAttachmentSortingRequired = ap.m_nClampType != desc.m_simulationParams.m_nClampType;

		ap.m_nClampType = desc.m_simulationParams.m_nClampType;
		if (showDebug && (ap.m_nClampType == SimulationParams::PENDULUM_CONE || ap.m_nClampType == SimulationParams::PENDULUM_HINGE_PLANE || ap.m_nClampType == SimulationParams::PENDULUM_HALF_CONE))
		{
			ap.m_useDebugSetup = desc.m_simulationParams.m_useDebugSetup;
			ap.m_useDebugText = desc.m_simulationParams.m_useDebugText;
			ap.m_useSimulation = desc.m_simulationParams.m_useSimulation;
			ap.m_useRedirect = 0;
			ap.m_nSimFPS = desc.m_simulationParams.m_nSimFPS;

			ap.m_fMaxAngle = desc.m_simulationParams.m_fMaxAngle;
			ap.m_vDiskRotation.x = desc.m_simulationParams.m_vDiskRotation.x;

			ap.m_fMass = desc.m_simulationParams.m_fMass;
			ap.m_fGravity = desc.m_simulationParams.m_fGravity;
			ap.m_fDamping = desc.m_simulationParams.m_fDamping;
			ap.m_fStiffness = desc.m_simulationParams.m_fStiffness;

			ap.m_vPivotOffset = desc.m_simulationParams.m_vPivotOffset;
			ap.m_vSimulationAxis = desc.m_simulationParams.m_vSimulationAxis;
			ap.m_vStiffnessTarget = desc.m_simulationParams.m_vStiffnessTarget;
			ap.m_strProcFunction.reset();
			ap.m_vCapsule = desc.m_simulationParams.m_vCapsule;

			ap.m_nProjectionType = desc.m_simulationParams.m_nProjectionType;
			ap.m_arrProxyNames = desc.m_simulationParams.m_arrProxyNames;
		}
		if (showDebug && ap.m_nClampType == SimulationParams::SPRING_ELLIPSOID)
		{
			ap.m_useDebugSetup = desc.m_simulationParams.m_useDebugSetup;
			ap.m_useDebugText = desc.m_simulationParams.m_useDebugText;
			ap.m_useSimulation = desc.m_simulationParams.m_useSimulation;
			ap.m_useRedirect = 0;
			ap.m_nSimFPS = desc.m_simulationParams.m_nSimFPS;

			ap.m_fRadius = desc.m_simulationParams.m_fRadius;
			ap.m_vSphereScale = desc.m_simulationParams.m_vSphereScale;
			ap.m_vDiskRotation = desc.m_simulationParams.m_vDiskRotation;

			ap.m_fMass = desc.m_simulationParams.m_fMass;
			ap.m_fGravity = desc.m_simulationParams.m_fGravity;
			ap.m_fDamping = desc.m_simulationParams.m_fDamping;
			ap.m_fStiffness = desc.m_simulationParams.m_fStiffness;

			ap.m_vStiffnessTarget = desc.m_simulationParams.m_vStiffnessTarget;
			ap.m_strProcFunction.reset();

			ap.m_nProjectionType = desc.m_simulationParams.m_nProjectionType;
			ap.m_vCapsule = desc.m_simulationParams.m_vCapsule;
			ap.m_vCapsule.x = 0;
			ap.m_vPivotOffset = desc.m_simulationParams.m_vPivotOffset;
			ap.m_arrProxyNames = desc.m_simulationParams.m_arrProxyNames;
		}

		pIAttachment->PostUpdateSimulationParams(bAttachmentSortingRequired);
	}

	QuatT engineLocation = pIAttachment->GetAttAbsoluteDefault();
	QuatT editorLocation = desc.m_characterSpacePosition;
	if (engineLocation.t != editorLocation.t || engineLocation.q != editorLocation.q)
		pIAttachment->SetAttAbsoluteDefault(desc.m_characterSpacePosition);
}

void CharacterDefinition::ApplySkinAttachment(IAttachment* pIAttachment, ICharacterManager* characterManager, const CharacterAttachment& desc, ICharacterInstance* pICharacterInstance, bool* skinChanged) const
{
	uint32 type = pIAttachment->GetType();
	IAttachmentObject* iattachmentObject = pIAttachment->GetIAttachmentObject();
	IAttachmentManager* attachmentManager = pICharacterInstance->GetIAttachmentManager();

	string existingMaterialFilename;
	string existingBindingFilename;
	bool hasExistingAttachment = false;
	bool hadSoftwareSkinningEnabled = false;

	if (iattachmentObject)
	{
		if (iattachmentObject->GetAttachmentType() == IAttachmentObject::eAttachment_SkinMesh)
		{
			CSKINAttachment* attachmentObject = (CSKINAttachment*)iattachmentObject;
			if (attachmentObject->m_pIAttachmentSkin)
			{
				if (ISkin* skin = attachmentObject->m_pIAttachmentSkin->GetISkin())
				{
					existingBindingFilename = skin->GetModelFilePath();
					if (IMaterial* material = attachmentObject->GetReplacementMaterial(0))
						existingMaterialFilename = material->GetName();
					hasExistingAttachment = true;

					hadSoftwareSkinningEnabled = ((pIAttachment->GetFlags() & FLAGS_ATTACH_SW_SKINNING) != 0);
				}
			}
		}
	}

	if (!hasExistingAttachment
	    || existingBindingFilename != desc.m_strGeometryFilepath
	    || existingMaterialFilename != NormalizeMaterialName(desc.m_strMaterial.c_str())
	    || hadSoftwareSkinningEnabled != ((desc.m_nFlags & FLAGS_ATTACH_SW_SKINNING) != 0))
	{
		string fileExt = PathUtil::GetExt(desc.m_strGeometryFilepath.c_str());
		ISkin* pISkin = 0;
		if (desc.m_strGeometryFilepath.empty())
		{
			pIAttachment->ClearBinding(CA_CharEditModel);
		}
		else
		{
			bool isSkin = stricmp(fileExt, CRY_SKIN_FILE_EXT) == 0;
			if (isSkin)
				pISkin = characterManager->LoadModelSKIN(desc.m_strGeometryFilepath.c_str(), CA_CharEditModel);
		}
		if (pISkin == 0)
			return;

		IAttachmentSkin* attachmentSkin = pIAttachment->GetIAttachmentSkin();
		if (attachmentSkin == 0)
			return;

		CSKINAttachment* attachmentObject = new CSKINAttachment();
		attachmentObject->m_pIAttachmentSkin = attachmentSkin;

		_smart_ptr<IMaterial> material;
		if (!desc.m_strMaterial.empty())
			material.reset(gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(NormalizeMaterialName(desc.m_strMaterial.c_str()).c_str(), false));
		attachmentObject->SetReplacementMaterial(material, 0);

		// AddBinding can destroy attachmentObject!
		pIAttachment->AddBinding(attachmentObject, pISkin, CA_CharEditModel);
		if (!attachmentSkin->GetISkin())
			return;

		if (skinChanged)
			*skinChanged = true;
	}

	return;
}

void CharacterDefinition::SynchModifiers(ICharacterInstance& character)
{
	IAnimationPoseModifierSetupPtr pPoseModifierSetup = character.GetISkeletonAnim()->GetPoseModifierSetup();
	if (pPoseModifierSetup == modifiers)
		return;

	Serialization::CloneBinary(*pPoseModifierSetup, *modifiers);
}

}

