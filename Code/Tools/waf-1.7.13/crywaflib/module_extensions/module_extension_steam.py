# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

import os
from waflib import Logs
from waflib.TaskGen import feature
from waflib.CryModuleExtension import module_extension
from waflib.Utils import run_once

@module_extension('steam')
def module_extensions_steam(ctx, kw, entry_prefix, platform, configuration):
	
	if not platform.startswith('win'):
		return		
		
	kw[entry_prefix + 'defines'] += [ 'USE_STEAM=1' ]
	kw[entry_prefix + 'includes'] += [ ctx.CreateRootRelativePath('Code/SDKs/Steamworks/public') ]

	if platform == 'win_x86':
		kw[entry_prefix + 'libpath'] += [ ctx.CreateRootRelativePath('Code/SDKs/Steamworks/redistributable_bin') ]
	elif platform == 'win_x64':
		kw[entry_prefix + 'libpath'] += [ ctx.CreateRootRelativePath('Code/SDKs/Steamworks/redistributable_bin/win64') ]
		
	if not platform  == 'project_generator':
		kw[entry_prefix + 'features'] += [ 'copy_steam_api_binaries' ]

@feature('copy_steam_api_binaries')
@run_once
def feature_copy_steam_api_binaries(self):
	bld 			= self.bld
	platform	= bld.env['PLATFORM']
	configuration = bld.env['CONFIGURATION']
		
	if platform  == 'project_generator':
		return
		
	bin_location = {'win_x86' : '/steam_api.dll'	, 'win_x64' : '/win64/steam_api64.dll'}
			
	if not platform in bin_location:
		Logs.error('[ERROR] Steam is not supported on plaform by WAF: %s' % platform)
		
	output_folder = bld.get_output_folders(platform, configuration)[0]			
	steam_bin = bld.CreateRootRelativePath('Code/SDKs/Steamworks/redistributable_bin') + bin_location[platform]
	self.create_task('copy_outputs', bld.root.make_node(steam_bin), output_folder.make_node(os.path.basename(steam_bin)))
