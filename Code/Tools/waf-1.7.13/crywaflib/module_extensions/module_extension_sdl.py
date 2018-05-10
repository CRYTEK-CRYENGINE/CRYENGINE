# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

import os
from waflib import Logs
from waflib.TaskGen import feature
from waflib.CryModuleExtension import module_extension
from waflib.Utils import run_once

@module_extension('sdl2')
def module_extensions_sdl2(ctx, kw, entry_prefix, platform, configuration):

	kw[entry_prefix + 'defines']  += ['USE_SDL2'] # SDL_Mixer, possibly renderer
	kw[entry_prefix + 'lib']      += ['SDL2']

	if platform.startswith('win_x86'):
		kw[entry_prefix + 'includes'] += [ ctx.CreateRootRelativePath('Code/SDKs/SDL2/include/win') ]
		kw[entry_prefix + 'libpath']  += [ ctx.CreateRootRelativePath('Code/SDKs/SDL2/lib/win_x86') ]
		kw[entry_prefix + 'defines']  += ['USE_SDL2'] # SDL_Mixer
		kw[entry_prefix + 'lib']      += ['SDL2']

	elif platform.startswith('win_x64'):
		kw[entry_prefix + 'includes'] += [ ctx.CreateRootRelativePath('Code/SDKs/SDL2/include/win') ]
		kw[entry_prefix + 'libpath']  += [ ctx.CreateRootRelativePath('Code/SDKs/SDL2/lib/win_x64') ]
		kw[entry_prefix + 'defines']  += ['USE_SDL2'] # SDL_Mixer
		kw[entry_prefix + 'lib']      += ['SDL2']

	elif platform.startswith('linux_x64'):
		kw[entry_prefix + 'includes'] += [ ctx.CreateRootRelativePath('Code/SDKs/SDL2/include/linux') ]
		kw[entry_prefix + 'libpath']  += [ ctx.CreateRootRelativePath('Code/SDKs/SDL2/lib/linux_x64') ]
		kw[entry_prefix + 'defines']  += ['USE_SDL2', 'USE_SDL2_WINDOWAPI'] # SDL_Mixer, Renderer
		kw[entry_prefix + 'lib']      += ['SDL2']
		
	elif platform == 'android_arm':
		kw[entry_prefix + 'includes'] += [ ctx.CreateRootRelativePath('Code/SDKs/SDL2/include/android-armeabi-v7a') ]
		kw[entry_prefix + 'libpath']  += [ ctx.CreateRootRelativePath('Code/SDKs/SDL2/lib/android-armeabi-v7a') ]
		kw[entry_prefix + 'defines']  += ['USE_SDL2', 'USE_SDL2_WINDOWAPI'] # SDL_Mixer, Renderer
		kw[entry_prefix + 'lib']      += ['SDL2']
		
	elif platform == 'android_arm64':
		kw[entry_prefix + 'includes'] += [ ctx.CreateRootRelativePath('Code/SDKs/SDL2/include/android-arm64-v8a') ]
		kw[entry_prefix + 'libpath']  += [ ctx.CreateRootRelativePath('Code/SDKs/SDL2/lib/android-arm64-v8a') ]
		kw[entry_prefix + 'defines']  += ['USE_SDL2', 'USE_SDL2_WINDOWAPI'] # SDL_Mixer, Renderer
		kw[entry_prefix + 'lib']      += ['SDL2']

	else:
		return

	if not platform  == 'project_generator':
		kw[entry_prefix + 'features'] += [ 'copy_sdl2_binaries' ]

@module_extension('sdl2_ext')
def module_extensions_sdl2_ext(ctx, kw, entry_prefix, platform, configuration):

	if platform == 'android_arm':
		kw[entry_prefix + 'includes'] += [ ctx.CreateRootRelativePath('Code/Tools/SDLExtension/src/include') ]
		kw[entry_prefix + 'lib']      += [ 'SDL2Ext' ]
		kw[entry_prefix + 'libpath']  += [ ctx.CreateRootRelativePath('Code/Tools/SDLExtension/lib/android-armeabi-v7a') ]

	if platform == 'android_arm64':
		kw[entry_prefix + 'includes'] += [ ctx.CreateRootRelativePath('Code/Tools/SDLExtension/src/include') ]
		kw[entry_prefix + 'lib']      += [ 'SDL2Ext' ]
		kw[entry_prefix + 'libpath']  += [ ctx.CreateRootRelativePath('Code/Tools/SDLExtension/lib/android-arm64-v8a') ]

	else:
		return		

@feature('copy_sdl2_binaries')
@run_once
def feature_copy_sdl2_binaries(self):
	bld 			= self.bld
	platform		= bld.env['PLATFORM']
	configuration	= bld.env['CONFIGURATION']

	if platform  == 'project_generator':
		return

	if platform == 'win_x64':
		files_to_copy = ['SDL2.dll']
		libfolder =  'win_x64'

	elif platform =='win_x86':
		files_to_copy = ['SDL2.dll']
		libfolder =  'win_x86'

	elif platform.startswith('linux_x64'):
		files_to_copy = ['libSDL2-2.0.so.0']
		libfolder = 'linux_x64'

	elif platform.startswith('android'):
		return # do nothing
	
	else:
		Logs.error('[ERROR] WAF does not support SDL2 for platform %s.' % platform)
		return

	sdl2_libpath = bld.CreateRootRelativePath('Code/SDKs/SDL2/lib') + os.sep + libfolder

	output_folder = bld.get_output_folders(platform, configuration)[0]

	if hasattr(self, 'output_sub_folder'):
		if os.path.isabs(self.output_sub_folder):
			output_folder = bld.root.make_node(self.output_sub_folder)
		else:
			output_folder = output_folder.make_node(self.output_sub_folder)

	for f in files_to_copy:
		if os.path.isfile(os.path.join(sdl2_libpath, f)):
			self.create_task('copy_outputs', self.bld.root.make_node(os.path.join(sdl2_libpath, f)), output_folder.make_node(f))
		else:
			Logs.error('[ERROR] Could not copy %s: file not found in %s.' % (f, sdl2_libpath))
