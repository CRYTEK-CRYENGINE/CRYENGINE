#!/bin/bash
#
# createMacExec.sh
#
# $Id: createMacExec.sh,v 1.1 2008/10/03 20:59:58 anonymous Exp $
#
# This is the Mac exec installation script.
# This creates the necessary files and folders needed to execute shell scripts
# under OSX.
#
# Installation instructions:
#
# To install the OSX shell script support, open a terminal window and change to
# the parent directory, possibly Presets/Scripts
#
# Run this script:
#   % ./createMacExec.sh
#
# When this script runs, it creates a folder called 'macexec.app' and some
# folders and files underneath which allows Exec from xexec.js to call
# shell scripts.
#
# Example:
#   alert(Exec.system("ls -l ~/"));
#
# Specify the desired parent directory here
# Exec defaults to using this path
IDIR="/Applications/Adobe Photoshop CS4/Presets/Scripts"

if [ ! -e "$IDIR" ]; then
   echo Bad directory specified ${IDIR}
   exit 1
fi

cd "$IDIR"
mkdir -p macexec.app
mkdir -p macexec.app/Contents
rm -f macexec.app/Contents/macexec
touch macexec.app/Contents/macexec
chmod 755 macexec.app/Contents/macexec

# this is a hook for later so that shell variables (e.g. PATH)
# can be autoloaded from macexec

cat > macexec.app/Contents/macexec.env << EOF
#!/bin/bash
EOF

echo macexec successfully installed

# EOT
