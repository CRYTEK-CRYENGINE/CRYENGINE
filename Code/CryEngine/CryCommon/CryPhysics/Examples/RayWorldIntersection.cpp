#include <CryEntitySystem/IEntitySystem.h>
#include <CryPhysics/physinterface.h>
#include <CryRenderer/IRenderer.h>
#include <array>

// Example of how we can cast a ray through the physical world and receive hit information
void RayWorldIntersection()
{
	// Store the half render size, used to calculate a direction that points outwards from the camera
	const float halfRenderWidth = static_cast<float>(gEnv->pRenderer->GetWidth()) * 0.5f;
	const float halfRenderHeight = static_cast<float>(gEnv->pRenderer->GetHeight()) * 0.5f;
	
	// For the sake of this example we will cast the ray 50 meters outwards from the camera
	const float searchRange = 50.f;

	// Calculate the centered position of the near and far planes
	Vec3 cameraCenterNear, cameraCenterFar;
	gEnv->pRenderer->UnProjectFromScreen(halfRenderWidth, halfRenderHeight, 0, &cameraCenterNear.x, &cameraCenterNear.y, &cameraCenterNear.z);
	gEnv->pRenderer->UnProjectFromScreen(halfRenderWidth, halfRenderHeight, 1, &cameraCenterFar.x, &cameraCenterFar.y, &cameraCenterFar.z);

	// Now subtract the far and near positions to get a direction, and multiply by the desired range
	const Vec3 searchDirection = (cameraCenterFar - cameraCenterNear).GetNormalized() * searchRange;

	// The result of RayWorldIntersection will be stored in this array
	// For the sake of this example we will limit to only one hit
	std::array<ray_hit, 1> hits;

	// Specify query flags, in this case we will allow any type of entity
	const uint32 queryFlags = ent_all;
	// Specify ray flags, see rwi_flags.
	// Values of 0 - 15 will indicate which surface types the ray will be able to pass through, this depends on the pierceability set in each project's surface types definition.
	// A < check is used, thus lower values are more "durable". For example, pierceability set to 10, and an encountered surface type with pierceability 11 results in the ray passing through. Otherwise, the ray stops at that surface type.
	// A pierceability of 15, or rwi_stop_at_pierceable will mean that the ray does not pierce any surface types.
	// Note that the first ray_hit result is *always* a solid object, thus hits can be reported while the first hit is invalid - this indicates that hits[1] and above were pierced through.
	const uint32 rayFlags = rwi_stop_at_pierceable;

	// Perform the ray intersection, note that this happens on the main thread and can thus be a slow operation
	const int numHits = gEnv->pPhysicalWorld->RayWorldIntersection(cameraCenterNear, searchDirection, queryFlags, rayFlags, hits.data(), hits.max_size());
	if (numHits > 0)
	{
		if (IEntity* pHitEntity = gEnv->pEntitySystem->GetEntityFromPhysics(hits[0].pCollider))
		{
			/* An entity was hit, and we can now use the pHitEntity pointer */
		}
	}
}