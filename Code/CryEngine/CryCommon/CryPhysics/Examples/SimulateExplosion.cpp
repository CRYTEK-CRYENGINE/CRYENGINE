#include <CryPhysics/physinterface.h>

// Example of how an explosion can be simulated in the world, affecting entities near the epicenter
void SimulateExplosion()
{
	// The position at which the explosion epicenter is, in world coordinates
	const Vec3 explosionPosition = ZERO;
	// Radius from the epicenter that the explosion will affect
	const float explosionRadius = 10.f;

	// Populate the pe_explosion structure
	pe_explosion explosion;
	// Set the epicenter and epicenter impulse (identical for the sake of this example)
	explosion.epicenter = explosion.epicenterImp = explosionPosition;
	// Set the radius, we keep it uniform for the sake of this example - but can be made to scale downwards further into the radius
	explosion.rmin = explosion.rmax = explosion.r = explosionRadius;

	// Simulate the explosion
	gEnv->pPhysicalWorld->SimulateExplosion(&explosion);
}