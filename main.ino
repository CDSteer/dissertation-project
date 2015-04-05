#include <FatReader.h>
#include <SdReader.h>
#include <avr/pgmspace.h>
#include "WaveUtil.h"
#include "WaveHC.h"
#include <TimedAction.h>

int16_t lastpotval = 0;
#define HYSTERESIS 3

SdReader card;    // This object holds the information for the card
FatVolume vol;    // This holds the information for the partition on the card
FatReader root;   // This holds the information for the filesystem on the card
FatReader f;      // This holds the information for the file we're play
WaveHC wave;    // This is the only wave (audio) object, since we will only play one at a time

#define trigPin 49
#define echoPin 48

//RGB1
#define L1R 22
#define L1G 23
#define L1B 24
//RGB2
#define L2R 28
#define L2G 29
#define L2B 30
//RGB3
#define L3R 32
#define L3G 33
#define L3B 34
//RGB4
#define L4R 38
#define L4G 39
#define L4B 40

#define ARRAYSIZE 9
char *jazzsounds[ARRAYSIZE] = {
    "DRUNK.WAV",
    "DUBBI.WAV",
    "GUITAR~1.WAV",
    "JAZZOIDZ.WAV",
    "MANDOLIN.WAV",
    "PHASEDL.WAV",
    "PIANO.WAV",
    "PIANOR~1.WAV"
};
#define LDR1 10
#define LDR2 11
#define LDR3 12
#define LDR4 13
int LDRreadings[4];
int onlight[4];
int tube = 0;

#define X_ACCEL_APIN 8
#define Y_ACCEL_APIN 9
#define Z_ACCEL_APIN 2
#define V_REF_APIN   3
#define Y_RATE_APIN  4
#define X_RATE_APIN  5
int xAccel=0, yAccel=0, vRef=0, xRate=0, yRate=0;
unsigned int startTag = 0xDEAD;  // Analog port maxes at 1023 so this is a safe termination value

#define DEBOUNCE 5  // button debouncer

// here is where we define the buttons that we'll use. button "1" is the first, button "6" is the 6th, etc
byte buttons[] = {14, 15, 16, 17, 18, 19};
// This handy macro lets us determine how big the array up above is, by checking the size
#define NUMBUTTONS sizeof(buttons)
// we will track if a button is just pressed, just released, or 'pressed' (the current state
volatile byte pressed[NUMBUTTONS], justpressed[NUMBUTTONS], justreleased[NUMBUTTONS];

long duration, distance;
int x, y, x1, x2, y1, y2, change, change1, change2;

int Shock = A15;
int vibrate;

void vibratesound(void);
void singlesoundloop(void);
void soundloop(void);
void readldr(int);
void readldrs(void);
void ldrcol(void);
void setonlight(int);
void setlight(void);
void setlights(void);
void colourLED(int led, char* colour);
void distanceread(void);
void playdistance(void);
void playcomplete(char *name);
void playfile(char *name);
void check_switches(void);
void accread(void);
void playspeed(char *name);
void accplay(void);
void RGBtest(void);


// this handy function will return the number of bytes currently free in RAM, great for debugging!
int freeRam(void) {
    extern int  __bss_end;
    extern int  *__brkval;
    int free_memory;
    if((int)__brkval == 0) {
        free_memory = ((int)&free_memory) - ((int)&__bss_end);
    }
    else {
        free_memory = ((int)&free_memory) - ((int)__brkval);
    }
    return free_memory;
}

void sdErrorCheck(void) {
    if (!card.errorCode()) return;
    putstring("\n\rSD I/O error: ");
    Serial.print(card.errorCode(), HEX);
    putstring(", ");
    Serial.println(card.errorData(), HEX);
    while(1);
}

void setup() {
    pinMode(L1R, OUTPUT);
    pinMode(L1G, OUTPUT);
    pinMode(L1B, OUTPUT);
    pinMode(L2R, OUTPUT);
    pinMode(L2G, OUTPUT);
    pinMode(L2B, OUTPUT);
    pinMode(L3R, OUTPUT);
    pinMode(L3G, OUTPUT);
    pinMode(L3B, OUTPUT);
    pinMode(L4R, OUTPUT);
    pinMode(L4G, OUTPUT);
    pinMode(L4B, OUTPUT);
    byte i;
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);
    pinMode(A8, INPUT);
    pinMode(A15, INPUT);
    pinMode(A10, INPUT);
    pinMode(A11, INPUT);
    //pinMode(led, OUTPUT);
    //pinMode(led2, OUTPUT);
    // set up serial port
    Serial.begin(9600);
    putstring_nl("WaveHC with ");
    Serial.print(NUMBUTTONS, DEC);
    putstring_nl("buttons");
    putstring_nl("\nWave test!");
    putstring("Free RAM: ");       // This can help with debugging, running out of RAM is bad
    Serial.println(freeRam());      // if this is under 150 bytes it may spell trouble!
    
    // Set the output pins for the DAC control. This pins are defined in the library
    pinMode(2, OUTPUT);
    pinMode(3, OUTPUT);
    pinMode(4, OUTPUT);
    pinMode(5, OUTPUT);
    pinMode(Shock, INPUT); // output interface defines vibration sensor
    
    // pin13 LED
    pinMode(13, OUTPUT);
    
    // Make input & enable pull-up resistors on switch pins
    for (i=0; i< NUMBUTTONS; i++) {
        pinMode(buttons[i], INPUT);
        digitalWrite(buttons[i], HIGH);
    }
    
    //  if (!card.init(true)) { //play with 4 MHz spi if 8MHz isn't working for you
    if (!card.init()) {         //play with 8 MHz spi (default faster!)
        putstring_nl("Card init. failed!");  // Something went wrong, lets print out why
        sdErrorCheck();
        while(1);                            // then 'halt' - do nothing!
    }
    
    // enable optimize read - some cards may timeout. Disable if you're having problems
    card.partialBlockRead(true);
    
    // Now we will look for a FAT partition!
    uint8_t part;
    for (part = 0; part < 5; part++) {     // we have up to 5 slots to look in
        if (vol.init(card, part))
            break;                             // we found one, lets bail
    }
    if (part == 5) {                       // if we ended up not finding one  :(
        putstring_nl("No valid FAT partition!");
        sdErrorCheck();      // Something went wrong, lets print out why
        while(1);                            // then 'halt' - do nothing!
    }
    
    // Lets tell the user about what we found
    putstring("Using partition ");
    Serial.print(part, DEC);
    putstring(", type is FAT");
    Serial.println(vol.fatType(),DEC);     // FAT16 or FAT32?
    
    // Try to open the root directory
    if (!root.openRoot(vol)) {
        putstring_nl("Can't open root dir!"); // Something went wrong,
        while(1);                             // then 'halt' - do nothing!
    }
    
    // Whew! We got past the tough parts.
    putstring_nl("Ready!");
    
    TCCR2A = 0;
    TCCR2B = 1<<CS22 | 1<<CS21 | 1<<CS20;
    //Timer2 Overflow Interrupt Enable
    TIMSK2 |= 1<<TOIE2;
    setonlight(0);
}

SIGNAL(TIMER2_OVF_vect) {
    check_switches();
}

void check_switches(void) {
    static byte previousstate[NUMBUTTONS];
    static byte currentstate[NUMBUTTONS];
    byte index;
    
    for (index = 0; index < NUMBUTTONS; index++) {
        currentstate[index] = digitalRead(buttons[index]);   // read the button
        if (currentstate[index] == previousstate[index]) {
            if ((pressed[index] == LOW) && (currentstate[index] == LOW)) {
                justpressed[index] = 1;
            }
            else if ((pressed[index] == HIGH) && (currentstate[index] == HIGH)) {
                justreleased[index] = 1;
            }
            pressed[index] = !currentstate[index];  // remember, digital HIGH means NOT pressed
        }
        previousstate[index] = currentstate[index];   // keep a running tally of the buttons
    }
}

void loop() {
    singlesoundloop();
    // soundloop();
    // vibratesound();
}

void vibratesound(){
    vibrate = analogRead(Shock); // read digital interface is assigned a value of 3 val
    Serial.println(vibrate);
    if (vibrate < 1){ // When the shock sensor detects a signal, LED flashes
        playcomplete(jazzsounds[1]);
    }
}

void singlesoundloop(){
    // if all are off turn one on
    readldrs();
    setlight();
    ldrcol();
    //for (int i = 0; i<=3; i++) {
        if (tube == 0) {
            if (LDRreadings[tube]>900){
                Serial.println("on LDR");
                playcomplete(jazzsounds[0]);
            } else if (LDRreadings[tube]>600){
                Serial.println("LDR strong");
                playcomplete(jazzsounds[1]);
            } else if (LDRreadings[tube]>350 && LDRreadings[tube]<600){
                Serial.println("LDR weak");
                playcomplete(jazzsounds[2]);
            } else if (LDRreadings[tube]>110 && LDRreadings[tube]<350){
                Serial.println("off LDR");
                playcomplete(jazzsounds[3]);
            }else{
                Serial.println("light off");
            }
        }
        if (tube == 1) {
            if (LDRreadings[tube]>900){
                Serial.println("on LDR");
                playcomplete(jazzsounds[4]);
            } else if (LDRreadings[tube]>600){
                Serial.println("LDR strong");
                playcomplete(jazzsounds[5]);
            } else if (LDRreadings[tube]>350 && LDRreadings[tube]<600){
                Serial.println("LDR weak");
                playcomplete(jazzsounds[6]);
            } else if (LDRreadings[tube]>110 && LDRreadings[tube]<350){
                Serial.println("off LDR");
                playcomplete(jazzsounds[7]);
            }else{
                Serial.println("light off");
            }
        }
    
        if (tube == 2) {
            if (LDRreadings[tube]>900){
                Serial.println("on LDR");
                playcomplete(jazzsounds[0]);
            } else if (LDRreadings[tube]>600){
                Serial.println("LDR strong");
                playcomplete(jazzsounds[1]);
            } else if (LDRreadings[tube]>350 && LDRreadings[tube]<600){
                Serial.println("LDR weak");
                playcomplete(jazzsounds[2]);
            } else if (LDRreadings[tube]>110 && LDRreadings[tube]<350){
                Serial.println("off LDR");
                playcomplete(jazzsounds[3]);
            }else{
                Serial.println("light off");
            }
        }
        if (tube == 3) {
            if (LDRreadings[tube]>900){
                Serial.println("on LDR");
                playcomplete(jazzsounds[4]);
            } else if (LDRreadings[tube]>600){
                Serial.println("LDR strong");
                playcomplete(jazzsounds[5]);
            } else if (LDRreadings[tube]>350 && LDRreadings[tube]<600){
                Serial.println("LDR weak");
                playcomplete(jazzsounds[6]);
            } else if (LDRreadings[tube]>110 && LDRreadings[tube]<350){
                Serial.println("off LDR");
                playcomplete(jazzsounds[7]);
            }else{
                Serial.println("light off");
            }
        }



//    }
}

void soundloop(){
    readldrs();
    setlights();
    for (int i = 0; i<=3; i++) {
        if (LDRreadings[i]>900){
            Serial.println("on LDR");
            playcomplete(jazzsounds[0]);
        } else if (LDRreadings[i]>600){
            Serial.println("LDR strong");
            playcomplete(jazzsounds[1]);
        } else if (LDRreadings[i]>350 && LDRreadings[i]<600){
            Serial.println("LDR weak");
            playcomplete(jazzsounds[2]);
        } else if (LDRreadings[i]>110 && LDRreadings[i]<350){
            Serial.println("off LDR");
            playcomplete(jazzsounds[3]);
        }else{
            Serial.println("light off");
        }
    }
}

void readldr(int lrd){
    LDRreadings[0] = analogRead(A11);
    LDRreadings[1] = analogRead(A10);
    LDRreadings[2] = analogRead(A9);
    LDRreadings[3] = analogRead(A8);
    //setlight();
}

void readldrs(){
    LDRreadings[0] = analogRead(A11);
    LDRreadings[1] = analogRead(A10);
    LDRreadings[2] = analogRead(A9);
    LDRreadings[3] = analogRead(A8);
//    Serial.print("0: ");
//    Serial.println(LDRreadings[0]);
//    Serial.print("1: ");
//    Serial.println(LDRreadings[1]);
//    Serial.print("2: ");
//    Serial.println(LDRreadings[2]);
//    Serial.print("3: ");
//    Serial.println(LDRreadings[3]);
    //setlights();
}

void ldrcol(){
    for (int i = 0; i<=3; i++) {
        if (LDRreadings[i]>900){
            setonlight(i);
        }
    }
}

void setonlight(int led){
    for (int i=0; i<4; i++) {
        if (i==led){
            tube = i;
        }
    }
    for (int i=0; i<4; i++) {
        if (i != tube){
            colourLED(i, "off");
        }
    }
    setlight();
}

void setlight(){
    if (LDRreadings[tube]>900){
        colourLED(tube, "green");
    } else if (LDRreadings[tube]>600){
        colourLED(tube, "blue");
    } else if (LDRreadings[tube]>350 && LDRreadings[tube]<600){
        colourLED(tube, "red");
    } else if (LDRreadings[tube]>110 && LDRreadings[tube]<350){
        colourLED(tube, "white");
    }else{
        colourLED(tube, "off");
    }
}

void setlights(){
    for (int i = 0; i<=3; i++) {
        if (LDRreadings[i]>900){
            colourLED(i, "green");
        } else if (LDRreadings[i]>600){
            colourLED(i, "blue");
        } else if (LDRreadings[i]>350 && LDRreadings[i]<600){
            colourLED(i, "red");
        } else if (LDRreadings[i]>110 && LDRreadings[i]<350){
            colourLED(i, "white");
        }else{
            colourLED(i, "off");
        }
    }
}

void colourLED(int led, char* colour){
    if (led == 0){
        if (colour == "red"){
            digitalWrite(L1R, LOW);
            digitalWrite(L1G, HIGH);
            digitalWrite(L1B, LOW);
        }
        if (colour == "green"){
            digitalWrite(L1R, LOW);
            digitalWrite(L1G, HIGH);
            digitalWrite(L1B, LOW);
        }
        if (colour == "blue"){
            digitalWrite(L1R, LOW);
            digitalWrite(L1G, LOW);
            digitalWrite(L1B, HIGH);
        }
        if (colour == "white"){
            digitalWrite(L1R, HIGH);
            digitalWrite(L1G, HIGH);
            digitalWrite(L1B, HIGH);
        }
        if (colour == "off"){
            digitalWrite(L1R, LOW);
            digitalWrite(L1G, LOW);
            digitalWrite(L1B, LOW);
        }
    }
    
    if (led == 1){
        if (colour == "red"){
            digitalWrite(L2R, LOW);
            digitalWrite(L2G, HIGH);
            digitalWrite(L2B, LOW);
        }
        if (colour == "green"){
            digitalWrite(L2R, LOW);
            digitalWrite(L2G, HIGH);
            digitalWrite(L2B, LOW);
        }
        if (colour == "blue"){
            digitalWrite(L2R, LOW);
            digitalWrite(L2G, LOW);
            digitalWrite(L2B, HIGH);
        }
        if (colour == "white"){
            digitalWrite(L2R, HIGH);
            digitalWrite(L2G, HIGH);
            digitalWrite(L2B, HIGH);
        }
        if (colour == "off"){
            digitalWrite(L2R, LOW);
            digitalWrite(L2G, LOW);
            digitalWrite(L2B, LOW);
        }
    }
    
    if (led == 2){
        if (colour == "red"){
            digitalWrite(L3R, LOW);
            digitalWrite(L3G, HIGH);
            digitalWrite(L3B, LOW);
        }
        if (colour == "green"){
            digitalWrite(L3R, LOW);
            digitalWrite(L3G, HIGH);
            digitalWrite(L3B, LOW);
        }
        if (colour == "blue"){
            digitalWrite(L3R, LOW);
            digitalWrite(L3G, LOW);
            digitalWrite(L3B, HIGH);
        }
        if (colour == "white"){
            digitalWrite(L3R, HIGH);
            digitalWrite(L3G, HIGH);
            digitalWrite(L3B, HIGH);
        }
        if (colour == "off"){
            digitalWrite(L3R, LOW);
            digitalWrite(L3G, LOW);
            digitalWrite(L3B, LOW);
        }
    }
    
    if (led == 3){
        if (colour == "red"){
            digitalWrite(L4R, LOW);
            digitalWrite(L4G, HIGH);
            digitalWrite(L4B, LOW);
        }
        if (colour == "green"){
            digitalWrite(L4R, LOW);
            digitalWrite(L4G, HIGH);
            digitalWrite(L4B, LOW);
        }
        if (colour == "blue"){
            digitalWrite(L4R, LOW);
            digitalWrite(L4G, LOW);
            digitalWrite(L4B, HIGH);
        }
        if (colour == "white"){
            digitalWrite(L4R, HIGH);
            digitalWrite(L4G, HIGH);
            digitalWrite(L4B, HIGH);
        }
        if (colour == "off"){
            digitalWrite(L4R, LOW);
            digitalWrite(L4G, LOW);
            digitalWrite(L4B, LOW);
        }
    }
}

void distanceread(void){
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    duration = pulseIn(echoPin, HIGH);
    distance = (duration/2) / 29.1;
}

// Plays a full file from beginning to end with no pause.
void playcomplete(char *name) {
    // call our helper to find and play this name
    playfile(name);
    Serial.println(name);
    while (wave.isplaying) {
        // do nothing while its playing
        readldrs();
        setlight();
        ldrcol();
        
        //readldrs();
        //setlights();
    }
    // now its done playing
}

void playspeed(char *name) {
    int16_t potval;
    uint32_t newsamplerate;
    playfile(name);
    do{
        while (wave.isplaying) {
            
            potval = analogRead(0);
            if ( ((potval - lastpotval) > HYSTERESIS) || ((lastpotval - potval) > HYSTERESIS)) {
                newsamplerate = wave.dwSamplesPerSec;  // get the original sample rate
                Serial.println(newsamplerate);
                LDRreadings[0] = analogRead(A8);
                newsamplerate *= (LDRreadings[0]);       // scale it by the analog value
                newsamplerate /= 2;                // we want to 'split' between 2x sped up and slowed down.
                wave.setSampleRate(newsamplerate);  // set it immediately!
                Serial.println(newsamplerate);
                Serial.println(newsamplerate, DEC);  // for debugging
                lastpotval = potval;
            }
        }
        wave.seek(0);
        wave.play();
    } while (true);
}

void playfile(char *name) {
    // see if the wave object is currently doing something
    if (wave.isplaying) {// already playing something, so stop it!
        wave.stop(); // stop it
    }
    // look in the root directory and open the file
    if (!f.open(root, name)) {
        putstring("Couldn't open file "); Serial.print(name); Serial.print("\n"); return;
    }
    // OK read the file and turn it into a wave object
    if (!wave.create(f)) {
        putstring_nl("Not a valid WAV"); return;
    }
    // ok time to play! start playback
    wave.play();
}

void accread(void) {
    int loopCount = 12;
    for(int i = 0; i< loopCount; ++i) {
        xAccel += analogRead(X_ACCEL_APIN);
        yAccel += analogRead(Y_ACCEL_APIN);
        vRef   += analogRead(V_REF_APIN);
        xRate  += analogRead(X_RATE_APIN);
        yRate  += analogRead(Y_RATE_APIN);
    }
    xAccel /= loopCount;
    yAccel /= loopCount;
    vRef   /= loopCount;
    xRate  /= loopCount;
    yRate  /= loopCount;
}

void accplay(void){
    if (xAccel > 600){
        playcomplete("TRIANGLE.WAV");
        wave.stop();
    }
    if (xAccel < 500){
        playcomplete("WOODBL~1.WAV");
        wave.stop();
    }
    if (yAccel > 600){
        playcomplete("SHAKER~4.WAV");
        wave.stop();
    }
    if (yAccel < 500){
        playcomplete("BELLTREE.WAV");
        wave.stop();
    }
}

void playdistance(void){
    if (distance < 4) {  // This is where the LED On/Off happens
        playcomplete("BELL-O~1.wav");
        wave.stop();
        //digitalWrite(led,LOW); // When the Red condition is met, the Green LED should turn off
        //digitalWrite(led2,HIGH);
    } else if (distance < 10) {  // This is where the LED On/Off happens
        playcomplete("CONGA-~2.wav");
        wave.stop();
        //digitalWrite(led,HIGH); // When the Red condition is met, the Green LED should turn off
        //digitalWrite(led2,LOW);
    } else if (distance < 15) {  // This is where the LED On/Off happens
        playcomplete("CYMBAL~3.wav");
        wave.stop();
        //digitalWrite(led,HIGH); // When the Red condition is met, the Green LED should turn off
        //digitalWrite(led2,LOW);
    }
    if (distance >= 200 || distance <= 0){
        //Serial.println("Out of range");
    } else {
        //Serial.println(" cm");
    }
    Serial.print("Distance: ");
    Serial.println(distance);
    
}

void RGBtest(){
    analogWrite(L1R, 204);
    analogWrite(L2R, 204);
    analogWrite(L3R, 204);
    analogWrite(L4R, 204);
    delay(1000);
    analogWrite(L1R, 0);
    analogWrite(L2R, 0);
    analogWrite(L3R, 0);
    analogWrite(L4R, 0);
    
    digitalWrite(L1G, HIGH);
    digitalWrite(L2G, HIGH);
    digitalWrite(L3G, HIGH);
    digitalWrite(L4G, HIGH);
    delay(1000);
    digitalWrite(L1G, LOW);
    digitalWrite(L2G, LOW);
    digitalWrite(L3G, LOW);
    digitalWrite(L4G, LOW);
    
    digitalWrite(L1B, HIGH);
    digitalWrite(L2B, HIGH);
    digitalWrite(L3B, HIGH);
    digitalWrite(L4B, HIGH);
    delay(1000);
    digitalWrite(L1B, LOW);
    digitalWrite(L2B, LOW);
    digitalWrite(L3B, LOW);
    digitalWrite(L4B, LOW);

}

