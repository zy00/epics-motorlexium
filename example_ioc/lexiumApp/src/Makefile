TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

PROD_IOC = lexium
# lexium.dbd will be created and installed
DBD += lexium.dbd

# lexium.dbd will be made up from these files:
lexium_DBD += base.dbd

# Include dbd files from all support applications:
lexium_DBD += asyn.dbd drvAsynIPPort.dbd motorSupport.dbd LexiumMotor.dbd
lexium_DBD += asSupport.dbd calcSupport.dbd stream.dbd
lexium_DBD += devIocStats.dbd system.dbd

# Add all the support libraries needed by this IOC
lexium_LIBS += asyn motor LexiumMotor
lexium_LIBS += autosave calc stream
lexium_LIBS += devIocStats 


# lexium_registerRecordDeviceDriver.cpp derives from lexium.dbd
lexium_SRCS += lexium_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
lexium_SRCS_DEFAULT += lexiumMain.cpp
lexium_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#lexium_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
lexium_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

