void setup() {
  // put your setup code here, to run once:
  pinMode(40, INPUT);
  Serial.begin(57600);
}

void loop() {
  // put your main code here, to run repeatedly:
  
  Serial.println(digitalRead(40));
}
