#include <CryEntitySystem/IEntitySystem.h>

// Load an alembic / geom cache file into the next available entity slot
void LoadGeomCache(IEntity& entity)
{
	// For the sake of this example, load the geometry cache from  <assets folder>/MyGeomCache.cbc
	const char* szGeomCachePath = "MyGeomCache.cbc";
	// Load the alembic definition into the next available slot
	int slotId = entity.LoadGeomCache(-1, szGeomCachePath);
	// Attempt to query the render node, will always succeed if the file was successfully loaded from disk
	if (IGeomCacheRenderNode* pRenderNode = entity.GetGeomCacheRenderNode(slotId))
	{
		/* pRenderNode can now be used to manipulate geom cache playback */
	}
}