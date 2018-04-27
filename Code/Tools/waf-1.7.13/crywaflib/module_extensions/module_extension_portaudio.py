# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

import os
from waflib import Logs
from waflib.TaskGen import feature
from waflib.CryModuleExtension import module_extension
from waflib.Utils import run_once

@module_extension('audio_portaudio')
def module_extensions_audio_portaudio(ctx, kw, entry_prefix, platform, configuration):	
	if not platform.startswith('win'):
		return
	
	if not platform  == 'project_generator':
		kw[entry_prefix + 'features'] += [ 'copy_audio_portaudio_binaries' ]

@feature('copy_audio_portaudio_binaries')
@run_once
def feature_copy_audio_portaudio_binaries(self):
	bld = self.bld
	platform = bld.env['PLATFORM']
	configuration = bld.env['CONFIGURATION']
	
	if platform == 'project_generator':
		return
	
	if not platform in ['win_x86', 'win_x64']:
		Logs.error('[ERROR] PortAudio audio is not supported for plaform %s by WAF' % platform)
	
	if platform == 'win_x64':
		files_to_copy = ['portaudio_x64.dll', 'portaudio_x64.pdb']
		portaudio_libfolder = libsndfile_libfolder = 'win64'

	elif platform =='win_x86':
		files_to_copy = ['portaudio_x86.dll', 'portaudio_x86.pdb']
		portaudio_libfolder = libsndfile_libfolder = 'win32'
	
	portaudio_configuration = ('release' if configuration != 'debug' else 'debug')
	
	portaudio_libpath = bld.CreateRootRelativePath('Code/SDKs/Audio/portaudio/lib') + os.sep + portaudio_libfolder + os.sep + portaudio_configuration
	output_folder = bld.get_output_folders(platform, configuration)[0]
	
	for f in files_to_copy:
		if os.path.isfile(os.path.join(portaudio_libpath, f)):
			self.create_task('copy_outputs', bld.root.make_node(os.path.join(portaudio_libpath, f)), output_folder.make_node(f))
		else:
			Logs.error('[ERROR] Could not copy %s: file not found in %s.' % (f, portaudio_libpath))

	libsndfile_file = 'libsndfile-1.dll'
	libsndfile_filepath = bld.CreateRootRelativePath('Code/SDKs/Audio/libsndfile/lib/' + libsndfile_libfolder + os.sep + libsndfile_file)
	
	if os.path.isfile(libsndfile_filepath):
		self.create_task('copy_outputs', bld.root.make_node(libsndfile_filepath), output_folder.make_node(libsndfile_file))
	else:
		Logs.warn('[WARNING] libsndfile DLL not found at: %s' % libsndfile_filepath)
	