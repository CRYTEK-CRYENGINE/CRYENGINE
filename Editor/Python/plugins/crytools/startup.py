import imp
import os
import sys


import SandboxBridge


init_script = ''

# Check for init script specified by custom tools path
if not init_script:
    custom_tools_path = os.getenv("CRYTOOLSPATH")
    if custom_tools_path:
        custom_init_script_path = os.path.join(custom_tools_path, 'sandbox', 'init.py')
        if os.path.exists(custom_init_script_path):
            init_script = custom_init_script_path

# Check for init script included with build
if not init_script:
    engine_tools_path = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(sys.executable))), 'Tools')
    engine_init_script_path = os.path.join(engine_tools_path, 'sandbox', 'init.py')
    print "checking " + engine_init_script_path
    if os.path.exists(engine_init_script_path):
        init_script = engine_init_script_path

# Load init script if it has been found
if init_script:
    print 'Loading crytools from ' + os.path.dirname(init_script)
    imp.load_source('sandbox_init_script', init_script)
