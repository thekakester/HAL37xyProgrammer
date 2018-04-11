#define PROGSPEED 1000                  /*Configurable.  See data sheet.  Either 1000uS or 250uS*/
#define HALFPROGSPEED (PROGSPEED/2)     /*DONT CHANGE THIS*/
#define THRESHOLD (HALFPROGSPEED*1.2)   /*Don't change.  This is how long we wait to determine if we're reading a 0 or 1.  Should be slightly higher than HALFPROGSPEED to allow the bit to change*/

bool command[10] = {0,0,0,0,0,0,0,0,0,0};
int16_t response[60];
int commandLength = 0;
int commandIndex = 0;  //Dont change
bool outputState = 0;

void setup() {
  // put your setup code here, to run once:
  
  Serial.begin(115200);
  Serial.println(THRESHOLD);

  for (int i = 0; i < 32; i++) {
    for (int j = 0; j < 1; j++) {
    Serial.print("(Read Register ");
    Serial.print(i);
    Serial.print(") ");
    readCommand(i);
    delay(50);
    }

  }
}

void readCommand(int address) {
  commandLength = 9;
  command[0] = 0;
  command[1] = 0;
  command[2] = 1;

  //Calculate the binary value to 5 bits
  for (int i = 0; i < 5; i++) {
    command[7-i] = (address >> i) & 0x1;
  }

  parity();
}

void parity() {
  commandIndex = 0;  //Reset command index.
  
  //Calculate parity. Global parity should always be even, meaning that INCLUDING the parity bit, there's always an even number of 1s
  int ones = 0;
  for (int i = 0; i < 8; i++) {
    if (command[i] == 1) { ones++; }
  }
  command[8] = 1-(ones%2);  //Parity bit is 1 if odd (ones%2==1), 0 otherwise

  for (int i = 0; i < 9; i++) {
    Serial.print(command[i]);
  }
  Serial.print(": ");

  sendCommand();
  readResponse();
}

void sendCommand() {
  pinMode(A0,OUTPUT);

  sync();
  
  while (true) {
    if (command[commandIndex] == 1) {
      writeOne();
    } else {
      writeZero();
    }
    
    commandIndex++;

    if (commandIndex > commandLength) { break; }
  }

  //Set high when we're done
  digitalWrite(A0,HIGH); delayMicroseconds(HALFPROGSPEED);
}

void sync() {  
  outputState = false;

  //Sync is:
  // high (2000us)
  // low (2000us)
  // high (4000us)
  // low (1000 or 250, configurable) (sync bit, logical 0)
  digitalWrite(A0,HIGH); delayMicroseconds(2000);
  digitalWrite(A0,LOW); delayMicroseconds(2000);
  digitalWrite(A0,HIGH); delayMicroseconds(4000);
  digitalWrite(A0,LOW); delayMicroseconds(PROGSPEED);
}

void writeZero() {
  outputState = !outputState;
  digitalWrite(A0,outputState);
  delayMicroseconds(PROGSPEED);
}

void writeOne() {
  digitalWrite(A0,!outputState);
  delayMicroseconds(HALFPROGSPEED);
  digitalWrite(A0,outputState);
  delayMicroseconds(HALFPROGSPEED);
}

void readResponse() {
  uint16_t timer;
  pinMode(A0,INPUT);
  
  int dummyBit = readBit(); //According to docs, this should always be 0

  timer = micros();
  //Read the response
  for (int i = 0; i < 20; i++) {
    response[i] = readBit();
  }

  /*
  bool waitForState = !outputState;
  for (int i = 0; i < 60; i++) {
    bool pass = waitUntil(waitForState);
    response[i] = micros() - timer;
    if (!pass) { response[i] *= -1;  }
    else { waitForState = !waitForState; }
    
    timer = micros();
  }*/

  timer = micros() - timer;
  Serial.print("Response: (");
  Serial.print(timer);
  Serial.print("us) ");

  for (int i = 0; i < 20; i++) {
    if (response[i] == 2) { Serial.print("{FAILED} "); break; }
  }

  Serial.print(dummyBit); Serial.print("|");
  
  int16_t result = 0;
  //Convert the result to 2 numbers
  for (int i = 0; i < 16; i++) {
    result |= (response[15-i] << i);
    Serial.print(response[i]);
  }
  Serial.print(" (");
  Serial.print(result);
  Serial.print(") ");

  //Convert the second number
  result = 0;
  for (int i = 0; i < 4; i++) {
    result |= (response[19-i] << i);
    Serial.print(response[16+i]);
  }
  Serial.print(" CRC:");
  Serial.print(result);

  //Perform CRC check on what we got to see if it matches
  int crc = calculateCRC();
  if (crc == result) { Serial.println(" valid!"); }
  else { Serial.println(" INVALID"); }
}

int calculateCRC() {
  //According to their documentation, their CRC is x^4+x+1, which translates to 1x^4 + 0x^3 + 0x^2 + 1x + 1 = 10011
  //See wikipedia's example: https://en.wikipedia.org/wiki/Cyclic_redundancy_check#Polynomial_representations_of_cyclic_redundancy_checks
  byte polynomial[5] = {1,0,0,1,1};
  byte crcCheck[20];  //(1 dummy byte IGNORED because 0), 16 bytes of data, plus 4 for the CRC
  for (int i = 0; i < 16; i++) { crcCheck[i] = response[i]; } //Copy response...
  for (int i = 16; i < 20; i++) { crcCheck[i] = 0; }          //...followed by 0s for the CRC to be calculated
  
  for (int i = 0; i < 20-4; i++) {
    //Skip if the number here is 0
    if (crcCheck[i] == 0) { continue; }
    for (int j = 0; j < 5; j++) {
      crcCheck[i+j] ^= polynomial[j];
    }

    /*
    //z loop is only for debugging to check each step of the way
    for (int z = 0; z < 20; z++) {
      Serial.print(crcCheck[z]);
    }
    Serial.println();
    */
  }

  //Turn the last 4 bytes into a number and return it
  int crc = 0;
  for (int i = 0; i < 4; i++) {
    crc |= (crcCheck[19-i] << i);
  }
  return crc;
}

byte readBit() {
  //Wait for it to toggle.  If we timeout, return 2
  if (waitUntil(!outputState)==false) {return 2;}

  //The output pin toggled!  Remember this
  outputState = !outputState;

  //Each bit takes 1000us.  A logical 1 will invert the signal after 500us.  A logical 0 will stay the same
  //Wait until the center of the signal (750us)
  delayMicroseconds(THRESHOLD);
  
  //read the signal.  If it changed, it's a 1.  If it's the same, it's a 0
  byte newState = analogRead(A0) > 800;
  byte logicalData = newState != outputState; //If it changed, it's a logical 1.  Else 0

  outputState = newState; //Remember the last thing we read

  return logicalData;

  /*//Wait for 30 times our weirdness
  uint16_t timer = millis();
  while (millis() - timer < 2) {
    Serial.println(analogRead(A0));
  }

  return 0;*/
}

bool waitUntil(bool high) {
  uint16_t timeout = millis();
  uint16_t timePassed;
  if (high) {
    while (analogRead(A0) < 800) {if (millis() - timeout > 10) { return false; }} //Wait until signal goes high
  } else {
    while (analogRead(A0) > 300) {if (millis() - timeout > 10) { return false; }} //Wait until signal goes low
  }
  return true;
}

void loop() {
  //Serial.println(analogRead(A0));
  delay(1000);
}

