#include <SoftwareSerial.h>
#include <dht.h>
#include <tinysnore.h>

SoftwareSerial HC12 (A1, 0);
dht DHT;

void setup () {
  pinMode(A1, OUTPUT);
  pinMode(PB1, INPUT);
  HC12.begin(9600);
  delay(10000);
}

void loop(){
  DHT.read22(PB1);
  float h = DHT.humidity;
  float t = DHT.temperature;
  HC12.print(t);
  HC12.print("@");
  HC12.println(h);
  snore(60000);
}
