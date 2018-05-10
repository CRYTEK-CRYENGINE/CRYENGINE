// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifdef CVAR_CPP

	#undef REGISTER_CVAR_AUTO_STRING
	#define REGISTER_CVAR_AUTO_STRING(_name, _def_val, _flags, _comment) \
	  _name = REGISTER_STRING(( # _name), _def_val, _flags, _comment)

	#undef REGISTER_CVAR_AUTO
	#define REGISTER_CVAR_AUTO(_type, _var, _def_val, _flags, _comment) \
	  m_arrVars.Add(REGISTER_CVAR(_var, _def_val, _flags, _comment))

PodArray<ICVar*> m_arrVars;

#else

	#undef REGISTER_CVAR_AUTO_STRING
	#define REGISTER_CVAR_AUTO_STRING(_name, _def_val, _flags, _comment) \
	  ICVar * _name;

	#undef REGISTER_CVAR_AUTO
	#define REGISTER_CVAR_AUTO(_type, _var, _def_val, _flags, _comment) \
	  _type _var;

#endif

REGISTER_CVAR_AUTO(int, e_svoLoadTree, 0, VF_NULL, "Start SVO generation or loading from disk");
REGISTER_CVAR_AUTO(int, e_svoDispatchX, 64, VF_NULL, "Controls parameters of SVO compute shaders execution");
REGISTER_CVAR_AUTO(int, e_svoDispatchY, SVO_BRICK_ALLOC_CHUNK_SIZE, VF_NULL, "Controls parameters of SVO compute shaders execution");
REGISTER_CVAR_AUTO(int, e_svoDVR, 0, VF_NULL, "Activate Direct Volume Rendering of SVO (trace and output results to the screen)");
REGISTER_CVAR_AUTO(float, e_svoDVR_DistRatio, 0, VF_NULL, "Controls voxels LOD ratio for streaming and tracing");
REGISTER_CVAR_AUTO(int, e_svoEnabled, 0, VF_NULL, "Activates SVO subsystem");
REGISTER_CVAR_AUTO(int, e_svoDebug, 0, VF_NULL, "6 = Visualize voxels, different colors shows different LOD\n7 = Visualize postponed nodes and not ready meshes");
REGISTER_CVAR_AUTO(int, e_svoMaxBricksOnCPU, /*800*/ 1024 * 8, VF_NULL, "Maximum number of voxel bricks allowed to cache on CPU side");
REGISTER_CVAR_AUTO(int, e_svoMaxBrickUpdates, 8, VF_NULL, "Limits the number of bricks uploaded from CPU to GPU per frame");
REGISTER_CVAR_AUTO(int, e_svoMaxStreamRequests, 256, VF_NULL, "Limits the number of brick streaming or building requests per frame");
REGISTER_CVAR_AUTO(float, e_svoMaxNodeSize, 32, VF_NULL, "Maximum SVO node size for voxelization (bigger nodes stays empty)");
REGISTER_CVAR_AUTO(float, e_svoMaxAreaSize, 32, VF_NULL, "Maximum SVO node size for detailed voxelization");
REGISTER_CVAR_AUTO(int, e_svoMaxAreaMeshSizeKB, 8000, VF_NULL, "Maximum number of KB per area allowed to allocate for voxelization mesh");
REGISTER_CVAR_AUTO(int, e_svoRender, 1, VF_NULL, "Enables CPU side (every frame) SVO traversing and update");
REGISTER_CVAR_AUTO(int, e_svoTI_ResScaleBase, 2, VF_NULL, "Defines resolution of GI cone-tracing targets; 2=half res");
REGISTER_CVAR_AUTO(int, e_svoTI_ResScaleAir, 4, VF_NULL, "Defines resolution of GI cone-tracing targets; 2=half res");
REGISTER_CVAR_AUTO(int, e_svoTI_DualTracing, 2, VF_NULL, "Double the number of rays per fragment\n1 = Always ON; 2 = Active only in multi-GPU mode");
REGISTER_CVAR_AUTO(float, e_svoTI_PointLightsMultiplier, 1.f, VF_NULL, "Modulates point light injection (controls the intensity of bounce light)");
REGISTER_CVAR_AUTO(float, e_svoTI_EmissiveMultiplier, 4.f, VF_NULL, "Modulates emissive materials light injection\nAllows controlling emission separately from post process glow");
REGISTER_CVAR_AUTO(float, e_svoTI_VegetationMaxOpacity, .18f, VF_NULL, "Limits the opacity of vegetation voxels");
REGISTER_CVAR_AUTO(int, e_svoTI_VoxelizationPostpone, 2, VF_NULL, "1 - Postpone voxelization until all needed meshes are streamed in\n2 - Postpone voxelization and request streaming of missing meshes\nUse e_svoDebug = 7 to visualize postponed nodes and not ready meshes");
REGISTER_CVAR_AUTO(float, e_svoTI_MinVoxelOpacity, 0.1f, VF_NULL, "Voxelize only geometry with opacity higher than specified value");
REGISTER_CVAR_AUTO(float, e_svoTI_MinReflectance, 0.2f, VF_NULL, "Controls light bouncing from very dark surfaces (and from surfaces missing in RSM)");
REGISTER_CVAR_AUTO(int, e_svoTI_ObjectsLod, 1, VF_NULL, "Mesh LOD used for voxelization\nChanges are visible only after re-voxelization (click <Update geometry> or restart)");
REGISTER_CVAR_AUTO(float, e_svoTI_AnalyticalOccludersRange, 4.f, VF_NULL, "Shadow length");
REGISTER_CVAR_AUTO(float, e_svoTI_AnalyticalOccludersSoftness, 0.5f, VF_NULL, "Shadow softness");
REGISTER_CVAR_AUTO(int, e_svoRootless, !gEnv->IsEditor(), VF_NULL, "Limits the area covered by SVO. Limits number of tree levels and speedup the tracing.");
REGISTER_CVAR_AUTO(float, e_svoTI_DistantSsaoAmount, 0.5f, VF_NULL, "Large scale SSAO intensity in the distance");
REGISTER_CVAR_AUTO(float, e_svoTI_PointLightsMaxDistance, 20.f, VF_NULL, "Maximum distance at which point lights produce light bounces");
REGISTER_CVAR_AUTO(float, e_svoTI_MaxSyncUpdateTime, 2.f, VF_NULL, "Limit the time (in seconds) allowed for synchronous voxelization (usually happens on level start)");

#ifdef CVAR_CPP
m_arrVars.Clear();
#endif

// UI parameters
#if CRY_PLATFORM_DESKTOP
REGISTER_CVAR_AUTO(int, e_svoTI_Active, 0, VF_NULL, "Activates voxel GI for the level"); // default value of 0 allows GI to be activated in level settings
#else
REGISTER_CVAR_AUTO(int, e_svoTI_Active, -1, VF_NULL, "Activates voxel GI for the level"); // default value of -1 forces disabling of GI on weak platforms
#endif

REGISTER_CVAR_AUTO(int, e_svoTI_IntegrationMode, 0, VF_EXPERIMENTAL,
                   "GI computations may be used in several ways:\n"
                   "0 = Basic diffuse GI mode\n"
                   "      GI completely replaces default diffuse ambient lighting\n"
                   "      Single light bounce (fully real-time) is supported for sun and projectors (use '_TI_DYN' in light name)\n"
                   "      Default ambient specular (usually coming from env probes) is modulated by intensity of diffuse GI\n"
                   "      This mode takes less memory (only opacity is voxelized) and works acceptable on consoles\n"
                   "      Optionally this mode may be converted into low cost AO-only mode: set InjectionMultiplier=0 and UseLightProbes=true\n"
                   "1 = Advanced diffuse GI mode (experimental)\n"
                   "      GI completely replaces default diffuse ambient lighting\n"
                   "      Two indirect light bounces are supported for sun and semi-static lights (use '_TI' in light name)\n"
                   "      Single fully dynamic light bounce is supported for projectors (use '_TI_DYN' in light name)\n"
                   "      Default ambient specular is modulated by intensity of diffuse GI\n"
                   "      Please perform scene re-voxelization if IntegrationMode was changed (use UpdateGeometry)\n"
                   "2 = Full GI mode (very experimental)\n"
                   "      Both ambient diffuse and ambient specular lighting is computed by voxel cone tracing\n"
                   "      This mode works fine only on good modern PC");
REGISTER_CVAR_AUTO(float, e_svoTI_InjectionMultiplier, 0, VF_NULL,
                   "Modulates light injection (controls the intensity of bounce light)");
REGISTER_CVAR_AUTO(float, e_svoTI_SkyColorMultiplier, 0, VF_NULL,
                   "Controls amount of the sky light\nThis value may be multiplied with TOD fog color");
REGISTER_CVAR_AUTO(float, e_svoTI_UseTODSkyColor, 0, VF_NULL,
                   "if non 0 - modulate sky light with TOD fog color (top)\nValues between 0 and 1 controls the color saturation");
REGISTER_CVAR_AUTO(float, e_svoTI_PortalsDeform, 0, VF_EXPERIMENTAL,
                   "Adjusts the sky light tracing kernel so that more rays are cast in direction of portals\nThis helps getting more detailed sky light indoor but may cause distortion of all other indirect lighting");
REGISTER_CVAR_AUTO(float, e_svoTI_PortalsInject, 0, VF_EXPERIMENTAL,
                   "Inject portal lighting into SVO\nThis helps getting better indoor sky light even with single bounce");
REGISTER_CVAR_AUTO(float, e_svoTI_DiffuseAmplifier, 0, VF_EXPERIMENTAL,
                   "Adjusts the output brightness of cone traced indirect diffuse component");
REGISTER_CVAR_AUTO(int, e_svoTI_NumberOfBounces, 0, VF_EXPERIMENTAL,
                   "Maximum number of indirect bounces (from 0 to 2)\nFirst indirect bounce is completely dynamic\nThe rest of the bounces are cached in SVO and mostly static");
REGISTER_CVAR_AUTO(float, e_svoTI_Saturation, 0, VF_NULL,
                   "Controls color saturation of propagated light");
REGISTER_CVAR_AUTO(float, e_svoTI_PropagationBooster, 0, VF_EXPERIMENTAL,
                   "Controls fading of the light during in-SVO propagation\nValues greater than 1 help propagating light further but may bring more light leaking artifacts");
REGISTER_CVAR_AUTO(float, e_svoTI_DiffuseBias, 0, VF_NULL,
                   "Constant ambient value added to GI\nHelps preventing completely black areas\nIf negative - modulate ambient with near range AO (prevents constant ambient in completely occluded indoor areas)");
REGISTER_CVAR_AUTO(float, e_svoTI_PointLightsBias, 0.2f, VF_EXPERIMENTAL,
                   "Modulates non shadowed injection from point light (helps simulating multiple bounces)");
REGISTER_CVAR_AUTO(float, e_svoTI_DiffuseConeWidth, 0, VF_NULL,
                   "Controls wideness of diffuse cones\nWider cones work faster but may cause over-occlusion and more light leaking\nNarrow cones are slower and may bring more noise");
REGISTER_CVAR_AUTO(float, e_svoTI_ConeMaxLength, 0, VF_NULL,
                   "Maximum length of the tracing rays (in meters)\nShorter rays work faster");
REGISTER_CVAR_AUTO(float, e_svoTI_SpecularAmplifier, 0, VF_NULL,
                   "Adjusts the output brightness of specular component");
REGISTER_CVAR_AUTO(int, e_svoTI_UpdateLighting, 0, VF_EXPERIMENTAL,
                   "When switched from OFF to ON - forces single full update of cached lighting\nIf stays enabled - automatically updates lighting if some light source was modified");
REGISTER_CVAR_AUTO(int, e_svoTI_UpdateGeometry, 0, VF_NULL,
                   "When switched from OFF to ON - forces single complete re-voxelization of the scene\nThis is needed if terrain, brushes or vegetation were modified");
REGISTER_CVAR_AUTO(float, e_svoMinNodeSize, 0, VF_NULL,
                   "Smallest SVO node allowed to create during level voxelization\nSmaller values helps getting more detailed lighting but may work slower and use more memory in pool\nIt may be necessary to increase VoxelPoolResolution in order to prevent running out of voxel pool");
REGISTER_CVAR_AUTO(int, e_svoTI_SkipNonGILights, 0, VF_NULL,
                   "Disable all lights except sun and lights marked to be used for GI\nThis mode ignores all local environment probes and ambient lights");
REGISTER_CVAR_AUTO(int, e_svoTI_LowSpecMode, 0, VF_NULL,
                   "Set low spec mode\nValues greater than 0 scale down internal render targets and simplify shaders\nIf set to -2 it will be initialized by value specified in sys_spec_Shading.cfg (on level load or on spec change)");
REGISTER_CVAR_AUTO(int, e_svoTI_HalfresKernelPrimary, 0, VF_NULL,
                   "Use less rays for first bounce and AO\nThis gives faster frame rate and sharper lighting but may increase noise and GI aliasing");
REGISTER_CVAR_AUTO(int, e_svoTI_HalfresKernelSecondary, 0, VF_EXPERIMENTAL,
                   "Use less rays for secondary bounce\nThis gives faster update of cached lighting but may reduce the precision of secondary bounce\nDifference is only visible with number of bounces more than 1");
REGISTER_CVAR_AUTO(int, e_svoTI_UseLightProbes, 0, VF_NULL,
                   "If enabled - environment probes lighting is multiplied with GI\nIf disabled - diffuse contribution of environment probes is replaced with GI\nIn modes 1-2 it enables usage of global env probe for sky light instead of TOD fog color");
REGISTER_CVAR_AUTO(float, e_svoTI_VoxelizationLODRatio, 0, VF_NULL,
                   "Controls distance LOD ratio for voxelization\nBigger value helps getting more detailed lighting at distance but may work slower and will use more memory in pool\nIt may be necessary to increase VoxelPoolResolution parameter in order to prevent running out of voxel pool");
REGISTER_CVAR_AUTO(int, e_svoTI_VoxelizationMapBorder, 0.f, VF_EXPERIMENTAL,
                   "Skip voxelization of geometry close to the edges of the map\nIn case of offline voxelization this will speedup the export process and reduce data size on disk.");
REGISTER_CVAR_AUTO(int, e_svoVoxelPoolResolution, 0, VF_NULL,
                   "Size of volume textures (x,y,z dimensions) used for SVO data storage\nValid values are 128 and 256\nEngine has to be restarted if this value was modified\nToo big pool size may cause long stalls when some GI parameter was changed");
REGISTER_CVAR_AUTO(float, e_svoTI_SSAOAmount, 0, VF_NULL,
                   "Allows to scale down SSAO (SSDO) amount and radius when GI is active");
REGISTER_CVAR_AUTO(float, e_svoTI_ObjectsMaxViewDistance, 0, VF_NULL,
                   "Voxelize only objects with maximum view distance greater than this value (only big and important objects)\nIf set to 0 - disable this check and also disable skipping of too small triangles\nChanges are visible after full re-voxelization (click <Update geometry> or restart)");
REGISTER_CVAR_AUTO(float, e_svoTI_ObjectsMaxViewDistanceScale, 1, VF_NULL,
                   "Scales e_svoTI_ObjectsMaxViewDistance, so it can be different on consoles.");
REGISTER_CVAR_AUTO(int, e_svoTI_SunRSMInject, 0, VF_EXPERIMENTAL,
                   "Enable additional RSM sun injection\nHelps getting sun bounces in over-occluded areas where primary injection methods are not able to inject enough sun light");
REGISTER_CVAR_AUTO(float, e_svoTI_SSDepthTrace, 0, VF_EXPERIMENTAL,
                   "Use SS depth tracing together with voxel tracing");
REGISTER_CVAR_AUTO(int, e_svoTI_AnalyticalOccluders, 0, VF_NULL,
                   "Enable basic support for hand-placed occlusion shapes like box, cylinder and capsule\nThis also enables indirect shadows from characters (shadow capsules are defined in .chrparams file)");
REGISTER_CVAR_AUTO(int, e_svoTI_AnalyticalGI, 0, VF_EXPERIMENTAL,
                   "Completely replace voxel tracing with analytical shapes tracing\nLight bouncing is supported only in integration mode 0");
REGISTER_CVAR_AUTO(int, e_svoTI_TraceVoxels, 1, VF_EXPERIMENTAL,
                   "Include voxels into tracing\nAllows to exclude voxel tracing if only proxies are needed");
REGISTER_CVAR_AUTO(float, e_svoTI_TranslucentBrightness, 0, VF_NULL,
                   "Adjusts the brightness of semi translucent surfaces\nAffects mostly vegetation leaves and grass");
REGISTER_CVAR_AUTO(int, e_svoStreamVoxels, 0, VF_EXPERIMENTAL,
                   "Enable steaming of voxel data from disk instead of run-time voxelization\n"
                   "Steaming used only in launcher, in the editor voxels are always run-time generated\n"
                   "If enabled, level export will include voxels pre-computation process which may take up to one hour for big complex levels");

// dump cvars for UI help
#ifdef CVAR_CPP
	#ifdef DUMP_UI_PARAMETERS
{
	FILE* pFile = 0;
	fopen_s(&pFile, "cvars_dump.txt", "wt");
	for (int i = 0; i < m_arrVars.Count(); i++)
	{
		ICVar* pCVar = m_arrVars[i];

		if (pCVar->GetFlags() & VF_EXPERIMENTAL)
			continue;

		int offset = strstr(pCVar->GetName(), "e_svoTI_") ? 8 : 5;

		#define IsLowerCC(c) (c >= 'a' && c <= 'z')
		#define IsUpperCC(c) (c >= 'A' && c <= 'Z')

		string sText = pCVar->GetName() + offset;

		for (int i = 1; i < sText.length() - 1; i++)
		{
			// insert spaces between words
			if ((IsLowerCC(sText[i - 1]) && IsUpperCC(sText[i])) || (IsLowerCC(sText[i + 1]) && IsUpperCC(sText[i - 1]) && IsUpperCC(sText[i])))
				sText.insert(i++, ' ');

			// convert single upper cases letters to lower case
			if (IsUpperCC(sText[i]) && IsLowerCC(sText[i + 1]))
			{
				unsigned char cNew = (sText[i] + ('a' - 'A'));
				sText.replace(i, 1, 1, cNew);
			}
		}

		fputs(sText.c_str(), pFile);
		fputs("\n", pFile);

		sText = pCVar->GetHelp();
		sText.insert(0, "\t");
		sText.replace("\n", "\n\t");

		fputs(sText.c_str(), pFile);
		fputs("\n", pFile);
		fputs("\n", pFile);
	}
	fclose(pFile);
}
	#endif // DUMP_UI_PARAMETERS
#endif   // CVAR_CPP

REGISTER_CVAR_AUTO(float, e_svoTI_VoxelOpacityMultiplier, 1, VF_NULL, "Allows making voxels more opaque, helps reducing light leaks");
REGISTER_CVAR_AUTO(float, e_svoTI_SkyLightBottomMultiplier, 0, VF_NULL, "Modulates sky light coming from the bottom");
REGISTER_CVAR_AUTO(int, e_svoTI_Apply, 0, VF_NULL, "Allows to temporary deactivate GI for debug purposes");
REGISTER_CVAR_AUTO(float, e_svoTI_Diffuse_Spr, 0, VF_NULL, "Adjusts the kernel of diffuse tracing; big value will merge all cones into single vector");
REGISTER_CVAR_AUTO(int, e_svoTI_Diffuse_Cache, 0, VF_NULL, "Pre-bake lighting in SVO and use it instead of cone tracing");
REGISTER_CVAR_AUTO(int, e_svoTI_SpecularFromDiffuse, 0, VF_EXPERIMENTAL, "Compute simplified specular lighting using intermediate results of diffuse SVO tracing\nIn this mode environment probes are not used. It works fine for materials with low smoothness but works wrong for mirrors");
REGISTER_CVAR_AUTO(int, e_svoTI_DynLights, 1, VF_NULL, "Allow single real-time indirect bounce from marked dynamic lights");
REGISTER_CVAR_AUTO(int, e_svoTI_ForceGIForAllLights, 0, VF_NULL, "Force dynamic GI for all lights except ambient lights and sun\nThis allows to quickly get dynamic GI working in unprepared scenes");
REGISTER_CVAR_AUTO(float, e_svoTI_ConstantAmbientDebug, 0, VF_NULL, "Replace GI computations with constant ambient color for GI debugging");
REGISTER_CVAR_AUTO(int, e_svoTI_ShadowsFromSun, 0, VF_EXPERIMENTAL, "Calculate sun shadows using SVO ray tracing\nNormally supposed to be used in combination with normal shadow maps and screen space shadows");
REGISTER_CVAR_AUTO(float, e_svoTI_ShadowsSoftness, 1.25f, VF_EXPERIMENTAL, "Controls softness of ray traced shadows");
REGISTER_CVAR_AUTO(int, e_svoTI_ShadowsFromHeightmap, 0, VF_EXPERIMENTAL, "Include terrain heightmap (whole level) into ray-traced sun shadows");
REGISTER_CVAR_AUTO(int, e_svoTI_Troposphere_Active, 0, VF_EXPERIMENTAL, "Activates SVO atmospheric effects (completely replaces default fog computations)\nIt is necessary to re-voxelize the scene after activation");
REGISTER_CVAR_AUTO(float, e_svoTI_Troposphere_Brightness, 0, VF_EXPERIMENTAL, "Controls intensity of atmospheric effects.");
REGISTER_CVAR_AUTO(float, e_svoTI_Troposphere_Ground_Height, 0, VF_EXPERIMENTAL, "Minimum height for atmospheric effects");
REGISTER_CVAR_AUTO(float, e_svoTI_Troposphere_Layer0_Height, 0, VF_EXPERIMENTAL, "Layered fog level");
REGISTER_CVAR_AUTO(float, e_svoTI_Troposphere_Layer1_Height, 0, VF_EXPERIMENTAL, "Layered fog level");
REGISTER_CVAR_AUTO(float, e_svoTI_Troposphere_Snow_Height, 0, VF_EXPERIMENTAL, "Snow on objects height");
REGISTER_CVAR_AUTO(float, e_svoTI_Troposphere_Layer0_Rand, 0, VF_EXPERIMENTAL, "Layered fog randomness");
REGISTER_CVAR_AUTO(float, e_svoTI_Troposphere_Layer1_Rand, 0, VF_EXPERIMENTAL, "Layered fog randomness");
REGISTER_CVAR_AUTO(float, e_svoTI_Troposphere_Layer0_Dens, 0, VF_EXPERIMENTAL, "Layered fog Density");
REGISTER_CVAR_AUTO(float, e_svoTI_Troposphere_Layer1_Dens, 0, VF_EXPERIMENTAL, "Layered fog Density");
REGISTER_CVAR_AUTO(float, e_svoTI_Troposphere_CloudGen_Height, 1000.f, VF_EXPERIMENTAL, "Maximum height for atmospheric effects");
REGISTER_CVAR_AUTO(float, e_svoTI_Troposphere_CloudGen_Freq, 0, VF_EXPERIMENTAL, "CloudsGen magic number");
REGISTER_CVAR_AUTO(float, e_svoTI_Troposphere_CloudGen_FreqStep, 0, VF_EXPERIMENTAL, "CloudsGen magic number");
REGISTER_CVAR_AUTO(float, e_svoTI_Troposphere_CloudGen_Scale, 0, VF_EXPERIMENTAL, "CloudsGen magic number");
REGISTER_CVAR_AUTO(float, e_svoTI_Troposphere_CloudGenTurb_Freq, 0, VF_EXPERIMENTAL, "CloudsGen magic number");
REGISTER_CVAR_AUTO(float, e_svoTI_Troposphere_CloudGenTurb_Scale, 0, VF_EXPERIMENTAL, "CloudsGen magic number");
REGISTER_CVAR_AUTO(float, e_svoTI_Troposphere_Density, 0, VF_EXPERIMENTAL, "Density of the atmosphere");
REGISTER_CVAR_AUTO(int, e_svoTI_Troposphere_Subdivide, 0, VF_EXPERIMENTAL, "Build detailed SVO also in areas not filled by geometry");
REGISTER_CVAR_AUTO(float, e_svoTI_RsmConeMaxLength, 12.f, VF_NULL, "Maximum length of the RSM rays (in meters)\nShorter rays work faster");
#if !CRY_PLATFORM_CONSOLE
REGISTER_CVAR_AUTO(int, e_svoTI_RsmUseColors, 1, VF_NULL, "Render also albedo colors and normals during RSM generation and use it in cone tracing");
#else
REGISTER_CVAR_AUTO(int, e_svoTI_RsmUseColors, 0, VF_NULL, "Render also albedo colors and normals during RSM generation and use it in cone tracing");
#endif
REGISTER_CVAR_AUTO(int, e_svoTI_Reflect_Vox_Max, 100, VF_NULL, "Controls amount of voxels allowed to refresh every frame");
REGISTER_CVAR_AUTO(int, e_svoTI_Reflect_Vox_MaxEdit, 10000, VF_NULL, "Controls amount of voxels allowed to refresh every frame during lights editing");
REGISTER_CVAR_AUTO(int, e_svoTI_Reflect_Vox_Max_Overhead, 50, VF_NULL, "Controls amount of voxels allowed to refresh every frame");
REGISTER_CVAR_AUTO(float, e_svoTI_RT_MaxDist, 0, VF_NULL, "Maximum distance for detailed mesh ray tracing prototype; applied only in case of maximum glossiness");
REGISTER_CVAR_AUTO(float, e_svoTI_Specular_Sev, 1, VF_NULL, "Controls severity of specular cones; this value limits the material glossiness");
REGISTER_CVAR_AUTO(float, e_svoVoxDistRatio, 14.f, VF_NULL, "Limits the distance where real-time GPU voxelization used");
REGISTER_CVAR_AUTO(int, e_svoVoxGenRes, 512, VF_NULL, "GPU voxelization dummy render target resolution");
REGISTER_CVAR_AUTO(float, e_svoVoxNodeRatio, 4.f, VF_NULL, "Limits the real-time GPU voxelization only to leaf SVO nodes");
REGISTER_CVAR_AUTO(int, e_svoTI_GsmCascadeLod, 2, VF_NULL, "Sun shadow cascade LOD for RSM GI");
REGISTER_CVAR_AUTO(float, e_svoTI_TemporalFilteringBase, .25f, VF_NULL, "Controls amount of temporal smoothing\n0 = less noise and aliasing, 1 = less ghosting");
REGISTER_CVAR_AUTO(float, e_svoTI_HighGlossOcclusion, 0.f, VF_NULL, "Normally specular contribution of env probes is corrected by diffuse GI\nThis parameter controls amount of correction (usually darkening) for very glossy and reflective surfaces");
REGISTER_CVAR_AUTO(int, e_svoTI_VoxelizeUnderTerrain, 0, VF_NULL, "0 = Skip underground triangles during voxelization");
REGISTER_CVAR_AUTO(int, e_svoTI_VoxelizeHiddenObjects, 0, VF_NULL, "0 = Skip hidden objects during voxelization");
REGISTER_CVAR_AUTO(int, e_svoTI_AsyncCompute, 0, VF_NULL, "Use asynchronous compute for SVO updates");

#if defined (CRY_PLATFORM_CONSOLE) || defined (CRY_PLATFORM_MOBILE)
const int svoTI_numStreamingThreads = 1;
#else
const int svoTI_numStreamingThreads = 2;
#endif

REGISTER_CVAR_AUTO(int, e_svoTI_NumStreamingThreads, svoTI_numStreamingThreads, VF_NULL, "Set number of voxelization data streaming threads");
