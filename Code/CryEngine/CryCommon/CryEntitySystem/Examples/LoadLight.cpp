#include <CryEntitySystem/IEntitySystem.h>
#include <CryRenderer/IRenderer.h>

// Loads a dynamic point light into the next available entity slot
void LoadPointLight(IEntity& entity)
{
	// Create the CDLight structure
	SRenderLight light;
	// Create a deferred point light that only affects the current area
	light.m_Flags = DLF_DEFERRED_LIGHT | DLF_THIS_AREA_ONLY | DLF_POINT;
	// Emit in a 10 meter radius
	light.m_fRadius = 10.f;
	// Set the color to pure white (RGBA), on a scale of 0 - 1
	light.SetLightColor(ColorF(1.f, 1.f, 1.f, 1.f));

	// Load the light into the next available entity slot
	const int lightSlot = entity.LoadLight(-1, &light);
}

// Loads a dynamic projector light into the next available entity slot
void LoadProjectorLight(IEntity& entity)
{
	// Create the CDLight structure
	SRenderLight light;
	// Create a deferred projector light that only affects the current area
	light.m_Flags = DLF_DEFERRED_LIGHT | DLF_THIS_AREA_ONLY | DLF_PROJECT;
	// Emit at a distance of 10 meters
	light.m_fRadius = 10.f;
	// Set the color to pure white (RGBA), on a scale of 0 - 1
	light.SetLightColor(ColorF(1.f, 1.f, 1.f, 1.f));

	// Load a sample projector texture from the engine assets (included with all projects)
	const char* szProjectedTexturePath = "%ENGINEASSETS%/Textures/Lights/ce_logo_white.dds";
	// Load the texture we want to project onto other objects
	light.m_pLightImage = gEnv->pRenderer->EF_LoadTexture(szProjectedTexturePath, 0);
	// Project in a 90-degree cone
	light.m_fLightFrustumAngle = 90;

	// Load the light into the next available entity slot
	const int lightSlot = entity.LoadLight(-1, &light);
}