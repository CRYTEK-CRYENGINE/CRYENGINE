#include <CryEntitySystem/IEntitySystem.h>
#include <Cry3DEngine/I3DEngine.h>

void LoadFogVolume(IEntity& entity)
{
	// Create a SFogVolumeProperties instance to define the fog volume characteristics
	SFogVolumeProperties fogProperties;
	// Render the volume as a box
	fogProperties.m_volumeType = IFogVolumeRenderNode::eFogVolumeType_Box;
	// Render as a 1x1x1m box
	fogProperties.m_size = Vec3(1, 1, 1);
	// Set color to red (range is 0 - 1)
	fogProperties.m_color = ColorF(1, 0, 0, 1);

	// Helpers to convert emission intensity to the engine's format
	const float kiloScale = 1000.0f;
	const float toLightUnitScale = kiloScale / RENDERER_LIGHT_UNIT_SCALE;

	// Specify the desired emitted color, in our case red.
	ColorF emission = ColorF(1, 0, 0, 1);
	// Scale the intensity of the emission
	const float emissionIntensity = 1.f;
	emission.adjust_luminance(emissionIntensity * toLightUnitScale);

	// Apply the calculated emission to the fog properties
	fogProperties.m_emission = emission.toVec3();

	// Load the fog volume into the next available slot
	const int loadedSlotId = entity.LoadFogVolume(-1, fogProperties);
}