#!../../bin/linux-x86_64/lexiumioc


< envPaths

epicsEnvSet("SYS",           "XF:12ID1-CT")
epicsEnvSet("IOC_PREFIX",    "$(SYS){IOC:Slt1}")
epicsEnvSet("STREAM_PROTOCOL_PATH", "$(TOP)/protocols")

cd ${TOP}
## Register all support components
dbLoadDatabase("dbd/lexiumioc.dbd")
lexiumioc_registerRecordDeviceDriver(pdbbase) 


##########################################
# Set up ASYN ports
# drvAsynIPPortConfigure("asynport","mdrive-ip:tcpport",priority,noAutoconnect,noProcessEOS)
drvAsynIPPortConfigure("P1","192.168.0.71:503",0,0,0)
drvAsynIPPortConfigure("P2","192.168.0.72:503",0,0,0)
drvAsynIPPortConfigure("P3","192.168.0.73:503",0,0,0)
drvAsynIPPortConfigure("P4","192.168.0.74:503",0,0,0)

# Set up Motor Controller
#LexiumCreateController("MotorPort", "AsynPort", "", MovingPoll_ms, IdlePoll_ms)
#ethernet motors do not support party mode, so set arg3 to "".
LexiumCreateController("M1", "P1", "", 100, 1000)
LexiumCreateController("M2", "P2", "", 100, 1000)
LexiumCreateController("M3", "P3", "", 100, 1000)
LexiumCreateController("M4", "P4", "", 100, 1000)

## Load record instances
dbLoadTemplate("db/motor.substitutions")
dbLoadTemplate("db/clearlock.substitutions")
dbLoadRecords("db/asynComm.substitutions")

## autosave/restore machinery
save_restoreSet_Debug(0)
save_restoreSet_IncompleteSetsOk(1)
save_restoreSet_DatedBackupFiles(1)

set_savefile_path("${TOP}/as","/save")
set_requestfile_path("${TOP}/as","/req")

system("install -m 777 -d ${TOP}/as/save")
system("install -m 777 -d ${TOP}/as/req")

set_pass0_restoreFile("info_positions.sav")
set_pass0_restoreFile("info_settings.sav")
set_pass1_restoreFile("info_settings.sav")

dbLoadRecords("$(EPICS_BASE)/db/save_restoreStatus.db","P=$(IOC_PREFIX)")
dbLoadRecords("$(EPICS_BASE)/db/iocAdminSoft.db","IOC=$(IOC_PREFIX)")
save_restoreSet_status_prefix("$(IOC_PREFIX)")

cd ${TOP}/iocBoot/${IOC}

iocInit()

## more autosave/restore machinery
cd ${TOP}/as/req
makeAutosaveFiles()
create_monitor_set("info_positions.req", 5 , "")
create_monitor_set("info_settings.req", 15 , "")

cd ${TOP}
dbl > ./records.dbl

