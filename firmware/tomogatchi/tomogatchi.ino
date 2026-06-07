/*
  TRAX_TOM - Virtual Pet for ESP32-C3 SuperMini
  Pin Map: SDA=8, SCL=9, BTN1=GPIO0, BTN2=GPIO3, TOUCH=10, BUZZER=6

  CONTROLS:
   HOME:    B1(GPIO0)=Stats   B2(GPIO3)=Pet Chooser   BOTH=Pet/Feed   Touch=Menu
   MENU:    B1=Left  B2(GPIO3)=Right   Touch=Select   BOTH=Exit
   STATS:   B1/B2=flip page   Touch=Back
   CHOOSER: B1=Prev pet  B2(GPIO3)=Next pet  Touch=Equip  BOTH=Exit

  3 cute pets included, all with feline features + big cute eyes:
   0 = Mochi  (round kitten)
   1 = Pip    (big-eared fox-cat)
   2 = Bean   (sleepy chubby cat)
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SDA_PIN 8
#define SCL_PIN 9
#define BTN1_PIN 0
#define BTN2_PIN 3
#define TOUCH_PIN 10
#define BUZZ_PIN 6
#define SW 128
#define SH 64

Adafruit_SSD1306 oled(SW, SH, &Wire, -1);

// ===================== TIMING =====================
unsigned long ms=0, lastStatDecay=0, lastIdleAnim=0, lastBlink=0;
unsigned long lastParticle=0, lastDayCheck=0, petBornTime=0;
unsigned long sleepStartTime=0, lastZzz=0, lastBounce=0;
unsigned long lastMiniGameTick=0, touchStartTime=0, lastTouchReact=0;
unsigned long lastSickShake=0, evolveAnimStart=0, deathAnimStart=0;
unsigned long bootAnimStart=0, lastBreath=0, idleTimer=0;
unsigned long lastScreenSaver=0, bothPressTime=0;

// ===================== INPUT =====================
bool btn1=0,btn2=0,tch=0,btn1Prev=0,btn2Prev=0,tchPrev=0;
bool btn1Rose=0,btn2Rose=0,tchRose=0;
bool bothRose=0, bothPrev=0;
unsigned long btn1DownTime=0,btn2DownTime=0;
int touchCount=0;
unsigned long touchBurstStart=0;

// ===================== STATES =====================
enum GS {
  S_BOOT,S_HATCH,S_HOME,S_MENU,S_FEED,S_PLAY,S_CLEAN,
  S_SLEEP,S_MED,S_STATS,S_CHOOSER,S_MG_SEL,S_MG_REACT,
  S_MG_DODGE,S_MG_RHYTHM,S_EVOLVE,S_DEATH,S_SCREENSAVER,
  S_SOUNDTEST,S_PET,S_DREAM
};
GS state=S_BOOT, prevState=S_BOOT;

// ===================== PET =====================
int hunger=80,happiness=80,energy=100,hygiene=80,affection=50,health=100;
int petAge=0,evoStage=0,personality=0,mood=0;
bool isSick=0,isAsleep=0,isDead=0;
int poopCount=0,careScore=0,dayCount=0;
bool isNight=0;
int sickCounter=0;
int petType=0;   // 0=Mochi 1=Pip 2=Bean

// ===================== ANIMATION =====================
int petX=52,petY=16,animFrame=0;
bool eyesOpen=1;
int bounceY=0,breathOffset=0;

struct Particle { int x,y,vx,vy,life,type; bool active; };
#define MAX_P 8
Particle particles[MAX_P];

// ===================== MENU =====================
const char* menuItems[]={"Feed","Play","Clean","Sleep","Medicine","Games","Sounds"};
#define MENU_COUNT 7
int menuIdx=0;
int statsPage=0;
int chooserIdx=0;

// ===================== MINIGAME =====================
int mgScore=0,mgLives=3,mgState=0,mgTimer=0,mgTarget=0;
int mgPlayerX=0,mgCombo=0;
int mgObjX[5],mgObjY[5]; bool mgObjActive[5];
unsigned long mgReactStart=0; bool mgWaiting=0;

// ===================== MISC =====================
int bootFrame=0,hatchFrame=0,eatAnimFrame=0,cleanAnimFrame=0;
int evolveFrame=0,deathFrame=0,dreamFrame=0,soundTestIdx=0;
long ssX=0,ssY=0; int ssDx=1,ssDy=1;
bool eatSoundPlayed=0;

// ===================== SOUND =====================
struct SoundNote { int freq; int dur; };
#define MAX_SQ 16
SoundNote soundQueue[MAX_SQ];
int sqHead=0,sqTail=0;
unsigned long sqNoteEnd=0;
bool sqPlaying=0;
bool soundEnabled=1;

// ======================================================
// SPRITES  24x24, 3 bytes/row
// Three cute pets, each with 8 expression frames:
//  [0]idle1 [1]idle2 [2]blink [3]happy [4]sad [5]angry [6]sleep [7]egg
// ======================================================

// ---------- PET 0 : MOCHI (round kitten, triangle ears, huge eyes) ----------
const unsigned char PROGMEM mochi_idle1[] = {
  0x18,0x01,0x80,
  0x3C,0x03,0xC0,
  0x3E,0x07,0xC0,
  0x1F,0x87,0xE0,
  0x0F,0xFE,0xE0,
  0x1F,0xFF,0xFE,
  0x3F,0xFF,0xFF,
  0x7F,0xFF,0xFF,
  0x79,0xF9,0xFF,
  0x79,0xF9,0xFF,
  0x79,0xF9,0xFF,
  0x7F,0xFF,0xFF,
  0x7F,0xB7,0xFF,
  0x7B,0xFF,0x7F,
  0x7F,0xFF,0xFF,
  0x3F,0xFF,0xFE,
  0x1F,0xFF,0xFC,
  0x0F,0xFF,0xF8,
  0x07,0xFF,0xF0,
  0x03,0xFF,0xE0,
  0x01,0xFF,0xC0,
  0x03,0x80,0xE0,
  0x03,0x80,0xE0,
  0x07,0x80,0xF0
};

const unsigned char PROGMEM mochi_idle2[] = {
  0x3E,0x07,0xC0,
  0x1F,0x87,0xE0,
  0x0F,0xFE,0xE0,
  0x1F,0xFF,0xFE,
  0x3F,0xFF,0xFF,
  0x7F,0xFF,0xFF,
  0x79,0xF9,0xFF,
  0x79,0xF9,0xFF,
  0x79,0xF9,0xFF,
  0x7F,0xFF,0xFF,
  0x7F,0xB7,0xFF,
  0x7B,0xFF,0x7F,
  0x7F,0xFF,0xFF,
  0x3F,0xFF,0xFE,
  0x1F,0xFF,0xFC,
  0x0F,0xFF,0xF8,
  0x07,0xFF,0xF0,
  0x03,0xFF,0xE0,
  0x01,0xFF,0xC0,
  0x03,0x80,0xE0,
  0x03,0x80,0xE0,
  0x07,0x80,0xF0,
  0x07,0x80,0xF0,
  0x07,0x80,0xF0
};

const unsigned char PROGMEM mochi_blink[] = {
  0x18,0x01,0x80,
  0x3C,0x03,0xC0,
  0x3E,0x07,0xC0,
  0x1F,0x87,0xE0,
  0x0F,0xFE,0xE0,
  0x1F,0xFF,0xFE,
  0x3F,0xFF,0xFF,
  0x7F,0xFF,0xFF,
  0x7F,0xFF,0xFF,
  0x79,0xF9,0xFF,
  0x7F,0xFF,0xFF,
  0x7F,0xFF,0xFF,
  0x7F,0xB7,0xFF,
  0x7B,0xFF,0x7F,
  0x7F,0xFF,0xFF,
  0x3F,0xFF,0xFE,
  0x1F,0xFF,0xFC,
  0x0F,0xFF,0xF8,
  0x07,0xFF,0xF0,
  0x03,0xFF,0xE0,
  0x01,0xFF,0xC0,
  0x03,0x80,0xE0,
  0x03,0x80,0xE0,
  0x07,0x80,0xF0
};

const unsigned char PROGMEM mochi_happy[] = {
  0x18,0x01,0x80,
  0x3C,0x03,0xC0,
  0x3E,0x07,0xC0,
  0x1F,0x87,0xE0,
  0x0F,0xFE,0xE0,
  0x1F,0xFF,0xFE,
  0x3F,0xFF,0xFF,
  0x7F,0xFF,0xFF,
  0x7F,0xFF,0xFF,
  0x79,0xF9,0xFF,
  0x7F,0xFF,0xFF,
  0x7F,0xFF,0xFF,
  0x7D,0xFB,0xFF,
  0x7E,0x07,0xFF,
  0x7F,0xFF,0xFF,
  0x3F,0xFF,0xFE,
  0x1F,0xFF,0xFC,
  0x0F,0xFF,0xF8,
  0x07,0xFF,0xF0,
  0x03,0xFF,0xE0,
  0x01,0xFF,0xC0,
  0x03,0x80,0xE0,
  0x03,0x80,0xE0,
  0x07,0x80,0xF0
};

const unsigned char PROGMEM mochi_sad[] = {
  0x18,0x01,0x80,
  0x3C,0x03,0xC0,
  0x3E,0x07,0xC0,
  0x1F,0x87,0xE0,
  0x0F,0xFE,0xE0,
  0x1F,0xFF,0xFE,
  0x3F,0xFF,0xFF,
  0x7F,0xFF,0xFF,
  0x79,0xF9,0xFF,
  0x7B,0xFB,0xFF,
  0x79,0xF9,0xFF,
  0x7F,0xFF,0xFF,
  0x7F,0xB7,0xFF,
  0x7E,0x07,0xFF,
  0x7F,0xFF,0xFF,
  0x3F,0xFF,0xFE,
  0x1F,0xFF,0xFC,
  0x0F,0xFF,0xF8,
  0x07,0xFF,0xF0,
  0x03,0xFF,0xE0,
  0x01,0xFF,0xC0,
  0x03,0x80,0xE0,
  0x03,0x80,0xE0,
  0x07,0x80,0xF0
};

const unsigned char PROGMEM mochi_angry[] = {
  0x18,0x01,0x80,
  0x3C,0x03,0xC0,
  0x3E,0x07,0xC0,
  0x1F,0x87,0xE0,
  0x0F,0xFE,0xE0,
  0x1F,0xFF,0xFE,
  0x3F,0xFF,0xFF,
  0x77,0xFF,0xDF,
  0x79,0xF9,0xFF,
  0x79,0xF9,0xFF,
  0x79,0xF9,0xFF,
  0x7F,0xFF,0xFF,
  0x7F,0xB7,0xFF,
  0x7B,0xFF,0x7F,
  0x7F,0xFF,0xFF,
  0x3F,0xFF,0xFE,
  0x1F,0xFF,0xFC,
  0x0F,0xFF,0xF8,
  0x07,0xFF,0xF0,
  0x03,0xFF,0xE0,
  0x01,0xFF,0xC0,
  0x03,0x80,0xE0,
  0x03,0x80,0xE0,
  0x07,0x80,0xF0
};

const unsigned char PROGMEM mochi_sleep[] = {
  0x18,0x01,0x80,
  0x3C,0x03,0xC0,
  0x3E,0x07,0xC0,
  0x1F,0x87,0xE0,
  0x0F,0xFE,0xE0,
  0x1F,0xFF,0xFE,
  0x3F,0xFF,0xFF,
  0x7F,0xFF,0xFF,
  0x7F,0xFF,0xFF,
  0x79,0xF9,0xFF,
  0x7F,0xFF,0xFF,
  0x7F,0xFF,0xFF,
  0x7F,0xB7,0xFF,
  0x7B,0xFF,0x7F,
  0x7F,0xFF,0xFF,
  0x3F,0xFF,0xFE,
  0x1F,0xFF,0xFC,
  0x0F,0xFF,0xF8,
  0x07,0xFF,0xF0,
  0x03,0xFF,0xE0,
  0x01,0xFF,0xC0,
  0x03,0x80,0xE0,
  0x03,0x80,0xE0,
  0x07,0x80,0xF0
};

const unsigned char PROGMEM pip_idle1[] = {
  0x18,0x00,0x18,
  0x18,0x00,0x18,
  0x18,0x00,0x18,
  0x1C,0x00,0x38,
  0x1E,0x00,0x78,
  0x0F,0xFF,0x70,
  0x0F,0xFF,0xF0,
  0x0F,0xFF,0xF0,
  0x0C,0x78,0xD0,
  0x0C,0x78,0xD0,
  0x0C,0x78,0xD0,
  0x0C,0x78,0xD0,
  0x0F,0xFF,0xF0,
  0x0F,0xB7,0xE0,
  0x0F,0xFF,0xF0,
  0x07,0xFF,0xE0,
  0x07,0xFF,0xE0,
  0x03,0xFF,0xC0,
  0x03,0xFF,0xC0,
  0x01,0xFF,0x80,
  0x01,0xFF,0x80,
  0x03,0x81,0xC0,
  0x03,0x81,0xC0,
  0x07,0x81,0xE0
};

const unsigned char PROGMEM pip_idle2[] = {
  0x18,0x00,0x18,
  0x18,0x00,0x18,
  0x18,0x00,0x18,
  0x18,0x00,0x18,
  0x1C,0x00,0xF0,
  0x0F,0xFF,0x70,
  0x0F,0xFF,0xF0,
  0x0F,0xFF,0xF0,
  0x0C,0x78,0xD0,
  0x0C,0x78,0xD0,
  0x0C,0x78,0xD0,
  0x0C,0x78,0xD0,
  0x0F,0xFF,0xF0,
  0x0F,0xB7,0xE0,
  0x0F,0xFF,0xF0,
  0x07,0xFF,0xE0,
  0x07,0xFF,0xE0,
  0x03,0xFF,0xC0,
  0x03,0xFF,0xC0,
  0x01,0xFF,0x80,
  0x01,0xFF,0x80,
  0x03,0x81,0xC0,
  0x03,0x81,0xC0,
  0x07,0x81,0xE0
};

const unsigned char PROGMEM pip_blink[] = {
  0x18,0x00,0x18,
  0x18,0x00,0x18,
  0x18,0x00,0x18,
  0x1C,0x00,0x38,
  0x1E,0x00,0x78,
  0x0F,0xFF,0x70,
  0x0F,0xFF,0xF0,
  0x0F,0xFF,0xF0,
  0x0F,0xFF,0xF0,
  0x0C,0x78,0xD0,
  0x0F,0xFF,0xF0,
  0x0F,0xFF,0xF0,
  0x0F,0xFF,0xF0,
  0x0F,0xB7,0xE0,
  0x0F,0xFF,0xF0,
  0x07,0xFF,0xE0,
  0x07,0xFF,0xE0,
  0x03,0xFF,0xC0,
  0x03,0xFF,0xC0,
  0x01,0xFF,0x80,
  0x01,0xFF,0x80,
  0x03,0x81,0xC0,
  0x03,0x81,0xC0,
  0x07,0x81,0xE0
};

const unsigned char PROGMEM pip_happy[] = {
  0x18,0x00,0x18,
  0x18,0x00,0x18,
  0x18,0x00,0x18,
  0x1C,0x00,0x38,
  0x1E,0x00,0x78,
  0x0F,0xFF,0x70,
  0x0F,0xFF,0xF0,
  0x0F,0xFF,0xF0,
  0x0F,0xFF,0xF0,
  0x0C,0x78,0xD0,
  0x0C,0x78,0xD0,
  0x0C,0x78,0xD0,
  0x0F,0x03,0xE0,
  0x0F,0xB7,0xE0,
  0x0F,0xFF,0xF0,
  0x07,0xFF,0xE0,
  0x07,0xFF,0xE0,
  0x03,0xFF,0xC0,
  0x03,0xFF,0xC0,
  0x01,0xFF,0x80,
  0x01,0xFF,0x80,
  0x03,0x81,0xC0,
  0x03,0x81,0xC0,
  0x07,0x81,0xE0
};

const unsigned char PROGMEM pip_sad[] = {
  0x18,0x00,0x18,
  0x18,0x00,0x18,
  0x18,0x00,0x18,
  0x1C,0x00,0x38,
  0x1E,0x00,0x78,
  0x0F,0xFF,0x70,
  0x0F,0xFF,0xF0,
  0x0F,0xFF,0xF0,
  0x0C,0x78,0xD0,
  0x0D,0xFC,0xD0,
  0x0C,0x78,0xD0,
  0x0C,0x78,0xD0,
  0x0F,0xFF,0xF0,
  0x0F,0x87,0xE0,
  0x0F,0xFF,0xF0,
  0x07,0xFF,0xE0,
  0x07,0xFF,0xE0,
  0x03,0xFF,0xC0,
  0x03,0xFF,0xC0,
  0x01,0xFF,0x80,
  0x01,0xFF,0x80,
  0x03,0x81,0xC0,
  0x03,0x81,0xC0,
  0x07,0x81,0xE0
};

const unsigned char PROGMEM pip_angry[] = {
  0x18,0x00,0x18,
  0x18,0x00,0x18,
  0x18,0x00,0x18,
  0x1C,0x00,0x38,
  0x1E,0x00,0x78,
  0x0F,0xFF,0x70,
  0x0F,0xFF,0xF0,
  0x0D,0xFF,0xA0,
  0x0C,0x78,0xD0,
  0x0C,0x78,0xD0,
  0x0C,0x78,0xD0,
  0x0C,0x78,0xD0,
  0x0F,0xFF,0xF0,
  0x0F,0xB7,0xE0,
  0x0F,0xFF,0xF0,
  0x07,0xFF,0xE0,
  0x07,0xFF,0xE0,
  0x03,0xFF,0xC0,
  0x03,0xFF,0xC0,
  0x01,0xFF,0x80,
  0x01,0xFF,0x80,
  0x03,0x81,0xC0,
  0x03,0x81,0xC0,
  0x07,0x81,0xE0
};

const unsigned char PROGMEM pip_sleep[] = {
  0x18,0x00,0x18,
  0x18,0x00,0x18,
  0x18,0x00,0x18,
  0x1C,0x00,0x38,
  0x1E,0x00,0x78,
  0x0F,0xFF,0x70,
  0x0F,0xFF,0xF0,
  0x0F,0xFF,0xF0,
  0x0F,0xFF,0xF0,
  0x0C,0x78,0xD0,
  0x0F,0xFF,0xF0,
  0x0C,0x78,0xD0,
  0x0F,0xFF,0xF0,
  0x0F,0xB7,0xE0,
  0x0F,0xFF,0xF0,
  0x07,0xFF,0xE0,
  0x07,0xFF,0xE0,
  0x03,0xFF,0xC0,
  0x03,0xFF,0xC0,
  0x01,0xFF,0x80,
  0x01,0xFF,0x80,
  0x03,0x81,0xC0,
  0x03,0x81,0xC0,
  0x07,0x81,0xE0
};

const unsigned char PROGMEM bean_idle1[] = {
  0x00,0x00,0x00,
  0x01,0xFF,0x80,
  0x0F,0xFF,0xF0,
  0x3F,0xFF,0xFC,
  0x7F,0xFF,0xFE,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xE7,0xFF,0x3F,
  0xE7,0xFF,0x3F,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xCF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0x7F,0xFF,0xFE,
  0x7F,0xFF,0xFE,
  0x3F,0xFF,0xFC,
  0x1F,0xFF,0xF8,
  0x0F,0xFF,0xF0,
  0x1E,0x01,0xE0,
  0x1E,0x01,0xE0
};

const unsigned char PROGMEM bean_idle2[] = {
  0x00,0x00,0x00,
  0x01,0xFF,0x80,
  0x0F,0xFF,0xF0,
  0x7F,0xFF,0xFE,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xE7,0xFF,0x3F,
  0xE7,0xFF,0x3F,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xCF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0x7F,0xFF,0xFE,
  0x7F,0xFF,0xFE,
  0x3F,0xFF,0xFC,
  0x1F,0xFF,0xF8,
  0x0F,0xFF,0xF0,
  0x1E,0x01,0xE0,
  0x1E,0x01,0xE0
};

const unsigned char PROGMEM bean_blink[] = {
  0x00,0x00,0x00,
  0x01,0xFF,0x80,
  0x0F,0xFF,0xF0,
  0x3F,0xFF,0xFC,
  0x7F,0xFF,0xFE,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xE7,0xFF,0x3F,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xCF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0x7F,0xFF,0xFE,
  0x7F,0xFF,0xFE,
  0x3F,0xFF,0xFC,
  0x1F,0xFF,0xF8,
  0x0F,0xFF,0xF0,
  0x1E,0x01,0xE0,
  0x1E,0x01,0xE0
};

const unsigned char PROGMEM bean_happy[] = {
  0x00,0x00,0x00,
  0x01,0xFF,0x80,
  0x0F,0xFF,0xF0,
  0x3F,0xFF,0xFC,
  0x7F,0xFF,0xFE,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xE7,0xFF,0x3F,
  0xE7,0xFF,0x3F,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0x3C,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0x7F,0xFF,0xFE,
  0x7F,0xFF,0xFE,
  0x3F,0xFF,0xFC,
  0x1F,0xFF,0xF8,
  0x0F,0xFF,0xF0,
  0x1E,0x01,0xE0,
  0x1E,0x01,0xE0
};

const unsigned char PROGMEM bean_sad[] = {
  0x00,0x00,0x00,
  0x01,0xFF,0x80,
  0x0F,0xFF,0xF0,
  0x3F,0xFF,0xFC,
  0x7F,0xFF,0xFE,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xEF,0x7E,0xBF,
  0xE7,0xFF,0x3F,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0x87,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0x7F,0xFF,0xFE,
  0x7F,0xFF,0xFE,
  0x3F,0xFF,0xFC,
  0x1F,0xFF,0xF8,
  0x0F,0xFF,0xF0,
  0x1E,0x01,0xE0,
  0x1E,0x01,0xE0
};

const unsigned char PROGMEM bean_angry[] = {
  0x00,0x00,0x00,
  0x01,0xFF,0x80,
  0x0F,0xFF,0xF0,
  0x3F,0xFF,0xFC,
  0x7F,0xFF,0xFE,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xDF,0xFF,0xFE,
  0xE7,0xFF,0x3F,
  0xE7,0xFF,0x3F,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xCF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0x7F,0xFF,0xFE,
  0x7F,0xFF,0xFE,
  0x3F,0xFF,0xFC,
  0x1F,0xFF,0xF8,
  0x0F,0xFF,0xF0,
  0x1E,0x01,0xE0,
  0x1E,0x01,0xE0
};

const unsigned char PROGMEM bean_sleep[] = {
  0x00,0x00,0x00,
  0x01,0xFF,0x80,
  0x0F,0xFF,0xF0,
  0x3F,0xFF,0xFC,
  0x7F,0xFF,0xFE,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xE7,0xFF,0x3F,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xCF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,
  0x7F,0xFF,0xFE,
  0x7F,0xFF,0xFE,
  0x3F,0xFF,0xFC,
  0x1F,0xFF,0xF8,
  0x0F,0xFF,0xF0,
  0x1E,0x01,0xE0,
  0x1E,0x01,0xE0
};


// Egg shared 24x24
const unsigned char PROGMEM spr_egg1[] = {
  0x00,0x3C,0x00,0x00,0x7E,0x00,0x00,0xFF,0x00,0x01,0xFF,0x80,
  0x03,0xFF,0xC0,0x03,0xFF,0xC0,0x07,0xFF,0xE0,0x07,0xFF,0xE0,
  0x07,0xFF,0xE0,0x0F,0xFF,0xF0,0x0F,0xFF,0xF0,0x0F,0xFF,0xF0,
  0x0F,0xFF,0xF0,0x0F,0xFF,0xF0,0x0F,0xFF,0xF0,0x0F,0xFF,0xF0,
  0x07,0xFF,0xE0,0x07,0xFF,0xE0,0x07,0xFF,0xE0,0x03,0xFF,0xC0,
  0x03,0xFF,0xC0,0x01,0xFF,0x80,0x00,0xFF,0x00,0x00,0x7E,0x00
};
const unsigned char PROGMEM spr_egg2[] = {
  0x00,0x3C,0x00,0x00,0x7E,0x00,0x00,0xFF,0x00,0x01,0xFF,0x80,
  0x03,0xFF,0xC0,0x03,0xFF,0xC0,0x07,0xF3,0xE0,0x07,0xE7,0xE0,
  0x07,0xCF,0xE0,0x0F,0x9F,0xF0,0x0F,0xFF,0xF0,0x0F,0xFF,0xF0,
  0x0F,0xFF,0xF0,0x0F,0xFF,0xF0,0x0F,0xFF,0xF0,0x0F,0xFF,0xF0,
  0x07,0xFF,0xE0,0x07,0xFF,0xE0,0x07,0xFF,0xE0,0x03,0xFF,0xC0,
  0x03,0xFF,0xC0,0x01,0xFF,0x80,0x00,0xFF,0x00,0x00,0x7E,0x00
};

// Sprite lookup table [petType][expression]
const unsigned char* const PET_SPRITES[3][7] = {
  { mochi_idle1, mochi_idle2, mochi_blink, mochi_happy, mochi_sad, mochi_angry, mochi_sleep },
  { pip_idle1,   pip_idle2,   pip_blink,   pip_happy,   pip_sad,   pip_angry,   pip_sleep   },
  { bean_idle1,  bean_idle2,  bean_blink,  bean_happy,  bean_sad,  bean_angry,  bean_sleep  }
};
const char* PET_NAMES[3] = {"Mochi","Pip","Bean"};

// 8x8 icons
const unsigned char PROGMEM ico_poop[] = {0x10,0x38,0x14,0x7E,0xFF,0xFF,0x7E,0x3C};
const unsigned char PROGMEM ico_food[] = {0x18,0x3C,0x7E,0xFF,0xFF,0x7E,0x3C,0x18};
const unsigned char PROGMEM ico_heart[]= {0x6C,0xFE,0xFE,0xFE,0x7C,0x38,0x10,0x00};
const unsigned char PROGMEM ico_star[] = {0x10,0x10,0x54,0x38,0x7C,0x38,0x54,0x10};
const unsigned char PROGMEM ico_skull[]= {0x3C,0x7E,0xDB,0xFF,0xFF,0x66,0x7E,0x3C};

// ======================================================
// SOUND ENGINE (gentle, 300-1200Hz)
// ======================================================
void sqClear(){sqHead=sqTail=0;sqPlaying=0;noTone(BUZZ_PIN);}
void sqPush(int f,int d){if(!soundEnabled)return;int n=(sqTail+1)%MAX_SQ;if(n!=sqHead){soundQueue[sqTail]={f,d};sqTail=n;}}
void sqUpdate(){
  if(sqPlaying&&millis()>=sqNoteEnd){noTone(BUZZ_PIN);sqPlaying=0;}
  if(!sqPlaying&&sqHead!=sqTail){SoundNote n=soundQueue[sqHead];sqHead=(sqHead+1)%MAX_SQ;
    if(n.freq>0)tone(BUZZ_PIN,n.freq,n.dur);sqNoteEnd=millis()+n.dur;sqPlaying=1;}
}
void sfxChirp(){sqPush(800,30);sqPush(1000,30);sqPush(1200,40);}
void sfxSad(){sqPush(500,80);sqPush(400,80);sqPush(300,150);}
void sfxHappy(){sqPush(600,40);sqPush(800,40);sqPush(1000,40);sqPush(1200,60);}
void sfxSelect(){sqPush(800,20);sqPush(1000,20);}
void sfxBack(){sqPush(600,20);sqPush(400,20);}
void sfxError(){sqPush(250,60);sqPush(200,80);}
void sfxPet(){sqPush(900,30);sqPush(1100,40);}
void sfxPoop(){sqPush(250,40);sqPush(200,40);sqPush(150,60);}
void sfxSnore(){sqPush(150,200);sqPush(180,150);sqPush(0,400);}
void sfxDream(){sqPush(600,120);sqPush(500,120);sqPush(600,120);sqPush(700,150);}
void sfxAlarm(){sqPush(1000,50);sqPush(0,30);sqPush(1000,50);}
void sfxGameWin(){sqPush(600,40);sqPush(800,40);sqPush(1000,40);sqPush(1200,80);}
void sfxGameLose(){sqPush(400,60);sqPush(300,60);sqPush(200,80);}
void sfxEat(){sqPush(400,30);sqPush(0,40);sqPush(500,30);sqPush(0,40);sqPush(600,40);}
void sfxClean(){sqPush(500,25);sqPush(700,25);sqPush(500,25);sqPush(700,25);sqPush(900,40);}
void sfxMedicine(){sqPush(400,60);sqPush(500,60);sqPush(600,60);sqPush(700,80);}
void sfxSleep(){sqPush(500,150);sqPush(400,150);sqPush(300,200);}
void sfxWakeUp(){sqPush(300,50);sqPush(400,50);sqPush(500,50);sqPush(700,80);}
void sfxEvolution(){for(int i=0;i<6;i++)sqPush(400+i*100,60);sqPush(1200,150);}
void sfxDeath(){sqPush(600,100);sqPush(500,100);sqPush(400,100);sqPush(300,150);sqPush(200,300);}
void sfxBoot(){sqPush(400,60);sqPush(0,30);sqPush(600,60);sqPush(0,30);sqPush(800,60);sqPush(0,30);sqPush(1200,150);}
void sfxHatch(){sqPush(400,60);sqPush(500,60);sqPush(600,60);sqPush(700,60);sqPush(900,60);sqPush(1100,120);}

// ======================================================
// PARTICLES
// ======================================================
void spawnP(int x,int y,int t){
  for(int i=0;i<MAX_P;i++)if(!particles[i].active){
    particles[i]={x,y,0,0,20+(int)random(15),t,true};
    switch(t){
      case 0:particles[i].vx=random(-1,2);particles[i].vy=-1;break;
      case 1:particles[i].vx=random(-2,3);particles[i].vy=random(-2,1);break;
      case 2:particles[i].vx=1;particles[i].vy=-1;break;
      case 3:particles[i].vx=random(0,2);particles[i].vy=1;break;
      case 4:particles[i].vx=1;particles[i].vy=-1;break;
      case 5:particles[i].vx=0;particles[i].vy=1;break;
      case 6:particles[i].vx=random(-2,3);particles[i].vy=random(-2,3);particles[i].life=10;break;
      case 7:particles[i].vx=random(-1,2);particles[i].vy=1;break;
    }return;
  }
}
void updateP(){for(int i=0;i<MAX_P;i++)if(particles[i].active){particles[i].life--;
  if(particles[i].life<=0){particles[i].active=0;continue;}
  if(ms%3==0){particles[i].x+=particles[i].vx;particles[i].y+=particles[i].vy;}}}
void drawP(){for(int i=0;i<MAX_P;i++){if(!particles[i].active)continue;
  int px=particles[i].x,py=particles[i].y;
  if(px<0||px>SW-4||py<0||py>SH-4){particles[i].active=0;continue;}
  switch(particles[i].type){
    case 0:oled.drawBitmap(px,py,ico_heart,8,8,1);break;
    case 1:oled.drawBitmap(px,py,ico_star,8,8,1);break;
    case 2:oled.setTextSize(1);oled.setCursor(px,py);oled.print((char)14);break;
    case 3:oled.drawLine(px,py,px,py+3,1);oled.drawPixel(px,py+4,1);break;
    case 4:oled.setTextSize(1);oled.setCursor(px,py);oled.print("Z");break;
    case 5:oled.drawLine(px,py,px,py+2,1);break;
    case 6:oled.drawPixel(px,py,1);oled.drawPixel(px+1,py+1,1);break;
    case 7:oled.fillRect(px,py,2,2,1);break;
  }}}

// ======================================================
// INPUT (debounced + BOTH detection)
// ======================================================
void readInputs(){
  static unsigned long b1lc=0,b2lc=0;static bool b1r=0,b2r=0;
  btn1Prev=btn1;btn2Prev=btn2;tchPrev=tch;bothPrev=(btn1&&btn2);
  bool b1=digitalRead(BTN1_PIN)==LOW;
  bool b2=digitalRead(BTN2_PIN)==LOW;
  tch=digitalRead(TOUCH_PIN)==HIGH;
  if(b1!=b1r){b1r=b1;b1lc=ms;}if(ms-b1lc>=30)btn1=b1r;
  if(b2!=b2r){b2r=b2;b2lc=ms;}if(ms-b2lc>=30)btn2=b2r;
  btn1Rose=btn1&&!btn1Prev;btn2Rose=btn2&&!btn2Prev;tchRose=tch&&!tchPrev;
  bool bothNow=btn1&&btn2;
  bothRose=bothNow&&!bothPrev;
  if(btn1Rose)btn1DownTime=ms;
  if(btn2Rose)btn2DownTime=ms;
  if(tchRose){if(ms-touchBurstStart>1000){touchCount=0;touchBurstStart=ms;}touchCount++;}
  if(btn1Rose)Serial.println("B1");
  if(btn2Rose)Serial.println("B2");
  if(tchRose)Serial.println("T");
  if(bothRose)Serial.println("BOTH");
}

// ======================================================
// STATS LOGIC
// ======================================================
void clampStats(){
  hunger=constrain(hunger,0,100);happiness=constrain(happiness,0,100);
  energy=constrain(energy,0,100);hygiene=constrain(hygiene,0,100);
  affection=constrain(affection,0,100);health=constrain(health,0,100);
}
void updateMood(){
  if(isSick){mood=4;return;}if(isAsleep){mood=6;return;}
  if(happiness>80&&hunger>60)mood=1;
  else if(happiness<20)mood=2;
  else if(hunger<15&&happiness<30)mood=3;
  else if(happiness>70&&affection>70)mood=5;
  else mood=0;
}
void triggerEvo(int ns){evoStage=ns;state=S_EVOLVE;evolveAnimStart=ms;evolveFrame=0;sfxEvolution();}
void decayStats(){
  if(ms-lastStatDecay<5000)return;lastStatDecay=ms;
  if(isAsleep){energy=min(energy+3,100);hunger-=1;}
  else{hunger-=2;happiness-=1;energy-=1;hygiene-=1;}
  if(hunger<20)health-=2;if(hygiene<20)health-=1;if(happiness<20)health-=1;
  if(hunger>80&&happiness>60)health=min(health+1,100);
  if(!isAsleep&&random(100)<8){poopCount=min(poopCount+1,3);hygiene-=10;sfxPoop();}
  if(poopCount>0)hygiene-=poopCount*2;
  if(!isSick&&health<30&&random(100)<20){isSick=1;sickCounter=0;sfxAlarm();}
  if(isSick){sickCounter++;health-=2;happiness-=3;
    if(sickCounter>30&&health<=0){isDead=1;state=S_DEATH;deathAnimStart=ms;sfxDeath();}}
  if(ms-lastDayCheck>120000){lastDayCheck=ms;dayCount++;petAge++;
    careScore+=(hunger+happiness+energy+hygiene+health)/50;isNight=!isNight;
    if(evoStage==0&&petAge>=1)triggerEvo(1);
    else if(evoStage==1&&petAge>=4)triggerEvo(2);
    else if(evoStage==2&&petAge>=8){triggerEvo(3);
      if(careScore>100)personality=1;else if(careScore>60)personality=0;
      else if(careScore>30)personality=2;else personality=3;}}
  if(energy<=5&&!isAsleep&&state==S_HOME){isAsleep=1;sleepStartTime=ms;state=S_SLEEP;sfxSleep();}
  updateMood();clampStats();
}

// ======================================================
// DRAW PET (uses petType + mood + animFrame)
// ======================================================
void drawPetAt(int x,int y,int type,int expr){
  const unsigned char* spr=PET_SPRITES[type][expr];
  oled.drawBitmap(x,y,spr,24,24,1);
}
void drawPet(int x,int y){
  int expr;
  if(evoStage==0){
    oled.drawBitmap(x,y+bounceY+breathOffset,(animFrame%2==0)?spr_egg1:spr_egg2,24,24,1);
    return;
  }
  if(mood==6) expr=6;
  else if(mood==1||mood==5) expr=3;
  else if(mood==2) expr=4;
  else if(mood==3) expr=5;
  else if(!eyesOpen) expr=2;
  else expr=(animFrame%2==0)?0:1;
  int dx=x,dy=y+bounceY+breathOffset;
  if(isSick&&ms-lastSickShake>100){lastSickShake=ms;dx+=random(-2,3);}
  drawPetAt(dx,dy,petType,expr);
}

void drawMoodIcon(){
  int ix=110,iy=2;oled.setTextSize(1);
  if(mood==1)oled.drawBitmap(ix,iy,ico_heart,8,8,1);
  else if(mood==2){oled.setCursor(ix,iy);oled.print(":(");}
  else if(mood==3){oled.setCursor(ix,iy);oled.print(">:");}
  else if(mood==4)oled.drawBitmap(ix,iy,ico_skull,8,8,1);
  else if(mood==5)oled.drawBitmap(ix,iy,ico_star,8,8,1);
  else if(mood==6){oled.setCursor(ix,iy);oled.print("Zz");}
  if(isSick){oled.setCursor(0,2);oled.print("SICK!");}
  if(isNight){oled.fillCircle(120,5,4,1);oled.fillCircle(122,3,3,0);}
}
void drawPoops(){for(int i=0;i<poopCount;i++)oled.drawBitmap(95+i*11,44,ico_poop,8,8,1);}
void drawGround(){for(int x=0;x<SW;x+=3)oled.drawPixel(x,55,1);}

// ======================================================
// ANIMATIONS
// ======================================================
void updateAnims(){
  if(ms-lastBounce>400){lastBounce=ms;bounceY=(bounceY==0)?-1:0;}
  if(ms-lastBreath>600){lastBreath=ms;breathOffset=(breathOffset==0)?1:0;}
  if(ms-lastBlink>3000+random(2000)){lastBlink=ms;eyesOpen=0;}
  if(!eyesOpen&&ms-lastBlink>150)eyesOpen=1;
  if(ms-lastIdleAnim>500){lastIdleAnim=ms;animFrame++;}
  if(ms-idleTimer>10000&&state==S_HOME){idleTimer=ms;if(random(4)==2)petX=30+random(50);}
  updateP();
  if(ms-lastParticle>2500){lastParticle=ms;
    if(mood==1)spawnP(petX+12,petY-4,0);
    if(mood==2)spawnP(petX+6,petY+6,5);
    if(mood==3)spawnP(petX+20,petY,3);
    if(mood==5)spawnP(petX+12,petY-4,1);
    if(mood==6)spawnP(petX+20,petY-6,4);}
  if(isAsleep&&ms-lastZzz>4000){lastZzz=ms;sfxSnore();}
  if(state==S_HOME&&ms-idleTimer>60000){state=S_SCREENSAVER;ssX=petX;ssY=petY;}
}

// ======================================================
// BOOT
// ======================================================
void updateBoot(){
  if(bootAnimStart==0){bootAnimStart=ms;sfxBoot();}
  oled.clearDisplay();int el=ms-bootAnimStart;
  if(el<1500){
    int ly=((el<800)?(int)el:800);ly=map(ly,0,800,-20,10);
    oled.setTextSize(2);oled.setCursor(18,ly);oled.print("TRAX TOM");
    if(el>600){oled.setTextSize(1);oled.setCursor(20,35);oled.print("~ virtual pet ~");}
    if(el>1000)for(int i=0;i<5;i++)oled.drawPixel(random(SW),random(SH),1);
  } else if(el<2500){oled.fillRect(0,0,map(el,1500,2500,0,SW),SH,1);}
  else{state=S_HATCH;hatchFrame=0;}
  oled.display();
}

// ======================================================
// HATCH
// ======================================================
void updateHatch(){
  static unsigned long lt=0;oled.clearDisplay();
  if(ms-lt>500){lt=ms;hatchFrame++;}
  if(hatchFrame<6){int w=(hatchFrame%2==0)?-2:2;
    oled.drawBitmap(52+w,18,spr_egg1,24,24,1);
    oled.setTextSize(1);oled.setCursor(30,52);oled.print("Hatching...");
    if(hatchFrame==3)sfxHatch();
  } else if(hatchFrame<10){int w=(hatchFrame%2==0)?-1:1;
    oled.drawBitmap(52+w,18,spr_egg2,24,24,1);
    spawnP(52+random(24),18+random(24),6);
    oled.setTextSize(1);oled.setCursor(35,52);oled.print("Almost...");
  } else if(hatchFrame<14){
    drawPetAt(52,18,petType,3);
    for(int i=0;i<3;i++)spawnP(52+random(24),14,1);
    oled.setTextSize(1);oled.setCursor(25,52);oled.print("It's alive!");
  } else{evoStage=1;petBornTime=ms;petX=52;petY=16;state=S_HOME;idleTimer=ms;}
  drawP();oled.display();
}

// ======================================================
// HOME
//  B1=Stats  B2=Chooser  BOTH=Pet  Touch=Menu
// ======================================================
void updateHome(){
  oled.clearDisplay();
  drawMoodIcon();drawGround();drawPet(petX,petY);drawPoops();drawP();
  oled.setTextSize(1);
  oled.setCursor(0,57);oled.print("B1:Stat B2:Pets T:Pet");

  if(bothRose){ // open menu with both buttons
    state=S_MENU;menuIdx=0;sfxSelect();
  } else {
    if(btn1Rose){state=S_STATS;statsPage=0;sfxSelect();}
    if(btn2Rose){state=S_CHOOSER;chooserIdx=petType;sfxSelect();}
    if(tchRose){state=S_PET;touchStartTime=ms;sfxPet();}
  }
  if(isAsleep&&(btn1Rose||btn2Rose)){isAsleep=0;sfxWakeUp();state=S_HOME;}
  oled.display();
}

// ======================================================
// STATS  full words, two pages, uncrowded
// ======================================================
void updateStats(){
  oled.clearDisplay();oled.setTextSize(1);oled.setTextColor(1);
  if(statsPage==0){
    oled.setCursor(34,0);oled.print("STATUS 1/2");
    const char* lb[]={"Hunger","Happiness","Energy"};
    int v[]={hunger,happiness,energy};
    for(int i=0;i<3;i++){
      int y=12+i*16;
      oled.setTextColor(1);
      oled.setCursor(0,y);oled.print(lb[i]);
      oled.setCursor(96,y);oled.print(v[i]);oled.print("%");
      oled.drawRect(0,y+9,110,5,1);
      oled.fillRect(1,y+10,map(v[i],0,100,0,108),3,1);
    }
    oled.setCursor(0,57);oled.print("B1:Exit  B2:Next page");
  } else {
    oled.setCursor(34,0);oled.print("STATUS 2/2");
    const char* lb[]={"Hygiene","Affection","Health"};
    int v[]={hygiene,affection,health};
    for(int i=0;i<3;i++){
      int y=12+i*16;
      oled.setTextColor(1);
      oled.setCursor(0,y);oled.print(lb[i]);
      oled.setCursor(96,y);oled.print(v[i]);oled.print("%");
      oled.drawRect(0,y+9,110,5,1);
      oled.fillRect(1,y+10,map(v[i],0,100,0,108),3,1);
    }
    oled.setCursor(0,57);oled.print("B1:Exit  B2:Next page");
  }
  if(btn1Rose){state=S_HOME;idleTimer=ms;sfxBack();}   // B1 exits
  if(btn2Rose){statsPage=1-statsPage;sfxSelect();}      // B2 flips page
  if(tchRose){state=S_HOME;idleTimer=ms;sfxBack();}
  oled.display();
}

// ======================================================
// PET CHOOSER (opened by B2/GPIO3)
//  B1=Prev  B2=Next  Touch=Equip  BOTH=Exit
// ======================================================
void updateChooser(){
  oled.clearDisplay();oled.setTextSize(1);
  oled.setCursor(28,0);oled.print("CHOOSE PET");

  // big preview of currently-highlighted pet
  drawPetAt(52,14,chooserIdx,(animFrame%2==0)?0:3);

  // name centered
  oled.setTextSize(1);
  int nameLen=strlen(PET_NAMES[chooserIdx]);
  oled.setCursor(64-(nameLen*3),44);
  oled.print(PET_NAMES[chooserIdx]);

  // dots showing which of 3
  for(int i=0;i<3;i++){
    int dx=56+i*8;
    if(i==chooserIdx)oled.fillCircle(dx,52,2,1);
    else oled.drawCircle(dx,52,2,1);
  }

  // equipped marker
  if(chooserIdx==petType){oled.setCursor(0,44);oled.print("[equipped]");}

  oled.setCursor(0,57);oled.print("B1:< B2:> T:Pick Both:X");

  if(bothRose){state=S_HOME;idleTimer=ms;sfxBack();}
  else{
    if(btn1Rose){chooserIdx=(chooserIdx+2)%3;sfxSelect();}  // prev
    if(btn2Rose){chooserIdx=(chooserIdx+1)%3;sfxSelect();}  // next
    if(tchRose){petType=chooserIdx;sfxHappy();}             // equip
  }
  oled.display();
}

// ======================================================
// MENU  B1=Left B2=Right Touch=Select BOTH=Exit
// ======================================================
void updateMenu(){
  oled.clearDisplay();oled.setTextSize(1);
  oled.setCursor(38,0);oled.print("= MENU =");
  for(int i=0;i<MENU_COUNT;i++){
    int c=i%2,r=i/2,x=c*64+4,y=12+r*13;
    if(i==menuIdx){oled.fillRoundRect(x-2,y-1,62,12,2,1);oled.setTextColor(0);}
    else oled.setTextColor(1);
    oled.setCursor(x+2,y+1);oled.print(menuItems[i]);oled.setTextColor(1);
  }
  oled.setCursor(2,57);oled.print("B1:< B2:> T:OK Both:X");
  if(bothRose){state=S_HOME;idleTimer=ms;sfxBack();}
  else{
    if(btn1Rose){menuIdx=(menuIdx+MENU_COUNT-1)%MENU_COUNT;sfxSelect();} // left
    if(btn2Rose){menuIdx=(menuIdx+1)%MENU_COUNT;sfxSelect();}            // right
    if(tchRose){
      sfxSelect();
      switch(menuIdx){
        case 0:state=S_FEED;eatAnimFrame=0;eatSoundPlayed=0;break;
        case 1:state=S_PLAY;break;
        case 2:state=S_CLEAN;cleanAnimFrame=0;break;
        case 3:if(!isAsleep){isAsleep=1;sleepStartTime=ms;state=S_SLEEP;sfxSleep();}else state=S_HOME;break;
        case 4:state=S_MED;break;
        case 5:state=S_MG_SEL;break;
        case 6:state=S_SOUNDTEST;soundTestIdx=0;break;
      }
    }
  }
  oled.display();
}

// ======================================================
// FEEDING (sound once)
// ======================================================
void updateFeeding(){
  static unsigned long lt=0;oled.clearDisplay();
  if(ms-lt>300){lt=ms;eatAnimFrame++;}drawGround();
  if(eatAnimFrame<8){int fx=10+eatAnimFrame*6;
    oled.drawBitmap(fx,28,ico_food,8,8,1);drawPet(petX,petY);
    if(eatAnimFrame>4)spawnP(petX+random(12),petY+18,7);
  } else if(eatAnimFrame<14){
    drawPet(petX,petY+((eatAnimFrame%2==0)?-2:2));
    if(!eatSoundPlayed){sfxEat();eatSoundPlayed=1;}
    spawnP(petX+random(24),petY+18,7);
    oled.setTextSize(1);oled.setCursor(petX-5,petY-8);oled.print("nom!");
  } else{hunger=min(hunger+25,100);happiness=min(happiness+5,100);
    hygiene-=5;careScore+=2;clampStats();state=S_HOME;idleTimer=ms;sfxHappy();}
  drawP();oled.display();
}

// ======================================================
// PLAYING
// ======================================================
void updatePlaying(){
  static int pf=0;static unsigned long lt=0;
  if(prevState!=S_PLAY)pf=0;
  oled.clearDisplay();drawGround();
  if(ms-lt>250){lt=ms;pf++;}
  if(pf<16){int dx=(pf%4<2)?-3:3,dy=(pf%2==0)?-4:0;
    drawPet(petX+dx,petY+dy);
    if(pf%3==0){spawnP(petX+random(24),petY-4,2);spawnP(petX+random(24),petY-4,1);}
    if(pf%4==0)sqPush(500+random(400),60);
    oled.setTextSize(1);oled.setCursor(30,5);oled.print("Playing!");
  } else{happiness=min(happiness+20,100);energy=max(energy-15,0);
    hunger-=10;careScore+=2;clampStats();state=S_HOME;idleTimer=ms;sfxHappy();}
  drawP();oled.display();
}

// ======================================================
// CLEANING
// ======================================================
void updateCleaning(){
  static unsigned long lt=0;oled.clearDisplay();
  if(ms-lt>200){lt=ms;cleanAnimFrame++;}drawGround();
  if(cleanAnimFrame<10){drawPet(petX,petY);
    if(cleanAnimFrame%2==0)spawnP(petX+random(28),petY+random(24),6);
    oled.drawFastVLine(cleanAnimFrame*13,10,48,1);
    oled.setTextSize(1);oled.setCursor(25,5);oled.print("Scrubbing...");
    if(cleanAnimFrame==1)sfxClean();
  } else{hygiene=min(hygiene+30,100);poopCount=0;
    happiness=min(happiness+5,100);careScore+=2;clampStats();state=S_HOME;idleTimer=ms;sfxHappy();}
  drawP();oled.display();
}

// ======================================================
// SLEEPING
// ======================================================
void updateSleeping(){
  oled.clearDisplay();drawGround();drawPet(petX,petY+4);
  if(ms-lastZzz>1500){lastZzz=ms;spawnP(petX+20,petY-2,4);}
  oled.fillCircle(110,10,6,1);oled.fillCircle(113,8,5,0);
  oled.drawPixel(20,8,1);oled.drawPixel(45,5,1);oled.drawPixel(80,12,1);
  oled.setTextSize(1);oled.setCursor(30,56);oled.print("Sleeping...");
  if(energy>=95){isAsleep=0;state=S_HOME;idleTimer=ms;sfxWakeUp();}
  if(tchRose){isAsleep=0;affection=min(affection+5,100);state=S_HOME;idleTimer=ms;sfxWakeUp();}
  if(btn1Rose||btn2Rose){isAsleep=0;happiness-=10;state=S_HOME;idleTimer=ms;sfxWakeUp();}
  if(ms-sleepStartTime>10000&&random(500)<2){state=S_DREAM;dreamFrame=0;sfxDream();}
  drawP();oled.display();
}

// ======================================================
// DREAM
// ======================================================
void updateDream(){
  static unsigned long lt=0;oled.clearDisplay();
  if(ms-lt>500){lt=ms;dreamFrame++;}
  oled.drawRoundRect(20,5,88,40,8,1);
  oled.fillCircle(30,48,3,1);oled.fillCircle(25,53,2,1);
  oled.setTextSize(1);oled.setCursor(30,12);oled.print("Dreaming of");
  oled.setCursor(35,25);
  switch((dayCount+petAge)%4){
    case 0:oled.print("food...");oled.drawBitmap(80,18,ico_food,8,8,1);break;
    case 1:oled.print("stars...");oled.drawBitmap(80,18,ico_star,8,8,1);break;
    case 2:oled.print("love...");oled.drawBitmap(80,18,ico_heart,8,8,1);break;
    case 3:oled.print("playing!");break;
  }
  if(dreamFrame%2==0)oled.drawPixel(random(30,100),random(8,42),1);
  oled.setCursor(45,56);oled.print("Zzz...");
  if(dreamFrame>10)state=S_SLEEP;oled.display();
}

// ======================================================
// MEDICINE
// ======================================================
void updateMedicine(){
  static int mf=0;static unsigned long lt=0;
  if(prevState!=S_MED){mf=0;sfxMedicine();}
  oled.clearDisplay();drawGround();
  if(ms-lt>300){lt=ms;mf++;}
  if(mf<10){drawPet(petX,petY);
    oled.setTextSize(2);oled.setCursor(petX-5,petY-12);if(mf%2==0)oled.print("+");
    spawnP(petX+random(24),petY,6);
    oled.setTextSize(1);oled.setCursor(20,56);oled.print(isSick?"Treating...":"Not sick!");
  } else{if(isSick){isSick=0;sickCounter=0;health=min(health+30,100);happiness=min(happiness+10,100);careScore+=5;sfxHappy();}
    clampStats();state=S_HOME;idleTimer=ms;}
  drawP();oled.display();
}

// ======================================================
// PETTING
// ======================================================
void updatePetting(){
  oled.clearDisplay();drawGround();
  drawPet(petX,petY+(int)(ms/100%3)-1);
  if(tch&&ms-lastTouchReact>300){lastTouchReact=ms;spawnP(petX+random(24),petY-4,0);sfxPet();}
  oled.setTextSize(1);
  if(touchCount>8){oled.setCursor(20,5);oled.print("Too much!!");spawnP(petX+20,petY,3);
    if(touchCount>12){happiness-=5;state=S_HOME;idleTimer=ms;sfxError();}
  } else if(touchCount>4){oled.setCursor(30,5);oled.print("Excited!");spawnP(petX+random(24),petY-4,1);}
  else{oled.setCursor(40,5);oled.print("Purr~");}
  affection=min(affection+1,100);happiness=min(happiness+1,100);
  if(!tch&&ms-lastTouchReact>1500){careScore+=1;state=S_HOME;idleTimer=ms;}
  if(bothRose){state=S_HOME;idleTimer=ms;sfxBack();}
  drawP();oled.display();
}

// ======================================================
// MINIGAME SELECT
// ======================================================
void updateMGSelect(){
  static int si=0;oled.clearDisplay();oled.setTextSize(1);
  oled.setCursor(22,0);oled.print("-- MINI GAMES --");
  const char* g[]={"Reaction","Dodge","Rhythm"};
  for(int i=0;i<3;i++){int y=16+i*16;
    if(i==si){oled.fillRoundRect(20,y,88,14,3,1);oled.setTextColor(0);}
    else{oled.drawRoundRect(20,y,88,14,3,1);oled.setTextColor(1);}
    oled.setCursor(30,y+3);oled.print(g[i]);oled.setTextColor(1);}
  oled.setCursor(2,57);oled.print("B1:< B2:> T:OK Both:X");
  if(bothRose){state=S_MENU;sfxBack();}
  else{
    if(btn1Rose){si=(si+2)%3;sfxSelect();}
    if(btn2Rose){si=(si+1)%3;sfxSelect();}
    if(tchRose){mgScore=0;mgLives=3;mgState=0;mgCombo=0;sfxSelect();
      switch(si){
        case 0:state=S_MG_REACT;mgReactStart=ms;mgWaiting=0;break;
        case 1:state=S_MG_DODGE;mgPlayerX=60;for(int i=0;i<5;i++)mgObjActive[i]=0;break;
        case 2:state=S_MG_RHYTHM;break;}}
  }
  oled.display();
}

// ======================================================
// MG: REACTION  (touch or any button = react)
// ======================================================
void updateMGReact(){
  oled.clearDisplay();oled.setTextSize(1);
  oled.setCursor(0,0);oled.print("Sc:");oled.print(mgScore);
  oled.setCursor(80,0);oled.print("Lv:");oled.print(mgLives);
  bool act=btn1Rose||btn2Rose||tchRose;
  if(mgState==0){oled.setTextSize(2);oled.setCursor(25,25);oled.print("Wait...");
    if(!mgWaiting){mgTarget=ms+1500+random(3000);mgWaiting=1;}
    if((long)(ms-mgTarget)>=0){mgState=1;mgReactStart=ms;sfxAlarm();}
    if(act){mgLives--;sfxError();mgWaiting=0;if(mgLives<=0)mgState=3;}
  } else if(mgState==1){oled.setTextSize(2);oled.setCursor(15,20);oled.print("PRESS!");
    if(ms%200<100)oled.drawRect(0,10,SW,44,1);
    if(act){int rt=ms-mgReactStart;mgState=2;mgTimer=ms;
      if(rt<200){mgScore+=3;mgCombo++;sfxGameWin();}
      else if(rt<400){mgScore+=2;mgCombo++;sfxChirp();}else{mgScore+=1;mgCombo=0;}}
    if(ms-mgReactStart>2000){mgLives--;mgState=2;mgTimer=ms;mgCombo=0;sfxError();if(mgLives<=0)mgState=3;}
  } else if(mgState==2){oled.setTextSize(1);oled.setCursor(20,25);oled.print("Score: ");oled.print(mgScore);
    if(mgCombo>1){oled.setCursor(20,38);oled.print("Combo: ");oled.print(mgCombo);oled.print("x!");}
    if(ms-mgTimer>1500){mgState=0;mgWaiting=0;}
  } else{oled.setTextSize(2);oled.setCursor(10,10);oled.print("GAME OVER");
    oled.setTextSize(1);oled.setCursor(30,35);oled.print("Score: ");oled.print(mgScore);
    oled.setCursor(2,50);oled.print("T:Again Both:Exit");
    if(tchRose){mgScore=0;mgLives=3;mgState=0;mgCombo=0;mgWaiting=0;sfxSelect();}
    if(bothRose){happiness=min(happiness+mgScore*2,100);state=S_HOME;idleTimer=ms;sfxBack();}}
  oled.display();
}

// ======================================================
// MG: DODGE  (B1 left, B2 right)
// ======================================================
void updateMGDodge(){
  oled.clearDisplay();oled.setTextSize(1);
  oled.setCursor(0,0);oled.print("Sc:");oled.print(mgScore);
  oled.setCursor(80,0);oled.print("Lv:");oled.print(mgLives);
  if(mgLives>0){oled.fillRect(mgPlayerX,54,8,8,1);
    if(btn1)mgPlayerX=max(mgPlayerX-3,0);
    if(btn2)mgPlayerX=min(mgPlayerX+3,120);
    if(ms-lastMiniGameTick>150){lastMiniGameTick=ms;
      for(int i=0;i<5;i++)if(mgObjActive[i]){mgObjY[i]+=3;
        if(mgObjY[i]>50&&mgObjY[i]<62&&mgObjX[i]>mgPlayerX-6&&mgObjX[i]<mgPlayerX+10){
          mgLives--;mgObjActive[i]=0;sfxError();if(mgLives<=0)sfxGameLose();}
        if(mgObjY[i]>64){mgObjActive[i]=0;mgScore++;sfxChirp();}}
      if(random(100)<25+mgScore)for(int i=0;i<5;i++)if(!mgObjActive[i]){mgObjX[i]=random(0,120);mgObjY[i]=0;mgObjActive[i]=1;break;}}
    for(int i=0;i<5;i++)if(mgObjActive[i])oled.fillRect(mgObjX[i],mgObjY[i],6,6,1);
    oled.drawFastHLine(0,63,SW,1);
  } else{oled.setTextSize(2);oled.setCursor(10,10);oled.print("GAME OVER");
    oled.setTextSize(1);oled.setCursor(30,35);oled.print("Score: ");oled.print(mgScore);
    oled.setCursor(2,50);oled.print("T:Again Both:Exit");
    if(tchRose){mgScore=0;mgLives=3;for(int i=0;i<5;i++)mgObjActive[i]=0;mgPlayerX=60;sfxSelect();}
    if(bothRose){happiness=min(happiness+mgScore*2,100);state=S_HOME;idleTimer=ms;sfxBack();}}
  oled.display();
}

// ======================================================
// MG: RHYTHM
// ======================================================
void updateMGRhythm(){
  oled.clearDisplay();oled.setTextSize(1);
  oled.setCursor(0,0);oled.print("Sc:");oled.print(mgScore);
  oled.setCursor(80,0);oled.print("Lv:");oled.print(mgLives);
  if(mgLives>0){static int by=0,bt=0;static unsigned long lt=0;static bool ba=1;
    oled.drawRect(0,48,SW,3,1);
    if(ms-lt>100){lt=ms;if(ba)by+=4;}
    if(!ba&&ms-lt>500){ba=1;by=0;bt=random(3);}
    if(ba){int bx=0;const char*lb="";
      if(bt==0){bx=20;lb="B1";}else if(bt==1){bx=55;lb="B2";}else{bx=90;lb="T";}
      oled.fillRoundRect(bx,by,30,10,3,1);
      oled.setTextColor(0);oled.setCursor(bx+6,by+1);oled.print(lb);oled.setTextColor(1);
      bool hit=(bt==0&&btn1Rose)||(bt==1&&btn2Rose)||(bt==2&&tchRose);
      if(hit){if(by>38&&by<55){mgScore+=2;mgCombo++;sfxChirp();spawnP(bx+15,45,1);}
        else if(by>28&&by<58){mgScore+=1;mgCombo=0;}else{mgLives--;mgCombo=0;sfxError();}ba=0;}
      bool wrong=(bt!=0&&btn1Rose)||(bt!=1&&btn2Rose)||(bt!=2&&tchRose);
      if(wrong&&ba){mgLives--;mgCombo=0;sfxError();ba=0;}
      if(by>60){mgLives--;mgCombo=0;sfxError();ba=0;if(mgLives<=0)sfxGameLose();}}
    oled.setCursor(28,55);oled.print("B1");oled.setCursor(63,55);oled.print("B2");oled.setCursor(100,55);oled.print("T");
    if(mgCombo>2){oled.setCursor(40,15);oled.print(mgCombo);oled.print("x COMBO!");}
  } else{oled.setTextSize(2);oled.setCursor(10,10);oled.print("GAME OVER");
    oled.setTextSize(1);oled.setCursor(30,35);oled.print("Score: ");oled.print(mgScore);
    oled.setCursor(2,50);oled.print("T:Again Both:Exit");
    if(tchRose){mgScore=0;mgLives=3;mgCombo=0;sfxSelect();}
    if(bothRose){happiness=min(happiness+mgScore,100);state=S_HOME;idleTimer=ms;sfxBack();}}
  drawP();oled.display();
}

// ======================================================
// EVOLUTION
// ======================================================
void updateEvolution(){
  static unsigned long lt=0;oled.clearDisplay();
  if(ms-lt>200){lt=ms;evolveFrame++;}
  if(evolveFrame<15){if(evolveFrame%2==0){oled.fillRect(0,0,SW,SH,1);oled.setTextColor(0);}else oled.setTextColor(1);
    oled.setTextSize(2);oled.setCursor(5,5);oled.print("EVOLVING!");
    drawPet(52,22);
    for(int i=0;i<3;i++){spawnP(random(SW),random(SH),6);spawnP(random(SW),random(SH),1);}
    oled.setTextColor(1);
  } else if(evolveFrame<25){drawPet(52,22);
    const char*st[]={"Egg?","Baby!","Teen!","Adult!"};
    oled.setTextSize(2);oled.setCursor(15,5);oled.print(st[evoStage]);
  } else{state=S_HOME;idleTimer=ms;sfxHappy();}
  drawP();oled.display();
}

// ======================================================
// DEATH
// ======================================================
void updateDeath(){
  static unsigned long lt=0;oled.clearDisplay();
  if(ms-lt>400){lt=ms;deathFrame++;}
  if(deathFrame<10){drawPet(petX,petY+deathFrame*2);
    oled.setTextSize(1);oled.setCursor(30,5);oled.print("Oh no...");
  } else{oled.drawBitmap(56,12,ico_skull,8,8,1);
    oled.setTextSize(2);oled.setCursor(30,24);oled.print("R.I.P.");
    oled.setTextSize(1);oled.setCursor(20,44);oled.print("Age: ");oled.print(petAge);oled.print(" days");
    oled.setCursor(8,54);oled.print("Press BOTH: New Game");
    if(bothRose){hunger=80;happiness=80;energy=100;hygiene=80;affection=50;health=100;
      petAge=0;evoStage=0;isSick=0;isAsleep=0;isDead=0;poopCount=0;careScore=0;dayCount=0;
      personality=0;mood=0;state=S_BOOT;bootAnimStart=0;deathFrame=0;}}
  oled.display();
}

// ======================================================
// SCREENSAVER
// ======================================================
void updateScreensaver(){
  oled.clearDisplay();
  if(ms-lastScreenSaver>50){lastScreenSaver=ms;ssX+=ssDx*2;ssY+=ssDy*2;
    if(ssX<=0||ssX>=SW-24)ssDx=-ssDx;if(ssY<=0||ssY>=SH-24)ssDy=-ssDy;}
  drawPet(ssX,ssY);
  if(btn1Rose||btn2Rose||tchRose){state=S_HOME;idleTimer=ms;sfxWakeUp();}
  oled.display();
}

// ======================================================
// SOUND TEST
// ======================================================
void updateSoundTest(){
  oled.clearDisplay();oled.setTextSize(1);
  oled.setCursor(18,0);oled.print("-- SOUND TEST --");
  const char*sn[]={"Chirp","Happy","Sad","Eat","Clean","Sleep","Boot","Evolve"};
  for(int i=0;i<8;i++){int c=i%2,r=i/2,x=c*64+4,y=14+r*12;
    if(i==soundTestIdx){oled.fillRoundRect(x-2,y-1,62,11,2,1);oled.setTextColor(0);}else oled.setTextColor(1);
    oled.setCursor(x+2,y+1);oled.print(sn[i]);oled.setTextColor(1);}
  oled.setCursor(2,57);oled.print("B1:< B2:> T:Play Both:X");
  if(bothRose){state=S_MENU;sfxBack();}
  else{
    if(btn1Rose){soundTestIdx=(soundTestIdx+7)%8;sfxSelect();}
    if(btn2Rose){soundTestIdx=(soundTestIdx+1)%8;sfxSelect();}
    if(tchRose){sqClear();
      switch(soundTestIdx){case 0:sfxChirp();break;case 1:sfxHappy();break;case 2:sfxSad();break;
        case 3:sfxEat();break;case 4:sfxClean();break;case 5:sfxSleep();break;
        case 6:sfxBoot();break;case 7:sfxEvolution();break;}}
  }
  oled.display();
}

// ======================================================
// SETUP
// ======================================================
void setup(){
  pinMode(BUZZ_PIN,OUTPUT);digitalWrite(BUZZ_PIN,LOW);noTone(BUZZ_PIN);
  Serial.begin(115200);delay(500);
  pinMode(BTN1_PIN,INPUT_PULLUP);pinMode(BTN2_PIN,INPUT_PULLUP);pinMode(TOUCH_PIN,INPUT);
  Wire.begin(SDA_PIN,SCL_PIN);
  if(!oled.begin(SSD1306_SWITCHCAPVCC,0x3C)){Serial.println("OLED FAIL");while(1);}
  oled.clearDisplay();oled.display();
  for(int i=0;i<MAX_P;i++)particles[i].active=0;
  for(int i=0;i<5;i++)mgObjActive[i]=0;
  randomSeed(analogRead(1));
  bootAnimStart=0;state=S_BOOT;
  Serial.println("TRAX_TOM");
}

// ======================================================
// LOOP
// ======================================================
void loop(){
  ms=millis();readInputs();sqUpdate();
  if(state!=S_BOOT&&state!=S_HATCH&&state!=S_EVOLVE&&state!=S_DEATH){decayStats();updateAnims();}
  prevState=state;
  switch(state){
    case S_BOOT:updateBoot();break;
    case S_HATCH:updateHatch();break;
    case S_HOME:updateHome();break;
    case S_MENU:updateMenu();break;
    case S_FEED:updateFeeding();break;
    case S_PLAY:updatePlaying();break;
    case S_CLEAN:updateCleaning();break;
    case S_SLEEP:updateSleeping();break;
    case S_MED:updateMedicine();break;
    case S_STATS:updateStats();break;
    case S_CHOOSER:updateChooser();break;
    case S_PET:updatePetting();break;
    case S_MG_SEL:updateMGSelect();break;
    case S_MG_REACT:updateMGReact();break;
    case S_MG_DODGE:updateMGDodge();break;
    case S_MG_RHYTHM:updateMGRhythm();break;
    case S_EVOLVE:updateEvolution();break;
    case S_DEATH:updateDeath();break;
    case S_SCREENSAVER:updateScreensaver();break;
    case S_SOUNDTEST:updateSoundTest();break;
    case S_DREAM:updateDream();break;
  }
}
