#define F_CPU 168000000
#include <WioLTEforArduino.h>
#include <MicroNMEA.h>
#include <Wire.h>
#include <SSCI_BME280.h>
#include <stdio.h>
#include "MutichannelGasSensor.h"

char buffer[85];
MicroNMEA nmea(buffer, sizeof(buffer));
WioLTE Wio;
SSCI_BME280 sens;
unsigned long ms_high=0;
unsigned long ms_base=0;

void setup() {
  Wio.Init();
  Wire.begin();
  Serial.begin(9600);
  SerialUSB.begin(115200);
  Wio.PowerSupplyGrove(true);
  Wio.PowerSupplyLTE(true);
  gas.begin(0x04); // gas power on grove
  gas.powerOn();
  if (!Wio.TurnOnOrReset()) {
    SerialUSB.println("### TurnOnOrReset ERROR! ###");
    return;
  }
  delay(1000);
  sens.setMode(0x76, 1, 1, 1, 3, 5, 0, 0);
  sens.readTrim();
  if(2 != gas.getVersion()){
    SerialUSB.println("gas protocol version error");
  }
  if (!Wio.Activate("soracom.io", "sora", "sora")) {
    SerialUSB.println("### Activate ERROR! ###");
  }
  SerialUSB.println("### Setup completed.");
}

void collect(time_t now, char *buf){
  unsigned long ms_prev = ms_base;
  ms_base = millis();
  if(ms_base < ms_prev){
    ms_high++;
  }
  double t,p,h;
  sens.readData(&t,&p,&h);
  long lat = nmea.getLatitude();
  long lon = nmea.getLongitude();
  if(nmea.isValid()){
    long alt;
    if(nmea.getAltitude(alt)){
      sprintf(buf, "{\"timestamp\":%d, \"co\":%.5f, \"no2\":%.5f, \"nh3\":%.5f, "
      "\"c4h10\":%.5f, \"ch4\":%.5f, \"h2\":%.5f, \"c2h5oh\":%.5f, "
      "\"tempC\":%.5f, \"humidity\":%.5f, \"pressure\":%.5f, "
      "\"sattelites\":%d, \"lat\":%.5f, \"lon\":%.5f, \"altitude\":%.5f, "
      "\"rssi\":%d, \"uptime_ms\":%ld, \"uptime_high\":%ld}",
      now, gas.measure_CO(), gas.measure_NO2(), gas.measure_NH3(),
      gas.measure_C4H10(), gas.measure_CH4(), gas.measure_H2(), gas.measure_C2H5OH(),
      t,h,p,
      nmea.getNumSatellites(), lat/1000000.0, lon/1000000.0, alt/1000.0,
      Wio.GetReceivedSignalStrength(), ms_base, ms_high);
    }else{
      sprintf(buf, "{\"timestamp\":%d, \"co\":%.5f, \"no2\":%.5f, \"nh3\":%.5f, "
      "\"c4h10\":%.5f, \"ch4\":%.5f, \"h2\":%.5f, \"c2h5oh\":%.5f, "
      "\"tempC\":%.5f, \"humidity\":%.5f, \"pressure\":%.5f, "
      "\"sattelites\":%d, \"lat\":%.5f, \"lon\":%.5f, "
      "\"rssi\":%d, \"uptime_ms\":%ld, \"uptime_high\":%ld}",
      now, gas.measure_CO(), gas.measure_NO2(), gas.measure_NH3(),
      gas.measure_C4H10(), gas.measure_CH4(), gas.measure_H2(), gas.measure_C2H5OH(),
      t,h,p,
      nmea.getNumSatellites(), lat/1000000.0, lon/1000000.0,
      Wio.GetReceivedSignalStrength(), ms_base, ms_high);
    }
  }else{
    sprintf(buf, "{\"timestamp\":%d, \"co\":%.5f, \"no2\":%.5f, \"nh3\":%.5f, "
    "\"c4h10\":%.5f, \"ch4\":%.5f, \"h2\":%.5f, \"c2h5oh\":%.5f, "
    "\"tempC\":%.5f, \"humidity\":%.5f, \"pressure\":%.5f, "
    "\"rssi\":%d, \"uptime_ms\":%ld, \"uptime_high\":%ld}",
    now, gas.measure_CO(), gas.measure_NO2(), gas.measure_NH3(),
    gas.measure_C4H10(), gas.measure_CH4(), gas.measure_H2(), gas.measure_C2H5OH(),
    t,h,p,
    Wio.GetReceivedSignalStrength(), ms_base, ms_high);
  }
}

time_t prev_tm = 0;

void loop() {
  while(Serial.available()){
    if(nmea.process(Serial.read())){
      if(nmea.isValid()){
        Wio.LedSetRGB(0, 128, 0);
      }else{
        Wio.LedSetRGB(64, 0, 0);
      }
    }else{
      Wio.LedSetRGB(0, 0, 0);
    }
  }
  struct tm now;
  if (!Wio.GetTime(&now)) {
    SerialUSB.println("### ERROR! ###");
    return;
  }
  time_t tm = mktime(&now);
  bool want = false;
  if(tm > prev_tm + 240){
    char data[512];
    Wio.LedSetRGB(128, 128, 0);
    collect(tm, data);

    int rc;
    if(Wio.HttpPost("http://harvest.soracom.io", data, &rc)){
      SerialUSB.println(data);
    }else{
      SerialUSB.print("post error");
      SerialUSB.println(rc);
    }
    Wio.LedSetRGB(0, 0, 0);

    prev_tm = tm;
  }
}

