#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006-2010 (ita)
# Christopher Bolte: Created a copy to easier adjustment to crytek specific changes

from waflib.Configure import conf
from waflib.TaskGen import feature, after_method,before_method, extension
from waflib.Task import Task
from waflib import Task
from waflib import Logs, Errors, Utils
from waflib import Context
from waflib.Utils import run_once
import os, re, threading
from waflib.Tools import c_preproc

try:
	from xml.sax import make_parser
	from xml.sax.handler import ContentHandler
except ImportError:
	xml_sax = False
	ContentHandler = object
else:
	xml_sax = True

# TODO: This should be moved to a global configuration file at some point
QT_ROOT = 'Code/SDKs/Qt'
QT_MAJOR_VERSION = "5"
QT_MINOR_VERSION = "6"

qt_modules = [
	'Core',
	'Gui',
	'Network',
	'OpenGL',
	'Svg',
	'Widgets',
	'Qml',
	'WebEngineWidgets'
]

qt_core_binaries = [
	'icudt', 
	'icuin', 
	'icuuc', 
	'libGLESv2'
]

qt_plugins = {
	'imageformats': ['qico', 'qsvg'],
	'platforms': ['qminimal', 'qwindows']
}

def get_qt_root_path(self, *subpaths):
	# Uses the current MSVC version to find the correct path to binaries/libraries/plugins of QT
	msvc_version = self.env['MSVC_VERSION']
	if isinstance(msvc_version, basestring):
		msvc_version = int(float(msvc_version))
	else:
		msvc_version = 11

	if msvc_version == 11:
		qt_base = 'msvc2012_64'
	elif msvc_version == 12:
		qt_base = 'msvc2013_64'
	elif msvc_version == 14:
		qt_base = 'msvc2015_64'
	else:
		self.fatal('[ERROR] Unable to find QT build to use for MSVC %d' % msvc_version)

	qt_version = QT_MAJOR_VERSION + '.' + QT_MINOR_VERSION
	qt_root = os.path.join(QT_ROOT, qt_version, qt_base, *subpaths)
	
	try:	
		return self.CreateRootRelativePath(qt_root)
	except:
		return self.bld.CreateRootRelativePath(qt_root)

def get_debug_postfix(configuration):
	return 'd' if configuration == 'debug' else ''

def get_qt_path(self):
	return get_qt_root_path(self, 'Qt')

def get_pyside_path(self):
	return get_qt_root_path(self, 'PySide')

def collect_qt_properties(self):

	# QT specific settings, should be better to move to configure step
	qt_root = get_qt_path(self)
	qt_includes_folder = os.path.join(qt_root, 'include')
	qt_libs_folder = os.path.join(qt_root, 'lib')
	qt_bin_folder = os.path.join(qt_root, 'bin')

	defines = [ 'QT_GUI_LIB', 'QT_NO_EMIT', 'QT_WIDGETS_LIB' ]
	includes = [ qt_includes_folder ]
	libpath = [ qt_libs_folder ]	
	
	if hasattr(self.env['CONFIG_OVERWRITES'], self.target):
		configuration = self.env['CONFIG_OVERWRITES'][self.target]
	else:
		configuration =  self.env['CONFIGURATION']
		
	# Collect module libs	
	libs = []
	debug_postfix = get_debug_postfix(configuration)
	qt_modules_include_path_base = qt_includes_folder + '/Qt'
	qt_lib_prefix = 'Qt' + QT_MAJOR_VERSION	
	for module in qt_modules:	
		# Collect include path
		includes += [qt_modules_include_path_base + module] # e.g. .../include/QtNetwork		
		
		# Collect lib and binary
		lib_name = '%s%s%s' % (qt_lib_prefix, module, debug_postfix)
		libs += [lib_name]

	# Save core binaries
	# Note: icudt has special treatment since it's shared between release and debug configurations (as of v55 at least)
	binaries = []
	for binary in qt_core_binaries:
		binaries += ['%s%s' % (binary, debug_postfix if binary != 'icudt' else '')]
	
	return (defines, includes, libpath, libs, binaries)

@conf
def configure_qt(conf):
	v = conf.env
	v['MOC'] = os.path.join(get_qt_path(conf), 'bin', 'moc.exe')
	v['MOC_PCH'] = ''
	v['MOC_ST'] = '-o'
	v['MOC_CPPPATH_ST'] = '-I"%s"'
	v['MOC_DEFINES_ST'] = '-D%s'
	v['MOC_PCH_ST'] = '-b"%s"'

@feature('qt')
@before_method('process_source')
def add_qt_header_source(self):
	""" 
	Projects with the "QT" feature should treat their header files as source files.
	The header files should be picked up and mocced by the extension mappings system.
	"""
	self.source += self.to_nodes(getattr(self, 'header_source', []))
		
class qt_moc(Task.Task):
	
	scan    = c_preproc.scan
	
	def __init__(self, *k, **kw):
		Task.Task.__init__(self, *k, **kw)
		self.h_node = None
				
	def runnable_status(self):
	
		for t in self.run_after:
			if not t.hasrun:
				return Task.ASK_LATER
				
		# Check if moc file exists
		if not os.path.isfile(self.inputs[0].abspath()) or not os.path.isfile(self.outputs[0].abspath()):
			return Task.RUN_ME
		
		status = Task.Task.runnable_status(self)
		
		if status == Task.SKIP_ME:
			if os.stat(self.outputs[0].abspath()).st_size == 0:
				self.remove_obj_from_linker() # remove empty moc file from linker
				
		return status
	
	def run (self):
		env = self.env
		
		cmd = '"%s" %s %s %s %s %s %s' % (
			env['MOC'],
			' '.join(env['MOC_FLAGS']),
			' '.join([env['MOC_CPPPATH_ST'] % x for x in env['INCPATHS'] if os.path.abspath(x).startswith(self.generator.bld.CreateRootRelativePath('Code')) and not os.path.abspath(x).startswith(self.generator.bld.CreateRootRelativePath('Code/SDKs'))]),
			' '.join([env['MOC_DEFINES_ST'] % x for x in env['DEFINES']]),
			env['MOC_PCH_ST'] % env['MOC_PCH'] if env['MOC_PCH'] else '',
			'%s"%s"' % (env['MOC_ST'], self.inputs[0].abspath()),
			'"%s"' % self.h_node.abspath() )

		# Run moc command
		ret = 0
		try:
			(out,err) = self.generator.bld.cmd_and_log(cmd, output=Context.BOTH, quiet=Context.BOTH)
		except Exception as e:
			out = e.stdout
			err = e.stderr
			ret = e.returncode		

		# Combine stdout and stderr 
		# Note: Incredibuild will return stderr via stdout whereas local builds keep the streams seperate	
		out_str = out + err	
		link_output = True
			
		if ret != 0:
			self.err_msg = '<++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++>\n'	
			self.err_msg += "Compilation failed - File: %r, Module: %r, Configuration: '%s|%s', error code %d\n" % (os.path.basename(self.outputs[0].abspath()), self.generator.target, self.generator.bld.env['PLATFORM'], self.generator.bld.env['CONFIGURATION'], ret )
			self.err_msg += "\tInput Files:   '%s'\n" % ', '.join(i.abspath() for i in self.inputs)
			self.err_msg += "\tOutput Files:  '%s'\n" % (', ').join(i.abspath() for i in self.outputs)
			self.err_msg += "\tCommand:       '%s'\n" % ''.join(cmd)
			self.err_msg += "\tOutput:\n%s" % out_str
			self.err_msg += "<++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++>\n"
			link_output = False
		# Check if file needed to be mocced. Otherwise return and strip file from linker			
		#elif out_str.rstrip().endswith('No output generated.'):  # commented out as IB 9.0 has a bug where it adds "Start{{xgTaskID=00000000}}" to the end of the line
		elif 'No output generated.' in out_str:
			link_output = False			
			
		if not link_output:
			open(self.outputs[0].abspath(), 'wb' ).close() # create dummy file to make waf post_run step happy
			self.remove_obj_from_linker()
			return ret # early out
		
		if out_str:
			self.generator.bld.to_log('MOC: %s' % out_str)

		# Run cxx command
		tsk = Task.classes['cxx'](env=self.env, generator=self.generator)
		tsk.set_inputs(self.inputs[0])
		tsk.set_outputs(self.outputs[0])		
		ret = tsk.run()
		
		# Copy error message if there is any
		if getattr(tsk, "err_msg", None):
			self.err_msg = tsk.err_msg						
		return ret		
		
	def remove_obj_from_linker(self):		
		if getattr(self.generator, 'link_task', None):
			with self.generator.link_inputs_lock:
				if self.outputs[0] in self.generator.link_task.inputs:				
					self.generator.link_task.inputs.remove(self.outputs[0])

@feature('qt')
@after_method('apply_link')
def apply_qt(self):
	lst = []
	for flag in self.to_list(self.env['CXXFLAGS']):
		if len(flag) < 2: continue
		f = flag[0:2]
		if f in ('-D', '-I', '/D', '/I'):
			if (f[0] == '/'):
				lst.append('-' + flag[1:])
			else:
				lst.append(flag)		
	self.env.append_value('MOC_FLAGS', lst)	

@feature('qt')
@before_method('apply_incpaths')
def apply_qt_flags(self):
	(defines, includes, libpath, lib, binaries) = collect_qt_properties(self)	
					
	# Add defines and include path for QT
	self.env['DEFINES'] += defines
	if not hasattr(self, 'includes'):
		self.includes = []
	self.includes = includes + self.includes 
	
	# Add Linker Flags for QT	      
	self.env['LIB'] += lib
	self.env['LIBPATH'] += libpath
	
	# lock for generators link_task.inputs (so that tasks can remove unneeded .obj from the linker)
	self.link_inputs_lock = threading.Lock()
		
@extension('.h')
def create_qt_moc_task(self, node):

	if self.env['PLATFORM'] == 'project_generator':
		return
	
	if not 'qt' in getattr(self, 'features', []):
		return

	# Check for moc folder
	moc_file_folder = self.bld.get_bintemp_folder_node()
	moc_file_folder = moc_file_folder.make_node('moc_files').make_node(self.target)			
	Utils.check_dir(moc_file_folder.abspath())	
	
	# Check for PCH file
	pch = ''
	if hasattr(self, 'pch_name'):
		pch = self.pch_name.replace('.cpp', '.h')
	elif hasattr(self, 'pch'):		
		pch = self.pch.replace('.cpp', '.h')
		
	# Create moc task and store in list to create a dependency on the link_task later
	moc_cxx_file = moc_file_folder.make_node(node.change_ext('_moc.cpp').name)
	moc_task = self.create_compiled_task('qt_moc', moc_cxx_file)
	moc_task.h_node = node
	moc_task.inputs.append(node)
	moc_task.env['MOC_PCH'] = pch
	
class XMLHandler(ContentHandler):
	"""
	Parser for *.qrc* files
	"""
	def __init__(self):
		self.buf = []
		self.files = []
	def startElement(self, name, attrs):
		if name == 'file':
			self.buf = []
	def endElement(self, name):
		if name == 'file':
			self.files.append(str(''.join(self.buf)))
	def characters(self, cars):
		self.buf.append(cars)
		
@extension('.qrc')
def create_rcc_task(self, node):
	"Create rcc and cxx tasks for *.qrc* files"
	rcnode = node.change_ext('_rc.cpp')
	rcctask = self.create_task('rcc', node, rcnode)
	rcctask.disable_pch = True
	rcctask.env.append_value( 'QT_RCC', os.path.join(get_qt_path(self), 'bin', 'rcc.exe') )
	cpptask = self.create_task('cxx', rcnode, rcnode.change_ext('.o'))
	cpptask.disable_pch = True
	try:
		self.compiled_tasks.append(cpptask)
	except AttributeError:
		self.compiled_tasks = [cpptask]
	return cpptask	
	
class rcc(Task.Task):
	"""
	Process *.qrc* files
	"""
	color   = 'BLUE'
	run_str = '${QT_RCC} -name ${SRC[0].name} ${SRC[0].abspath()} ${RCC_ST} -o ${TGT}'
	ext_out = ['.h']

	def scan(self):
		"""Parse the *.qrc* files"""
		node = self.inputs[0]

		if not xml_sax:
			Logs.error('no xml support was found, the rcc dependencies will be incomplete!')
			return ([], [])

		parser = make_parser()
		curHandler = XMLHandler()
		parser.setContentHandler(curHandler)
		fi = open(self.inputs[0].abspath(), 'r')
		try:
			parser.parse(fi)
		finally:
			fi.close()

		nodes = []
		names = []
		root = self.inputs[0].parent
		for x in curHandler.files:
			nd = root.find_resource(x)
			if nd: nodes.append(nd)
			else: names.append(x)
		return (nodes, names)

@feature('copy_qt_binaries')
@run_once
def copy_qt_binaries(self):

	if self.env['PLATFORM'] == 'project_generator':
		return

	if not 'msvc' in (self.env.CC_NAME, self.env.CXX_NAME) or self.env['MSVC_TARGETS'][0] != 'x64':
		return
		
	if hasattr(self.env['CONFIG_OVERWRITES'], self.target):
		configuration = self.env['CONFIG_OVERWRITES'][self.target]
	else:
		configuration =  self.env['CONFIGURATION']
	
	bld	= self.bld
	platform	= bld.env['PLATFORM']
	output_folder = bld.get_output_folders(platform, configuration)[0]
	qt_root_path = get_qt_path(self)
	qt_root_node = bld.root.make_node(qt_root_path)
	
	# Copy qt plugins	
	debug_postfix = get_debug_postfix(configuration)
	for qt_plugin in qt_plugins.keys():
		qt_plugin_node = qt_root_node.make_node('/plugins/' + qt_plugin)		
		output_folder_plugin = output_folder.make_node(qt_plugin)
		for plugin_name in qt_plugins[qt_plugin]:
			file = plugin_name + debug_postfix + '.dll'
			self.create_task('copy_outputs', qt_plugin_node.make_node(file), output_folder_plugin.make_node(file))
			pdb = plugin_name + debug_postfix + '.pdb'
			if os.path.isfile(os.path.join(qt_root_path, 'plugins', qt_plugin, pdb)):
				self.create_task('copy_outputs', qt_plugin_node.make_node(pdb), output_folder_plugin.make_node(pdb))

	# Copy QT core binaries	
	(defines, includes, libpath, libs, binaries) = collect_qt_properties(self)
	qt_binaries = libs + binaries

	qt_bin_node = qt_root_node.make_node('bin')
	for file_name in os.listdir(qt_bin_node.abspath()):
		file_base, file_exension = os.path.splitext(file_name)
		
		# Special case qt icu binaries which carry version number in name
		# e.g. icutest53.dll
		# Note: we can't just strip the last few characters since there may be a 'd' affix for debug libraries
		if file_base.startswith('icu'):
			file_base = ''.join(c for c in file_base if not c.isdigit())

		pdb_name = file_base + '.pdb'
		pdb_path = os.path.join(qt_root_path, 'bin', pdb_name)
			
		# Copy binaries
		if file_base in qt_binaries:
			self.create_task('copy_outputs', qt_bin_node.make_node(file_name), output_folder.make_node(file_name))
			if os.path.isfile(pdb_path):
				self.create_task('copy_outputs', qt_bin_node.make_node(pdb_name), output_folder.make_node(pdb_name))

@feature('copy_pyside')
@run_once
def copy_pyside(self):
	bld = self.bld
	platform = bld.env['PLATFORM']
	configuration = bld.env['CONFIGURATION']

	if platform == 'project_generator':
		return
	pyside_src_path_str = os.path.join(get_pyside_path(bld), 'PySide2')
	pyside_folder = bld.root.make_node(pyside_src_path_str)
	pyside_uic_foler = bld.root.make_node(os.path.join(get_pyside_path(bld), 'pyside2uic'))
	bin_output_folder = bld.get_output_folders(platform, configuration)[0]
	

	if not os.path.exists(pyside_folder.abspath()):
		Logs.warn('[WARNING] Pyside folder not found: ' + pyside_folder.abspath())
		return
	dll_postfilx = '-dbg' if configuration == 'debug' else ''
	dlls_to_copy = ["shiboken2-python2.7%s" % dll_postfilx, "pyside2-python2.7%s" % dll_postfilx]

	qt_dlls = []

	for module in qt_modules:
		qt_dlls.append(module.lower())

	pyside_output_folder = bin_output_folder.make_node('PySide2')
	for file_name in os.listdir(pyside_folder.abspath()):
		file_base, file_extension = os.path.splitext(file_name)

		# Copy scripts folder
		if file_base == 'scripts':
			for script_name in os.listdir(os.path.join(pyside_folder.abspath(), file_base)):
				self.create_task('copy_outputs', pyside_folder.make_node(file_base + '/' + script_name), pyside_output_folder.make_node(file_base + '/' + script_name))

		# Ignore folders
		if not file_extension:
			continue

		if file_extension == ".pyd" or file_extension == ".py":
			# Ignore Pyside modules for Qt libraries we're not using.
			last_index = len(file_base)
			if file_base.endswith("_d"):
				last_index -= 2
			if file_name.startswith('Qt') and not file_base[2:last_index].lower() in qt_dlls:
				continue
			# Copy to PySide2 folder in bin
			self.create_task('copy_outputs', pyside_folder.make_node(file_name), pyside_output_folder.make_node(file_name))
			pdb_name = file_base + '.pdb'
			pdb_path = os.path.join(pyside_src_path_str, pdb_name)
			if os.path.isfile(pdb_path):
				self.create_task('copy_outputs', pyside_folder.make_node(pdb_name), pyside_output_folder.make_node(pdb_name))
		elif (file_extension == ".dll" or file_extension == ".pdb") and file_base in dlls_to_copy:
			# Copy DLLs directly to bin
			self.create_task('copy_outputs', pyside_folder.make_node(file_name), bin_output_folder.make_node(file_name))

	pyside_uic_output_folder = bin_output_folder.make_node('pyside2uic')
	for file_name in os.listdir(pyside_uic_foler.abspath()):
		file_base, file_extension = os.path.splitext(file_name)

		# Copy subfolders
		if file_base in ('Compiler', 'port_v2', 'widget-plugins'):
			for script_name in os.listdir(os.path.join(pyside_uic_foler.abspath(), file_base)):
				self.create_task('copy_outputs', pyside_uic_foler.make_node(file_base + '/' + script_name), pyside_uic_output_folder.make_node(file_base + '/' + script_name))

		# Ignore folders
		if not file_extension:
			continue

		self.create_task('copy_outputs', pyside_uic_foler.make_node(file_name), pyside_uic_output_folder.make_node(file_name))
			
############ DEPRECATED ############
@extension('.ui')
def create_uic_task(self, node):
	if not self.env['PLATFORM'] == 'project_generator':				
		Logs.warn('[WARNING] QT .ui files are deprecated. Please replace: ' + node.name)
		
	uictask = self.create_task('ui4', node)
	uictask.outputs = [self.path.find_or_declare('ui_%s.h' % node.name[:-3])]
	uictask.env.append_value( 'QT_UIC', os.path.join(get_qt_path(self), 'bin', 'uic.exe') )
	
	
class ui4(Task.Task):
	"""
	Process *.ui* files
	"""
	color   = 'BLUE'
	run_str = '${QT_UIC} ${SRC} -o ${TGT}'
	ext_out = ['.h']
