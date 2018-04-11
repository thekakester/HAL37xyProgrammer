#define PROGSPEED 1000                  /*Configurable.  See data sheet.  Either 1000uS or 250uS*/
#define HALFPROGSPEED (PROGSPEED/2)     /*DONT CHANGE THIS*/
#define THRESHOLD (HALFPROGSPEED*1.2)   /*Don't change.  This is how long we wait to determine if we're reading a 0 or 1.  Should be slightly higher than HALFPROGSPEED to allow the bit to change*/

#include "HAL37xy.h"

HAL37xy::HAL37xy(int outputPin) {
	HAL37xy::outputPin = outputPin;		//The pin for programming (same as output pin, woah!)
}

uint16_t HAL37xy::readAddress(int address) {
	//First 3 bytes of read command are 001
	buffer[0] = 0;
	buffer[1] = 0;
	buffer[2] = 1;
	
	//Translate the address to a 5-bit number
	//and store it in the next 5 bits of the command
	for (int i = 0; i < 5; i++) {
		buffer[7-i] = (address >> i) & 0x1;
	}
	
	//Calculate the parity bit and add that at the end of our buffer
	buffer[8] = calculateParityBit();
	
	//Actually send the command that's in our buffer
	//9 bits for a read command: 3-read, 5-address, 1-parity
	sendCommand(9);
	
	//Now read the response we get back, and store any errorcode we get
	errorCode = readResponse();
	
	return response;
}

bool HAL37xy::calculateParityBit() {
	//Calculate parity. Count the number of 1's in our command.
	//If there's an even amount of 1's, parity bit is 1.
	//Parity bit is 0 for odd number of 1's
	byte ones = 0;
	for (int i = 0; i < 8; i++) {
		if (buffer[i] == 1) { ones++; }
	}
	
	//Return 1 if "ones" is even.  Return 0 otherwise
	return 1-(ones%2);
}

void HAL37xy::sendCommand(int length) {	
	pinMode(outputPin,OUTPUT);	//We communicate over the HAL37xy's output pin.
	
	//Send sync command to initialize command
	sendSync();
	
	for (int i = 0; i < length; i++) {
		if (buffer[i] == 1) { writeLogicalOne(); }
		else {writeLogicalZero(); }
	}
	
	//Set the pin high when we're done
	digitalWrite(outputPin,HIGH); delayMicroseconds(PROGSPEED);
}

void HAL37xy::sendSync() {
	//As described in the datasheet, sync is:
	// high (t_overc = 2000us)
	// low (t_overc = 2000us)
	// high (t_switch = 4000us)
	// low (sync bit: logical 0, can be 1000 or 250 depending on config)
	digitalWrite(outputPin,HIGH); delayMicroseconds(2000);
	digitalWrite(outputPin,LOW);  delayMicroseconds(2000);
	digitalWrite(outputPin,HIGH); delayMicroseconds(4000);
	digitalWrite(outputPin,LOW);  delayMicroseconds(PROGSPEED);
	
	//Remember that we left off at a low state
	outputState = 0;
}

void HAL37xy::writeLogicalOne() {
	//Logical one: Invert signal for HALFPROGSPEED microseconds, then invert again and hold for another HALFPROGSPEED
	digitalWrite(outputPin,!outputState); delayMicroseconds(HALFPROGSPEED);
	digitalWrite(outputPin, outputState); delayMicroseconds(HALFPROGSPEED);
}

void HAL37xy::writeLogicalZero() {
	//Logical zero: Invert signal for PROGSPEED microseconds
	digitalWrite(outputPin,!outputState); delayMicroseconds(PROGSPEED);
	outputState = !outputState;	//We changed the status of our outputPin!  Remember where we left it!
}

byte HAL37xy::getError() { return errorCode; }

byte HAL37xy::readResponse() {
	pinMode(outputPin,INPUT);	//Its time to read what HAL37xy has to say!  Set our pin to input mode
	
	//HAL37xy always starts by sending a dummy bit (always 0)
	byte dummyBit = readBit();	
	
	//Read the response (16 data bits + 4 CRC bits)
	for (int i = 0; i < 20; i++) {
		buffer[i] = readBit();
	}
	
	//Convert our buffer (bytes 0-15) to a 16-bit integer
	response = 0;
	for (int i = 0; i < 16; i++) {
		//Quick check to make sure we didn't get a timeout error (ErrorCode: 1)
		if (buffer[15-i] == 2) { return 1; }
		
		//Add this bit to our response variable
		response |= buffer[15-i] << i;
	}
	
	//Convert our buffer (bytes 16-19) to a 4-bit CRC
	byte responseCRC = 0;
	for (int i = 0; i < 4; i++) {
		//Quick check to make sure we didn't get a timeout error (ErrorCode: 1)
		if (buffer[19-i] == 2) { return 1; }
		//Add this bit to our response variable
		responseCRC |= buffer[19-i] << i;
	}

	//Calculate the CRC for the response and make sure it matches what we received
	//If not, return Invalid Checksum error (ErrorCode: 2)
	if (calculateCRC() != responseCRC) { return 2; }
	
	//Wow!  It looks like everything worked out swell!  Return success!
	return 0;	//0=no error
}

byte HAL37xy::readBit() {
	//Wait for the output pin to toggle, signifying the start of this bit
	//If we timeout, return 2
	if (waitUntil(!outputState) == false) { return 2; }
	
	//The output pin changed.  Remember what it is now
	outputState = !outputState;
	
	//Each bit takes 1000 microseconds (or 250 if configured).
	//Logical 1: Inverts signal after 500uS
	//Logical 0: Leaves signal the same after 500uS
	//We'll wait for the signal to (potentially change), then read the value.
	//THRESHOLD is just slightly longer than 500uS to give the signal time to change
	delayMicroseconds(THRESHOLD);
	
	//Read the signal now.
	byte newState = digitalRead(outputPin);
	
	//Compare the current signal to how it was before we waited.
	//If it changed, we just read a logical 1.  If it's the same, we read logical 0
	byte logicalData = newState != outputState;
	
	//Before we return our findings, remember the current state of outputPin
	outputState = newState;
	
	return logicalData;
}

bool HAL37xy::waitUntil(bool high) {
	//Wait until the signal changes to high/low, but have a timeout of 10ms
	uint16_t timer = millis();
	while (digitalRead(outputPin)!=high) {
		//DO NOTHING, just wait
		if (millis()-timer > 10) { return false; }	//Oh no!  We timed out!
	}
	return true;
}

byte HAL37xy::calculateCRC() {
	//According to the datasheet, the CRC polynomial is (x^4 + x + 1)
	//This translates to 1x^4 + 0x^3 + 0x^2 + 1x + 1  =  10011
	//See wikipedia for how to calculate CRC
	//https://en.wikipedia.org/wiki/Cyclic_redundancy_check#Polynomial_representations_of_cyclic_redundancy_checks
	byte polynomial[5] = {1,0,0,1,1};
	
	//WARNING: This modifies buffer!  I hope you already extracted all the important info you needed
	//Leave bytes 0-15 as they are
	//HAL37xy uses a 4-bit CRC, so clear the next 4 bits
	for (int i = 16; i < 20; i++) { buffer[i] = 0; }
	
	//Here's the magic.  If you don't understand. Check the above wikipedia link
	for (int i = 0; i < 16; i++) {
		//Skip this step if the bit is 0
		if (buffer[i] == 0) { continue; }
		
		//XOR our polynomial with the next 5 bits of our buffer
		for (int j = 0; j < 5; j++) {
			buffer[i+j] ^= polynomial[j];
		}
	}
	
	//Not too bad.  We did it!  Now just convert those last 4 CRC bits (16-19) to a number
	byte crc = 0;
	for (int i = 0; i < 4; i++) {
		crc |= buffer[19-i] << i;
	}
	
	return crc;
}