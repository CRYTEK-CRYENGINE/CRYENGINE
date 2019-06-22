#!/bin/sh

SQUISHSERVER=$1
SQUISHRUNNER=$2
TESTSUITE=$3
TESTCASE=$4
AUT=$5
AUTDIR=$6
SETTINGSGROUP=$7

$SQUISHSERVER --stop > /dev/null 2>&1

echo "Adding AUT... $SQUISHSERVER --settingsGroup $SETTINGSGROUP --config addAUT $AUT $AUTDIR"
$SQUISHSERVER --settingsGroup "$SETTINGSGROUP" --config addAUT "$AUT" "$AUTDIR" || exit 255
# sleep 1

echo "Starting the squish server... $SQUISHSERVER --daemon"
$SQUISHSERVER --daemon || exit 255
# sleep 2

echo "Running the test case...$SQUISHRUNNER --settingsGroup $SETTINGSGROUP --testsuite $TESTSUITE --testcase $TESTCASE"
$SQUISHRUNNER --settingsGroup "$SETTINGSGROUP" --testsuite "$TESTSUITE" --testcase "$TESTCASE"
returnValue=$?

echo "Stopping the squish server... $SQUISHSERVER --stop"
$SQUISHSERVER --stop

exit $returnValue
