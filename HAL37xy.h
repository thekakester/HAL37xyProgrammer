#ifndef _HAL37XY_H_
#define _HAL37XY_H_

#include <Arduino.h>

class HAL37xy {

	public:
	HAL37xy(int outputPin);
	uint16_t readAddress(int address);				//Read from a specific address and return unsigned 16-bit int.  Specific to address in datasheet
	bool writeAddress(int address, uint16_t data);	//Write to a specific address.  Returns true on success
	bool setBaseAddress(uint16_t base);				//Set the base block for reading/writing.  base must be 0,1,2,3
	byte getError();								//Returns an error code.  0=success, 1=response timeout, 2=invalid checksum
	
	//These are all my helper functions.
	private:
	bool calculateParityBit();
	void sendCommand(int length);
	void sendSync();
	void writeLogicalZero();
	void writeLogicalOne();
	byte readResponse();
	byte calculateCRC(uint32_t data, byte length);
	byte readBit();
	bool waitUntil(bool high, uint16_t timeout);
	bool waitForACK();
	
	int outputPin;		//The arduino pin used for communication.  Must be A0-A5 (A6 and A7 are input only)
	byte buffer[30];	//Buffer for output command and response
	bool outputState;	//Keep track of the last known state of outputPin (high/low)
	byte errorCode;		//0 if no error. 1 = response timeout.  2 = invalid checksum (CRC)
	uint16_t response;	//The value of our response.  Unfortunately functions can only return one variable, otherwise we wouldn't need this
};

#endif