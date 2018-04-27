# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

import os
from waflib import Logs
from waflib.TaskGen import feature
from waflib.CryModuleExtension import module_extension
from waflib.Utils import run_once

def get_xt_version_tag(env):
	msvc_version = env['MSVC_VERSION']
	if not isinstance(msvc_version, basestring):
		msvc_version = '11.0'

	if msvc_version == '11.0':
		xt_version_tag = 'vc11'
	if msvc_version == '12.0':
		xt_version_tag = 'vc12'
	if msvc_version == '14.0':
		xt_version_tag = 'vc14'
	return xt_version_tag

@module_extension('toolkit_pro')
def module_extensions_toolkit_pro(ctx, kw, entry_prefix, platform, configuration):
	
	if not platform.startswith('win'):
		return
	
	xt_version_tag = get_xt_version_tag(ctx.env)
	
	kw[entry_prefix + 'includes'] += [ ctx.CreateRootRelativePath('Code/SDKs/XT_13_4/Include') ]
	kw[entry_prefix + 'winres_includes'] += [ ctx.CreateRootRelativePath('Code/SDKs/XT_13_4/Include') ]
	kw[entry_prefix + 'libpath'] += [ ctx.CreateRootRelativePath('Code/SDKs/XT_13_4/lib_' + xt_version_tag) ]
	
	if not platform  == 'project_generator':
		kw[entry_prefix + 'features'] += [ 'copy_toolkit_pro_binaries' ]

@feature('copy_toolkit_pro_binaries')
@run_once
def feature_copy_toolkit_pro_binaries(self):
	bld 			= self.bld
	platform	= bld.env['PLATFORM']
	configuration = bld.env['CONFIGURATION']
		
	if platform  == 'project_generator':
		return
		
	if not platform in ['win_x86', 'win_x64']:
		Logs.error('[ERROR] ToolkitPro is not supported for plaform %s by WAF' % platform)
	
	xt_version_tag = get_xt_version_tag(bld.env)
	
	binary_name = 'ToolkitPro1340' + xt_version_tag + '0' + ( 'x64' if platform == 'win_x64' else '') + ('D' if configuration == 'debug' else '')

	output_folder = bld.get_output_folders(platform, configuration)[0]
	xt_bin = bld.CreateRootRelativePath('Code/SDKs/XT_13_4/bin_' + xt_version_tag) + os.sep + binary_name
	self.create_task('copy_outputs', bld.root.make_node(xt_bin + '.dll'), output_folder.make_node(binary_name + '.dll'))
	if os.path.isfile(xt_bin + '.pdb'):
		self.create_task('copy_outputs', bld.root.make_node(xt_bin + '.pdb'), output_folder.make_node(binary_name + '.pdb'))
