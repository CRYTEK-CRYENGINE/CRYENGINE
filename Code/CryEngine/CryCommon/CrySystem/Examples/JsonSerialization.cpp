#include <CrySystem/ISystem.h>
#include <CrySerialization/IArchiveHost.h>

// Expose an enumeration that can be saved to disk
enum class EMyEnum
{
	Value,
};

// Reflect the enum values to the serialization library
// This is required in order to serialize custom enums
YASLI_ENUM_BEGIN(EMyEnum, "MyEnum")
YASLI_ENUM(EMyEnum::Value , "Value", "Value")
YASLI_ENUM_END()

// Example of how an XML structure can be created programmatically and saved to disk
void SerializeJson()
{
	// Create a native representation of the JSON we are parsing
	struct SNativeJsonRepresentation
	{
		string myString;
		EMyEnum myEnum;
		std::vector<int> myVector;

		// The Serialize function is required, and will be called when reading from the JSON
		void Serialize(Serialization::IArchive& ar)
		{
			// Serialize the string under the name "myString"
			// The second argument is the label, used for UI serialization
			ar(myString, "myString", "My String");
			// Serialize our enum
			ar(myEnum, "myEnum", "My Enum");
			// Serialize the vector
			ar(myVector, "myVector", "My Vector");
		}
	};

	// The example JSON that we will parse
	const string jsonExample =
		"{"
		"	\"myString\": \"value\","
		"	\"myEnum\": \"Value\","
		"	\"myVector\": ["
		"		0,"
		"		1"
		"	]"
		"}";

	// Create the object into which we will parse the data
	SNativeJsonRepresentation parsedObject;
	// Parse the provided JSON into 'parsedObject'
	// Note that Serialization::LoadJsonFile can also be used to read from disk
	if (Serialization::LoadJsonBuffer(parsedObject, jsonExample.data(), jsonExample.length()))
	{
		// Buffer was successfully parsed into 'parsedObject'.

		/* Modify 'parsedObject' here */

		// Save the object to disk
		Serialization::SaveJsonFile("MyJsonFile.json", parsedObject);
	}
}