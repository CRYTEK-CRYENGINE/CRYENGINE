# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

import os
from waflib import Logs, Utils
from waflib.TaskGen import feature
from waflib.CryModuleExtension import module_extension
from waflib.Utils import run_once

@Utils.memoize
def check_path_exists(path):
	if os.path.isdir(path):
		return True
	else:
		Logs.warn('[WARNING] ' + path + ' not found, this feature is excluded from this build.')
	return False

@module_extension('eyex')
def module_extensions_eyex(ctx, kw, entry_prefix, platform, configuration):
	
	if not platform.startswith('win'):
		return		

	# Only include EyeX if it exists
	if not check_path_exists(ctx.CreateRootRelativePath('Code/SDKs/Tobii')):
		return

	kw[entry_prefix + 'defines'] += [ 'USE_EYETRACKER' ]
	kw[entry_prefix + 'includes'] += [ ctx.CreateRootRelativePath('Code/SDKs/Tobii/SDK/include') ]

	if platform == 'win_x86':
		kw[entry_prefix + 'libpath'] += [ ctx.CreateRootRelativePath('Code/SDKs/Tobii/SDK/lib/x86') ]
	elif platform == 'win_x64':
		kw[entry_prefix + 'libpath'] += [ ctx.CreateRootRelativePath('Code/SDKs/Tobii/SDK/lib/x64') ]
		
	if not platform  == 'project_generator':
		kw[entry_prefix + 'features'] += [ 'copy_eyex_binaries' ]

@feature('copy_eyex_binaries')
@run_once
def feature_copy_eyex_binaries(self):
	bld 			= self.bld
	platform	= bld.env['PLATFORM']
	configuration = bld.env['CONFIGURATION']
		
	if platform  == 'project_generator':
		return
		
	bin_location = {'win_x86' : '/x86/'	, 'win_x64' : '/x64/'}
	bin_items = [ 'Tobii.EyeX.Client.dll' ]
			
	if not platform in bin_location:
		Logs.error('[ERROR] EyeX is not supported on plaform by WAF: %s' % platform)
		
	# Copy core eyex binary
	output_folder = bld.get_output_folders(platform, configuration)[0]
	for item in bin_items:
		eyex_bin = bld.CreateRootRelativePath('Code/SDKs/Tobii/SDK/lib') + bin_location[platform] + item
		self.create_task('copy_outputs', bld.root.make_node(eyex_bin), output_folder.make_node(os.path.basename(eyex_bin)))

	# Copy additional required binaries at runtime
	eyex_base = "Code/SDKs/Tobii/SDK"
	eyex_base_len = len(eyex_base)
	eyex_node = bld.root.make_node(eyex_base)
	eyex_lib_node = eyex_node.make_node("lib/eyex")
	eyex_etc_node = eyex_node.make_node("etc/eyex")
	
	tgt_folder = output_folder.make_node("eyex")
	
	for src_path_node in [eyex_lib_node, eyex_etc_node]:
		for path, subdirs, files in os.walk(src_path_node.abspath(), followlinks=True):
			path_node = bld.root.make_node(path)
			stripped_path = path[eyex_base_len:]
			for name in files:		
				if name.endswith('.dll') or name.endswith('.config'):
					src_node = path_node.make_node(name)
					tgt_node = tgt_folder.make_node(stripped_path).make_node(name)
					#print "== tgt", tgt_node.abspath()
					self.create_task('copy_outputs', src_node, tgt_node)
