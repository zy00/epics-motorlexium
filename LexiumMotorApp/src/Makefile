TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build a support library

LIBRARY_IOC += LexiumMotor
# lexium.dbd will be created and installed
DBD += LexiumMotor.dbd

# lexium_registerRecordDeviceDriver.cpp derives from lexium.dbd
LexiumMotor_SRCS += LexiumMotorController.cpp
LexiumMotor_SRCS += LexiumMotorAxis.cpp


LexiumMotor_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

