//  Description : This is the "model 3" asyn motor driver for Lexium.
//                Based on HytecMotorDriver.h.

#ifndef LexiumMotorAxis_H
#define LexiumMotorAxis_H

#include <epicsTime.h>

#include "asynMotorController.h"
#include "asynMotorAxis.h"

#define DRIVER_NAME "LexiumMotorDriver"

#define NUM_AXES 1
#define DEFAULT_NUM_CARDS 32
#define MAX_MESSAGES 100
#define Lexium_TIMEOUT 2
#define MAX_BUFF_LEN 80
#define MAX_CMD_LEN MAX_BUFF_LEN-10  // leave room for line feeds surrounding command
#define MAX_NAME_LEN 10
#define LOCAL_LINE_LEN 256

class epicsShareClass LexiumMotorController;

////////////////////////////////////
// LexiumMotorAxis class
// derived from asynMotorAxis class
////////////////////////////////////
class LexiumMotorAxis : public asynMotorAxis
{
public:
	///////////////////////////////////
	// Override asynMotorAxis functions
	///////////////////////////////////
	LexiumMotorAxis(LexiumMotorController *pC, int axis);
	asynStatus move(double position, int relative, double min_velocity, double max_velocity, double acceleration);
	asynStatus moveVelocity(double min_velocity, double max_velocity, double acceleration);
 	asynStatus home(double min_velocity, double max_velocity, double acceleration, int forwards);
  	asynStatus stop(double acceleration);
  	asynStatus poll(bool *moving);
  	asynStatus setPosition(double position);

	////////////////////////////////////////////////////
	// Lexium specific functions
	////////////////////////////////////////////////////
  	asynStatus saveToNVM();
protected:


private:
	LexiumMotorController *pController;

	//int useEncoder;                         //! using encoder flag
	// FIXME handle lost position in driver or ioc??
//	epicsTime idleTimeStart;                //! timer used to track idle time for saving to NVM
//	double prevPosition;                    //! previous position used to see if motor has moved and need to update position in NVM in case of power failure
//	int prevMovingState;                    //! saves previous moving value

	////////////////////////////////////////////////////
	// Lexium specific functions
	////////////////////////////////////////////////////
	asynStatus configAxis();
	asynStatus setAxisMoveParameters(double min_velocity, double max_velocity, double acceleration);
	void handleAxisError(char *errMsg);

friend class LexiumMotorController;
};

#endif // LexiumMotorAxis_H


