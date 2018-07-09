# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
from waflib.Configure import conf
from waflib import Context
from waflib import Logs
from waflib import Build
from waflib import Options
from waflib import Utils

from collections import Counter

import ConfigParser

import subprocess
import sys
import os

user_settings = None

ib_lic_win = "Make && Build Tools"
#ib_lic_xb1 = "Xbox One" #disabled as current build pipeline does not require Durango licence
ib_lic_ps4 = "PlayStation"

ATTRIBUTE_CALLBACKS = {}
""" Global Registry of callbacks for options which requires special processing """

ATTRIBUTE_VERIFICATION_CALLBACKS = {}
""" Global Registry of callbacks for options which requires special processing """

ATTRIBUTE_HINT_CALLBACKS = {}
""" Global Registry of callbacks for options which requires special processing """

LOADED_OPTIONS = {}
""" Global List of already loaded options, can be used to skip initializations if a master value is set to False """

def register_attribute_callback(f):
	""" 
	Decorator function to register a callback for an options attribute.
	*note* The callback function must have the same name as the attribute *note*
	"""
	ATTRIBUTE_CALLBACKS[f.__name__] = f
	
def register_verify_attribute_callback(f):
	""" 
	Decorator function to register a callback verifying an options attribute.
	*note* The callback function must have the same name as the attribute *note*
	"""
	ATTRIBUTE_VERIFICATION_CALLBACKS[f.__name__] = f
	
def register_hint_attribute_callback(f):
	""" 
	Decorator function to register a callback verifying an options attribute.
	*note* The callback function must have the same name as the attribute *note*
	"""
	ATTRIBUTE_HINT_CALLBACKS[f.__name__] = f

def _is_user_option_true(value):
	""" Convert multiple user inputs to True, False or None	"""
	value = str(value)
	if value.lower() == 'true' or value.lower() == 't' or value.lower() == 'yes' or value.lower() == 'y' or value.lower() == '1':
		return True
	if value.lower() == 'false' or value.lower() == 'f' or value.lower() == 'no' or value.lower() == 'n' or value.lower() == '0':
		return False
		
	return None
	
def _is_user_input_allowed(ctx, option_name, value):
	""" Function to check if it is currently allowed to ask for user input """						
	# If we run a no input build, all new options must be set
	if not ctx.is_option_true('ask_for_user_input'):
		if value != '':
			return False # If we have a valid default value, keep it without user input
		else:
			ctx.fatal('No valid default value for %s, please provide a valid input on the command line' % option_name)

	return True
	
	
def _get_string_value(ctx, msg, value):
	""" Helper function to ask the user for a string value """
	msg += ' '
	while len(msg) < 53:
		msg += ' '
	msg += '['+value+']: '
	
	user_input = raw_input(msg)
	if user_input == '':	# No input -> return default
		return value
	return user_input

	
def _get_boolean_value(ctx, msg, value):
	""" Helper function to ask the user for a boolean value """
	msg += ' '
	while len(msg) < 53:
		msg += ' '
	msg += '['+value+']: '
	
	while True:
		user_input = raw_input(msg)
		
		# No input -> return default
		if user_input == '':	
			return value
			
		ret_val = _is_user_option_true(user_input)
		if ret_val != None:
			return str(ret_val)
			
		Logs.warn('Unknown input "%s"\n Acceptable values (none case sensitive):' % user_input)
		Logs.warn("True : 'true'/'t' or 'yes'/'y' or '1'")
		Logs.warn("False: 'false'/'f' or 'no'/'n' or '0'")


def _default_settings_node(ctx):
	""" Return a Node object pointing to the defaul_settings.json file """
	return ctx.root.make_node(Context.launch_dir).make_node('_WAF_/default_settings.json')
	
	
def _load_default_settings_file(ctx):
	""" Util function to load the default_settings.json file and cache it within the build context """
	if hasattr(ctx, 'default_settings'):
		return
		
	default_settings_node = _default_settings_node(ctx)	
	ctx.default_settings = ctx.parse_json_file(default_settings_node)		
	if not ctx.is_bootstrap_available():
		del ctx.default_settings['Bootstrap Support']


def _validate_incredibuild_registry_settings(ctx):
	""" Helper function to verify the correct incredibuild settings """
	if Utils.unversioned_sys_platform() != 'win32':	
		return # Check windows registry only
		
	if not ctx.is_option_true('use_incredibuild'):
		return # No need to check IB settings if there is no IB
	
	allowUserInput = True
	if not ctx.is_option_true('ask_for_user_input'):
		allowUserInput = False
				
	if Options.options.execsolution:
		allowUserInput = False
			
	if ctx.is_option_true('internal_dont_check_recursive_execution'):
		allowUserInput = False
		
	try:
		import _winreg
		IB_settings_read_only = _winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE, "Software\\Wow6432Node\\Xoreax\\Incredibuild\\Builder", 0, _winreg.KEY_READ)
	except:
		Logs.warn('[WARNING] Cannot open registry entry "HKEY_LOCAL_MACHINE\\Software\\Wow6432Node\\Xoreax\\Incredibuild\\Builder" for reading, please check your incredibuild installation')
		return
		
	def _check_ib_setting(setting_name, required_value, desc):
		""" Helper function to read (and potentially modify a registry setting for IB """
		(data, type) = (None, _winreg.REG_SZ)
		try:
			(data,type) = _winreg.QueryValueEx(IB_settings_read_only, setting_name)			
		except:
			import traceback
			traceback.print_exc(file=sys.stdout)
			Logs.warn('[WARNING] Cannot find a registry entry for "HKEY_LOCAL_MACHINE\\Software\\Wow6432Node\\Xoreax\\Incredibuild\\Builder\\%s"' % setting_name )

		# Do we have the right value?
		if str(data) != required_value:		

			if not allowUserInput: # Dont try anything if no input is allowed
				Logs.warn('[WARNING] "HKEY_LOCAL_MACHINE\\Software\\Wow6432Node\\Xoreax\\Incredibuild\\Builder\\%s" set to "%s" but should be "%s"; run WAF outside of Visual Studio to fix automatically' % (setting_name, data, required_value) )
				return
			
			try: # Try to open the registry for writing
				IB_settings_writing = _winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE, "Software\\Wow6432Node\\Xoreax\\Incredibuild\\Builder", 0,  _winreg.KEY_SET_VALUE |  _winreg.KEY_READ)
			except:
				Logs.warn('[WARNING] Cannot access a registry entry "HKEY_LOCAL_MACHINE\\Software\\Wow6432Node\\Xoreax\\Incredibuild\\Builder\\%s" for writing.' % setting_name)
				Logs.warn('[WARNING] Please run cry_waf.exe as an administrator or change the value to "%s" in the registry to ensure a correct operation of WAF' % required_value)
				return

			if data is None:
				info_str = [('Should WAF create "HKEY_LOCAL_MACHINE\\Software\\Wow6432Node\\Xoreax\\Incredibuild\\Builder\\%s" with value "%s"?' % (setting_name, required_value) )]
			else:
				info_str = [('Should WAF change "HKEY_LOCAL_MACHINE\\Software\\Wow6432Node\\Xoreax\\Incredibuild\\Builder\\%s" from "%s" to "%s"?' % (setting_name, data, required_value) )]
			info_str.append(desc)
			
			# Get user choice
			if not ctx.is_option_true('console_mode'): # gui
				retVal = 'True' if ctx.gui_get_choice('\n'.join(info_str)) else 'False'
			else: # console
				Logs.info('\n'.join(info_str))
				retVal = _get_boolean_value(ctx, 'Input', 'Yes')		

			if retVal == 'True' or retVal == 'Yes':
				_winreg.SetValueEx(IB_settings_writing, setting_name,0, type, str(required_value))
			else:
				Logs.warn('[WARNING] WAF is running with "unsupported" IncrediBuild settings. Expect to encounter IncrediBuild errors during compilation.')
	
	_check_ib_setting('MaxConcurrentPDBs', '0', '"MaxConcurrentPDBs" controls how many files can be processed in parallel (an optimization also useful for MSBuild)')
	_check_ib_setting('AllowDoubleTargets', '0', '"AllowDoubleTargets" controls if remote processes can be restarted on local machine when possible\nThis option is mandatory since it causes compiler crashed with WAF')

def _incredibuild_disclaimer(ctx):
	""" Helper function to show a disclaimer over incredibuild before asking for settings """
	if getattr(ctx, 'incredibuild_disclaimer_shown', False):
		return
	Logs.info('\nWAF is using Incredibuild for distributed Builds')
	Logs.info('To be able to compile with WAF, various licenses are required:')
	Logs.info('The ' + ib_lic_win + ' Extension Package is always needed')
	Logs.info('The ' + ib_lic_ps4 + ' Extension Package is needed for PS4 Builds')
	#Logs.info('The ' + ib_lic_xb1 + ' Extension Package is needed for Xbox One Builds')
	Logs.info('If some packages are missing, please ask IT')
	Logs.info('to assign the needed ones to your machine')

	ctx.incredibuild_disclaimer_shown = True
	 
	 
def _verify_incredibuild_licence(licence_name, platform_name):
	""" Helper function to check if user has a incredibuild licence """	
	try:
		result = subprocess.check_output(['xgconsole.exe', '/QUERYLICENSE'])
	except:
		error = '[ERROR] Incredibuild not found on system'
		return False, "", error
	
	if not licence_name in result:
		error = '[ERROR] Incredibuild on "%s" Disabled - Missing IB licence: "%s"' % (platform_name, licence_name)
		return False, "", error
		
	return True,"", ""

@register_attribute_callback	
def use_incredibuild(ctx, section_name, option_name, value):
	""" If Incredibuild should be used, check for required packages """	
	(isValid, warning, error) = ATTRIBUTE_VERIFICATION_CALLBACKS['verify_use_incredibuild'](ctx, option_name, value)	
	if not isValid:
		return 'False'
	return value	
	
	
@register_verify_attribute_callback
def verify_use_incredibuild(ctx, option_name, value):	
	""" Verify value for use_incredibuild """		
	if not _is_user_option_true(value):
		return (True,"","")	
	(res, warning, error) = _verify_incredibuild_licence(ib_lic_win, 'All Platforms')	
	return (res, warning, error)	

	
@register_attribute_callback		
def use_incredibuild_win(ctx, section_name, option_name, value):
	""" IB uses Make && Build Tools also for MSVC Builds, hence we can skip a check here """
	if LOADED_OPTIONS.get('use_incredibuild', 'False') == 'False':
		return 'False'
		
	return 'True'


@register_attribute_callback	
def support_recode(ctx, section_name, option_name, value):
	""" Configure whether recode is supported """

	if not ctx.get_recode_license_key():
		return False

	return value


@register_verify_attribute_callback
def verify_support_recode(ctx, option_name, value):
	""" Helper function to check if user has a recode licence """
	if ctx.get_recode_license_key() != '':
		return (True, "", "")
	else:
		return (True, "", "No recode license found in waf_branch_spec.py.")


@register_hint_attribute_callback
def hint_support_recode(ctx, section_name, option_name, value):
	""" Hint enable/disable recode based on presence of a license """
	has_key = str(ctx.get_recode_license_key() != '')

	return ([has_key], [has_key], [], "boolean")


@register_verify_attribute_callback
def verify_use_incredibuild_win(ctx, option_name, value):
	""" Verify value for verify_use_incredibuild_win """	
	if not _is_user_option_true(value):
		return (True,"","")
	(res, warning, error) = _verify_incredibuild_licence(ib_lic_win, 'Windows')
	return (res, warning, error)

	
#Disabled as current build pipeline does not require the licence
#
#@register_attribute_callback		
#def use_incredibuild_durango(ctx, section_name, option_name, value):
#	""" If Incredibuild should be used, check for requiered packages """
#	if LOADED_OPTIONS.get('use_incredibuild', 'False') == 'False':
#		return 'False'
#		
#	(isValid, warning, error) = ATTRIBUTE_VERIFICATION_CALLBACKS['verify_use_incredibuild_durango'](ctx, option_name, value)	
#	if not isValid:
#		return 'False'
#	return value	
#	
#@register_verify_attribute_callback
#def verify_use_incredibuild_durango(ctx, option_name, value):
#	""" Verify value for verify_use_incredibuild_durango """
#	if not _is_user_option_true(value):
#		return (True,"","")	
#	(res, warning, error) = _verify_incredibuild_licence(ib_lic_xb1, 'Durango')	
#	return (res, warning, error)

	
@register_attribute_callback		
def use_incredibuild_orbis(ctx, section_name, option_name, value):
	""" If Incredibuild should be used, check for required packages """
	if LOADED_OPTIONS.get('use_incredibuild', 'False') == 'False':
		return 'False'
		
	(isValid, warning, error) = ATTRIBUTE_VERIFICATION_CALLBACKS['verify_use_incredibuild_orbis'](ctx, option_name, value)	
	if not isValid:
		return 'False'
	return value
	
@register_verify_attribute_callback
def verify_use_incredibuild_orbis(ctx, option_name, value):
	""" Verify value for verify_use_incredibuild_orbis """		
	if not _is_user_option_true(value):
		return (True,"","")	
	(res, warning, error) = _verify_incredibuild_licence(ib_lic_ps4, 'Orbis')	
	return (res, warning, error)

@register_verify_attribute_callback
def verify_auto_run_bootstrap(ctx, option_name, value):
	""" Verify auto run bootstrap """
	res = False
	warning = ""
	error = ""
	
	if not _is_user_option_true(value):
		res = True
		return (res, warning, error)
	
	try:
		subprocess.check_output(['p4']) # use check output as we do not want to spam the waf cmd window
		res = True
	except subprocess.CalledProcessError:
		# The process ran but did not return 0.
		# All we want to check here is that p4 exists.
		# Hence this is valid
		res = True
	except:
		error = "[ERROR] Unable to execute 'p4'"
		res = False
		
	return (res, warning, error)

@register_verify_attribute_callback
def verify_bootstrap_dat_file(ctx, option_name, value):
	""" Verify bootstrap dat file """
	res = True
	error = ""
	
	node = ctx.root.make_node(Context.launch_dir).make_node(value)
	if not os.path.exists(node.abspath()):
		error = 'Could not find Bootstrap.dat file: "%s"' % node.abspath()
		res = False
	
	return (res,"", error)

@Utils.run_once
def _print_auto_detect_warning():
	Logs.warn('[WARNING] No default compiler found, enabling compiler auto-detection')

def _validate_auto_detect_compiler(ctx):
	"""Force compiler auto-detection if bootstrap is not available."""
	if not ctx.is_option_true('auto_detect_compiler') and not ctx.is_bootstrap_available():
		_print_auto_detect_warning()
		setattr(ctx.options, 'auto_detect_compiler', 'True')

###############################################################################
@register_attribute_callback		
def specs_to_include_in_project_generation(ctx, section_name, option_name, value):	
	""" Configure all Specs which should be included in Visual Studio """
	if LOADED_OPTIONS.get('generate_vs_projects_automatically', 'False') == 'False':
		return ''
		
	if not ctx.is_option_true('ask_for_user_input'): # allow null value for specs list
		return value	
	
	info_str = ['Specify which specs to include when generating Visual Studio projects.']
	info_str.append('Each spec defines a list of dependent project for that modules .')
	
	# GUI
	if not ctx.is_option_true('console_mode'):
		return ctx.gui_get_attribute(section_name, option_name, value, '\n'.join(info_str))
	
	# Console	
	info_str.append("\nQuick option(s) (separate by comma):")
	
	spec_list = ctx.loaded_specs()
	spec_list.sort()
	for idx , spec in enumerate(spec_list):
		output = '   %s: %s: ' % (idx, spec)
		while len(output) < 25:
			output += ' '
		output += ctx.spec_description(spec)
		info_str.append(output)
		
	info_str.append("(Press ENTER to keep the current default value shown in [])")
	Logs.info('\n'.join(info_str))
	
	while True:
		specs = _get_string_value(ctx, 'Comma separated spec list', value)		
		spec_input_list = specs.replace(' ', '').split(',')
				
		# Replace quick options
		options_valid = True
		for spec_idx, spec_name in enumerate(spec_input_list):
			if spec_name.isdigit():
				option_idx = int(spec_name)
				try:
					spec_input_list[spec_idx] = spec_list[option_idx]
				except:
					Logs.warn('[WARNING] - Invalid option: "%s"' % option_idx)
					options_valid = False
					
		if not options_valid:
			continue
					
		specs = ','.join(spec_input_list)	
		(res, warning, error) = ATTRIBUTE_VERIFICATION_CALLBACKS['verify_specs_to_include_in_project_generation'](ctx, option_name, specs)
		
		if error:
			Logs.warn(error)
			continue
			
		return specs


###############################################################################
@register_verify_attribute_callback
def verify_specs_to_include_in_project_generation(ctx, option_name, value):	
	""" Configure all Specs which should be included in Visual Studio """	
	if not value:
		if not ctx.is_option_true('ask_for_user_input'): # allow null value for specs list
			return (True, "", "")
		if not ctx.is_option_true('generate_vs_projects_automatically'): # allow null value if we're not auto-generating projects
			return (True, "", "")
		error = ' [ERROR] - Empty spec list not supported'
		return (False, "", error)
		
	spec_list =  ctx.loaded_specs()
	spec_input_list = value.strip().replace(' ', '').split(',')	
	
	# Get number of occurrences per item in list
	num_of_occurrences = Counter(spec_input_list)	
	
	for input in spec_input_list:
		# Ensure spec is valid
		if not input in spec_list:
			error = ' [ERROR] Unkown spec: "%s".' % input
			return (False, "", error)
		# Ensure each spec only exists once in list
		elif not num_of_occurrences[input] == 1:
			error = ' [ERROR] Multiple occurrences of "%s" in final spec value: "%s"' % (input, value)
			return (False, "", error)
		
	return True, "", ""	


###############################################################################
@register_hint_attribute_callback
def hint_specs_to_include_in_project_generation(ctx, section_name, option_name, value):
	""" Hint list of specs for projection generation """
	spec_list = sorted(ctx.loaded_specs())
	desc_list = []
	
	for spec in spec_list:
		desc_list.append(ctx.spec_description(spec))			

	return (spec_list, spec_list, desc_list, "multi")


###############################################################################
def options(ctx):
	""" Create options for all entries in default_settings.json """
	_load_default_settings_file(ctx)
			
	default_settings_file = _default_settings_node(ctx).abspath()
		
	# Load default settings
	for settings_group, settings_list in ctx.default_settings.items():		
		group = ctx.add_option_group(settings_group)		

		# Iterate over all options in this group
		for settings in settings_list:
			
			# Do some basic sanity checking for each option
			if not settings.get('attribute', None):
				ctx.cry_file_error('One Option in group %s is missing mandatory setting "attribute"' % settings_group, default_settings_file)
			if not settings.get('long_form', None):
				ctx.cry_file_error('Option "%s" is missing mandatory setting "long_form"' % settings['attribute'], default_settings_file)
			if not settings.get('description', None):
				ctx.cry_file_error('Option "%s" is missing mandatory setting "description"' % settings['attribute'], default_settings_file)
				
			#	Add option with its default value
			if settings.get('short_form'):
				group.add_option(settings['short_form'], settings['long_form'],  dest=settings['attribute'], action='store', default=str(settings.get('default_value', '')), help=settings['description'])
			else:
				group.add_option(settings['long_form'],  dest=settings['attribute'], action='store', default=str(settings.get('default_value', '')), help=settings['description'])							
			
###############################################################################
@conf
def get_user_settings_node(ctx):
	""" Return a Node object pointing to the user_settings.options file """
	return ctx.root.make_node(Context.launch_dir).make_node('_WAF_/user_settings.options')
	
###############################################################################
@conf 
def	load_user_settings(ctx):
	""" Apply all loaded options if they are different that the default value, and no cmd line value is presented """
	global user_settings

	_load_default_settings_file(ctx)

	write_user_settings		= False	
	user_settings 				= ConfigParser.ConfigParser()	
	user_setting_file 		= ctx.get_user_settings_node().abspath()
	new_options   				= {}
		
	# Load existing user settings
	if not os.path.exists( user_setting_file ):
		write_user_settings = True	# No file, hence we need to write it
	else:
		user_settings.read( [user_setting_file] )
		
	# Load settings and check for newly set ones
	for section_name, settings_list in ctx.default_settings.items():
		
		# Add not already present sections
		if not user_settings.has_section(section_name):
			user_settings.add_section(section_name)
			write_user_settings = True
			
		# Iterate over all options in this group
		for settings in settings_list:
			option_name 	= settings['attribute']
			default_value = settings.get('default_value', '')

			# Load the value from user settings if it is already present	
			if  user_settings.has_option(section_name, option_name):
				value = user_settings.get(section_name, settings['attribute'])
				LOADED_OPTIONS[ option_name ] = value
			else:
				# Add info about newly added option
				if not new_options.has_key(section_name):
					new_options[section_name] = []
				
				new_options[section_name].append(option_name)
				
				# Load value for current option and stringify it
				value = settings.get('default_value', '')
				if getattr(ctx.options, option_name) != value:
					value = getattr(ctx.options, option_name)
					
				if not isinstance(value, str):
					value = str(value)
					
				if	ATTRIBUTE_CALLBACKS.get(option_name, None):					
					value = ATTRIBUTE_CALLBACKS[option_name](ctx, section_name, settings['attribute'], value)
				
				(isValid, warning, error) = ctx.verify_settings_option(option_name, value)
								
				# Add option
				if isValid:
					user_settings.set( section_name, settings['attribute'], str(value) )
					LOADED_OPTIONS[ option_name ] = value
					write_user_settings = True

			# Check for settings provided by the cmd line
			long_form			= settings['long_form']
			short_form		= settings.get('short_form', None)
			
			# Settings on cmdline should have priority, do a sub string match to batch both --option=<SomeThing> and --option <Something>			
			bOptionSetOnCmdLine = False
			for arg in sys.argv:
				if long_form in arg:	
					bOptionSetOnCmdLine = True
					value = getattr(ctx.options, option_name)
					break
			for arg in sys.argv:
				if short_form and short_form in arg:
					bOptionSetOnCmdLine = True
					value = getattr(ctx.options, option_name)
					break
					
			# Remember option for internal processing
			if bOptionSetOnCmdLine:
				LOADED_OPTIONS[ option_name ] = value			
			elif user_settings.has_option(section_name, option_name): # Load all settings not coming form the cmd line from the config file
				setattr(ctx.options, option_name, user_settings.get(section_name, option_name))

	# Ensure that the user_settings file only contains valid options that are part of the default list
	if not write_user_settings:
		for section in user_settings.sections():
			# Check for "section" in default list else remove				
			if section not in ctx.default_settings:
				Logs.warn("[user_settings.options]: Removing section: \"[%s]\" as it is not part of the supported settings found in default_settings.options" % (section))
				user_settings.remove_section(section)
				write_user_settings = True
				continue
	
			# Check for "option" in default list else remove
			for option in user_settings.options(section):
				found_match = False
				for option_desc in ctx.default_settings[section]: # loop over all option descriptions :(
					if option_desc['attribute'] == option:
						found_match = True
						break						
				if not found_match:
					Logs.warn("[user_settings.options]: Removing entry: \"%s=%s\" from section: \"[%s]\" as it is not part of the supported settings found in default_settings.options" % (option, user_settings.get(section, option), section))
					user_settings.remove_option(section, option)
					write_user_settings = True
		
	# Write user settings
	if write_user_settings:
		ctx.save_user_settings(user_settings)

  # Verify IB registry settings after loading all options
	_validate_incredibuild_registry_settings(ctx)
	_validate_auto_detect_compiler(ctx)
			
	return (user_settings, new_options)


###############################################################################
@conf
def get_default_settings(ctx, section_name, setting_name):
	default_value = ""
	default_description = ""
	
	if not hasattr(ctx, 'default_settings'):
		return (default_value, default_description)
		
	for settings_group, settings_list in ctx.default_settings.items():
		if settings_group == section_name:
			for settings in settings_list:
				if settings['attribute'] == setting_name:
					default_value = settings.get('default_value', '')
					default_description = settings.get('description', '')
					break
			break
	
	return (default_value, default_description)


###############################################################################
@conf 
def	save_user_settings(ctx, config_parser):
	# Write user settings	
	with open(ctx.get_user_settings_node().abspath(), 'wb') as configfile:
		config_parser.write(configfile)

		
###############################################################################
@conf
def verify_settings_option(ctx, option_name, value):		
	verify_fn_name = "verify_" + option_name	
	if ATTRIBUTE_VERIFICATION_CALLBACKS.get(verify_fn_name, None):
		return ATTRIBUTE_VERIFICATION_CALLBACKS[verify_fn_name](ctx, option_name, value)
	else:
		return (True, "", "")

	
###############################################################################
@conf
def hint_settings_option(ctx, section_name, option_name, value):		
	hint_fn_name = "hint_" + option_name
	if ATTRIBUTE_HINT_CALLBACKS.get(hint_fn_name, None):
		return ATTRIBUTE_HINT_CALLBACKS[hint_fn_name](ctx, section_name, option_name, value)
	else:
		return ([], [], [], "single")


###############################################################################
@conf
def set_settings_option(ctx, section_name, option_name, value):	
	global user_settings
	user_settings.set(section_name, option_name, str(value))
	LOADED_OPTIONS[option_name] = value	


###############################################################################
@conf
def check_user_settings_files(self):	
	local_user_settings_node = self.get_user_settings_node()
	if os.path.isfile(local_user_settings_node.abspath()):	
		self.hash = hash((self.hash, local_user_settings_node.read('rb')))
		self.files.append(os.path.normpath(local_user_settings_node.abspath()))
