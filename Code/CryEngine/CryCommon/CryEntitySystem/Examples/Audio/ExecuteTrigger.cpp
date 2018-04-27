#include <CryEntitySystem/IEntitySystem.h>

// Simple example of how an audio trigger can be executed using IEntityAudioComponent 
void ExecuteTrigger(IEntity& entity)
{
	// Get the internal identifier for the trigger, computed at compile-time.
	// Note that the trigger itself is created by the Audio Controls Editor (ACE).
	constexpr CryAudio::ControlId triggerIdentifier = CryAudio::StringToId("MyTrigger");
	
	// Query whether an IEntityAudioComponent instance exists in the entity, if not create it.
	if (IEntityAudioComponent* pAudioComponent = entity.GetOrCreateComponent<IEntityAudioComponent>())
	{
		// Now execute the trigger on the entity, playing back anything contained within on the entities current transformation.
		pAudioComponent->ExecuteTrigger(triggerIdentifier);
	}
}