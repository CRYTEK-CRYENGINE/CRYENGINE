#include "StdAfx.h"
#include "BreakableJoint.h"

namespace Cry
{
	namespace DefaultComponents
	{
		void CBreakableJointComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
		{
			// Functions

			// Signals
			{
				auto pSignal = SCHEMATYC_MAKE_ENV_SIGNAL(CBreakableJointComponent::SBrokenSignal);
				pSignal->SetDescription("Sent when the joint breaks");
				componentScope.Register(pSignal);
			}
		}

		void CBreakableJointComponent::ReflectType(Schematyc::CTypeDesc<CBreakableJointComponent>& desc)
		{
			desc.SetGUID("{F5C08E7D-6C16-422e-AF2F-4BD19A55E81F}"_cry_guid);
			desc.SetEditorCategory("Physics Constraints");
			desc.SetLabel("Internal Breakable Joint");
			desc.SetDescription("Holds parts of the entity together until broken");
			//desc.SetIcon("icons:ObjectTypes/object.ico");
			desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });

			desc.AddMember(&CBreakableJointComponent::m_maxForcePush, 'push', "MaxPush", "Max Push Force", "Push force limit along the normal", 1e6f);
			desc.AddMember(&CBreakableJointComponent::m_maxForcePull, 'pull', "MaxPull", "Max Pull Force", "Pull force limit along the normal", 1e6f);
			desc.AddMember(&CBreakableJointComponent::m_maxForceShift, 'shft', "MaxShift", "Max Shift Force", "Force limit orthogonal to the normal", 1e6f);
			desc.AddMember(&CBreakableJointComponent::m_maxTorqueBend, 'bend', "MaxBend", "Max Bend Torque", "Normal bend torque limit", 1e6f);
			desc.AddMember(&CBreakableJointComponent::m_maxTorqueTwist, 'twst', "MaxTwist", "Max Twist Torque", "Twist torque limit arounf the normal", 1e6f);
			desc.AddMember(&CBreakableJointComponent::m_defDmgAccum, 'dfda', "DefaultDamageAccum", "DefaultDamageAccum", "Use default damage accum and threshold (from CVar)", true);
			desc.AddMember(&CBreakableJointComponent::m_damageAccum, 'dmga', "DmgAccum", "Damage Accumulation Fraction", "Accumulated fraction of damage", 0.0f);
			desc.AddMember(&CBreakableJointComponent::m_damageAccumThresh, 'dmgt', "DmgThresh", "Damage Accumulation Threshold", 
				"Threshold of damage accumulation, normalized to 0..1", 0.0f);
			desc.AddMember(&CBreakableJointComponent::m_breakable, 'brk', "Brekable", "Breakable", nullptr, true);
			desc.AddMember(&CBreakableJointComponent::m_directBreaksOnly, 'dbrk', "DirectBrk", "Direct Breaks Only", "Break only if impulse was applied to one of the connected parts", false);
			desc.AddMember(&CBreakableJointComponent::m_szSensor, 'sens', "SzSensor", "Sensor Size", "Sensor sphere diameter for attachment and re-attachment", 0.05f);
			desc.AddMember(&CBreakableJointComponent::m_constr, 'cons', "Constraint", "Dynamic Constraint", 
				"Properties of the constraint created after joint breaking", CBreakableJointComponent::SDynConstraint());
		}

		static void ReflectType(Schematyc::CTypeDesc<CBreakableJointComponent::SDynConstraint>& desc)
		{
			desc.SetGUID("{BA874580-D9CF-4db0-B4BB-8B4E95CF867D}"_cry_guid);
			desc.AddMember(&CBreakableJointComponent::SDynConstraint::m_minAngle, 'cmin', "ConstrMinA", "Min Angle", nullptr, 0.0_degrees);
			desc.AddMember(&CBreakableJointComponent::SDynConstraint::m_maxAngle, 'cmax', "ConstrMaxA", "Max Angle", nullptr, 0.0_degrees);
			desc.AddMember(&CBreakableJointComponent::SDynConstraint::m_maxForce, 'clim', "ConstrLim", "Force Limit", "Bend torque limit for breaking the dynamic constraint", 0.0f);
			desc.AddMember(&CBreakableJointComponent::SDynConstraint::m_noColl, 'ccol', "ConstrNoColl", "Ignore Collisions", "Ignore collisions between the connected parts", false);
			desc.AddMember(&CBreakableJointComponent::SDynConstraint::m_damping, 'cdmp', "ConstrDamping", "Damping", "Contraint-specific damping", 0.0f);
		}

		static void ReflectType(Schematyc::CTypeDesc<CBreakableJointComponent::SBrokenSignal>& desc)
		{
			desc.SetGUID("{BC763AAA-C1CB-4f83-8F7E-4AF3F3CC156B}"_cry_guid);
			desc.SetLabel("On Break");
			desc.AddMember(&CBreakableJointComponent::SBrokenSignal::idJoint, 'id', "Id", "Joint Id", nullptr, 0);
			desc.AddMember(&CBreakableJointComponent::SBrokenSignal::point, 'pt', "Point", "Location", nullptr, Vec3(ZERO));
			desc.AddMember(&CBreakableJointComponent::SBrokenSignal::normal, 'norm', "Normal", "Normal", nullptr, Vec3(0,0,1));
		}

		void CBreakableJointComponent::Initialize()
		{
		}

		void CBreakableJointComponent::Physicalize()
		{
			if (IPhysicalEntity *pent = m_pEntity->GetPhysics())
			{
				pe_params_structural_joint psj;
				psj.id = GetOrMakeEntitySlotId();
				Quat q = GetTransform()->GetRotation().ToQuat();
				psj.n = q*Vec3(0,0,1);
				psj.pt = GetTransform()->GetTranslation();
				psj.axisx = q*Vec3(1,0,0);
				psj.partid[0] = psj.partid[1] = -1;
					
				primitives::sphere sph;
				sph.r = 0.5f; sph.center.zero();
				static IGeometry *pSph = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::sphere::type, &sph);
				geom_world_data gwd[2];
				gwd[0].offset = psj.pt;
				gwd[0].scale = m_szSensor;
				intersection_params ip;
				ip.bNoAreaContacts = true;
				float Vnorm = 0;
				for(int i = m_pEntity->GetSlotCount()-1, nslots = 0; i >= 0 && nslots < 2; i--)
				{
					IStatObj *pStatObj = m_pEntity->GetStatObj(i);
					if (!pStatObj || !pStatObj->GetPhysGeom())
						continue;
					const Matrix34 &mtx = m_pEntity->GetSlotLocalTM(i, false);
					gwd[1].offset = mtx.GetTranslation();
					gwd[1].scale = mtx.GetColumn0().len();
					gwd[1].R = Matrix33(mtx) / gwd[1].scale;
					geom_contact *pcont;
					if (pSph->Intersect(pStatObj->GetPhysGeom()->pGeom, gwd, gwd+1, &ip, pcont))
					{
						float V = pStatObj->GetPhysGeom()->pGeom->GetVolume() * cube(gwd[1].scale);
						if (V > Vnorm)
						{
							psj.n = -pcont->n;
							V = Vnorm;
						}
						psj.partid[nslots++] = i;
					}
				}
				(psj.axisx -= psj.n*(psj.n*psj.axisx)).normalize();

				psj.maxForcePush = m_maxForcePush;
				psj.maxForcePull = m_maxForcePull;
				psj.maxForceShift = m_maxForceShift;
				psj.maxTorqueBend = m_maxTorqueBend;
				psj.maxTorqueTwist = m_maxTorqueTwist;
				if (!m_defDmgAccum)
				{
					psj.damageAccum = m_damageAccum;
					psj.damageAccumThresh = m_damageAccumThresh;
				}
				psj.bBreakable = m_breakable ? 1 : 0;
				psj.limitConstraint = Vec3(m_constr.m_minAngle.ToRadians(), m_constr.m_maxAngle.ToRadians(), m_constr.m_maxForce);
				psj.bConstraintWillIgnoreCollisions = m_constr.m_noColl ? 1 : 0;
				psj.dampingConstraint = m_constr.m_damping;
				psj.bDirectBreaksOnly = m_directBreaksOnly ? 1 : 0;
				psj.szSensor = m_szSensor;
				psj.bReplaceExisting = 1;

				pent->SetParams(&psj);
			}
		}

		void CBreakableJointComponent::ProcessEvent(const SEntityEvent& event)
		{
			if (event.event == ENTITY_EVENT_PHYSICAL_TYPE_CHANGED)
			{
				m_pEntity->UpdateComponentEventMask(this);
				Physicalize();
			}
			else if (event.event == ENTITY_EVENT_COLLISION)
			{
				EventPhysJointBroken* jbrk = reinterpret_cast<EventPhysJointBroken*>(event.nParam[0]);
				if (jbrk->idJoint == GetEntitySlotId())
				{
					m_pEntity->GetSchematycObject()->ProcessSignal(SBrokenSignal(jbrk->idJoint, jbrk->pt, jbrk->n), GetGUID());
				}
			}
		}
	}
}