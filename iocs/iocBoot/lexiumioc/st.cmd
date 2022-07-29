#!../../bin/linux-x86_64/lexiumioc

#- You may have to change lexiumioc to something else
#- everywhere it appears in this file

#< envPaths

## Register all support components
dbLoadDatabase("../../dbd/lexiumioc.dbd",0,0)
lexiumioc_registerRecordDeviceDriver(pdbbase) 

## Load record instances
dbLoadRecords("../../db/lexiumioc.db","user=zyin")

iocInit()

## Start any sequence programs
#seq snclexiumioc,"user=zyin"
