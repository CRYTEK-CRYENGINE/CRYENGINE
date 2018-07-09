# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

import os
from waflib import Logs
from waflib.TaskGen import feature
from waflib.CryModuleExtension import module_extension
from waflib.Utils import run_once

@module_extension('python27')
def module_extensions_python27(ctx, kw, entry_prefix, platform, configuration):
	
	if configuration == "":
		configuration = ctx.env['CONFIGURATION']

	kw[entry_prefix + 'defines']  += ['USE_PYTHON_SCRIPTING', 'BOOST_PYTHON_STATIC_LIB', 'HAVE_ROUND']

	kw[entry_prefix + 'use'] += ['python27']

	if configuration == 'debug':
		kw[entry_prefix + 'defines']  += ['BOOST_DEBUG_PYTHON', 'BOOST_LINKING_PYTHON']

	kw[entry_prefix + 'includes'] += [ctx.CreateRootRelativePath('Code/Libs/python'), ctx.CreateRootRelativePath('Code/SDKs/Python27/Include'), ctx.CreateRootRelativePath('Code/SDKs/boost')]
	kw[entry_prefix + 'libpath'] += [ctx.CreateRootRelativePath('Code/SDKs/boost/lib/64bit')]
	
	if not platform  == 'project_generator':
		kw[entry_prefix + 'features'] += [ 'copy_python_lib_zip' ]

@feature('copy_python_lib_zip')
@run_once
def feature_copy_python_lib_zip(self):
	bld = self.bld
	platform = bld.env['PLATFORM']
	configuration = bld.env['CONFIGURATION']
		
	if platform  == 'project_generator':
		return
		  
	output_folder = bld.get_output_folders(platform, configuration)[0]
	python_zip = bld.CreateRootRelativePath('Code/SDKs/Python27') + '/python27.zip'
	debug_zip = "python27_d.zip"
	self.create_task('copy_outputs', bld.root.make_node(python_zip), output_folder.make_node(debug_zip if configuration == 'debug' else os.path.basename(python_zip)))
