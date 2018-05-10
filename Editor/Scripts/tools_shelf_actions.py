# Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.
import time, re, os, sys, itertools, ast 
import general

global ctrlFile
ctrlFile = 'setState.txt'
global logfile
logfile = 'Editor.log'

def getLogList(logfile):
	logList = [x for x in open(logfile, 'r')]
	return logList

def getCheckList(ctrlFile):	
	try:
		with open(ctrlFile):
			stateList = open(ctrlFile, 'r')

	except:
		open(ctrlFile, 'w').close()
		stateList = open(ctrlFile, 'r')

	checkList = [x for x in stateList]
	return checkList
	
#toggle CVars--------------------------------------------------------------------#	

def toggleCvarsRestartCheck(log, state, mode, cVars, onValue, offValue, ctrlFile):
	if log in state:
		toggleCvarsV(mode, cVars, onValue, offValue, ctrlFile)
	else:
		stateList = open(ctrlFile, 'w')
		stateList.write(log)
		stateList = open(ctrlFile, 'r') 
		toggleCvarsV(mode, cVars, onValue, offValue, ctrlFile)
		
def toggleCvarsV(mode, cVars, onValue, offValue, ctrlFile):
	stateList = open(ctrlFile, 'r') 
	setState = [x for x in enumerate(stateList)] 
	blankCheck = [x for x in setState]
	getList = [x for x in stateList]
	
	if blankCheck == []:
		stateList = open(ctrlFile, 'w')
		stateList.write(''.join(str("%s,{'%s': %s}" % (mode, cVars, offValue))+'\n'))
		general.set_cvar(cVars, offValue)
	else:
		stateList = open(ctrlFile, 'r')
		checkFor = str([x for x in stateList])

		if mode not in str(checkFor):
			stateList = open(ctrlFile, 'r')
			getList = [x for x in stateList] 
			getList.insert(1, str("%s,{'%s': %s}\n" % (mode, cVars , offValue)))
			print str("{'%s': %s}\n" % (cVars , offValue))
			stateList = open(ctrlFile, 'w')
			stateList.write(''.join(getList))	
			general.set_cvar(cVars, offValue)
			
		else:
			stateList = open(ctrlFile, 'r')
			for d in enumerate(stateList):
				values = d[1].split(',')
				stateList = open(ctrlFile, 'r')
				getList = [x for x in stateList] 
				
				if mode in values[0]:
					getDict = ast.literal_eval(values[1])
					getState = getDict.get(cVars)
					
					if getState == offValue:
						getDict[cVars] = onValue
						joinStr = [mode,",",str(getDict), '\n']
						newLine = ''.join(joinStr)
						print getDict
						getList[d[0]] = newLine
						stateList = open(ctrlFile, 'w')
						stateList.write(''.join(str(''.join(getList))))
						general.set_cvar(cVars, onValue)
					else: 
						getDict[cVars] = offValue
						joinStr = [mode,",",str(getDict), '\n']
						newLine = ''.join(joinStr)
						print getDict
						getList[d[0]] = newLine
						stateList = open(ctrlFile, 'w')
						stateList.write(''.join(str(''.join(getList))))
						general.set_cvar(cVars, offValue)

def toggleCvarsValue(mode, cVars, onValue, offValue):
	logList = getLogList(logfile)
	checkList = getCheckList(ctrlFile)
	toggleCvarsRestartCheck(logList[1], checkList, mode, cVars, onValue, offValue, ctrlFile)	
							
#toggleConsol--------------------------------------------------------------------#		
					
def toggleConsolRestartCheck(log,state, mode, onValue, offValue, ctrlFile):	
	if log in state:
		toggleConsolV(mode, onValue, offValue, ctrlFile)
	else:
		stateList = open(ctrlFile, 'w')
		stateList.write(log)
		stateList = open(ctrlFile, 'r') 
		toggleConsolV(mode, onValue, offValue, ctrlFile)
	
def toggleConsolV(mode, onValue, offValue):
	stateList = open(ctrlFile, 'r') 
	setState 	= [x for x in enumerate(stateList)] 
	blankCheck 	= [x for x in setState]
	getList = [x for x in stateList]
	onOffList = [onValue, offValue]
	
	if blankCheck == []:
		stateList = open(ctrlFile, 'w')
		stateList.write(''.join(str("%s,'%s'" % (mode, offValue))+'\n'))
		general.run_console(offValue)
	else:
		stateList = open(ctrlFile, 'r')
		checkFor = str([x for x in stateList])
		
		if mode not in str(checkFor):
			stateList = open(ctrlFile, 'r')
			getList = [x for x in stateList] 
			getList.insert(1, str("%s,'%s'\n" % (mode, offValue)))
			print str("{'%s': %s}\n" % (cVars , offValue))
			stateList = open(ctrlFile, 'w')
			stateList.write(''.join(getList))	
			general.run_console(onValue)
			
		else:
			stateList = open(ctrlFile, 'r')
			for d in enumerate(stateList):
			
				values = d[1].split(',')
				
				stateList = open(ctrlFile, 'r')
				getList = [x for x in stateList] 
			
				if mode in values[0]:
					getDict = values[1]
					off = ["'",str(offValue),"'", '\n']
					joinoff = ''.join(off)

					if values[1] == joinoff:
						getDict = onValue
						joinStr = [mode,",","'",getDict,"'", '\n']
						newLine = ''.join(joinStr)
						print getDict
						getList[d[0]] = str(newLine)							
						stateList = open(ctrlFile, 'w')
						stateList.write(''.join(str(''.join(getList))))
						general.run_console(onValue)
					else: 
						getDict = offValue
						joinStr = [mode,",","'",getDict,"'", '\n']
						newLine = ''.join(joinStr)
						print getDict
						getList[d[0]] = str(newLine)					
						stateList = open(ctrlFile, 'w')
						stateList.write(''.join(str(''.join(getList))))
						general.run_console(offValue)			

def toggleConsolValue(log, state, mode, onValue, offValue):
	logList = getLogList(logfile)
	checkList = getCheckList(ctrlFile)
	toggleConsolRestartCheck(logList[1],checkList, mode, onValue, offValue, ctrlFile)
	
#cycleCvars----------------------------------------------------------------------#
		
def cycleCvarsRestartCheck(log, state, mode, cVars, cycleList, ctrlFile):	
	if log in state:
		cycleCvarsV(mode, cVars, cycleList, ctrlFile)
	else:
		stateList = open(ctrlFile, 'w')
		stateList.write(log)
		stateList = open(ctrlFile, 'r') 
		cycleCvarsV(mode, cVars, cycleList, ctrlFile)
	
def cycleCvarsV(mode, cVars, cycleList, ctrlFile):
	stateList = open(ctrlFile, 'r') 
	setState 	= [x for x in enumerate(stateList)] 
	blankCheck 	= [x for x in setState]
	getList = [x for x in stateList]

	if blankCheck == []:
		stateList = open(ctrlFile, 'w')
		stateList.write(''.join(str("%s,{'%s': %s}" % (mode, cVars, cycleList[1]))+'\n'))
		general.set_cvar(cVars, cycleList[1])
	else:
		stateList = open(ctrlFile, 'r')
		checkFor = str([x for x in stateList])
		
		if mode not in str(checkFor):
			stateList = open(ctrlFile, 'r')
			getList = [x for x in stateList] 
			getList.insert(1, str("%s,{'%s': %s}\n" % (mode, cVars , cycleList[1])))
			#print '%s' % cycleList[1]
			stateList = open(ctrlFile, 'w')
			stateList.write(''.join(getList))	
			general.set_cvar(cVars, cycleList[1])

		else:
			stateList = open(ctrlFile, 'r')
			
			for d in enumerate(stateList):
				stateList = open(ctrlFile, 'r')
				getList = [x for x in stateList] 
				values = d[1].split(',')
				
				if mode in values[0]:
					getDict = ast.literal_eval(values[1])
					getState = getDict.get(cVars)

					cycleL = [x for x in enumerate(cycleList)]

					for x in cycleL:
						if getState == x[1]:
							number = [n[1] for n in cycleL] 
							getMax = max(number)
							nextNum = x[0]+1
						
							if nextNum > getMax:
								getDict[cVars] = cycleList[0]
								joinStr = [mode,",",str(getDict), '\n']
								newLine = ''.join(joinStr)
								getList[d[0]] = newLine
								print getDict
								stateList = open(ctrlFile, 'w')
								stateList.write(''.join(str(''.join(getList))))
								general.set_cvar(cVars, cycleList[0])
							else:
								getDict[cVars] = cycleList[x[0]+1]
								joinStr = [mode,",",str(getDict), '\n']
								newLine = ''.join(joinStr)
								getList[d[0]] = newLine
								print getDict
								stateList = open(ctrlFile, 'w')
								stateList.write(''.join(str(''.join(getList))))
								general.set_cvar(cVars, cycleList[x[0]+1])

def cycleCvarsValue(log, state, mode, cVars, cycleList):
	logList = getLogList(logfile)
	checkList = getCheckList(ctrlFile)
	cycleCvarsRestartCheck(logList[1],checkList, mode, cVars, cycleList, ctrlFile)		
	
#cycleConsol----------------------------------------------------------------------#		
		
def cycleConsolRestartCheck(log, state, mode, cycleList, ctrlFile):	
	if log in state:
		cycleConsolV(mode, cycleList, ctrlFile)
	else:
		stateList = open(ctrlFile, 'w')
		stateList.write(log)
		stateList = open(ctrlFile, 'r') 
		cycleConsolV(mode, cycleList, ctrlFile)
	
def cycleConsolV(mode, cycleList, ctrlFile):
	stateList = open(ctrlFile, 'r') 
	setState 	= [x for x in enumerate(stateList)] 
	blankCheck 	= [x for x in setState]
	getList = [x for x in stateList]
	
	if blankCheck == []:
		stateList = open(ctrlFile, 'w')
		stateList.write(''.join(str("%s,'%s'" % (mode, cycleList[0]))+'\n'))
		general.run_console(cycleList[0])
	else:
		stateList = open(ctrlFile, 'r')
		checkFor = str([x for x in stateList])
				
		if mode not in str(checkFor):
			stateList = open(ctrlFile, 'r')
			getList = [x for x in stateList] 
			getList.insert(1, str("%s,'%s'\n" % (mode, cycleList[0])))
			#print '%s' % cycleList[0]
			stateList = open(ctrlFile, 'w')
			stateList.write(''.join(getList))	
			general.run_console(cycleList[0])
			
		else:
			stateList = open(ctrlFile, 'r')
			for d in enumerate(stateList):
				stateList = open(ctrlFile, 'r')
				getList = [x for x in stateList] 
				values = d[1].split(',')

				if mode in values[0]:
					newValue = ''.join(values[1].split('\n'))
					cycleL = [e for e in enumerate(cycleList)]
					getDict = ''.join(values[1].split('\n'))
					
					for x in cycleL:

						if newValue in "'%s'" % x[1]:
							number = [n[0] for n in cycleL] 
							getMax = max(number)
							nextNum = x[0]+1
							
							if nextNum > getMax:
								getDict = '%s' % cycleList[0]
								joinStr = [mode,",","'",getDict,"'", '\n']
								newLine = ''.join(joinStr)
								getList[d[0]] = newLine
								print getDict
								stateList = open(ctrlFile, 'w')
								stateList.write(''.join(str(''.join(getList))))
								general.run_console(getDict)
							else:
								getDict = '%s' % cycleList[x[0]+1]
								joinStr = [mode,",","'",getDict,"'", '\n']
								newLine = ''.join(joinStr)
								getList[d[0]] = newLine
								print getDict
								stateList = open(ctrlFile, 'w')
								stateList.write(''.join(str(''.join(getList))))
								general.run_console(getDict)
								
def cycleConsolValue(mode, cycleList):
	logList = getLogList(logfile)
	checkList = getCheckList(ctrlFile)
	cycleConsolRestartCheck(logList[1],checkList, mode, cycleList, ctrlFile)
	
def toggleHideMaskValues(type):
	if (general.get_hidemask(type)):
		general.set_hidemask(type, 0)
	else:
		general.set_hidemask(type, 1)

		
#toggleHide------------------------------------------------------------------------#
		
def toggleHideRestartCheck(log, state, mode, type, onValue, offValue, ctrlFile):
	if log in state:
		toggleHideByT(mode, type, onValue, offValue, ctrlFile)
	else:
		stateList = open(ctrlFile, 'w')
		stateList.write(log)
		stateList = open(ctrlFile, 'r') 
		toggleHideByT(mode, type, onValue, offValue, ctrlFile)

def toggleHideByType(mode, type, onValue, offValue):
	logList = getLogList(logfile)
	checkList = getCheckList(ctrlFile)
	toggleHideRestartCheck(logList[1],checkList, mode, type, onValue, offValue, ctrlFile)
	
def toggleHideByT(mode, type, onValue, offValue, ctrlFile):
	stateList = open(ctrlFile, 'r') 
	setState 	= [x for x in enumerate(stateList)] 
	blankCheck 	= [x for x in setState]
	getList = [x for x in stateList]
	
	if blankCheck == []:
		stateList = open(ctrlFile, 'w')
		stateList.write(''.join(str("%s,{'%s': %s}" % (mode, type, offValue))+'\n'))
		
		hideByType(type)
	else:
		stateList = open(ctrlFile, 'r')
		checkFor = str([x for x in stateList])
		
		if mode not in str(checkFor):
			stateList = open(ctrlFile, 'r')
			getList = [x for x in stateList] 
			getList.insert(1, str("%s,{'%s': %s}\n" % (mode, type , offValue)))
			print str("{'%s': %s}\n" % (type , offValue))
			stateList = open(ctrlFile, 'w')
			stateList.write(''.join(getList))	
			hideByType(type)

		else:
			stateList = open(ctrlFile, 'r')
			for d in enumerate(stateList):
				values = d[1].split(',')
				stateList = open(ctrlFile, 'r')
				getList = [x for x in stateList] 
				
				if mode in values[0]:
					getDict = ast.literal_eval(values[1])
					getState = getDict.get(type)
					
					if getState == offValue:
						getDict[type] = onValue
						joinStr = [mode,",",str(getDict), '\n']
						newLine = ''.join(joinStr)
						print getDict
						getList[d[0]] = newLine
						stateList = open(ctrlFile, 'w')
						stateList.write(''.join(str(''.join(getList))))
						unHideByType(type)
					else: 
						getDict[type] = offValue
						joinStr = [mode,",",str(getDict), '\n']
						newLine = ''.join(joinStr)
						print getDict
						getList[d[0]] = newLine
						stateList = open(ctrlFile, 'w')
						stateList.write(''.join(str(''.join(getList))))
						hideByType(type)

def hideByType(type):	
	typeList = object.get_all_objects(str(type), "")
	#print typeList
	#selection.select_objects(typeList)
	for x in typeList:
		object.hide(x)
			
def unHideByType(type):				
	typeList = object.get_all_objects(str(type), "")
	#print typeList
	#selection.select_objects(typeList)
	for x in typeList:
		object.show(x)