#include <CryEntitySystem/IEntitySystem.h>
#include <CryRenderer/IRenderAuxGeom.h>

class CMyPreviewComponent : public IEntityComponent, public IEntityComponentPreviewer
{
public:
	virtual ~CMyPreviewComponent() = default;
	static void ReflectType(Schematyc::CTypeDesc<CMyPreviewComponent>& desc) { /* Reflect the component GUID in here. */}
	
	// Override the interface function to return the previewer. This is needed by internal systems.
	virtual IEntityComponentPreviewer* GetPreviewer() final { return this; } 

	// Override the serialization function which can be empty.
	virtual void SerializeProperties(Serialization::IArchive& archive) final {}

	// Function which will be called every frame so debug drawing can be executed.
	// As an example this function will draw a bounding box at the origin of this entity.
	virtual void Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const final
	{
		// Create a AABB in this example with a radius.
		AABB aabb(Vec3(0.f, 0.f, 0.f), 10.f);

		// Call the draw function from the AuxGeomRenderer to draw the AABB.
		gEnv->pAuxGeomRenderer->DrawAABB(aabb, m_pEntity->GetWorldTM(), false, Col_Red, EBoundingBoxDrawStyle::eBBD_Faceted);
	}
};