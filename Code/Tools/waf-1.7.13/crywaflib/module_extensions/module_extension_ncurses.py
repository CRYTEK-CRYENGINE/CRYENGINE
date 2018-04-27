# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

import os
from waflib import Logs
from waflib.TaskGen import feature
from waflib.CryModuleExtension import module_extension
from waflib.Utils import run_once

@module_extension('ncurses')
def module_extensions_ncurses(ctx, kw, entry_prefix, platform, configuration):

	if platform.startswith('linux_x64'):
		kw[entry_prefix + 'includes'] += [ ctx.CreateRootRelativePath('Code/SDKs/ncurses/include')]
		kw[entry_prefix + 'lib']      += [ 'ncursesw', 'menuw', 'formw' ]
		kw[entry_prefix + 'libpath']  += [ ctx.CreateRootRelativePath('Code/SDKs/ncurses/lib') ]
	else:
		return

	if not platform  == 'project_generator':
		kw[entry_prefix + 'features'] += [ 'copy_ncurses_binaries' ]


@feature('copy_ncurses_binaries')
@run_once
def copy_ncurses_binaries(self):
	bld 			= self.bld
	platform		= bld.env['PLATFORM']
	configuration	= bld.env['CONFIGURATION']

	ncurses_libpath = bld.CreateRootRelativePath('Code/SDKs/ncurses/lib')

	if platform == 'project_generator':
		return

	if platform.startswith('linux_x64'):

		output_folder = bld.get_output_folders(platform, configuration)[0]

		if hasattr(self, 'output_sub_folder'):
			if os.path.isabs(self.output_sub_folder):
				output_folder = bld.root.make_node(self.output_sub_folder)
			else:
				output_folder = output_folder.make_node(self.output_sub_folder)

		for f in ['libncursesw.so.6', 'libmenuw.so.6', 'libformw.so.6']:
			if os.path.isfile(os.path.join(ncurses_libpath, f)):
				self.create_task('copy_outputs', self.bld.root.make_node(os.path.join(ncurses_libpath, f)), output_folder.make_node(f))
			else:
				Logs.error('[ERROR] Could not copy %s: file not found in %s.' % (f, ncurses_libpath))

	else:
		Logs.error('[ERROR] WAF does not support ncurses for platform %s.' % platform)

