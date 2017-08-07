# Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.
	
# Recode Settings
RECODE_LICENSE_PATH = 'Code/Tools/waf-1.7.13/recode.lic'

# Build Layout
BINTEMP_FOLDER = 'BinTemp'

# Build Configuration
COMPANY_NAME = 'Crytek GmbH'
COPYRIGHT = '(C) 2016 Crytek GmbH'

# Supported branch platforms/configurations
# This is a map of host -> target platforms
PLATFORMS = {
	'darwin': [ 'darwin_x64' ],
	'win32' : [ 'win_x86', 'win_x64', 'durango', 'orbis', 'android_arm', 'android_arm64' ],
	'linux':  [ 'linux_x86_gcc', 'linux_x64_gcc', 'linux_x86_clang', 'linux_x64_clang' ]
}
	
# List of build configurations to generate for each supported platform	
CONFIGURATIONS = [ 'debug', 'profile', 'performance', 'release' ]
