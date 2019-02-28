#include <CryEntitySystem/IEntitySystem.h>

// Load a static geometry (.CGF) file into the next available entity slot
void LoadGeometry(IEntity& entity)
{
	// For the sake of this example, load the static geometry from <assets folder>/Objects/MyBrush.cgf
	const char* szGeometryPath = "Objects/MyBrush.cgf";
	// Load the specified CGF into the next available slot
	const int slotId = entity.LoadGeometry(-1, szGeometryPath);
	// Attempt to query the IStatObj pointer, will always succeed if the file was successfully loaded from disk
	if (IStatObj* pStatObj = entity.GetStatObj(slotId))
	{
		/* 
			pStatObj can now be used to query the rendered geometry
			Note that pStatObj does *not* represent an instance, any modifications will modify all instances.
		*/
	}
}