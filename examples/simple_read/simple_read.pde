#include <HAL37xy.h>

HAL37xy sensor(A0); //Use any analog pin A0-A5 (A6 and A7 are input only)

void setup() {
  Serial.begin(9600);
}

void loop() {
  //Read whatever data is at HAL37xy's address 22
  uint16_t data = sensor.readAddress(22);

  //Check if there was an error
  byte errorCode = sensor.getError();

  //Print our response/error
  if (errorCode == 0) {
    Serial.print("Unsigned: "); Serial.println(data);
    Serial.print("Signed:   "); Serial.println((int16_t)data);
  } else if (errorCode == 1){
    Serial.println("Response timed out");
  } else if (errorCode == 2) {
    Serial.println("Invalid response checksum (CRC)");
  }

  delay(1000);
}