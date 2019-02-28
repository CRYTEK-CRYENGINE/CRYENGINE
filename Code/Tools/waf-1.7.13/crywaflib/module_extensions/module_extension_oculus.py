# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

import os
from waflib import Logs
from waflib.TaskGen import feature
from waflib.CryModuleExtension import module_extension
from waflib.Utils import run_once

@module_extension('oculus_audio_wwise')
def module_extensions_oculus_audio_wwise(ctx, kw, entry_prefix, platform, configuration):	
	if platform  == 'project_generator':
		return

	if not platform.startswith('win'):
		return

	if not platform  == 'project_generator':
		kw[entry_prefix + 'features'] += [ 'copy_oculus_audio_wwise_binaries' ]

@feature('copy_oculus_audio_wwise_binaries')
@run_once
def feature_copy_oculus_audio_wwise_binaries(self):
	bld 			= self.bld
	platform	= bld.env['PLATFORM']
	configuration = bld.env['CONFIGURATION']
		
	if platform  == 'project_generator':
		return

	if not platform in ['win_x86', 'win_x64']:
		Logs.error('[ERROR] Oculus Wwise Audio is not supported for plaform %s by WAF' % platform)

	target_platform = 'x64' if platform == 'win_x64' else 'Win32'
	bin_name = 'OculusSpatializer.dll'
	src_path = bld.CreateRootRelativePath('Code/SDKs/audio/oculus/wwise/' + target_platform + '/bin/plugins/' + bin_name)
	
	output_folder = bld.get_output_folders(platform, configuration)[0]
	
	if os.path.isfile(src_path):
		self.create_task('copy_outputs', bld.root.make_node(src_path), output_folder.make_node(bin_name))
	else:
		Logs.warn('[WARNING] Oculus Wwise Audio DLL not found at: %s' % src_path)
