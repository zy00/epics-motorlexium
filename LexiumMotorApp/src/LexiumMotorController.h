//!  Description : This is the "model 3" asyn motor driver for Lexium.
//!                Based on HytecMotorDriver.h.
//! @Lexium.h

#ifndef LexiumMotorController_H
#define LexiumMotorController_H

#include "asynMotorController.h"
#include "asynMotorAxis.h"
#include "LexiumMotorAxis.h"

////////////////////////////////////
//  LexiumMotorController class
//! derived from asynMotorController class
////////////////////////////////////
class epicsShareClass LexiumMotorController : public asynMotorController
{
public:
	/////////////////////////////////////////
	// Override asynMotorController functions
	/////////////////////////////////////////
	LexiumMotorController(const char *motorPortName, const char *IOPortName, const char *deviceName, double movingPollPeriod, double idlePollPeriod);
	LexiumMotorAxis* getAxis(asynUser *pasynUser);
	LexiumMotorAxis* getAxis(int axisNo);
	//void report(FILE *fp, int level);
	asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);

	/////////////////////////////////////////
	// Lexium specific functions
	/////////////////////////////////////////
	asynStatus writeReadController(const char *output, char *input, size_t maxChars, size_t *nread, double timeout);
	asynStatus writeController(const char *output, double timeout);
	// add this to read PR IS  - for lexium
    asynStatus writeReadController2(const char *output, char *input, size_t maxChars, size_t *nread, double timeout);

	

protected:
	 LexiumMotorAxis **pAxes_;       // Array of pointers to axis objects

	///////////////////////////////////////////
	// Lexium controller function codes
	///////////////////////////////////////////

	//! extra parameters that the Lexium controller supports
	int LexiumLoadMCode_;    //! Load MCode string, NOT SUPPORTED YET
	int LexiumClearMCode_;   //! Clear program buffer, NOT SUPPORTED YET
	int LexiumSaveToNVM_;    //! Store current user variables and flags to nonvolatile ram
#define FIRST_Lexium_PARAM LexiumLoadMCode_
#define LAST_Lexium_PARAM LexiumSaveToNVM_
#define NUM_Lexium_PARAMS (&LAST_Lexium_PARAM - &FIRST_Lexium_PARAM + 1)

private:
// drvInfo strings for extra parameters that the Lexium controller supports
#define LexiumLoadMCodeControlString	"Lexium_LOADMCODE"    // NOT SUPPORTED YET
#define LexiumClearMCodeControlString	"Lexium_CLEARMCODE"   // NOT SUPPORTED YET
#define LexiumSaveToNVMControlString	"Lexium_SAVETONVM"

	asynUser *pAsynUserLexium;
	char motorName[MAX_NAME_LEN];
	char deviceName[MAX_NAME_LEN];
	int homeSwitchInput;
	int posLimitSwitchInput;
	int negLimitSwitchInput;

	void initController(const char *devName, double movingPollPeriod, double idlePollPeriod);
	int readHomeAndLimitConfig();  // read home, positive limit, and neg limit switch configuration from controller (S1-S4 settings)

	friend class LexiumMotorAxis;
};
//! iocsh function to create controller object
//! NOTE: drvAsynIPPortConfigure() must be called first
//extern "C" int LexiumCreateController(const char *motorPortName, const char *IOPortName, char *devName, double movingPollPeriod, double idlePollPeriod);

#endif // LexiumMotorController_H

