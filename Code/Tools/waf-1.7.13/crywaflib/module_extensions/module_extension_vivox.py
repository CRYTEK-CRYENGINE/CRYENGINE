# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

import os
from waflib import Logs
from waflib.TaskGen import feature
from waflib.CryModuleExtension import module_extension
from waflib.Utils import run_once

@run_once
def log_vivox_not_supported(platform):
	Logs.warn('Vivox is not configured for current platform: %s' % platform )

@module_extension('vivox')
def module_extensions_vivox(ctx, kw, entry_prefix, platform, configuration):
	
	if not platform.startswith('win'):
		return		
		
	kw[entry_prefix + 'includes'] += [ ctx.CreateRootRelativePath('Code/SDKs/vivox/vivoxclientsdk/include') ]	
	
	if platform == 'win_x64':
		kw[entry_prefix + 'libpath'] += [ ctx.CreateRootRelativePath('Code/SDKs/vivox/vivoxclientsdk/x64/bin') ]
		kw[entry_prefix + 'lib'] += [ 'vivoxplatform', 'vivoxsdk_x64' ]
	else:
		log_vivox_not_supported(platform)
		
	if not platform  == 'project_generator':
		kw[entry_prefix + 'features'] += [ 'copy_vivox_api_binaries' ]

@feature('copy_vivox_api_binaries')
@run_once
def feature_copy_vivox_api_binaries(self):
	bld 			= self.bld
	platform	= bld.env['PLATFORM']
	configuration = bld.env['CONFIGURATION']
		
	if platform  == 'project_generator':
		return
		
	bin_location = {'win_x64' : ['x64/bin/vivoxplatform.dll', 'x64/bin/vivoxsdk_x64.dll', 'x64/bin/ortp_x64.dll', 'x64/bin/ortp_x64.pdb', 'x64/bin/vivoxplatform.pdb', 'x64/bin/vivoxsdk_x64.pdb']}
			
	if not platform in bin_location:
		Logs.error('[ERROR] Vivox is not configured on plaform: %s' % platform)
		
	output_folder = bld.get_output_folders(platform, configuration)[0]		


	for f in bin_location[platform]:
		vivox_bin = bld.CreateRootRelativePath('Code/SDKs/vivox/vivoxclientsdk') + os.sep + f
		self.create_task('copy_outputs', bld.root.make_node(vivox_bin), output_folder.make_node(os.path.basename(vivox_bin)))
