# Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

objects = object.get_all_objects("AnimObject", "") # Get the name list of all anim objects in the level.
selection.clear()
# If there is any object with the geometry file whose name contains "\\story\\", select it
for obj in objects:
	geometry_file = entity.get_geometry_file(obj)
	if geometry_file.find("\\story\\") != -1:
		selection.select_object(obj)
