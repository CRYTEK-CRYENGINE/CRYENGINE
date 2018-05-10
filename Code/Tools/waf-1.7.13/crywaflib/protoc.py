#!/usr/bin/env python
# encoding: utf-8
# Philipp Bender, 2012
# Matt Clarkson, 2012

from waflib.Task import *
from waflib.TaskGen import extension
from waflib.TaskGen import feature,after_method,before_method
from waflib.Configure import conf
from waflib import Logs
import os
import sys

"""
A simple tool to integrate protocol buffers into your build system.

    def configure(conf):
        conf.load('compiler_cxx cxx protoc')

    def build(bld):
        bld(
                features = 'cxx cxxprogram'
                source   = 'main.cpp file1.proto proto/file2.proto',
                includes = '. proto',
                target   = 'executable')

"""

is_host_linux = 'linux' in sys.platform
proto_plugins_rpath = 'Tools/crycloud/protobuf/plugins/' + ('linux_x64' if is_host_linux else 'win_x64')
cpp_plugin_file = '/protobuf_cppplugin' if is_host_linux else '/protobuf_cppplugin.exe'

class protoc(Task):
	color   = 'BLUE'	
	run_str =  '${PROTOC} ${PROTOC_INCLUDE_ST:PROTOC_INCLUDE_PATHS} ${PROTOC_FLAGS} ${PROTOCPLUGIN_FLAGS} ${PROTOCPLUGIN} ${SRC[0].abspath()}'
	ext_out = ['.h', 'pb.cc']

	# make the plugin an implicit dependency
	def scan(self):
		plugin_node = self.generator.bld.path.find_resource(proto_plugins_rpath + cpp_plugin_file)
		return ([plugin_node], [])

	def runnable_status(self):
		"""
		Override :py:meth:`waflib.Task.TaskBase.runnable_status` to determine if the task is ready
		to be run (:py:attr:`waflib.Task.Task.run_after`)
		"""
		#return 0 # benchmarking

		for t in self.run_after:
			if not t.hasrun:
				return ASK_LATER

		bld = self.generator.bld

		# first compute the signature
		try:
			new_sig = self.signature()
		except Errors.TaskNotReady:
			return ASK_LATER

		# compare the signature to a signature computed previously
		key = self.uid()
		try:
			prev_sig = bld.task_sigs[key]
		except KeyError:
			Logs.info("task: task %r must run as it was never run before or the task code changed" % self)
			return RUN_ME

		# This logic is different from Task.runnable_status.
		# In Task it compares all node.sig in outputs and new_sig. If they are different, it will return RUN_ME to run this task.
		# Here we only need to check whether the raw proto files are changed or the target .cc and .h files are deleted. The protoc will generate new .cc and .h files
		for node in self.outputs:
			if not os.path.isfile(node.abspath()):
				return RUN_ME

		if new_sig != prev_sig:
			return RUN_ME
		return SKIP_ME
		
@feature('protoc')
@before_method('apply_incpaths')
def apply_protoc_include_path(self):
	self.env['INCLUDES'] += [
		self.bld.get_bintemp_folder_node().make_node('protobuf').abspath(),
		self.bld.CreateRootRelativePath('Code/SDKs/protobuf-3.1.0/include')
		]

@extension('.proto')
def process_protoc(self, node):		
	bin_temp_folder_node = self.bld.get_bintemp_folder_node().make_node('protobuf').make_node('GeneratedFiles')
	bin_temp_folder_node.mkdir()
	cpp_node = bin_temp_folder_node.make_node(node.change_ext('.pb.cc').name)
	hpp_node = bin_temp_folder_node.make_node(node.change_ext('.pb.h').name)
	task = self.create_task('protoc', node, [cpp_node, hpp_node])
	if self.bld.cmd == "msvs":
		self.source.extend(task.outputs)

	cpptask = self.create_compiled_task('cxx', cpp_node)
	cpptask.disable_pch = True		
	
	use = getattr(self, 'use', '')
	if not 'PROTOBUF' in use:
		self.use = self.to_list(use) + ['PROTOBUF']
		
	self.env['PROTOC_INCLUDE_PATHS'] =  getattr(self, 'protobuf_additional_includes', []) + self.env['PROTOC_INCLUDE_PATHS']
	
	try:
		self.protoc_tasks.append(task)
	except AttributeError:
		self.protoc_tasks = [task]
	
@conf
def configure_protoc(conf):
	v = conf.env
	bin_temp_folder_node = conf.get_bintemp_folder_node().make_node('protobuf').make_node('GeneratedFiles')
	proto_compiler_folder = conf.CreateRootRelativePath('Code/SDKs/protobuf-3.1.0')
	proto_plugin_folder = conf.CreateRootRelativePath(proto_plugins_rpath)
	
	if 'linux' in sys.platform:
		v['PROTOC'] = proto_compiler_folder + '/bin/linux/protoc'
	else:
		v['PROTOC'] = proto_compiler_folder + '/bin/win_x86/protoc.exe'		
		
	#conf.check_cfg(package="protobuf", uselib_store="PROTOBUF", args=['--cflags', '--libs'])
	#conf.find_program('protoc', var='PROTOC')	
	
	protobuf_include_path = [
			conf.CreateRootRelativePath('Code/SDKs/protobuf-3.1.0/include/'),
			conf.CreateRootRelativePath('Code/CryCloud/ProtocolBase/'),
			conf.CreateRootRelativePath('Code/CryCloud/Messaging/'),
			conf.CreateRootRelativePath('Code/CryCloud/GameModules/Common/GameCommon/Protobuf/'),
			conf.CreateRootRelativePath('Code/CryCloud/GameTelemetryProvider/protobuf/'),
			conf.get_bintemp_folder_node().make_node('protobuf').abspath(),
			bin_temp_folder_node.abspath()]
	v['PROTOC_INCLUDE_PATHS'] = protobuf_include_path
	v['PROTOC_INCLUDE_ST'] = '-I%s'
	v['PROTOC_FLAGS'] = '--cpp_out=%s' % bin_temp_folder_node.abspath()
	v['PROTOCPLUGIN_FLAGS'] = '--cry_out=%s' % bin_temp_folder_node.abspath()
	v['PROTOCPLUGIN'] = '--plugin=protoc-gen-cry=%s' % (os.path.normpath(proto_plugin_folder + cpp_plugin_file))
