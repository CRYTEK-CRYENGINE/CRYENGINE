# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

########### Below are various getter to expose the global values ############
from waflib.Configure import conf
from waflib import Context, Utils, Logs
from waflib import Errors

from waf_branch_spec import RECODE_LICENSE_PATH
from waf_branch_spec import BINTEMP_FOLDER

from waf_branch_spec import COMPANY_NAME
from waf_branch_spec import COPYRIGHT

from waf_branch_spec import PLATFORMS
from waf_branch_spec import CONFIGURATIONS

import itertools
import sys, os
#############################################################################	
@conf 
def get_recode_license_key(self):
	key = ''
	try:
		fd = open(os.path.normpath(RECODE_LICENSE_PATH), 'r')
		key = fd.read().strip()
		fd.close()
	except:
		pass
	return key
	
#############################################################################	
@conf 
def get_recode_base_dir(self):
	return '../'	

#############################################################################	
#############################################################################	
@conf 
def get_bintemp_folder_node(self):	
	return self.root.make_node(Context.launch_dir).make_node(BINTEMP_FOLDER)	

#############################################################################	
@conf 
def get_project_output_folder(self):
	project_folder_node = self.root.make_node(Context.launch_dir).make_node(self.options.visual_studio_solution_folder).make_node('.depproj')
	project_folder_node.mkdir()
	return project_folder_node

#############################################################################	
@conf 
def get_solution_name(self):
	return self.options.visual_studio_solution_folder + '/' + self.options.visual_studio_solution_name + '.sln'

#############################################################################	
#############################################################################	
@conf 
def get_company_name(self):
	return COMPANY_NAME	
	
#############################################################################	
@conf 
def get_product_version(self):	
	return self.options.version	

#############################################################################	
@conf 
def get_copyright(self):
	return COPYRIGHT	
		
#############################################################################
#############################################################################	
_all_platforms = [p for l in PLATFORMS.values() for p in l]
@conf
def get_all_platforms(self):
	return _all_platforms

#############################################################################
#############################################################################	
@conf
def get_supported_platforms(self):
	host = Utils.unversioned_sys_platform()	
	return PLATFORMS[host]

#############################################################################
#############################################################################	
@conf
def set_supported_platforms(self, platforms):
	host = Utils.unversioned_sys_platform()	
	if isinstance(platforms,list):
		PLATFORMS[host] = platforms
	else:
		PLATFORMS[host] = [platforms]
	
#############################################################################	
@conf
def get_supported_configurations(self):
	return CONFIGURATIONS
	
#############################################################################
#############################################################################	
@conf
def get_project_vs_filter(self, target):		
	if not hasattr(self, 'vs_project_filters'):
		self.fatal('vs_project_filters not initialized')

	if not target in self.vs_project_filters:
		self.fatal('No visual studio filter entry found for %s' % target)
		
	return self.vs_project_filters[target]
	
#############################################################################
#############################################################################	

def _preserved_order_remove_duplicates(seq):
	seen = set()
	seen_add = seen.add
	return [ x for x in seq if not (x in seen or seen_add(x))]

from collections import Iterable
def _flatten(coll):
	for i in coll:
		if isinstance(i, Iterable) and not isinstance(i, basestring):
			for subc in _flatten(i):
				yield subc
		else:
			yield i

def _dict_to_list(dict):
	ret = list(itertools.chain.from_iterable(dict.values()))
	_flatten(ret)	
	ret = _preserved_order_remove_duplicates(ret) # strip duplicate
	return ret

def _load_specs(ctx):
	""" Helper function to find all specs stored in _WAF_/specs/*.json """
	if hasattr(ctx, 'loaded_specs_dict'):
		return
		
	ctx.loaded_specs_dict = {}
	spec_file_folder 	= ctx.root.make_node(Context.launch_dir).make_node('/_WAF_/specs')
	spec_files 				= spec_file_folder.ant_glob('**/*.json')
	
	for file in spec_files:
		try:
			spec = ctx.parse_json_file(file)
			spec_name = str(file).split('.')[0]
			ctx.loaded_specs_dict[spec_name] = spec
		except Exception as e:
			ctx.cry_file_error(str(e), file.abspath())	

@conf	
def loaded_specs(ctx):
	""" Get a list of the names of all specs """
	_load_specs(ctx)
	
	ret = []				
	for (spec,entry) in ctx.loaded_specs_dict.items():
		ret.append(spec)
	
	return ret
	
def _spec_entry_as_single_list(ctx, entry, spec_name = None, platform = None, configuration = None):
	 return _dict_to_list(_spec_entry_as_dict(ctx, entry, spec_name, platform, configuration))

def _spec_entry_as_dict(ctx, entry, spec_name = None, platform = None, configuration = None):
	""" Util function to load a specifc spec entry (always returns a list) """
	_load_specs(ctx)
	
	if not spec_name: # Fall back to command line specified spec if none was given
		spec_name = ctx.options.project_spec

	spec_dict = ctx.loaded_specs_dict.get(spec_name, None)
	if not spec_dict:
		ctx.fatal('[ERROR] Unknown Spec "%s", valid settings are "%s"' % (spec_name, ', '.join(ctx.loaded_specs())))

	def _to_list( value ):
		if isinstance(value,list):
			return value
		return [ value ]		
		
	if hasattr(ctx, 'env'): # The options context is missing an env attribute, hence don't try to find platform settings in this case
		if not platform:
			platform = ctx.env['PLATFORM']
			
		if not configuration:
			configuration = ctx.env['CONFIGURATION']				
	
		res = ctx.GetPlatformSpecificSettings(spec_dict, entry, platform, configuration)
	else:
		res = dict()

	res.setdefault(entry, []).extend(_to_list( spec_dict.get(entry, []) ))
	res[entry] = _preserved_order_remove_duplicates(res[entry])
	return res
	
# Allow a module to have an alias. This is usefull when building multiple projects at once where e.g. the WindowsLauncher is build for multiple game projects
spec_modules_name_alias = {}
@conf
def set_spec_modules_name_alias(ctx, module_name, module_name_alias):
	alias_list = spec_modules_name_alias.setdefault(module_name, [])	
	if module_name_alias not in alias_list and not module_name == module_name_alias:	
		alias_list.extend([module_name_alias])

@conf	
def spec_features(ctx, spec_name = None, platform = None, configuration = None):
	""" Get all a list of all features needed for this spec """
	return _spec_entry_as_dict(ctx, 'features', spec_name, platform, configuration)	
	
@conf	
def spec_modules(ctx, spec_name = None, platform = None, configuration = None):
	""" Get all a list of all modules needed for this spec """
	modules = _spec_entry_as_single_list(ctx, 'modules', spec_name, platform, configuration)
	
	# Add module alias names to list
	for module, module_alias in spec_modules_name_alias.iteritems():
		if module in modules:
			modules.extend(module_alias)
	
	return modules
	
@conf	
def spec_game_projects(ctx, spec_name = None, platform = None, configuration = None):
	""" get a list of all game projects in this spec """
	return _spec_entry_as_single_list(ctx, 'game_projects', spec_name, platform, configuration)

@conf	
def spec_defines(ctx, spec_name = None, platform = None, configuration = None):
	""" Get all a list of all defines needed for this spec """
	""" This function is deprecated """
	return _spec_entry_as_dict(ctx, 'defines', spec_name, platform, configuration)

@conf	
def spec_module_extensions(ctx, spec_name = None, platform = None, configuration = None):
	""" Get all a list of all defines needed for this spec """
	""" This function is deprecated """
	return _spec_entry_as_dict(ctx, 'module_extensions', spec_name, platform, configuration)

@conf	
def spec_visual_studio_name(ctx, spec_name = None, platform = None, configuration = None):
	""" Get all a the name of this spec for Visual Studio """
	visual_studio_name =  _spec_entry_as_single_list(ctx, 'visual_studio_name', spec_name, platform, configuration)
	
	if not visual_studio_name:
		ctx.fatal('[ERROR] Mandatory field "visual_studio_name" missing from spec "%s"' % (spec_name or ctx.options.project_spec) )
		
	# _spec_entry always returns a list, but client code expects a single string
	assert( len(visual_studio_name) == 1 )
	return visual_studio_name[0]

@conf	
def spec_output_folder_post_fix(ctx, spec_name = None, platform = None, configuration = None):
	""" get a list of all game projects in this spec """
	output_folder_post_fix =  _spec_entry_as_single_list(ctx, 'output_folder_post_fix', spec_name, platform, configuration)
	
	# _spec_entry always returns a list, but client code expects a single string
	return output_folder_post_fix[0] if output_folder_post_fix else ""
	
@conf	
def spec_valid_configurations(ctx, spec_name = None, platform = None, configuration = None):
	""" Get all a list of all valid configurations for this spec """
	valid_configuration =  _spec_entry_as_single_list(ctx, 'valid_configuration', spec_name, platform, configuration)
	if valid_configuration == []:
		ctx.fatal('[ERROR] Mandatory field "valid_configuration" missing from spec "%s"' % (spec_name or ctx.options.project_spec))
		
	return valid_configuration

@conf	
def spec_valid_platforms(ctx, spec_name = None, platform = None, configuration = None):
	""" Get all a list of all valid platforms for this spec """
	valid_platforms =  _spec_entry_as_single_list(ctx, 'valid_platforms', spec_name, platform, configuration)
	
	if not valid_platforms:
		ctx.fatal('[ERROR] Mandatory field "valid_platforms" missing from spec "%s"' % (spec_name or ctx.options.project_spec) )
		
	return valid_platforms

@conf	
def spec_description(ctx, spec_name = None, platform = None, configuration = None):
	""" Get description for this spec """
	description = _spec_entry_as_single_list(ctx, 'description', spec_name, platform, configuration)
	if description == []:
		ctx.fatal('[ERROR] Mandatory field "description" missing from spec "%s"' % (spec_name or ctx.options.project_spec) )
	
	# _spec_entry always returns a list, but client code expects a single string
	assert( len(description) == 1 )
	return description[0]
	
@conf	
def spec_monolithic_build(ctx, spec_name = None, platform = None, configuration = None):
	""" Return true if the current platform|configuration should be a monolithic build in this spec """
	monolithic_build = _spec_entry_as_single_list(ctx, 'monolithic_build', spec_name, platform, configuration)
	if monolithic_build == []:
		return False
	
	# _spec_entry always returns a list, but client code expects a single boolean
	assert( len(monolithic_build) == 1 )
	return monolithic_build[0].lower() == "true"	
	
@conf	
def spec_force_shared_monolithic_build_modules(ctx, spec_name = None, platform = None, configuration = None):
	""" Return list of all modules that should be linked dynamically on monolithical builds """
	return _spec_entry_as_single_list(ctx, 'force_shared_monolithic_build_modules', spec_name, platform, configuration)

@conf	
def spec_force_individual_build_directory(ctx, spec_name = None, platform = None, configuration = None):
	""" Return true if the current platform|configuration should use it's own temporary build directory """
	force_individual_build_directory = _spec_entry_as_single_list(ctx, 'force_individual_build_directory', spec_name, platform, configuration)
	if force_individual_build_directory == []:
		return False
	
	# _spec_entry always returns a list, but client code expects a single boolean
	assert( len(force_individual_build_directory) == 1 )
	return force_individual_build_directory[0].lower() == "true"	
	
# Set global output directory	
setattr(Context.g_module, Context.OUT, BINTEMP_FOLDER)
