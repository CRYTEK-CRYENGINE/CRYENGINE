#include <CryEntitySystem/IEntitySystem.h>

// Load a character definition (.CDF) file into the next available entity slot
void LoadCharacter(IEntity& entity)
{
	// For the sake of this example, load the character definition from <assets folder>/Characters/MyCharacter.cdf
	const char* szCharacterPath = "Characters/MyCharacter.cdf";
	// Load the specified CDF into the next available slot
	const int slotId = entity.LoadCharacter(-1, szCharacterPath);
	// Attempt to query the character instance, will always succeed if the file was successfully loaded from disk
	if (ICharacterInstance* pCharacter = entity.GetCharacter(slotId))
	{
		/* pCharacter can now be used to manipulate and query the rendered character */
	}
}