# Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.
from waflib.TaskGen import before_method, feature
from waflib import Utils

import os
import sys
from waflib.Configure import conf
from waflib import Logs
from cStringIO import StringIO

###############################################################################
def _set_override(override_options_map, node_name, node_value, env, proj_path):
	
	def _set_override_value(override_options_map, key, value):
		try:
			override_options_map[key] += value
		except:
			override_options_map[key] = value
		
	# [WAF_ExcludeFromUberFile]	
	if node_name == 'WAF_ExcludeFromUberFile':
		if node_value.lower() == 'true':		
				override_options_map['exclude_from_uber_file'] = True
	# [WAF_DisableCompilerOptimization]
	elif node_name == 'WAF_DisableCompilerOptimization': 
		if node_value.lower() == 'true' and node_value:
			_set_override_value(override_options_map, 'CFLAGS', env['COMPILER_FLAGS_DisableOptimization'])
			_set_override_value(override_options_map, 'CXXFLAGS', env['COMPILER_FLAGS_DisableOptimization'])				
	# [WAF_AdditionalCompilerOptions_C]
	elif node_name == 'WAF_AdditionalCompilerOptions_C':
		_set_override_value(override_options_map, 'CFLAGS', [node_value])				
	# [WAF_AdditionalCompilerOptions_CXX]
	elif node_name == 'WAF_AdditionalCompilerOptions_CXX':
		_set_override_value(override_options_map, 'CXXFLAGS', [node_value])
	# [WAF_AdditionalLinkerOptions]
	elif node_name == 'WAF_AdditionalLinkerOptions':
		_set_override_value(override_options_map, 'LINKFLAGS', node_value.split())
	# [WAF_AdditionalPreprocessorDefinitions]
	elif node_name == 'WAF_AdditionalPreprocessorDefinitions':
		_set_override_value(override_options_map, 'DEFINES', [x.strip() for x in node_value.split(';')])
	# [WAF_AdditionalIncludeDirectories]
	elif node_name == 'WAF_AdditionalIncludeDirectories':	
		path_list = [x.strip() for x in node_value.split(';')] 
		
		# Ensure paths are absolute paths
		for idx, path in enumerate(path_list):		
			if os.path.isabs(path) == False:
				path_list[idx] = proj_path.make_node(path).abspath()
		
		# Store paths
		_set_override_value(override_options_map, 'INCPATHS', path_list)			
		
###############################################################################
def collect_tasks_for_project(self):
	tasks = []
	if hasattr(self, 'compiled_tasks'):
		tasks += self.compiled_tasks
		
	if hasattr(self, 'pch_task'):
		tasks += [self.pch_task]
		
	if hasattr(self, 'link_task'):
		tasks += [self.link_task]
	return tasks	
	
###############################################################################
def collect_tasks_for_file(self, vs_file_path):
	tasks = []
	
	# look in compilation tasks	
	for t in getattr(self, 'compiled_tasks', []):			
		# [Early out] Assume a vs_file_path is unique between all task.input lists
		if tasks:
			break			
			
		for waf_path_node in t.inputs:
			# none uber_files
			if vs_file_path == waf_path_node.abspath():
				tasks += [t]
				break # found match
				
			#uber files											
			try:
				for waf_uber_file_source_node in self.uber_file_lookup[waf_path_node.abspath()]:
					if vs_file_path == waf_uber_file_source_node:
						tasks += [t]
						break # found match
			except:
				pass
	
	if hasattr(self, 'pch_task'):
		for waf_path_node in self.pch_task.inputs:
			if vs_file_path == waf_path_node.abspath():
				tasks += [t] # found match
				break # found match
		
	if hasattr(self, 'link_task'):
		for waf_path_node in self.link_task.inputs:
			if vs_file_path == waf_path_node.abspath():
				tasks += [t] # found match
				break # found match
	
	return tasks
	
###############################################################################
def get_element_name(line):		
		#e.g. <WAF_DisableCompilerOptimization> 
		posElementEnd = line.find('>',1)		
		
		# e.g. <WAF_DisableCompilerOptimization Condition"...">
		alternativeEnd = line.find(' ',1, posElementEnd)		
		if alternativeEnd != -1:
			return line[1:alternativeEnd]
			
		return line[1:posElementEnd]
		
###############################################################################
def get_element_value(line):
		pos_valueEnd = line.rfind('<')
		pos_valueStart = line[:-1].rfind('>', 0, pos_valueEnd)
		return line[pos_valueStart+1:pos_valueEnd]		


###############################################################################
# Extract per project overrides from .vcxproj file (apply to all project items)
def _get_project_overrides(ctx, target):	
	
	if ctx.cmd == 'generate_uber_files' or ctx.cmd == 'msvs' or ctx.cmd == 'configure':
		return ({}, {})
				
	# Only perform on VS executed builds
	if not getattr(ctx.options, 'execsolution', None):
		return ({}, {})

	if Utils.unversioned_sys_platform() != 'win32':
		return ({}, {})

	if not hasattr(ctx, 'project_options_overrides'):
		ctx.project_options_overrides = {}

	if not hasattr(ctx, 'project_file_options_overrides'):
		ctx.project_file_options_overrides = {}
		
	try:
		return (ctx.project_options_overrides[target], ctx.project_file_options_overrides[target])
	except:
		pass

	# Check if project is valid for current spec, config and platform
	waf_spec = ctx.options.project_spec
	waf_platform = ctx.env['PLATFORM']
	waf_configuration = ctx.env['CONFIGURATION']

	if not ctx.is_valid_build_target(target, waf_spec, waf_configuration, waf_platform):
		return ({}, {})
	
	ctx.project_options_overrides[target] = {}
	ctx.project_file_options_overrides[target] = {}
	
	project_options_overrides = ctx.project_options_overrides[target]
	project_file_options_overrides = ctx.project_file_options_overrides[target]	

	vs_spec =	ctx.convert_waf_spec_to_vs_spec(waf_spec)
	vs_platform = ctx.convert_waf_platform_to_vs_platform(waf_platform)
	vs_configuration = ctx.convert_waf_configuration_to_vs_configuration(waf_configuration)
	vs_valid_spec_for_build = '[%s] %s|%s' % (vs_spec, vs_configuration, vs_platform)

	vcxproj_file =  (ctx.get_project_output_folder().make_node('%s%s'%(target,'.vcxproj'))).abspath()

	# Example: <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='[Hunt] Profile|Win64'">	
	project_override = '<ItemDefinitionGroup Condition="\'$(Configuration)|$(Platform)\'==\'%s\'"' % vs_valid_spec_for_build
  
	# Open file
	try:
		file = open(vcxproj_file)
	except:	
		Logs.warn('warning: Unable to parse .vcxproj file to extract configuration overrides. [File:%s] [Exception:%s]' % (vcxproj_file, sys.exc_info()[0]) )
		return ({}, {})
 
	# Iterate line by line 
	#(Note: skipping lines inside loop)
	file_iter = iter(file)
	for line in file_iter:
		stripped_line = line.lstrip()
		
		#[Per Project]
		# Example:
		# <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='[Hunt] Profile|x64'">
		#	<ClCompile>
		#		<WAF_DisableCompilerOptimization>true</WAF_DisableCompilerOptimization>
		#		<WAF_AdditionalPreprocessorDefinitions>/MyProjectDefinition</WAF_AdditionalPreprocessorDefinitions>
		#	</ClCompile>
		#</ItemDefinitionGroup>		
		if stripped_line.startswith(project_override) == True:

			# Extract WAF items
			while True:
				next_line = file_iter.next().strip()
				if next_line.startswith('<WAF_'):						
					element_name = get_element_name(next_line)
					element_value = get_element_value(next_line)

					# Cache override
					_set_override(project_options_overrides, element_name, element_value, ctx.env, ctx.path)
					
				elif next_line.startswith('</ItemDefinitionGroup>'):
					break
 
		#[Per Item]
		# Example:
		# <ItemGroup>
		#		<ClCompile Include="e:\P4\CE_STREAMS\Code\CryEngine\CryAnimation\Attachment.cpp">
		#			<WAF_AdditionalPreprocessorDefinitions Condition="'$(Configuration)|$(Platform)'=='[Hunt] Profile|x64'">/MyItemDefinition</WAF_AdditionalPreprocessorDefinitions>
		#			<WAF_AdditionalCompilerOptions_CXX Condition="'$(Configuration)|$(Platform)'=='[Hunt] Profile|x64'">/MyItemCompilerOption</WAF_AdditionalCompilerOptions_CXX>
		#		</ClCompile>
		# </ItemGroup>
		elif stripped_line.startswith('<ItemGroup>') == True:	
			while True:
				next_line = file_iter.next().strip()
				
				# Check that element is a "ClCompile" element that has child elements i.e. not <ClCompile ... />
				if next_line.endswith('/>') == False and next_line.startswith('<ClCompile') == True:					
					item_tasks = []				
					# Is WAF Element
					while True:
						next_line_child = file_iter.next().strip()						
						if next_line_child.startswith('<WAF_'):						
							# Condition meets platform specs (optimize if needed)
							if vs_valid_spec_for_build in next_line_child:
							
								# For first valid item, collect tasks
								if not item_tasks:
									# Get include path
									pos_include_name_start = next_line.find('"')+1
									pos_include_name_end = next_line.find('"', pos_include_name_start)
									vs_file_path = next_line[pos_include_name_start:pos_include_name_end].lower()
									
								# Get element info
								element_name = get_element_name(next_line_child)
								element_value = get_element_value(next_line_child)		

								# Cache override
								try:
									override_map = project_file_options_overrides[vs_file_path]
								except:
									project_file_options_overrides[vs_file_path] = {}
									override_map = project_file_options_overrides[vs_file_path]

									_set_override(override_map, element_name, element_value, ctx.env, ctx.path)

						#end of ClCompile Element
						elif next_line_child.startswith('</ClCompile>'):
							break
				# end of "ItemGroup" Element
				elif next_line.startswith('</ItemGroup>'):
					break
					
	return (project_options_overrides, project_file_options_overrides)

###############################################################################
@conf 
def get_project_overrides(ctx, target):
	return _get_project_overrides(ctx, target)[0]

###############################################################################
@conf
def get_file_overrides(ctx, target):
	return _get_project_overrides(ctx, target)[1]

###############################################################################
# Extract per project overrides from .vcxproj file (apply to all project items)
@feature('parse_vcxproj')
@before_method('verify_compiler_options_msvc')
def apply_project_compiler_overrides_msvc(self):	
	if self.bld.cmd == 'generate_uber_files' or self.bld.cmd == 'msvs':
		return
				
	# Only perform on VS executed builds
	if not hasattr(self.bld.options, 'execsolution'):
		return
	
	if not self.bld.options.execsolution:	
		return
  
	if Utils.unversioned_sys_platform() != 'win32':
		return
		
	def _apply_env_override(env, option_name, value):
		if option_name in ['CFLAGS', 'CXXFLAGS', 'LINKFLAGS', 'DEFINES', 'INCPATHS']:
			try:
				env[option_name] += value
			except:
				env[option_name] = value
		
	target = getattr(self, 'base_name', None)
	if not target:
		target = self.target
  
	# Project override
	for key, value  in self.bld.get_project_overrides(target).iteritems():
		project_tasks = collect_tasks_for_project(self)
		
		for t in project_tasks:			
			t.env.detach() 
			
		for t in project_tasks:
			_apply_env_override(t.env, key, value)		
  
	# File override
	for file_path, override_options in self.bld.get_file_overrides(target).iteritems():
		item_tasks = collect_tasks_for_file(self, file_path)
		
		for t in item_tasks:
			t.env.detach() 
  
		for t in item_tasks:
			for key, value in override_options.iteritems():
				_apply_env_override(t.env, key, value)

###############################################################################
@conf
def get_solution_overrides(self):	
		
	if self.cmd == 'generate_uber_files' or self.cmd == 'msvs':
		return {}
		
	# Only perform on VS executed builds
	try:
		sln_file = self.options.execsolution
	except:
		return {}	
	
	if not sln_file:
			return {}	
			
	if Utils.unversioned_sys_platform() != 'win32':
		return

	# Open sln file
	try:
		file = open(sln_file)		
	except:	
		Logs.debug('warning: Unable to parse .sln file to extract configuration overrides: [File:%s] [Exception:%s]' % (sln_file, sys.exc_info()[0]) )
		return {}
		
	ret_vs_project_override = {}
	vs_spec =	self.convert_waf_spec_to_vs_spec(self.options.project_spec)
	vs_platform = self.convert_waf_platform_to_vs_platform(self.env['PLATFORM'])
	vs_configuration = self.convert_waf_configuration_to_vs_configuration(self.env['CONFIGURATION'])				
	
	vs_build_configuration = '[%s] %s|%s' % (vs_spec, vs_configuration, vs_platform) # Example: [Hunt] Debug|x64	
	vs_project_identifier = 'Project("{8BC9CEB8' # C++ project: 8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942
	
	# Iterate over all basic project  information
	# Example:
	#   Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "Cry3DEngine", "e:\P4\CE_STREAMS\Solutions\.depproj\Cry3DEngine.vcxproj", "{60178AE3-57FD-488C-9A53-4AE4F66419AA}"
	project_guid_to_name = {}
	file_iter = iter(file)
	for line in file_iter:
		stripped_line = line.lstrip()	
		if stripped_line.startswith(vs_project_identifier) == True:
			project_info = stripped_line[51:].split(',')  # skip first 51 character ... "Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") ="
			project_name = project_info[0].strip()[1:-1]  # trim left and right and remove '"'
			project_path = project_info[1].strip()[1:-1]
			project_guid = project_info[2].strip()[2:-2]
						
			# Store project GUID and Name pair
			project_guid_to_name[project_guid] = project_name
			
		elif stripped_line.startswith('Global') == True:
			file_iter.next()
			break 
		else:
			continue
			
	# Skip to beginning of project configurations information
	for line in file_iter:		
		if line.lstrip().startswith('GlobalSection(ProjectConfigurationPlatforms) = postSolution') == True:
			file_iter.next()
			break		
	  
	# Loop over all project
	# Example:
	# {60178AE3-57FD-488C-9A53-4AE4F66419AA}.[Hunt] Debug|x64.ActiveCfg = [GameSDK and Tools] Debug|x64
	# or 
	# {60178AE3-57FD-488C-9A53-4AE4F66419AA}.[Hunt] Debug|x64.Build.0 = [GameSDK and Tools] Debug|x64
	for line in file_iter:
		stripped_line = line.strip()
		
		# Reached end of section
		if stripped_line.startswith('EndGlobalSection'):
			break		
		
		if stripped_line[39:].startswith(vs_build_configuration) == True:
			project_build_info = stripped_line.split('.')
		
			starts_of_override_configuration = project_build_info[-1].find('[')
			project_build_info[-1] = project_build_info[-1][starts_of_override_configuration:] # remove anything prior to [xxx] e.g. "ActiveCfg = "	
			
			vs_project_configuration = project_build_info[1]
			vs_project_override_configuation = project_build_info[-1]

			# Check for no override condition
			if vs_project_configuration == vs_project_override_configuation:
				continue
							
			project_guid = project_build_info[0][1:-1] # remove surrounding '{' and '}'

			try:
				project_name = project_guid_to_name[project_guid]
			except:
				# Continue if project GUID not in list. 
				# Since we only store C++ projects in the list this project is most likely of an other type e.g. C#. The user will have added this project to the solution manualy.
				Logs.debug('Warning: Override Handling: Unsupported project id "%s" found. Project is most likely not a C++ project' % project_gui)
				continue
			
			# Check that spec is the same
			vs_override_spec_end = vs_project_override_configuation.find(']')
			vs_override_spec = vs_project_override_configuation[1:vs_override_spec_end]
			if vs_spec != vs_override_spec:
				self.cry_error('Project "%s" : Invalid override spec is of type "%s" when it should be "%s"' % (project_name, vs_override_spec, vs_spec))
			
			# Get WAF configuration from VS project configuration e.g. [Hunt] Debug|x64 -> debug
			vs_project_configuration_end = vs_project_override_configuation.rfind('|')
			vs_project_configuration_start = vs_project_override_configuation.rfind(']', 0, vs_project_configuration_end) + 2
			vs_project_configuration = vs_project_override_configuation[vs_project_configuration_start : vs_project_configuration_end]
			waf_configuration = self.convert_vs_configuration_to_waf_configuration(vs_project_configuration)
			
			# Store override
			ret_vs_project_override[project_name] = waf_configuration
			
	return ret_vs_project_override
