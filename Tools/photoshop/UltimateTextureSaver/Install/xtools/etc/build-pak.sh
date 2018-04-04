#
# build-pak.sh
# $Id: build-pak.sh,v 1.17 2015/02/09 22:48:16 anonymous Exp $
#
REV=2_2

ZFILE="xtools-${REV}.zip"
ZPATH="/tmp/${ZFILE}"

#ZIP_CMD="7z a -r -tzip"    # WinXP
ZIP_CMD="zip -r"           # OS X

echo Building ${REV}

#
# Pre-build manual steps.
# 0) Change the REV tags where appropriate
# 1) Test on XP and Mac
# 2) Checkin code
# 3) Flatten xapps->apps
# 4) Verify that flattened scripts work
# 5) Convert odt docs to pdfs and checkin
# 6) Run this script
# 7) Do a test installation to verify the build
# 8) Tag for the new rev
#

cd /tmp
rm -rf xtools
cvs export -Dnow xtools
rm -f xtools.zip
cd xtools

cp xapps/Install.jsx .

perl -p -e 's/\n/\r\n/g' < ChangeLog > ChangeLog.txt
perl -p -e 's/\n/\r\n/g' < README > README.txt
perl -p -e 's/\n/\r\n/g' < ChangeLog >> README.txt
perl -p -e 's/\n/\r\n/g' < COPYRIGHT > COPYRIGHT.txt
perl -p -e 's/\n/\r\n/g' < INSTALLATION > INSTALLATION.txt

cd docs
perl -p -e 's/\n/\r\n/g' < README > README.txt

cd ../..


${ZIP_CMD} "${ZPATH}" xtools
rm -rf xtools
chmod 777 "${ZPATH}"

cp "${ZPATH}" ~/Desktop/"${ZFILE}"
ls -l ~/Desktop/"${ZFILE}"

# EOF
