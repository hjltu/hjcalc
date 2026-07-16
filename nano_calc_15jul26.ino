// 2026 hjltu
// PROVIDED AS IS, WITHOUT WARRANTY OF ANY KIND

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <math.h>

// System constants
#define BUF_CLC_SIZE 64
#define BUF_CLCSTR_SIZE 64
#define BUF_PROG_SIZE 256
#define BUF_SERIAL_SIZE 96
#define ANALOG_POLL_MS 999
#define SCROLL_MS 777
#define KEY_DEBOUNCE_MS 50
#define MODE_DISPLAY_MS 800
#define STARTUP_DELAY_MS 2222
#define LOOP_MAX_DEPTH 4

// NTC Thermistor constants (10k NTC, B=3950, 25°C=298.15K)
#define NTC_R25 10000.0
#define NTC_BETA 3950.0
#define NTC_T0 298.15

// -------------------- LCD --------------------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// -------------------- Keypad --------------------
// keypad 5x5
// D2-D6 row
// D7-D11 col
char keymap[5][5] = {
  { 'A', 'M', 'X', 'Y', 'Z' },
  { '7', '8', '9', '+', '(' },
  { '4', '5', '6', '-', ')' },
  { '1', '2', '3', '*', 'B' },
  { '0', '.', '=', '/', 'C' }
};

byte myCol[5] = {2, 3, 4, 5, 6};
byte myRow[5] = {7, 8, 9, 10, 11};
Keypad myKeypad = Keypad(makeKeymap(keymap), myRow, myCol, 5, 5);

// -------------------- Global State --------------------
bool f1 = false;              // F1 (A) modifier active
bool f2 = false;              // F2 (M) modifier active
bool analogMode = false;      // Analog/NTC polling active
bool ntcMode = false;      // Analog/NTC polling active
bool degMode = false;         // true = GRAD, false = RAD
bool progMode = false;        // In PRG editing mode
bool progRunning = false;     // Program executing
bool analogPin = 0;           // 0=A0, 1=A1
int buzzPin = 12;
int digitalPin = 13;
int analogVal = 0;
float temp = 0;
bool d13state = false;
unsigned long lastAnalogRead = 0;
char lastModeCmd[8] = "";

char clc[BUF_CLC_SIZE] = "";            // Input buffer
char clcStr[BUF_CLCSTR_SIZE] = "";         // Display buffer (expanded)
int cursor = 0;               // Cursor position in clc

float xyz[3] = {0, 0, 0};     // X, Y, Z register stack
int mem[10] = {0};            // Memory slots 0-9
float lastAnsVal = 0;
unsigned long progPauseEnd = 0;  // millis() when PAUSE ends

// Program interpreter state
char progBuf[BUF_PROG_SIZE] = "";
byte progPc = 0;
byte loopStack[LOOP_MAX_DEPTH];
byte loopPc[LOOP_MAX_DEPTH];
byte loopDepth = 0;

// Serial buffer
char serialBuf[BUF_SERIAL_SIZE] = "";
int serialBufLen = 0;

// -------------------- Function Prototypes --------------------
void setup();
void loop();
void clearAll();
void backspace();
bool my_add_key(char c);
void my_char2string();
void my_check_mode();
void my_check_mode_cmd(const char* cmd);
void my_analog();
void my_ntc();
void updateDisplay();
void scrollDisplay();
void processKeypad();
void processSerial();
void handleKey(char key);
void handleF1Key(char key);
void handleF2Key(char key);
void handleNormalKey(char key);
float evaluateExpression(char* expr);
float parseExpression(char* str, int& pos);
float parseComparison(char* str, int& pos);
float parseTerm(char* str, int& pos);
float parseFactor(char* str, int& pos);
float parsePower(char* str, int& pos);
float parsePrimary(char* str, int& pos);
float parseNumber(char* str, int& pos);
void skipSpaces(char* str, int& pos);
void processSerialChar(char c);
void serialCommand(char* cmd);
void runProgram();
void executeStatement();
byte progLen();
void evaluateAndDisplay();

// ============================================================
// Main Setup & Loop
// ============================================================
void setup() {
  Serial.begin(9600);
  
  pinMode(buzzPin, OUTPUT);  // Buzzer
  pinMode(digitalPin, OUTPUT);  // D13 LED
  
  lcd.init();
  lcd.backlight();
  lcd.cursor();
  lcd.blink();
  lcd.clear();
  lcd.print("  ** hjcalc **");
  lcd.setCursor(0, 1);
  lcd.print("2026-07-16");
  delay(STARTUP_DELAY_MS);
  //lcd.clear();
  //clearAll();
  tone(buzzPin, 500, 99);
  Serial.println("hjcalc ready");
  Serial.println("Commands: rad, grad, run, stop, out, prog, <expr>");
  Serial.println("Keys: A=F1, M=F2, X/Y/Z=vars, 0-9, +,-,*,/,^,=,C,B,(,)");
}

// -------------------- Main Loop --------------------
void loop() {
  processKeypad();
  processSerial();
  
  if (analogMode) {
    my_analog();
  }
  if (ntcMode) {
    my_ntc();
  }
  
  if (progRunning && (progPauseEnd == 0 || millis() >= progPauseEnd)) {
    progPauseEnd = 0;
    runProgram();
  }
  
  if (!analogMode || ntcMode)
    updateDisplay();
}

// -------------------- Display --------------------
void updateDisplay() {
  my_char2string();
  my_check_mode();
  
  int len = strlen(clcStr);
  if (len <= 16) {
    lcd.setCursor(0, 0);
    lcd.print(clcStr);
    for (int i = len; i < 16; i++) lcd.print(' ');
    lcd.setCursor(0, 1);
    for (int i = 0; i < 16; i++) lcd.print(' ');
  } else if (len <= 32) {
    lcd.setCursor(0, 0);
    for (int i = 0; i < 16; i++) lcd.print(clcStr[i]);
    lcd.setCursor(0, 1);
    for (int i = 16; i < len; i++) lcd.print(clcStr[i]);
    for (int i = len; i < 32; i++) lcd.print(' ');
  } else {
    static int scrollPos = 0;
    static unsigned long lastScroll = 0;
    if (millis() - lastScroll > SCROLL_MS) {
      scrollPos = (scrollPos + 1) % (len - 16);
      lastScroll = millis();
    }
    lcd.setCursor(0, 0);
    for (int i = 0; i < 16; i++) lcd.print(clcStr[(scrollPos + i) % len]);
    lcd.setCursor(0, 1);
    for (int i = 16; i < 32; i++) lcd.print(clcStr[(scrollPos + i) % len]);
  }
  
  // Status line indicators
  lcd.setCursor(15, 1);
  if (f1) lcd.print('1');
  else if (f2) lcd.print('2');
  else if (analogMode) lcd.print('A');
  else if (progMode) lcd.print('P');
  else if (progRunning) lcd.print('R');
  else lcd.print('C');
}

void scrollDisplay() {
  // Handled in updateDisplay for >32 chars
}

void my_char2string() {
  clcStr[0] = '\0';
  int j = 0;
  int maxJ = BUF_CLCSTR_SIZE - 1;
  for (int i = 0; i < cursor && j < maxJ; i++) {
    char c = clc[i];
    switch (c) {
      // Functions (F1 layer) - documented tokens
      case 's': if (j + 3 < maxJ) { strcpy(clcStr + j, "sin"); j += 3; } break;
      case 'c': if (j + 3 < maxJ) { strcpy(clcStr + j, "cos"); j += 3; } break;
      case 'r': if (j + 3 < maxJ) { strcpy(clcStr + j, "cos"); j += 3; } break;  // doc: F1+0
      case 't': if (j + 2 < maxJ) { strcpy(clcStr + j, "tg"); j += 2; } break;
      case 'v': if (j + 2 < maxJ) { strcpy(clcStr + j, "tg"); j += 2; } break;  // doc: F1+2
      case 'u': if (j + 3 < maxJ) { strcpy(clcStr + j, "ctg"); j += 3; } break;
      case 'q': if (j + 4 < maxJ) { strcpy(clcStr + j, "sqrt"); j += 4; } break;
      case 'l': if (j + 2 < maxJ) { strcpy(clcStr + j, "ln"); j += 2; } break;
      case 'n': if (j + 2 < maxJ) { strcpy(clcStr + j, "ln"); j += 2; } break;  // doc: F1+9
      case 'e': if (j + 3 < maxJ) { strcpy(clcStr + j, "exp"); j += 3; } break;
      case 'o': if (j + 3 < maxJ) { strcpy(clcStr + j, "exp"); j += 3; } break;  // doc: F1+5
      case 'g': if (j + 2 < maxJ) { strcpy(clcStr + j, "lg"); j += 2; } break;
      case 'm': if (j + 2 < maxJ) { strcpy(clcStr + j, "lg"); j += 2; } break;  // doc: F1+6
      case 'p': if (j + 2 < maxJ) { strcpy(clcStr + j, "pi"); j += 2; } break;
      case '^': if (j + 1 < maxJ) { strcpy(clcStr + j, "^"); j++; } break;
      case 'A': if (j + 3 < maxJ) { strcpy(clcStr + j, "abs"); j += 3; } break;
      // Program control tokens
      case '$': if (j + 3 < maxJ) { strcpy(clcStr + j, "RUN"); j += 3; } break;
      case '@': if (j + 4 < maxJ) { strcpy(clcStr + j, "LOOP"); j += 4; } break;
      case '#': if (j + 2 < maxJ) { strcpy(clcStr + j, "IF"); j += 2; } break;
      case '&': if (j + 5 < maxJ) { strcpy(clcStr + j, "PAUSE"); j += 5; } break;
      case 'w': if (j + 4 < maxJ) { strcpy(clcStr + j, "GRAD"); j += 4; } break;
      case ':': if (j + 3 < maxJ) { strcpy(clcStr + j, "PRG"); j += 3; } break;
      case ';': if (j + 3 < maxJ) { strcpy(clcStr + j, "END"); j += 3; } break;
      // Comparison operators
      case '<': if (j + 1 < maxJ) { strcpy(clcStr + j, "<"); j++; } break;
      case '>': if (j + 1 < maxJ) { strcpy(clcStr + j, ">"); j++; } break;
      case '!': if (j + 2 < maxJ) { strcpy(clcStr + j, "!="); j += 2; } break;
      case '|': if (j + 3 < maxJ) { strcpy(clcStr + j, "BUZ"); j += 3; } break;
      // Variables X, Y, Z with values
      case 'X':
      case 'x': {
        if (j + 1 < maxJ) clcStr[j++] = 'x';
        if (c == 'X' && j + 10 < maxJ) {  // Show value for uppercase X
          char buf[16];
          dtostrf(xyz[0], 0, 4, buf);
          int len = strlen(buf);
          if (j + len + 3 < maxJ) {
            strcpy(clcStr + j, " = ");
            j += 3;
            strcpy(clcStr + j, buf);
            j += len;
          }
        }
        break;
      }
      case 'Y':
      case 'y': {
        if (j + 1 < maxJ) clcStr[j++] = 'y';
        if (c == 'Y' && j + 10 < maxJ) {
          char buf[16];
          dtostrf(xyz[1], 0, 4, buf);
          int len = strlen(buf);
          if (j + len + 3 < maxJ) {
            strcpy(clcStr + j, " = ");
            j += 3;
            strcpy(clcStr + j, buf);
            j += len;
          }
        }
        break;
      }
      case 'Z':
      case 'z': {
        if (j + 1 < maxJ) clcStr[j++] = 'z';
        if (c == 'Z' && j + 10 < maxJ) {
          char buf[16];
          dtostrf(xyz[2], 0, 4, buf);
          int len = strlen(buf);
          if (j + len + 3 < maxJ) {
            strcpy(clcStr + j, " = ");
            j += 3;
            strcpy(clcStr + j, buf);
            j += len;
          }
        }
        break;
      }
      // Memory (F2 layer) - a,b,c,d,e,f,g,h,i,j
      case 'a': if (j + 6 < maxJ) { strcpy(clcStr + j, "mem[0]"); j += 6; } break;
      case 'b': if (j + 6 < maxJ) { strcpy(clcStr + j, "mem[1]"); j += 6; } break;
      case 'C': if (j + 6 < maxJ) { strcpy(clcStr + j, "mem[2]"); j += 6; } break;
      case 'd': if (j + 6 < maxJ) { strcpy(clcStr + j, "mem[3]"); j += 6; } break;
      case 'E': if (j + 6 < maxJ) { strcpy(clcStr + j, "mem[4]"); j += 6; } break;
      case 'f': if (j + 6 < maxJ) { strcpy(clcStr + j, "mem[5]"); j += 6; } break;
      case 'G': if (j + 6 < maxJ) { strcpy(clcStr + j, "mem[6]"); j += 6; } break;
      case 'h': if (j + 2 < maxJ) { strcpy(clcStr + j, "A0"); j += 2; } break;
      case 'i': if (j + 2 < maxJ) { strcpy(clcStr + j, "A1"); j += 2; } break;
      case 'j': if (j + 3 < maxJ) { strcpy(clcStr + j, "OUT"); j += 3; } break;
      // F2 special tokens
      case '_': if (j + 1 < maxJ) { strcpy(clcStr + j, "_"); j++; } break;
      case '?': if (j + 1 < maxJ) { strcpy(clcStr + j, "?"); j++; } break;
      case '%': if (j + 1 < maxJ) { strcpy(clcStr + j, "%"); j++; } break;
      case '{': if (j + 4 < maxJ) { strcpy(clcStr + j, "PRG"); j += 4; } break;
      case '}': if (j + 3 < maxJ) { strcpy(clcStr + j, "END"); j += 3; } break;
      default: if (j < maxJ) clcStr[j++] = c; break;
    }
  }
  clcStr[j] = '\0';
}

void my_check_mode() {
  if (strstr(clcStr, "A0") && strcmp(lastModeCmd, "A0")) {
    my_check_mode_cmd("A0");
  } else if (strstr(clcStr, "A1") && strcmp(lastModeCmd, "A1")) {
    my_check_mode_cmd("A1");
  } else if (strstr(clcStr, "OUT") && strcmp(lastModeCmd, "OUT")) {
    my_check_mode_cmd("OUT");
  } else if (strstr(clcStr, "GRAD") && strcmp(lastModeCmd, "GRAD")) {
    my_check_mode_cmd("GRAD");
  } else if (strstr(clcStr, "RAD") && strcmp(lastModeCmd, "RAD")) {
    my_check_mode_cmd("RAD");
  } else if (strstr(clcStr, "rad") && strcmp(lastModeCmd, "rad")) {
    my_check_mode_cmd("rad");
  } else if (strstr(clcStr, "grad") && strcmp(lastModeCmd, "grad")) {
    my_check_mode_cmd("grad");
  } else if (strstr(clcStr, "PRG") && strcmp(lastModeCmd, "PRG")) {
    my_check_mode_cmd("prog");
  } else if (strstr(clcStr, "prog") && strcmp(lastModeCmd, "prog")) {
    my_check_mode_cmd("prog");
  }
}

void my_check_mode_cmd(const char* cmd) {
  strcpy(lastModeCmd, cmd);
  if (strcmp(cmd, "A0") == 0) {
    analogPin = 0;
    analogMode = true;
    lastAnalogRead = 0;
    my_analog();
  } else if (strcmp(cmd, "A1") == 0) {
    analogPin = 1;
    ntcMode = true;
    lastAnalogRead = 0;
    my_ntc();
  } else if (strcmp(cmd, "OUT") == 0) {
    d13state = !d13state;
    lcd.clear();
    lcd.print("OUT="); lcd.print(d13state, 1);
    digitalWrite(digitalPin, d13state);
    delay(MODE_DISPLAY_MS);
    //clearAll();
  } else if (strcmp(cmd, "GRAD") == 0) {
    degMode = true;
    lcd.clear();
    lcd.print("GRAD mode");
    delay(MODE_DISPLAY_MS);
    clearAll();
  } else if (strcmp(cmd, "RAD") == 0) {
    degMode = false;
    lcd.clear();
    lcd.print("RAD mode");
    delay(MODE_DISPLAY_MS);
    clearAll();
  } else if (strcmp(cmd, "rad") == 0) {
    degMode = false;
    lcd.clear();
    lcd.print("RAD mode");
    delay(MODE_DISPLAY_MS);
    clearAll();
  } else if (strcmp(cmd, "grad") == 0) {
    degMode = true;
    lcd.clear();
    lcd.print("GRAD mode");
    delay(MODE_DISPLAY_MS);
    clearAll();
  } else if (strcmp(cmd, "prog") == 0) {
    progMode = true;
    progBuf[0] = '\0';
    progPc = 0;
    lcd.clear();
    lcd.print("PRG mode");
    delay(MODE_DISPLAY_MS);
    clearAll();
  }
}

// -------------------- Keypad Processing --------------------
void processKeypad() {
  static unsigned long lastKeyTime = 0;
  char key = myKeypad.getKey();
  if (key && (millis() - lastKeyTime > KEY_DEBOUNCE_MS)) {
    lastKeyTime = millis();
    handleKey(key);
  }
}

void handleKey(char key) {
  if (f1) {
    handleF1Key(key);
    f1 = false;
    return;
  }
  if (f2) {
    handleF2Key(key);
    f2 = false;
    return;
  }
  handleNormalKey(key);
}

void handleNormalKey(char key) {
  if (key != 'A' && key != 'M') {
    analogMode = false;  // Exit analog mode on any keypress
    ntcMode = false;  // Exit ntc mode on any keypress
  }
  switch (key) {
    case 'A': f1 = true; break;
    case 'M': f2 = true; break;
    case 'C': clearAll(); break;
    case 'B': backspace(); break;
    case '=': evaluateAndDisplay(); break;
    default: my_add_key(key); break;
  }
}

void handleF1Key(char key) {
  switch (key) {
    case 'X': my_add_key('$'); break;  // RUN
    case 'Y': my_add_key('<'); break;  // IF <
    case 'Z': my_add_key('>'); break;  // IF >
    case '0': my_add_key('r'); break;  // cos
    case '1': my_add_key('s'); break;  // sin
    case '2': my_add_key('v'); break;  // tg
    case '3': my_add_key('u'); break;  // ctg
    case '4': my_add_key('p'); break;  // pi
    case '5': my_add_key('o'); break;  // exp
    case '6': my_add_key('m'); break;  // lg
    case '7': my_add_key('q'); break;  // sqrt
    case '8': my_add_key('^'); break;  // ^
    case '9': my_add_key('n'); break;  // ln
    case '.': my_add_key(','); break;  // reserved
    case '=': my_add_key('~'); break;  // reserved
    case '+': my_add_key('@'); break;  // LOOP
    case '-': my_add_key('#'); break;  // IF
    case '*': my_add_key('&'); break;  // PAUSE
    case '/': my_add_key('w'); break;  // GRAD
    case '(': my_add_key(':'); break;  // PRG
    case ')': my_add_key(';'); break;  // END
    case 'M': my_check_mode_cmd("rad"); break;
    case 'C': my_check_mode_cmd("grad"); break;
    case 'B': my_check_mode_cmd("prog"); break;
    default: my_add_key(key); break;
  }
}

void handleF2Key(char key) {
  switch (key) {
    case 'X': my_add_key('x'); break;  // Variable X
    case 'Y': my_add_key('y'); break;  // Variable Y
    case 'Z': my_add_key('z'); break;  // Variable Z
    case '0': my_add_key('a'); break;  // mem[0]
    case '1': my_add_key('b'); break;  // mem[1]
    case '2': my_add_key('C'); break;  // mem[2]
    case '3': my_add_key('d'); break;  // mem[3]
    case '4': my_add_key('E'); break;  // mem[4]
    case '5': my_add_key('f'); break;  // mem[5]
    case '6': my_add_key('G'); break;  // mem[6]
    case '7': my_add_key('h'); break;  // A0
    case '8': my_add_key('i'); break;  // A1
    case '9': my_add_key('j'); break;  // OUT
    case '.': my_add_key(' '); break;  // Space
    case '=': my_add_key('!'); break;  // !=
    case '+': my_add_key('|'); break;  // BUZ
    case '-': my_add_key('_'); break;  // reserved
    case '*': my_add_key('?'); break;  // reserved
    case '/': my_add_key('%'); break;  // reserved
    case '(': my_add_key('{'); break;  // Programm mode
    case ')': my_add_key('}'); break;  // Programm mode
    case 'M': analogMode = false; break;  // Exit analog
    case 'C': progMode = !progMode; if (progMode) { progBuf[0] = '\0'; progPc = 0; } break;
    case 'B': if (progMode) { progRunning = true; progPc = 0; loopDepth = 0; progPauseEnd = 0; } else { progRunning = !progRunning; progPc = 0; } break;
    case 'A': d13state = !d13state; digitalWrite(13, d13state); break;
    default: my_add_key(key); break;
  }
}

bool my_add_key(char c) {
  if (cursor >= 63) return false;
  
  // Operator normalization per docs §5.5
  if (cursor > 0) {
    char prev = clc[cursor - 1];
    if (strchr("+-*/^", c) && strchr("+-*/^(", prev)) {
      // Keep unary sequences: /-, *-, +-, -+, -- (Doc §5.5)
      if ((prev == '/' && c == '-') ||
          (prev == '*' && c == '-') ||
          (prev == '+' && c == '-') ||
          (prev == '-' && c == '+') ||
          (prev == '-' && c == '-')) {
        // Keep both operators
      } else {
        // Reduce other double operators to single
        // */ -> *, /+ -> /, -* -> -, *+ -> *, // -> /, ** -> *
        clc[cursor - 1] = c;
        return true;
      }
    }
  }
  
  clc[cursor++] = c;
  clc[cursor] = '\0';
  return true;
}

void clearAll() {
  clc[0] = '\0';
  clcStr[0] = '\0';
  cursor = 0;
  analogMode = false;
  ntcMode = false;
  //d13state = false;
  //digitalWrite(13, LOW);
  lastAnsVal = 0;
  tone(buzzPin, 300, 99);
}

void backspace() {
  if (cursor > 0) {
    cursor--;
    clc[cursor] = '\0';
  }
  tone(buzzPin, 300, 99);
}

void evaluateAndDisplay() {
  float result = evaluateExpression(clc);
  if (isnan(result)) {
    clc[0] = '\0';
    cursor = 0;
    strcpy(clcStr, "Error");
    tone(buzzPin, 500, 99);
  } else {
    lastAnsVal = result;
    xyz[2] = xyz[1];
    xyz[1] = xyz[0];
    xyz[0] = result;
    // Append =result to input buffer for display
    char resultStr[16];
    dtostrf(result, 0, 4, resultStr);
    int len = strlen(clc);
    if (len + 1 + strlen(resultStr) < 63) {
      clc[len] = '=';
      strcpy(clc + len + 1, resultStr);
      cursor = strlen(clc);
    }
  }
  my_char2string();
}

// -------------------- Expression Parser --------------------
float evaluateExpression(char* expr) {
  int pos = 0;
  skipSpaces(expr, pos);
  if (expr[pos] == '\0') return NAN;
  float result = parseExpression(expr, pos);
  skipSpaces(expr, pos);
  if (expr[pos] != '\0') return NAN;
  return result;
}

void skipSpaces(char* str, int& pos) {
  while (str[pos] == ' ') pos++;
}

float parseExpression(char* str, int& pos) {
  float left = parseComparison(str, pos);
  skipSpaces(str, pos);
  while (str[pos] == '+' || str[pos] == '-') {
    char op = str[pos++];
    float right = parseComparison(str, pos);
    if (op == '+') left += right;
    else left -= right;
    skipSpaces(str, pos);
  }
  return left;
}

float parseComparison(char* str, int& pos) {
  float left = parseTerm(str, pos);
  skipSpaces(str, pos);
  while (true) {
    if (str[pos] == '<' && str[pos+1] == '=') { pos += 2; float right = parseTerm(str, pos); left = (left <= right) ? 1.0 : 0.0; }
    else if (str[pos] == '>' && str[pos+1] == '=') { pos += 2; float right = parseTerm(str, pos); left = (left >= right) ? 1.0 : 0.0; }
    else if (str[pos] == '=' && str[pos+1] == '=') { pos += 2; float right = parseTerm(str, pos); left = (left == right) ? 1.0 : 0.0; }
    else if (str[pos] == '!' && str[pos+1] == '=') { pos += 2; float right = parseTerm(str, pos); left = (left != right) ? 1.0 : 0.0; }
    else if (str[pos] == '<') { pos++; float right = parseTerm(str, pos); left = (left < right) ? 1.0 : 0.0; }
    else if (str[pos] == '>') { pos++; float right = parseTerm(str, pos); left = (left > right) ? 1.0 : 0.0; }
    else break;
    skipSpaces(str, pos);
  }
  return left;
}

float parseTerm(char* str, int& pos) {
  float left = parseFactor(str, pos);
  skipSpaces(str, pos);
  while (str[pos] == '*' || str[pos] == '/') {
    char op = str[pos++];
    float right = parseFactor(str, pos);
    if (op == '*') left *= right;
    else {
      if (right == 0) return NAN;
      left /= right;
    }
    skipSpaces(str, pos);
  }
  return left;
}

float parseFactor(char* str, int& pos) {
  float left = parsePower(str, pos);
  skipSpaces(str, pos);
  if (str[pos] == '^') {
    pos++;
    float right = parseFactor(str, pos);  // Right-associative
    left = pow(left, right);
  }
  return left;
}

float parsePower(char* str, int& pos) {
  return parsePrimary(str, pos);
}

float parsePrimary(char* str, int& pos) {
  skipSpaces(str, pos);
  char c = str[pos];
  
  // Functions (single-char tokens from keypad)
  if (c == 's') { pos++; float arg = parsePrimary(str, pos); return degMode ? sin(arg * M_PI / 200.0) : sin(arg); }
  if (c == 'c') { pos++; float arg = parsePrimary(str, pos); return degMode ? cos(arg * M_PI / 200.0) : cos(arg); }
  if (c == 'r') { pos++; float arg = parsePrimary(str, pos); return degMode ? cos(arg * M_PI / 200.0) : cos(arg); }
  if (c == 't') { pos++; float arg = parsePrimary(str, pos); float v = degMode ? tan(arg * M_PI / 200.0) : tan(arg); return v; }
  if (c == 'v') { pos++; float arg = parsePrimary(str, pos); float v = degMode ? tan(arg * M_PI / 200.0) : tan(arg); return v; }
  if (c == 'u') { pos++; float arg = parsePrimary(str, pos); float v = degMode ? tan(arg * M_PI / 200.0) : tan(arg); return (v == 0) ? NAN : 1.0 / v; }
  if (c == 'q') { pos++; float arg = parsePrimary(str, pos); return (arg < 0) ? NAN : sqrt(arg); }
  if (c == 'l') { pos++; float arg = parsePrimary(str, pos); return (arg <= 0) ? NAN : log(arg); }
  if (c == 'n') { pos++; float arg = parsePrimary(str, pos); return (arg <= 0) ? NAN : log(arg); }
  if (c == 'e') { pos++; float arg = parsePrimary(str, pos); return exp(arg); }
  if (c == 'o') { pos++; float arg = parsePrimary(str, pos); return exp(arg); }
  if (c == 'g') { pos++; float arg = parsePrimary(str, pos); return (arg <= 0) ? NAN : log10(arg); }
  if (c == 'm') { pos++; float arg = parsePrimary(str, pos); return (arg <= 0) ? NAN : log10(arg); }
  if (c == 'A') { pos++; float arg = parsePrimary(str, pos); return fabs(arg); }
  
  // Constants and variables
  if (c == 'p') { pos++; return M_PI; }
  if (c == 'x') { pos++; return xyz[0]; }
  if (c == 'y') { pos++; return xyz[1]; }
  if (c == 'z') { pos++; return xyz[2]; }
  if (c >= 'a' && c <= 'j') { pos++; return mem[c - 'a']; }
  if (c == 'C') { pos++; return mem[2]; }
  if (c == 'E') { pos++; return mem[4]; }
  if (c == 'G') { pos++; return mem[6]; }
  
  // Parentheses
  if (c == '(') {
    pos++;
    float val = parseExpression(str, pos);
    skipSpaces(str, pos);
    if (str[pos] == ')') pos++;
    return val;
  }
  
  // Number
  return parseNumber(str, pos);
}

float parseNumber(char* str, int& pos) {
  skipSpaces(str, pos);
  bool neg = false;
  if (str[pos] == '-') { neg = true; pos++; }
  else if (str[pos] == '+') { pos++; }
  
  float val = 0;
  bool hasDigits = false;
  while (isdigit(str[pos])) {
    val = val * 10 + (str[pos] - '0');
    pos++;
    hasDigits = true;
  }
  
  if (str[pos] == '.') {
    pos++;
    float frac = 0;
    float div = 10;
    while (isdigit(str[pos])) {
      frac += (str[pos] - '0') / div;
      div *= 10;
      pos++;
    }
    val += frac;
  }
  
  if (!hasDigits && str[pos - 1] != '.') return NAN;
  return neg ? -val : val;
}

// -------------------- A0 Read --------------------
void my_analog() {
  analogVal = analogRead(analogPin);
  
  lcd.clear();
  lcd.print("A0 = ");
  lcd.print(analogVal);
  lcd.setCursor(0, 1);
  lcd.print("0 - 1023");
  
  Serial.print("A0 = ");
  Serial.print(analogVal, 2);
  delay(ANALOG_POLL_MS);
  // analogMode stays true for continuous polling; cleared by keypress
}

// -------------------- NTC Thermistor TODO--------------------
void my_ntc() {
  analogVal = analogRead(analogPin);
  float v = analogVal * 5.0 / 1023.0;
  float r = NTC_R25 * v / (5.0 - v);
  temp = 1.0 / (1.0 / NTC_T0 + log(r / NTC_R25) / NTC_BETA) - 273.15;
  
  lcd.clear();
  lcd.print("temp = ");
  lcd.print(temp, 2);
  lcd.print(" C");
  lcd.setCursor(0, 1);
  lcd.print("NTC 10kOm");
  
  Serial.print("temp = ");
  Serial.print(temp, 2);
  Serial.println(" C  NTC 10kOm");
  delay(ANALOG_POLL_MS);
  // analogMode stays true for continuous polling; cleared by keypress
}

// -------------------- Serial Processing --------------------
void processSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    processSerialChar(c);
  }
}

void processSerialChar(char c) {
  if (c == '\n' || c == '\r') {
    if (serialBufLen > 0) {
      serialBuf[serialBufLen] = '\0';
      serialCommand(serialBuf);
      serialBufLen = 0;
    }
  } else if (serialBufLen < 95) {
    serialBuf[serialBufLen++] = c;
  }
}

void serialCommand(char* cmd) {
  // Trim
  while (serialBufLen > 0 && (cmd[serialBufLen - 1] == '\n' || cmd[serialBufLen - 1] == '\r' || cmd[serialBufLen - 1] == ' ')) {
    cmd[--serialBufLen] = '\0';
  }
  
  if (strcmp(cmd, "rad") == 0) {
    degMode = false;
    Serial.println("RAD mode");
  } else if (strcmp(cmd, "grad") == 0) {
    degMode = true;
    Serial.println("GRAD mode");
  } else if (strcmp(cmd, "run") == 0) {
    progRunning = true;
    progPc = 0;
    loopDepth = 0;
    progPauseEnd = 0;
    Serial.println("RUNNING");
  } else if (strcmp(cmd, "stop") == 0) {
    progRunning = false;
    progPauseEnd = 0;
    Serial.println("STOPPED");
  } else if (strcmp(cmd, "out") == 0) {
    d13state = !d13state;
    digitalWrite(13, d13state);
    Serial.print("OUT ");
    Serial.println(d13state ? "HIGH" : "LOW");
  } else if (strcmp(cmd, "a0") == 0) {
    analogPin = 0;
    analogMode = true;
    lastAnalogRead = 0;
    Serial.println("A0 mode");
  } else if (strcmp(cmd, "a1") == 0) {
    analogPin = 1;
    analogMode = true;
    lastAnalogRead = 0;
    Serial.println("A1 NTC mode");
  } else if (strcmp(cmd, "prog") == 0) {
    progMode = true;
    progBuf[0] = '\0';
    progPc = 0;
    Serial.println("PRG mode");
  } else if (strncmp(cmd, "load ", 5) == 0) {
    strncpy(progBuf, cmd + 5, BUF_PROG_SIZE - 1);
    progBuf[BUF_PROG_SIZE - 1] = '\0';
    Serial.println("Program loaded");
  } else {
    // Treat as expression
    float result = evaluateExpression(cmd);
    if (!isnan(result)) {
      lastAnsVal = result;
      xyz[2] = xyz[1];
      xyz[1] = xyz[0];
      xyz[0] = result;
      Serial.print("= ");
      Serial.println(result, 4);
      strcpy(clc, cmd);
      cursor = strlen(clc);
      evaluateAndDisplay();
    } else {
      Serial.println("Error");
    }
  }
}

// -------------------- Program Interpreter --------------------
byte progLen() {
  return strlen(progBuf);
}

void runProgram() {
  if (progPc >= progLen()) return;
  
  if (progPauseEnd > 0) {
    if (millis() < progPauseEnd) return;
    progPauseEnd = 0;
  }
  
  executeStatement();
}

void executeStatement() {
  // Skip whitespace
  while (progPc < progLen() && progBuf[progPc] == ' ') progPc++;
  if (progPc >= progLen()) return;
  
  char* start = progBuf + progPc;
  
  // Find end of statement (semicolon or newline or end)
  char* end = start;
  while (*end && *end != ';' && *end != '\n') end++;
  
  int len = end - start;
  if (len == 0) { progPc = end - progBuf + 1; return; }
  
  char stmt[64];
  strncpy(stmt, start, len);
  stmt[len] = '\0';
  
  // Trim
  char* p = stmt;
  while (*p == ' ') p++;
  char* q = p + strlen(p) - 1;
  while (q > p && (*q == ' ' || *q == '\r')) *q-- = '\0';
  
  // Parse statement
  if (strncmp(p, "PAUSE", 5) == 0) {
    char* pauseVal = p + 5;
    while (*pauseVal == ' ' || *pauseVal == '=') pauseVal++;
    progPauseEnd = millis() + atoi(pauseVal) * 100;  // centiseconds -> milliseconds
  } else if (strncmp(p, "OUT", 3) == 0) {
    Serial.print(F("OUT=")); Serial.println(d13state, 1);
    lcd.clear();
    lcd.print("OUT="); lcd.print(d13state, 1);
    digitalWrite(digitalPin, d13state);
    progPauseEnd = millis() + 2000;  // Show for 2 seconds
  } else if (strncmp(p, "A0", 2) == 0) {
    xyz[0] = analogRead(A0) * 5.0 / 1023.0;
  } else if (strncmp(p, "A1", 2) == 0) {
    xyz[0] = analogRead(A1) * 5.0 / 1023.0;
  } else if (strncmp(p, "LIGHT", 5) == 0) {
    d13state = !d13state;
    digitalWrite(13, d13state);
  } else if (strncmp(p, "BLINK", 5) == 0) {
    for (int i = 0; i < 3; i++) {
      digitalWrite(13, HIGH); delay(100);
      digitalWrite(13, LOW); delay(100);
    }
  } else if (strncmp(p, "LOOP", 4) == 0) {
    if (loopDepth < LOOP_MAX_DEPTH) {
      int count = atoi(p + 4);
      if (count > 0) {
        loopStack[loopDepth] = count;
        loopPc[loopDepth] = end - progBuf + 1;  // Save PC after LOOP statement (first body statement)
        loopDepth++;
      }
    }
  } else if (strncmp(p, "END", 3) == 0) {
    if (loopDepth > 0) {
      loopDepth--;
      if (loopStack[loopDepth] > 1) {
        loopStack[loopDepth]--;
        progPc = loopPc[loopDepth];
        loopDepth++;  // Restore depth for next iteration
        return;  // Don't advance progPc - we jumped back
      }
    }
  } else if (strncmp(p, "IF", 2) == 0) {
    // IF condition THEN stmt - copy condition to avoid corrupting buffer
    char* thenPtr = strstr(p, "THEN");
    if (thenPtr) {
      char cond[32];
      int condLen = thenPtr - (p + 2);
      if (condLen > 31) condLen = 31;
      strncpy(cond, p + 2, condLen);
      cond[condLen] = '\0';
      float condVal = evaluateExpression(cond);
      if (condVal != 0) {
        // Execute THEN statement (simple assignment only)
        char* thenStmt = thenPtr + 4;
        while (*thenStmt == ' ') thenStmt++;
        if (strchr(thenStmt, '=')) {
          char* eq = strchr(thenStmt, '=');
          *eq = '\0';
          char var = thenStmt[0];
          float val = evaluateExpression(eq + 1);
          if (var == 'X') xyz[0] = val;
          else if (var == 'Y') xyz[1] = val;
          else if (var == 'Z') xyz[2] = val;
          else if (var >= 'A' && var <= 'J') mem[var - 'A'] = (int)val;
        }
      }
    }
  } else if (strchr(p, '=')) {
    // Assignment
    char* eq = strchr(p, '=');
    *eq = '\0';
    char var = p[0];
    float val = evaluateExpression(eq + 1);
    if (var == 'X') xyz[0] = val;
    else if (var == 'Y') xyz[1] = val;
    else if (var == 'Z') xyz[2] = val;
    else if (var >= 'A' && var <= 'J') mem[var - 'A'] = (int)val;
  } else {
    // Bare expression
    float val = evaluateExpression(p);
    if (!isnan(val)) {
      xyz[2] = xyz[1];
      xyz[1] = xyz[0];
      xyz[0] = val;
      lastAnsVal = val;
    }
  }
  
  progPc = end - progBuf + 1;
  
  if (progPc >= progLen()) {
    progRunning = false;
    Serial.println("DONE");
    lcd.clear();
    lcd.print("DONE");
    delay(1000);
  }
}

// ============================================================
// End of sketch
// ============================================================
