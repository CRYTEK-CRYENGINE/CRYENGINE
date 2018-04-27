# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
from waflib.Configure import conf
from waflib import Logs, Node, Errors
from waflib.Scripting import _is_option_true
from collections import OrderedDict
import json, os

@conf
def is_option_true(ctx, option_name):
	""" Util function to better intrepret all flavors of true/false """
	return _is_option_true(ctx.options, option_name)
	
#############################################################################
#############################################################################
# Helper functions to handle error and warning output
@conf
def cry_error(conf, msg):
	conf.fatal("error: %s" % msg) 
	
@conf
def cry_file_error(conf, msg, filePath, lineNum = 0 ):
	if isinstance(filePath, Node.Node):
		filePath = filePath.abspath()
	if not os.path.isabs(filePath):
		filePath = conf.path.make_node(filePath).abspath()
	conf.fatal('%s(%s): error: %s' % (filePath, lineNum, msg))
	
@conf
def cry_warning(conf, msg):
	Logs.warn("warning: %s" % msg) 
	
@conf
def cry_file_warning(conf, msg, filePath, lineNum = 0 ):
	Logs.warn('%s(%s): warning: %s.' % (filePath, lineNum, msg))
	
#############################################################################
#############################################################################	
# Helper functions to json file parsing and validation		
def _decode_unicode_to_utf8(input):
	if isinstance(input, dict):
		return dict((_decode_unicode_to_utf8(key), _decode_unicode_to_utf8(value)) for key, value in input.iteritems())
	elif isinstance(input, list):
		return [_decode_unicode_to_utf8(element) for element in input]
	elif isinstance(input, unicode):
		return input.encode('utf-8')
	else:
		return input

@conf
def parse_json_file(conf, file_node):

	# Do not follow python json implemented RFC 7159 standard.
	# Duplicate keys are not allowed. 
	def dupe_checking_hook(pairs):				
		result = OrderedDict()
		for key,val in pairs:
			key = _decode_unicode_to_utf8(key)
			val = _decode_unicode_to_utf8(val)
			if key in result:
				raise KeyError("Duplicate key specified: %s" % key)
			result[key] = val
		return result
		
	try:
		file_content_raw = file_node.read()
		file_content_parsed = json.loads(file_content_raw, object_hook=_decode_unicode_to_utf8, object_pairs_hook = dupe_checking_hook	)		
		return file_content_parsed
	except Exception as e:
		line_num = 0
		exception_str = str(e)
		
		# Handle invalid last entry in list error
		if "No JSON object could be decoded" in exception_str:
			cur_line = ""
			prev_line = ""
			file_content_by_line = file_content_raw.split('\n')
			for lineIndex, line in enumerate(file_content_by_line):
			
				# Sanitize string
				cur_line = ''.join(line.split())	
				
				# Handle empty line
				if not cur_line:
					continue
				
				# Check for invalid JSON schema
				if any(substring in (prev_line + cur_line) for substring in [",]", ",}"]):
					line_num = lineIndex
					exception_str = 'Invalid JSON, last list/dictionary entry should not end with a ",". [Original exception: "%s"]' % exception_str
					break;
					
				prev_line = cur_line
	  
		# If exception has not been handled yet
		if not line_num:
			# Search for 'line' in exception and output pure string
			exception_str_list = exception_str.split(" ")
			for index, elem in enumerate(exception_str_list):
				if elem == 'line':
					line_num = exception_str_list[index+1]
					break
					
		# Raise fatal error
		conf.cry_file_error(exception_str, file_node.abspath(), line_num)
	
@conf
def save_to_json_file(conf, file_node, data):
	try:
		#data_json = json.dumps(data)
		#Logs.info('JSON DATA: %s' % data_json)
		file_node.write(json.dumps(data))
	except Exception as e:
		line_num = 0
		exception_str = str(e)
		
		conf.cry_file_error(exception_str, file_node.abspath(), 0)
		
@conf
def ToVersionNumbers(conf, s):
	return [int(e) for e in s.split('.')]