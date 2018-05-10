# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
from waflib.Configure import conf
from waflib.TaskGen import after_method, feature, extension
from waflib.Task import Task,RUN_ME
import os

#############################################################################
# Helper Function to load settings which are the same for each configuration
@conf
def load_cppcheck_common_settings(conf):
	v = conf.env
		
	# CC/CXX Compiler	
	v['CC'] 		= v['CXX'] 			= conf.CreateRootRelativePath('Tools/CppCheck/cppcheck.exe')
	v['CC_NAME']	= v['CXX_NAME'] 	= 'cppcheck'
	v['CC_SRC_F']  	= v['CXX_SRC_F']	= []
	v['CC_TGT_F'] 	= v['CXX_TGT_F']	= []
	

	v['CCLNK_SRC_F'] = v['CXXLNK_SRC_F'] = []
	v['CCLNK_TGT_F'] = v['CXXLNK_TGT_F'] = ''
		
	# Specify how to translate some common operations for a specific compiler
	v['CPPPATH_ST'] 	= '-I%s'
	v['DEFINES_ST'] 	= '-D%s'
	v['LIB_ST'] 		= ''
	v['LIBPATH_ST'] 	= ''
	v['STLIB_ST'] 		= ''
	v['STLIBPATH_ST'] 	= ''
	
	# Pattern to transform outputs
	v['cprogram_PATTERN'] 	= ''
	v['cxxprogram_PATTERN'] = ''
	v['cshlib_PATTERN'] 	= ''
	v['cxxshlib_PATTERN'] 	= ''
	v['cstlib_PATTERN']      = ''
	v['cxxstlib_PATTERN']    = ''
		
		
	# Set up compiler flags
	COMMON_COMPILER_FLAGS = [
		#'-v',
		'--inline-suppr',
		'--template=vs',
		'--enable=style',
		'--force',
		]
		
	# Copy common flags to prevent modifing references
	v['CFLAGS'] = COMMON_COMPILER_FLAGS[:]
	v['CXXFLAGS'] = COMMON_COMPILER_FLAGS[:]	

	
#############################################################################
@conf
def load_debug_win_x64_cppcheck_settings(conf):
	conf.load_cppcheck_common_settings()
	
#############################################################################	
@conf
def load_profile_win_x64_cppcheck_settings(conf):
	conf.load_cppcheck_common_settings()
	
#############################################################################	
@conf
def load_performance_win_x64_cppcheck_settings(conf):
	conf.load_cppcheck_common_settings()
	
#############################################################################	
@conf
def load_release_win_x64_cppcheck_settings(conf):
	conf.load_cppcheck_common_settings()
	
#############################################################################	
import re, sys, os, string, traceback
re_inc = re.compile(
	'^[ \t]*(#|%:)[ \t]*(include)[ \t]*[<"](.*)[>"]\r*$',
	re.IGNORECASE | re.MULTILINE)

def lines_includes(node):
	code = node.read()
	if c_preproc.use_trigraphs:
		for (a, b) in c_preproc.trig_def: code = code.split(a).join(b)
	code = c_preproc.re_nl.sub('', code)
	code = c_preproc.re_cpp.sub(c_preproc.repl, code)
	return [(m.group(2), m.group(3)) for m in re.finditer(re_inc, code)]

from waflib.Tools import c_preproc

class dumb_parser(c_preproc.c_parser):
	def addlines(self, node):
		if node in self.nodes[:-1]:
			return
		self.currentnode_stack.append(node.parent)
		self.lines = lines_includes(node) + [(c_preproc.POPFILE, '')] +  self.lines

	def start(self, node, env):
		self.addlines(node)
		while self.lines:
			(x, y) = self.lines.pop(0)
			if x == c_preproc.POPFILE:
				self.currentnode_stack.pop()
				continue
			self.tryfind(y)
	
from waflib import Context, Errors

import threading
#from waflib.Tools import c_preproc
lock = threading.Lock()
g_printed_messages = {}
class cppcheck(Task):
	
	def scan(task):
		try:
			incn = task.generator.includes_nodes
		except AttributeError:
			raise Errors.WafError('%r is missing a feature such as "c", "cxx" or "includes": ' % task.generator)
		incn_new = []
		
		# For CppCheck, only probe engine files, don't track deps for 3rdParty
		for i in incn:
			if task.generator.bld.CreateRootRelativePath('Code/SDKs') in i.abspath():
				continue
			if task.generator.bld.CreateRootRelativePath('Code/Tools') in i.abspath():
				continue
			incn_new.append(i)
				
		nodepaths = [x for x in incn_new if x.is_child_of(x.ctx.srcnode) or x.is_child_of(x.ctx.bldnode)]
			
		tmp = dumb_parser(nodepaths)	
		tmp.start(task.inputs[0], task.env)
			
		return (tmp.nodes, tmp.names)
	
	def run(self):
		
		returnValue = 0
		cmd = [self.env['CC']] + self.env['CFLAGS'] + [ self.inputs[0].abspath() ]
		
		try:	
			(out,err) = self.generator.bld.cmd_and_log(cmd, output=Context.BOTH, quiet=Context.BOTH)
		except Exception as e:
			print 'mh, error?'
			(out,err) = (e.stdout, e.stderr)			
			returnValue = 1
					
		self.outputs[0].write(err)
		
		# Filter to locks to prevent duplicate entries
		global lock
		lock.acquire()
		for line in [err]:
			if not line in g_printed_messages:
				g_printed_messages[line] = True
				print line				
		lock.release()

		return returnValue
		
	def runnable_status(self):	
		return RUN_ME # always execute cppcheck
		#return super(cppcheck,self).runnable_status()		
	


@feature('cppcheck')
@after_method('apply_incpaths')
def create_cppcheck_tasks(self):
		
	lst = self.to_incnodes(self.to_list(getattr(self, 'includes', [])) + self.env['INCLUDES'])	
	self.includes_nodes = lst
	
	nodes = self.to_nodes(self.to_check_sources)
	
	for node in nodes:	
		if 'LSAOShadersCache.cpp' in node.abspath():
			continue #Hack, this is a single file which completly breaks cppcheck
	
		tsk = self.create_task('cppcheck', node, node.change_ext('.cppcheck.out') )			
		tsk.env['CONFIGURATION'] = 'force'
					
	self.bld.env['PLATFORM'] = 'cppcheck'		
			
	
