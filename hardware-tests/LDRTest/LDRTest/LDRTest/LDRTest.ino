int LDR_Pin0 = A0; //analog pin 0
int LDR_Pin1 = A1; //analog pin 1
int LDR_Pin2 = A2; //analog pin 2
int LDR_Pin3 = A3; //analog pin 3

int LDRreadings[3];
int total;

void setup(){
    Serial.begin(9600);
}

void loop(){
    LDRreadings[0] = analogRead(LDR_Pin0);
    //LDRreadings[1] = analogRead(LDR_Pin1);
    //LDRreadings[2] = analogRead(LDR_Pin2);
    //LDRreadings[3] = analogRead(LDR_Pin3);
    for (int i; i<4; i++) {
//        Serial.print("LDR "); Serial.print(i); Serial.print(" : ");
//        Serial.println(LDRreadings[i]);
    }
    total = LDRreadings[0] + LDRreadings[1] + LDRreadings[2] + LDRreadings[3];
//    Serial.println(total);
    Serial.println(LDRreadings[0]);
    if (LDRreadings[0]>900){
        Serial.println("on LDR");
        playcomplete(jazzsounds[0]);
    } else if (LDRreadings[0]>600){
        Serial.println("LDR strong");
        playcomplete(jazzsounds[1]);
    } else if (LDRreadings[0]>350 && LDRreadings[0]<600){
        Serial.println("LDR weak");
        playcomplete(jazzsounds[2]);
    } else if (LDRreadings[0]>110 && LDRreadings[0]<350){
        Serial.println("off LDR");
        playcomplete(jazzsounds[3]);
    }else{
        Serial.println("light off");
        playcomplete(jazzsounds[4]);
    }
    delay(100); //just here to slow down the output for easier reading
}
