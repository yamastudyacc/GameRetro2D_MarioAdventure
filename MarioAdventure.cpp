#include <Arduino.h>
#include <U8g2lib.h>
#include <EEPROM.h>

const int PIN_JOY_X  = A0;
const int PIN_JOY_Y  = A1;
const int PIN_JOY_SW = 2;
const int BUZZER_PIN = 3;
U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
const uint8_t SCREEN_W = 128;
const uint8_t SCREEN_H = 64;
const uint8_t GROUND_Y = 52;                 
const unsigned long FRAME_MS = 33;       
const uint8_t MAX_HP = 3;
const uint8_t MARIO_W = 10;
const uint8_t MARIO_H = 14;
const uint8_t HB_X = 2;
const uint8_t HB_Y = 1;
const uint8_t HB_W = 6;
const uint8_t HB_H = 13;
const int16_t MOVE_SPEED = 20;
const int16_t GRAVITY = 5;
const int16_t JUMP_FORCE = -56;         
const int16_t MAX_FALL = 60;
const int16_t STOMP_BOUNCE = -38;
const uint8_t COYOTE_FRAMES      = 4;    
const uint8_t JUMP_BUFFER_FRAMES = 5;    
const uint8_t ATTACK_FRAMES          = 6;
const uint8_t ATTACK_COOLDOWN_FRAMES = 12;
const uint8_t ATTACK_REACH = 15;
const uint8_t ATTACK_H     = 8;
const uint8_t INVINCIBLE_FRAMES  = 45;
const uint8_t ENEMY_HURT_FRAMES  = 12;
const uint8_t PIPE_W        = 18;
const uint8_t PIPE_MEDIUM_H = 12;
const uint8_t PIPE_TALL_H   = 16;
const int16_t LEVEL1_GOAL = 10000;
const int16_t LEVEL2_GOAL = 15000;
const uint16_t JOY_LEFT_THRESHOLD  = 380;
const uint16_t JOY_RIGHT_THRESHOLD = 650;
const uint16_t JOY_UP_THRESHOLD    = 350;
const uint16_t JOY_DOWN_THRESHOLD  = 680;
const uint8_t MAX_ENEMIES  = 10;
const uint8_t MAX_PIPES    = 16;
const uint8_t MAX_COINS    = 40;
const uint8_t MAX_GAPS     = 4;
const uint8_t MAX_FLOATERS = 4;
const uint8_t FLOATER_LIFE = 26;
const uint8_t EXP_HEAL_STEP = 60;            
const uint8_t EEPROM_ADDR_HI = 0;
enum GameState : uint8_t {
  STATE_START,
  STATE_INTRO,
  STATE_PLAY,
  STATE_LEVEL_CLEAR,
  STATE_GAMEOVER,
  STATE_WIN
};

enum EnemyType : uint8_t {
  ENEMY_GOOMBA,
  ENEMY_STRONG,
  ENEMY_BIRD
};

enum ItemKind : uint8_t {
  ITEM_COIN,
  ITEM_HEART
};

enum FloaterKind : uint8_t {
  FL_SCORE,
  FL_EXP,
  FL_HEAL
};

struct Enemy {
  int16_t x;
  uint8_t speed;
  int16_t y;          
  uint8_t type;
  int8_t hp;
  int8_t dir;         
  uint8_t hurt;       
  bool active;
  bool awake;         
};

struct Pipe {
  int16_t x;
  uint8_t h;
  bool passed;
};

struct Item {
  int16_t x;
  uint8_t y;
  uint8_t kind;
  bool taken;
};

struct Gap {
  int16_t x;
  uint8_t w;
};

struct Floater {
  int16_t x;
  int16_t y;
  int8_t value;
  uint8_t kind;
  uint8_t life;
};

struct Note {
  uint16_t f;
  uint8_t d;
} __attribute__((packed));

const Note THEME_GRASS[] PROGMEM = {
  {523, 11}, {659, 11}, {784, 11}, {659, 11}, {880, 22}, {784, 11}, {659, 11}, {587, 22},
  {523, 11}, {659, 11}, {784, 11}, {1047, 22}, {880, 11}, {784, 11}, {659, 22}, {0, 11},
  {587, 11}, {698, 11}, {880, 11}, {698, 11}, {784, 22}, {698, 11}, {587, 11}, {494, 22},
  {523, 11}, {659, 11}, {784, 11}, {659, 11}, {523, 33}, {0, 11}
};

const Note THEME_CASTLE[] PROGMEM = {
  {440, 14}, {523, 14}, {659, 14}, {523, 14}, {440, 14}, {415, 14}, {440, 28}, {0, 14},
  {349, 14}, {440, 14}, {523, 14}, {440, 14}, {392, 14}, {370, 14}, {392, 28}, {0, 14}
};

const Note JINGLE_CLEAR[] PROGMEM = {
  {523, 8}, {659, 8}, {784, 8}, {1047, 25}
};
const Note JINGLE_START[] PROGMEM = {
  {659, 6}, {784, 6}, {988, 6}, {1319, 14}
};
const Note JINGLE_WIN[] PROGMEM = {
  {523, 10}, {659, 10}, {784, 10}, {1047, 20}, {784, 10}, {1047, 10}, {1319, 40}
};

const Note JINGLE_OVER[] PROGMEM = {
  {500, 16}, {420, 16}, {350, 16}, {260, 45}
};

const int8_t WAVE[16] PROGMEM = {
  0, 1, 2, 3, 3, 3, 2, 1, 0, -1, -2, -3, -3, -3, -2, -1
};

GameState gameState = STATE_START;
uint16_t frameCounter = 0;
uint16_t stateTimer = 0;
unsigned long lastFrameTime = 0;

int16_t marioX = 280;
int16_t marioY = (GROUND_Y - MARIO_H) * 10;
int16_t velocityY = 0;
bool onGround = true;
bool facingRight = true;
bool moving = false;
uint8_t walkTimer = 0;

uint8_t coyoteTimer = 0;
uint8_t jumpBuffer = 0;
uint8_t attackFrames = 0;
uint8_t attackCooldown = 0;
uint8_t invincibleFrames = 0;

uint8_t playerHP = MAX_HP;

int16_t cameraX = 0;

uint8_t currentLevel = 1;
uint16_t score = 0;
uint16_t expPoints = 0;
uint16_t nextExpHeal = EXP_HEAL_STEP;
uint8_t coinCount = 0;
uint8_t pipesPassed = 0;
uint8_t levelBonus = 0;

uint16_t highScore = 0;
bool newHighScore = false;

Enemy   enemies[MAX_ENEMIES];
Pipe    pipes[MAX_PIPES];
Item    items[MAX_COINS];
Gap     gaps[MAX_GAPS];
Floater floaters[MAX_FLOATERS];

uint8_t numEnemies = 0;
uint8_t numPipes = 0;
uint8_t numItems = 0;
uint8_t numGaps = 0;

bool inLeft = false;
bool inRight = false;
bool inUpHeld = false;
bool inUpPressed = false;
bool inDownPressed = false;
bool inBtnPressed = false;

bool prevUp = false;
bool prevDown = false;
bool prevBtn = false;

bool soundOn = true;
bool audioActive = false;
bool audioLoop = false;
const Note* audioTrack = nullptr;
uint8_t audioLen = 0;
uint8_t audioIdx = 0;
unsigned long audioNextTime = 0;
unsigned long sfxUntil = 0;

#define PLAY_TRACK(arr, loop) playTrack((arr), sizeof(arr) / sizeof(Note), (loop))
#define FP(x) ((int16_t)((x) * 10))
#define PX(x) ((int16_t)((x) / 10))


int16_t wrapMod(int16_t v, int16_t m) {
  v %= m;
  if (v < 0) v += m;
  return v;
}

bool rectOverlap(
  int16_t ax, int16_t ay, int16_t aw, int16_t ah,
  int16_t bx, int16_t by, int16_t bw, int16_t bh
) {
  return (ax < bx + bw * 10 && ax + aw * 10 > bx &&
          ay < by + bh * 10 && ay + ah * 10 > by);
}

int16_t goalX() {
  return (currentLevel == 1) ? LEVEL1_GOAL : LEVEL2_GOAL;
}

int enemyW(uint8_t type) {
  if (type == ENEMY_GOOMBA) return 11;
  return 12;
}

int enemyH(uint8_t type) {
  if (type == ENEMY_BIRD) return 8;
  if (type == ENEMY_STRONG) return 12;
  return 10;
}

int16_t enemyTopY(int i) {
  if (enemies[i].type == ENEMY_BIRD) {
    uint8_t idx = (uint8_t)((frameCounter / 3 + (uint16_t)i * 5) & 15);
    return enemies[i].y + (int8_t)pgm_read_byte(&WAVE[idx]) * 10;
  }
  return enemies[i].y;
}

int gapAt(int16_t x) {
  for (uint8_t g = 0; g < numGaps; g++) {
    if (x >= gaps[g].x && x <= gaps[g].x + gaps[g].w * 10) return g;
  }
  return -1;
}

bool insideGap(int16_t l, int16_t r) {
  for (uint8_t g = 0; g < numGaps; g++) {
    if (l >= gaps[g].x && r <= gaps[g].x + gaps[g].w * 10) return true;
  }
  return false;
}

bool blinkOn(uint8_t period) {
  return ((frameCounter / period) % 2) == 0;
}

void playTrack(const Note* track, uint8_t len, bool loop) {
  audioTrack = track;
  audioLen = len;
  audioIdx = 0;
  audioLoop = loop;
  audioActive = soundOn;
  audioNextTime = millis();
  noTone(BUZZER_PIN);
}

void stopAudio() {
  audioActive = false;
  noTone(BUZZER_PIN);
}

void playSfx(uint16_t freq, uint16_t ms) {
  if (!soundOn) return;
  tone(BUZZER_PIN, freq, ms);
  sfxUntil = millis() + ms;
}

void updateAudio() {
  if (!audioActive || !soundOn) return;

  unsigned long now = millis();
  if (now < sfxUntil || now < audioNextTime) return;

  if (audioIdx >= audioLen) {
    if (audioLoop) {
      audioIdx = 0;
    } else {
      audioActive = false;
      noTone(BUZZER_PIN);
      return;
    }
  }

  Note n;
  memcpy_P(&n, &audioTrack[audioIdx], sizeof(Note));
  audioIdx++;

  unsigned long ms = (unsigned long)n.d * 10UL;

  if (n.f > 0) {
    tone(BUZZER_PIN, n.f, ms * 85UL / 100UL);
  } else {
    noTone(BUZZER_PIN);
  }

  audioNextTime = now + ms;
}

void loadHighScore() {
#if defined(ESP32) || defined(ESP8266)
  EEPROM.begin(8);
#endif
  uint16_t v = 0;
  EEPROM.get(EEPROM_ADDR_HI, v);
  highScore = (v == 0xFFFF) ? 0 : v;
}

void saveHighScoreIfNeeded() {
  newHighScore = false;

  uint16_t s = score;
  if (s > 65000) s = 65000;

  if (s > highScore) {
    highScore = (uint16_t)s;
    EEPROM.put(EEPROM_ADDR_HI, highScore);
#if defined(ESP32) || defined(ESP8266)
    EEPROM.commit();
#endif
    newHighScore = true;
  }
}

void spawnFloater(int16_t wx, int16_t wy, int16_t value, uint8_t kind) {
  int slot = 0;

  for (uint8_t i = 0; i < MAX_FLOATERS; i++) {
    if (floaters[i].life == 0) {
      slot = i;
      break;
    }
  }

  floaters[slot].x = wx;
  floaters[slot].y = wy;
  floaters[slot].value = value;
  floaters[slot].kind = kind;
  floaters[slot].life = FLOATER_LIFE;
}

void updateFloaters() {
  for (uint8_t i = 0; i < MAX_FLOATERS; i++) {
    if (floaters[i].life > 0) floaters[i].life--;
  }
}

void addScore(uint8_t n) {
  score += n;
}

void addExp(uint8_t n) {
  expPoints += n;

  while (expPoints >= nextExpHeal) {
    nextExpHeal += EXP_HEAL_STEP;

    if (playerHP < MAX_HP) {
      playerHP++;
      spawnFloater(marioX, marioY - 40, 0, FL_HEAL);
      playSfx(1319, 80);
    }
  }
}

void addEnemy(int16_t x, uint8_t type, uint8_t speed, uint8_t birdY = 32) {
  if (numEnemies >= MAX_ENEMIES) return;

  Enemy &e = enemies[numEnemies++];

  e.x = x * 10;
  e.speed = speed;

  e.type = type;
  e.hp = (type == ENEMY_STRONG) ? 2 : 1;
  e.dir = -1;
  e.hurt = 0;
  e.active = true;
  e.awake = false;
  e.y = ((type == ENEMY_BIRD) ? birdY : (GROUND_Y - enemyH(type))) * 10;
}

void addPipe(int x, bool tall) {
  if (numPipes >= MAX_PIPES) return;

  pipes[numPipes].x = x * 10;
  pipes[numPipes].h = tall ? PIPE_TALL_H : PIPE_MEDIUM_H;
  pipes[numPipes].passed = false;
  numPipes++;
}

void addGap(int x, int w) {
  if (numGaps >= MAX_GAPS) return;

  gaps[numGaps].x = x * 10;
  gaps[numGaps].w = w;
  numGaps++;
}

void addItem(int x, int y, uint8_t kind) {
  if (numItems >= MAX_COINS) return;

  items[numItems].x = x * 10;
  items[numItems].y = y;
  items[numItems].kind = kind;
  items[numItems].taken = false;
  numItems++;
}

void addCoinArc(int cx, int topY, int n) {
  for (int i = 0; i < n; i++) {
    int d = 2 * i - (n - 1);
    addItem(cx + d * 5 - 3, topY + (d * d * 3) / 4, ITEM_COIN);
  }
}

void addCoinRow(int x, int y, int n) {
  for (int i = 0; i < n; i++) {
    addItem(x + i * 10, y, ITEM_COIN);
  }
}

void beginLevel(int level) {
  currentLevel = level;

  numEnemies = 0;
  numPipes = 0;
  numItems = 0;
  numGaps = 0;

  for (uint8_t i = 0; i < MAX_FLOATERS; i++) floaters[i].life = 0;

  marioX = 280;
  marioY = (GROUND_Y - MARIO_H) * 10;
  velocityY = 0;
  onGround = true;
  facingRight = true;
  moving = false;
  walkTimer = 0;

  cameraX = 0;

  playerHP = MAX_HP;          

  invincibleFrames = 0;
  attackFrames = 0;
  attackCooldown = 0;
  coyoteTimer = 0;
  jumpBuffer = 0;

  pipesPassed = 0;
}

void setupLevel1() {
  beginLevel(1);

  addPipe(110, false);
  addPipe(230, true);
  addPipe(300, false);
  addPipe(430, true);
  addPipe(560, false);
  addPipe(700, true);
  addPipe(830, false);
  addPipe(930, true);

  addEnemy(170, ENEMY_GOOMBA, 6);
  addEnemy(340, ENEMY_GOOMBA, 7);
  addEnemy(480, ENEMY_BIRD,   9, 33);
  addEnemy(620, ENEMY_GOOMBA, 7);
  addEnemy(770, ENEMY_BIRD,   9, 30);
  addEnemy(880, ENEMY_GOOMBA, 8);

  addCoinRow(60, 40, 3);
  addCoinArc(119, 20, 3);
  addCoinArc(239, 14, 3);
  addCoinRow(365, 40, 4);
  addCoinArc(439, 14, 5);
  addItem(566, 18, ITEM_HEART);
  addCoinRow(645, 40, 3);
  addCoinArc(709, 14, 5);
  addCoinRow(790, 40, 3);
  addCoinArc(939, 14, 3);
}

void setupLevel2() {
  beginLevel(2);

  addGap(380, 26);
  addGap(850, 28);
  addGap(1200, 28);
  addGap(1360, 30);

  addPipe(100, false);
  addPipe(190, true);
  addPipe(260, false);
  addPipe(470, true);
  addPipe(560, false);
  addPipe(660, true);
  addPipe(740, false);
  addPipe(950, true);
  addPipe(1020, false);
  addPipe(1090, true);
  addPipe(1290, false);
  addPipe(1420, true);

  addEnemy(160,  ENEMY_STRONG, 9);
  addEnemy(310,  ENEMY_GOOMBA, 9);
  addEnemy(420,  ENEMY_BIRD,   10, 34);
  addEnemy(520,  ENEMY_STRONG, 10);
  addEnemy(610,  ENEMY_BIRD,   11, 31);
  addEnemy(800,  ENEMY_GOOMBA, 10);
  addEnemy(900,  ENEMY_STRONG, 10);
  addEnemy(1000, ENEMY_BIRD,   11, 33);
  addEnemy(1150, ENEMY_GOOMBA, 9);
  addEnemy(1330, ENEMY_STRONG, 11);

  addCoinArc(199, 14, 3);
  addCoinArc(393, 20, 3);
  addCoinArc(479, 14, 5);
  addCoinArc(669, 14, 3);
  addCoinArc(864, 20, 3);
  addCoinArc(959, 14, 5);
  addItem(1026, 18, ITEM_HEART);
  addCoinArc(1099, 14, 3);
  addCoinArc(1214, 20, 3);
  addCoinArc(1375, 20, 3);
  addCoinArc(1429, 14, 3);
}

void setupLevel(int level) {
  if (level == 1) setupLevel1();
  else setupLevel2();

  stopAudio();

  gameState = STATE_INTRO;
  stateTimer = 0;
}

void startNewGame() {
  score = 0;
  expPoints = 0;
  nextExpHeal = EXP_HEAL_STEP;
  coinCount = 0;
  newHighScore = false;

  setupLevel(1);
  PLAY_TRACK(JINGLE_START, false);
}

void enterPlay() {
  gameState = STATE_PLAY;
  stateTimer = 0;

  if (currentLevel == 1) PLAY_TRACK(THEME_GRASS, true);
  else PLAY_TRACK(THEME_CASTLE, true);
}

void triggerGameOver() {
  gameState = STATE_GAMEOVER;
  stateTimer = 0;

  saveHighScoreIfNeeded();

  stopAudio();
  PLAY_TRACK(JINGLE_OVER, false);
}

void movePlayerX(int16_t dx) {
  marioX += dx;
  if (marioX < cameraX * 10) marioX = cameraX * 10;

  int16_t maxX = goalX() + 300;
  if (marioX > maxX) marioX = maxX;

  int16_t hy = marioY + HB_Y * 10;

  for (uint8_t p = 0; p < numPipes; p++) {
    int16_t pt = (GROUND_Y - pipes[p].h) * 10;

    if (rectOverlap(marioX + HB_X * 10, hy, HB_W, HB_H,
                    pipes[p].x, pt, PIPE_W, pipes[p].h)) {
      if (dx > 0) marioX = pipes[p].x - HB_X * 10 - HB_W * 10;
      else if (dx < 0) marioX = pipes[p].x + PIPE_W * 10 - HB_X * 10;
    }
  }

  if (marioY + MARIO_H * 10 > (GROUND_Y + 2) * 10) {
    int g = gapAt(marioX + MARIO_W * 5);
    if (g >= 0) {
      int16_t lo = gaps[g].x - HB_X * 10;
      int16_t hi = gaps[g].x + gaps[g].w * 10 - HB_X * 10 - HB_W * 10;
      if (marioX < lo) marioX = lo;
      if (marioX > hi) marioX = hi;
    }
  }
}

void fallIntoPit() {
  int g = gapAt(marioX + MARIO_W * 5);

  playerHP--;
  playSfx(150, 200);

  if (playerHP <= 0) {
    triggerGameOver();
    return;
  }

  int16_t rx = (g >= 0) ? (gaps[g].x - 240) : (marioX - 400);
  int16_t cam = cameraX * 10;
  if (rx < cam + 20) rx = cam + 20;

  marioX = rx;
  marioY = (GROUND_Y - MARIO_H) * 10;
  velocityY = 0;
  onGround = true;
  invincibleFrames = 60;
}

void movePlayerY() {
  velocityY += GRAVITY;
  if (velocityY > MAX_FALL) velocityY = MAX_FALL;

  int16_t oldBottom = marioY + MARIO_H * 10;
  marioY += velocityY;
  int16_t bottom = marioY + MARIO_H * 10;

  onGround = false;

  int16_t l = marioX + HB_X * 10;
  int16_t r = l + HB_W * 10;

  if (velocityY >= 0) {
    for (uint8_t p = 0; p < numPipes; p++) {
      int16_t top = (GROUND_Y - pipes[p].h) * 10;

      if (r > pipes[p].x && l < pipes[p].x + PIPE_W * 10 &&
          oldBottom <= top + 10 && bottom >= top) {
        marioY = top - MARIO_H * 10;
        velocityY = 0;
        onGround = true;
        break;
      }
    }

    if (!onGround && !insideGap(l, r) &&
        oldBottom <= (GROUND_Y + 1) * 10 && bottom >= GROUND_Y * 10) {
      marioY = (GROUND_Y - MARIO_H) * 10;
      velocityY = 0;
      onGround = true;
    }
  }

  if (marioY < 0) {
    marioY = 0;
    if (velocityY < 0) velocityY = 0;
  }

  if (marioY > (SCREEN_H + 6) * 10) fallIntoPit();
}

void startAttack() {
  if (attackFrames > 0 || attackCooldown > 0) return;

  attackFrames = ATTACK_FRAMES;
  attackCooldown = ATTACK_FRAMES + ATTACK_COOLDOWN_FRAMES;

  playSfx(1100, 55);
}

void updatePlayer() {
  if (invincibleFrames > 0) invincibleFrames--;
  if (attackFrames > 0) attackFrames--;
  if (attackCooldown > 0) attackCooldown--;

  int16_t dx = 0;

  if (inLeft) {
    dx = -MOVE_SPEED;
    facingRight = false;
  }
  else if (inRight) {
    dx = MOVE_SPEED;
    facingRight = true;
  }

  moving = (dx != 0);

  if (moving) movePlayerX(dx);

  if (moving && onGround) walkTimer++;
  else if (!moving) walkTimer = 0;

  if (inUpPressed) jumpBuffer = JUMP_BUFFER_FRAMES;
  else if (jumpBuffer > 0) jumpBuffer--;

  if (onGround) coyoteTimer = COYOTE_FRAMES;
  else if (coyoteTimer > 0) coyoteTimer--;

  if (jumpBuffer > 0 && coyoteTimer > 0) {
    velocityY = JUMP_FORCE;
    onGround = false;
    coyoteTimer = 0;
    jumpBuffer = 0;
    playSfx(880, 70);
  }

  if (!inUpHeld && velocityY < -25) velocityY = -25;

  if (inBtnPressed || inDownPressed) startAttack();

  movePlayerY();
}

void updateCamera() {
  int16_t target = marioX / 10 - 44;
  if (target > cameraX) cameraX = target;

  int16_t maxCam = goalX() / 10 + 40 - SCREEN_W;
  if (cameraX > maxCam) cameraX = maxCam;
  if (cameraX < 0) cameraX = 0;
}

void damageEnemy(int i) {
  Enemy &e = enemies[i];

  e.hp--;
  e.hurt = ENEMY_HURT_FRAMES;

  if (e.hp > 0) {
    playSfx(500, 40);
    return;
  }

  e.active = false;

  int8_t pts;
  int8_t xp;

  if (e.type == ENEMY_STRONG) {
    pts = 40;
    xp = 10;
  }
  else if (e.type == ENEMY_BIRD) {
    pts = 30;
    xp = 5;
  }
  else {
    pts = 20;
    xp = 5;
  }

  addScore(pts);
  spawnFloater(e.x, enemyTopY(i) - 20, pts, FL_SCORE);
  addExp(xp);

  playSfx(1200, 55);
}

void updateEnemies() {
  for (uint8_t i = 0; i < numEnemies; i++) {
    Enemy &e = enemies[i];
    if (!e.active) continue;

    uint8_t w = enemyW(e.type);
    uint8_t h = enemyH(e.type);

    if (!e.awake) {
      if (e.x - cameraX * 10 < (SCREEN_W + 8) * 10) e.awake = true;
      else continue;
    }

    if (e.hurt > 0) e.hurt--;

    e.x += e.dir * e.speed;

    if (e.type != ENEMY_BIRD) {
      for (uint8_t p = 0; p < numPipes; p++) {
        int16_t pt = (GROUND_Y - pipes[p].h) * 10;

        if (rectOverlap(e.x, e.y, w, h,
                        pipes[p].x, pt, PIPE_W, pipes[p].h)) {
          if (e.dir < 0) {
            e.x = pipes[p].x + PIPE_W * 10;
            e.dir = 1;
          } else {
            e.x = pipes[p].x - w * 10;
            e.dir = -1;
          }
        }
      }

      if (gapAt(e.x + w * 5) >= 0) e.active = false;
    }

    if (e.x < cameraX * 10 - 300 ||
        e.x > cameraX * 10 + (SCREEN_W + 160) * 10) {
      e.active = false;
    }
  }
}

void checkAttackHits() {
  if (attackFrames == 0) return;

  int16_t ax = facingRight ? (marioX + MARIO_W * 10 - 10)
                           : (marioX + 10 - ATTACK_REACH * 10);
  int16_t ay = marioY + 40;

  for (uint8_t i = 0; i < numEnemies; i++) {
    Enemy &e = enemies[i];
    if (!e.active || !e.awake || e.hurt > 0) continue;

    int16_t ey = enemyTopY(i);

    if (rectOverlap(ax, ay, ATTACK_REACH, ATTACK_H,
                    e.x, ey, enemyW(e.type), enemyH(e.type))) {
      e.x += facingRight ? 40 : -40;
      damageEnemy(i);
    }
  }
}

void hurtPlayer(int16_t sourceCenterX) {
  playerHP--;
  invincibleFrames = INVINCIBLE_FRAMES;

  velocityY = -30;
  onGround = false;

  playSfx(180, 120);

  if (playerHP <= 0) {
    triggerGameOver();
    return;
  }

  if (sourceCenterX > marioX + MARIO_W * 5) movePlayerX(-120);
  else movePlayerX(120);
}

void checkEnemyContacts() {
  for (uint8_t i = 0; i < numEnemies; i++) {
    Enemy &e = enemies[i];
    if (!e.active || !e.awake || e.hurt > 0) continue;

    uint8_t w = enemyW(e.type);
    uint8_t h = enemyH(e.type);
    int16_t ey = enemyTopY(i);

    int16_t hx = marioX + HB_X * 10;
    int16_t hy = marioY + HB_Y * 10;

    if (rectOverlap(hx, hy, HB_W, HB_H, e.x, ey, w, h)) {
      int16_t prevBottom = marioY + MARIO_H * 10 - velocityY;

      if (velocityY > 0 && prevBottom <= ey + 60) {
        marioY = ey - MARIO_H * 10;
        velocityY = STOMP_BOUNCE;
        onGround = false;
        damageEnemy(i);
      }
      else if (invincibleFrames == 0) {
        hurtPlayer(e.x + w * 5);
        return;
      }
    }
  }
}

void collectItems() {
  int16_t hx = marioX + HB_X * 10;
  int16_t hy = marioY + HB_Y * 10;

  for (uint8_t i = 0; i < numItems; i++) {
    if (items[i].taken) continue;

    if (rectOverlap(hx, hy, HB_W, HB_H, items[i].x, items[i].y * 10, 7, 7)) {
      items[i].taken = true;

      if (items[i].kind == ITEM_COIN) {
        coinCount++;
        addScore(10);
        playSfx(1568, 45);
      }
      else {
        if (playerHP < MAX_HP) playerHP++;
        addScore(50);
        spawnFloater(items[i].x, items[i].y * 10 - 20, 0, FL_HEAL);
        playSfx(1319, 120);
      }
    }
  }
}

void updatePipeScore() {
  for (uint8_t p = 0; p < numPipes; p++) {
    if (pipes[p].passed) continue;

    if (marioX > pipes[p].x + PIPE_W * 10 + 40) {
      pipes[p].passed = true;
      addScore(10);
      pipesPassed++;

      if (pipesPassed % 3 == 0) {
        addExp(15);
        spawnFloater(marioX, marioY - 40, 15, FL_EXP);
      }
    }
  }
}

void checkGoal() {
  int16_t gx = goalX();
  uint8_t gw, gh;

  if (currentLevel == 1) {
    gw = 12;
    gh = 30;
  } else {
    gw = 18;
    gh = 24;
  }

  int16_t gy = (GROUND_Y - gh) * 10;

  if (!rectOverlap(marioX + HB_X * 10, marioY + HB_Y * 10,
                   HB_W, HB_H, gx, gy, gw, gh)) return;

  stopAudio();

  levelBonus = playerHP * 25;
  stateTimer = 0;

  if (currentLevel == 1) {
    addScore(100 + levelBonus);
    gameState = STATE_LEVEL_CLEAR;
    PLAY_TRACK(JINGLE_CLEAR, false);
  }
  else {
    addScore(250 + levelBonus);
    gameState = STATE_WIN;
    saveHighScoreIfNeeded();
    PLAY_TRACK(JINGLE_WIN, false);
  }
}

void updateGameplay() {

  updatePlayer();
  if (gameState != STATE_PLAY) return;

  updateCamera();
  updateEnemies();

  checkAttackHits();

  checkEnemyContacts();
  if (gameState != STATE_PLAY) return;

  collectItems();
  updatePipeScore();
  updateFloaters();

  checkGoal();
}

void mbox(int8_t sx, int8_t sy, bool right, int8_t dx, int8_t dy, int8_t w, int8_t h) {
  int x = right ? (sx + dx) : (sx + MARIO_W - dx - w);
  u8g2.drawBox(x, sy + dy, w, h);
}

void drawMarioSprite(int sx, int sy, bool right, uint8_t pose) {

  mbox(sx, sy, right, 2, 0, 6, 2);
  mbox(sx, sy, right, 6, 1, 4, 1);

  mbox(sx, sy, right, 2, 2, 6, 3);
  mbox(sx, sy, right, 8, 3, 1, 1);          

  u8g2.setDrawColor(0);
  mbox(sx, sy, right, 6, 3, 1, 1);          
  u8g2.setDrawColor(1);

  mbox(sx, sy, right, 2, 5, 6, 5);

  u8g2.setDrawColor(0);
  mbox(sx, sy, right, 2, 8, 6, 1);          
  u8g2.setDrawColor(1);

  if (pose == 3) {
    mbox(sx, sy, right, 8, 4, 2, 3);
    mbox(sx, sy, right, 0, 5, 2, 3);
  } else {
    mbox(sx, sy, right, 8, 6, 2, 3);
    mbox(sx, sy, right, 0, 6, 2, 3);
  }

  if (pose == 1) {
    mbox(sx, sy, right, 1, 10, 3, 4);
    mbox(sx, sy, right, 6, 10, 3, 3);
    mbox(sx, sy, right, 0, 13, 4, 1);
  }
  else if (pose == 2) {
    mbox(sx, sy, right, 3, 10, 4, 3);
    mbox(sx, sy, right, 2, 13, 6, 1);
  }
  else if (pose == 3) {
    mbox(sx, sy, right, 1, 11, 3, 3);
    mbox(sx, sy, right, 6, 10, 3, 3);
    mbox(sx, sy, right, 8, 13, 2, 1);
  }
  else {
    mbox(sx, sy, right, 2, 10, 3, 3);
    mbox(sx, sy, right, 5, 10, 3, 3);
    mbox(sx, sy, right, 1, 13, 4, 1);
    mbox(sx, sy, right, 5, 13, 5, 1);
  }
}

void drawSword(int sx, int sy) {
  if (attackFrames == 0) return;

  int progress = ATTACK_FRAMES - attackFrames;      
  int len = 5 + progress * 3;
  if (len > ATTACK_REACH) len = ATTACK_REACH;

  int y = sy + 6;

  if (facingRight) {

    int x0 = sx + MARIO_W - 1;

    u8g2.drawBox(x0, y, len, 2);                    
    u8g2.drawBox(x0, y - 2, 2, 6);                  

    if (progress >= 2) {                            
      u8g2.drawPixel(x0 + len + 1, y - 2);
      u8g2.drawPixel(x0 + len + 2, y + 1);
      u8g2.drawPixel(x0 + len + 1, y + 4);
    }
  }
  else {

    int x0 = sx + 1;

    u8g2.drawBox(x0 - len, y, len, 2);
    u8g2.drawBox(x0 - 1, y - 2, 2, 6);

    if (progress >= 2) {
      u8g2.drawPixel(x0 - len - 2, y - 2);
      u8g2.drawPixel(x0 - len - 3, y + 1);
      u8g2.drawPixel(x0 - len - 2, y + 4);
    }
  }
}

void drawMario() {

  if (invincibleFrames > 0 && ((frameCounter / 2) % 2 == 0)) return;

  int sx = marioX / 10 - cameraX;
  int sy = marioY / 10;

  uint8_t pose = 0;

  if (!onGround) pose = 3;
  else if (moving) pose = 1 + ((walkTimer / 4) & 1);

  drawMarioSprite(sx, sy, facingRight, pose);
  drawSword(sx, sy);
}

void drawGoomba(int sx, int sy, bool frame) {

  u8g2.drawBox(sx + 2, sy, 7, 2);                   
  u8g2.drawBox(sx + 1, sy + 2, 9, 4);
  u8g2.drawBox(sx + 3, sy + 6, 5, 2);               

  u8g2.setDrawColor(0);                             
  u8g2.drawBox(sx + 3, sy + 3, 2, 2);
  u8g2.drawBox(sx + 6, sy + 3, 2, 2);
  u8g2.setDrawColor(1);

  if (frame) {
    u8g2.drawBox(sx, sy + 8, 4, 2);
    u8g2.drawBox(sx + 7, sy + 8, 4, 2);
  } else {
    u8g2.drawBox(sx + 1, sy + 8, 4, 2);
    u8g2.drawBox(sx + 6, sy + 8, 4, 2);
  }
}

void drawStrongCreep(int sx, int sy, int hp) {

  u8g2.drawBox(sx + 2, sy + 1, 2, 2);
  u8g2.drawBox(sx + 5, sy, 2, 3);
  u8g2.drawBox(sx + 8, sy + 1, 2, 2);

  u8g2.drawBox(sx + 1, sy + 3, 10, 9);
  u8g2.drawBox(sx, sy + 10, 3, 2);
  u8g2.drawBox(sx + 9, sy + 10, 3, 2);

  u8g2.setDrawColor(0);
  u8g2.drawBox(sx + 3, sy + 5, 2, 2);
  u8g2.drawBox(sx + 7, sy + 5, 2, 2);
  u8g2.drawBox(sx + 4, sy + 9, 4, 1);
  u8g2.setDrawColor(1);

  for (int i = 0; i < hp; i++) {
    u8g2.drawBox(sx + i * 4, sy - 3, 3, 2);
  }
}

void drawBird(int sx, int sy, bool flap) {

  u8g2.drawBox(sx + 3, sy + 3, 7, 3);               
  u8g2.drawBox(sx, sy + 2, 4, 3);                   
  u8g2.drawPixel(sx - 1, sy + 3);                   
  u8g2.drawLine(sx + 10, sy + 4, sx + 11, sy + 3);  

  u8g2.setDrawColor(0);
  u8g2.drawPixel(sx + 1, sy + 3);                   
  u8g2.setDrawColor(1);

  if (flap) {
    u8g2.drawLine(sx + 5, sy + 3, sx + 8, sy);
    u8g2.drawLine(sx + 6, sy + 3, sx + 10, sy + 1);
  } else {
    u8g2.drawLine(sx + 5, sy + 5, sx + 8, sy + 7);
    u8g2.drawLine(sx + 6, sy + 5, sx + 10, sy + 7);
  }
}

void drawEnemies() {

  for (int i = 0; i < numEnemies; i++) {

    Enemy &e = enemies[i];

    if (!e.active) continue;

    int sx = e.x / 10 - cameraX;

    if (sx < -20 || sx > SCREEN_W) continue;

    if (e.hurt > 0 && ((frameCounter / 2) % 2 == 0)) continue;

    int sy = enemyTopY(i);

    bool anim = (((frameCounter / 6) + i) & 1) == 0;

    if (e.type == ENEMY_GOOMBA) drawGoomba(sx, sy, anim);
    else if (e.type == ENEMY_STRONG) drawStrongCreep(sx, sy, e.hp);
    else drawBird(sx, sy, anim);
  }
}

void drawHeart(int x, int y, bool filled) {

  u8g2.drawBox(x + 1, y, 2, 1);
  u8g2.drawBox(x + 4, y, 2, 1);
  u8g2.drawBox(x, y + 1, 7, 2);
  u8g2.drawBox(x + 1, y + 3, 5, 1);
  u8g2.drawBox(x + 2, y + 4, 3, 1);
  u8g2.drawPixel(x + 3, y + 5);

  if (!filled) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(x + 1, y + 1, 5, 2);
    u8g2.drawBox(x + 2, y + 3, 3, 1);
    u8g2.drawPixel(x + 3, y + 4);
    u8g2.setDrawColor(1);
  }
}

void drawItems() {

  uint8_t spin = (frameCounter / 5) & 3;
  const uint8_t rx[4] = {3, 2, 1, 2};

  for (int i = 0; i < numItems; i++) {

    if (items[i].taken) continue;

    int sx = items[i].x / 10 - cameraX;

    if (sx < -8 || sx > SCREEN_W) continue;

    if (items[i].kind == ITEM_COIN) {
      u8g2.drawFilledEllipse(sx + 3, items[i].y + 3, rx[spin], 3);
    } else {
      
      drawHeart(sx, items[i].y + ((frameCounter / 8) & 1), true);
    }
  }
}

void drawCloud(int cx, int cy) {
  u8g2.drawCircle(cx, cy, 3);
  u8g2.drawCircle(cx + 5, cy - 2, 4);
  u8g2.drawCircle(cx + 10, cy, 3);
}

void drawHill(int cx, int rx, int ry, bool eyes) {

  if (cx < -rx - 2 || cx > SCREEN_W + rx + 2) return;

  u8g2.drawEllipse(cx, GROUND_Y - 1, rx, ry,
                   U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);

  if (eyes) {
    u8g2.drawVLine(cx - 4, GROUND_Y - ry + 5, 3);
    u8g2.drawVLine(cx + 4, GROUND_Y - ry + 5, 3);
  }
}

void drawBackground() {

  if (currentLevel == 1) {

    drawHill(wrapMod(70 - cameraX / 3, 210) - 40, 24, 14, true);
    drawHill(wrapMod(160 - cameraX / 3, 230) - 40, 16, 9, false);

    drawCloud(wrapMod(30 - cameraX / 5, 190) - 30, 18);
    drawCloud(wrapMod(120 - cameraX / 7, 210) - 30, 28);
  }
  else {

    int off = (cameraX / 2) % 64;

    for (int k = -1; k < 3; k++) {

      int px = 20 + k * 64 - off;

      u8g2.drawFrame(px, 16, 12, GROUND_Y - 16);
      u8g2.drawVLine(px + 5, 24, 10);               
      u8g2.drawVLine(px + 6, 24, 10);

      int tx = px + 38;
      int flame = 2 + ((frameCounter / 3 + k) % 3);

      u8g2.drawVLine(tx, 32, 4);
      u8g2.drawVLine(tx, 32 - flame, flame);
      u8g2.drawPixel(tx - 1, 32 - flame + 1);
      u8g2.drawPixel(tx + 1, 32 - flame + 1);
    }
  }
}

void drawGround() {

  int off = cameraX % 16;

  u8g2.drawHLine(0, GROUND_Y, SCREEN_W);

  for (int row = 0; row < 3; row++) {

    int y0 = GROUND_Y + 1 + row * 4;

    u8g2.drawHLine(0, y0 + 3, SCREEN_W);

    int shift = (row & 1) ? 8 : 0;

    for (int i = -1; i < 9; i++) {
      u8g2.drawVLine(i * 16 + shift - off, y0, 3);
    }
  }

  for (uint8_t g = 0; g < numGaps; g++) {

    int sx = gaps[g].x / 10 - cameraX;
    int w = gaps[g].w;

    if (sx + w < -2 || sx > SCREEN_W + 2) continue;

    u8g2.setDrawColor(0);
    u8g2.drawBox(sx, GROUND_Y, w, SCREEN_H - GROUND_Y);
    u8g2.setDrawColor(1);

    u8g2.drawVLine(sx - 1, GROUND_Y, SCREEN_H - GROUND_Y);
    u8g2.drawVLine(sx + w, GROUND_Y, SCREEN_H - GROUND_Y);

    for (int x = 0; x < w; x += 3) {
      u8g2.drawPixel(sx + x, 61 + (((x / 3) + (frameCounter / 4)) & 1));
    }
  }
}

void drawPipe(int sx, int h) {

  int sy = GROUND_Y - h;

  u8g2.drawBox(sx, sy, PIPE_W, 4);                  
  u8g2.drawBox(sx + 1, sy + 4, PIPE_W - 2, h - 4);  

  u8g2.setDrawColor(0);
  u8g2.drawVLine(sx + 4, sy + 5, h - 5);            
  u8g2.setDrawColor(1);
}

void drawPipes() {

  for (int p = 0; p < numPipes; p++) {

    int sx = pipes[p].x / 10 - cameraX;

    if (sx < -PIPE_W || sx > SCREEN_W) continue;

    drawPipe(sx, pipes[p].h);
  }
}

void drawGoal() {

  int sx = goalX() / 10 - cameraX;

  if (sx < -24 || sx > SCREEN_W) return;

  u8g2.setFont(u8g2_font_5x7_tr);

  if (currentLevel == 1) {

    u8g2.drawVLine(sx + 5, GROUND_Y - 30, 30);
    u8g2.drawBox(sx + 4, GROUND_Y - 32, 3, 3);

    int wave = (frameCounter / 6) & 1;

    u8g2.drawTriangle(sx + 6, GROUND_Y - 29,
                      sx + 17 - wave, GROUND_Y - 25,
                      sx + 6, GROUND_Y - 21);

    u8g2.drawStr(sx - 4, 17, "GOAL");
  }
  else {

    u8g2.drawFrame(sx, GROUND_Y - 24, 18, 24);
    u8g2.drawBox(sx + 4, GROUND_Y - 16, 10, 16);
    u8g2.drawBox(sx + 6, GROUND_Y - 19, 6, 3);

    u8g2.setDrawColor(0);
    u8g2.drawPixel(sx + 11, GROUND_Y - 8);
    u8g2.setDrawColor(1);

    u8g2.drawStr(sx - 3, 20, "DOOR");
  }
}

void drawHUD() {

  for (int i = 0; i < MAX_HP; i++) {
    drawHeart(i * 9, 1, i < playerHP);
  }

  u8g2.setFont(u8g2_font_5x7_tr);

  char buf[16];

  u8g2.drawFilledEllipse(32, 4, 2, 3);
  snprintf(buf, sizeof(buf), "x%d", coinCount);
  u8g2.drawStr(37, 7, buf);

  snprintf(buf, sizeof(buf), "%05u", (unsigned int)score);
  u8g2.drawStr(62, 7, buf);

  snprintf(buf, sizeof(buf), "LV%d", currentLevel);
  u8g2.drawStr(108, 7, buf);
}

void drawFloaters() {

  u8g2.setFont(u8g2_font_5x7_tr);

  char buf[10];

  for (uint8_t i = 0; i < MAX_FLOATERS; i++) {

    if (floaters[i].life == 0) continue;

    int sx = floaters[i].x / 10 - cameraX;
    int yy = floaters[i].y / 10 - (FLOATER_LIFE - floaters[i].life) / 2;

    if (floaters[i].kind == FL_SCORE) snprintf(buf, sizeof(buf), "+%d", floaters[i].value);
    else if (floaters[i].kind == FL_EXP) snprintf(buf, sizeof(buf), "+%dXP", floaters[i].value);
    else snprintf(buf, sizeof(buf), "HP+1");

    int w = (int)strlen(buf) * 5;

    if (sx < 0) sx = 0;
    if (sx > SCREEN_W - w) sx = SCREEN_W - w;
    if (yy < 8) yy = 8;

    u8g2.drawStr(sx, yy, buf);
  }
}

void drawGameplay() {

  drawBackground();
  drawGround();
  drawPipes();
  drawGoal();
  drawItems();
  drawEnemies();
  drawMario();
  drawFloaters();
  drawHUD();
}

void drawCentered(int y, const char* s) {
  int w = u8g2.getStrWidth(s);
  u8g2.drawStr((SCREEN_W - w) / 2, y, s);
}

void drawStartScreen() {

  char buf[24];

  u8g2.setFont(u8g2_font_7x14B_tr);
  drawCentered(15, "SUPER MARIO");
  drawCentered(30, "ADVENTURE");

  u8g2.setFont(u8g2_font_5x7_tr);

  snprintf(buf, sizeof(buf), "HI-SCORE %05u", (unsigned int)highScore);
  drawCentered(41, buf);

  if (blinkOn(15)) drawCentered(52, "PRESS JOY UP");

  drawCentered(62, soundOn ? "DOWN:SOUND ON" : "DOWN:SOUND OFF");

  drawMarioSprite(4, 36, true, 0);
  drawGoomba(113, 42, blinkOn(10));
}

void drawIntroScreen() {

  char buf[16];

  u8g2.setFont(u8g2_font_7x14B_tr);

  snprintf(buf, sizeof(buf), "WORLD %d", currentLevel);
  drawCentered(22, buf);

  u8g2.setFont(u8g2_font_5x7_tr);
  drawCentered(35, currentLevel == 1 ? "GRASSLAND" : "CASTLE");

  drawMarioSprite(59, 42, true, 0);

  u8g2.drawHLine(0, 56, SCREEN_W);
}

void drawLevelClearScreen() {

  char buf[32];

  u8g2.setFont(u8g2_font_7x14B_tr);
  drawCentered(15, "LEVEL 1");
  drawCentered(30, "CLEAR!");

  u8g2.setFont(u8g2_font_5x7_tr);

  snprintf(buf, sizeof(buf), "SCORE:%u  EXP:%u", (unsigned int)score, (unsigned int)expPoints);
  drawCentered(41, buf);

  snprintf(buf, sizeof(buf), "HP BONUS +%d", levelBonus);
  drawCentered(50, buf);

  if (stateTimer > 40 && blinkOn(15)) drawCentered(61, "PRESS JOY>LV2");
}

void drawGameOverScreen() {

  char buf[32];

  u8g2.setFont(u8g2_font_7x14B_tr);
  drawCentered(16, "GAME OVER");

  u8g2.setFont(u8g2_font_5x7_tr);

  snprintf(buf, sizeof(buf), "SCORE:%u  EXP:%u", (unsigned int)score, (unsigned int)expPoints);
  drawCentered(29, buf);

  snprintf(buf, sizeof(buf), "COINS:%d", coinCount);
  drawCentered(38, buf);

  if (newHighScore) {
    if (blinkOn(8)) drawCentered(48, "NEW HI SCORE!");
  } else {
    snprintf(buf, sizeof(buf), "HI-SCORE:%05u", (unsigned int)highScore);
    drawCentered(48, buf);
  }

  if (stateTimer > 40 && blinkOn(15)) drawCentered(60, "PRESS JOY RESTART");
}

void drawWinScreen() {

  char buf[32];

  u8g2.setFont(u8g2_font_7x14B_tr);
  drawCentered(14, "ADVENTURE");
  drawCentered(28, "COMPLETE!");

  u8g2.setFont(u8g2_font_5x7_tr);

  snprintf(buf, sizeof(buf), "SCORE:%u  EXP:%u", (unsigned int)score, (unsigned int)expPoints);
  drawCentered(40, buf);

  if (newHighScore) {
    if (blinkOn(8)) drawCentered(50, "NEW HI SCORE!");
  } else {
    snprintf(buf, sizeof(buf), "COINS:%d  HI:%05u", coinCount, (unsigned int)highScore);
    drawCentered(50, buf);
  }

  if (stateTimer > 60 && blinkOn(15)) drawCentered(61, "PRESS JOY");

  int hop = (frameCounter / 6) & 1;
  drawMarioSprite(6, 40 - hop * 2, true, 3);
  drawMarioSprite(112, 40 - (1 - hop) * 2, false, 3);
}

void renderGame() {

  u8g2.firstPage();

  do {

    switch (gameState) {

      case STATE_START:
        drawStartScreen();
        break;

      case STATE_INTRO:
        drawIntroScreen();
        break;

      case STATE_PLAY:
        drawGameplay();
        break;

      case STATE_LEVEL_CLEAR:
        drawLevelClearScreen();
        break;

      case STATE_GAMEOVER:
        drawGameOverScreen();
        break;

      case STATE_WIN:
        drawWinScreen();
        break;
    }

  } while (u8g2.nextPage());
}

void readInput() {

  int jx = analogRead(PIN_JOY_X);
  int jy = analogRead(PIN_JOY_Y);

  bool up   = jy < JOY_UP_THRESHOLD;
  bool down = jy > JOY_DOWN_THRESHOLD;
  bool btn  = (digitalRead(PIN_JOY_SW) == LOW);

  inLeft  = jx < JOY_LEFT_THRESHOLD;
  inRight = jx > JOY_RIGHT_THRESHOLD;

  inUpHeld = up;

  inUpPressed   = up && !prevUp;
  inDownPressed = down && !prevDown;
  inBtnPressed  = btn && !prevBtn;

  prevUp = up;
  prevDown = down;
  prevBtn = btn;
}

void updateGame() {

  frameCounter++;
  if (stateTimer < 65000) stateTimer++;

  switch (gameState) {

    case STATE_START:

      if (inDownPressed) {
        soundOn = !soundOn;
        if (!soundOn) noTone(BUZZER_PIN);
        else playSfx(880, 60);
      }

      if (inBtnPressed || inUpPressed) startNewGame();
      break;

    case STATE_INTRO:

      if (stateTimer > 50) enterPlay();
      break;

    case STATE_PLAY:

      updateGameplay();
      break;

    case STATE_LEVEL_CLEAR:

      if (stateTimer > 40 && (inBtnPressed || inUpPressed)) {
        setupLevel(2);
      }
      break;

    case STATE_GAMEOVER:

      if (stateTimer > 40 && (inBtnPressed || inUpPressed)) {
        startNewGame();
      }
      break;

    case STATE_WIN:

      if (stateTimer > 60 && inBtnPressed) {
        gameState = STATE_START;
        stateTimer = 0;
      }
      break;
  }
}

void setup() {

  pinMode(PIN_JOY_SW, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  u8g2.setBusClock(400000);       
  u8g2.begin();
  u8g2.setContrast(180);
  u8g2.setFontMode(1);            

  loadHighScore();

  prevBtn  = (digitalRead(PIN_JOY_SW) == LOW);
  prevUp   = analogRead(PIN_JOY_Y) < JOY_UP_THRESHOLD;
  prevDown = analogRead(PIN_JOY_Y) > JOY_DOWN_THRESHOLD;

  gameState = STATE_START;
  lastFrameTime = millis();
}

void loop() {

  updateAudio();                  

  unsigned long now = millis();

  if (now - lastFrameTime < FRAME_MS) return;

  lastFrameTime = now;

  readInput();
  updateGame();
  renderGame();
}
