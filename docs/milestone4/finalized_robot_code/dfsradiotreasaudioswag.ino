/* 
 *    Maze mapping Robot
 *  
 *  
 */

#include <Servo.h>
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
#define LOG_OUT 1 // use the log output function
#define FFT_N 128 // set to 256 point fft
#include <FFT.h> // include the library

/**************************************************************************************Pin assignments*/
// Pin Assignments
const int rightServoPin = 6;
const int leftServoPin = 5;

const int middleRightPin = A2;
const int middleLeftPin = A1;
const int outerRightPin = A5;
const int outerLeftPin = A3;

const int audioPin = A4;
int startSignal = 0;

const int ledPin = 13;
const int pushButton = 8;

const int frontWall = 4;
const int leftWall = 3;
const int rightWall = 7;

int pin = 2; // treasure detecting triggering pin
int pin_irq = 0; //IRQ that matches to pin 2
volatile int IRQcount;

Servo rightServo;
Servo leftServo; 

/************************************************************************************radio pins and defs*/
RF24 radio(9,10);
const uint64_t pipes[2] = { 0x0000000038LL, 0x0000000039LL };
typedef enum { role_ping_out = 1, role_pong_back } role_e;
const char* role_friendly_name[] = { "invalid", "Ping out", "Pong back"};
role_e role = role_pong_back;
/**************************************************************************************radio stuff end*/

// Constants
#define WALL_NORTH  1 // 0001
#define WALL_EAST   2 // 0010
#define WALL_SOUTH  4 // 0100
#define WALL_WEST   8 // 1000

const int threshold = 800; 

const int delayTime = 2;

typedef enum {
              NORTH = 1,
              EAST  = 2,
              SOUTH = 4,
              WEST  = 8,
              } ORIENTATION;

typedef enum {
              FORWARD = 0,
              LEFT  = 1,
              RIGHT = 2,
              } TURN;

/*************************************************************************************Global variables*/
// Global Variables
int leftServoAngle = 90;
int rightServoAngle = 90;

void writeAngles(int left, int right){
  rightServoAngle = rightServoMap(right);
  leftServoAngle = left;
  rightServo.write(rightServoAngle);
  leftServo.write(leftServoAngle);
  delay(delayTime);
}
int middleLeftVal = 0;
int middleRightVal = 0;
int outerLeftVal = 0;
int outerRightVal = 0;

int visitedBox = 0;

byte robotX = 3;

byte robotY = 4;

byte pastPosX = NULL;
byte pastPosY = NULL;

byte robotOrientation = NORTH;

byte xleft=robotX;
byte yleft=robotY;
byte xfront=robotX;
byte yfront=robotY;
byte xright=robotX;
byte yright=robotY;
byte xback=robotX;
byte yback=robotY;

bool detectNoFWall = true;
bool detectNoRWall = true;
bool detectNoLWall = true;
bool accessGranted = false;
bool doOpposite    = true;

byte visited[5][4] =  {{0,0,0,0},
                       {0,0,0,0},
                       {0,0,0,0},
                       {0,0,0,0},
                       {0,0,0,6}};
                  
byte wall[5][4] = {{9,1,1,3},
                   {8,0,0,2},
                   {8,0,0,2},
                   {8,0,0,2},
                   {12,4,4,6}};

byte posStack [20][2];
byte turnStack [20];
byte cardinalStack [30];
int stackIndex=0;

// printer functions
void printCardinalStack(){
  Serial.println("this is the cardinal stack!");
  Serial.print("[");
  Serial.print(cardinalStack[0]);
  Serial.print(", ");
  Serial.print(cardinalStack[1]);
  Serial.print(", ");
  Serial.print(cardinalStack[2]);
  Serial.print(", ");
  Serial.print(cardinalStack[3]);
  Serial.print(", ");
  Serial.print(cardinalStack[4]);
  Serial.print(", ");
  Serial.print(cardinalStack[5]);
  Serial.print(", ");
  Serial.print(cardinalStack[6]);
  Serial.print(", ");
  Serial.print(cardinalStack[7]);
  Serial.print(", ");
  Serial.print(cardinalStack[8]);
  Serial.print(", ");
  Serial.print(cardinalStack[9]);
  Serial.print(", ");
  Serial.print(cardinalStack[10]);
  Serial.print(", ");
  Serial.print(cardinalStack[11]);
  Serial.print(", ");
  Serial.print(cardinalStack[12]);
  Serial.print(", ");
  Serial.print(cardinalStack[13]);
  Serial.print(", ");
  Serial.print(cardinalStack[14]);
  Serial.print(", ");
  Serial.print(cardinalStack[15]);
  Serial.print("]");
}

void printState(){
  //dfs to choose where to go next
    Serial.print(robotX);
    Serial.print(" ,");
    Serial.println(robotY);

    if (robotOrientation == NORTH){
    Serial.println("NORTH");
    }
    else if (robotOrientation == EAST) {
    Serial.println("EAST");
    }
    else if (robotOrientation == SOUTH) {
    Serial.println("SOUTH");
    }
    else if (robotOrientation == WEST) {
    Serial.println("WEST");
    }
    
    Serial.print(visited[0][0]);
    Serial.print(visited[0][1]);
    Serial.print(visited[0][2]);
    Serial.println(visited[0][3]);
    
    Serial.print(visited[1][0]);
    Serial.print(visited[1][1]);
    Serial.print(visited[1][2]);
    Serial.println(visited[1][3]);

    Serial.print(visited[2][0]);
    Serial.print(visited[2][1]);
    Serial.print(visited[2][2]);
    Serial.println(visited[2][3]);

    Serial.print(visited[3][0]);
    Serial.print(visited[3][1]);
    Serial.print(visited[3][2]);
    Serial.println(visited[3][3]);

    Serial.print(visited[4][0]);
    Serial.print(visited[4][1]);
    Serial.print(visited[4][2]);
    Serial.println(visited[4][3]);
}

//the matrix that the robot is going to transmit            
word makeSend [5][4];


int rightServoMap(int angle) {
  int newAngle = 180 - angle;
  return newAngle;
}

void rightOrientation(){
  if(robotOrientation == NORTH){
    robotOrientation = EAST;
  }
  else if(robotOrientation == EAST) {
    robotOrientation = SOUTH;
  }
  else if(robotOrientation == SOUTH) {
    robotOrientation = WEST;
  }
  else if(robotOrientation == WEST) {
    robotOrientation = NORTH;
  }
}

void leftOrientation(){
  if(robotOrientation == NORTH){
    robotOrientation = WEST;
  }
  else if(robotOrientation == WEST) {
    robotOrientation = SOUTH;
  }
  else if(robotOrientation == SOUTH) {
    robotOrientation = EAST;
  }
  else if(robotOrientation == EAST) {
    robotOrientation = NORTH;
  }
}

bool withinMazeX(byte xCoord) {
  if(xCoord > 3 || xCoord < 0) {
    return false;
  } else {
    return true;
  } 
}

bool withinMazeY(byte yCoord) {
  if(yCoord > 4 || yCoord < 0) {
    return false;
  } else {
    return true;
  } 
}

//OK
void neighbourIndex(){
  if(robotOrientation == NORTH){
     xleft=robotX-1;
     yleft=robotY;
     xfront=robotX;
     yfront=robotY-1;
     xright=robotX+1;
     yright=robotY;
     xback=robotX;
     yback=robotY+1;
  }
  if(robotOrientation == EAST) {
     xleft=robotX;
     yleft=robotY-1;
     xfront=robotX+1;
     yfront=robotY;
     xright=robotX;
     yright=robotY+1;
     xback=robotX-1;
     yback=robotY;
  }
  if(robotOrientation == SOUTH) {
     xleft=robotX+1;
     yleft=robotY;
     xfront=robotX;
     yfront=robotY+1;
     xright=robotX-1;
     yright=robotY;
     xback=robotX;
     yback=robotY-1;
  }
  if(robotOrientation == WEST) {
     xleft=robotX;
     yleft=robotY+1;
     xfront=robotX-1;
     yfront=robotY;
     xright=robotX;
     yright=robotY-1;
     xback=robotX+1;
     yback=robotY;
  }
  if (robotX==NULL  && robotY==NULL){
    xfront=3;
    yfront=4;
  }
}

/***********************************************************************************Movement functions*/

void rightTurn() {
  leftServoAngle = 180;
  rightServoAngle = rightServoMap(0);
  leftServo.write(leftServoAngle);
  rightServo.write(rightServoAngle);
  delay(400);

  while ((analogRead(outerLeftPin) < threshold) && (analogRead(outerRightPin) < threshold)){
    leftServo.write(leftServoAngle);
    rightServo.write(rightServoAngle);
    delay(delayTime);
  }
  
  rightServoAngle = rightServoMap(180);
  leftServoAngle = 180;
  leftServo.write(leftServoAngle);
  rightServo.write(rightServoAngle);
  delay(200);

  rightOrientation();
  visited[yright][xright] = 6;
  visited[robotY][robotX] = 1;
  robotX = xright;
  robotY = yright;
}

void opposite() {
  leftServoAngle = 180;
  rightServoAngle = rightServoMap(0);
  leftServo.write(leftServoAngle);
  rightServo.write(rightServoAngle);
  delay(400);
  while ((analogRead(outerLeftPin) < threshold) && (analogRead(outerRightPin) < threshold)){
    leftServo.write(leftServoAngle);
    rightServo.write(rightServoAngle);
    delay(delayTime);
  }
  leftServoAngle = 180;
  rightServoAngle = rightServoMap(0);
  leftServo.write(leftServoAngle);
  rightServo.write(rightServoAngle);
  delay(400);
  while ((analogRead(outerLeftPin) < threshold) && (analogRead(outerRightPin) < threshold)){
    leftServo.write(leftServoAngle);
    rightServo.write(rightServoAngle);
    delay(delayTime);
  }
  rightServoAngle = rightServoMap(180);
  leftServoAngle = 180;
  leftServo.write(leftServoAngle);
  rightServo.write(rightServoAngle);
  delay(200);

  rightOrientation();
  rightOrientation();
  visited[yback][xback] = 6;
  visited[robotY][robotX] = 1;
  robotX = xback;
  robotY = yback;
}

void leftTurn(){
  leftServoAngle = 0;
  rightServoAngle = rightServoMap(180);
  leftServo.write(leftServoAngle);
  rightServo.write(rightServoAngle);
  delay(400);
  while ((analogRead(outerLeftPin) < threshold) && (analogRead(outerRightPin) < threshold)){
    leftServo.write(leftServoAngle);
    rightServo.write(rightServoAngle);
    delay(delayTime);
  }  
  rightServoAngle = rightServoMap(180);
  leftServoAngle = 180;
  leftServo.write(leftServoAngle);
  rightServo.write(rightServoAngle);
  delay(200);
  leftOrientation();
  
    if (robotX == NULL && robotY == NULL) {
      robotX = 3;
      robotY = 4;
      visited[robotY][robotX] = 6;
    } 
    else {
      visited[yleft][xleft]=6;
      visited[robotY][robotX] = 1;
      robotX = xleft;
      robotY = yleft;
    }    
}

void lineFollow(){
  if ((middleLeftVal > threshold) && (middleRightVal < threshold)) {
    writeAngles(95,180);
  }
  if ((middleLeftVal < threshold) && (middleRightVal > threshold)) {
    writeAngles(180,95);
  }
  if ((middleLeftVal > threshold) && (middleRightVal > threshold)) {
    writeAngles(180,180);
  }    
}

void goStraight(){
  writeAngles(180,180);
  delay(150);
    
  if (robotX == NULL && robotY == NULL) {
    robotX = 3;
    robotY = 4;
    visited[robotY][robotX] = 6;
  } 
  else {
    visited[yfront][xfront]=6;
    visited[robotY][robotX] = 1;
    robotX = xfront;
    robotY = yfront;
  }    
}

void stop(){
  writeAngles(90,90);
  delay(100);
}

word assemble(word x, word y) {
  word xShift = x;
  xShift << 14;
  word yShift = y;
  yShift << 11;
  word base = xShift || yShift;
  base = base || (visited[y][x] << 8);
  base = base || (wall[y][x] << 4); 
}

//CHECK LOIS LOOK HERE INDEX STUFF
void make2send(){
  for (int i=0; i<5; i++){
    for (int j=0; j<4; i++){
      makeSend[i][j]= assemble (i,j);
    }
  }
}
//OK
/*********************************************************************************************Light up*/
void lightUp(){
  // check if done with maze, aka, all have been visited
  pinMode (ledPin, HIGH);
}

//OK
byte wallOrientation() {
  byte wall = 0;
  if(robotOrientation == NORTH) {
    if(!detectNoFWall){
      wall |= NORTH;
    }
    if(!detectNoLWall){
      wall |= WEST;
    }
    if(!detectNoRWall){
      wall |= EAST;
    }  
  }
  else if(robotOrientation == EAST) {
    if(!detectNoFWall){
      wall |= EAST;
    }
    if(!detectNoLWall){
      wall |= NORTH;
    }
    if(!detectNoRWall){
      wall |= SOUTH;
    } 
  }
  else if(robotOrientation == SOUTH) {
    if(!detectNoFWall){
      wall |= SOUTH;
    }
    if(!detectNoLWall){
      wall |= EAST;
    }
    if(!detectNoRWall){
      wall |= WEST;
    } 
  }
  else if(robotOrientation == WEST) {
    if(!detectNoFWall){
      wall |= WEST;
    }
    if(!detectNoLWall){
      wall |= SOUTH;
    }
    if(!detectNoRWall){
      wall |= NORTH;
    } 
  }
  return wall;  
}

//Need to check where to place this function
void updateWallMatrix() {
  wall[robotY][robotX] = wallOrientation();
}
/**************************************************************************************Stack functions*/
//stack helper functions

void stack_push(byte x, byte y) {
  posStack[stackIndex][0] = x;
  posStack[stackIndex][1] = y;
  stackIndex++;
}

// Return true if the stack is empty. Only works on the stack defined globally as int stack[50].
// problematic because when you add to turnstack, won't have same index, must also pop turnstack separately
bool stack_empty() {
  return (stackIndex == 0);
}

bool turnStack_empty() {
  return (stackIndex == 0);
}

bool cardinalStack_empty(){
  return (stackIndex==0);
}

// Return the value on top of the stack.
// Need to verify
int turnStack_top () {
  if (turnStack_empty()) return NULL;
  return turnStack[stackIndex];
}

//THIS IS FOR THE TURN STUFF, USE BOTH
ORIENTATION from (){
  return robotOrientation;
}

ORIENTATION to() {
  if (cardinalStack_empty()) return NULL;
  return cardinalStack[stackIndex];
}



void cardinalTurn(){
  //lois you moved this here
  neighbourIndex(); 
  
//  straight
  if (from()==1 && to()==1 && withinMazeX(xfront) && withinMazeY(yfront)){
    goStraight();
  }  
  else if (from()==2 && to()==2 && withinMazeX(xfront) && withinMazeY(yfront)){
    goStraight();
  }
  else if(from()==3 && to()==3 && withinMazeX(xfront) && withinMazeY(yfront)){
    goStraight();
  }
  else if(from()==4 && to()==4 && withinMazeX(xfront) && withinMazeY(yfront)){
    goStraight();
  }
  //right
  else if (from()==1 && to()==2 && withinMazeX(xright) && withinMazeY(yright)){
    rightTurn();
  }
  else  if (from()==4 && to()==8 && withinMazeX(xright) && withinMazeY(yright)){
    rightTurn();
  }
  else if (from()==8 && to()==1 && withinMazeX(xright) && withinMazeY(yright)){
    rightTurn();
  }
  else if (from()==2 && to()==4 && withinMazeX(xright) && withinMazeY(yright)){
    rightTurn();
  }
  
  //left
  else if (from()==1 && to()==8 && withinMazeX(xleft) && withinMazeY(yleft)){
    leftTurn();
  }
  else if (from()==4 && to()==2 && withinMazeX(xleft) && withinMazeY(yleft)){
    leftTurn();
  }
  else if (from()==8 && to()==4 && withinMazeX(xleft) && withinMazeY(yleft)){
    leftTurn();
  }
  else if (from()==2 && to()==1 && withinMazeX(xleft) && withinMazeY(yleft)){
    leftTurn();
  }
  
  //opposite
  else if (from()==1 && to()==4 && withinMazeX(xback) && withinMazeY(yback)){
    opposite();
  }
  else if (from()==4 && to()==1 && withinMazeX(xback) && withinMazeY(yback)){
    opposite();
  }
  else if (from()==8 && to()==2 && withinMazeX(xback) && withinMazeY(yback)){
    opposite();
  }
  else if (from()==2 && to()==8 && withinMazeX(xback) && withinMazeY(yback)){
    opposite();
  } 
}
  

//OK
void stack_pop() {
  if (stack_empty()) return ;
  stackIndex--;
}


void cardinalStackLeftTurn(){
  Serial.println("cardinal stack left turn");
  if (robotOrientation==NORTH){
    cardinalStack[stackIndex]=EAST;
  }
  if (robotOrientation==SOUTH){
    cardinalStack[stackIndex]=WEST;
  }
  if (robotOrientation==EAST){
    cardinalStack[stackIndex]=SOUTH;
  }
  if (robotOrientation==WEST){
    cardinalStack[stackIndex]=NORTH;
  }
  return;
}


void cardinalStackRightTurn(){
    Serial.println("cardinal stack right turn");
  
  if (robotOrientation==NORTH){
    cardinalStack[stackIndex]=WEST;
  }
  if (robotOrientation==SOUTH){
    cardinalStack[stackIndex]=EAST;
  }
  if (robotOrientation==EAST){
    cardinalStack[stackIndex]=NORTH;
  }
  if (robotOrientation==WEST){
    cardinalStack[stackIndex]=SOUTH;
  }
  return;
}

void cardinalStackStraight(){
  Serial.println("cardinal stack straight");
  if (robotOrientation==NORTH){
    cardinalStack[stackIndex]=SOUTH;
  }
  if (robotOrientation==SOUTH){
    cardinalStack[stackIndex]=NORTH;
  }
  if (robotOrientation==EAST){
    cardinalStack[stackIndex]=WEST;
  }
  if (robotOrientation==WEST){
    cardinalStack[stackIndex]=EAST;
  }
  return;
}


/********************************************************************************************Backtrack*/

//helper function: pop current position, go to previous position, 
//check left and right sides for unvisited and walls (priority)
//if both are not possible, then go to previous in stack
//Needs work as a whole  

/*
 * backtrack() puts the robot in the last previous position 
 * from its stack that has an accessible, unvisited position to go to.
 */
void backtrack(){
  Serial.println("in backtrack");
  Serial.print("Stack Index:");
  Serial.println(stackIndex);
  cardinalTurn();
  stack_pop();
  
  //while there is not unvisited neighbor 
  //lois why do we need this while loop?
//  while ((!detectNoLWall || visited[yleft][xleft]==1 ) && (!detectNoRWall || visited[yright][xright]==1) 
//  && (!detectNoFWall || visited[yfront][xfront]==1) && !cardinalStack_empty()){
    //printer();
    //using from and to, determine direction to go to
//    cardinalTurn();
//    stack_pop();
    //at an intersection, read the current orientation and the orientation of the next on, 
//  }
}

// treasure detecting code
void IRQcounter() {
  IRQcount++;
}

void treasureDetect() {
   cli();//disable interrupts
  IRQcount = 0;
  sei();//enable interrupts

  delay(25);

  cli();//disable interrupts
  int result = IRQcount;
  sei();//enable interrupts

  Serial.print(F("Counted = "));
  Serial.println(result);

  if(result> 130 && result < 180) {
      Serial.println("7 kHz");
    }
    else if(result> 240 && result < 270){
      Serial.println("12 kHz");
    }
    else if(result>  310 && result < 390){
      Serial.println("17 kHz");
    }
    else{
      Serial.println("none");
    }
    Serial.println("");
}

void audioFFT(){
  // cli();  // UDRE interrupt slows this way down on arduino1.0
  while (startSignal == 0) {
    for (int i = 0 ; i < 256 ; i += 2) { // save 128 samples
      fft_input[i] = analogRead(audioPin); // put real data into even bins
      fft_input[i+1] = 0; // set odd bins to 0
    }
    fft_window(); // window the data for better frequency response
    fft_reorder(); // reorder the data before doing the fft
    fft_run(); // process the data in the fft
    fft_mag_log(); // take the output of the fft
    //sei();
//    Serial.println("start");
    for (byte i = 0 ; i < FFT_N/2 ; i++) { 
//      Serial.println(fft_log_out[i]); // send out the data
      if (fft_input[18] > 60)
        startSignal = 1;
        delay(100);
        digitalWrite(LED_BUILTIN, HIGH);      
    }
  }
}

/*******************************************************************************************Setting up*/
//Start robot behind first intersection, in the case where there is a
//wall right in front of the robot
void setup() {
  rightServo.attach(rightServoPin);
  leftServo.attach(leftServoPin);
  

  pinMode(frontWall, INPUT);
  pinMode(rightWall, INPUT);
  pinMode(leftWall, INPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(pushButton, INPUT);

  Serial.begin(9600);

  attachInterrupt(digitalPinToInterrupt(pin), IRQcounter, RISING);
  audioFFT();
  
  writeAngles(90,90);

  delay(2000); //Wait 2 seconds

  /**************************************************************************************radio stuff start*/
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(57600);
  printf_begin();
  printf("\n\rRF24/examples/GettingStarted/\n\r");
  printf("ROLE: %s\n\r",role_friendly_name[role]);
  printf("*** PRESS 'T' to begin transmitting to the other node\n\r");

  // Setup and configure rf radio
 
  radio.begin();

  radio.setRetries(15,15);
  radio.setAutoAck(true);
  radio.setChannel(0x50);
  // set the power
  radio.setPALevel(RF24_PA_MIN);
  radio.setDataRate(RF24_250KBPS);

//  //**************************************************************************//

  if ( role == role_ping_out )
  {
    radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1,pipes[1]);
  }
  else
  {
    radio.openWritingPipe(pipes[1]);
    radio.openReadingPipe(1,pipes[0]);
  }
  radio.startListening();
  radio.printDetails();
}

/*************************************************************************************************Loop*/
void loop() { 

  //Could combine outer left and outer right line sensors 
  // greater than 750 on the line
  // less than 750 off the line
  middleLeftVal = analogRead(middleLeftPin);
  middleRightVal = analogRead(middleRightPin);
  outerLeftVal = analogRead(outerLeftPin);
  outerRightVal = analogRead(outerRightPin);
  
  lineFollow();
  
  //Detect walls
  detectNoFWall = digitalRead(frontWall);
  detectNoRWall = digitalRead(rightWall);
  detectNoLWall = digitalRead(leftWall);

  //intersection
  if((outerLeftVal > threshold) && (outerRightVal > threshold)){
    printCardinalStack();
    printState();
    treasureDetect();
/**************************************************************************************************DFS*/
    //dfs
    //when we are at a new position (at intersection)
    //IF NEW PLACE EXISTS FROM CURRENT POSITION
    //check walls and surrounded areas' visited states

    neighbourIndex();   //so it'll set front back left right indexes correctly based on the robot orientation
    updateWallMatrix();
    //front priority
    //check wall sensor
    if (detectNoFWall && visited[yfront][xfront] == 0 && withinMazeX(xfront) && withinMazeY(yfront)) {
      //check visited (so if unvisited go straight) 
      Serial.println("going straight!");       
      //go front
      //update visited to reflect new current position and set the old current position to 1
      //I changed the movement functions so that they'll update the old pos and new pos according to how they move
      goStraight();
      //add to stack
      turnStack[stackIndex] = FORWARD; 
      cardinalStackStraight();
      stack_push(robotX, robotY);
    }
    else if (detectNoLWall && visited[yleft][xleft] == 0 && withinMazeX(xleft) && withinMazeY(yleft)) {
      //go left
      //update visited to reflect new current position and set the old current position to 1
      Serial.println("turn left");
      leftTurn();
      //add to stack 
    
      turnStack[stackIndex] = RIGHT;
      cardinalStackLeftTurn();
      stack_push(robotX, robotY);
    } 
    else if (detectNoRWall && visited[yright][xright] == 0 && withinMazeX(xright) && withinMazeY(yright)) {
      Serial.println("turn right");
      rightTurn();
      //go right
      //update visited to reflect new current position and set the old current position to 1
      //add to stack
      turnStack[stackIndex] = LEFT; 
      cardinalStackRightTurn();
      stack_push(robotX, robotY);        
    } 
    else {
      backtrack();
      //helper function: pop current position, go to previous position, 
      //check left and right sides for unvisited and walls (priority)
      //if both are not possible, then go to previous in stack
    }
  }
  else{
    lightUp();

  }

   /******************************************************************************************radio start*/
//need to call make2send so it updates the matrix makeSend
make2send();
//ping out 

  if (role == role_ping_out)
  {
    radio.stopListening();
    
    //SENDING THE MAZE LOOK HERE 
    printf("Now sending the word maze");
    bool ok = radio.write( &makeSend, sizeof(word));

    if (ok)
      printf("ok...");
    else
      printf("failed.\n\r");


    radio.startListening();

    unsigned long started_waiting_at = millis();
    bool timeout = false;
    while ( ! radio.available() && ! timeout )
      if (millis() - started_waiting_at > 200 )
        timeout = true;

    if ( timeout )
    {
      printf("Failed, response timed out.\n\r");
    }
    else
    {
      unsigned long got_time;
      radio.read( &got_time, sizeof(unsigned long) );
      printf("Got response %lu, round-trip delay: %lu\n\r",got_time,millis()-got_time);
    }
    delay(1000);
  

 
}
}
  