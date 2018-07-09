# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

import os

from waflib.Build import BuildContext
from waflib import TaskGen
from waflib.TaskGen import extension
from waflib.Task import Task,RUN_ME

#############################################################################
class uber_file_generator(BuildContext):
	''' Util class to generate Uber Files after configure '''
	cmd = 'generate_uber_files'
	fun = 'build'
					
@TaskGen.taskgen_method
@extension('.xpm')
@extension('.def')
@extension('.rtf')
@extension('.action_filters')
@extension('.man')
@extension('.icns')
@extension('.xml')
@extension('.rc2')
@extension('.lib')
@extension('.png')
@extension('.manifest')
@extension('.cfi')
@extension('.cfx')
@extension('.ext')
@extension('.in')
@extension('.am')
@extension('.h')
@extension('.H')
@extension('.hpp')
@extension('.inl')
@extension('.txt')
@extension('.bmp')
@extension('.ico')
@extension('.S')
@extension('.pssl')
@extension('.cur')
@extension('.actions')
@extension('.lua')
@extension('.cfg')
@extension('.csv')
@extension('.appxmanifest')
@extension('.hlsl')
@extension('.java')
def header_dummy(tgen,node):
	pass
	
@TaskGen.taskgen_method
@TaskGen.feature('generate_uber_file')
def gen_create_uber_file_task(tgen):

	uber_file_folder = tgen.bld.bldnode.make_node('uber_files').make_node(tgen.target)
	uber_file_folder.mkdir()
	
	# Iterate over all uber files, to collect files per Uber file
	uber_file_list = getattr(tgen, 'uber_file_list')
	for (uber_file,project_filter) in uber_file_list.items():
		if uber_file == 'NoUberFile':
			continue # Don't create a uber file for the special NoUberFile name
		files = []
		for (k_1,file_list) in project_filter.items():
			for file in file_list:
				if file.endswith('.c') or file.endswith('.cpp') or file.endswith('.CPP'):
					files.append(file)

		file_nodes = tgen.to_nodes(files)
		# Create Task to write Uber File
		tsk = tgen.create_task('gen_uber_file', file_nodes, uber_file_folder.make_node(uber_file) ) 
		setattr( tsk, 'UBER_FILE_FOLDER', uber_file_folder )
		tsk.pch = getattr(tgen, 'pch')
					
class gen_uber_file(Task):
	color =  'BLUE'	
		
	def compute_uber_file(self):
		lst = []		
		uber_file_folder = self.UBER_FILE_FOLDER
		if not self.pch == '':
			lst += ['#include "%s"\n' % self.pch.replace('.cpp','.h')]
		lst += ['#include "%s"\n' % node.path_from(uber_file_folder) for node in self.inputs]
		uber_file_content = ''.join(lst)		
		return uber_file_content
		
	def run(self):
		uber_file_content = self.compute_uber_file()
		self.outputs[0].write(uber_file_content)
		return 0
		
	def runnable_status(self):	
		if super(gen_uber_file, self).runnable_status() == -1:
			return -1

		uber_file = self.outputs[0]
		# If no uber file exist, we need to write one
		try:
			stat_tgt = os.stat(uber_file.abspath())
		except OSError:	
			return RUN_ME
			
		# We have a uber file, lets compare content
		uber_file_content = self.compute_uber_file()
			
		current_content = self.outputs[0].read()
		if uber_file_content == current_content:
			return -2 # SKIP_ME
			
		return RUN_ME	# Always execute Uber file Generation
						
