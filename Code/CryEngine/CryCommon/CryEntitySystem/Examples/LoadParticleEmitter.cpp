#include <CryEntitySystem/IEntitySystem.h>
#include <CryParticleSystem/IParticles.h>

// Creates a particle emitter into the specified entity slot that emits continuously emits a specific effect
void LoadParticleEmitter(IEntity& entity)
{
	// For the sake of this example, load the particle effect from <assets folder>/Particles/MyEffect.pfx2
	const char* szParticleEffectPath = "Particles/MyEffect.pfx2";
	if (IParticleEffect* pEffect = gEnv->pParticleManager->FindEffect(szParticleEffectPath, "ParticleLoadExample"))
	{
		// Create a new SpawnParams instance to define how our particles should behave
		SpawnParams spawnParams;
		/* The spawn params could be modified here, but we choose to use the default settings set up by the designer in the Particle Editor */

		// Create a particle emitter in the next available slot, using the specified spawn parameters
		const int slotId = entity.LoadParticleEmitter(-1, pEffect, &spawnParams);
	}
}