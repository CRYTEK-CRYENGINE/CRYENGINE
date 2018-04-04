#pragma once

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IShader.h>
#include <Cry3DEngine/IRenderNode.h>

#include <CrySchematyc/ResourceTypes.h>
#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

class CPlugin_CryDefaultEntities;

static void ReflectType(Schematyc::CTypeDesc<IFogVolumeRenderNode::eFogVolumeType>& desc)
{
	desc.SetGUID("{2402BB2D-F5D3-47E6-9EDE-D83CC05DC2BF}"_cry_guid);
	desc.SetLabel("Fog Volume Type");
	desc.SetDescription("Type of fog volume");
	desc.SetDefaultValue(IFogVolumeRenderNode::eFogVolumeType_Ellipsoid);
	desc.AddConstant(IFogVolumeRenderNode::eFogVolumeType_Ellipsoid, "Ellipsoid", "Ellipsoid");
	desc.AddConstant(IFogVolumeRenderNode::eFogVolumeType_Box, "Box", "Box");
}

namespace Cry
{
	namespace DefaultComponents
	{
		class CFogComponent
			: public IEntityComponent
		{
		protected:
			friend CPlugin_CryDefaultEntities;
			static void Register(Schematyc::CEnvRegistrationScope& componentScope);

			// IEntityComponent
			virtual void Initialize() final;

			virtual void ProcessEvent(SEntityEvent& event) final;
			virtual uint64 GetEventMask() const final;
			// ~IEntityComponent

		public:
			virtual ~CFogComponent() {}

			static void ReflectType(Schematyc::CTypeDesc<CFogComponent>& desc)
			{
				desc.SetGUID("{5E98F483-BDF2-4497-8654-3474643AF147}"_cry_guid);
				desc.SetEditorCategory("Effects");
				desc.SetLabel("Fog Volume");
				desc.SetDescription("Renders a fog effect");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::ClientOnly });

				desc.AddMember(&CFogComponent::m_bActive, 'actv', "Active", "Active", "Whether or not the fog volume is currently active", true);
				desc.AddMember(&CFogComponent::m_type, 'type', "Type", "Type", "Type of shape to use for rendering the fog volume", IFogVolumeRenderNode::eFogVolumeType_Ellipsoid);
				desc.AddMember(&CFogComponent::m_size, 'size', "Size", "Size", "Size of the fog volume", Vec3(1.f));
				desc.AddMember(&CFogComponent::m_color, 'col', "Color", "Color", "Color of the fog volume", ColorF(1.f));

				desc.AddMember(&CFogComponent::m_options, 'opti', "Options", "Options", nullptr, CFogComponent::SOptions());
			}

			struct SOptions
			{
				inline bool operator==(const SOptions &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

				static void ReflectType(Schematyc::CTypeDesc<SOptions>& desc)
				{
					desc.SetGUID("{7EFB4456-3395-42EE-BE30-5AB565EC7707}"_cry_guid);
					desc.AddMember(&CFogComponent::SOptions::m_bUseGlobalFogColor, 'glob', "UseGlobalFogColor", "Use Global Fog Color", "Whether or not to use the global fog color for this volume", false);
					desc.AddMember(&CFogComponent::SOptions::m_bIgnoreVisAreas, 'igvi', "IgnoreVisAreas", "Ignore VisAreas", nullptr, false);
					desc.AddMember(&CFogComponent::SOptions::m_bAffectsOnlyThisArea, 'area', "OnlyAffectThisArea", "Only Affect This Area", nullptr, false);

					desc.AddMember(&CFogComponent::SOptions::m_globalDensity, 'glde', "GlobalDensity", "Global Density", nullptr, 1.f);
					desc.AddMember(&CFogComponent::SOptions::m_densityOffset, 'deno', "DensityOffset", "Density Offset", nullptr, 0.f);

					//desc.AddMember(&CFogComponent::SOptions::m_nearCutoff, 'necu', "NearCutoff", "Near Cutoff", nullptr, 0.f);
					//desc.AddMember(&CFogComponent::SOptions::m_softEdges, 'soed', "SoftEdges", "Soft Edges", nullptr, 1.f);
					desc.AddMember(&CFogComponent::SOptions::m_heightFallOffDirLong, 'helo', "HeightFalloffDirLong", "Height Falloff Longitude", nullptr, 0.f);
					desc.AddMember(&CFogComponent::SOptions::m_heightFallOffDirLati, 'hela', "HeightFalloffDirLati", "Height Falloff Latitude", nullptr, 0.f);
					desc.AddMember(&CFogComponent::SOptions::m_heightFallOffShift, 'hesh', "HeightFalloffShift", "Height Falloff Shift", nullptr, 0.f);
					desc.AddMember(&CFogComponent::SOptions::m_heightFallOffScale, 'hesc', "HeightFalloffScale", "Height Falloff Scale", nullptr, 1.f);
					//desc.AddMember(&CFogComponent::SOptions::m_rampStart, 'rast', "RampStart", "Ramp Start", nullptr, 0.f);
					//desc.AddMember(&CFogComponent::SOptions::m_rampEnd, 'raen', "RampEnd", "Ramp End", nullptr, 50.f);
					//desc.AddMember(&CFogComponent::SOptions::m_rampInfluence, 'rain', "RampInfluence", "Ramp Influence", nullptr, 0.f);
					//desc.AddMember(&CFogComponent::SOptions::m_windInfluence, 'wiin', "WindInfluence", "Wind Influence", nullptr, 1.f);
					//desc.AddMember(&CFogComponent::SOptions::m_densityNoiseScale, 'dens', "DensityNoiseScale", "Density Noise Scale", nullptr, 0.f);
					//desc.AddMember(&CFogComponent::SOptions::m_densityNoiseOffset, 'deof', "DensityNoiseOffset", "Density Noise Offset", nullptr, 0.f);
					//desc.AddMember(&CFogComponent::SOptions::m_densityNoiseTimeFrequency, 'denf', "DensityNoiseTimeFrequency", "Density Noise Time Frequency", nullptr, 0.f);
					//desc.AddMember(&CFogComponent::SOptions::m_densityNoiseFrequency, 'defn', "DensityNoiseFrequence", "Density Noise Frequenc", nullptr, Vec3(1.f));
				}

				bool m_bUseGlobalFogColor = false;
				bool m_bIgnoreVisAreas = false;
				bool m_bAffectsOnlyThisArea = false;
				Schematyc::Range<0, 10000> m_globalDensity = 1.f;
				Schematyc::Range<0, 100> m_densityOffset = 0.f;
				Schematyc::Range<0, 100> m_nearCutoff = 0.f;
				Schematyc::Range<0, 1> m_softEdges = 1.f;
				Schematyc::Range<0, 100> m_heightFallOffDirLong = 0.f;
				Schematyc::Range<0, 100> m_heightFallOffDirLati = 0.f;
				Schematyc::Range<0, 100> m_heightFallOffShift = 0.f;
				Schematyc::Range<0, 100> m_heightFallOffScale = 1.f;
				Schematyc::Range<0, 30000> m_rampStart = 0.f;
				Schematyc::Range<0, 30000> m_rampEnd = 50.f;
				Schematyc::Range<0, 1> m_rampInfluence = 0.f;
				Schematyc::Range<0, 20> m_windInfluence = 1.f;
				Schematyc::Range<0, 20> m_densityNoiseScale = 0.f;
				Schematyc::Range<-20, 20> m_densityNoiseOffset = 0.f;
				Schematyc::Range<0, 10> m_densityNoiseTimeFrequency = 0.f;
				Vec3 m_densityNoiseFrequency = Vec3(1.f);
			};

			virtual void Enable(bool bEnable)
			{
				if (bEnable)
				{
					SFogVolumeProperties fogProperties;
					fogProperties.m_volumeType = m_type;
					fogProperties.m_size = m_size;
					fogProperties.m_color = m_color;

					fogProperties.m_useGlobalFogColor = m_options.m_bUseGlobalFogColor;
					fogProperties.m_fHDRDynamic = 0.f;

					fogProperties.m_globalDensity = m_options.m_globalDensity;
					fogProperties.m_densityOffset = m_options.m_densityOffset;
					fogProperties.m_nearCutoff = m_options.m_nearCutoff;
					fogProperties.m_softEdges = m_options.m_softEdges;
					fogProperties.m_heightFallOffDirLong = m_options.m_heightFallOffDirLong;
					fogProperties.m_heightFallOffDirLati = m_options.m_heightFallOffDirLati;
					fogProperties.m_heightFallOffShift = m_options.m_heightFallOffShift;
					fogProperties.m_heightFallOffScale = m_options.m_heightFallOffScale;
					fogProperties.m_rampStart = m_options.m_rampStart;
					fogProperties.m_rampEnd = m_options.m_rampEnd;
					fogProperties.m_rampInfluence = m_options.m_rampInfluence;
					fogProperties.m_windInfluence = m_options.m_windInfluence;
					fogProperties.m_densityNoiseScale = m_options.m_densityNoiseScale;
					fogProperties.m_densityNoiseOffset = m_options.m_densityNoiseOffset;
					fogProperties.m_densityNoiseTimeFrequency = m_options.m_densityNoiseTimeFrequency;
					fogProperties.m_densityNoiseFrequency = m_options.m_densityNoiseFrequency;

					const float kiloScale = 1000.0f;
					const float toLightUnitScale = kiloScale / RENDERER_LIGHT_UNIT_SCALE;

					m_pEntity->LoadFogVolume(GetOrMakeEntitySlotId(), fogProperties);
				}
				else
				{
					FreeEntitySlot();
				}

				m_bActive = bEnable;
			}
			bool IsEnabled() const { return m_bActive; }

			virtual void SetType(IFogVolumeRenderNode::eFogVolumeType type) { m_type = type; }
			IFogVolumeRenderNode::eFogVolumeType GetType() const { return m_type; }

			virtual void SetSize(const Vec3& size) { m_size = size; }
			const Vec3& GetSize() const { return m_size; }

			virtual void SetColor(const ColorF& color) { m_color = color; }
			const ColorF& GetColor() const { return m_color; }

			virtual SOptions& GetOptions() { return m_options; }
			const SOptions& GetOptions() const { return m_options; }

		protected:
			bool m_bActive = true;
			IFogVolumeRenderNode::eFogVolumeType m_type = IFogVolumeRenderNode::eFogVolumeType_Ellipsoid;
			Vec3 m_size = Vec3(1.f);
			ColorF m_color = ColorF(1.f);

			SOptions m_options;
		};
	}
}