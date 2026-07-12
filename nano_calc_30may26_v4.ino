// 2019 hjltu@ya.ru
// PROVIDED AS IS, WITHOUT WARRANTY OF ANY KIND

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <math.h>

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

// -------------------- State --------------------
bool f1 = false;
bool f2 = false;
bool light = false;
bool blinkMode = true;
bool analogMode = false;
bool degMode = true;      // default DEG
bool outD13 = false;

char clc[64];
int cursor = 0;

char serialBuf[96];
byte serialPos = 0;

float xyz[3] = {0, 0, 0};
int mem[10] = {0};
int analogPin = A0;
int analogVal = 0;
float temp = 0.0;
float lastAns = 0.0;

// For mode re-trigger prevention
char lastModeCmd[8] = "";

// -------------------- UI Strings --------------------
char clcStr[64] = " ;)  hjcalc  |8  :O   XYZ    (:";

// -------------------- Helpers --------------------
void my_print(const char *arg);
void my_char2string();
void my_check_mode();
bool my_add_key(char b);
void my_solve_equation();
void clearAll();
void backspace();
void serialCommand(char *cmd);
void processSerialChar(char c);
float parseExpression(const char *&p);
float parseTerm(const char *&p);
float parseFactor(const char *&p);
float parseNumber(const char *&p);
void skipSpaces(const char *&p);
void my_ntc();

int set_cursor(int cur) {
  return (cur > 31) ? (cur - 31) : 0;
}

void clearAll() {
  cursor = 0;
  clc[0] = '\0';
  analogMode = false;
  digitalWrite(13, LOW);
  my_print("CX");
}

void backspace() {
  if (cursor > 0) {
    cursor--;
    clc[cursor] = '\0';
  }
  my_print("BS");
}

// -------------------- Setup --------------------
void setup() {
  Serial.begin(9600);
  Serial.println(F("Start"));

  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);
  tone(12, 999, 99);

  lcd.init();
  lcd.cursor();
  lcd.blink();
  lcd.backlight();

  lcd.setCursor(0, 0);
  my_print("setup");

  delay(699);
}

// -------------------- Loop --------------------
void loop() {
  byte key = myKeypad.getKey();
  if (key) event(key);

  while (Serial.available()) {
    processSerialChar((char)Serial.read());
  }

  if (millis() % 999 == 1) {
    if (analogMode) {
      if (analogPin == A1) {
        analogVal = 0;
        int x = 8;
        for (int i = 0; i < x; i++) {
          analogVal += analogRead(analogPin);
          delay(x);
        }
        analogVal /= x;
        my_ntc();
        my_print("ntc");
        Serial.println(temp);
      } else if (analogPin == A0) {
        analogVal = analogRead(analogPin);
        my_print("analog");
        Serial.println(analogVal);
      }
    }
  }
}

// -------------------- Event --------------------
void event(char k) {
  switch (k) {
    case 'A':   // F1 Alt
      f1 = true;
      f2 = false;
      my_print("F1");
      tone(12, 200, 99);
      return;

    case 'M':   // F2 Mem
      f1 = false;
      f2 = true;
      my_print("F2");
      tone(12, 250, 99);
      return;

    case 'B':   // BS
      if (!f1 && !f2) {
        backspace();
        tone(12, 300, 99);
      }
      return;

    case 'C':   // CX = clear all
      if (!f1 && !f2) {
        clearAll();
        tone(12, 450, 99);
      }
      return;

    case '=':
      if (my_add_key(k)) cursor++;
      my_solve_equation();
      tone(12, 600, 99);
      return;

    default:
      if (my_add_key(k)) cursor++;
      char keyStr[2] = {k, 0}; my_print(keyStr);
      return;
  }
}

// -------------------- Display --------------------
void my_print(const char *arg) {
  if (strcmp(arg, "setup") != 0) {
    my_char2string();
    my_check_mode();
  }

  if (strcmp(arg, "solve") == 0) {
    char ansStr[16];
    dtostrf(lastAns, 1, 4, ansStr);
    strncat(clcStr, ansStr, sizeof(clcStr) - strlen(clcStr) - 1);
  }

  Serial.print(arg); Serial.print(" ");
  Serial.println(clcStr);

  if (strcmp(arg, "analog") == 0) {
    snprintf(clcStr, sizeof(clcStr), "A0 = %d           0 - 1023", analogVal);
  } else if (strcmp(arg, "ntc") == 0) {
    char ntc[10];
    dtostrf(temp, 5, 2, ntc);
    snprintf(clcStr, sizeof(clcStr), "temp = %s      NTC 10kOm", ntc);
  } else if (strcmp(arg, "out") == 0) {
    snprintf(clcStr, sizeof(clcStr), "D13 = %d", digitalRead(13));
  } else if (strcmp(arg, "deg") == 0) {
    strcpy(clcStr, "Mode = GRAD");
  } else if (strcmp(arg, "rad") == 0) {
    strcpy(clcStr, "Mode = RAD");
  }

  lcd.clear();
  int begin = set_cursor(strlen(clcStr));
  char line1[17];
  char line2[17];
  strncpy(line1, clcStr + begin, 16);
  line1[16] = '\0';
  lcd.setCursor(0, 0);
  lcd.print(line1);
  if (strlen(clcStr) > begin + 16) {
    strncpy(line2, clcStr + begin + 16, 16);
    line2[16] = '\0';
    lcd.setCursor(0, 1);
    lcd.print(line2);
  }
}

// -------------------- Expression Parser --------------------
void skipSpaces(const char *&p) {
  while (*p == ' ') p++;
}

float parseNumber(const char *&p) {
  skipSpaces(p);
  char buf[24];
  byte i = 0;
  bool hasDot = false;

  if (*p == '+' || *p == '-') buf[i++] = *p++;

  while ((*p >= '0' && *p <= '9') || *p == '.') {
    if (*p == '.') {
      if (hasDot) break;  // Stop parsing number at second dot
      hasDot = true;
    }
    if (i < sizeof(buf) - 1) buf[i++] = *p;
    p++;
  }
  buf[i] = '\0';
  return atof(buf);
}

float parseFactor(const char *&p) {
  skipSpaces(p);

  if (*p == '+') {
    p++;
    return parseFactor(p);
  }

  if (*p == '-') {
    p++;
    return -parseFactor(p);
  }

  if (*p == '(') {
    p++;
    float v = parseExpression(p);
    if (*p == ')') p++;
    return v;
  }

  if (*p == 'X') return xyz[0];
  if (*p == 'Y') return xyz[1];
  if (*p == 'Z') return xyz[2];
  if (*p == 'x') { p++; return xyz[0]; }
  if (*p == 'y') { p++; return xyz[1]; }
  if (*p == 'z') { p++; return xyz[2]; }
  if (*p == 'a') return mem[0];
  if (*p == 'b') return mem[1];
  if (*p == 'c') return mem[2];
  if (*p == 'd') return mem[3];
  if (*p == 'e') return mem[4];
  if (*p == 'f') return mem[5];
  if (*p == '$') return lastAns;

  if (*p == 'p') { p++; return PI; }   // pi
  if (*p == 'r') {                     // cos
    p++;
    float x = parseFactor(p);
    if (degMode) x = x * PI / 180.0;
    return cos(x);
  }
  if (*p == 's') {                     // sin
    p++;
    float x = parseFactor(p);
    if (degMode) x = x * PI / 180.0;
    return sin(x);
  }
  if (*p == 'v') {                     // tg
    p++;
    float x = parseFactor(p);
    if (degMode) x = x * PI / 180.0;
    return tan(x);
  }
  if (*p == 'u') {                     // ctg
    p++;
    float x = parseFactor(p);
    if (degMode) x = x * PI / 180.0;
    return 1.0 / tan(x);
  }
  if (*p == 'q') {                     // sqrt
    p++;
    return sqrt(parseFactor(p));
  }
  if (*p == 'n') {                     // ln
    p++;
    return log(parseFactor(p));
  }
  if (*p == 'o') {                     // exp
    p++;
    return exp(parseFactor(p));
  }
  if (*p == 'm') {                     // lg
    p++;
    return log10(parseFactor(p));
  }
  if (*p == '"') {                     // abs
    p++;
    return fabs(parseFactor(p));
  }

  float v = parseNumber(p);

  return v;
}

float parseTerm(const char *&p) {
  float v = parseFactor(p);

  while (true) {
    skipSpaces(p);

    if (*p == '*') {
      p++;
      v *= parseFactor(p);
    } else if (*p == '/') {
      p++;
      v /= parseFactor(p);
    } else if (*p == '%') {
      p++;
      v = fmod(v, parseFactor(p));
    } else {
      break;
    }
  }
  return v;
}

float parseExpression(const char *&p) {
  float v = parseTerm(p);

  while (true) {
    skipSpaces(p);

    if (*p == '+') {
      p++;
      v += parseTerm(p);
    } else if (*p == '-') {
      p++;
      v -= parseTerm(p);
    } else {
      break;
    }
  }
  return v;
}

// -------------------- Solve --------------------
void my_solve_equation() {
  clc[cursor] = '\0';

  const char *p = clc;
  lastAns = parseExpression(p);

  xyz[2] = xyz[1];
  xyz[1] = xyz[0];
  xyz[0] = lastAns;

  my_print("solve");
}

// -------------------- Modes --------------------
void my_check_mode() {
  // Only trigger on new command (prevent re-trigger on redraw)
  if (strncmp(clcStr, "LIGHT", 5) == 0 && strcmp(lastModeCmd, "LIGHT") != 0) {
    light = !light;
    if (light) lcd.backlight();
    else lcd.noBacklight();
    strcpy(lastModeCmd, "LIGHT");
    Serial.println("li");
  }
  else if (strncmp(clcStr, "BLINK", 5) == 0 && strcmp(lastModeCmd, "BLINK") != 0) {
    blinkMode = !blinkMode;
    if (blinkMode) lcd.blink();
    else lcd.noBlink();
    strcpy(lastModeCmd, "BLINK");
    Serial.println("bl");
  }
  else if (strncmp(clcStr, "A0", 2) == 0 && strcmp(lastModeCmd, "A0") != 0) {
    analogMode = true;
    analogPin = A0;
    strcpy(lastModeCmd, "A0");
  }
  else if (strncmp(clcStr, "A1", 2) == 0 && strcmp(lastModeCmd, "A1") != 0) {
    analogMode = true;
    analogPin = A1;
    strcpy(lastModeCmd, "A1");
  }
  else if (strncmp(clcStr, "OUT", 3) == 0) {
    digitalWrite(13, !digitalRead(13));
    cursor = 0;
    clc[0] = '\0';
    my_print("out");
  }
  else if (strncmp(clcStr, "GRAD", 4) == 0) {
    degMode = !degMode;  // Toggle GRAD <-> RAD
    cursor = 0;
    clc[0] = '\0';
    Serial.println(degMode ? "deg" : "rad");
  }
  else if (strncmp(clcStr, "RAD", 3) == 0) {
    degMode = false;
    cursor = 0;
    clc[0] = '\0';
    Serial.println("rad");
  }
  // Reset lastModeCmd when user starts typing new expression
  else if (cursor > 0 && (clc[cursor-1] >= '0' && clc[cursor-1] <= '9' || clc[cursor-1] == '+' || clc[cursor-1] == '-' || clc[cursor-1] == '*' || clc[cursor-1] == '/' || clc[cursor-1] == '(' || clc[cursor-1] == ')')) {
    lastModeCmd[0] = '\0';
  }
}

// -------------------- Render buffer --------------------
void my_char2string() {
  clcStr[0] = '\0';

  for (int i = 0; i < cursor; i++) {
    switch (clc[i]) {
      case 'X': {
        char buf[16];
        strcpy(buf, "X = ");
        dtostrf(xyz[0], 1, 2, buf + strlen(buf));
        strcat(clcStr, buf);
        break;
      }
      case 'Y': {
        char buf[16];
        strcpy(buf, "Y = ");
        dtostrf(xyz[1], 1, 2, buf + strlen(buf));
        strcat(clcStr, buf);
        break;
      }
      case 'Z': {
        char buf[16];
        strcpy(buf, "Z = ");
        dtostrf(xyz[2], 1, 2, buf + strlen(buf));
        strcat(clcStr, buf);
        break;
      }

      case 'a': {
        char buf[8];
        itoa(mem[0], buf, 10);
        strcat(clcStr, buf);
        break;
      }
      case 'b': {
        char buf[8];
        itoa(mem[1], buf, 10);
        strcat(clcStr, buf);
        break;
      }
      case 'c': {
        char buf[8];
        itoa(mem[2], buf, 10);
        strcat(clcStr, buf);
        break;
      }
      case 'd': {
        char buf[8];
        itoa(mem[3], buf, 10);
        strcat(clcStr, buf);
        break;
      }
      case 'e': {
        char buf[8];
        itoa(mem[4], buf, 10);
        strcat(clcStr, buf);
        break;
      }
      case 'f': {
        char buf[8];
        itoa(mem[5], buf, 10);
        strcat(clcStr, buf);
        break;
      }

      case 'g': strcat(clcStr, "IN"); break;
      case 'h': strcat(clcStr, "A0"); break;
      case 'i': strcat(clcStr, "A1"); break;
      case 'j': strcat(clcStr, "OUT"); break;
      case '$': strcat(clcStr, "RUN"); break;

      case 'r': strcat(clcStr, "cos"); break;
      case 's': strcat(clcStr, "sin"); break;
      case 'v': strcat(clcStr, "tg"); break;
      case 'u': strcat(clcStr, "ctg"); break;
      case 'p': strcat(clcStr, "pi"); break;
      case 'o': strcat(clcStr, "exp"); break;
      case 'm': strcat(clcStr, "lg"); break;
      case 'q': strcat(clcStr, "sqrt"); break;
      case 'n': strcat(clcStr, "ln"); break;
      case '@': strcat(clcStr, "LOOP"); break;
      case '#': strcat(clcStr, "IF"); break;
      case '&': strcat(clcStr, "PAUSE"); break;
      case 'w': strcat(clcStr, "GRAD"); break;
      case ':': strcat(clcStr, "PRG"); break;
      case ';': strcat(clcStr, "END"); break;
      case '"': strcat(clcStr, "abs"); break;
      case 'l': strcat(clcStr, "LIGHT"); break;
      case '!': strcat(clcStr, "!="); break;
      case '|': strcat(clcStr, "BUZ"); break;
      case '_': strcat(clcStr, "OUT"); break;
      case '`': strcat(clcStr, "factor"); break;
      case 'k': strcat(clcStr, "BLINK"); break;

      default: {
        char c[2] = {clc[i], '\0'};
        strcat(clcStr, c);
        break;
      }
    }
  }
}

// -------------------- Key mapping --------------------
bool my_add_key(char b) {
  // Operator normalization for invalid sequences (2.9)
  // Check if new key is an operator and last char is also an operator
  if (!f1 && !f2 && cursor > 0 && strchr("+-*/%", b)) {
    char last = clc[cursor - 1];
    if (strchr("+-*/%", last)) {
      // Cases to REPLACE last operator with normalized one (overwrite, don't append)
      if (last == '*' && b == '/') { clc[cursor - 1] = '*'; cursor--; return true; }        // */ -> *
      else if (last == '/' && b == '+') { clc[cursor - 1] = '/'; cursor--; return true; }       // /+ -> /
      else if (last == '-' && b == '*') { clc[cursor - 1] = '-'; cursor--; return true; }       // -* -> -
      else if (last == '*' && b == '+') { clc[cursor - 1] = '*'; cursor--; return true; }       // *+ -> *
      else if (last == '/' && b == '/') { clc[cursor - 1] = '/'; cursor--; return true; }       // // -> /
      else if (last == '*' && b == '*') { clc[cursor - 1] = '*'; cursor--; return true; }       // ** -> * (not power)
      // Cases to KEEP both operators (fall through to normal append)
      else if (last == '/' && b == '-') { }            // /- keep (unary minus)
      else if (last == '*' && b == '-') { }            // *- keep (unary minus)
      else if (last == '+' && b == '-') { }            // +- keep (unary minus)
      else if (last == '-' && b == '+') { }            // -+ keep
      else if (last == '-' && b == '-') { }            // -- keep
      else { clc[cursor - 1] = last; cursor--; return true; }  // default: replace with last
    }
  }

  if (!f1 && !f2) {
    if (cursor >= sizeof(clc) - 1) {
      tone(12, 450, 99);
      return false;
    }
    clc[cursor] = b;
    clc[cursor + 1] = '\0';
    return true;
  }

  if (f1) {
    if (cursor >= sizeof(clc) - 1) {
      tone(12, 450, 99);
      f1 = false;
      return false;
    }
    f1 = false;
    switch (b) {
      case 'X': clc[cursor] = '$'; break;
      case 'Y': clc[cursor] = '<'; break;
      case 'Z': clc[cursor] = '>'; break;
      case '0': clc[cursor] = 'r'; break;
      case '1': clc[cursor] = 's'; break;
      case '2': clc[cursor] = 'v'; break;
      case '3': clc[cursor] = 'u'; break;
      case '4': clc[cursor] = 'p'; break;
      case '5': clc[cursor] = 'o'; break;
      case '6': clc[cursor] = 'm'; break;
      case '7': clc[cursor] = 'q'; break;
      case '8': clc[cursor] = 'n'; break;
      case '9': clc[cursor] = 'n'; break;
      case '.': clc[cursor] = ','; break;
      case '=': clc[cursor] = '~'; break;
      case '+': clc[cursor] = '@'; break;
      case '-': clc[cursor] = '#'; break;
      case '*': clc[cursor] = '&'; break;
      case '/': clc[cursor] = 'w'; break;
      case '(': clc[cursor] = ':'; break;
      case ')': clc[cursor] = ';'; break;
      case 'B': clc[cursor] = '"'; break;
      case 'C': clc[cursor] = 'l'; break;
    }
    return true;
  }

  if (f2) {
    if (cursor >= sizeof(clc) - 1) {
      tone(12, 450, 99);
      f2 = false;
      return false;
    }
    f2 = false;
    switch (b) {
      case 'X': clc[cursor] = 'x'; break;
      case 'Y': clc[cursor] = 'y'; break;
      case 'Z': clc[cursor] = 'z'; break;
      case '0': clc[cursor] = 'a'; break;
      case '1': clc[cursor] = 'b'; break;
      case '2': clc[cursor] = 'c'; break;
      case '3': clc[cursor] = 'd'; break;
      case '4': clc[cursor] = 'e'; break;
      case '5': clc[cursor] = 'f'; break;
      case '6': clc[cursor] = 'g'; break;
      case '7': clc[cursor] = 'h'; break;
      case '8': clc[cursor] = 'i'; break;
      case '9': clc[cursor] = 'j'; break;
      case '.': clc[cursor] = ' '; break;
      case '=': clc[cursor] = '!'; break;
      case '+': clc[cursor] = '|'; break;
      case '-': clc[cursor] = '_'; break;
      case '*': clc[cursor] = '?'; break;
      case '/': clc[cursor] = '%'; break;
      case '(': clc[cursor] = '{'; break;
      case ')': clc[cursor] = '}'; break;
      case 'B': clc[cursor] = '`'; break;
      case 'C': clc[cursor] = 'k'; break;
    }
    return true;
  }
  return false;
}

// -------------------- NTC --------------------
void my_ntc() {
  if (analogVal < 9) analogVal = 9;
  if (analogVal > 999) analogVal = 999;

  unsigned int R = 10000 / (1024.0 / analogVal - 1);
  temp = 1 / (1 / 298.3 + log(R / 10000.0) / 4000.0) - 273.3;
}

// -------------------- Serial command mode --------------------
void processSerialChar(char c) {
  if (c == '\r') return;

  if (c == '\n') {
    serialBuf[serialPos] = '\0';
    if (serialPos > 0) serialCommand(serialBuf);
    serialPos = 0;
    return;
  }

  if (serialPos < sizeof(serialBuf) - 1) {
    serialBuf[serialPos++] = c;
  }
}

void serialCommand(char *cmd) {
  // trim whitespace
  char *start = cmd;
  while (*start == ' ' || *start == '\t') start++;
  char *end = start + strlen(start) - 1;
  while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
  *(end + 1) = '\0';
  
  // to lowercase
  for (char *p = start; *p; p++) {
    if (*p >= 'A' && *p <= 'Z') *p += 32;
  }

  if (strcmp(start, "deg") == 0) {
    degMode = true;
    my_print("deg");
    return;
  }

  if (strcmp(start, "rad") == 0) {
    degMode = false;
    my_print("rad");
    return;
  }

  if (strcmp(start, "bs") == 0) {
    backspace();
    return;
  }

  if (strcmp(start, "clear") == 0 || strcmp(start, "cx") == 0) {
    clearAll();
    return;
  }

  if (strcmp(start, "a0") == 0) {
    analogMode = true;
    analogPin = A0;
    my_print("analog");
    return;
  }

  if (strcmp(start, "a1") == 0) {
    analogMode = true;
    analogPin = A1;
    my_print("ntc");
    return;
  }

  if (strcmp(start, "out") == 0) {
    digitalWrite(13, !digitalRead(13));
    my_print("out");
    return;
  }

  // expression mode
  strncpy(clc, start, sizeof(clc) - 1);
  clc[sizeof(clc) - 1] = '\0';
  cursor = strlen(clc);
  my_solve_equation();
}
