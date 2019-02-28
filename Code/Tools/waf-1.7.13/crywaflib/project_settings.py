# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

import os
from waflib.Configure import conf
from waflib import Context

@conf
def _projects_node(ctx):
	return ctx.root.make_node(Context.launch_dir).make_node('_WAF_/projects.json')

@conf
def _internal_projects_node(ctx):
	return ctx.root.make_node(Context.launch_dir).make_node('_WAF_/projects_internal.json')

def _load_project_settings_file(ctx):
	"""
	Util function to load the projects.json file and cache it within the build context
	"""
	if hasattr(ctx, 'projects_settings'):
		return

	if os.path.isfile(_internal_projects_node(ctx).abspath()):
		projects_node = _internal_projects_node(ctx)
	else:
		projects_node = _projects_node(ctx)

	ctx.projects_settings = ctx.parse_json_file(projects_node)


def _project_setting_entry(ctx, project, entry):
	"""
	Util function to load an entry from the projects.json file
	"""
	_load_project_settings_file(ctx)

	if not project in ctx.projects_settings:
		ctx.cry_file_error('Cannot find project entry for "%s"' % project, _projects_node(ctx).abspath() )

	if not entry in ctx.projects_settings[project]:
		ctx.cry_file_error('Cannot find entry "%s" for project "%s"' % (entry, project), _projects_node(ctx).abspath())		
		
	return ctx.projects_settings[project][entry]

	
def _project_durango_setting_entry(ctx, project, entry):
	"""
	Util function to load an entry from the projects.json file
	"""
	
	durango_settings = _project_setting_entry(ctx,project, 'durango_settings')
	
	if not entry in durango_settings:
		ctx.cry_file_error('Cannot find entry "%s" for project "%s"' % (entry, project), _projects_node(ctx).abspath())		

	return durango_settings[entry]
	
def _project_orbis_setting_entry(ctx, project, entry):
	"""
	Util function to load an entry from the projects.json file
	"""
	
	orbis_settings = _project_setting_entry(ctx,project, 'orbis_settings')
	
	if not entry in orbis_settings:
		ctx.cry_file_error('Cannot find entry "%s" for project "%s"' % (entry, project), _projects_node(ctx).abspath())		

	return orbis_settings[entry]

#############################################################################
#############################################################################	
@conf		
def game_projects(self):
	_load_project_settings_file(self)

	# Build list of game code folders
	projects = []
	for key, value in self.projects_settings.items():
		projects += [ key ]
	return projects
	
@conf	
def project_idx	(self, project):
	_load_project_settings_file(self)	
	
	nIdx = 0
	for key, value in self.projects_settings.items():
		if key == project:
			return nIdx
		nIdx += 1
	return nIdx;
	

#############################################################################	
@conf		
def game_code_folder(self, project):
	return _project_setting_entry(self, project, 'code_folder')
	
@conf		
def get_executable_name(self, project):
	return _project_setting_entry(self, project, 'executable_name')
	
@conf		
def get_dedicated_server_executable_name(self, project):
	return _project_setting_entry(self, project, 'executable_name') + '_Server'
	
@conf		
def project_output_folder(self, project):
	return _project_setting_entry(self, project, 'project_output_folder')
	
#############################################################################	
@conf 
def get_launcher_product_name(self, game_project):
	return _project_setting_entry(self, game_project, 'product_name')

#############################################################################	
@conf 
def get_dedicated_server_product_name(self, game_project):
	return _project_setting_entry(self,  game_project, 'product_name') + ' Dedicated Server'
	
#############################################################################	
@conf
def get_product_name(self, target, game_project):		
	if target.startswith('WindowsLauncher') or target.startswith('OrbisLauncher') or target.startswith('DurangoLauncher'):
		return self.get_launcher_product_name(game_project)
	elif target.startswith('DedicatedLauncher'):
		return self.get_dedicated_server_product_name(game_project)
	else:
		return target

#############################################################################	
@conf
def project_durango_app_id(self,project):
		return _project_durango_setting_entry(self, project, 'app_id')
		
@conf
def project_durango_package_name(self,project):
		return _project_durango_setting_entry(self, project, 'package_name')	
		
@conf
def project_durango_version(self,project):
		return _project_durango_setting_entry(self, project, 'version')			
		
@conf
def project_durango_display_name(self,project):
		return _project_durango_setting_entry(self, project, 'display_name')	

@conf
def project_durango_publisher_name(self,project):
		return _project_durango_setting_entry(self, project, 'publisher')	

@conf
def project_durango_description(self,project):
		return _project_durango_setting_entry(self, project, 'description')	

@conf
def project_durango_foreground_text(self,project):
		return _project_durango_setting_entry(self, project, 'foreground_text')			
		
@conf
def project_durango_background_color(self,project):
		return _project_durango_setting_entry(self, project, 'background_color')			

@conf
def project_durango_titleid(self,project):
		return _project_durango_setting_entry(self, project, 'titleid')			

@conf
def project_durango_scid(self,project):
		return _project_durango_setting_entry(self, project, 'scid')			

@conf
def project_durango_appxmanifest(self,project):
		return _project_durango_setting_entry(self, project, 'appxmanifest')			

@conf
def project_durango_logo(self,project):
		return _project_durango_setting_entry(self, project, 'logo')			

@conf
def project_durango_small_logo(self,project):
		return _project_durango_setting_entry(self, project, 'small_logo')			

@conf
def project_durango_splash_screen(self,project):
		return _project_durango_setting_entry(self, project, 'splash_screen')			

@conf
def project_durango_store_logo(self,project):
		return _project_durango_setting_entry(self, project, 'store_logo')			

#############################################################################		
@conf
def project_orbis_data_folder(self,project):
		return _project_orbis_setting_entry		(self, project, 'data_folder')	
