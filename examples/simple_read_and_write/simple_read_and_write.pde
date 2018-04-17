#include <HAL37xy.h>
HAL37xy sensor(A0); //Use any analog pin A0-A5 (A6 and A7 are input only)

/* This program will read the Customer ID value stored on the chip,
 * increase it by 1, then write it back to the chip
 * 
 * This program is split into 4 parts:
 *  Step 1: Set Base Address
 *  Step 2: Read current Customer ID
 *  Step 3: Write new Customer ID
 *  Step 4: Read back new customer ID
 *  
 *  Each step includes error checking.  While this is not required, it's highly recommended
 */


void setup() {
  Serial.begin(9600);

  //-----------------------------------
  // STEP 1: Set Base Address
  //-----------------------------------

  //Set the bank to 0x0.  See datasheet for list of variables in each bank
  sensor.setBaseAddress(0);

  //-----------------------------------
  // Step 2: Read current Customer ID
  //-----------------------------------
  
  //Read the value in the customer ID register (address 0)
  uint16_t custID = sensor.readAddress(0);

  //If there was an error, halt the program
  if (sensor.getError() > 0) {
    Serial.println("Read Error");
    return; //Exit the program
  }

  //Print the value we read
  Serial.print("Original Customer ID: ");
  Serial.println(custID);

  //-----------------------------------
  // Step 3: Write new Customer ID
  //-----------------------------------
  
  //Change the custID to something different
  custID = custID + 1;

  //Write the address back
  bool success = sensor.writeAddress(0,custID);

  //If there was an error writing, halt the program
  if (!success) {
    Serial.println("Write Error");
    return; //Exit the program
  }

  //-----------------------------------
  // Step 4: Read new Customer ID
  //-----------------------------------
  
  //Read the address again to see if our new value stuck
  custID = sensor.readAddress(0);

  //If there was an error, halt the program
  if (sensor.getError() > 0) {
    Serial.println("Read Error");
    return; //Exit the program
  }

  //Print the value we read
  Serial.print("New Customer ID: ");
  Serial.println(custID);
}

void loop() {
}