# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.


###############################################################################
from waflib.Configure import conf
from waflib.TaskGen import feature, before_method, after_method
from waflib import Utils, Logs, Errors
import os
import copy
import re	
import itertools
from collections import Iterable
from collections import Counter
from waflib import Context
import collections

def _to_list( value ):
	""" Helper function to ensure a value is always a list """
	if isinstance(value,list):
		return value
	return [ value ]

def _preserved_order_remove_duplicates(seq):
	seen = set()
	seen_add = seen.add
	return [ x for x in seq if not (x in seen or seen_add(x))]
		
@conf
def GetConfiguration(ctx, target):	
	if target in ctx.env['CONFIG_OVERWRITES']:		
		return ctx.env['CONFIG_OVERWRITES'][target]
	return ctx.env['CONFIGURATION']
	
@conf
def get_platform_list(self, platform):
	"""
	Util-function to find valid platform aliases for the current platform
	"""
	supported_platforms = self.get_supported_platforms()

	# Create all possible platform subsets e.g. ['android', 'android_arm', 'android_arm_gcc']
	# For all possible platforms
	if platform in ('project_generator', 'cppcheck', []): # if empty platform or on of the other in the list
		ret_value = set()
		for concrete_platform in supported_platforms:
			platform_name = ""
			for platform_subset in concrete_platform.split('_'):
				platform_name = '%s_%s' % (platform_name, platform_subset) if platform_name else platform_subset
				ret_value.add(platform_name)
		ret_value = _preserved_order_remove_duplicates(list(ret_value)) # strip duplicate
		
	# For the given platform
	else:
		ret_value = []
		platform_name = ""
		for platform_subset in platform.split('_'):
			platform_name = '%s_%s' % (platform_name, platform_subset) if platform_name else platform_subset
			if any(platform_name in s for s in supported_platforms):
				ret_value += [platform_name]

	return ret_value	
	
def SanitizeInput(ctx, kw):
	"""
	Util-function to sanitize all inputs: add missing fields and make sure all fields are list
	"""
	def _SanitizeInput(kw, entry):
		"""
		Private Helper function to sanitize a single entry
		"""	
		assert( isinstance(entry, str) )
		if not entry in kw:
			kw[entry] = []

		if not isinstance(kw[entry],list):
			kw[entry] = [ kw[entry] ]

	for entry in 'output_file_name files additional_settings use_module export_definitions meta_includes file_list winres_includes winres_defines defines features java_source lib libpath linkflags use cflags cxxflags includes framework module_extensions additional_manifests'.split():
	
		# Base entry
		_SanitizeInput(kw, entry)
		
		# By Configuration entry
		for conf in ctx.get_supported_configurations():
			_SanitizeInput(kw, conf + '_' + entry)
		
		# By Platform entry
		platform_list = ctx.get_platform_list( [] )
		for platform in platform_list:
			_SanitizeInput(kw, platform + '_' + entry)

		# By Platform,Configuration entry
		for platform in platform_list:
			for conf in ctx.get_supported_configurations():
				_SanitizeInput(kw, platform + '_' + conf + '_' + entry)

	# Process module-provided settings
	for setting in kw.setdefault('module_provides', dict()).keys():
		_SanitizeInput(kw['module_provides'], setting)
		
	# Recurse for additional settings
	for setting in kw['additional_settings']:
		SanitizeInput(ctx, setting)
		
@Utils.memoize
def _scan_module_folder(root):			
	root_len = len(root)
	relative_file_paths = collections.OrderedDict()
	os_path_join = os.path.join
	
	for path, subdirectories, files in os.walk(root):	
		relative_path = path[root_len+1:]
		
		# Do not parse folders that have a wscript as they define their own modules
		if relative_path  and 'wscript' in files:
			continue
						
		relative_file_paths[relative_path if relative_path else 'Root'] = [os.path.join(relative_path,file_name) for file_name in files]	
		
	return {"auto_gen_uber.cpp":  relative_file_paths  }
		
@Utils.memoize_ignore_first_arg
def _scan_waf_file(ctx, absolute_file_path):
		
	def _extract_file_lists(raw_file_list, filter_name_prefix = ""):		
		# [Recursive]
		# Loop over items in uber_file list
		# 1) If item is a file, add to file set for current filter
		# 2) If item is a filter, recurse call this function
		def _extract_files_and_filters(parent_uber_file, filter_items, filter_name_prefix):
			for (child_filter_name, child_filter_items) in filter_items.iteritems():				
				final_filter_name = (filter_name_prefix + '/' + child_filter_name) if filter_name_prefix else child_filter_name
				filter_file_list = parent_uber_file.setdefault(final_filter_name, set())
				for entry in child_filter_items:
					if isinstance(entry, dict):
						_extract_files_and_filters(parent_uber_file, entry, final_filter_name)
					else:
						filter_file_list.add(entry)
	
		# Loop over uber file list
		uber_files = {}	
		for (uber_file_name, uber_file_items) in raw_file_list.items():
			uber_file_entry = uber_files.setdefault(uber_file_name, {})
			_extract_files_and_filters(uber_file_entry, uber_file_items, "")
		return uber_files
	
	root_node = absolute_file_path
	waf_file_json = ctx.parse_json_file(root_node)	
	return _extract_file_lists(waf_file_json)		
	
def _scan_waf_files(ctx, file_node_list):

	def _merge_file_lists(in_0, in_1):	
		""" Merge two file lists """
		result = dict(in_0)
		
		for (uber_file,project_filter) in in_1.iteritems():
			for (filter_name,file_list) in project_filter.iteritems():				
				for file in file_list:				
					if not uber_file in result:						
						result[uber_file] = {}					
					if not filter_name in result[uber_file]:						
						result[uber_file][filter_name] = []									
					result[uber_file][filter_name].append(file)	
		return result

	final_file_list = {}		
	for waf_file in file_node_list:
		final_file_list = _merge_file_lists(final_file_list, _scan_waf_file(ctx, waf_file))
		
	return final_file_list
	
@conf
def GetModuleFileList(ctx, kw):		

	file_list = {} 
	
	# Get  files by recursivly scanning the current wscript path
	if kw.get('auto_detect_files', False):
		file_list =  _scan_module_folder(ctx.path.abspath())
		
	target = kw['target']
	platform = ctx.env['PLATFORM']
	configuration = ctx.GetConfiguration(target)
	
	# Read x.waf_file(s)
	# Get files from xxx.waf_file(s) from file_list entry
	waf_files = [ctx.path.make_node(waf_file) for waf_file in GetFlattendPlatformSpecificSetting(ctx, kw, 'file_list', platform, configuration)]
	file_list.update(_scan_waf_files(ctx, waf_files))
	
	return file_list
		
def RegisterVisualStudioFilter(ctx, kw):
	"""
	Util-function to register each provided visual studio filter parameter in a central lookup map
	"""
	if not 'vs_filter' in kw:
		ctx.fatal('Mandatory "vs_filter" task generater parameter missing in %s/wscript' % ctx.path.abspath())
		
	if not hasattr(ctx, 'vs_project_filters'):
		ctx.vs_project_filters = {}
			
	ctx.vs_project_filters[ kw['target' ] ] = kw['vs_filter']	
	
def AssignTaskGeneratorIdx(ctx, kw):
	"""
	Util-function to assing a unique idx to prevent concurrency issues when two task generator output the same file.
	"""
	if not hasattr(ctx, 'index_counter'):		ctx.index_counter = 0
	if not hasattr(ctx, 'index_map'):
		ctx.index_map = {}
		
	# Use a path to the wscript and the actual taskgenerator target as a unqiue key
	key = ctx.path.abspath() + '___' + kw['target']
	
	if key in ctx.index_map:
		kw['idx'] = ctx.index_map.get(key)
	else:
		ctx.index_counter += 1	
		kw['idx'] = ctx.index_map[key] = ctx.index_counter
		
	kw['features'] += ['parse_vcxproj']
		
def SetupRunTimeLibraries(ctx, kw, overwrite_settings = None):
	"""
	Util-function to set the correct flags and defines for the runtime CRT (and to keep non windows defines in sync with windows defines)
	By default CryEngine uses the "Multithreaded, dynamic link" variant (/MD)
	"""
	runtime_crt = 'dynamic'						# Global Setting
	if overwrite_settings:						# Setting per Task Generator Type
		runtime_crt = overwrite_settings
	if kw.get('force_static_crt', False):		# Setting per Task Generator
		runtime_crt = 'static'
	if kw.get('force_dynamic_crt', False):		# Setting per Task Generator
		runtime_crt = 'dynamic'
		
	if runtime_crt != 'static' and runtime_crt != 'dynamic':
		ctx.fatal('Invalid Settings: "%s" for runtime_crt' % runtime_crt )
		
	crt_flag = []
	config = ctx.GetConfiguration(kw['target'])			

	if runtime_crt == 'static':
		kw['defines'] 	+= [ '_MT' ]
		if ctx.env['CC_NAME'] == 'msvc':
			if config == 'debug':
				crt_flag = [ '/MTd' ]
			else:
				crt_flag	= [ '/MT' ]			
	else: # runtime_crt == 'dynamic':
		kw['defines'] 	+= [ '_MT', '_DLL' ]	
		if ctx.env['CC_NAME'] == 'msvc':
			if config == 'debug':
				crt_flag = [ '/MDd' ]
			else:
				crt_flag	= [ '/MD' ]		
	
	kw['cflags']	+= crt_flag
	kw['cxxflags']	+= crt_flag
	
	
def GetFlattendPlatformSpecificSetting(ctx, kw, entry, platform, configuration):
	
	# Get global entry (Note: This is already flattened for none-project_generator builds for the active platform and configuration)
	ret_list = list(kw[entry])
	for setting in kw['additional_settings']:
		ret_list += setting[entry]
		
	# Flatten platform specific setting into the list
	if platform == 'project_generator' and not ctx.cmd == 'generate_uber_files':
		for key, value in  ctx.GetPlatformSpecificSettings(kw, entry, platform, configuration, True).iteritems():
			ret_list += value
		for setting in kw['additional_settings']:
			for key, value in  ctx.GetPlatformSpecificSettings(setting, entry, platform, configuration, True).iteritems():
				ret_list += value
			
	ret_list = _preserved_order_remove_duplicates(ret_list)
	return ret_list
	
def TrackFileListChanges(ctx, kw):
	"""
	Util function to ensure file lists are correctly tracked regardless of current target platform
	"""	
	files_to_track = []
	kw['waf_file_entries'] = []
	target = kw['target']
	platform = ctx.env['PLATFORM']
	configuration = ctx.GetConfiguration(target)
	
	files_to_track = GetFlattendPlatformSpecificSetting(ctx,kw, 'file_list', platform, configuration)

	if files_to_track:
		if not hasattr(ctx, 'addional_files_to_track'):
			ctx.addional_files_to_track = []	
			
		# Remove duplicates
		files_to_track = list(set(files_to_track)) 	
	
		# Add results to global lists
		for file in files_to_track:
			file_node = ctx.path.make_node(file)				
			ctx.addional_files_to_track += [ file_node ]		
			kw['waf_file_entries'] += [ file_node ]

def LoadFileLists(ctx, kw):
	"""
	Util function to extract a list of needed source files, based on uber files and current command
	It expects that kw['file_list'] points to a valid file, containing a JSON file with the following mapping:
	Dict[ <UberFile> -> Dict[ <Project Filter> -> List[Files] ] ]
	"""			
	def _DisableUberFile(ctx, project_filter_list, files_marked_for_exclusion):
		for (filter_name, file_list) in project_filter_list.items():				
			if any(ctx.path.make_node(file).abspath().lower() in files_marked_for_exclusion for file in file_list): # if file in exclusion list			
				return True
		return False
				
	task_generator_files		= set() # set of all files in this task generator (store as abspath to be case insenstive)
	
	file_to_project_filter	= {}
	uber_file_to_file_list	= {}
	file_list_to_source			= {}
	
	source_files						= []
	no_uber_file_files			= []
	header_files						= []
	darwin_source_files			= []
	java_source_files       = []
	qt_source_files					= []
	resource_files					= []
	swig_files							= []
	protobuf_files					= []
	other_files							= []
	uber_file_relative_list	= []
	
	target 									= kw['target']
	found_pch								= False
	pch_file								= kw.get('pch', '')
	platform 								= ctx.env['PLATFORM']
	uber_file_folder 				= ctx.root.make_node(Context.launch_dir).make_node('BinTemp/uber_files/' + target)

	# Apply project override
	disable_uber_files_for_project = ctx.get_project_overrides(target).get('exclude_from_uber_file', False)
											
	files_marked_for_uber_file_exclusion = []	
	if not disable_uber_files_for_project:
		for key, value in ctx.get_file_overrides(target).iteritems():
			if value.get('exclude_from_uber_file', False):
				files_marked_for_uber_file_exclusion.append(key)
				
	# Build various mappings/lists based in file just
	file_list = GetModuleFileList(ctx, kw)
	for (uber_file, project_filter_list) in file_list.iteritems():
		# Disable uber file usage if defined by override parameter
		disable_uber_file = disable_uber_files_for_project or _DisableUberFile(ctx, project_filter_list, files_marked_for_uber_file_exclusion)
		
		if disable_uber_file:
			Logs.debug('[Option Override] - %s - Disabled uber file "%s"' %(target, uber_file))
			
		generate_uber_file = uber_file != 'NoUberFile' and not disable_uber_file
		if generate_uber_file:
			# Collect Uber file related informations	
			uber_file_node 			= uber_file_folder.make_node(uber_file)			
			uber_file_relative 	= uber_file_node.path_from(ctx.path)
							
			task_generator_files.add(uber_file_node.abspath().lower())
			uber_file_relative_list						+= [ uber_file_relative ]
			file_to_project_filter[uber_file_node.abspath()] = 'Uber Files'
		
		for (filter_name, file_entries) in project_filter_list.items():				
			for file in file_entries:
				file_node = ctx.path.make_node(file)
				
				task_generator_files.add(file_node.abspath().lower())
				# Collect per file information		
				if file == pch_file:
					# PCHs are not compiled with the normal compilation, hence don't collect them
					found_pch = True							
					
				elif file.endswith('.c') or file.endswith('.C'):
					source_files 									+= [ file ]
					if not generate_uber_file:
						no_uber_file_files 					+= [ file ]
						
				elif file.endswith('.cpp') or file.endswith('.CPP'):
					source_files 									+= [ file ]
					if not generate_uber_file:
						no_uber_file_files 					+= [ file ]

				elif 'android' in ctx.env['PLATFORM'] and file.endswith('.s'): #TODO this should have a real handling for assembly files
					source_files 								+= [ file ]
					no_uber_file_files 					+= [ file ]
					
				elif file.endswith('.java'):
					java_source_files 						+= [ file ]
						
				elif file.endswith('.mm'):
					darwin_source_files 					+= [ file ]
					
				elif file.endswith('.ui') or file.endswith('.qrc'):
					qt_source_files 							+= [ file ]
				
				elif file.endswith('.h') or file.endswith('.H'):
					header_files 									+= [ file ]
				elif file.endswith('.hpp') or file.endswith('.HPP'):
					header_files 									+= [ file ]
				
				elif file.endswith('.rc') or file.endswith('.r'):
					resource_files								+= [ file ]
					
				elif file.endswith('.i') or file.endswith('.swig'):
					swig_files										+= [ file ]
					
				elif file.endswith('.proto'):
					protobuf_files								+= [ file ]
					
				else:
					other_files										+= [ file ]
					
				# Build file name -> Visual Studio Filter mapping
				file_to_project_filter[file_node.abspath()] = filter_name

				# Build list of uber files to files
				if generate_uber_file:
					uber_file_abspath = uber_file_node.abspath()
					if not uber_file_abspath in uber_file_to_file_list:
						uber_file_to_file_list[uber_file_abspath]  	= []
					uber_file_to_file_list[uber_file_abspath] 		+= [ file_node ]
			
	if file_list:
		# Compute final source list based on platform	
		if platform == 'project_generator' or ctx.options.file_filter != "":			
			# Collect all files plus uber files for project generators and when doing a single file compilation
			kw['source'] = uber_file_relative_list + source_files + qt_source_files + darwin_source_files + java_source_files + header_files + resource_files + swig_files + protobuf_files + other_files
			if platform == 'project_generator' and pch_file != '':
				kw['source'] += [ pch_file ] # Also collect PCH for project generators

		elif platform == 'cppcheck':
			# Collect source files and headers for cpp check
			kw['source'] = source_files + header_files
			
		else:
			# Regular compilation path
			if ctx.is_option_true('use_uber_files'):
				# Only take uber files when uber files are enabled and files not using uber files
				kw['source'] = uber_file_relative_list + no_uber_file_files + qt_source_files + swig_files + protobuf_files
			else:
				# Fall back to pure list of source files
				kw['source'] = source_files + qt_source_files + swig_files + protobuf_files
				
			kw['header_source'] = header_files
				
			# Append platform specific files
			platform_list = ctx.get_platform_list( platform )
			if 'darwin' in platform_list:
				kw['source'] += darwin_source_files
			if 'win' in platform_list:
				kw['source'] += resource_files
			kw['java_source'] = java_source_files

		# Handle PCH files
		if pch_file != '' and found_pch == False:
			# PCH specified but not found
			ctx.cry_file_error('[%s] Could not find PCH file "%s" in provided file list (%s).\nPlease verify that the name of the pch is the same as provided in a WAF file and that the PCH is not stored in an UberFile.' % (kw['target'], pch_file, ', '.join(file_lists)), 'wscript' )
		
		# Try some heuristic when to use PCH files
		if ctx.is_option_true('use_uber_files') and found_pch and len(uber_file_relative_list) > 0 and ctx.options.file_filter == "" and ctx.cmd != 'generate_uber_files':
			# Disable PCH files when having UberFiles as they  bring the same benefit in this case
			kw['pch_name'] = kw['pch']
			del kw['pch']
			
	else:
		if not 'source' in kw:
			kw['source'] = []
		
	# Store global lists in context	
	kw['task_generator_files'] 	= task_generator_files
	kw['file_list_content'] 		= file_list
	kw['project_filter'] 				=	file_to_project_filter
	kw['uber_file_lookup'] 			= uber_file_to_file_list
	
def VerifyInput(ctx, kw):
	"""
	Helper function to verify passed input values
	"""
	wscript_file = ctx.path.make_node('wscript').abspath()		
	if 'source' in kw:
		ctx.cry_file_error('TaskGenerator "%s" is using unsupported parameter "source", please use "file_list"' % kw['target'], wscript_file )

def InitializeTaskGenerator(ctx, kw):
	""" 
	Helper function to call all initialization routines requiered for a task generator
	"""		
	RegisterCryModule(ctx, kw)
	SanitizeInput(ctx, kw)
	VerifyInput(ctx, kw)
	AssignTaskGeneratorIdx(ctx, kw)
	RegisterVisualStudioFilter(ctx, kw)	
	TrackFileListChanges(ctx, kw)
	
target_modules = {}
def RegisterCryModule(ctx, kw):
	if 'target' not in kw:
		Logs.warn('Module in %s does not specify a target' % ctx.path.abspath())
		return
	target_modules[kw['target']] = kw
	
def ConfigureModuleUsers(ctx, kw):
	"""
	Helper function to maintain a list of modules use by this task generator
	"""
	if not hasattr(ctx, 'cry_module_users'):
		ctx.cry_module_users = {}
	
	for lib in kw['use_module']:		
		if not lib  in ctx.cry_module_users:
			ctx.cry_module_users[lib] = []
		ctx.cry_module_users[lib] += [ kw['target'] ]	
	
def ConfigureOutputFileOverrideModules(ctx,kw):
	"""
	Helper function to maintain a list of modules that have a different output_file_name than their target name
	"""
	if not hasattr(ctx, 'cry_module_output_file_name_overrides'):
		ctx.cry_module_output_file_name_overrides = {}
		
	if kw['output_file_name']:
		out_file_name = kw['output_file_name'][0] if isinstance(kw['output_file_name'],list) else kw['output_file_name']
		ctx.cry_module_output_file_name_overrides[kw['target']] =  out_file_name	

def AddModuleToPackage(ctx, kw):
	# Package the output of the following module (only used for android right now)
	if not hasattr(ctx, 'deploy_modules_to_package'):
		ctx.deploy_modules_to_package = []

	ctx.deploy_modules_to_package += [kw['target']]
	
def LoadAdditionalFileSettings(ctx, kw):
	"""
	Load all settings from the addional_settings parameter, and store them in a lookup map
	"""
	kw['features'] 							+= [ 'apply_additional_settings' ]	
	kw['file_specifc_settings'] = {}
	
	for setting in kw['additional_settings']:
				
		setting['target'] = kw['target'] # reuse target name
								
		file_list = []
					
		if 'files' in setting:		
			# Option A: The files are already specified as an list
			file_list += setting['files']			
			
		if 'regex' in setting:
			# Option B: A regex is specifed to match the files			
			p = re.compile(setting['regex'])

			for file in kw['source']: 
				if p.match(file):
					file_list += [file]		

		# insert files into lookup dictonary, but make sure no uber file and no file within an uber file is specified
		uber_file_folder = ctx.bldnode.make_node('..')
		uber_file_folder = uber_file_folder.make_node('uber_files')
		uber_file_folder = uber_file_folder.make_node(kw['target'])
			
		for file in file_list:
			file_abspath 			= ctx.path.make_node(file).abspath()			
			uber_file_abspath = uber_file_folder.make_node(file).abspath()

			if 'uber_file_lookup' in kw:
				for uber_file in kw['uber_file_lookup']:

					# Uber files are not allowed for additional settings
					if uber_file_abspath == uber_file:
						ctx.cry_file_error("To ensure consistent behavior, Additional File Settings are not supported for files in UberFiles - please adjust your setup" % file, ctx.path.make_node('wscript').abspath())
						
					for entry in kw['uber_file_lookup'][uber_file]:						
						if file_abspath == entry.abspath():
							ctx.cry_file_error("To ensure consistent behavior, Additional File Settings are not supported for files using UberFiles (%s) - please adjust your setup" % file, ctx.path.make_node('wscript').abspath())
							
			# All fine, add file name to dictonary
			kw['file_specifc_settings'][file_abspath] = setting
			setting['source'] = []
			
def ApplyMonolithicBuildSettings(ctx, kw):	

	# Add collected settings to link task	
	kw['use']  += ctx.monolitic_build_settings['use']
	kw['use_module']  += ctx.monolitic_build_settings['use_module']	
	kw['lib']  += ctx.monolitic_build_settings['lib']
	kw['libpath']    += ctx.monolitic_build_settings['libpath']
	kw['linkflags']  += ctx.monolitic_build_settings['linkflags']
	
	# Add game specific files
	prefix = kw['project_name'] + '_'
	kw['use']  += ctx.monolitic_build_settings[prefix + 'use']
	kw['use_module'] += ctx.monolitic_build_settings[prefix + 'use_module']
	kw['lib']  += ctx.monolitic_build_settings[prefix + 'lib']
	kw['libpath']    += ctx.monolitic_build_settings[prefix + 'libpath']
	kw['linkflags']  += ctx.monolitic_build_settings[prefix + 'linkflags']
	
	kw['use']  = _preserved_order_remove_duplicates(kw['use'])
	kw['use_module'] = _preserved_order_remove_duplicates(kw['use_module'])
	kw['lib']  = _preserved_order_remove_duplicates(kw['lib'])
	kw['libpath']   = _preserved_order_remove_duplicates(kw['libpath'])
	kw['linkflags'] = _preserved_order_remove_duplicates(kw['linkflags'])
	
@conf
def PostprocessBuildModules(ctx, *k, **kw):
	if hasattr(ctx, '_processed_modules'):
		return
	ctx._processed_modules = True
	platform = ctx.env['PLATFORM']
	
	if not ctx.env['PLATFORM'] == 'project_generator' and not ctx.cmd == 'generate_uber_files' and not ctx.cmd == 'configure':	
		for (target_module, kw_target_module) in target_modules.items():
			if not BuildTaskGenerator(ctx, kw_target_module):
				continue
			# Configure the modules users for static libraries
			if _is_monolithic_build(ctx, kw_target_module['target']) and kw_target_module.get('is_monolithic_host', None):
				ApplyMonolithicBuildSettings(ctx, kw_target_module)
				ConfigureModuleUsers(ctx,kw_target_module)

	for (target_module, kw_target_module) in target_modules.items():
		target = kw_target_module['target']
		configuration = ctx.GetConfiguration(target)
		update_settings = False

		for module in kw_target_module['use_module']:
			if module in target_modules:
				provides = target_modules[module]['module_provides']

				# Apply all keys to target module
				for key in provides:					
					# If the module does not have a sanitized key, it is not interesting for this platform and thus can be disguarded
					try:
						kw_target_module[key] += provides[key]
						update_settings = True
					except:
						pass
			else:
				ctx.fatal('Error module "%s" in module\'s "%s" "use_module" entry has not been registered for this platform|configuration "%s|%s" or does not exist.' % (module, target_module, platform, configuration))

		if update_settings:
			CollectPlatformSpecificSettings(ctx, kw_target_module, platform, configuration)
	
@conf
def ApplyPlatformSpecificSettings(ctx, kw, platform, configuration, spec):
	# First apply spec specific platform entries
	
	if platform == 'project_generator' and not ctx.cmd == 'generate_uber_files': # Get info when looping over all projects and configuration 
		return
	
	if not ctx.cmd == 'generate_uber_files': # generate_uber_files files does not know the concept of specs
		ApplySpecSpecificSettings(ctx, kw, platform, configuration, spec)	
	
	ApplyPlatformSpecificModuleExtension(ctx, kw, platform, configuration) # apply prior to ApplyPlatformSpecificModuleExtension() as it modifies the entry list on a per platform basis
	CollectPlatformSpecificSettings(ctx, kw, platform, configuration)	# will flatten all platform specific entries into a single entry (except when in project_generation mode)

def ConfigureTaskGenerator(ctx, kw):
	"""
	Helper function to apply default configurations and to set platform/configuration dependent settings
	"""	
	target = kw['target']
	spec = ctx.options.project_spec
	platform = ctx.env['PLATFORM']
	configuration = ctx.GetConfiguration(target)
	
	# Apply all settings, based on current platform and configuration
	ApplyConfigOverwrite(ctx, kw)
	ctx.ApplyPlatformSpecificSettings(kw, platform, configuration, spec)	
	ApplyBuildOptionSettings(ctx, kw)
	
	LoadFileLists(ctx, kw)
	
	LoadAdditionalFileSettings(ctx, kw)

	# Configure the modules users for static libraries
	if not _is_monolithic_build(ctx, target):
		ConfigureModuleUsers(ctx,kw)
	
	# Configure modules that have a different module output_file_name than their target name
	ConfigureOutputFileOverrideModules(ctx,kw)
		
	# Handle meta includes for WinRT
	for meta_include in kw.get('meta_includes', []):
		kw['cxxflags'] += [ '/AI' + meta_include ]		
	
	# Handle export definitions file	
	kw['linkflags'] 	+= [ '/DEF:' + ctx.path.make_node( export_file ).abspath() for export_file in kw['export_definitions']]
	
	# Generate output file name (without file ending), use target as an default if nothing is specified
	if kw['output_file_name'] == []:
		kw['output_file_name'] = target
	elif isinstance(kw['output_file_name'],list):
		kw['output_file_name'] = kw['output_file_name'][0] # Change list into a single string

	# Handle force_disable_mfc (Small Hack for Perforce Plugin (no MFC, needs to be better defined))
	if kw.get('force_disable_mfc', False) and '_AFXDLL' in kw['defines']:
		kw['defines'].remove('_AFXDLL')


def MonolithicBuildModule(ctx, *k, **kw):
	""" 
	Util function to collect all libs and linker settings for monolithic builds
	(Which apply all of those only to the final link as no DLLs or libs are produced)
	"""
	# Set up member for monolithic build settings
	if not hasattr(ctx, 'monolitic_build_settings'):
		ctx.monolitic_build_settings = {}
		
	# For game specific modules, store with a game unique prefix
	prefix = ''
	if kw.get('game_project', False):		
		prefix = kw['game_project'] + '_'

	# Collect libs for later linking
	def _append(key, values):
		if not ctx.monolitic_build_settings.get(key):
			ctx.monolitic_build_settings[key] = []
		ctx.monolitic_build_settings[key] += values
		
	_append(prefix + 'use',         [ kw['target']] + kw['use'] )
	_append(prefix + 'use_module',    kw['use_module']  )
	_append(prefix + 'lib',           kw['lib'] )
	_append(prefix + 'libpath',       kw['libpath'] )
	_append(prefix + 'linkflags',     kw['linkflags'] )
	
	# Remove use and use_module as passed on to the monolithic parent_uber_file
	kw['use'] = []
	kw['use_module'] = []	

	# Adjust own task gen settings
	# When compiling as monolithic, set this define so modules are aware of this
	# This should (eventually) replace most uses of _LIB in the code base
	kw['defines'] += [ '_LIB', 'CRY_IS_MONOLITHIC_BUILD' ]

	# Remove rc files from the soures for monolithic builds (only the rc of the launcher will be used)
	tmp_src = []		
	for file in kw['source']:		
		if not file.endswith('.rc'):
			tmp_src += [file]
	kw['source'] = tmp_src		
		
	return ctx.objects(*k, **kw)
	
def MonolithicBuildStaticModule(ctx, *k, **kw):
	""" 
	Util function to collect all libs and linker settings for monolithic builds
	(Which apply all of those only to the final link as no DLLs or libs are produced)
	"""
	# Set up member for monolithic build settings
	if not hasattr(ctx, 'monolitic_build_settings'):
		ctx.monolitic_build_settings = {}
		
	# For game specific modules, store with a game unique prefix
	prefix = ''
	if kw.get('game_project', False):		
		prefix = kw['game_project'] + '_'

	# Collect libs for later linking
	def _append(key, values):
		if not ctx.monolitic_build_settings.get(key):
			ctx.monolitic_build_settings[key] = []
		ctx.monolitic_build_settings[key] += values
		
	_append(prefix + 'use',         [ kw['target']] + kw['use'] )	
	_append(prefix + 'lib',           kw['lib'] )
	_append(prefix + 'libpath',       kw['libpath'] )
	#_append(prefix + 'use_module',    kw['use_module']  )
	#_append(prefix + 'linkflags',     kw['linkflags'] )
		
###############################################################################
def BuildTaskGenerator(ctx, kw):
	"""
	Check if this task generator should be build at all in the current configuration
	"""
	target = kw['target']
	spec = ctx.options.project_spec
	platform = ctx.env['PLATFORM']
	configuration = ctx.GetConfiguration(target)
			
	if ctx.cmd == 'configure':
		return False 		# Dont build during configure
		
	if ctx.cmd == 'generate_uber_files':
		ctx(features='generate_uber_file', uber_file_list=kw['file_list_content'], target=target, pch=kw.get('pch', ''))
		return False 		# Dont do the normal build when generating uber files
		
	if ctx.env['PLATFORM'] == 'cppcheck':
		ctx(features='generate_uber_file', to_check_sources = kw['source_files'] + kw['header_files'], target=target)
		return False		# Dont do the normal build when running cpp check

	# Always include all projects when generating project for IDEs
	if ctx.env['PLATFORM'] == 'project_generator':
		return True
	
	if not spec:
		return False
			
	if target in ctx.spec_modules():
		return True		# Skip project is it is not part of the current spec
			
	return False
			
@feature('apply_additional_settings')
@before_method('extract_vcxproj_overrides')
def tg_apply_additional_settings(self):
	""" 
	Apply all settings found in the addional_settings parameter after all compile tasks are generated 
	"""
	if len(self.file_specifc_settings) == 0:		
		return # no file specifc settings found

	for t in getattr(self, 'compiled_tasks', []):
		input_file = t.inputs[0].abspath()

		file_specific_settings = self.file_specifc_settings.get(input_file, None)
		
		if not file_specific_settings:
			continue
			
		t.env['CFLAGS'] 	+= file_specific_settings.get('cflags', [])
		t.env['CXXFLAGS'] += file_specific_settings.get('cxxflags', [])
		t.env['DEFINES'] 	+= file_specific_settings.get('defines', [])
		
		for inc in file_specific_settings.get('defines', []):
			if os.path.isabs(inc):
				t.env['INCPATHS'] += [ inc ]
			else:
				t.env['INCPATHS'] += [ self.path.make_node(inc).abspath() ]
	
###############################################################################
def set_cryengine_flags(ctx, kw):

	kw['includes'] = [ 
		'.', 
		ctx.CreateRootRelativePath('Code/SDKs/boost'), 
		ctx.CreateRootRelativePath('Code/CryEngine/CryCommon'),
        ctx.CreateRootRelativePath('Code/CryEngine/CryCommon/3rdParty')
		] + kw['includes']
		
###############################################################################
@conf
def CryEngineModule(ctx, *k, **kw):
	"""
	CryEngine Modules are mostly compiled as dynamic libraries
	Except the build configuration requieres a monolithic build
	"""
	# Initialize the Task Generator
	InitializeTaskGenerator(ctx, kw)	

	# Setup TaskGenerator specific settings	
	set_cryengine_flags(ctx, kw)
	SetupRunTimeLibraries(ctx,kw)	
	if hasattr(ctx, 'game_project'):
		kw['game_project'] = ctx.game_project	
		
	ConfigureTaskGenerator(ctx, kw)

	if not BuildTaskGenerator(ctx, kw):
		return
	
	if _is_monolithic_build(ctx, kw['target']): # For monolithc builds, simply collect all build settings
		if not kw.get('is_package_host', False):	
			MonolithicBuildModule(ctx, getattr(ctx, 'game_project', None), *k, **kw)
			return
		else:
			kw['defines'] += [ '_LIB', 'CRY_IS_MONOLITHIC_BUILD' ]
			kw['is_monolithic_host'] = True
			active_project_name = ctx.spec_game_projects(ctx.game_project)
			
			if len (active_project_name) != 1:
				Logs.warn("Multiple project names found: %s, but only one is supported for android - using '%s'." % (active_project_name, active_project_name[0]))
				active_project_name = active_project_name[0]
				
			kw['project_name'] = "".join(active_project_name)
		
	kw['features'] 			+= [ 'generate_rc_file' ]		# Always Generate RC files for Engine DLLs
	if ctx.env['PLATFORM'] == 'darwin_x64':
		kw['mac_bundle'] 		= True										# Always create a Mac Bundle on darwin	

	AddModuleToPackage(ctx, kw)
	ctx.shlib(*k, **kw)

@conf
def CreateDynamicModule(ctx, *k, **kw):
	"""
	Builds a non-CRYENGINE dynamic library (for use by 3rd Party libraries)
	"""
	InitializeTaskGenerator(ctx, kw)
	SetupRunTimeLibraries(ctx,kw)
	ConfigureTaskGenerator(ctx, kw)
	if not BuildTaskGenerator(ctx, kw):
		return

	AddModuleToPackage(ctx, kw)
	ctx.shlib(*k, **kw)

def CreateStaticModule(ctx, *k, **kw):
	if ctx.cmd == 'configure':
		return
	
	if ctx.cmd == 'generate_uber_files':
		ctx(features='generate_uber_file', uber_file_list=kw['file_list_content'], target=kw['target'], pch=kw.get('pch', ''))
		return
		
	# Setup TaskGenerator specific settings	
	kw['kw'] 								= kw 
	kw['base_name'] 				= kw['target']
	kw['original_features'] = kw['features']
	kw['features'] 					= ['create_static_library']
	
	# And ensure that we run before all other tasks
	ctx.set_group('generate_static_lib')	
	ctx(*k, **kw)
	
	# Set back to default grp
	ctx.set_group('regular_group')
	
###############################################################################
@conf
def CryEngineStaticModule(ctx, *k, **kw):
	"""
	CryEngine Static Modules are compiled as static libraries
	Except the build configuration requieres a monolithic build
	"""	
	# Initialize the Task Generator
	InitializeTaskGenerator(ctx, kw)
	
	# Static modules are still valid module users in monolithic builds 	
	ConfigureModuleUsers(ctx, kw)
	
	set_cryengine_flags(ctx, kw)

	ConfigureTaskGenerator(ctx, kw)
	
	if _is_monolithic_build(ctx, kw['target']): # For monolithc builds, simply collect all build settings
		MonolithicBuildStaticModule(ctx, getattr(ctx, 'game_project', None), *k, **kw)
		
	CreateStaticModule(ctx, *k, **kw)

###############################################################################
@feature('create_static_library')
@before_method('process_source')
def tg_create_static_library(self):
	"""
	This function resolves the 'use_module' directives, and transforms them to basic WAF 'use' directives.
	Unlike a normal 'use', this function will use certain settings from the consuming projects that will be "inherited".
	Thus, it's possible to have one module be instantiated for two distinct projects (ie, different CRT settings or different APIs).
	An example here is that certain DCC tools and RC will link with /MT instead of /MD, which would otherwise conflict.
	The "inherited" settings are defined in a white-list.
	"""
	if self.bld.env['PLATFORM'] == 'project_generator':
		return

	# Utility to perform whitelist filtering
	# Returns the intersection of the two lists
	def _filter(unfiltered, allowed):
		result = []
		for item in unfiltered:
			if item in allowed:
				result += [ item ]
		return result

	# Walks the use_module tree and collects the leaf nodes
	# This finds the WAF projects that (directly or indirectly) use the project 'node', and adds them to the 'out' list
	def _recursive_collect_leaves(bld, node, out, depth):
		node_users = bld.cry_module_users.get(node, [])
		if depth > 100:
			fatal('Circular dependency introduced, including at least module' + self.target)
		if len(node_users) == 0:
			if not node in out and depth != 1: #depth != 1: Do not build a static library that is not referenced by anything
				out += [ node ]
		else:
			for user in node_users:
				_recursive_collect_leaves(bld, user, out, depth + 1)

	# 1. Pass: Find the candidate projects to which this module should be applied
	# For each module that uses this module, a set of properties that is inherited from the consuming project is computed
	# Each distinct set of properties will result in one "instantiation" of the module
	module_list = []
	_recursive_collect_leaves(self.bld, self.target, module_list, 1)

	tg_user_settings = []
	for module in module_list:
		tg_settings = {}

		if not module in self.bld.spec_modules():
			continue # Skip if the user is not configurated to be build in current spec
			
		# Get other task generator
		other_tg = self.bld.get_tgen_by_name(module)		
		
		# We will only use a subset of the settings of a user task generator
		for entry in 'defines cxxflags cflags'.split():
			if not entry in tg_settings:
				tg_settings[entry] = []

			# Filter the settings from the consumer of this module using the white-list
			filter_list = getattr(self.env, 'module_whitelist_' + entry, [])
			unfiltered_values = getattr(other_tg, entry, [])
			filtered_values = _filter(unfiltered_values, filter_list)

			tg_settings[entry] += filtered_values
		
		# Try to find a match for those settings
		bAddNewEntry = True
		for entry in tg_user_settings:	
			
			def _compare_settings():
				""" Peform a deep compare to ensure we have the same settings """
				for field in 'defines cxxflags cflags'.split():
					listA = entry['settings'][field]
					listB = tg_settings[field]
					if len(listA) != len(listB):
						return False
					listA.sort()
					listB.sort()
					for i in range(len(listA)):
						if listA[i] != listB[i]:
							return False
				return True
						
					
			if _compare_settings():
				entry['users'] += [ other_tg.name ] # Found an entry, just add ourself to users list
				bAddNewEntry = False
				break
				
		if bAddNewEntry == True:
			new_entry = {}
			new_entry['users'] = [ other_tg.name ]
			new_entry['settings'] = tg_settings
			tg_user_settings.append( new_entry )

	# 2. Pass for each user set, compute a MD5 to identify it and create new task generators
	for entry in tg_user_settings:
		
		def _compute_settings_md5(settings):
			""" Util function to compute the MD4 of all settings for a tg """
			keys = list(settings.keys())
			keys.sort()
			tmp = ''
			for k in keys:
				values = settings[k]
				values.sort()
				tmp += str([k, values])			
			return Utils.md5(tmp.encode()).hexdigest().upper()
		
		settings_md5 = _compute_settings_md5(entry['settings'])
		
		target = self.target + '.' + settings_md5
		if not 'clean_' in self.bld.cmd: # Dont output info when doing a clean build
			Logs.info("[WAF] Creating Static Library '%s' for module: '%s', SettingsHash: '%s'" % (self.target, ', '.join(entry['users']), settings_md5)	)
		
		
		kw = {}
		# Deep copy all source values
		for (key,value) in self.kw.items():
			if key == 'env':
				kw['env'] = value.derive()
				kw['env'].detach()
			else:
				kw[key] = copy.copy(value)				
			
		# Append relevant users settings to ensure a consistent behavior (runtime CRT, potentially global defines)
		for (key, value) in entry['settings'].items():
			kw[key] += value
			
		# Patch fields which need unique values:
		kw['target'] = target
		kw['output_file_name'] = target		
		kw['idx'] = int(settings_md5[:8],16)	# Use part of the  MD5 as the per taskgenerator filename to ensure resueable object files
		kw['_type'] = 'stlib'
		kw['features'] = kw['original_features']
		
		# Tell the users to link this static library
		for user in entry['users']:
			user_tg = self.bld.get_tgen_by_name(user)
			user_tg.use += [ target ]
		
		# Ensure that we are executed
		if self.bld.targets != '' and not target in self.bld.targets:
			self.bld.targets += ',' + target
		
		# Enforce task generation for new tg		
		if _is_monolithic_build(self.bld, target):
			tg = MonolithicBuildModule(self.bld, **kw) #TODO this must get the settings from ther user in regards of game projects
		else:
			kw['features'] += ['c', 'cxx', 'cstlib', 'cxxstlib' ]			
			tg = self.bld(**kw)
			
		tg.path = self.path		# Patch Path
		tg.post()

	self.source = [] # Remove all source entries to prevent building the source task generator
	
###############################################################################
@conf
def CryLauncher(ctx, *k, **kw):
	"""
	Wrapper for CryEngine Executables
	"""
	if not ctx.env['PLATFORM']:
		if not BuildTaskGenerator(ctx, kw):
			# Initialize the Task Generator
			InitializeTaskGenerator(ctx, kw)	
		
			# Setup TaskGenerator specific settings	
			set_cryengine_flags(ctx, kw)
			SetupRunTimeLibraries(ctx,kw)
			kw.setdefault('win_linkflags', []).extend(['/SUBSYSTEM:WINDOWS'])
			
			ConfigureTaskGenerator(ctx, kw)
			return
		
	# Create multiple projects for each Launcher, based on the number of active projects in the current spec
	if ctx.env['PLATFORM'] == 'project_generator': 	# For the project generator, just use the first project (doesnt matter which project)
		active_projects = [ ctx.game_projects()[0] ]
	else: # Only use projects for current spec
		active_projects = ctx.spec_game_projects()
		
	exectuable_name_clash = Counter( [ ctx.get_executable_name(project) for project in active_projects ] )
		
	orig_target = kw['target']
	counter = 1
	num_active_projects = len(active_projects)
	for project in active_projects:		
		
		kw_per_launcher = kw if counter == num_active_projects else copy.deepcopy(kw)
		kw_per_launcher['target'] = orig_target if num_active_projects == 1 else kw_per_launcher['target'] + '_' + project
		ctx.set_spec_modules_name_alias(orig_target, kw_per_launcher['target'])
		
		# Initialize the Task Generator
		InitializeTaskGenerator(ctx, kw_per_launcher)	
					
		# Setup TaskGenerator specific settings	
		set_cryengine_flags(ctx, kw_per_launcher)
		SetupRunTimeLibraries(ctx,kw_per_launcher)	
		
		ConfigureTaskGenerator(ctx, kw_per_launcher)
		
		if not BuildTaskGenerator(ctx, kw):
			return
	
		kw_per_launcher['idx'] = kw_per_launcher['idx'] + (1000 * (ctx.project_idx(project) + 1));		
		
		# Setup values for Launcher Projects
		kw_per_launcher['features'] 			+= [ 'generate_rc_file' ]	
		kw_per_launcher['is_launcher'] 			= True
		kw_per_launcher['is_package_host'] 		= True
		kw_per_launcher['resource_path'] 		= ctx.launch_node().make_node(ctx.game_code_folder(project) + '/Resources')
		kw_per_launcher['project_name'] 		= project
		executable_name = ctx.get_executable_name(project)
		
		# Avoid two project with the same executable name to link to the same output_file_name simultaneously. 
		# Two link.exe writing to the same file in parallel does not work.
		if exectuable_name_clash[executable_name] > 1:
			Logs.warn('Warning: (%s) : Multiple project resolving to the same executable name:"%s. Using "project name" as "executable name" to avoid multiple linker writing to the same file simultaneously.' % (project, executable_name) )
			executable_name = project
			
		kw_per_launcher['output_file_name'] 	= executable_name
		
		if _is_monolithic_build(ctx, kw_per_launcher['target']):	
			kw_per_launcher['defines'] += [ '_LIB', 'CRY_IS_MONOLITHIC_BUILD' ]
			kw_per_launcher['is_monolithic_host'] = True
		
		ctx.program(*k, **kw_per_launcher)
		counter += 1
	
	
###############################################################################
@conf
def CryDedicatedServer(ctx, *k, **kw):
	"""	
	Wrapper for CryEngine Dedicated Servers
	"""	
	if not ctx.env['PLATFORM']:
		if not BuildTaskGenerator(ctx, kw):
			# Initialize the Task Generator
			InitializeTaskGenerator(ctx, kw)	
		
			# Setup TaskGenerator specific settings	
			set_cryengine_flags(ctx, kw)
			SetupRunTimeLibraries(ctx,kw)
			kw.setdefault('win_linkflags', []).extend(['/SUBSYSTEM:WINDOWS'])
			
			ConfigureTaskGenerator(ctx, kw)
			return
	
	# Create multiple projects for each Launcher, based on the number of active projects in the current spec
	if ctx.env['PLATFORM'] == 'project_generator': 	# For the project generator, just use the first project (doesnt matter which project)
		active_projects = [ ctx.game_projects()[0] ]
	else: #  Only use projects for current spec
		active_projects = ctx.spec_game_projects()

	exectuable_name_clash = Counter( [ ctx.get_dedicated_server_executable_name(project) for project in active_projects ] )

	orig_target = kw['target']
	counter = 1
	num_active_projects = len(active_projects)
	for project in active_projects:	
	
		kw_per_launcher = kw if counter == num_active_projects else copy.deepcopy(kw)
		kw_per_launcher['target'] = orig_target if num_active_projects == 1 else kw_per_launcher['target'] + '_' + project
		ctx.set_spec_modules_name_alias(orig_target, kw_per_launcher['target'])
		
		# Initialize the Task Generator
		InitializeTaskGenerator(ctx, kw_per_launcher)	
	
		# Setup TaskGenerator specific settings	
		set_cryengine_flags(ctx, kw_per_launcher)
		SetupRunTimeLibraries(ctx,kw_per_launcher)
		kw_per_launcher.setdefault('win_linkflags', []).extend(['/SUBSYSTEM:WINDOWS'])
		
		ConfigureTaskGenerator(ctx, kw_per_launcher)
		
		if not BuildTaskGenerator(ctx, kw):
			return
		
		kw_per_launcher['idx'] = kw_per_launcher['idx'] + (1000 * (ctx.project_idx(project) + 1));		
		
		kw_per_launcher['features'] 					+= [ 'generate_rc_file' ]
		kw_per_launcher['is_dedicated_server']			= True
		kw_per_launcher['is_package_host'] 	            = True
		kw_per_launcher['resource_path'] 				= ctx.launch_node().make_node(ctx.game_code_folder(project) + '/Resources')
		kw_per_launcher['project_name'] 				= project
		executable_name = ctx.get_dedicated_server_executable_name( project )
		
		# Avoid two project with the same executable name to link to the same output_file_name simultaneously. 
		# Two link.exe writing to the same file in parallel does not work.
		if exectuable_name_clash[executable_name] > 1:
			Logs.warn('Warning: (%s) : Multiple project resolving to the same executable name:"%s. Using "project name" as "executable name" to avoid multiple linker writing to the same file simultaneously.' % (project, executable_name) )
			executable_name = project + '_Server'
			
		kw_per_launcher['output_file_name'] 	= executable_name
		
		if _is_monolithic_build(ctx, kw_per_launcher['target']):
			kw_per_launcher['defines'] += [ '_LIB', 'CRY_IS_MONOLITHIC_BUILD' ]
			kw_per_launcher['is_monolithic_host'] = True

		ctx.program(*k, **kw_per_launcher)
		counter += 1
	
###############################################################################
@conf
def CryConsoleApplication(ctx, *k, **kw):
	"""
	Wrapper for CryEngine Executables
	"""
	# Initialize the Task Generator
	InitializeTaskGenerator(ctx, kw)	

	# Setup TaskGenerator specific settings	
	set_cryengine_flags(ctx, kw)
	SetupRunTimeLibraries(ctx,kw)
	kw.setdefault('win_linkflags', []).extend(['/SUBSYSTEM:CONSOLE'])
	
	ConfigureTaskGenerator(ctx, kw)
		
	if not BuildTaskGenerator(ctx, kw):
		return
		
	ctx.program(*k, **kw)
	
###############################################################################
@conf
def CryFileContainer(ctx, *k, **kw):	
	"""
	Function to create a header only library
	"""
	# Initialize the Task Generator
	InitializeTaskGenerator(ctx, kw)	

	# Setup TaskGenerator specific settings
	ConfigureTaskGenerator(ctx, kw)
		
	if not BuildTaskGenerator(ctx, kw):
		return
        
	if ctx.env['PLATFORM'] == 'project_generator':
		ctx.shlib(*k, **kw)	
		
		
###############################################################################
@conf
def CryEditor(ctx, *k, **kw):
	"""
	Wrapper for CryEngine Editor Executables
	"""
	# Initialize the Task Generator
	InitializeTaskGenerator(ctx, kw)	

	# Setup TaskGenerator specific settings	
	ctx.set_editor_flags(kw)
	SetupRunTimeLibraries(ctx,kw)
	
	kw['use']  +=['EditorCommon']
			
	kw['features'] += [ 'generate_rc_file' ]
	kw['cxxflags'] += ['/EHsc', '/GR', '/bigobj', '/Zm200', '/wd4251', '/wd4275' ]
	kw['defines']  += [ 'SANDBOX_EXPORTS', 'PLUGIN_IMPORTS', 'EDITOR_COMMON_IMPORTS' ]
	kw.setdefault('win_linkflags', []).extend(['/SUBSYSTEM:WINDOWS'])
	
	ConfigureTaskGenerator(ctx, kw)
		
	if not BuildTaskGenerator(ctx, kw):
		return
		
	ctx.program(*k, **kw)
	
	###############################################################################
@conf
def CryGenericExecutable(ctx, *k, **kw):
	"""
	Wrapper for CryEngine Generic Executables
	"""
	# Initialize the Task Generator
	InitializeTaskGenerator(ctx, kw)	

	# Setup TaskGenerator specific settings	
	SetupRunTimeLibraries(ctx,kw)
	ConfigureTaskGenerator(ctx, kw)
		
	if not BuildTaskGenerator(ctx, kw):
		return
		
	ctx.program(*k, **kw)
	
###############################################################################
@conf
def CryPlugin(ctx, *k, **kw):
	"""
	Wrapper for CryEngine Editor Plugins
	"""
	# Initialize the Task Generator
	InitializeTaskGenerator(ctx, kw)	

	# Setup TaskGenerator specific settings	
	ctx.set_editor_module_flags(kw)
	
	SetupRunTimeLibraries(ctx,kw)
	kw['cxxflags'] += ['/EHsc', '/GR', '/wd4251', '/wd4275']
	kw['defines']   += [ 'PLUGIN_EXPORTS', 'EDITOR_COMMON_IMPORTS', 'NOT_USE_CRY_MEMORY_MANAGER' ]
	kw['output_sub_folder']  = 'EditorPlugins'
	kw['use']  += ['EditorCommon']
	kw['includes'] +=  [ctx.CreateRootRelativePath('Code/Sandbox/EditorQt/Include'), ctx.CreateRootRelativePath('Code/Sandbox/EditorQt')]
		
	ConfigureTaskGenerator(ctx, kw)
		
	if not BuildTaskGenerator(ctx, kw):
		return
		
	ctx.shlib(*k, **kw)
	
###############################################################################
@conf
def CryPluginModule(ctx, *k, **kw):
	"""
	Wrapper for CryEngine Editor Plugins Util dlls, those used by multiple plugins
	"""
	# Initialize the Task Generator
	InitializeTaskGenerator(ctx, kw)	

	# Setup TaskGenerator specific settings	
	ctx.set_editor_module_flags(kw)
	SetupRunTimeLibraries(ctx,kw)
	kw['cxxflags'] += [ '/EHsc', '/GR', '/wd4251', '/wd4275' ]
	kw['defines']  += [ 'PLUGIN_EXPORTS', 'NOT_USE_CRY_MEMORY_MANAGER' ]
	
	ConfigureTaskGenerator(ctx, kw)
		
	if not BuildTaskGenerator(ctx, kw):
		return
		
	kw['features'] += ['create_dynamic_library']
		
	ctx.shlib(*k, **kw)
	

###############################################################################
@conf
def CryResourceCompiler(ctx, *k, **kw):
	"""
	Wrapper for RC
	"""
	# Initialize the Task Generator
	InitializeTaskGenerator(ctx, kw)	

	# Setup TaskGenerator specific settings	
	SetupRunTimeLibraries(ctx,kw, 'static')
	
	ctx.set_rc_flags(kw)	
	kw['output_file_name'] 	= 'rc'
	kw['output_sub_folder'] = ctx.CreateRootRelativePath('Tools/rc')
	
	kw.setdefault('win_linkflags', []).extend(['/SUBSYSTEM:CONSOLE'])
	
	ConfigureTaskGenerator(ctx, kw)

	if not BuildTaskGenerator(ctx, kw):
		return

	ctx.program(*k, **kw)

###############################################################################
@conf
def CryResourceCompilerModule(ctx, *k, **kw):
	"""
	Wrapper for RC modules
	"""
	# Initialize the Task Generator
	InitializeTaskGenerator(ctx, kw)	

	# Setup TaskGenerator specific settings	
	SetupRunTimeLibraries(ctx,kw, 'static')
	ctx.set_rc_flags(kw)
	kw['output_sub_folder'] = ctx.CreateRootRelativePath('Tools/rc')
	kw.setdefault('win_linkflags', []).extend(['/SUBSYSTEM:CONSOLE'])
	
	ConfigureTaskGenerator(ctx, kw)

	if not BuildTaskGenerator(ctx, kw):
		return

	ctx.shlib(*k, **kw)

###############################################################################
@conf
def CryPipelineModule(ctx, *k, **kw):
	"""
	Wrapper for Pipleine modules (mostly DCC exporters)
	"""
	# Initialize the Task Generator	
	InitializeTaskGenerator(ctx, kw)	

	# Setup TaskGenerator specific settings	
	SetupRunTimeLibraries(ctx,kw, 'static')
	ctx.set_pipeline_flags(kw)
	kw.setdefault('win_linkflags', []).extend(['/SUBSYSTEM:CONSOLE'])
	kw.setdefault('debug_linkflags', []).extend(['/NODEFAULTLIB:libcmt.lib', '/NODEFAULTLIB:msvcprtd.lib'])
	
	ConfigureTaskGenerator(ctx, kw)
		
	if not BuildTaskGenerator(ctx, kw):
		return

	ctx.shlib(*k, **kw)

###############################################################################
# Override lib for targets that have a diffent output_file_name than their target name
@feature('c', 'cxx', 'use')
@after_method('apply_link')
def override_libname(self):
	platform = self.bld.env['PLATFORM']
	spec = self.bld.options.project_spec
	configuration = self.bld.GetConfiguration(self.target)	
	if platform and not platform == 'project_generator' and not self.bld.cmd == 'generate_uber_files':
		for target_module, target_override in self.bld.cry_module_output_file_name_overrides.iteritems():
			if target_module in self.bld.spec_modules(spec, platform, configuration):
				link_task = getattr(self, 'link_task', None)
				if link_task:
					link_task.env['LIB'] = [ target_override if lib == target_module else lib for lib in link_task.env['LIB']]
	
###############################################################################
# Helper function to set Flags based on options
def ApplyBuildOptionSettings(self, kw):
	"""
	Util function to apply flags based on waf options
	"""		
	# Add debug flags if requested
	if self.is_option_true('generate_debug_info'):
		kw['cflags'].extend(self.env['COMPILER_FLAGS_DebugSymbols'])
		kw['cxxflags'].extend(self.env['COMPILER_FLAGS_DebugSymbols'])
		kw['linkflags'].extend(self.env['LINKFLAGS_DebugSymbols'])

	# Add show include flags if requested	
	if self.is_option_true('show_includes'):		
		kw['cflags'].extend(self.env['SHOWINCLUDES_cflags'])
		kw['cxxflags'].extend(self.env['SHOWINCLUDES_cxxflags'])
	
	# Add preprocess to file flags if requested	
	if self.is_option_true('show_preprocessed_file'):	
		kw['cflags'].extend(self.env['PREPROCESS_cflags'])
		kw['cxxflags'].extend(self.env['PREPROCESS_cxxflags'])	
		self.env['CC_TGT_F'] = self.env['PREPROCESS_cc_tgt_f']
		self.env['CXX_TGT_F'] = self.env['PREPROCESS_cxx_tgt_f']	
	
	# Add disassemble to file flags if requested	
	if self.is_option_true('show_disassembly'):	
		kw['cflags'].extend(self.env['DISASSEMBLY_cflags'])
		kw['cxxflags'].extend(self.env['DISASSEMBLY_cxxflags'])	
		self.env['CC_TGT_F'] = self.env['DISASSEMBLY_cc_tgt_f']
		self.env['CXX_TGT_F'] = self.env['DISASSEMBLY_cxx_tgt_f']
		
	
###############################################################################
# Helper function to extract platform specific flags
@conf
def GetPlatformSpecificSettings(ctx, kw, entry, _platform, _configuration, force_flatten = False):
	"""
	Util function to apply flags based on current platform
	"""
	def _to_list( value ):
		if isinstance(value,list):
			return value
		return [ value ]
	
	def _flatten(coll):
		for i in coll:
			if isinstance(i, Iterable) and not isinstance(i, basestring):
				for subc in _flatten(i):
					yield subc
			else:
				yield i
				
	returnValue = {}	
	platforms 	= ctx.get_platform_list( _platform )

	# If not configuration defined load all configurations
	if not _configuration:
		configurations = ctx.get_supported_configurations()
	else:
		configurations = [_configuration]

	# Check for entry in <platform>_<entry> style
	for platform in platforms:
		platform_entry = platform + '_' + entry 
		if platform_entry in kw:
			if kw[platform_entry]:
				returnValue[platform_entry] = _to_list( kw[platform_entry] )
				
	# Check for entry in <configuration>_<entry> style
	for configuration in configurations:
		configuration_entry = configuration + '_' + entry
		if configuration_entry in kw:
			if kw[configuration_entry]:
				returnValue[configuration_entry] = _to_list( kw[configuration_entry] )
	
	# Check for entry in <platform>_<configuration>_<entry> style
	for platform in platforms:
		for configuration in configurations:
			platform_configuration_entry =   platform + '_' + configuration + '_' + entry
			if platform_configuration_entry in kw:
				if kw[platform_configuration_entry]:
					returnValue[platform_configuration_entry] = _to_list( kw[platform_configuration_entry] )			

	# flatten to a single entry
	if force_flatten or (not _platform == 'project_generator' or ctx.cmd == 'generate_uber_files'):
		returnValue = list(itertools.chain.from_iterable(returnValue.values()))
		_flatten(returnValue)
		returnValue = _preserved_order_remove_duplicates(returnValue) # strip duplicate
		returnValue = {entry : returnValue } # convert to dict 
		
	return returnValue
	
###############################################################################
@conf
def get_platform_permutation_map(ctx, platform):
	platform_permutations 	= ctx.get_platform_list( platform )
	supported_platforms = ctx.get_supported_platforms()
	
	# Map all platform_permutations to supported platforms
	# 'win' -> ['win_x86', 'win_x64']
	palatform_conversion_map = dict()	
	for platform_mutation in platform_permutations:
		matching_platforms = [s for s in supported_platforms if platform_mutation in s]
		palatform_conversion_map[platform_mutation] = matching_platforms
		
	return palatform_conversion_map
	
###############################################################################
# Helper function to apply platform specific module extensions
def ApplyPlatformSpecificModuleExtension(ctx, kw, _platform, _configuration):
	"""
	Util function to apply module extensions to platform and configuration 
	"""
	# This function will only call ctx.execute_module_extension() with supported platforms name e.g. win_x86 NOT win
	# 1) if no permutation or configuration detected, ctx.execute_module_extension() is call with ALL valid supported platforms
	# 2) if a permutation is detected e.g. "win", ctx.execute_module_extension() is call for all matching supported platforms e.g. win_x86 AND win_x64
	# 3) if a configuration only type is detected e.g. "profile", ctx.execute_module_extension() is call with ALL valid supported platforms for this build e.g. win_x86_profile, win_x64_profile, linux_x86_profile,...
	# 4) if a permutation and a configuration type is detected, ctx.execute_module_extension() is called for that individual case e.g. win_x64_profile
	#
	# Note:
	# Case 1) and 2) may result in the same ctx.execute_module_extension(). Where 1) is valid for all platforms and 2) only for the special platform permutation case.
	# Case 3) and 4) may result in the same ctx.execute_module_extension(). Where 3) is valid for all platforms and 4) only for the special platform+console permutation case.
	# However 2) and 4) might have even further module_extensions that should only be executed for that platform compared to 1) and 3)
	#   [valid]   e.g. profile_module_extension = ['SomeExtension'] and win_x64_profile = ['OtherExtension'] and/or win_x64_profile = ['EvenOtherExtension']
	#   [invalid] e.g. profile_module_extension = ['SomeExtension'] and win_x64_profile = ['SomeExtension'] and/or win_x64_profile = ['SomeExtension','OtherExtension']
	
	def _to_list( value ):
		if isinstance(value,list):
			return value
		return [ value ]
		
	supported_platforms = ctx.get_supported_platforms()
	platform_permutations 	= ctx.get_platform_list( _platform )
	palatform_conversion_map = ctx.get_platform_permutation_map(_platform)
		
	# If not configuration or 'project_generator' load all configurations. (project_generator has to know about every potential permutation)
	if not _configuration or _configuration == 'project_generator':
		configurations = ctx.get_supported_configurations()
	else:
		configurations = [_configuration]

	# 1) Check for entry in <entry> style
	for module_extension in _to_list( kw['module_extensions'] ):
		for supported_platform in supported_platforms:
			ctx.execute_module_extension(module_extension, kw, '%s_' %(supported_platform), supported_platform, "")

	# 2) Check for entry in <platform_mutation>_<entry> style
	platform_entry_set = set()
	for platform_mutation in platform_permutations:
		platform_entry = platform_mutation + '_module_extensions'
		if platform_entry in kw:
			for module_extension in _to_list( kw[platform_entry] ):
				for supported_platform in palatform_conversion_map[platform_mutation]:
					ctx.execute_module_extension(module_extension, kw, '%s_' %(supported_platform), supported_platform, "")

	# 3) Check for entry in <configuration>_<entry> style	
	for configuration in configurations:
		configuration_entry = configuration + '_module_extensions'
		if configuration_entry in kw:
			for module_extension in _to_list( kw[configuration_entry] ):
				for supported_platform in supported_platforms:
					ctx.execute_module_extension(module_extension, kw, '%s_%s_' %(supported_platform, configuration), supported_platform, configuration)

	# 4) Check for entry in <platform_mutation>_<configuration>_<entry> style
	for platform_mutation in platform_permutations:
		for configuration in configurations:
			platform_configuration_entry =   platform_mutation + '_' + configuration + '_module_extensions'
			if platform_configuration_entry in kw:
				for module_extension in _to_list( kw[platform_configuration_entry] ):
					for supported_platform in palatform_conversion_map[platform_mutation]:
						ctx.execute_module_extension(module_extension, kw, '%s_%s_' %(supported_platform, configuration), supported_platform, configuration)						

###############################################################################
# Wrapper for ApplyPlatformSpecificFlags for all flags to apply
def CollectPlatformSpecificSettings(ctx, kw, platform, configuration):
	"""
	Check each compiler/linker flag for platform specific additions
	"""
	# handle list entries
	for entry in 'additional_settings use_module export_definitions meta_includes file_list use defines includes cxxflags cflags lib libpath linkflags framework features'.split():
		for key, value in GetPlatformSpecificSettings(ctx, kw, entry, platform, configuration).iteritems():
			kw[key] += value
			if key != "additional_settings":
				kw[key] = _preserved_order_remove_duplicates(kw[key])

	# Handle string entries
	for entry in 'output_file_name'.split():
		for key, value in GetPlatformSpecificSettings(ctx, kw, entry, platform, configuration).iteritems():
			if kw[key] == []: # no general one set
				kw[key] = value

	# Recurse for additional settings
	for setting in kw['additional_settings']:
		CollectPlatformSpecificSettings(ctx, setting, platform, configuration)

###############################################################################
# Set env in case a env overwrite is specified for this project
def ApplyConfigOverwrite(ctx, kw):	
	
	target = kw['target']
	if not target in ctx.env['CONFIG_OVERWRITES']:
		return
		
	platform = ctx.env['PLATFORM']
	overwrite_config = ctx.env['CONFIG_OVERWRITES'][target]
	
	# Need to set crytek specific shortcuts if loading another environment
	ctx.all_envs[platform + '_' + overwrite_config]['PLATFORM'] = platform
	ctx.all_envs[platform + '_' + overwrite_config]['CONFIGURATION'] = overwrite_config
	
	# Create a deep copy of the env for overwritten task to prevent modifying other task generator envs
	kw['env'] = ctx.all_envs[platform + '_' + overwrite_config].derive()
	kw['env'].detach()
	
###############################################################################
def ApplySpecSpecificSettings(ctx, kw, platform, configuration, spec):
	
	if platform == 'project_generator' or platform == []:
		return {} 	# Return only an empty dict when generating a project
		
	# To DO:
	## handle list entries
	#for entry in 'use_module use export_definitions meta_includes file_list  defines includes cxxflags cflags lib libpath linkflags framework features'.split():		
	#	for key, value in GetPlatformSpecificSettings(ctx, kw, entry, platform, configuration).iteritems():
	#		kw[key] += value
	#		
	## Handle string entries
	#for entry in 'output_file_name'.split():
	#	for key, value in GetPlatformSpecificSettings(ctx, kw, entry, platform, configuration).iteritems():
	#		if kw[key] == []: # no general one set
	#			kw[key] = value

	for key, value in ctx.spec_defines(spec, platform, configuration).iteritems():
		kw[key] += value
		
	for key, value in  ctx.spec_module_extensions(spec, platform, configuration).iteritems():
		kw[key] += value
		
	for key, value in  ctx.spec_features(spec, platform, configuration).iteritems():
		kw[key] += value

###############################################################################		
def _is_monolithic_build(ctx, target):
	if ctx.env['PLATFORM'] == 'project_generator' or ctx.cmd == 'generate_uber_files' or ctx.cmd == 'configure':
		return False
		
	spec = ctx.options.project_spec
	platform = ctx.env['PLATFORM']
	configuration = ctx.GetConfiguration(target)		
	
	if ctx.spec_monolithic_build(spec, platform, configuration):
		always_dynamically_linked_modules =	ctx.spec_force_shared_monolithic_build_modules(spec, platform, configuration)
		if target in always_dynamically_linked_modules:
			Logs.info("[Info]: '%s' force linked as shared library as configured for this spec." % target)
			return False
		return True
		
	return False

@feature('copy_bin_output_to_all_platform_output_folders')
@after_method('set_pdb_flags')
@after_method('apply_incpaths')
@after_method('apply_map_file')
def feature_copy_bin_output_to_all_platform_output_folders(self):
	bld 			= self.bld
	platform	= bld.env['PLATFORM']
	configuration = bld.env['CONFIGURATION']
		
	if platform  == 'project_generator' or not bld.options.project_spec: # only support build that have a project spec
		return
	
	if not getattr(self, 'link_task', None):
		return	
		
	if self._type == 'stlib': # Do not copy static libs
		return

	# Copy output file into all configuration folders
	for additional_out_config in self.bld.get_supported_configurations():	
		if additional_out_config != configuration:
			output_folder = self.bld.get_output_folders(platform, additional_out_config)[0]		
			for src in self.link_task.outputs:				
				if not src.abspath().endswith('.manifest'):
					self.create_task('copy_outputs', src, output_folder)
					self.copy_files(src, [output_folder])
					

# Set _USRDLL on shared libraries (DLLs)
# Note: Ideally we would only set this on windows targets, but these names are used on all platforms
@feature('cshlib', 'cxxshlib')
@before_method('process_source')
def apply_shlib(self):
	self.defines = getattr(self, 'defines', []) + [ '_USRDLL' ]

# Set _LIB on static library
# Note: Ideally we would only set this on windows targets, but these names are used on all platforms
@feature('cstlib', 'cxxstlib')
@before_method('process_source')
def apply_stlib(self):
	self.defines = getattr(self, 'defines', []) + [ '_LIB' ]
	

##################################################
@Utils.memoize_ignore_first_arg
def _scan_all_waf_files_in_folder(ctx, path_node):
		
	waf_file_nodes = []
	for root, dirs, files in os.walk(path_node.abspath(), True):
		for file in files:
			if file.endswith('.waf_files'):
				waf_file_nodes += [path_node.make_node(file)]
		break # do not recurse .. 
				
	return _scan_waf_files(ctx, waf_file_nodes) 

@Utils.memoize_ignore_first_arg
def _get_flat_file_list(ctx, waf_file_node):
	return list(itertools.chain.from_iterable(_scan_all_waf_files_in_folder(ctx, waf_file_node).values()[0].values()))
	
@Utils.memoize
def _get_p4_workspace(dummy):
	return raw_input('Enter your p4 workspace (or leave empty to not use P4): ')
		
import subprocess
def _p4_edit_file(path):
	workspace = '--client=' + _get_p4_workspace("dummy")
	subprocess.call(['p4', workspace, 'edit', path])

def _p4_move_file(old_path, new_path):
	workspace = '--client=' + _get_p4_workspace("dummy")
	_p4_edit_file(old_path)
	subprocess.call(['p4', workspace, 'move', old_path, new_path])
	
@conf
def apply_waf_file_mapping_to_folder(ctx, module_path_node):

	use_p4 = True if _get_p4_workspace("dummy") else False
	Logs.info("Starting conversion process...")
		
	abs_root_path = module_path_node.abspath()
	root_len = len(abs_root_path)
	
	os_path_join = os.path.join
	os_path_basename = os.path.basename
	
	move_function = _p4_move_file if use_p4 else  os.rename
	
	waf_file_nodes = []
	for root, dirs, files in os.walk(abs_root_path, True):
		for file in files:
			if file.endswith('.waf_files'):
				waf_file_nodes += [module_path_node.make_node(file)]
		break # do not recurse .. 
				
				
	# Per waf file so we can re-write the file
	for waf_file in waf_file_nodes:
		file_map = _scan_waf_files(ctx, [waf_file]).values()[0]
		
		# Move files physically
		for vs_filter, file_list in file_map.iteritems():
			final_path = os_path_join(abs_root_path,vs_filter)
			
			if vs_filter.lower() == "root":
				continue
			
			if not os.path.exists(final_path):
				os.makedirs(final_path)
				
			for file in file_list:
				filename = os_path_basename(file)
				relative_output_location = os_path_join(vs_filter,filename)			
				if not file == relative_output_location:
					source = os_path_join(abs_root_path, file)
					target = os_path_join(abs_root_path, relative_output_location)
					move_function(source, target)
					Logs.info( 'Moved: "%s" -> "%s"' % (file, relative_output_location))
			
		# Change waf_file in place
		waf_file_path = waf_file.abspath()
		source_file = waf_file_path
		destination_file = waf_file_path+"._bak"
	    
		try:
			os.chmod( destination_file, stat.S_IWRITE )	
			os.remove(destination_file)
		except:
			pass
		
		
		destination = open(destination_file, 'w')
		source = open(source_file, 'r')
		
		destination_has_changed = False
		for line in source.readlines(): # try readlines
			final_line = line
			line = line.strip()
			
			if line and line[0] == '"':
				if not ':' in line:
					filename = line[1: line.find('"', 1)]
					for vs_filter, file_list in file_map.iteritems():
						if vs_filter.lower() == "root":
							continue
						for file in file_list:
							if file == filename:							
								final_line = final_line.replace(filename, os_path_join(vs_filter,  os_path_basename(file)) ).replace('\\', '/')
								destination_has_changed = True
								#Logs.info( 'Found match: "%s" and "%s"' % (file, filename))
								break
							
			destination.write(final_line)
		destination.close()
		source.close()
		
		if destination_has_changed:
			_p4_edit_file(waf_file_path)
			
			try:
				os.chmod( source_file, stat.S_IWRITE )	
				os.remove(source_file)
			except:
				pass

			os.rename( destination_file, source_file)
		else:
			os.remove(destination_file)

import stat
import itertools

conversion_resolved_warning_summary = []
conversion_unresolved_warning_summary = []
	
def convert_source_files_to_waf_file_mapping(ctx, src_waf_file_mapping_module_node, target_mapping_module_node, use_p4 = False ):

	file_path = target_mapping_module_node.abspath()
	if not os.path.isfile(file_path):
		Logs.error("File path does not exist. Unable to convert file to new CryCommon layout (%s) " % file_path)
		return		

	src_file_filter_list = _get_flat_file_list(ctx, src_waf_file_mapping_module_node)
	rel_path_to_crycommon = os.path.relpath(src_waf_file_mapping_module_node.abspath(), file_path).replace("\\", "/")
	
	include_len = len("include")
	import_len = len("import")
	ending_char_map = {'"': '"', "<" : ">"}
	
	filename, file_extension = os.path.splitext(file_path)

	if not file_extension.lower() in ['.c', '.cpp', '.hpp', '.cxx', '.inl', '.h', '.swig', '.i']:
		Logs.info("Skipping: Unsupported file format for file. %s" % file_path)
		return
			 
	source_file = file_path
	destination_file = file_path +"._bak"
	
	try:
		os.chmod( destination_file, stat.S_IWRITE )	
		os.remove(destination_file)
	except:
		pass
		
	destination = open(destination_file, 'w')
	source = open(source_file, 'r')
	
	destination_has_changed = False
	
	for line in source.readlines():
		final_line = line
		line = line.strip()		
		if line:
			line_specifier = line[0]
			if line_specifier == '#' or line_specifier == '%' :
				line = line[1:].lstrip()
				if line.startswith('include') or line.startswith('import'):
					if line.startswith('include'):
						line = line[include_len:].lstrip()
					else:
						line = line[import_len:].lstrip()
					
					# Handle use of gcc #include_next
					if line[0] == "_":
						line = line[len("_next"):].lstrip()
						
					begin_char = line[0]
					
					ending_char = ending_char_map[line[0]]
					filename = line[1: line.find(ending_char, 1)]
					file_base_name = os.path.basename(filename)
					file_base_name_lower = file_base_name.lower()
					potential_matches = []
					
					# Check for local inlcude first
					if begin_char == "\"":
						if os.path.isfile(os.path.join(os.path.dirname(file_path), filename)):
							destination.write(final_line)
							continue
							
					#################
					# SPECIAL CASES
					#################
					# Handle yasli includes
					if filename.startswith("yasli/"):
						destination.write(final_line)
						continue
						
					# check special system files
					if begin_char == "<":					
						exclusion_list = [
						"math.h",
						"memory.h",
						"time.h"
						]						
						skip = False
						for exclusion_file in exclusion_list:
							if exclusion_file == filename:
								skip = True
								break						
						if skip:
							destination.write(final_line)
							continue
							
					# Prevent editor clash where ClassFactory.h is incorrecly overriden with CrySerialization/ClassFactory.h
					if filename == "ClassFactory.h":
						if not "Serialization/" in line:
							destination.write(final_line)
							continue
							
					# Editor has own smartptr.h implementation which it sometimes uses but not all the time
					if "util/smartptr.h" in filename.lower():
						destination.write(final_line)
						continue
              
					if "util/math.h" in filename.lower():
						destination.write(final_line)
						continue	

					if "editorframework/inspector.h" in filename.lower():
						destination.write(final_line)
						continue	
						
					#################
					# END SPECIAL CASES
					#################

					
					#################
					# TRY AND MATCH FILE WITH CRYCOMMON
					#################
					for map_file in src_file_filter_list:
						map_file_lower = map_file.lower()
						if map_file_lower.endswith(file_base_name_lower):							
							if os.path.basename(map_file_lower) == file_base_name_lower:
								potential_matches += [map_file]								
								
								
					#################
					# RESOLVE DUPLICATE MATCHES
					#################
					if potential_matches:					
						final_map_file = ""
						if len(potential_matches) != 1:
							resolved_potential_matches = []
							
							# Resolved naming clash of CryString and CrySerialization
							if "Serialization" in filename or "Serialization" in file_path:
								for potential_match in potential_matches:
									if  "Serialization" in potential_match:									
										resolved_potential_matches += [potential_match]
							else:
								for potential_match in potential_matches:
									if  not "Serialization" in potential_match:
										resolved_potential_matches += [potential_match]
								
							# Resolve reference to local file in folder using #include "xxx" and not #include <xxx>
							if len(resolved_potential_matches) != 1 and begin_char == "\"": # check for local file
								file_path_dir = os.path.dirname(file_path).replace("\\", "/")
								for potential_match in potential_matches:
									if file_path_dir.endswith(os.path.dirname(potential_match)):										
										resolved_potential_matches = [potential_match]
										break
										
							# Resolve cases where files case sensitivity solves the case
							if len(resolved_potential_matches) != 1:
								_resolved_potential_matches = []
								for potential_match in potential_matches:
									if file_base_name == os.path.basename(potential_match):
										_resolved_potential_matches += [potential_match]
								
								if _resolved_potential_matches:
									resolved_potential_matches = _resolved_potential_matches

							if len(resolved_potential_matches) == 1:
								global conversion_resolved_warning_summary
								conversion_resolved_warning_summary += ["Resolved multiple matches: %s with %s from list %s (%s) :" % (filename, resolved_potential_matches, potential_matches, file_path)]
								Logs.warn("Resolved multiple matches: %s with %s  from list %s (%s):" % (filename, resolved_potential_matches, potential_matches, file_path))
								potential_matches = resolved_potential_matches
							else:
								global conversion_unresolved_warning_summary
								conversion_unresolved_warning_summary += ["Unresolved multiple matches: %s with %s (%s):" % (filename, potential_matches, file_path)]

						if len(potential_matches) == 1:
							replace_filename = potential_matches[0]
							if filename != replace_filename:
								if line_specifier == '#':
									final_line = final_line.replace('%s%s%s' % (begin_char,filename,ending_char), '%s%s%s' % ('<', replace_filename, '>')) # always replace include from CryCommon with #include <xxx>
									Logs.info("Replaced: %s with %s (%s):" % (filename, replace_filename, filename))
									destination_has_changed = True
								elif line_specifier == '%':
									final_line = final_line.replace('%s' % (filename), '%s/%s' % (rel_path_to_crycommon, replace_filename)) # replace  #include "<relative_path_to_crycommon>/xxx"
									Logs.info("Replaced: %s with %s (%s):" % (filename, replace_filename, filename))
									destination_has_changed = True				
								
		destination.write(final_line)
		
	destination.close()
	source.close()
	
	if destination_has_changed:
		_p4_edit_file(source_file)		
		try:
			os.chmod( source_file, stat.S_IWRITE )	
			os.remove(source_file)
		except:
			pass
			
		os.rename( destination_file, source_file)	
	else:
		os.remove(destination_file)
	
	
##################################################
@conf
def convert_files_in_folder_to_cry_common(ctx, module_nodes):

	use_p4 = True if _get_p4_workspace("dummy") else False
	Logs.info("Starting conversion process...")
	
	cry_common_node = ctx.path.make_node("Code/CryEngine/CryCommon")		
		
	for module_node in module_nodes:
	
		if not os.path.isdir(module_node.abspath()):
			Logs.error("Module file path does not exist. Unable to convert files to new CryCommon layout. (%s) " % module_node.abspath())
			return

		for dirName, subdirList, file_list in os.walk(module_node.abspath(), followlinks=True):
			subdir_path_node = ctx.root.make_node(dirName)
			for file in file_list: 
				convert_source_files_to_waf_file_mapping(ctx, cry_common_node, subdir_path_node.make_node(file), use_p4)
			
	global conversion_resolved_warning_summary
	global conversion_unresolved_warning_summary
	
	if conversion_resolved_warning_summary or conversion_unresolved_warning_summary:
		Logs.warn("===============================")
		Logs.warn("====Summary Conversion Conflicts ===")
		Logs.warn("===============================")
	
		if conversion_resolved_warning_summary:
			Logs.warn("\n==== (RESOLVED:) ===")
			for conversion_conflict in conversion_resolved_warning_summary:
				Logs.warn(conversion_conflict) 
			
			conversion_resolved_warning_summary = []
				
		if conversion_unresolved_warning_summary:
			Logs.warn("\n==== (UNRESOLVED:) ===")
			for conversion_conflict in conversion_unresolved_warning_summary:
				Logs.warn(conversion_conflict) 
			
			conversion_unresolved_warning_summary = []

