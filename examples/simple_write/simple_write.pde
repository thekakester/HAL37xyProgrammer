#include <HAL37xy.h>
HAL37xy sensor(A0); //Use any analog pin A0-A5 (A6 and A7 are input only)

void setup() {
  Serial.begin(9600);

  //Write "1234" to address 0 (Address 0: Customer ID)
  if (sensor.writeAddress(0,1234) == true) {
    Serial.println("Write Success");
  } else {
    Serial.println("Write Failed");
  }
}

void loop() {
}