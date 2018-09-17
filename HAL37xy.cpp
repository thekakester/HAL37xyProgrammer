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
	outputState = 1;
	digitalWrite(outputPin,HIGH); delayMicroseconds(HALFPROGSPEED/2);
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
	
	//Give it a wee bit of time a sec to pull up the voltage before we start reading
	delayMicroseconds(HALFPROGSPEED/2);
	
	if (waitUntil(HIGH,10) == false) { return 1; }	//Return timeout
	outputState = 1;
	
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
		if (buffer[15-i] == 2) { return 3; }
		
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
	if (calculateCRC(response,16) != responseCRC) { return 2; }
	
	//Wow!  It looks like everything worked out swell!  Return success!
	return 0;	//0=no error
}

byte HAL37xy::readBit() {
	//Wait for the output pin to toggle, signifying the start of this bit
	//If we timeout, return 2
	if (waitUntil(!outputState,10) == false) { return 2; }
	
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

bool HAL37xy::waitUntil(bool high, uint16_t timeoutMS) {
	//Wait until the signal changes to high/low, but have a timeout of 10ms
	uint16_t timer = millis();
	uint16_t timePassed;
	while (digitalRead(outputPin)!=high) {
		//DO NOTHING, just wait
		timePassed = millis();	//This code had to be split up becasue the if() statement converts things from long to ints, causing overflow and timeouts
		timePassed -= timer;
		if (timePassed > timeoutMS) { return false; }	//Oh no!  We timed out!
	}
	return true;
}

byte HAL37xy::calculateCRC(uint32_t data, byte length) {
	//According to the datasheet, the CRC polynomial is (x^4 + x + 1)
	//See wikipedia for how to calculate CRC
	//https://en.wikipedia.org/wiki/Cyclic_redundancy_check#Polynomial_representations_of_cyclic_redundancy_checks
	//I will be following the crc calculation exactly as they have it in the programming guide
	//Unfortunately, I have not taken the time to learn exactly what is happening here
	
	byte bit_in,bit_out,bit_comp,crc;
	crc = 0;
	for (int i = length; i > -1; i--) {
		bit_in = (data >> i) & 0x1;	/*Extract input bit*/
		bit_out = (crc >> 3) & 0x1;		/*Extract bit b3 of crc*/
		bit_comp = (bit_out ^ bit_in) & 0x1;
		crc = (crc << 1) + bit_comp;	/*Calculate interim value of crc*/
		crc = crc ^ (bit_comp << 1);
	}
	
	crc &= 0xf;
	return crc;
}

bool HAL37xy::writeAddress(int address,uint16_t data) {
	//First 3 bytes of write command are 110 (6 in decimal)
	buffer[0] = 1;
	buffer[1] = 1;
	buffer[2] = 0;
	
	//Translate the address to a 5-bit number
	//and store it in the next 5 bits of the command
	for (int i = 0; i < 5; i++) {
		buffer[7-i] = (address >> i) & 0x1;
	}
	
	//Calculate the parity bit and add that at the end of our buffer
	buffer[8] = calculateParityBit();
	
	//Next bit is a dummy bit (always 0)
	buffer[9] = 0;
	
	//Next 16 bits (10-25) are the data we want to write
	for (int i = 0; i < 16; i++) {
		buffer[25 - i] = (data >> i) & 0x1;
	}
	
	//Calculate the CRC for our data we're writing
	//The CRC is made up of the COMMAND, ADDRESS, PARITY, Dummy, and DATA
	//First, combine these to make one number
	uint32_t transmission = 0;
	for (int i = 0; i <=25; i++) {
		transmission |= (uint32_t)(buffer[25-i]) << i;
	}

	//We send this "25" bit string to our CRC algorithm
	//Technically COMMAND+ADDRESS+PARITY+DUMMY+DATA is 26-bit, but bit 0 is always 0, so it's ignored
	byte crc = calculateCRC(transmission,25);
	for (int i = 0; i < 4; i++) {
		buffer[29-i] = (crc >> i) & 0x1;
	}
	
	//Actually send the command that's in our buffer
	//30 bits for a read command: 3-write, 5-address, 1-parity, 1-dummy, 16-data, 4-crc
	sendCommand(30);
	
	return waitForACK();
}

bool HAL37xy::setBaseAddress(uint16_t base) {
	//First 3 bytes of SetBaseAddress command are 011 (3 in decimal)
	buffer[0] = 0;
	buffer[1] = 1;
	buffer[2] = 1;
	
	//Address Bits (bits 3-7) dont matter.
	//To be safe set it all to 0
	for (int i = 3; i <= 8; i++) {
		buffer[i] = 0;
	}
	
	//Parity bit (bit 8)
	buffer[8] = calculateParityBit();
	
	//Next bit is a dummy bit (always 0)
	buffer[9] = 0;
	
	//Next 16 bits (10-25) are the Base address to use
	//According to the data sheet, this is 0,1,2,3.  We must not be able to access 0x4 and 0x5
	for (int i = 0; i < 16; i++) {
		buffer[25 - i] = (base >> i) & 0x1;
	}
	
	//Calculate the CRC for our data we're writing
	//The CRC is made up of the COMMAND, ADDRESS, PARITY, Dummy, and DATA
	//First, combine these to make one number
	uint32_t transmission = 0;
	for (int i = 0; i <=25; i++) {
		transmission |= (uint32_t)(buffer[25-i]) << i;
	}

	//We send this "25" bit string to our CRC algorithm
	//Technically COMMAND+ADDRESS+PARITY+DUMMY+DATA is 26-bit, but bit 0 is always 0, so it's ignored
	byte crc = calculateCRC(transmission,25);
	for (int i = 0; i < 4; i++) {
		buffer[29-i] = (crc >> i) & 0x1;
	}
	
	//Actually send the command that's in our buffer
	//30 bits for a read command: 3-write, 5-address, 1-parity, 1-dummy, 16-data, 4-crc
	sendCommand(30);
	
	return waitForACK();
}

bool HAL37xy::waitForACK() {
	/*According to the datasheet, the sensor will pull the signal:
	* LEVEL | Duration
	* ------+----------
	*  HIGH | tSpace (PROGSPEED, 1000 or 250uS depending on config)
	*  LOW  | tAck (PROGSPEED)
	*  HIGH | tAck (PROGSPEED)
	*  HIGH | 2*tPROG (2 * 1.75ms) -> (note, TPROG is between 1.57 and 1.93ms)
	*  LOW  | tAck (PROGSPEED)
	*  HIGH | tAPPL (PROGSPEED)
	*/
	
	pinMode(outputPin,INPUT);
	
	//NOTE: We will wait up to 2x the timeouts specified in the datasheet.
	
	//Wait for the pin to go high to start
	if (waitUntil(HIGH,10) == false) { return false; }

	//Wait for the pin to go low (after tAck time units)
	if (waitUntil(LOW,(PROGSPEED/1000)*2) == false) { return false; }

	//Wait for the pin to go high (after tAck time units)
	if (waitUntil(HIGH,(PROGSPEED/1000)*2) == false) { return false; }
	
	//Wait for the pin to go low (after tAck + (2*tPROG) time units)
	if (waitUntil(LOW, 2*((PROGSPEED/1000) + 4)) == false) { return false;}
	
	//Finally goes high again (after tAck time units)
	//Note, it's normal for this one to potentially time out.  Don't return false
	waitUntil(HIGH, (PROGSPEED/1000) * 2);
	
	//We did it successfully!
	return true;
}
