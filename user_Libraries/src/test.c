/* Test functions */
#include "stm32f4xx.h"
#include "test.h"
#include "delay.h"
#include "sensor_Function.h"
#include "pwm.h"
#include "led.h"
#include "encoder.h"
#include "global.h"
#include "speedProfile.h"
#include "config.h"
#include "turn.h"
#include "buzzer.h"
#include "align.h"
#include "maze.h"
#include <stdio.h>


/**
 *	Hug Front Wall
 */
void hugFrontWall(int LSensorVal, int RSensorVal) {
	while (1) {
		int curt = micros(); //start to track time in order to make one adjust every 1000us
		readSensor();
		setLeftPwm(LSensorVal - LFSensor);
		setRightPwm(RSensorVal - RFSensor);
		elapseMicros(1000, curt); //elapse 1000 micro seconds
	}
}


void ledTest (void) {
	if (LFSensor > frontWallThresholdL)
		LED2_ON;
	else
		LED2_OFF;
	
	if (LDSensor > leftWallThreshold)
		LED1_ON;
	else
		LED1_OFF;
	
	if (RDSensor > rightWallThreshold)
		LED4_ON;
	else
		LED4_OFF;
	
	if (RFSensor > frontWallThresholdR)
		LED3_ON;
	else
		LED3_OFF;
}

/*	
		Function: wheelOffsetTest()
		Parameters: motor speed, ontime
		Return: right - left encoder count
 */
int wheelOffsetTest(int speed, int ontime) {
	resetLeftEncCount();
	resetRightEncCount();

	setLeftPwm(speed);
	setRightPwm(speed);    
	delay_ms(ontime);
	turnMotorOff; 
	delay_ms(100);
	
	return getRightEncCount() - getLeftEncCount();
}



/*
 * Random movements using pivot turns
 */
void randomMovement(void) {
	isWaiting = 0;
	isSearching = 1;
	isSpeedRunning = 0;
	
	int cellCount = 1;						// number of explored cells
	int turnCount = 0;
	int remainingDist = 0;				// positional distance
	bool beginCellFlag = 0;
	bool quarterCellFlag = 0;
	bool halfCellFlag = 0;
	bool threeQuarterCellFlag = 0;
	bool fullCellFlag = 0;
	bool hasFrontWall = 0;
	bool hasLeftWall = 0;
	bool hasRightWall = 0;
	int nextMove = 0;
	
	targetSpeedX = searchSpeed;
	
	while(1) {	// run forever
		remainingDist = cellCount*cellDistance - encCount;
		
		if (!beginCellFlag && (remainingDist <= cellDistance))	{	// run once
			beginCellFlag = 1;
			
			useIRSensors = 1;
			useGyro = 0;
			useSpeedProfile = 1;
		}
		
		// Reached quarter cell
		if (!quarterCellFlag && (remainingDist <= cellDistance*3/4))	{
			quarterCellFlag = 1;
		}
		
		if (quarterCellFlag && !threeQuarterCellFlag)
			useIRSensors = 1;
		
		
		// Reached half cell
		if (!halfCellFlag && (remainingDist <= cellDistance/2)) {		// Run once
			halfCellFlag = 1;
			// Read wall and set wall flags
			if ((LFSensor > frontWallThresholdL) && (RFSensor > frontWallThresholdR))
				hasFrontWall = 1;
			if (LDSensor > leftWallThreshold)
				hasLeftWall = 1;
			if (RDSensor > rightWallThreshold)
				hasRightWall = 1;
			
			// Store destination cell's wall data
			
			// Decide next movement (search algorithm)
			while (1) {
				nextMove = (millis() % 4) + 1;
				if ((nextMove == GOFORWARD) && (!hasFrontWall))
					break;	
				if ((nextMove == TURNLEFT) && (!hasLeftWall))
					break;
				if ((nextMove == TURNRIGHT) && (!hasRightWall))
					break;
				if ((nextMove == TURNBACK) && (hasFrontWall && hasLeftWall && hasRightWall))
					break;
			}

		}
		
		// Reached three quarter cell
		if (!threeQuarterCellFlag && (remainingDist <= cellDistance*1/4)) {	// run once
			threeQuarterCellFlag = 1;
		}
		
		
		if (threeQuarterCellFlag) {
			// Check for front wall to turn off for the remaining distance
			if (hasFrontWall)
				useIRSensors = 0;
		}
		
		// If has front wall or needs to turn, decelerate to 0 within half a cell distance
		if (hasFrontWall || nextMove == TURNLEFT || nextMove == TURNRIGHT || nextMove == TURNBACK) {
			if(needToDecelerate(remainingDist, (int)speed_to_counts(curSpeedX), (int)speed_to_counts(stopSpeed)) < decX)
				targetSpeedX = searchSpeed;
			else
				targetSpeedX = stopSpeed;
		}
		
		// Reached full cell
		if ((!fullCellFlag && (remainingDist <= 0)) || (LFSensor > 2000) || (RFSensor > 2000)) {
			fullCellFlag = 1;
			cellCount++;
			shortBeep(200, 1000);
			
			// If has front wall, align with front wall
			if (hasFrontWall) {
				alignFrontWall(1360, 1330, 100);	// left, right value
			}
			
			
			// Reached full cell, perform next move
			if (nextMove == TURNLEFT) {
				pivotTurn(turnLeft90);
				turnCount++;
			}
			else if (nextMove == TURNRIGHT) {
				pivotTurn(turnRight90);
				turnCount++;
			}
			else if (nextMove == TURNBACK) {
				pivotTurn(turnLeft180);
				turnCount++;
			}
			else if (nextMove == GOFORWARD) {
				// Continue moving forward
			}
			
			beginCellFlag = 0;
			quarterCellFlag = 0;
			halfCellFlag = 0;
			threeQuarterCellFlag = 0;
			fullCellFlag = 0;
			hasFrontWall = 0;
			hasLeftWall = 0;
			hasRightWall = 0;
			
		}
	}
	
}

void speedRun(void) {
	isWaiting = 0;
	isSearching = 0;
	isSpeedRunning = 1;
	
	int cellCount = 1;						// number of explored cells
	int remainingDist = 0;				// positional distance
	bool beginCellFlag = 0;
	bool quarterCellFlag = 0;
	bool halfCellFlag = 0;
	bool threeQuarterCellFlag = 0;
	bool fullCellFlag = 0;

	int distN = 0;   // distances around current position
  int distE = 0;
  int distS = 0;
  int distW = 0;
	
	// Close off untraced routes
	closeUntracedCells();
	
	// Update distanced
	updateDistance();
	
	visualizeGrid();
	
	// Starting cell
	xPos = 0;
	yPos = 0;
	orientation = 'N';
	
	resetLeftEncCount();
	resetRightEncCount();

  //TODO BEFORE SETTING SPEED, CALCULATE ACTUAL MOVES TO MAKE 
  
  /*
  while(!atCenter())
  {
    int currCellCount = cellCount;

    while(!hasNorth(block[yPos][xPos]))
    {

    }

  }
  */

  //TODO AFTERWARDS, WE HAVE A QUEUE WITH THE MOVES TO DO 
  //(OR JUST RUN THE DISTANCES)
  //SET SPEED
	targetSpeedX = maxSpeed;
	
	while(!atCenter()) {
		remainingDist = cellCount*cellDistance - encCount;
		
		// Beginning of cell
		if (!beginCellFlag && (remainingDist <= cellDistance))	{	// run once
			beginCellFlag = 1;
			
			useIRSensors = 1;
			useGyro = 0;
			useSpeedProfile = 1;
			
			// Update position
			if (orientation == 'N') {
				yPos += 1;
			}
			if (orientation == 'E') {
				xPos += 1;
			}
			if (orientation == 'S') {
				yPos -= 1;
			}
			if (orientation == 'W') {
				xPos -= 1;
			}
		}
		
		// Reached quarter cell
		if (!quarterCellFlag && (remainingDist <= cellDistance*3/4))	{	// run once
			quarterCellFlag = 1;
		}
		
		if (quarterCellFlag && !threeQuarterCellFlag)	// middle half
			useIRSensors = 1;
		
		
		// Reached half cell
		if (!halfCellFlag && (remainingDist <= cellDistance/2)) {		// Run once
			halfCellFlag = 1;
			
			// Get distances around current block
			distN = (hasNorth(block[yPos][xPos]))? 500 : distance[yPos + 1][xPos];
			distE = (hasEast(block[yPos][xPos]))? 500 : distance[yPos][xPos + 1];
			distS = (hasSouth(block[yPos][xPos]))? 500 : distance[yPos - 1][xPos];
			distW = (hasWest(block[yPos][xPos]))? 500 : distance[yPos][xPos - 1];
			
			// Decide next movement
			if (DEBUG) printf("Deciding next movement\n\r");
			// 1. Pick the shortest route
			if ( (distN < distE) && (distN < distS) && (distN < distW) )
				nextMove = MOVEN;
			else if ( (distE < distN) && (distE < distS) && (distE < distW) )
				nextMove = MOVEE;
			else if ( (distS < distE) && (distS < distN) && (distS < distW) )
				nextMove = MOVES;
			else if ( (distW < distE) && (distW < distS) && (distW < distN) )
				nextMove = MOVEW;
			 
			// 2. If multiple equally short routes, go straight if possible
			else if ( orientation == 'N' && !hasNorth(block[yPos][xPos]) )
				nextMove = MOVEN;
			else if ( orientation == 'E' && !hasEast(block[yPos][xPos]) )
				nextMove = MOVEE;
			else if ( orientation == 'S' && !hasSouth(block[yPos][xPos]) )
				nextMove = MOVES;
			else if ( orientation == 'W' && !hasWest(block[yPos][xPos]) )
				nextMove = MOVEW;
			 
			// 3. Otherwise prioritize N > E > S > W
			else if (!hasNorth(block[yPos][xPos]))
				nextMove = MOVEN;
			else if (!hasEast(block[yPos][xPos]))
				nextMove = MOVEE;
			else if (!hasSouth(block[yPos][xPos]))
				nextMove = MOVES;
			else if (!hasWest(block[yPos][xPos]))
				nextMove = MOVEW;
			
			if (DEBUG) printf("nextMove %d\n\r", nextMove);
			
			else {
				if (DEBUG) {
					printf("Stuck... Can't find center.\n\r");
					turnMotorOff;
					useSpeedProfile = 0;
					while(1);
				}
			}
		}
		
		// Reached three quarter cell
		if (!threeQuarterCellFlag && (remainingDist <= cellDistance*1/4)) {	// run once
			threeQuarterCellFlag = 1;
		}
		
		
		if (threeQuarterCellFlag) {
		}
		
		// If has front wall or needs to turn, decelerate to 0 within half a cell distance
		if (willTurn()) {
			if(needToDecelerate(remainingDist, (int)speed_to_counts(curSpeedX), (int)speed_to_counts(stopSpeed)) < decX)
				targetSpeedX = maxSpeed;
			else
				targetSpeedX = stopSpeed;
		}
		else 
			targetSpeedX = maxSpeed;
		
		// Reached full cell
		if ((!fullCellFlag && (remainingDist <= 0)) || (LFSensor > 2500) || (RFSensor > 2500)) {	// run once
			if (DEBUG) printf("Reached full cell\n\r");
			fullCellFlag = 1;
			cellCount++;
			shortBeep(200, 1000);
			
			// Reached full cell, perform next move
			if (nextMove == MOVEN) {
				printf("Moving N\n\r");
				moveN();
			}
			else if (nextMove == MOVEE) {
				printf("Moving E\n\r");
				moveE();
			}
			else if (nextMove == MOVES) {
				printf("Moving S\n\r");
				moveS();
			}
			else if (nextMove == MOVEW) {
				printf("Moving W\n\r");
				moveW();
			}
			
			beginCellFlag = 0;
			quarterCellFlag = 0;
			halfCellFlag = 0;
			threeQuarterCellFlag = 0;
			fullCellFlag = 0;
		}
	}
	
	targetSpeedX = 0;
	
	// Reached center
	// Move one more cell
	alignFrontWall(1360, 1330, 1000);
	
	// Update position
	if (orientation == 'N') {
		yPos += 1;
	}
	if (orientation == 'E') {
		xPos += 1;
	}
	if (orientation == 'S') {
		yPos -= 1;
	}
	if (orientation == 'W') {
		xPos -= 1;
	}
		
	useSpeedProfile = 0;
	turnMotorOff;
	
	if (DEBUG) visualizeGrid();
}

void closeUntracedCells(void) {
	int j, k;
	for (j = 0; j < SIZE; j++) {
		for (k = 0; k < SIZE; k++) {
			if (!hasTrace(block[j][k]))
      {
				block[j][k] |= 15;
        if(j < SIZE - 1)
          block[j+1][k] |= 4;
        if(j > 0)
          block[j-1][k] |= 1;
        if(k < SIZE - 1)
          block[j][k+1] |= 8;
        if(k > 0)
          block[j][k-1] |= 2;
      }
		}
	}
}
