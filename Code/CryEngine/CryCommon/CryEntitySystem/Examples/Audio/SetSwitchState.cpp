#include <CryEntitySystem/IEntitySystem.h>

void SetSwitchState(IEntity& entity)
{
	// Get the internal identifier for the switch, computed at compile-time.
	constexpr CryAudio::ControlId switchIdentifier = CryAudio::StringToId("MySwitch");
	// In addition, we need to get the identifier for any state we want to set on the switch as well.
	// Note that the switch and its states are created by the Audio Controls Editor (ACE) tool in the Editor. 
	constexpr CryAudio::SwitchStateId switchStateIdentifier = CryAudio::StringToId("MySwitchState");

	// Query whether an IEntityAudioComponent instance exists in the entity, if not create it.
	if (IEntityAudioComponent* pAudioComponent = entity.GetOrCreateComponent<IEntityAudioComponent>())
	{
		// Now set the new state of the switch, affecting any triggers being executed now or in the future.
		pAudioComponent->SetSwitchState(switchIdentifier, switchStateIdentifier);
	}
}