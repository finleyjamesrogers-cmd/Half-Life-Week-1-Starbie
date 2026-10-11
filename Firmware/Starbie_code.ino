/*
  Starbie: a tiny motion-controlled digital pet

  This is intentionally one file. Open this .ino file in Arduino IDE, edit
  the BEGINNER SETTINGS section if you want, and upload it to a Seeed XIAO ESP32-C3.

  Install these libraries with Sketch > Include Library > Manage Libraries:
    - Adafruit GFX Library
    - Adafruit SSD1306
    - Adafruit MPU6050
    - DHT sensor library
  Arduino IDE will offer to install their dependencies too. Click Install All.
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <DHT.h>
#include <Preferences.h>
#include <math.h>

// =========================== BEGINNER SETTINGS ===========================
// Everything most builders will want to customize is in this one section.
// Anything else then thats up to you to figure out :) ... dont worry ive put a few comments on the most important parts

// --- Your board's pins ----------------------------------------------------
// Make sure to match these to your board
// These are ESP32-C3 GPIO numbers, with the XIAO pin labels beside them.
const int I2C_SDA_PIN = 6;       // XIAO D4: OLED + MPU6050 SDA
const int I2C_SCL_PIN = 5;       // XIAO D5: OLED + MPU6050 SCL
const int DHT_PIN = 2;           // XIAO D1: DHT11 data
const int BUTTON_ONE_PIN = 3;    // XIAO D2: opens/confirms the radial menu
const int BUTTON_TWO_PIN = 4;    // XIAO D3: shows/hides stats

// Set this to false if your submission does not have a DHT11.
const bool USE_DHT11 = true;

// Most parts use these addresses. Only change them if an I2C scanner says
// your part is different (common OLED alternative: 0x3D).
const uint8_t OLED_ADDRESS = 0x3C;
const uint8_t MPU6050_ADDRESS = 0x68;

// --- Starting stats -------------------------------------------------------
// Stats are 0 to 100. They only change when you select an action or shake
// The higher the starting stats the easier it is at the begining. 
// The preset stats are good for a middle level start.
// Starbie; nothing slowly drains while it sits on your desk.
const int STARTING_JOY = 60; // How happy it is at the start.
const int STARTING_ENERGY = 85; // How much energy it has at the start.
const int STARTING_FULLNESS = 70; // How much saturation/food in the tank it has at the start.

// WARNING this can reset your current pet!
// Set true, upload once, then set it back to false if you want a fresh pet.
const bool RESET_SAVED_PET_ON_BOOT = false;

// --- Radial menu ----------------------------------------------------------
// These four actions appear at TOP, RIGHT, BOTTOM, then LEFT in the menu.
// Tilting the board is what controls what action is selected.
// The last word controls the matching little visual reaction.
enum PetReaction {
  NAP_REACTION,
  JUMP_REACTION,
  HEART_REACTION,
  RUN_REACTION,
};

struct MenuItem {
  const char *label;
  int joyChange;
  int energyChange;
  int fullnessChange;
  PetReaction reaction;
};

// You can change the names of the reactions by changing the names in "" below.
// for example you can change "SNOOZE" to "SLEEP" or something else a bit more creative.
const MenuItem MENU_ITEMS[] = {
  {"SNOOZE",   1,  18, -4, NAP_REACTION},   // top: sleep and emit Zs - sleepy pet
  {"PLAY", 12, -9, -5, RUN_REACTION},    // right: two fast laps + hearts - playfull pet
  {"DANCE",  3,  2,  18, JUMP_REACTION},  // bottom: wiggle and jump - party pet
  {"FEED",   7,  0,  0, HEART_REACTION},  // left: jump and emit hearts - hungry pet
};
const int MENU_ITEM_COUNT = sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]);

// --- Movement feel --------------------------------------------------------
// Bigger MENU_TILT_LIMIT makes the menu ball move less.
// Recomended to try with the preset first then change if you want to after testing.
const float MENU_TILT_LIMIT = 6.0f;

// Again another thing you should only do after testing first.
// MPU6050 orientation: change these settings instead of rewiring your board.
// 1. If the sensor is turned 90 degrees, set SWAP_MPU_AXES to true.
// 2. If a direction feels backwards, change that direction from 1.0f to -1.0f.
const bool SWAP_MPU_AXES = false;
const float MENU_X_DIRECTION = 1.0f;
const float MENU_Y_DIRECTION = -1.0f;

// If the board is randomly choosing options with barely any input then increce deadzone by small increments testing each time.
// A small dead zone makes the ball rest in the center until the board is tilted.
const float MENU_CENTER_DEADZONE = 0.8f;
const float SHAKE_THRESHOLD = 7.0f;

// To make easier increce values or decrece to make harder
// Its recomended to allways have atleast one negative to not make it insainly easy... or if you are bored make them all +1 idrc.
// If you want to make your pet not want to be shaken then set all values to negatives. The preset likes the shakes... or atleast I think they do.
// Shake is the only movement that has an effect outside the radial menu.
const int SHAKE_JOY_CHANGE = 5;
const int SHAKE_ENERGY_CHANGE = -2;
const int SHAKE_FULLNESS_CHANGE = -1;

// --- Your pet's bitmap and animation -------------------------------------
// This is a simplified 32 by 32, one-bit version of the supplied character.
// You can replace it with your own art later; keep these size values matched
// 32x32 is a good size but can be changed!
// to the sprite array you paste in.
const int PET_SPRITE_WIDTH = 32;
const int PET_SPRITE_HEIGHT = 32;

// The pet wanders unless it is asleep. A non-nap action does a tiny shake,
// then a jump, except PLAY which runs two fast laps.
const uint16_t PET_WALK_PIXEL_MS = 71; // Yeah THATS RIGHT, a whole 1ms longer than the OG!
const uint16_t PET_PRE_JUMP_MS = 220; // SHORTER PRE JUMP!!! - due to good pre workout... i think.
const uint16_t PET_JUMP_MS = 470; // more hight = more time in the air. thats gravity btw - free science lesson, your welcome!
const int PET_JUMP_HEIGHT = 18; // Thats a high jump - Gold medal at the pet olympics I believe.
const uint32_t NAP_DURATION_MS = 60000;  // 60 seconds: five times the old nap. - make as long/short as you like... No not that long, I know your thinking.
const uint16_t HEARTS_DURATION_MS = 1800; // 200 more ms than the og! - more love - can be changed.
const uint16_t PLAY_LAP_MS = 1000; // 200 more... again! - can be changed - big breakfast I think.
const uint8_t PLAY_LAP_COUNT = 3; // 1 more lap ;) more running, hehe. - can be changed - dont put it at more than 10, thats mean to your pet :(

// Here is the cool bit, the pet itself :)
// The one that is provided is designed by me and it is a turtle :D, I know very cool.
// You can create your own by going to any pixle designer, I used Piskel, and creating a 32x32 pixle design of a pet -
// - then go to a image2cpp and convert the image to a string of bytes like you see below, then change the bytes below to your own! Copy and paste works fine usualy.
// Each byte stores eight pixels, left to right. This sparse outline keeps the
// supplied creature readable on a tiny, one-bit OLED: head, eye, legs, tail,
// and a small flower-like ear accent all stay separate instead of becoming a blob.
const uint8_t PROGMEM PET_SPRITE[] = {
	0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xf8, 0x1f, 
	0xff, 0xff, 0xf0, 0x0f, 
	0xff, 0xff, 0xe3, 0xe7, 
	0xff, 0xff, 0xe5, 0xb3, 
	0xff, 0xff, 0xc4, 0x9b, 
	0xff, 0xff, 0xce, 0xdb, 
	0xff, 0xe1, 0xcf, 0xfb, 
	0xff, 0x80, 0x49, 0x9b, 
	0xff, 0x0e, 0x0c, 0x33, 
	0xfe, 0x1f, 0x0f, 0xe7, 
	0xfe, 0x4f, 0x47, 0xcf, 
	0xfc, 0xe6, 0x60, 0x1f, 
	0xfc, 0xf0, 0xfc, 0x7f, 
	0xf9, 0xe1, 0xfe, 0xff, 
	0xf9, 0xcc, 0xfc, 0xff, 
	0xf3, 0x9e, 0x79, 0xff, 
	0xe0, 0x1f, 0x35, 0xff, 
	0xe3, 0xcf, 0x89, 0xff, 
	0xf0, 0x30, 0x73, 0xff, 
	0xfc, 0x8f, 0x07, 0xff, 
	0xf8, 0x80, 0x47, 0xff, 
	0xf9, 0x2e, 0x57, 0xff, 
	0xfa, 0x4c, 0x97, 0xff, 
	0xf8, 0x9d, 0x27, 0xff, 
	0xfe, 0x3c, 0x4f, 0xff, 
	0xff, 0xff, 0x1f, 0xff, 
	0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff
};
// ===========================================================================
// You can read below without needing to understand every line. The rest of
// the sketch handles buttons, sensors, drawing, and saving automatically.

const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;
const int MENU_CENTER_X = 64;
const int MENU_CENTER_Y = 32;
const int MENU_ITEM_X[] = {64, 108, 64, 20};
const int MENU_ITEM_Y[] = {12, 32, 53, 32};
const int MENU_BALL_X_RANGE = 37;
const int MENU_BALL_Y_RANGE = 22;
const uint16_t BUTTON_DEBOUNCE_MS = 30;
const uint16_t MPU_READ_INTERVAL_MS = 30;
const uint16_t DHT_READ_INTERVAL_MS = 2200;
const uint16_t SHAKE_COOLDOWN_MS = 650; // you can change it but idk why you would want to lol
const float STANDARD_GRAVITY = 9.80665f;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_MPU6050 mpu;
DHT dht(DHT_PIN, DHT11);
Preferences preferences;

struct PetState {
  int joy;
  int energy;
  int fullness;
};

struct ButtonState {
  int pin;
  bool stableState;
  bool lastRawState;
  uint32_t lastChangedAt;
};

enum View {
  PET_VIEW,
  MENU_VIEW,
  STATS_VIEW,
};

PetState pet = {STARTING_JOY, STARTING_ENERGY, STARTING_FULLNESS};
ButtonState buttonOne = {BUTTON_ONE_PIN, HIGH, HIGH, 0};
ButtonState buttonTwo = {BUTTON_TWO_PIN, HIGH, HIGH, 0};
View currentView = PET_VIEW;

bool mpuFound = false;
bool dhtFound = false;
float accelerationX = 0.0f;
float accelerationY = 0.0f;
float accelerationZ = STANDARD_GRAVITY;
float temperatureC = NAN;
float humidity = NAN;
float menuBallX = MENU_CENTER_X;
float menuBallY = MENU_CENTER_Y;
float menuCenterAccelerationX = 0.0f;
float menuCenterAccelerationY = 0.0f;
int selectedMenuItem = -1;
uint32_t lastMpuReadAt = 0;
uint32_t lastDhtReadAt = 0;
uint32_t lastShakeAt = 0;
uint32_t shakeAnimationEndsAt = 0;
uint32_t petJumpStartedAt = 0;
uint32_t nappingUntil = 0;
uint32_t heartAnimationEndsAt = 0;
uint32_t playRunStartedAt = 0;
int nappingPetX = 0;

int petWalkingX(uint32_t now);

int keepInRange(int value, int smallest, int largest) {
  return constrain(value, smallest, largest);
}

// just keeps the ranges of the stats between 0 and 100 to keep things simple.
void changePet(int joyChange, int energyChange, int fullnessChange) {
  pet.joy = keepInRange(pet.joy + joyChange, 0, 100);
  pet.energy = keepInRange(pet.energy + energyChange, 0, 100);
  pet.fullness = keepInRange(pet.fullness + fullnessChange, 0, 100);
}

// uses the true false in the beginner setting to decide if it needs to reset all the pets stats and preferences, essentialy reseting the pet.
void loadPet() {
  preferences.begin("starbie", false);
  if (RESET_SAVED_PET_ON_BOOT) {
    preferences.clear();
  }
  pet.joy = preferences.getInt("joy", STARTING_JOY);
  pet.energy = preferences.getInt("energy", STARTING_ENERGY);
  pet.fullness = preferences.getInt("full", STARTING_FULLNESS);
}

void savePet() {
  preferences.putInt("joy", pet.joy);
  preferences.putInt("energy", pet.energy);
  preferences.putInt("full", pet.fullness);
}

void setUpButton(ButtonState &button) {
  pinMode(button.pin, INPUT_PULLUP);
  button.stableState = digitalRead(button.pin);
  button.lastRawState = button.stableState;
  button.lastChangedAt = millis();
}

// Returns true exactly once for each press, even if the switch bounces.
bool wasPressed(ButtonState &button) {
  const bool rawState = digitalRead(button.pin);
  const uint32_t now = millis();

  if (rawState != button.lastRawState) {
    button.lastRawState = rawState;
    button.lastChangedAt = now;
  }

  if (rawState != button.stableState &&
      now - button.lastChangedAt >= BUTTON_DEBOUNCE_MS) {
    button.stableState = rawState;
    return button.stableState == LOW;
  }

  return false;
}

void updateMpu() {
  if (!mpuFound || millis() - lastMpuReadAt < MPU_READ_INTERVAL_MS) {
    return;
  }
  lastMpuReadAt = millis();

  sensors_event_t acceleration;
  sensors_event_t gyro;
  sensors_event_t sensorTemperature;
  mpu.getEvent(&acceleration, &gyro, &sensorTemperature);
  accelerationX = acceleration.acceleration.x;
  accelerationY = acceleration.acceleration.y;
  accelerationZ = acceleration.acceleration.z;
}

void updateDht() {
  if (!dhtFound || millis() - lastDhtReadAt < DHT_READ_INTERVAL_MS) {
    return;
  }
  lastDhtReadAt = millis();

  const float newHumidity = dht.readHumidity();
  const float newTemperatureC = dht.readTemperature();
  if (!isnan(newHumidity)) {
    humidity = newHumidity;
  }
  if (!isnan(newTemperatureC)) {
    temperatureC = newTemperatureC;
  }
}

int petWalkingX(uint32_t now) {
  const int farthestX = SCREEN_WIDTH - PET_SPRITE_WIDTH;
  const uint32_t roundTrip = static_cast<uint32_t>(farthestX) * 2;
  const uint32_t step = (now / PET_WALK_PIXEL_MS) % roundTrip;
  return step <= farthestX ? step : roundTrip - step;
}

uint32_t playRunDuration() {
  return static_cast<uint32_t>(PLAY_LAP_MS) * PLAY_LAP_COUNT;
}

bool isNapping() {
  return nappingUntil != 0 && millis() < nappingUntil;
}

bool isPlaying() {
  return playRunStartedAt != 0 && millis() - playRunStartedAt < playRunDuration();
}

int playRunX(uint32_t now) {
  const int farthestX = SCREEN_WIDTH - PET_SPRITE_WIDTH;
  const uint32_t lapAge = (now - playRunStartedAt) % PLAY_LAP_MS;
  const float progress = static_cast<float>(lapAge) / PLAY_LAP_MS;
  return progress < 0.5f ? static_cast<int>(progress * 2.0f * farthestX)
                         : static_cast<int>((1.0f - progress) * 2.0f * farthestX);
}

void updatePetTimers() {
  if (nappingUntil != 0 && !isNapping()) {
    nappingUntil = 0;  // The pet wakes naturally when its nap time is over. - 60 secs btw... yeah ik I would not want to be woken up either - let me sleep fr
  }
  if (playRunStartedAt != 0 && !isPlaying()) {
    playRunStartedAt = 0;  // PLAY is exactly THREE left-to-right-to-left laps. - very strict at the pet olympics
  }
}

// While the menu is open, the MPU6050 acts like a tiny air mouse. The position
// at the instant Button 1 opens the menu becomes the center for that menu use. - yeah you can use this while lying on your side - very cool
void updateMenuBall() {
  float xTilt = accelerationX - menuCenterAccelerationX;
  float yTilt = accelerationY - menuCenterAccelerationY;

  if (SWAP_MPU_AXES) {
    const float oldXTilt = xTilt;
    xTilt = yTilt;
    yTilt = oldXTilt;
  }
  xTilt = constrain(xTilt * MENU_X_DIRECTION, -MENU_TILT_LIMIT, MENU_TILT_LIMIT);
  yTilt = constrain(yTilt * MENU_Y_DIRECTION, -MENU_TILT_LIMIT, MENU_TILT_LIMIT);
  const float targetX = MENU_CENTER_X + (xTilt / MENU_TILT_LIMIT) * MENU_BALL_X_RANGE;
  const float targetY = MENU_CENTER_Y + (yTilt / MENU_TILT_LIMIT) * MENU_BALL_Y_RANGE;

  // This small amount of smoothing makes the ball feel less jittery.
  menuBallX += (targetX - menuBallX) * 0.20f;
  menuBallY += (targetY - menuBallY) * 0.20f;

  // The menu has four full sectors, not four tiny targets. Any tilt direction
  // past the center dead zone selects its matching top/right/bottom/left action. - its not a game of skill, well it depends how bad your aim really is...
  if (fabsf(xTilt) < MENU_CENTER_DEADZONE && fabsf(yTilt) < MENU_CENTER_DEADZONE) {
    selectedMenuItem = -1;
  } else if (fabsf(xTilt) > fabsf(yTilt)) {
    selectedMenuItem = xTilt > 0.0f ? 1 : 3;  // right or left
  } else {
    selectedMenuItem = yTilt > 0.0f ? 2 : 0;  // bottom or top
  }
}

void checkForShake() {
  if (!mpuFound || currentView != PET_VIEW) {
    return;
  }

  const float magnitude = sqrtf(accelerationX * accelerationX +
                                accelerationY * accelerationY +
                                accelerationZ * accelerationZ);
  const uint32_t now = millis();
  if (fabsf(magnitude - STANDARD_GRAVITY) >= SHAKE_THRESHOLD &&
      now - lastShakeAt >= SHAKE_COOLDOWN_MS) {
    lastShakeAt = now;
    nappingUntil = 0;  // A shake wakes a sleeping pet right away. ONLY USE IN EMERGENCY, the pet has the right to get angry... I would anyway.
    shakeAnimationEndsAt = now + 350;
    changePet(SHAKE_JOY_CHANGE, SHAKE_ENERGY_CHANGE, SHAKE_FULLNESS_CHANGE);
    savePet();
  }
}

void openMenu() {
  currentView = MENU_VIEW;
  selectedMenuItem = -1;
  menuBallX = MENU_CENTER_X;
  menuBallY = MENU_CENTER_Y;
  // Calibrate here so every menu starts from however the builder is holding it.
  menuCenterAccelerationX = accelerationX;
  menuCenterAccelerationY = accelerationY;
}

void chooseMenuItem() {
  if (selectedMenuItem == -1) {
    return;  // Keep the menu open until the ball reaches a choice.
  }

  const MenuItem &item = MENU_ITEMS[selectedMenuItem];
  changePet(item.joyChange, item.energyChange, item.fullnessChange);
  savePet();
  const uint32_t now = millis();
  nappingUntil = 0;
  heartAnimationEndsAt = 0;
  playRunStartedAt = 0;

  if (item.reaction == NAP_REACTION) {
    petJumpStartedAt = 0;
    nappingPetX = petWalkingX(now);
    nappingUntil = now + NAP_DURATION_MS;
  } else if (item.reaction == RUN_REACTION) {
    petJumpStartedAt = 0;
    playRunStartedAt = now;
    heartAnimationEndsAt = now + playRunDuration();
  } else {
    // A confirmed action gets a little anticipation shake before the main event... alrighttt its just a jump but still.
    petJumpStartedAt = now;
    if (item.reaction == HEART_REACTION) {
      heartAnimationEndsAt = now + HEARTS_DURATION_MS;
    }
  }
  currentView = PET_VIEW;
}

void handleButtons() {
  if (wasPressed(buttonOne)) {
    if (currentView == MENU_VIEW) {
      chooseMenuItem();
    } else {
      openMenu();
    }
  }

  if (wasPressed(buttonTwo)) {
    // Button 2 never changes the pet. It only lets you peek at the values. stats, stats, a few more stats and some last minute stats.
    currentView = currentView == STATS_VIEW ? PET_VIEW : STATS_VIEW;
  }
}

void drawSparkle(int x, int y) {
  display.drawPixel(x, y, SSD1306_WHITE);
  display.drawPixel(x, y + 3, SSD1306_WHITE);
  display.drawPixel(x, y - 3, SSD1306_WHITE);
  display.drawPixel(x + 3, y, SSD1306_WHITE);
  display.drawPixel(x - 3, y, SSD1306_WHITE);
  display.drawPixel(x + 1, y + 1, SSD1306_WHITE);
  display.drawPixel(x + 2, y + 1, SSD1306_WHITE);
  display.drawPixel(x + 1, y + 2, SSD1306_WHITE);
  display.drawPixel(x + 1, y - 1, SSD1306_WHITE);
  display.drawPixel(x + 2, y - 1, SSD1306_WHITE);
  display.drawPixel(x + 1, y - 2, SSD1306_WHITE);
  display.drawPixel(x - 1, y - 1, SSD1306_WHITE);
  display.drawPixel(x - 2, y - 1, SSD1306_WHITE);
  display.drawPixel(x - 1, y - 2, SSD1306_WHITE);
  display.drawPixel(x - 1, y + 1, SSD1306_WHITE);
  display.drawPixel(x - 2, y + 1, SSD1306_WHITE);
  display.drawPixel(x - 1, y + 2, SSD1306_WHITE);
  display.drawPixel(x + 3, y + 4, SSD1306_WHITE);
  display.drawPixel(x + 4, y + 3, SSD1306_WHITE);
  display.drawPixel(x + 2, y + 3, SSD1306_WHITE);
  display.drawPixel(x + 3, y + 2, SSD1306_WHITE);
  display.drawPixel(x + 3, y - 2, SSD1306_WHITE);
  display.drawPixel(x + 2, y - 3, SSD1306_WHITE);
  display.drawPixel(x + 4, y - 3, SSD1306_WHITE);
  display.drawPixel(x + 3, y - 4, SSD1306_WHITE);
  display.drawPixel(x - 2, y - 3, SSD1306_WHITE);
  display.drawPixel(x - 3, y - 2, SSD1306_WHITE);
  display.drawPixel(x - 3, y - 4, SSD1306_WHITE);
  display.drawPixel(x - 4, y - 3, SSD1306_WHITE);
  display.drawPixel(x - 2, y + 3, SSD1306_WHITE);
  display.drawPixel(x - 3, y + 2, SSD1306_WHITE);
  display.drawPixel(x - 3, y + 4, SSD1306_WHITE);
  display.drawPixel(x - 4, y + 3, SSD1306_WHITE);
}

void drawHeart(int x, int y) {
  // A tiny seven-pixel-wide heart that stays crisp on the OLED, aww cute!
  display.fillRect(x - 2, y, 2, 2, SSD1306_WHITE);
  display.fillRect(x + 1, y, 2, 2, SSD1306_WHITE);
  display.fillRect(x - 3, y + 2, 7, 2, SSD1306_WHITE);
  display.fillRect(x - 2, y + 4, 5, 1, SSD1306_WHITE);
  display.fillRect(x - 1, y + 5, 3, 1, SSD1306_WHITE);
  display.drawPixel(x, y + 6, SSD1306_WHITE);
}

void drawHearts(uint32_t now, int petX, int petY) {
  if (now >= heartAnimationEndsAt) {
    return;
  }

  const float progress =
      1.0f - static_cast<float>(heartAnimationEndsAt - now) / HEARTS_DURATION_MS;
  const int rise = static_cast<int>(progress * 18.0f);
  drawHeart(petX + 10, petY - 3 - rise);
  drawHeart(petX + 22, petY - 9 - rise / 2);
}

void drawSleepZs(uint32_t now, int petX, int petY) {
  // The Zs float upward, then loop back down while the pet is napping. Just like a cartoon!
  const int rise = static_cast<int>((now / 300UL) % 15UL);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(petX + 23, petY - 2 - rise);
  display.print("z");
  display.setCursor(petX + 28, petY - 8 - rise / 2);
  display.print("z");
}

void drawPet() {
  display.clearDisplay();

  const uint32_t now = millis();
  int petX = isNapping() ? nappingPetX : petWalkingX(now);
  if (isPlaying()) {
    petX = playRunX(now);
  }
  int petY = SCREEN_HEIGHT - PET_SPRITE_HEIGHT;

  // The pet always wanders except during a nap; PLAY temporarily takes over. The pet is a real explorer of the small oled screen!
  if (now < shakeAnimationEndsAt) {
    petX += static_cast<int>(sinf(now / 18.0f) * 3.0f);
  }

  if (!isNapping() && petJumpStartedAt != 0) {
    const uint32_t animationAge = now - petJumpStartedAt;

    if (animationAge < PET_PRE_JUMP_MS) {
      petX += static_cast<int>(sinf(now / 16.0f) * 3.0f);
    } else if (animationAge < PET_PRE_JUMP_MS + PET_JUMP_MS) {
      const float jumpProgress =
          static_cast<float>(animationAge - PET_PRE_JUMP_MS) / PET_JUMP_MS;
      petY -= static_cast<int>(sinf(jumpProgress * PI) * PET_JUMP_HEIGHT);
    } else {
      petJumpStartedAt = 0;
    }
  }

  petX = constrain(petX, 0, SCREEN_WIDTH - PET_SPRITE_WIDTH);
  display.drawBitmap(petX, petY, PET_SPRITE, PET_SPRITE_WIDTH, PET_SPRITE_HEIGHT,
                     SSD1306_WHITE);
  
  if (!isNapping() && (now / 250) % 2 == 0) {
  drawSparkle(petX - 4, petY + 18);
}

  if (isNapping()) {
    drawSleepZs(now, petX, petY);
  }
  drawHearts(now, petX, petY);
}

void drawMenuItem(int item) {
  const int boxWidth = 33;
  const int boxHeight = 12;
  const int boxX = MENU_ITEM_X[item] - boxWidth / 2;
  const int boxY = MENU_ITEM_Y[item] - boxHeight / 2;
  const bool isSelected = item == selectedMenuItem;

  if (isSelected) {
    display.fillRoundRect(boxX, boxY, boxWidth, boxHeight, 3, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  } else {
    display.drawRoundRect(boxX, boxY, boxWidth, boxHeight, 3, SSD1306_WHITE);
    display.setTextColor(SSD1306_WHITE);
  }

  display.setTextSize(1);
  const int labelLength = strlen(MENU_ITEMS[item].label);
  display.setCursor(MENU_ITEM_X[item] - labelLength * 3, MENU_ITEM_Y[item] - 3);
  display.print(MENU_ITEMS[item].label);
  display.setTextColor(SSD1306_WHITE);
}

void drawMenu() {
  display.clearDisplay();
  // The two diagonal lines divide the screen into four big selection sectors. State of the art tech... also a good way to show what way you have to tilt it... I guess.
  display.drawLine(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);
  display.drawLine(0, SCREEN_HEIGHT - 1, SCREEN_WIDTH - 1, 0, SSD1306_WHITE);
  for (int item = 0; item < MENU_ITEM_COUNT; item++) {
    drawMenuItem(item);
  }

  display.drawCircle(MENU_CENTER_X, MENU_CENTER_Y, 11, SSD1306_WHITE);
  display.fillCircle(static_cast<int>(menuBallX), static_cast<int>(menuBallY), 4,
                     SSD1306_WHITE);
}

void drawStatBar(int y, const char *label, int value) {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(4, y);
  display.print(label);
  display.drawRect(48, y, 57, 8, SSD1306_WHITE);
  display.fillRect(49, y + 1, map(value, 0, 100, 0, 55), 6, SSD1306_WHITE);
  display.setCursor(109, y);
  display.print(value);
}

// shows bars that fill up depending on the level of Joy/Energy/Fullness that the pet has :)
// the numbers are just how far down the screen they are not the starting values, they are picked in the beginner settings at the top of the code! :)
void drawStats() {
  display.clearDisplay();
  drawStatBar(5, "JOY", pet.joy);
  drawStatBar(20, "ENERGY", pet.energy);
  drawStatBar(35, "FULL", pet.fullness);

  display.setCursor(4, 53);
  if (!isnan(temperatureC)) {
    display.print("TEMP ");
    display.print(static_cast<int>(temperatureC));
    display.print("C");
  } else {
    display.print("TEMP --");
  }

  display.setCursor(76, 53);
  if (!isnan(humidity)) {
    display.print("H ");
    display.print(static_cast<int>(humidity));
    display.print("%");
  } else {
    display.print("H --");
  }
}

void drawCurrentView() {
  if (currentView == MENU_VIEW) {
    drawMenu();
  } else if (currentView == STATS_VIEW) {
    drawStats();
  } else {
    drawPet();
  }
  display.display();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  setUpButton(buttonOne);
  setUpButton(buttonTwo);
  loadPet();

// a built in warning code to give feedback if it doesent go perfect the first time, which lets be honest it wont.
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("OLED not found. Check power, GND, SDA, SCL, and address.");
    while (true) {
      delay(10);
    }
  }

// another warning code
  mpuFound = mpu.begin(MPU6050_ADDRESS, &Wire);
  if (mpuFound) {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  } else {
    Serial.println("MPU6050 not found. The menu will not move until it is connected.");
  }

  if (USE_DHT11) {
    dht.begin();
    dhtFound = true;
  }

  drawCurrentView();
}

void loop() {
  updateMpu();
  updateDht();
  updatePetTimers();
  handleButtons();

  if (currentView == MENU_VIEW) {
    updateMenuBall();
  } else {
    checkForShake();
  }

  drawCurrentView();
  delay(16);  // Smooth display updates without making the code complicated.
}
// thats it! well done you have made it to the bottom of the code sheet!
// you better of read all of my comments... please I spent too much time on those
// anyway thanks and good luck!