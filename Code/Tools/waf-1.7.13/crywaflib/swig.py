# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

from waflib.Configure import conf
from waflib.TaskGen import feature, after_method, before_method, extension
from waflib import Task
from waflib.Tools import c_preproc

import os
import subprocess

########################################
############### MONO ###################
########################################
@conf
def configure_mono(conf):
	v = conf.env
	v['MONO'] = conf.CreateRootRelativePath('Code/SDKs/Mono/bin/mcs')
	mono_lib_path = conf.CreateRootRelativePath('Code/SDKs/Mono/lib/mono')
	v['MONO_FLAGS'] =   ['-target:library', '-langversion:4', '-platform:anycpu', '-optimize', '-g', '-L ' + mono_lib_path]
	# TODO: for debug add -debug
	v['MONO_TGT_F'] = ['-out:']
	
class mono(Task.Task):
	colo = 'GREEN'	
	cs_meta_data_node = None
	
	scan = c_preproc.scan
	
	def run(self):
		env = self.env
		gen = self.generator
		bld = gen.bld

		# Write assembly info
		if os.path.isfile(self.cs_meta_data_node.abspath()):
			try:
				os.remove(self.cs_meta_data_node.abspath())
			except:
				pass

		file_cs_meta = open(self.cs_meta_data_node.abspath(), 'wb')
		file_cs_meta.write("using System.Reflection;\n")
		file_cs_meta.write("[assembly: AssemblyProduct(\"%s\")]\n" % gen.name)
		file_cs_meta.write("[assembly: AssemblyTitle(\"%s\")]\n" % gen.name)
		file_cs_meta.write("[assembly: AssemblyDescription(\"%s\")]\n" % gen.name)
		file_cs_meta.write("[assembly: AssemblyVersion(\"%s\")]\n" % bld.get_product_version())
		file_cs_meta.write("[assembly: AssemblyCompany(\"%s\")]\n" % bld.get_company_name())
		file_cs_meta.write("[assembly: AssemblyCopyright(\"%s\")]\n" % bld.get_copyright())
		file_cs_meta.close()
		
		# Execute mono command
		mcs_cmd = '%s %s %s' % (env['MONO'], ' '.join(env['MONO_FLAGS']), self.cs_meta_data_node.abspath())
		for cs_source_node in self.inputs:
			mcs_cmd += ' %s' % cs_source_node.abspath()
		
		mcs_cmd += ' %s%s' % (' '.join(env['MONO_TGT_F']), self.outputs[0].abspath())		
		return self.exec_command(mcs_cmd, env=env.env or None)

@feature('swig')
def create_mono_task(self):
	bld 			= self.bld
	platform	= bld.env['PLATFORM']
	configuration = bld.env['CONFIGURATION']
	
	if platform  == 'project_generator':
		return
		
	if not getattr(self, 'mono_source', []):
		return

	# Create Mono task
	mono_output_node = self.path.make_node('CryEngine.Common.dll')	
	mono_task = self.create_task('mono', self.mono_source, [mono_output_node])
	mono_task.cs_meta_data_node = self.path.make_node(self.name + '_meta.cs')

	# Copy to output folder
	output_folder = self.bld.get_output_folders(platform, configuration)[0]
	task_copy = self.create_task('copy_outputs', mono_output_node, output_folder.make_node('CryEngine.Common.dll'))

########################################
############### SWIG ###################
########################################

class swig(Task.Task):
	always_run = True
	run_str = '${SWIG} -c++ ${SWIG_DEFINES_ST:DEFINES} -csharp -outdir ${TGT[2].parent.abspath()} ${SWIG_TGT_F}${TGT[2].abspath()} -namespace ${SWIG_NAMESPACE} -pch-file ${SWIG_PCH} -fno-include-guards -outfile ${TGT[0].name} -dllimport ${SWIG_TARGET} ${SRC[0].abspath()}'
	color   = 'BLUE'
	
@extension('.swig')
def swig_file_extension(self, node):
	pass #ignore
	
@conf
def configure_swig(conf):
	v = conf.env
	v['SWIG'] = conf.CreateRootRelativePath('Code/SDKs/swig/swig')
	v['SWIG_DEFINES_ST'] 	= '-D%s'
	v['SWIG_TGT_F'] = ['-o']
	
	v['DEFINES'] += ['SWIG_CSHARP_NO_IMCLASS_STATIC_CONSTRUCTOR']

@feature('swig')
@after_method('apply_link')
@after_method('process_pch_msvc')
def swig_apply_task_flags(self):		
	iter_swig_tasks = iter(getattr(self, 'swig_tasks', []))
	next(iter_swig_tasks, None)
	for t in iter_swig_tasks:	
		t.env.detach()
		t.env['DEFINES'] += ['SWIG_CXX_EXCLUDE_SWIG_INTERFACE_FUNCTIONS', ' SWIG_CSHARP_EXCLUDE_STRING_HELPER',  'SWIG_CSHARP_EXCLUDE_EXCEPTION_HELPER']

@feature('swig')
@before_method('apply_incpaths')
def apply_swig_flags(self):
	v = self.env
	v['SWIG_TARGET'] = self.target
	v['SWIG_NAMESPACE'] = 'CryEngine.Common'
	v['SWIG_PCH'] = '"StdAfx.h"'

@extension('.i')
def create_swig_task(self, node):

	if self.bld.env['PLATFORM']  == 'project_generator':
		return
	
	# Create SWIG task
	node_common = node.parent.make_node('CryEngine.swig')
	cs_node = node.change_ext('.cs')
	tsk = self.create_task('swig', [node, node_common], [cs_node, node.change_ext('.h'), node.change_ext('.cpp')])
	
	try:
		self.swig_tasks.append(tsk)
	except AttributeError:
		self.swig_tasks = [tsk]

	try:
		self.mono_source.append(cs_node)
	except AttributeError:
		self.mono_source = [cs_node]
	
	# Create CXX task
	swig_cpp_task = self.create_compiled_task('cxx', node.change_ext('.cpp'))
