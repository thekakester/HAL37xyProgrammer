#include <HAL37xy.h>

char* addresses[] = {
  "Customer ID 1",
  "Customer ID 2",
  "GAIN_CH2",
  "GAIN_CH1",
  "CH1/CH2_GAIN",
  "CUST_OFFSET1",
  "CUST_OFFSET2",
  "CUST_OFFSET3",
  "CUST_OFFSETCH1",
  "CUST_OFFSETCH2",
  "OUT_ZERO",
  "PRE_OFFSET",
  "OUT_GAIN",
  "OUT_OFFSET",
  "CLAMP_HIGH",
  "CLAMP_LOW",
  "MAG-HIGH",
  "MAG-LOW",
  "HYSTERESIS_CH1",
  "HYSTERESIS_CH2",
  "LP_FILTER",
  "MODULO",
  "Micronas ID 1",
  "Micronas ID 2"
};
  

HAL37xy sensor(A0); //Use any analog pin A0-A5 (A6 and A7 are input only)

void setup() {
  Serial.begin(9600);

  //Loop over every address (hex 0x0-0x17, which is 0-23 in decimal)
  for (int i = 0; i < 0x17; i++) {
    //Read the data at the given address
    uint16_t data = sensor.readAddress(i);

    Serial.print("0x"); Serial.print(i,HEX); Serial.print(" ");
    Serial.print(addresses[i]); Serial.print(": ");
    
    if (sensor.getError() == 0) {
      Serial.println(data);
    } else {
      Serial.print("ERROR CODE ");
      Serial.println(sensor.getError());
    }
  }
}

void loop() {
}