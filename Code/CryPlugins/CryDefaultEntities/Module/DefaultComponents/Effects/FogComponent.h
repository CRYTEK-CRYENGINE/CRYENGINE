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

			virtual void ProcessEvent(const SEntityEvent& event) final;
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
				desc.AddMember(&CFogComponent::m_emission, 'emi', "Emission", "Emission", "Specifies the color to be multiplied by EmissionIntensity.", ColorF(0.f));
				desc.AddMember(&CFogComponent::m_EmissionIntensity, 'emii', "EmissionIntensity", "Emission Intensity", "Specifies how much luminance (kcd/m2) the fog emits.", 0.0f);

				desc.AddMember(&CFogComponent::m_options, 'opti', "Options", "Options", nullptr, CFogComponent::SOptions());
			}

			struct SOptions
			{
				inline bool operator==(const SOptions &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

				static void ReflectType(Schematyc::CTypeDesc<SOptions>& desc)
				{
					desc.SetGUID("{7EFB4456-3395-42EE-BE30-5AB565EC7707}"_cry_guid);
					desc.AddMember(&CFogComponent::SOptions::m_HDRDynamic, 'hdrd', "HDRDynamic", "HDR Dynamic", "Specifies how much brighter than the default 255,255,255 white the fog is.", 0.f);
					desc.AddMember(&CFogComponent::SOptions::m_bUseGlobalFogColor, 'glob', "UseGlobalFogColor", "Use Global Fog Color", "If true, the Color property is ignored. Instead, the current global fog color is used.", false);
					desc.AddMember(&CFogComponent::SOptions::m_bIgnoreVisAreas, 'igvi', "IgnoreVisAreas", "Ignore VisAreas", "Controls whether this entity should respond to visareas.", false);
					desc.AddMember(&CFogComponent::SOptions::m_bAffectsOnlyThisArea, 'area', "OnlyAffectThisArea", "Only Affect This Area", "Set this parameter to false to make this entity affect in multiple visareas.", false);

					desc.AddMember(&CFogComponent::SOptions::m_globalDensity, 'glde', "GlobalDensity", "Global Density", "Controls the density of the fog. The higher the value the more dense the fog and the less you'll be able to see objects behind or inside the fog volume.", 1.f);
					desc.AddMember(&CFogComponent::SOptions::m_densityOffset, 'deno', "DensityOffset", "Density Offset", "Offset fog density, used in conjunction with the GlobalDensity parameter.", 0.f);

					desc.AddMember(&CFogComponent::SOptions::m_nearCutoff, 'necu', "NearCutoff", "Near Cutoff", "Stop rendering the object, depending on camera distance to object.", 0.f);
					desc.AddMember(&CFogComponent::SOptions::m_softEdges, 'soed', "SoftEdges", "Soft Edges", "Specifies a factor that is used to soften the edges of the fog volume when viewed from outside.", 1.f);
					desc.AddMember(&CFogComponent::SOptions::m_heightFallOffDirLong, 'helo', "HeightFalloffDirLong", "Height Falloff Longitude", "Controls the longitude of the world space fall off direction of the fog. 0 represents East, rotation is counter-clockwise.", 0.f);
					desc.AddMember(&CFogComponent::SOptions::m_heightFallOffDirLati, 'hela', "HeightFalloffDirLati", "Height Falloff Latitude", "Controls the latitude of the world space fall off direction of the fog. 90 lets the fall off direction point upwards in world space.", 90.f);
					desc.AddMember(&CFogComponent::SOptions::m_heightFallOffShift, 'hesh', "HeightFalloffShift", "Height Falloff Shift", "Controls how much to shift the fog density distribution along the fall off direction in world units (m).", 0.f);
					desc.AddMember(&CFogComponent::SOptions::m_heightFallOffScale, 'hesc', "HeightFalloffScale", "Height Falloff Scale", "Scales the density distribution along the fall off direction. Higher values will make the fog fall off more rapidly and generate thicker fog layers along the negative fall off direction.", 1.f);
					desc.AddMember(&CFogComponent::SOptions::m_rampStart, 'rast', "RampStart", "Ramp Start", "Specifies the start distance of fog density ramp in world units (m).", 0.f);
					desc.AddMember(&CFogComponent::SOptions::m_rampEnd, 'raen', "RampEnd", "Ramp End", "Specifies the end distance of fog density ramp in world units (m).", 50.f);
					desc.AddMember(&CFogComponent::SOptions::m_rampInfluence, 'rain', "RampInfluence", "Ramp Influence", "Controls the influence of fog density ramp.", 0.f);
					desc.AddMember(&CFogComponent::SOptions::m_windInfluence, 'wiin', "WindInfluence", "Wind Influence", "Controls the influence of the wind.", 1.f);
					desc.AddMember(&CFogComponent::SOptions::m_densityNoiseScale, 'dens', "DensityNoiseScale", "Density Noise Scale", "Scales the noise for the density.", 1.f);
					desc.AddMember(&CFogComponent::SOptions::m_densityNoiseOffset, 'deof', "DensityNoiseOffset", "Density Noise Offset", "Offsets the noise for the density.", 1.f);
					desc.AddMember(&CFogComponent::SOptions::m_densityNoiseTimeFrequency, 'denf', "DensityNoiseTimeFrequency", "Density Noise Time Frequency", "Controls the time frequency of the noise for the density.", 0.f);
					desc.AddMember(&CFogComponent::SOptions::m_densityNoiseFrequency, 'defn', "DensityNoiseFrequence", "Density Noise Frequency", "Controls the spatial frequency of the noise for the density.", Vec3(1.f));
				}

				bool m_bUseGlobalFogColor = false;
				bool m_bIgnoreVisAreas = false;
				bool m_bAffectsOnlyThisArea = false;
				Schematyc::Range<0, 1000, 0, 1000, float> m_globalDensity = 1.f;
				Schematyc::Range<-10, 20, -10, 20, float> m_HDRDynamic = 0.f;
				Schematyc::Range<-1000, 1000, -1000, 1000, float> m_densityOffset = 0.f;
				Schematyc::Range<0, 2, 0, 2, float> m_nearCutoff = 0.f;
				Schematyc::Range<0, 1, 0, 1, float> m_softEdges = 1.f;
				Schematyc::Range<0, 360, 0, 360, float> m_heightFallOffDirLong = 0.f;
				Schematyc::Range<0, 360, 0, 360, float> m_heightFallOffDirLati = 90.f;
				Schematyc::Range<-100, 100, -100, 100, float> m_heightFallOffShift = 0.f;
				Schematyc::Range<-100, 100, -100, 100, float> m_heightFallOffScale = 1.f;
				Schematyc::Range<0, 30000, 0, 30000, float> m_rampStart = 0.f;
				Schematyc::Range<0, 30000, 0, 30000, float> m_rampEnd = 50.f;
				Schematyc::Range<0, 1, 0, 1, float> m_rampInfluence = 0.f;
				Schematyc::Range<0, 20, 0, 20, float> m_windInfluence = 1.f;
				Schematyc::Range<0, 10, 0, 10, float> m_densityNoiseScale = 1.f;
				Schematyc::Range<-2, 2, -2, 2, float> m_densityNoiseOffset = 1.f;
				Schematyc::Range<0, 1, 0, 1, float> m_densityNoiseTimeFrequency = 0.f;

				Vec3 m_densityNoiseFrequency = Vec3(10.f);
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
					fogProperties.m_fHDRDynamic = m_options.m_HDRDynamic;

					fogProperties.m_affectsThisAreaOnly = m_options.m_bAffectsOnlyThisArea;
					fogProperties.m_ignoresVisAreas = m_options.m_bIgnoreVisAreas;

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

					m_emission.adjust_luminance(m_EmissionIntensity * toLightUnitScale);

					fogProperties.m_emission = m_emission.toVec3();

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

			virtual void SetEmission(const ColorF& color) { m_emission = color; }
			const ColorF& GetEmission() const { return m_emission; }

			virtual SOptions& GetOptions() { return m_options; }
			const SOptions& GetOptions() const { return m_options; }

		protected:
			bool m_bActive = true;
			IFogVolumeRenderNode::eFogVolumeType m_type = IFogVolumeRenderNode::eFogVolumeType_Ellipsoid;
			Vec3 m_size = Vec3(1.f);
			ColorF m_color = ColorF(1.f);
			ColorF m_emission = ColorF(0.f);
			Schematyc::Range<0, 100> m_EmissionIntensity = 0.f;

			SOptions m_options;
		};
	}
}