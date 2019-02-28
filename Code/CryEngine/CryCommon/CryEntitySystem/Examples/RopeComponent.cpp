#include <CryEntitySystem/IEntitySystem.h>
#include <CryPhysics/physinterface.h>
#include <CryRenderer/IRenderer.h>
#include <Cry3DEngine/IRenderNode.h>

void CreateRopeComponent(IEntity& entity)
{
	// Create a new rope component for the entity, automatically creates a rope render node
	IEntityRopeComponent* pRopeComponent = entity.CreateComponent<IEntityRopeComponent>();
	IRopeRenderNode* pRenderNode = pRopeComponent->GetRopeRenderNode();

	// Set the desired rope behavior
	IRopeRenderNode::SRopeParams ropeParams;

	// Create a smoothened rope that responds to collisions from other entities
	ropeParams.nFlags = IRopeRenderNode::eRope_CheckCollisinos | IRopeRenderNode::eRope_Smooth;
	// Specify the thickness of the rope
	ropeParams.fThickness = 0.05f;
	// Determine the number of segments to use on the rope
	ropeParams.nNumSegments = 12;
	// Number of sides / faces to use on the rope
	ropeParams.nNumSides = 4;

	// Number of segments to use for the physical representation of the rope
	ropeParams.nPhysSegments = 12;
	// Total mass of the rope, a value of 0 would result in a static / immovable rope
	ropeParams.mass = 5.0f;

	pRenderNode->SetParams(ropeParams);

	// Now specify the world  space coordinates of the points of our rope
	// For the sake of this example we use two points, but more can be used.
	std::array<Vec3, 2> ropePoints;
	// Start the rope at the entity world position
	ropePoints[0] = entity.GetWorldPos();
	// Give the length of one meter / unit, along the forward direction of the entity
	ropePoints[1] = entity.GetWorldPos() + entity.GetWorldRotation() * Vec3(0, 1, 0);
	// Now set the points on the rope, thus creating the render and physical representation.
	pRenderNode->SetPoints(ropePoints.data(), ropePoints.max_size());
}