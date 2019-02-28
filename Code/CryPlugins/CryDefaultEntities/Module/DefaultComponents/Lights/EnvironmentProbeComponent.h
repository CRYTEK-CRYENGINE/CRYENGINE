#pragma once

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IShader.h>
#include <CrySystem/IProjectManager.h>
#include <CryGame/IGameFramework.h>
#include <ILevelSystem.h>

#include <CrySchematyc/ResourceTypes.h>
#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h>

class CPlugin_CryDefaultEntities;

namespace Cry
{
	namespace DefaultComponents
	{
#if !defined(RELEASE) && defined(WIN32) && !defined(CRY_PLATFORM_CONSOLE)
#define SUPPORT_ENVIRONMENT_PROBE_GENERATION
#endif

		class CEnvironmentProbeComponent
			: public IEntityComponent
#ifndef RELEASE
			, public IEntityComponentPreviewer
#endif
		{
		protected:
			friend CPlugin_CryDefaultEntities;
			static void Register(Schematyc::CEnvRegistrationScope& componentScope);

			// IEntityComponent
			virtual void Initialize() final;

			virtual void   ProcessEvent(const SEntityEvent& event) final;
			virtual uint64 GetEventMask() const final;

#ifndef RELEASE
			virtual IEntityComponentPreviewer* GetPreviewer() final { return this; }
#endif
			// ~IEntityComponent

#ifndef RELEASE
			// IEntityComponentPreviewer
			virtual void SerializeProperties(Serialization::IArchive& archive) final {}

			virtual void Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const final;
			// ~IEntityComponentPreviewer
#endif

		public:
			CEnvironmentProbeComponent() = default;
			virtual ~CEnvironmentProbeComponent() {}

			static void ReflectType(Schematyc::CTypeDesc<CEnvironmentProbeComponent>& desc)
			{
				desc.SetGUID("63297E89-EA00-4F56-AC23-2E93C68FE71D"_cry_guid);
				desc.SetEditorCategory("Lights");
				desc.SetLabel("Environment Probe");
				desc.SetDescription("Captures an image of its full surroundings and used to light nearby objects with reflective materials");
				desc.SetIcon("icons:Designer/Designer_Box.ico");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::ClientOnly });
				
				desc.AddMember(&CEnvironmentProbeComponent::m_generation, 'gen', "Generation", "Generation Parameters", "Parameters for default cube map generation and load", CEnvironmentProbeComponent::SGeneration());
				desc.AddMember(&CEnvironmentProbeComponent::m_bActive, 'actv', "Active", "Active", "Determines whether the environment probe is enabled", true);
				desc.AddMember(&CEnvironmentProbeComponent::m_extents, 'exts', "BoxSize", "Box Size", "Size of the area the probe affects.", Vec3(10.f));
				desc.AddMember(&CEnvironmentProbeComponent::m_options, 'opt', "Options", "Options", "Specific Probe Options", CEnvironmentProbeComponent::SOptions());
				desc.AddMember(&CEnvironmentProbeComponent::m_color, 'colo', "Color", "Color", "Probe Color emission information", CEnvironmentProbeComponent::SColor());
			}

			struct SOptions
			{
				inline bool operator==(const SOptions &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

				static void ReflectType(Schematyc::CTypeDesc<SOptions>& desc)
				{
					desc.SetGUID("{DB10AB64-7A5B-4B91-BC90-6D692D1D1222}"_cry_guid);
					desc.AddMember(&CEnvironmentProbeComponent::SOptions::m_bIgnoreVisAreas, 'igvi', "IgnoreVisAreas", "Ignore VisAreas", nullptr, false);
					desc.AddMember(&CEnvironmentProbeComponent::SOptions::sortPriority, 'spri', "SortPriority", "Sort Priority", nullptr, (uint32)SRenderLight().m_nSortPriority);
					desc.AddMember(&CEnvironmentProbeComponent::SOptions::m_attenuationFalloffMax, 'atte', "AttenuationFalloffMax", "Maximum Attenuation Falloff", nullptr, 1.f);
					desc.AddMember(&CEnvironmentProbeComponent::SOptions::m_bAffectsVolumetricFog, 'volf', "AffectVolumetricFog", "Affect Volumetric Fog", nullptr, true);
					desc.AddMember(&CEnvironmentProbeComponent::SOptions::m_bVolumetricFogOnly, 'volo', "VolumetricFogOnly", "Only Affect Volumetric Fog", nullptr, false);
					desc.AddMember(&CEnvironmentProbeComponent::SOptions::m_bAffectsOnlyThisArea, 'area', "OnlyAffectThisArea", "Only Affect This Area", nullptr, true);
					desc.AddMember(&CEnvironmentProbeComponent::SOptions::m_bBoxProjection, 'boxp', "BoxProjection", "Use Box Projection", nullptr, false);
				}

				bool m_bIgnoreVisAreas = false;
				uint32 sortPriority = SRenderLight().m_nSortPriority;
				Schematyc::Range<0, 64000> m_attenuationFalloffMax = 1.f;
				bool m_bVolumetricFogOnly = false;
				bool m_bAffectsVolumetricFog = true;
				bool m_bAffectsOnlyThisArea = true;
				bool m_bBoxProjection = false;
			};

			struct SColor
			{
				inline bool operator==(const SColor &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

				static void ReflectType(Schematyc::CTypeDesc<SColor>& desc)
				{
					desc.SetGUID("{B71C3414-F85A-4EAA-9CE0-5110A2E040AD}"_cry_guid);
					desc.AddMember(&CEnvironmentProbeComponent::SColor::m_color, 'col', "Color", "Color", nullptr, ColorF(1.f));
					desc.AddMember(&CEnvironmentProbeComponent::SColor::m_diffuseMultiplier, 'diff', "DiffMult", "Diffuse Multiplier", nullptr, 1.f);
					desc.AddMember(&CEnvironmentProbeComponent::SColor::m_specularMultiplier, 'spec', "SpecMult", "Specular Multiplier", nullptr, 1.f);
				}

				ColorF m_color = ColorF(1.f);
				Schematyc::Range<0, 100> m_diffuseMultiplier = 1.f;
				Schematyc::Range<0, 100> m_specularMultiplier = 1.f;
			};

			struct SGeneration
			{
				inline bool operator==(const SGeneration &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

#ifdef SUPPORT_ENVIRONMENT_PROBE_GENERATION
				enum class EResolution
				{
					x256 = 256,
					x512 = 512,
					x1024 = 1024
				};
#endif

				static void ReflectType(Schematyc::CTypeDesc<CEnvironmentProbeComponent::SGeneration>& desc)
				{
					desc.SetGUID("{1CF55472-FAD0-42E7-B06A-20B96F954914}"_cry_guid);
					desc.AddMember(&CEnvironmentProbeComponent::SGeneration::m_generatedCubemapPath, 'tex', "GeneratedCubemapPath", "Cube Map Path", nullptr, "");
					desc.AddMember(&CEnvironmentProbeComponent::SGeneration::m_bAutoLoad, 'auto', "AutoLoad", "Load Texture Automatically", "If set, tries to load the cube map texture specified on component creation", true);

#ifdef SUPPORT_ENVIRONMENT_PROBE_GENERATION
					desc.AddMember(&CEnvironmentProbeComponent::SGeneration::m_resolution, 'res', "Resolution", "Resolution", "Resolution to render cube map at", CEnvironmentProbeComponent::SGeneration::EResolution::x256);
					desc.AddMember(&CEnvironmentProbeComponent::SGeneration::m_generateButton, 'btn', "Generate", "Generate", "Generates the cube map", Serialization::FunctorActionButton<std::function<void()>>());
#endif
				}

				bool m_bAutoLoad = true;

#ifdef SUPPORT_ENVIRONMENT_PROBE_GENERATION
				Serialization::FunctorActionButton<std::function<void()>> m_generateButton;
				EResolution m_resolution = EResolution::x256;
#endif

#ifndef RELEASE
				_smart_ptr<IStatObj> pSelectionObject;
#endif

				Schematyc::TextureFileName m_generatedCubemapPath;
			};

#ifdef SUPPORT_ENVIRONMENT_PROBE_GENERATION
			virtual void Generate(const Schematyc::TextureFileName& cubemapFileName)
			{
				IRenderNode* pProbeRenderNode = m_pEntity->GetRenderNode(GetEntitySlotId());

				// Hide the environment probe before generating, to prevent recursive dependency on itself
				if (pProbeRenderNode != nullptr)
				{
					pProbeRenderNode->SetRndFlags(ERF_HIDDEN, true);
				}

				string outputPath = cubemapFileName.value;

				// Fix up the texture path, just in case
				string texname = PathUtil::GetFileName(outputPath);
				string path = PathUtil::GetPathWithoutFilename(outputPath);

				// Add _CM suffix if missing
				int32 nCMSufixCheck = texname.Find("_cm");
				outputPath = PathUtil::Make(path, texname + ((nCMSufixCheck == -1) ? "_cm.tif" : ".tif"));
				outputPath = PathUtil::ToUnixPath(outputPath);

				// Now generate the cube map
				// Render 16x bigger cube map (4x4) - 16x SSAA
				const auto renderResolution = static_cast<std::size_t>(m_generation.m_resolution) * 4;
				const auto srcPitch = renderResolution * 4;
				const auto srcSideSize = renderResolution * srcPitch;
				const auto sides = 6;

				const auto& vecData = gEnv->pRenderer->EF_RenderEnvironmentCubeHDR(renderResolution, m_pEntity->GetWorldPos());
				if (vecData.size() >= srcSideSize * sides)
				{
					TArray<unsigned short> downsampledImageData;

					int width = (int)m_generation.m_resolution * 4 * 6;
					int height = (int)m_generation.m_resolution;

					downsampledImageData.resize(width * height);

					size_t dstPitch = (int)m_generation.m_resolution * 4;
					for (int side = 0; side < sides; ++side)
					{
						for (uint32 y = 0; y < (uint32)m_generation.m_resolution; ++y)
						{
							CryHalf4* pSrcSide = (CryHalf4*)&vecData[side * srcSideSize];
							CryHalf4* pDst = (CryHalf4*)&downsampledImageData[side * dstPitch + y * width];
							for (uint32 x = 0; x < (uint32)m_generation.m_resolution; ++x)
							{
								Vec4 cResampledColor(0.f, 0.f, 0.f, 0.f);

								// re-sample the image at the original size
								for (uint32 yres = 0; yres < 4; ++yres)
								{
									for (uint32 xres = 0; xres < 4; ++xres)
									{
										const CryHalf4& pSrc = pSrcSide[(y * 4 + yres) * renderResolution + (x * 4 + xres)];
										cResampledColor += Vec4(CryConvertHalfToFloat(pSrc.x), CryConvertHalfToFloat(pSrc.y), CryConvertHalfToFloat(pSrc.z), CryConvertHalfToFloat(pSrc.w));
									}
								}

								cResampledColor /= 16.f;

								*pDst++ = CryHalf4(cResampledColor.x, cResampledColor.y, cResampledColor.z, cResampledColor.w);
							}
						}
					}

					outputPath = PathUtil::Make(gEnv->pSystem->GetIProjectManager()->GetCurrentAssetDirectoryAbsolute(), outputPath);

					string outputDirectory = PathUtil::GetPathWithoutFilename(outputPath);
					gEnv->pCryPak->MakeDir(outputDirectory);

					gEnv->pRenderer->WriteTIFToDisk(downsampledImageData.Data(), (int)m_generation.m_resolution * 6, (int)m_generation.m_resolution, 2, 4, true, "HDRCubemap_highQ", outputPath);

					CResourceCompilerHelper::ERcCallResult result = CResourceCompilerHelper::CallResourceCompiler(outputPath, nullptr, nullptr, false, CResourceCompilerHelper::eRcExePath_editor, true, true);
					if (result == CResourceCompilerHelper::eRcCallResult_success)
					{
						LoadFromDisk(cubemapFileName);
					}
				}
				else
				{
					CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed to generate cube map!");
				}

				// Un-hide the probe again
				if (pProbeRenderNode != nullptr)
				{
					pProbeRenderNode->SetRndFlags(ERF_HIDDEN, false);
				}
			}

			virtual void GenerateToDefaultPath()
			{
				// Set the output path if nothing was specified by the user
				if (!HasOutputCubemapPath())
				{
					const char* szLevelName;

					if (ILevelInfo* pLevel = gEnv->pGameFramework->GetILevelSystem()->GetCurrentLevel())
					{
						szLevelName = pLevel->GetName();
					}
					else
					{
						szLevelName = "UnknownLevel";
					}

					string folder = string("textures\\cubemaps\\") + szLevelName;
					m_generation.m_generatedCubemapPath.value = folder + "\\" + m_pEntity->GetName() + string("_cm.tif");
				}

				Generate(m_generation.m_generatedCubemapPath);
			}
#endif

			virtual void LoadFromDisk(const Schematyc::TextureFileName& cubemapFileName)
			{
				ITexture* pSpecularTexture, *pDiffuseTexture;
				if (GetCubemapTextures(cubemapFileName.value, &pSpecularTexture, &pDiffuseTexture))
				{
					Load(pSpecularTexture, pDiffuseTexture);

#ifndef RELEASE
					if (gEnv->IsEditor())
					{
						IMaterialManager* pMaterialManager = gEnv->p3DEngine->GetMaterialManager();
						if (IMaterial* pSourceMaterial = pMaterialManager->LoadMaterial("%EDITOR%/Objects/envcube", false, true))
						{
							IMaterial* pMaterial = pMaterialManager->CreateMaterial(cubemapFileName.value, pSourceMaterial->GetFlags() | MTL_FLAG_NON_REMOVABLE);
							if (pMaterial)
							{
								SShaderItem& si = pSourceMaterial->GetShaderItem();
								SInputShaderResourcesPtr isr = gEnv->pRenderer->EF_CreateInputShaderResource(si.m_pShaderResources);
								isr->m_Textures[EFTT_ENV].m_Name = cubemapFileName.value;
								SShaderItem siDst = gEnv->pRenderer->EF_LoadShaderItem(si.m_pShader->GetName(), true, 0, isr, si.m_pShader->GetGenerationMask());
								pMaterial->AssignShaderItem(siDst);

								m_generation.pSelectionObject->SetMaterial(pMaterial);
							}
						}
					}
#endif
				}
				else
				{
					FreeEntitySlot();
				}
			}

			virtual void SetCubemap(const Schematyc::ExplicitTextureId specularTextureId, const Schematyc::ExplicitTextureId diffuseTextureId)
			{
				ITexture* pSpecularTexture = gEnv->pRenderer->EF_GetTextureByID((int)specularTextureId);
				ITexture *pDiffuseTexture = gEnv->pRenderer->EF_GetTextureByID((int)diffuseTextureId);

				if (pDiffuseTexture != nullptr && pSpecularTexture != nullptr)
				{
					Load(pSpecularTexture, pDiffuseTexture);
				}
				else
				{
					FreeEntitySlot();

					if (pDiffuseTexture == nullptr)
					{
						CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed to load set cube map for environment probe component attached to entity %s, diffuse texture with id %i did not exist!!", m_pEntity->GetName(), (int)diffuseTextureId);
					}
					if (pSpecularTexture == nullptr)
					{
						CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed to load set cube map for environment probe component attached to entity %s, specular texture with id %i did not exist!!", m_pEntity->GetName(), (int)specularTextureId);
					}
				}
			}

			virtual bool GetCubemapTextures(const char* path, ITexture** pSpecular, ITexture** pDiffuse, bool bLoadFromDisk = true) const
			{
				stack_string specularCubemap = PathUtil::ReplaceExtension(path, ".dds");

				int strIndex = specularCubemap.find("_diff");
				if (strIndex >= 0)
				{
					specularCubemap = specularCubemap.substr(0, strIndex) + specularCubemap.substr(strIndex + 5, specularCubemap.length());
				}

				char diffuseCubemap[ICryPak::g_nMaxPath];
				cry_sprintf(diffuseCubemap, "%s%s%s.%s", PathUtil::AddSlash(PathUtil::GetPathWithoutFilename(specularCubemap)).c_str(),
					PathUtil::GetFileName(specularCubemap).c_str(), "_diff", PathUtil::GetExt(specularCubemap));

				// '\\' in filename causing texture duplication
				stack_string specularCubemapUnix = PathUtil::ToUnixPath(specularCubemap.c_str());
				stack_string diffuseCubemapUnix = PathUtil::ToUnixPath(diffuseCubemap);

				if (bLoadFromDisk)
				{
					if (!gEnv->pCryPak->IsFileExist(specularCubemapUnix) || !gEnv->pCryPak->IsFileExist(diffuseCubemapUnix))
					{
						CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed to load cube map for environment probe component attached to entity %s!", m_pEntity->GetName());
						return false;
					}

					*pSpecular = gEnv->pRenderer->EF_LoadTexture(specularCubemapUnix, FT_DONT_STREAM);
					*pDiffuse = gEnv->pRenderer->EF_LoadTexture(diffuseCubemapUnix, FT_DONT_STREAM);
				}
				else
				{
					*pSpecular = gEnv->pRenderer->EF_GetTextureByName(specularCubemapUnix, FT_DONT_STREAM);
					*pDiffuse = gEnv->pRenderer->EF_GetTextureByName(diffuseCubemapUnix, FT_DONT_STREAM);
				}
				
				if (*pSpecular == nullptr || !(*pSpecular)->IsTextureLoaded())
				{
					CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed to load specular cube map for environment probe component attached to entity %s!", m_pEntity->GetName());
					return false;
				}

				if (*pDiffuse == nullptr || !(*pDiffuse)->IsTextureLoaded())
				{
					CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed to load diffuse cube map for environment probe component attached to entity %s!", m_pEntity->GetName());
					return false;
				}

				return true;
			}

			virtual void Load(ITexture* pSpecular, ITexture* pDiffuse)
			{
				if (!m_bActive)
				{
					FreeEntitySlot();

					return;
				}

				SRenderLight light;

				light.m_nLightStyle = 0;
				light.SetPosition(ZERO);
				light.m_fLightFrustumAngle = 45;
				light.m_Flags = DLF_POINT | DLF_DEFERRED_CUBEMAPS;
				light.m_LensOpticsFrustumAngle = 255;

				// Extents is radius
				light.m_ProbeExtents = m_extents * 0.5f;

				light.m_fBoxWidth = m_extents.x;
				light.m_fBoxLength = m_extents.y;
				light.m_fBoxHeight = m_extents.z;

				light.m_fRadius = light.m_ProbeExtents.GetLength();

				light.SetLightColor(m_color.m_color * m_color.m_diffuseMultiplier);
				light.SetSpecularMult(m_color.m_specularMultiplier);

				light.m_fHDRDynamic = 0.f;

				if (m_options.m_bAffectsOnlyThisArea)
					light.m_Flags |= DLF_THIS_AREA_ONLY;

				if (m_options.m_bIgnoreVisAreas)
					light.m_Flags |= DLF_IGNORES_VISAREAS;

				if (m_options.m_bVolumetricFogOnly)
					light.m_Flags |= DLF_VOLUMETRIC_FOG_ONLY;

				if (m_options.m_bAffectsVolumetricFog)
					light.m_Flags |= DLF_VOLUMETRIC_FOG;

				light.SetFalloffMax(m_options.m_attenuationFalloffMax);

				if (m_options.m_bBoxProjection)
					light.m_Flags |= DLF_BOX_PROJECTED_CM;

				light.m_fFogRadialLobe = 0.f;

				light.SetAnimSpeed(0.f);
				light.m_fProjectorNearPlane = 0.f;

				light.SetSpecularCubemap(pSpecular);
				light.SetDiffuseCubemap(pDiffuse);

				// Load the light source into the entity
				m_pEntity->LoadLight(GetOrMakeEntitySlotId(), &light);
			}

			virtual void Enable(bool bEnable) { m_bActive = bEnable; }
			bool IsEnabled() const { return m_bActive; }

			virtual void EnableAutomaticLoad(bool bEnable) { m_generation.m_bAutoLoad = bEnable; }
			bool IsAutomaticLoadEnabled() const { return m_generation.m_bAutoLoad; }

			virtual void SetExtents(const Vec3& extents) { m_extents = extents; }
			const Vec3& GetExtents() const { return m_extents; }

			virtual SOptions& GetOptions() { return m_options; }
			const SOptions& GetOptions() const { return m_options; }

			virtual SColor& GetColorParameters() { return m_color; }
			const SColor& GetColorParameters() const { return m_color; }

			virtual void SetOutputCubemapPath(const char* szPath);
			const char* GetOutputCubemapPath() const { return m_generation.m_generatedCubemapPath.value.c_str(); }
			bool HasOutputCubemapPath() const { return m_generation.m_generatedCubemapPath.value.size() > 0; }

		protected:
			bool m_bActive = true;

			Vec3 m_extents = Vec3(10.f);

			SOptions m_options;
			SColor m_color;
			SGeneration m_generation;
		};

#ifdef SUPPORT_ENVIRONMENT_PROBE_GENERATION
		static void ReflectType(Schematyc::CTypeDesc<CEnvironmentProbeComponent::SGeneration::EResolution>& desc)
		{
			desc.SetGUID("204041D6-A198-4DAA-B8B0-4D034DF849BD"_cry_guid);
			desc.SetLabel("Cube map Resolution");
			desc.SetDescription("Resolution to render a cube map at");
			desc.SetDefaultValue(CEnvironmentProbeComponent::SGeneration::EResolution::x256);
			desc.AddConstant(CEnvironmentProbeComponent::SGeneration::EResolution::x256, "Low", "256");
			desc.AddConstant(CEnvironmentProbeComponent::SGeneration::EResolution::x512, "Medium", "512");
			desc.AddConstant(CEnvironmentProbeComponent::SGeneration::EResolution::x1024, "High", "1024");
		}
#endif
	}
}