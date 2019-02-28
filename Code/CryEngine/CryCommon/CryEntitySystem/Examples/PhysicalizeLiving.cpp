#include <CryEntitySystem/IEntitySystem.h>
#include <CryPhysics/physinterface.h>

// Example for how a living (walking) character can be physicalized
void PhysicalizeLiving(IEntity& entity)
{
	SEntityPhysicalizeParams physParams;
	// Set the physics type to PE_LIVING
	physParams.type = PE_LIVING;
	// Set the mass to 90 kilograms
	physParams.mass = 90;

	// Living entities have to set the SEntityPhysicalizeParams::pPlayerDimensions field
	pe_player_dimensions playerDimensions;
	// Prefer usage of a cylinder instead of capsule
	playerDimensions.bUseCapsule = 0;
	// Specify the size of our cylinder
	playerDimensions.sizeCollider = Vec3(0.3f, 0.3f, 0.935f * 0.5f);
	// Keep pivot at the player's feet (defined in player geometry) 
	playerDimensions.heightPivot = 0.f;
	// Offset collider upwards
	playerDimensions.heightCollider = 1.f;
	physParams.pPlayerDimensions = &playerDimensions;

	// Living entities have to set the SEntityPhysicalizeParams::pPlayerDynamics field
	pe_player_dynamics playerDynamics;
	// Mass needs to be repeated in the pe_player_dynamics structure
	playerDynamics.mass = physParams.mass;
	physParams.pPlayerDynamics = &playerDynamics;

	// Now physicalize the entity
	entity.Physicalize(physParams);
}

// Example for how a living (walking) character's move direction can be modified
void MoveLiving(IPhysicalEntity& physicalEntity)
{
	pe_action_move moveAction;
	// Apply movement request directly to velocity
	moveAction.iJump = 2;
	moveAction.dir = Vec3(0, 1, 0);

	physicalEntity.Action(&moveAction);
}