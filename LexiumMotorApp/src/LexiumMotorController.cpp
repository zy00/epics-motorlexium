//! @File : LexiumController.cpp
//!         Motor record driver level support for Intelligent Motion Systems, Inc.
//!         Lexium Mdrive series; M17, M23, M34.
//!	      Simple implementation using "model 3" asynMotorController and asynMotorAxis base classes (derived from asynPortDriver)
//!
//!  Copied most of the code from Mdriveplus driver from SLAC (Nia Fong) in motor record
//!  EOS different from Mdriveplus, as well as command for reading limit/home  
//!  ZY, zyin@bnl.gov
//!
//!  Assumptions :
//!    Assume single controller-card-axis relationship; 1 controller = 1 axis.  
//!    No address on mdriveplus, config "" for device name
//
//  Revision History
//  ----------------
//  02-2019  ZY copied from Mdriveplus driver to support Lexium Ethernet model

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <exception>
#include <epicsThread.h>
#include <iocsh.h>
#include <asynOctetSyncIO.h>

#include "asynMotorController.h"
#include "asynMotorAxis.h"
#include "LexiumMotorAxis.h"

#include <epicsExport.h>
#include "LexiumMotorController.h"

////////////////////////////////////////////////////////
//! @LexiumMotorController()
//! Constructor
//! Driver assumes only 1 axis configured per controller for now...
//!
//! @param[in] motorPortName     Name assigned to the port created to communicate with the motor
//! @param[in] IOPortName        Name assigned to the asyn IO port, name that was assigned in drvAsynIPPortConfigure()
//! @param[in] devName           Name of device (DN) assigned to motor axis in MCode, the device name is prepended to the MCode command to support Party Mode (PY) multidrop communication setup
//!                              set to empty string "" if no device name needed/not using Party Mode
//! @param[in] movingPollPeriod  Moving polling period in milliseconds
//! @param[in] idlePollPeriod    Idle polling period in milliseconds
////////////////////////////////////////////////////////
LexiumMotorController::LexiumMotorController(const char *motorPortName, const char *IOPortName, const char *devName, double movingPollPeriod, double idlePollPeriod)
    : asynMotorController(motorPortName, NUM_AXES, NUM_Lexium_PARAMS,
						  asynInt32Mask | asynFloat64Mask | asynUInt32DigitalMask,
						  asynInt32Mask | asynFloat64Mask | asynUInt32DigitalMask,
						  ASYN_CANBLOCK | ASYN_MULTIDEVICE,
						  1, // autoconnect
						  0, 0),  // Default priority and stack size
    pAsynUserLexium(0)
{
	static const char *functionName = "LexiumMotorController()";
	asynStatus status;
	LexiumMotorAxis *pAxis;
	// asynMotorController constructor calloc's memory for array of axis pointers
	pAxes_ = (LexiumMotorAxis **)(asynMotorController::pAxes_);

	// copy names
	strcpy(motorName, motorPortName);

	// setup communication
	status = pasynOctetSyncIO->connect(IOPortName, 0, &pAsynUserLexium, NULL);
	if (status != asynSuccess) {
		printf("\n\n%s:%s: ERROR connecting to Controller's IO port=%s\n\n", DRIVER_NAME, functionName, IOPortName);
	}

	// write version, cannot use asynPrint() in constructor since controller (motorPortName) hasn't been created yet
//	printf("%s:%s: motorPortName=%s, IOPortName=%s, devName=%s \n", DRIVER_NAME, functionName, motorPortName, IOPortName, devName);
	printf("==> motorPort = %s: ",  motorPortName);

	// init
  // ZY: for LEXIUM Mdrive, EM=2, OEOS = "\r", IEOS="\r\n". 
    pasynOctetSyncIO->setInputEos(pAsynUserLexium, "\r\n", 2);
	pasynOctetSyncIO->setOutputEos(pAsynUserLexium, "\r", 1); 

	// Create controller-specific parameters
	createParam(LexiumSaveToNVMControlString, asynParamInt32, &LexiumSaveToNVM_);
	createParam(LexiumLoadMCodeControlString, asynParamOctet, &this->LexiumLoadMCode_);
	createParam(LexiumClearMCodeControlString, asynParamOctet, &this->LexiumClearMCode_);

	// Check the validity of the arguments and init controller object
	initController(devName, movingPollPeriod, idlePollPeriod);

	// Create axis
	// Assuming single axis per controller the way drvAsynIPPortConfigure( "M06", "ts-b34-nw08:2101", 0, 0 0 ) is called in st.cmd script
	pAxis = new LexiumMotorAxis(this, 0);
	pAxis = NULL;  // asynMotorController constructor tracking array of axis pointers

	// read home and limit config from Response from "PR IS"
	readHomeAndLimitConfig();

	startPoller(movingPollPeriod, idlePollPeriod, 2);
}

////////////////////////////////////////
//! initController()
//! config controller variables - axis, etc.
//! only support single axis per controller
//
//! @param[in] devName Name of device (DN) used to identify axis within controller for Party Mode
//! @param[in] movingPollPeriod  Moving polling period in milliseconds
//! @param[in] idlePollPeriod    Idle polling period in milliseconds
////////////////////////////////////////
void LexiumMotorController::initController(const char *devName, double movingPollPeriod, double idlePollPeriod)
{
	strcpy(this->deviceName, devName);

	// initialize asynMotorController variables
	this->numAxes_ = NUM_AXES;  // only support single axis
	this->movingPollPeriod_ = movingPollPeriod;
	this->idlePollPeriod_ = idlePollPeriod;

	// initialize switch inputs
	this->homeSwitchInput=-1;
	this->posLimitSwitchInput=-1;
	this->negLimitSwitchInput=-1;

	// flush io buffer
	pasynOctetSyncIO->flush(pAsynUserLexium);
}

////////////////////////////////////////
//! readHomeAndLimitConfig
//! read home, positive limit, and neg limit switch configuration from MCode S1-S4 settings
//! Limits must be set up beforehand with IS command: 
// IS = <input#>, <type>, <active>
//! I1-I4 are used to read the status of
//  Use logic from existing drvMDrive.cc
////////////////////////////////////////
int LexiumMotorController::readHomeAndLimitConfig()
{
	asynStatus status = asynError;
	char cmd[MAX_CMD_LEN];
	char resp[MAX_BUFF_LEN];
	size_t nread;
	static const char *functionName = "readHomeAndLimitConfig()";
	int type;
    int type_signal;
    int bytesread = 0;
    int i, bytespass=0;

	// iterate through response of IS = and parse each configuration to see if home, pos, and neg limits are set
		sprintf(cmd, "PR IS"); // query IS setting
//		status = this->writeReadController2(cmd, resp, sizeof(resp), &nread, Lexium_TIMEOUT);
//		Hardwired to use 0.1 sec as timeout below, IEOS of "" will wait until timeout
		status = this->writeReadController2(cmd, resp, sizeof(resp), &nread, 0.1);
//        printf("resp:\n %s, length = %d\n", resp, nread);
	for (int j=0; j<=4; j++) {
        sscanf(resp + bytespass, "IS = %d, %d, %d\r\n%n", &i, &type, &type_signal, &bytesread);
		// asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s:%s: S%d: type=%d\n", DRIVER_NAME, functionName, i, type);
  //      printf("j=%d; i=%d; type=%d; signal=%d; bytesread=%d; bytespass=%d\n", j, i, type, type_signal, bytesread, bytespass);
		if (type != 0)
			//printf("%s:%s: S%d: type=%d\n", DRIVER_NAME, functionName, i, type);
		switch (type) {
		case 0: break; // general purpose input
		case 1: // home switch input
			homeSwitchInput = i; break;
		case 2: // positive limit switch input
			posLimitSwitchInput = i; break;
		case 3: // negative limit switch input
			negLimitSwitchInput = i; break;
		default:
			printf("%s:%s: ERROR invalid data type for IS%d=%d\n", DRIVER_NAME, functionName, i, type);
		}
        bytespass += bytesread ;
	}
	
	printf("+LimitInput = %d,  -LimitInput = %d,   homeSwitch = %d\n", posLimitSwitchInput, negLimitSwitchInput,homeSwitchInput);

	return status;
}

////////////////////////////////////////
//! getAxis()
//! Override asynMotorController function to return pointer to Lexium axis object
//
//! Returns a pointer to an LexiumAxis object.
//! Returns NULL if the axis number encoded in pasynUser is invalid
//
//! param[in] pasynUser asynUser structure that encodes the axis index number
////////////////////////////////////////
LexiumMotorAxis* LexiumMotorController::getAxis(asynUser *pasynUser)
{
	int axisNo;

	getAddress(pasynUser, &axisNo);
	return getAxis(axisNo);
}

////////////////////////////////////////
//! getAxis()
//! Override asynMotorController function to return pointer to Lexium axis object
//
//! Returns a pointer to an LexiumAxis object.
//! Returns NULL if the axis number is invalid.
//
//! param[in] axisNo Axis index number
////////////////////////////////////////
LexiumMotorAxis* LexiumMotorController::getAxis(int axisNo)
{
	if ((axisNo < 0) || (axisNo >= numAxes_)) return NULL;
	return pAxes_[axisNo];
}

////////////////////////////////////////
//! writeInt32()
//! Override asynMotorController function to add hooks to Lexium records
// Based on XPSController.cpp
//
//! param[in] pointer to asynUser object
//! param[in] value to pass to function
////////////////////////////////////////
asynStatus LexiumMotorController::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
	int reason = pasynUser->reason;
	int status = asynSuccess;
	LexiumMotorAxis *pAxis;
	static const char *functionName = "writeInt32";

	pAxis = this->getAxis(pasynUser);
	if (!pAxis) return asynError;

	//asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s:%s: function=%s, val=%d\n", DRIVER_NAME, functionName, value);
	asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s: function=%s, val=%d\n", DRIVER_NAME, functionName, value);

	// Set the parameter and readback in the parameter library.  This may be overwritten when we read back the
	// status at the end, but that's OK
	status = pAxis->setIntegerParam(reason, value);

	if (reason == LexiumSaveToNVM_) {
		if (value == 1) { // save current user parameters to NVM
			status = pAxis->saveToNVM();
			if (status) asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s:%s: ERROR saving to NVM\n", DRIVER_NAME, functionName);
			else asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "%s:%s: Successfully saved to NVM\n", DRIVER_NAME, functionName);
		} else {
			asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s:%s: ERROR, value of 1 to save to NVM\n", DRIVER_NAME, functionName);
		}
	} else { // call base class method to continue handling
			status = asynMotorController::writeInt32(pasynUser, value);
	}

	callParamCallbacks(pAxis->axisNo_);
	return (asynStatus)status;
}

////////////////////////////////////////
//! writeController()
//! reference ACRMotorDriver
//
//! Writes a string to the Lexium controller.
//! Prepends deviceName to command string, if party mode not enabled, set device name to ""
//! @param[in] output the string to be written.
//! @param[in] timeout Timeout before returning an error.
////////////////////////////////////////
asynStatus LexiumMotorController::writeController(const char *output, double timeout)
{
	size_t nwrite;
	asynStatus status;
	char outbuff[MAX_BUFF_LEN];
	static const char *functionName = "writeController()";

	// in party-mode Line Feed must follow command string
	sprintf(outbuff, "%s%s", deviceName, output);
	asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s:%s: deviceName=%s, command=%s\n", DRIVER_NAME, functionName, deviceName, outbuff);
	status = pasynOctetSyncIO->write(pAsynUserLexium, outbuff, strlen(outbuff), timeout, &nwrite);
	if (status) { // update comm flag
		setIntegerParam(this->motorStatusCommsError_, 1);
	}
	return status ;
}

////////////////////////////////////////
//! writeReadController()
//! reference ACRMotorDriver
//
//! Writes a string to the Lexium controller and reads a response.
//! Prepends deviceName to command string, if party mode not enabled, set device name to ""
//! param[in] output Pointer to the output string.
//! param[out] input Pointer to the input string location.
//! param[in] maxChars Size of the input buffer.
//! param[out] nread Number of characters read.
//! param[out] timeout Timeout before returning an error.*/
////////////////////////////////////////
asynStatus LexiumMotorController::writeReadController(const char *output, char *input, size_t maxChars, size_t *nread, double timeout)
{
	size_t nwrite;
	asynStatus status;
	int eomReason;
	char outbuff[MAX_BUFF_LEN];
	static const char *functionName = "writeReadController()";

	sprintf(outbuff, "%s%s", deviceName, output);
	status = pasynOctetSyncIO->writeRead(pAsynUserLexium, outbuff, strlen(outbuff), input, maxChars, timeout, &nwrite, nread, &eomReason);
	if (status) { // update comm flag
		setIntegerParam(this->motorStatusCommsError_, 1);
	}
	asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s:%s: deviceName=%s, command=%s, response=%s\n", DRIVER_NAME, functionName, deviceName, outbuff, input);
	return status;
}

////////////////////////////////////////
//! writeReadController2()
//! Writes a string to the Lexium controller and reads a response.
//! Differing from writeReadController() is IEOS is now set to ""(NULL)
//! This is to get correct read from controller for "PR IS" command,
//! which has multi-line IS = 1,0,1\r\nIS=2,2,1 ... 
//! with IEOS set to \r\n, only the first line is read
////////////////////////////////////////
asynStatus LexiumMotorController::writeReadController2(const char *output, char *input, size_t maxChars, size_t *nread, double timeout)
{
	size_t nwrite;
	asynStatus status;
	int eomReason;
	char outbuff[MAX_BUFF_LEN];
	static const char *functionName = "writeReadController2()";
     
    pasynOctetSyncIO->setInputEos(pAsynUserLexium, "", 0);
	
    sprintf(outbuff, "%s%s", deviceName, output);
	status = pasynOctetSyncIO->writeRead(pAsynUserLexium, outbuff, strlen(outbuff), input, maxChars, timeout, &nwrite, nread, &eomReason);
	if (status) { // update comm flag
		setIntegerParam(this->motorStatusCommsError_, 1);
	}
	asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s:%s: deviceName=%s, command=%s, response=%s\n", DRIVER_NAME, functionName, deviceName, outbuff, input);
   
  // revert back IEOS to \r\n:
    pasynOctetSyncIO->setInputEos(pAsynUserLexium, "\r\n", 2);
	
    return status;
}

////////////////////////////////////////////////////////
////////////////////////////////////////////////////////
// Start code for iocsh Registration :
// Available Functions :
//   LexiumCreateController()
////////////////////////////////////////////////////////

////////////////////////////////////////////////////////
//! LexiumCreateController()
//! IOCSH function
//! Creates a new LexiumController object
//
//! Configuration command, called directly or from iocsh
//! @param[in] motorPortName     User-specific name of motor port
//! @param[in] IOPortName        User-specific name of port that was configured by drvAsynIPPortConfigure()
//! @param[in] deviceName        Name of device, used to address motor by MCODE in party mode
//                              If not using party mode, config LexiumCreateController() with empty string "" for deviceName
//! @param[in] movingPollPeriod  time in ms between polls when any axis is moving
//! @param[in] idlePollPeriod    time in ms between polls when no axis is moving
////////////////////////////////////////////////////////
extern "C" int LexiumCreateController(const char *motorPortName, const char *IOPortName, char *devName, double movingPollPeriod, double idlePollPeriod)
{
	LexiumMotorController *pImsController;
	pImsController = new LexiumMotorController(motorPortName, IOPortName, devName, movingPollPeriod/1000., idlePollPeriod/1000.);
	pImsController = NULL; 
	return(asynSuccess);
}

////////////////////////////////////////////////////////
// Lexium IOCSH Registration
// Copied from ACRMotorDriver.cpp
//
// Motor port name    : user-specified name of port
// IO port name       : user-specific name of port that was initialized with drvAsynIPPortConfigure()
// Device name        : name of device, used to address motor by MCODE in party mode
//                    : if not using party mode, config LexiumCreateController() with empty string "" for deviceName
// Moving poll period : time in ms between polls when any axis is moving
// Idle poll period   : time in ms between polls when no axis is moving
////////////////////////////////////////////////////////
static const iocshArg LexiumCreateControllerArg0 = {"Motor port name", iocshArgString};
static const iocshArg LexiumCreateControllerArg1 = {"IO port name", iocshArgString};
static const iocshArg LexiumCreateControllerArg2 = {"Device name", iocshArgString};
static const iocshArg LexiumCreateControllerArg3 = {"Moving poll period (ms)", iocshArgDouble};
static const iocshArg LexiumCreateControllerArg4 = {"Idle poll period (ms)", iocshArgDouble};
static const iocshArg * const LexiumCreateControllerArgs[] = {&LexiumCreateControllerArg0,
                                                                     &LexiumCreateControllerArg1,
                                                                     &LexiumCreateControllerArg2,
                                                                     &LexiumCreateControllerArg3,
                                                                     &LexiumCreateControllerArg4};
static const iocshFuncDef LexiumCreateControllerDef = {"LexiumCreateController", 5, LexiumCreateControllerArgs};
static void LexiumCreateControllerCallFunc(const iocshArgBuf *args)
{
	LexiumCreateController(args[0].sval, args[1].sval, args[2].sval, args[3].dval, args[4].dval);
}

static void LexiumMotorRegister(void)
{
	iocshRegister(&LexiumCreateControllerDef, LexiumCreateControllerCallFunc);
}

extern "C" {
	epicsExportRegistrar(LexiumMotorRegister);
}
