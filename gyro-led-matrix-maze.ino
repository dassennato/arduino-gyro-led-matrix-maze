#include <LedControl.h>
#include<Wire.h>

/**
 * ARDUINO SETUP
 */
// LED MAX72XX
const int PIN_LED_DIN = 12; // Pin LED DataIn
const int PIN_LED_CS = 11; // Pin LED LOAD(CS)
const int PIN_LED_CLK = 10; // Pin LED Clock
const int LEDS_AMOUNT = 1; // The number of leds chained, currently the code only supports one.
const int LEDS_BRIGHTNESS = 8; // Intensity of the led, a number between 1-15

LedControl lc = LedControl(PIN_LED_DIN, PIN_LED_CLK, PIN_LED_CS, LEDS_AMOUNT);

// GYRO GY-521 (MPU-6050)
const int MPU_ADDR = 0x68;  // I2C address of the MPU-6050

/**
 * GYRO
 */
int16_t AcX,AcY,AcZ,Tmp,GyX,GyY,GyZ; // raw gyro data
float rotX, rotY, rotZ, gForzeX, gForzeY, gForzeZ; // processed gyro data

/**
 * GAME
 */
const int LEVELS_COUNT = 3; // number of available leves FIXME: We have to calculate this instead of declaring it
const int LEVELS_ROWS = 8; // number of rows of each level
const int LEVELS_COLUMNS = LEVELS_ROWS; // number of columns of each level
const int GAME_SPEED = 70; // The higher number the slower game
const int GAME_WIN_BLINKS = 4; // The number of blinks to show once a level is won
const int GAME_WIN_BLINKS_SPEED = 50; // The speed of the level win blinks. The higher the slower.
const float MOVEMENT_FORZE_OFFSET = 0.25; // The amount of forze needed from the gyro to move the player

/**
 * 's': Start position
 * 'f': Finish position
 * 'X': Wall
 * ' ': Movement zone
 */
char levels[LEVELS_COUNT][LEVELS_ROWS][LEVELS_COLUMNS] = {
  // Level 1
  {
    {'X','s','X','X','X','X','X','X'},
    {'X',' ','X','X','X','X','X','X'},
    {'X',' ','X',' ',' ',' ',' ','X'},
    {'X',' ','X',' ','X','X',' ','X'},
    {'X',' ','X',' ','X','X',' ','X'},
    {'X',' ','X',' ','X','X',' ','X'},
    {'X',' ',' ',' ','X','X',' ','X'},
    {'X','X','X','X','X','X','f','X'},
  },
  // Level 2
  {
    {'X','X','X','X','X','X','s','X'},
    {'f',' ',' ','X','X','X',' ','X'},
    {'X','X',' ','X',' ',' ',' ','X'},
    {'X',' ',' ','X',' ','X','X','X'},
    {'X',' ','X','X',' ','X','X','X'},
    {'X',' ','X','X',' ','X','X','X'},
    {'X',' ',' ',' ',' ','X','X','X'},
    {'X','X','X','X','X','X','X','X'},
  },
  // Level 3
  {
    {'X','X','X','X','X','X','X','X'},
    {'X',' ',' ',' ','X',' ',' ','s'},
    {'X',' ','X',' ','X',' ','X','X'},
    {'X',' ','X',' ',' ',' ','X','X'},
    {'X',' ','X','X','X','X','X','X'},
    {'X',' ','X','X','X','X','X','X'},
    {'X',' ','X','X','X','X','X','X'},
    {'X','f','X','X','X','X','X','X'},
  }
  /*
  // Level Template
  {
    {'X','X','X','X','X','X','X','X'},
    {'X','X','X','X','X','X','X','X'},
    {'X','X','X','X','X','X','X','X'},
    {'X','X','X','X','X','X','X','X'},
    {'X','X','X','X','X','X','X','X'},
    {'X','X','X','X','X','X','X','X'},
    {'X','X','X','X','X','X','X','X'},
    {'X','X','X','X','X','X','X','X'},
  }
  */
};

/**
 * GAME STATE
 */
int currentLevel = 0; // Current level paying
int currentX; // Current player X position
int currentY; // Current player Y position
int startX; // Current level start X position
int startY; // Current level start Y position
int finishX; // Current level finish X position
int finishY; // Current level finish Y position
const int MIN_CURRENT = 0; // The min value of currentX
const int MAX_CURRENT = LEVELS_ROWS - 1; // The max value of currentX
enum Move { Left, Right, Up, Down }; // Possible directions to move the player

void setup() {
  Serial.begin(9600);

  //Setup Gyro
  Wire.begin();
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); // PWR_MGMT_1 register
  Wire.write(0); // set to zero (wakes up the MPU-6050)
  Wire.endTransmission(true);

  //Setup Led
  /*
   The MAX72XX is in power-saving mode on startup,
   we have to do a wakeup call
   */
  lc.shutdown(0, false);
  /* Set the brightness to a medium values */
  lc.setIntensity(0, 8);
  /* and clear the display */
  lc.clearDisplay(0);

  initLevel();
}

void loop() {
  fetchGyro();
//  debugGyro();
//  debugGyroProcessed();

  hidePlayer();
  delay(GAME_SPEED / 2);

  // Gyro movement
  if (gForzeY > MOVEMENT_FORZE_OFFSET && tryToMove(Left)) {
    Serial.println("Moved Left");
  }
  else if(gForzeY < -MOVEMENT_FORZE_OFFSET && tryToMove(Right)) {
    Serial.println("Moved Right");
  }
  else if(gForzeZ > MOVEMENT_FORZE_OFFSET && tryToMove(Down)) {
    Serial.println("Moved Down");
  }
  else if(gForzeZ < -MOVEMENT_FORZE_OFFSET && tryToMove(Up)) {
    Serial.println("Moved Up");
  }

  showPlayer();
  delay(GAME_SPEED / 2);

  checkWin();
}

void fetchGyro() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true);  // request a total of 14 registers
  AcX=Wire.read()<<8|Wire.read();  // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)    
  AcY=Wire.read()<<8|Wire.read();  // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  AcZ=Wire.read()<<8|Wire.read();  // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
  Tmp=Wire.read()<<8|Wire.read();  // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
  GyX=Wire.read()<<8|Wire.read();  // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  GyY=Wire.read()<<8|Wire.read();  // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  GyZ=Wire.read()<<8|Wire.read();  // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)

  rotX = GyX / 131.0;
  rotY = GyY / 131.0;
  rotZ = GyZ / 131.0;

  gForzeX = AcX / 16384.0;
  gForzeY = AcY / 16384.0;
  gForzeZ = AcZ / 16384.0;
}

void debugGyro() {
  Serial.print("AcX = "); Serial.print(AcX);
  Serial.print(" | AcY = "); Serial.print(AcY);
  Serial.print(" | AcZ = "); Serial.print(AcZ);
  Serial.print(" | Tmp = "); Serial.print(Tmp/340.00+36.53);  //equation for temperature in degrees C from datasheet
  Serial.print(" | GyX = "); Serial.print(GyX);
  Serial.print(" | GyY = "); Serial.print(GyY);
  Serial.print(" | GyZ = "); Serial.println(GyZ);
}

void debugGyroProcessed() {
  Serial.print("gForzeX = "); Serial.print(gForzeX);
  Serial.print(" | gForzeY = "); Serial.print(gForzeY);
  Serial.print(" | gForzeZ = "); Serial.print(gForzeZ);
  Serial.print(" | rotX = "); Serial.print(rotX);
  Serial.print(" | rotY = "); Serial.print(rotY);
  Serial.print(" | rotZ = "); Serial.println(rotZ);
}

void initLevel() {
  lc.clearDisplay(0);

  // sets startX, startY, finishX and finishY
  for (int x=0; x<LEVELS_ROWS; x++) {
    for (int y=0; y<LEVELS_COLUMNS; y++) {
      if (levels[currentLevel][x][y] == 's') {
        startX = x;
        startY = y;
      } else if (levels[currentLevel][x][y] == 'f') {
        finishX = x;
        finishY = y;
      }
    }
  }

  currentX = startX;
  currentY = startY;

  printLevel();
}

void showPlayer() {
  lc.setLed(0,currentX,currentY,true);
}

void hidePlayer() {
  lc.setLed(0,currentX,currentY,false);
}

void printLevel() {
  for (int x=0; x<LEVELS_ROWS; x++) {
    for (int y=0; y<LEVELS_COLUMNS; y++) {
      if (levels[currentLevel][x][y] == 'X') {
        lc.setLed(0,x,y,true);  
      }
    }
  }
}

/**
 * Tries to move the player to a given direction.
 * The movement is allowed based on MIN_CURRENT and MAX_CURRENT values 
 * and also if the desired movement is towards an allowed movement placed, i.e. 'X', 's', 'f'
 * Returns true if the movement was successful, otherwise false
 */
boolean tryToMove(Move movement) {
  int possibleX = currentX;
  int possibleY = currentY;

  if (movement == Left && currentX > MIN_CURRENT) {
    possibleX--;
  } else if(movement == Right && currentX < MAX_CURRENT) {
    possibleX++;
  } else if(movement == Down && currentY > MIN_CURRENT) {
    possibleY--;
  } else if(movement == Up && currentY < MAX_CURRENT) {
    possibleY++;
  }

  // Checks what would happen if we apply the possible new position
  if (levels[currentLevel][possibleX][possibleY] != 'X') {
    currentX = possibleX;
    currentY = possibleY;
    return true;
  }
  return false;
}

void checkWin() {
  if (levels[currentLevel][currentX][currentY] == 'f') {
    showWinScreen();
    changeToNextLevel();
    initLevel();
  }
}

void showWinScreen() {
  for (int i = 0; i < GAME_WIN_BLINKS; i++) {
    lc.clearDisplay(0);
    delay(GAME_WIN_BLINKS_SPEED);
    printLevel();
    delay(GAME_WIN_BLINKS_SPEED);  
  }
}

void changeToNextLevel() {
  if (currentLevel + 1 < LEVELS_COUNT) {
    currentLevel++;
  } else {
    currentLevel = 0;
  }
}
