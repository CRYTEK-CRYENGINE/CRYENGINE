import sandbox

# Flow of this testing scripts is
#  1) setup -> fills objectsToTest list
#  2) objects on objectsToTest are serialized to archives
#  3) execute -> executes the functions that need testing
#  4) objects on objectsToTest are serialized to archives again
#  5) the archives serialized after setup and execute are compared, if there are discrepancies
#     the test fails
#  6)  the cleanup function is called so that the objects created for the test can be removed

# How many objects we want to attach
objectsCount = 2
# The type of the brush we want to spawn
assetName = "objects/props/fishing_camp/camping_chair.cgf"
# The objects that will be tested for differences, this list needs to be instantiated
# for the test to work
objectsToTest = []
# The objects to attach to our group
objectsToAddToGroup = []
# The name of the Group object
groupName = "Group"


# Prepare the test, instantiate all necessary objects and add them to the objectsToTest list
# after this function ends all the objects in the objectsToTest list will be serialized into
# archives
def setup():
    # First create a group
    sandbox.general.new_object("Group", "", groupName, 520, 500, 32)
    # Then create and add the brushes to the list of objects to attach to the group
    for i in range(objectsCount):
        objectName = "test-group" + str(i)
        sandbox.general.new_object("Brush", assetName, objectName, 520, 500 + (i * 5), 32)
        # Add all the brushes to the list of objects to serialize and test
        objectsToTest.append(objectName)
        objectsToAddToGroup.append(objectName)
    # And then add the group itself
    objectsToTest.append("Group")


# Execute the test, after this function ends the objects in the objectsToTest will be serialized
# one more time and then compared to the archives serialized just after the end of the setup
# function
def executeTest():
    # Attach all the objects to the group
    sandbox.group.attach_objects_to(objectsToAddToGroup, "Group")
    # Undo the operation
    sandbox.general.undo()


# This should destroy every object created for this test
def cleanup():
    for object in objectsToTest:
        sandbox.object.delete(object)
