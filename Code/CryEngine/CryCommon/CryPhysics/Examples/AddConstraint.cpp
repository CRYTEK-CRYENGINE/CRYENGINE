#include <CryPhysics/physinterface.h>

// Constrains an entity to the specified point
void AddPointConstraint(IPhysicalEntity& physicalEntity)
{
	// Specify the world-space position where the entity will be constrained
	const Vec3 constraintPosition(ZERO);
	// Specify the axis that our constraint will enforce limits on, in our case up.
	const Vec3 constraintAxis(0, 0, 1);

	// Set the minimum rotation angle along the specified axis (in our case, up) to be 0 degrees
	const float minRotationForAxis = 0.f;
	// Set the maximum rotation angle along the specified axis (in our case, up) to be 45 degrees
	const float maxRotationForAxis = DEG2RAD(45);
	// Disallow rotation on the other axes
	const float minRotationOtherAxis = 0.f;
	// Disallow rotation on the other axes
	const float maxRotationOtherAxis = 0.f;

	// Define the constraint
	pe_action_add_constraint constraint;
	// Create a constraint using world coordinates
	constraint.flags = world_frames;
	// Constrain the entity to the specified point
	constraint.pt[0] = constraintPosition;
	// Set the rotational frame for the constraint, based on the specified axis
	constraint.qframe[0] = constraint.qframe[1] = Quat::CreateRotationV0V1(Vec3(1, 0, 0), constraintAxis);
	
	// Now apply the constraint limits defined above
	// Note that if the max<=min (or the limits are left unused), the constraint is treated as a fully free constraint where only position is enforced.
	// To create a fully locked constraint, see the constraint_no_rotation flag.
	constraint.xlimits[0] = minRotationForAxis;
	constraint.xlimits[1] = maxRotationForAxis;
	constraint.yzlimits[0] = minRotationOtherAxis;
	constraint.yzlimits[1] = maxRotationOtherAxis;

	// Constrain the entity around the world, alternatively we can apply another IPhysicalEntity pointer
	constraint.pBuddy = WORLD_ENTITY;

	// Now add the constraint, and store the identifier. This can be used with pe_update_constraint to remove it later on.
	const int constraintId = physicalEntity.Action(&constraint);
}

// Constrains an entity to a line
void AddLineConstraint(IPhysicalEntity& physicalEntity)
{
	// Specify the world-space position where the entity will be constrained
	const Vec3 constraintPosition(ZERO);
	// Specify the axis that movement will be constrained to - as well as being the axis for rotational limits
	const Vec3 constraintAxis(0, 0, 1);

	// Minimum point from the constrained position along the axis that our entity can move to
	const float minimumMovement = 0.f;
	// Maximum point from the constrained position along the axis that our entity can move to
	const float maximumMovement = 1.f;

	// Define the constraint
	pe_action_add_constraint constraint;
	// Create a constraint using world coordinates
	constraint.flags = world_frames | constraint_line;
	// Constrain the entity to the specified point
	constraint.pt[0] = constraintPosition;
	// Set the rotational frame for the constraint, based on the specified axis
	constraint.qframe[0] = constraint.qframe[1] = Quat::CreateRotationV0V1(Vec3(1, 0, 0), constraintAxis);
	constraint.xlimits[0] = minimumMovement;
	constraint.xlimits[1] = maximumMovement;

	// Constrain the entity around the world, alternatively we can apply another IPhysicalEntity pointer
	constraint.pBuddy = WORLD_ENTITY;

	// Now add the constraint, and store the identifier. This can be used with pe_update_constraint to remove it later on.
	const int constraintId = physicalEntity.Action(&constraint);
}

// Constrains an entity to a plane
void AddPlaneConstraint(IPhysicalEntity& physicalEntity)
{
	// Specify the world-space position where the entity will be constrained
	const Vec3 constraintPosition(ZERO);
	// Specify the axis to which movement will be constrained (onto a virtual plane) - as well as being the axis for rotational limits
	const Vec3 constraintAxis(0, 0, 1);

	// Minimum point from the constrained position along the X axis that our entity can move to
	const float minimumMovementX = 0.f;
	// Maximum point from the constrained position along the X axis that our entity can move to
	const float maximumMovementX = 1.f;
	// Minimum point from the constrained position along the Y axis that our entity can move to
	const float minimumMovementY = 0.f;
	// Maximum point from the constrained position along the Y axis that our entity can move to
	const float maximumMovementY = 1.f;

	// Define the constraint
	pe_action_add_constraint constraint;
	// Create a constraint using world coordinates
	constraint.flags = world_frames | constraint_plane;
	// Constrain the entity to the specified point
	constraint.pt[0] = constraintPosition;
	// Set the rotational frame for the constraint, based on the specified axis
	constraint.qframe[0] = constraint.qframe[1] = Quat::CreateRotationV0V1(Vec3(1, 0, 0), constraintAxis);
	constraint.xlimits[0] = minimumMovementX;
	constraint.xlimits[1] = maximumMovementX;
	constraint.xlimits[0] = minimumMovementY;
	constraint.xlimits[1] = maximumMovementY;

	// Constrain the entity around the world, alternatively we can apply another IPhysicalEntity pointer
	constraint.pBuddy = WORLD_ENTITY;

	// Now add the constraint, and store the identifier. This can be used with pe_update_constraint to remove it later on.
	const int constraintId = physicalEntity.Action(&constraint);
}