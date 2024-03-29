//! @File : LexiumMotorAxis.cpp
//!         Motor record driver level support for Intelligent Motion Systems, Inc.
//!         MDrivePlus series; M17, M23, M34.
//!         Use "model 3" asyn motor, asynMotorController and asynMotorAxis classes.
//!
//! Original Author : Nia Fong
//! Date : 11-21-2011
//! Current Author : Mitch D'Ewart (SLAC)
//
//  Revision History
//  ----------------
//  11-21-2011  NF Initial version
//  12-15-2014 RLS Bug fix from Thierry Zamofing (PSI); acceleration was set to the same value as the speed.

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

#include <epicsExport.h>
#include "LexiumMotorController.h"

////////////////////////////////////////////////////////
//! LexiumMotorAxis()
//  Constructor
//
//! @param[in] pC pointer to LexiumMotorController
//! @param[in] axisNum axis number
////////////////////////////////////////////////////////
LexiumMotorAxis::LexiumMotorAxis(LexiumMotorController *pC, int axisNum)
  : asynMotorAxis(pC, axisNum), pController(pC)
{
	static const char *functionName = "LexiumMotorAxis()";
	asynPrint(pC->pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s:%s: Create Axis %d\n", DRIVER_NAME, functionName, axisNum);

    // run setup/initialize routines here
    // check communication, set moving status
	if (configAxis() == asynError) {
    	asynPrint(pC->pasynUserSelf, ASYN_TRACE_ERROR, "%s:%s: controller config failed for motor port=%s\n", DRIVER_NAME, functionName, pController->motorName);
    	// TODO throw exception
    }
	callParamCallbacks();
}

////////////////////////////////////////
//! configAxis()
//! Used smarACTMCMotorDriver.cpp as reference
//
//! check communication by checking version returns a good string
//! set moving status (PR MV)
////////////////////////////////////////
asynStatus LexiumMotorAxis::configAxis()
{
	asynStatus status = asynError;
	char cmd[MAX_CMD_LEN];
	char resp[MAX_BUFF_LEN];
	size_t nread;
	int maxRetries=3;
	static const char *functionName = "configAxis()";
	// figure out what needs to be done to initialize controller

	// try getting firmware version to make sure communication works
	sprintf(cmd, "PR VR");
	for (int i=0; i<maxRetries; i++) {
		status = pController->writeReadController(cmd, resp, sizeof(resp), &nread, Lexium_TIMEOUT);
		asynPrint(pController->pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s:%s: Version retry.\n", pController->motorName, functionName);
		if (status == asynError) {
			asynPrint(pController->pasynUserSelf, ASYN_TRACE_ERROR, "%s:%s: Version inquiry FAILED.\n", pController->motorName, functionName);
		} else { // ok to check firmware level/format or just strlen? v3.009
			if (strlen(resp) < 2) {
				asynPrint(pController->pasynUserSelf, ASYN_TRACE_ERROR, "%s:%s: Version inquiry FAILED version=%s.\n", pController->motorName, functionName, resp);
				setIntegerParam(pController->motorStatusProblem_, 1);
				setIntegerParam(pController->motorStatusCommsError_, 1);
				status = asynError; return(status);
			}
			break;
		}
	}

	// set encoder flags
	sprintf(cmd, "PR EE");
	status = pController->writeReadController(cmd, resp, sizeof(resp), &nread, Lexium_TIMEOUT);
	if (status == asynSuccess) {
		int val = atoi(resp);
		setIntegerParam(pController->motorStatusHasEncoder_, val ? 1:0);
		setIntegerParam(pController->motorStatusGainSupport_, val ? 1:0);
		asynPrint(pController->pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s:%s: set motorStatusHasEncoder_=%d, motorStatusGainSupport_=%d.\n", pController->motorName, functionName, val, val);
	}

	// start idle timer
	//idleTimeStart = epicsTime::getCurrent();

	return status;
}


////////////////////////////////////////////////////////
//! setAxisMoveParameters()
//! set base velocity, moving velocity, and acceleration
//
//! @param[in] minVelocity
//! @param[in] maxVelocity
//! @param[in] acceleration
////////////////////////////////////////////////////////
asynStatus LexiumMotorAxis::setAxisMoveParameters(double minVelocity, double maxVelocity, double acceleration)
{
	asynStatus status = asynError;
	char cmd[MAX_CMD_LEN];
	static const char *functionName = "setAxisMoveParameters()";

	// check if using base velocity (VI)
	// for MDrivePlus initial velocity must be below max_velocity
	if (minVelocity > 0) { // base velocity set
        // ZY: comment out the following minvel/max vel check, since this is done at motor record level
        // with this check, JOGR does not work, as it set maxvel to neg val, minvel>maxvel becomes true
		//if (minVelocity > maxVelocity ) { // illegal state
		//	asynPrint(pController->pasynUserSelf, ASYN_TRACE_ERROR, "%s:%s: base velocity=%f cannot be greater than max velocity=%f\n", DRIVER_NAME, functionName, minVelocity, maxVelocity);
		//	goto bail;
		//}

		// set base velocity
		sprintf(cmd, "VI=%ld", (long)minVelocity);
		status = pController->writeController(cmd, Lexium_TIMEOUT);
		if (status) goto bail;
	}


	// set velocity
	sprintf(cmd, "VM=%ld", (long)maxVelocity);
	status = pController->writeController(cmd, Lexium_TIMEOUT);
	if (status) goto bail;

	// set accceleration
	if (acceleration != 0) {
		sprintf(cmd, "A=%ld", (long)acceleration);
		status = pController->writeController(cmd, Lexium_TIMEOUT);
		if (status) goto bail;
	}

	bail:
	if (status) {
		char buff[LOCAL_LINE_LEN];
		sprintf(buff, "%s:%s: ERROR setting motor velocity and acceleration", pController->motorName, functionName);
		handleAxisError(buff);
	}

	callParamCallbacks();
	return status;
}

////////////////////////////////////////////////////////
//! move()
//! Override asynMotorAxis class implementation
// Based on smarActMCSMotorDriver.cpp
//
//! @param[in] position
//! @param[in] relative making absolute or relative move
//! @param[in] minVelocity
//! @param[in] maxVelocity
//! @param[in] acceleration
////////////////////////////////////////////////////////
asynStatus LexiumMotorAxis::move(double position, int relative, double minVelocity, double maxVelocity, double acceleration)
{
	asynStatus status = asynError;
	char cmd[MAX_CMD_LEN];
	static const char *functionName = "move()";

	// sent commands to motor to set velocities and acceleration
	status = setAxisMoveParameters(minVelocity, maxVelocity, acceleration);
	if (status) goto bail;

	// move
	asynPrint(pController->pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s:%s: VBASE=%f, VELO=%f, ACCL=%f, position=%f, relative=%d\n", pController->motorName, functionName, minVelocity, maxVelocity, acceleration, position, relative);
	if (relative) { // relative move MR
		sprintf(cmd, "MR %ld", (long)position);
	} else { // absolute move MA
		sprintf(cmd, "MA %ld", (long)position);
	}
	status = pController->writeController(cmd, Lexium_TIMEOUT);
	if (status) goto bail;

	bail:
	if (status) {
		char buff[LOCAL_LINE_LEN];
		sprintf(buff, "%s:%s: ERROR moving motor", pController->motorName, functionName);
		handleAxisError(buff);
	}

	callParamCallbacks();
	return status;
}

////////////////////////////////////////////////////////
//! moveVelocity()
//! Override asynMotorAxis class implementation
// Based on smarActMCSMotorDriver.cpp
//! move at a fixed velocity until stop() called
//
//! @param[in] minVelocity
//! @param[in] maxVelocity
//! @param[in] acceleration
////////////////////////////////////////////////////////
asynStatus LexiumMotorAxis::moveVelocity(double minVelocity, double maxVelocity, double acceleration)
{
	asynStatus status = asynError;
	char cmd[MAX_CMD_LEN];
	static const char *functionName = "moveVelocity()";

	// sent commands to motor to set velocities and acceleration
	status = setAxisMoveParameters(minVelocity, maxVelocity, acceleration);
	if (status) goto bail;

	// move
	asynPrint(pController->pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s:%s: VBASE=%f, VELO=%f, ACCL=%f\n", pController->motorName, functionName, minVelocity, maxVelocity, acceleration);
	sprintf(cmd, "SL %ld", (long)maxVelocity);
	status = pController->writeController(cmd, Lexium_TIMEOUT);
	if (status) goto bail;

	bail:
	if (status) {
		char buff[LOCAL_LINE_LEN];
		sprintf(buff, "%s:%s: ERROR jogging motor", pController->motorName, functionName);
		handleAxisError(buff);
	}

	callParamCallbacks();
	return status;
}

////////////////////////////////////////////////////////
//! stop()
//! Override asynMotorAxis class implementation
// Based on smarActMCSMotorDriver.cpp
//
//! @param[in] acceleration
////////////////////////////////////////////////////////
asynStatus LexiumMotorAxis::stop(double acceleration)
{
	asynStatus status = asynError;
	char cmd[MAX_CMD_LEN];
	static const char *functionName = "stop()";

	// set accceleration
	if (acceleration != 0) {
		sprintf(cmd, "A=%ld", (long)acceleration);
		status = pController->writeController(cmd, Lexium_TIMEOUT);
		if (status) goto bail;
	}

	// move
	sprintf(cmd, "SL 0");
	status = pController->writeController(cmd, Lexium_TIMEOUT);
	if (status) goto bail;

	bail:
	if (status) {
		char buff[LOCAL_LINE_LEN];
		sprintf(buff, "%s:%s: ERROR stopping motor", pController->motorName, functionName);
		handleAxisError(buff);
	}

	callParamCallbacks();
	return status;
}

////////////////////////////////////////////////////////
//! home()
//! Override asynMotorAxis class implementation
// Based on smarActMCSMotorDriver.cpp
//
//! direction=1: slew at maxVelocity in the minus direction (until switch activates) then creep at vi in the plus direction (until switch becomes inactive again)
//! direction=3: slew at maxVelocity in the plus direction (until switch activates) then creep at vi in the minus direction (until switch becomes inactive again)
//
//! @param[in] minVelocity
//! @param[in] maxVelocity
//! @param[in] acceleration
//! @param[in] forwards direction to home, 0=minus direction, 1=plus direction
////////////////////////////////////////////////////////
asynStatus LexiumMotorAxis::home(double minVelocity, double maxVelocity, double acceleration, int forwards)
{
	asynStatus status = asynError;
	char cmd[MAX_CMD_LEN];
	char resp[MAX_BUFF_LEN];
	size_t nread;
	int direction = 1;  // direction to home, initialize homing in minus direction
	double baseVelocity=0;
	static const char *functionName = "home()";

	// check if using base velocity (VI)
	// for MDrivePlus initial velocity must be below max_velocity
	if (minVelocity > 0) { // base velocity being configured
		if (minVelocity > maxVelocity ) { // illegal state
			asynPrint(pController->pasynUserSelf, ASYN_TRACE_ERROR, "%s:%s: base velocity=%f cannot be greater than max velocity=%f\n", pController->motorName, functionName, minVelocity, maxVelocity);
			goto bail;
		}
	} else { // base velocity needs to be set because creeping back to home switch at base velocity, so make sure it's nonzero
		sprintf(cmd, "PR VI");  // get base velocity setting
		status = pController->writeReadController(cmd, resp, sizeof(resp), &nread, Lexium_TIMEOUT);
		if (status) goto bail;
		baseVelocity = atof(resp);
		if (baseVelocity == 0) { // set to factory default of 1000
			baseVelocity=1000;
		}
	}

	// sent commands to motor to set velocities and acceleration
	if (status = setAxisMoveParameters(minVelocity, maxVelocity, acceleration)) goto bail;

	// home
	if (forwards == 1) { // homing in forward direction
		direction = 3;
	}
	asynPrint(pController->pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s:%s: VBASE=%f, VELO=%f, ACCL=%f, forwards=%d\n", pController->motorName, functionName, minVelocity, maxVelocity, acceleration, forwards);
	sprintf(cmd, "HM %d", direction);
	status  = pController->writeController(cmd, Lexium_TIMEOUT);
	if (status) goto bail;

	bail:
	if (status) {
		char buff[LOCAL_LINE_LEN];
		sprintf(buff, "%s:%s: ERROR homing motor", pController->motorName, functionName);
		handleAxisError(buff);
	}

	callParamCallbacks();
	return status;
}

////////////////////////////////////////////////////////
//! setPosition()
//! Override asynMotorAxis class implementation
// Based on smarActMCSMotorDriver.cpp
//
//! @param[in] position value to set motor's internal position
////////////////////////////////////////////////////////
asynStatus LexiumMotorAxis::setPosition(double position)
{
	asynStatus status = asynError;
	char cmd[MAX_CMD_LEN];
	static const char *functionName = "setPosition()";

	// set position
	asynPrint(pController->pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s:%s: position=%f\n", pController->motorName, functionName, position);
	  sprintf(cmd, "P=%ld", (long)position);
	status = pController->writeController(cmd, Lexium_TIMEOUT);
     // ZY add cmd to set C1 and C2 to the position for internal encoders 
     // (DAExxx model).
        sprintf(cmd, "C1=%ld", (long)position);
        status = pController->writeController(cmd, Lexium_TIMEOUT);
        sprintf(cmd, "C2=%ld", (long)position*4000/51200);
        status = pController->writeController(cmd, Lexium_TIMEOUT);
	if (status) goto bail;

	bail:
	if (status) {
		char buff[LOCAL_LINE_LEN];
		sprintf(buff, "%s:%s: ERROR setting motor position", pController->motorName, functionName);
		handleAxisError(buff);
	}

	callParamCallbacks();
	return status;
}


////////////////////////////////////////////////////////
//! poll()
//! Override asynMotorAxis class implementation
// Based on smarActMCSMotorDriver.cpp
//
//! Set position and moving flag
//
//! @param[in] moving pointer to moving flag
////////////////////////////////////////////////////////
asynStatus LexiumMotorAxis::poll(bool *moving)
{
	asynStatus status = asynError;
	char cmd[MAX_CMD_LEN];
	char resp[MAX_BUFF_LEN];
	size_t nread;
	int val=0;
	double position;
	*moving = false;
	static const char *functionName = "poll()";
	//epicsTime currentTime;

	// get position
	sprintf(cmd, "PR P");
	status = pController->writeReadController(cmd, resp, sizeof(resp), &nread, Lexium_TIMEOUT);
	if (status) goto bail;
	position = atof(resp);
	// update motor record position values, just update encoder's even if not using one
	setDoubleParam(pController->motorEncoderPosition_, position);
	setDoubleParam(pController->motorPosition_, position);

	// get moving flag
	sprintf(cmd, "PR MV");
	status = pController->writeReadController(cmd, resp, sizeof(resp), &nread, Lexium_TIMEOUT);
	if (status) goto bail;
	val = atoi(resp);
	if (val == 1) *moving = true;  	// updating moving flag
/*	else { // not moving
		if (prevMovingState == 1) {// state changed, moving before, start idle timer
			idleTimeStart = epicsTime::getCurrent();
		}
	}
	prevMovingState = val;
*/
	// update motor record status done with moving status
	setIntegerParam(pController->motorStatusDone_, ! *moving );

/*
	// if position has changed and moving stopped for over 30sec, update NVM with current position so the motor will remember
	// its location just in case its power dies
	currentTime = epicsTime::getCurrent();
	idleTime = currentTime - idleTimeStart;
	//printf("prevPos=%f, pos=%f, moving=%d, idleTime=%f\n", prevPosition, position, *moving, idleTime);
	if (prevPosition != position && *moving == false && idleTime > 30) {
		sprintf(cmd, "S");
		if (status = pController->writeController(cmd, Lexium_TIMEOUT)) {
			asynPrint(pController->pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s:poll(): ERROR saving position to NVM\n", pController->motorName);
			goto bail;
		}
		asynPrint(pController->pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s:poll(): saving parameters to NVM\n", pController->motorName);
		idleTimeStart = epicsTime::getCurrent();
		prevPosition = position;
	}
*/

	// get home switch value
	if (pController->homeSwitchInput != -1) {
		sprintf(cmd, "PR I%d", pController->homeSwitchInput);
		status = pController->writeReadController(cmd, resp, sizeof(resp), &nread, Lexium_TIMEOUT);
		if (status) goto bail;
		val = atoi(resp);
		setIntegerParam(pController->motorStatusHome_, val);
	}

	// get positive limit switch value
	if (pController->posLimitSwitchInput != -1) {
		sprintf(cmd, "PR I%d", pController->posLimitSwitchInput);
		status = pController->writeReadController(cmd, resp, sizeof(resp), &nread, Lexium_TIMEOUT);
		if (status) goto bail;
		val = atoi(resp);
		setIntegerParam(pController->motorStatusHighLimit_, val);
	}

	// get negative limit switch value
	if (pController->negLimitSwitchInput != -1) {
		sprintf(cmd, "PR I%d", pController->negLimitSwitchInput);
		status = pController->writeReadController(cmd, resp, sizeof(resp), &nread, Lexium_TIMEOUT);
		if (status) goto bail;
		val = atoi(resp);
		setIntegerParam(pController->motorStatusLowLimit_, val);
	}

	// error polling
	bail:
	if (status) {
		char buff[LOCAL_LINE_LEN];
		sprintf(buff, "%s:%s: ERROR polling motor", pController->motorName, functionName);
		handleAxisError(buff);
	}

	// update motor status if polling was successful
	if (status == 0) {
		setIntegerParam(pController->motorStatusCommsError_, 0);  // reset COMM error
		setIntegerParam(pController->motorStatusProblem_, 0);     // reset problem error, bit 10
	}

	// update motor record
	callParamCallbacks();

	int mstat;
	pController->getIntegerParam(pController->motorStatus_, &mstat);
	asynPrint(pController->pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s:%s: POS=%f, MSTAT=%d\n", pController->motorName, functionName, position, mstat);

	return status;

}

////////////////////////////////////////////////////////
//! saveToNVM()
//! Save user variables and flags to non-volatile RAM in case of power loss
//
////////////////////////////////////////////////////////
asynStatus LexiumMotorAxis::saveToNVM()
{
	asynStatus status = asynError;
	char cmd[MAX_CMD_LEN];
	static const char *functionName = "saveToNVM()";

	// send save command
	sprintf(cmd, "S");
	status = pController->writeController(cmd, Lexium_TIMEOUT);
	if (status) goto bail;
	asynPrint(pController->pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s:%s: Saved to NVM\n", pController->motorName, functionName);

	bail:
	if (status) {
		char buff[LOCAL_LINE_LEN];
		sprintf(buff, "%s:%s: ERROR saving to NVM", pController->motorName, functionName);
		handleAxisError(buff);
	}

	callParamCallbacks(); //FIXME is this necessary??
	return status;
}

////////////////////////////////////////////////////////
//! handleAxisError()
//! Set motorStatusProblem_
//! Then try to get and print error code
//
////! @param[in] errMsg pointer to error message from calling function
////////////////////////////////////////////////////////
void LexiumMotorAxis::handleAxisError(char *errMsg)
{
	char cmd[MAX_CMD_LEN];
	char resp[MAX_BUFF_LEN];
	size_t nread=0;
	int errCode=0;
	char errCodeString[MAX_BUFF_LEN];
	static const char *functionName = "handleAxisError()";

	// set motorStatusProblem_ bit
	setIntegerParam(pController->motorStatusProblem_, 1);

	// read error code
	sprintf(cmd, "PR ER");
	pController->writeReadController(cmd, resp, sizeof(resp), &nread, Lexium_TIMEOUT);
	errCode = atoi(resp);

	switch (errCode) {
	case 0: strcpy(errCodeString, "No Error"); break;
	case 6: strcpy(errCodeString, "An I/O is already set to this type. Applies to non-General Purpose I/O"); break;
	case 8: strcpy(errCodeString, "Tried to set an I/O to an incorrect I/O type"); break;
	case 9: strcpy(errCodeString, "Tried to write to I/O set as Input or is 'TYPED'"); break;
	case 10: strcpy(errCodeString, "Illegal I/O number"); break;
	case 11: strcpy(errCodeString, "Incorrect CLOCK type"); break;
	case 12: strcpy(errCodeString, "Illegal Trip/Capture"); break;
	case 20: strcpy(errCodeString, "Tried to set unknown variable or flag"); break;
	case 21: strcpy(errCodeString, "Tried to set an incorrect value"); break;
	case 22: strcpy(errCodeString, "VI is set greater than or equal to VM"); break;
	case 23: strcpy(errCodeString, "VM is set less than or equal to VI"); break;
	case 24: strcpy(errCodeString, "Illegal data entered"); break;
	case 25: strcpy(errCodeString, "Variable or flag is read only"); break;
	case 26: strcpy(errCodeString, "Variable or flag is not allowed to be incremented or decremented"); break;
	case 27: strcpy(errCodeString, "Trip not defined"); break;
	case 28: strcpy(errCodeString, "Trying to redefine a program label or variable"); break;
	case 29: strcpy(errCodeString, "Trying to redefine a build in command, variable, or flag"); break;
	case 30: strcpy(errCodeString, "Unknown label or user variable"); break;
	case 31: strcpy(errCodeString, "Program label or user variable table is full"); break;
	case 32: strcpy(errCodeString, "Trying to set a label"); break;
	case 33: strcpy(errCodeString, "Trying to set and instruction"); break;
	case 34: strcpy(errCodeString, "Trying to execute a variable or flag"); break;
	case 35: strcpy(errCodeString, "Trying to print illegal variable or flag"); break;
	case 36: strcpy(errCodeString, "Illegal motor count to encoder count ratio"); break;
	case 37: strcpy(errCodeString, "Command, variable, or flag not available in drive"); break;
	case 38: strcpy(errCodeString, "Missing parameter separator"); break;
	case 39: strcpy(errCodeString, "Trip on position and trip on relative distance not allowed together"); break;
	case 40: strcpy(errCodeString, "Program not running"); break;
	case 41: strcpy(errCodeString, "Stack overflow"); break;
	case 42: strcpy(errCodeString, "Illegal program address"); break;
	case 43: strcpy(errCodeString, "Tried to overflow program stack"); break;
	case 44: strcpy(errCodeString, "Program locked"); break;
	case 45: strcpy(errCodeString, "Trying to overflow program space"); break;
	case 46: strcpy(errCodeString, "Not in program mode"); break;
	case 47: strcpy(errCodeString, "Tried to write in illegal flash address"); break;
	case 48: strcpy(errCodeString, "Program execution stopped by I/O set as stop"); break;
	case 61: strcpy(errCodeString, "Trying to set illegal baud rate"); break;
	case 62: strcpy(errCodeString, "IV already pending or IF flag already true"); break;
	case 63: strcpy(errCodeString, "Character over-run"); break;
	case 64: strcpy(errCodeString, "Startup calibration failed"); break;
	case 70: strcpy(errCodeString, "Flash check sum failed"); break;
	case 71: strcpy(errCodeString, "Internal temperature warning, 10 C to shutdown"); break;
	case 72: strcpy(errCodeString, "Internal over temp fault, disabling drive"); break;
	case 73: strcpy(errCodeString, "Tried to save while moving"); break;
	case 74: strcpy(errCodeString, "Tried to initialize parameters or clear program while moving"); break;
	case 75: strcpy(errCodeString, "Linear over temperature error"); break;
	case 80: strcpy(errCodeString, "Home switch not defined"); break;
	case 81: strcpy(errCodeString, "Home type not defined"); break;
	case 82: strcpy(errCodeString, "Went to both limits and did not find home"); break;
	case 83: strcpy(errCodeString, "Reached plus limit switch"); break;
	case 84: strcpy(errCodeString, "Reached minus limit switch"); break;
	case 85: strcpy(errCodeString, "MA or MR isn't allowed during home and home isn't allowed while moving"); break;
	case 86: strcpy(errCodeString, "Stall detected"); break;
	case 87: strcpy(errCodeString, "In clock mode"); break;
	case 88: strcpy(errCodeString, "Following error"); break;
	case 90: strcpy(errCodeString, "Motion variables are too low switching to EE=1"); break;
	case 91: strcpy(errCodeString, "Motion stopped by I/O set as stop"); break;
	case 92: strcpy(errCodeString, "Position error in closed loop"); break;
	case 93: strcpy(errCodeString, "MR or MA not allowed while correcting position"); break;
	case 100: strcpy(errCodeString, "Configuration test done, encoder resolution mismatch"); break;
	case 101: strcpy(errCodeString, "Configuration test done, encoder direction incorrect"); break;
	case 102: strcpy(errCodeString, "Configuration test done, encoder resolution and direction incorrect"); break;
	case 103: strcpy(errCodeString, "Configuration not done, drive not enabled"); break;
	case 104: strcpy(errCodeString, "Locked rotor, send CF to clear"); break;
	case 105: strcpy(errCodeString, "Maximum position count reached"); break;
	case 106: strcpy(errCodeString, "Lead limit reached"); break;
	case 107: strcpy(errCodeString, "Lag limit reached"); break;
	case 108: strcpy(errCodeString, "Lead/lag not zero at the end of a move"); break;
	case 109: strcpy(errCodeString, "Calibration failed because drive not enabled"); break;
	case 110: strcpy(errCodeString, "Make-up disabled"); break;
	case 111: strcpy(errCodeString, "Factory calibration failed"); break;
	}
  if(errCode !=0){
	asynPrint(pController->pasynUserSelf, ASYN_TRACE_ERROR, "%s:%s: %s, %d:'%s'\n", pController->motorName, functionName, errMsg, errCode, errCodeString);
   }
}
