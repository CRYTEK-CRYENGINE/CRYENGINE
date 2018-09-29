message(STATUS "DISABLING: Trackable disabling of certain build options.")

SET(PROJECT_CRYENGINE_GAMESDK OFF CACHE BOOL "a" FORCE)	## Don't bother with this piece of junk.
SET(OPTION_UNITY_BUILD OFF CACHE BOOL "a" FORCE)		## Not caring about this or crymono for now.
SET(OPTION_CRYMONO OFF CACHE BOOL "a" FORCE)