#include <CryEntitySystem/IEntitySystem.h>

void SetParameterValue(IEntity& entity)
{
	// Get the internal identifier for the parameter, computed at compile-time.
	// Note that the parameter itself is created by the Audio Controls Editor (ACE).
	constexpr CryAudio::ControlId parameterIdentifier = CryAudio::StringToId("MyParameter");
	// The new value of the parameter that we want to set.
	const float newParameterValue = 5.0f;

	// Query whether an IEntityAudioComponent instance exists in the entity, if not create it.
	if (IEntityAudioComponent* pAudioComponent = entity.GetOrCreateComponent<IEntityAudioComponent>())
	{
		// Now set the new value of the parameter, affecting any triggers being executed now or in the future.
		pAudioComponent->SetParameter(parameterIdentifier, newParameterValue);
	}
}