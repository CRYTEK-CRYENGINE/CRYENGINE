message(STATUS "OVERRIDE: Overriding user-set options. Trackable in OVERRIDE.cmake")

SET(OPTION_RC ON CACHE BOOL "a" FORCE)					## Needed to compile any shaders etc.
SET(PROJECT_CRYENGINE_GAMESDK OFF CACHE BOOL "a" FORCE)	## Don't bother with this piece of junk.
SET(OPTION_UNITY_BUILD OFF CACHE BOOL "a" FORCE)		## Not caring about this or crymono for now.
SET(OPTION_CRYMONO OFF CACHE BOOL "a" FORCE)

## For main-branch build
SET(PLUGIN_GAMEPLATFORM OFF CACHE BOOL "a" FORCE)	## Doesn't detect if sdk exists automatically anymore
SET(AUDIO_SDL_MIXER OFF CACHE BOOL "a" FORCE)		## 5.5.0 available SDK's fail to build sdl_mixer