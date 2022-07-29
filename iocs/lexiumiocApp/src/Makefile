TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

PROD_IOC = lexiumioc
# lexiumioc.dbd will be created and installed
DBD += lexiumioc.dbd

# lexiumioc.dbd will be made up from these files:
lexiumioc_DBD += base.dbd

# Include dbd files from all support applications:
lexiumioc_DBD += asyn.dbd drvAsynIPPort.dbd motorSupport.dbd LexiumMotor.dbd
lexiumioc_DBD += asSupport.dbd calcSupport.dbd stream.dbd
lexiumioc_DBD += devIocStats.dbd system.dbd

# Add all the support libraries needed by this IOC
lexiumioc_LIBS += asyn motor LexiumMotor
lexiumioc_LIBS += autosave calc stream
lexiumioc_LIBS += devIocStats 


# lexiumioc_registerRecordDeviceDriver.cpp derives from lexiumioc.dbd
lexiumioc_SRCS += lexiumioc_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
lexiumioc_SRCS_DEFAULT += lexiumiocMain.cpp
lexiumioc_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#lexiumioc_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
lexiumioc_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

