# nano_calc

Arduino Nano calculator with 5×5 keypad and 16×2 I2C LCD.

**Version:** 30may26_v4  
**Author:** hjltu@ya.ru (2019)  
**License:** Provided as-is, without warranty

---

## Hardware

| Component | Pin / Address |
|-----------|---------------|
| Arduino Nano | — |
| LCD 1602 I2C | 0x27 (SDA=A4, SCL=A5) |
| Keypad 5×5 | Rows: D7–D11, Cols: D2–D6 |
| Buzzer | D12 |
| Digital Out | D13 |
| Analog In (A0) | A0 |
| NTC Thermistor | A1 (10 kΩ, β=4000) |

### Keypad Layout

```
A   M   X   Y   Z
7   8   9   +   (
4   5   6   -   )
1   2   3   *   B
0   .   =   /   C
```

- **A** — F1 (Alt) mode
- **M** — F2 (Mem) mode
- **B** — Backspace (BS)
- **C** — Clear All (CX)
- **=** — Evaluate expression

---

## Quick Start

1. Wire hardware per table above.
2. Install libraries: `Wire`, `LiquidCrystal_I2C`, `Keypad`.
3. Open `nano_calc_30may26_v4.ino` in Arduino IDE.
4. Select **Arduino Nano**, **ATmega328P (Old Bootloader)**.
5. Upload.

---

## Modes

| Key | Mode | Description |
|-----|------|-------------|
| (none) | Normal | Digits, operators, parentheses, decimal point |
| **A** then key | F1 (Alt) | Functions, constants, special commands |
| **M** then key | F2 (Mem) | Variables, memory cells, control words |

F1/F2 are **one-shot**: press the modifier, then the target key. Modifier clears after one use.

---

## Normal Mode Keys

| Key | Input |
|-----|-------|
| 0–9 | Digits |
| `.` | Decimal point |
| `+` `-` `*` `/` | Operators |
| `(` `)` | Parentheses |
| `=` | Evaluate |
| `B` | Backspace |
| `C` | Clear All |

---

## F1 Mode (press **A** first)

| Key | Inserts | Displayed As | Function |
|-----|---------|--------------|----------|
| `X` | `$` | `ANS` | Last answer |
| `Y` | `<` | `<` | (unused) |
| `Z` | `>` | `>` | (unused) |
| `0` | `r` | `cos` | Cosine |
| `1` | `s` | `sin` | Sine |
| `2` | `v` | `tg` | Tangent |
| `3` | `u` | `ctg` | Cotangent |
| `4` | `p` | `pi` | π (3.14159…) |
| `5` | `o` | `exp` | eˣ |
| `6` | `m` | `lg` | log₁₀ |
| `7` | `q` | `sqrt` | Square root |
| `8` | `^` | `^` | (unused) |
| `9` | `n` | `ln` | Natural log |
| `.` | `,` | `,` | (unused) |
| `=` | `~` | `(-1)*` | Unary minus |
| `+` | `@` | `LOOP` | (unused) |
| `-` | `#` | `IF` | (unused) |
| `*` | `&` | `PAUSE` | (unused) |
| `/` | `w` | `GRAD` | **Toggle GRAD/RAD** (shows 2 sec) |
| `(` | `:` | `PRG` | (unused) |
| `)` | `;` | `END` | (unused) |
| `B` | `"` | `abs` | Absolute value |
| `C` | `l` | `LIGHT` | Toggle LCD backlight |

> **Note:** F1+`/` now toggles **GRAD ↔ RAD**. Displays `Mode = GRAD` or `Mode = RAD` on LCD for **2 seconds**, then returns to expression. (Legacy label `GRAD` kept in key mapping; actual behavior = angle mode toggle.)

---

## F2 Mode (press **M** first)

| Key | Inserts | Displayed As | Meaning |
|-----|---------|--------------|---------|
| `X` | `x` | `x` | Variable x |
| `Y` | `y` | `y` | Variable y |
| `Z` | `z` | `z` | Variable z |
| `0` | `a` | `a` | Memory cell 0 |
| `1` | `b` | `b` | Memory cell 1 |
| `2` | `c` | `c` | Memory cell 2 |
| `3` | `d` | `d` | Memory cell 3 |
| `4` | `e` | `e` | Memory cell 4 |
| `5` | `f` | `f` | Memory cell 5 |
| `6` | `g` | `IN` | (unused) |
| `7` | `h` | `A0` | Analog mode A0 |
| `8` | `i` | `A1` | NTC mode A1 |
| `9` | `j` | `OUT` | Toggle D13 |
| `.` | ` ` | ` ` | Space |
| `=` | `!` | `!=` | (unused) |
| `+` | `\|` | `BUZ` | (unused) |
| `-` | `_` | `OUT` | (alias) |
| `*` | `?` | `?` | (unused) |
| `/` | `%` | `%` | Modulo operator |
| `(` | `{` | `{` | (unused) |
| `)` | `}` | `}` | (unused) |
| `B` | `` ` `` | `` ` `` | (unused) |
| `C` | `k` | `BLINK` | Toggle cursor blink |

---

## Built-in Variables & Memory

| Symbol | Stored Value |
|--------|--------------|
| `X` `Y` `Z` | Last 3 results (X = newest) — **stored as `float`** (full precision) |
| `a`–`f` | Memory cells (user-assigned, not auto-saved) |
| `$` / `ANS` | Last answer (same as `X`) |

After each `=`: `Z = Y; Y = X; X = result` (all `float`).

---

## Expression Syntax

- **Operators:** `+ - * / %` (modulo)
- **Parentheses:** `( )` — full precedence support
- **Numbers:** Integer or decimal (`12.5`, `.5`, `-3.14`)
- **Unary minus:** `(-1)*` via F1+`=` or write `-5+2`

### Functions (F1 mode or typed via serial)

| Name | Key (F1) | Parser Token | Example |
|------|----------|--------------|---------|
| sin | `1` | `s` | `sin(30)` |
| cos | `0` | `r` | `cos(60)` |
| tan | `2` | `v` | `tg(45)` |
| cot | `3` | `u` | `ctg(45)` |
| sqrt | `7` | `q` | `sqrt(16)` |
| ln | `9` | `n` | `ln(2.718)` |
| exp | `5` | `o` | `exp(1)` |
| lg | `6` | `m` | `lg(100)` |
| abs | `B` | `"` | `abs(-5)` |
| pi | `4` | `p` | `pi` |

### Angle Mode

- **Default:** Gradians (`GRAD`)
- **Trig functions** (`sin`, `cos`, `tg`, `ctg`) use current mode
- **Toggle via serial:** send `deg` or `rad` + Enter (legacy commands)
- **Keypad:** **F1 + `/`** toggles **GRAD ↔ RAD**, displays mode for **2 seconds** then returns to expression

---

## Special Commands (Keypad or Serial)

Enter the word as an expression, then press `=`:

| Command | Effect |
|---------|--------|
| `LIGHT` | Toggle LCD backlight **(only on blank screen, not during equation entry)** |
| `BLINK` | Toggle cursor blink **(only on blank screen, not during equation entry)** |
| `A0` | Start analog read on A0 (0–1023) |
| `A1` | Start NTC temperature read on A1 |
| `OUT` | Toggle D13 output |
| `GRAD` | Set GRAD mode (serial only) |
| `RAD` | Set radian mode (serial only) |

> In analog/NTC mode, display updates ~1 Hz. Press `C` (Clear) to exit.

---

## Serial Interface

Baud: **9600**, newline-terminated.

### Commands

| Command | Description |
|---------|-------------|
| `grad` | Set GRAD mode (legacy) |
| `rad` | Set RAD mode |
| `bs` | Backspace |
| `clear` / `cx` | Clear all |
| `a0` | Start A0 analog read |
| `a1` | Start A1 NTC read |
| `out` | Toggle D13 |

### Expressions

Send any valid expression followed by Enter:

```
2*(3.5+1)
sin(30)
sqrt(144)
abs(-9)
pi*2
```

Result prints to Serial and shows on LCD.

---

## NTC Thermistor (A1)

- **Thermistor:** 10 kΩ NTC, β = 4000
- **Formula:** Steinhart-Hart approximation
- **Range:** ~–20 °C to +100 °C
- **Display:** `temp = XX.XX  NTC 10kOm`
- **Serial:** Raw temperature (°C) printed each cycle

---

## Display Behavior

- 16×2 LCD, scrolls long lines (32 char window)
- Status messages: `F1`, `F2`, `solve`, `analog`, `ntc`, `out`, `deg`, `rad`
- Angle mode indicator: ` GRAD` or ` RAD` appended to expression

---

## Example Sessions

### Basic Arithmetic

```
12+3=           → 15
10/2=           → 5
2*3+4=          → 10
2*(3+4)=        → 14
(10-2)/4=       → 2
```

### Floating Point

```
3.5+1.2=        → 4.7
(2.5*4)-0.5=    → 9.5
7.25/2=         → 3.625
```

### Parentheses

```
(1+2)*(3+4)=    → 21
((2+3)*4)=      → 20
(10-(2+3))=     → 5
```

### Trigonometry (GRAD mode)

```
sin(30)=        → 0.5
cos(60)=        → 0.5
tg(45)=         → 1
ctg(45)=        → 1
```

### Trigonometry (RAD mode — via serial)

```
> rad
> sin(1.5708)   → 1
> cos(3.14159)  → -1
> deg
```

### Functions

```
sqrt(81)=       → 9
ln(2.71828)=    → 1
exp(1)=         → 2.718...
lg(100)=        → 2
abs(-12)=       → 12
pi=             → 3.14159...
```

### Variables & Memory

```
X+1=            → (last result + 1)
Y*2=            → (previous result * 2)
a+b=            → (mem[0] + mem[1])
```

### Serial Commands

```
> deg
> rad
> bs
> clear
> a0
> a1
> out
> 2*(3+4)
> sin(30)
> sqrt(144)
> abs(-9)
```

### Expression History (planned: F1+`<` / F1+`>`)
```
> F1+<
2*(3+4)=14
> F1+<
sin(30)=0.5
> F1+<
sqrt(144)=12
```

---

## Known Limitations

- **F1 + `/`** now toggles **GRAD/RAD** (shows 2 sec) — legacy label `GRAD` kept in key mapping
- **Power operator `^`** (F1+`8`) parsed but not yet implemented — planned
- F1/F2 symbols `< > ~ @ # & : ; ,` ` ` `! | _ ? { }` ` ` ` are display-only
- No persistent EEPROM storage for memory cells
- Modulo `%` only works via F2+`/` or serial
- No error reporting for divide-by-zero or syntax errors (shows 0 or partial result)
- Expression history (F1+`<` / F1+`>`) not yet implemented
- `X` `Y` `Z` currently stored as `int` — will change to `float` for full precision

---

## File Structure

```
nano_calc/
├── nano_calc_30may26_v4.ino   # Main sketch
├── README.md                   # This file
└── AGENT.md                    # AI assistant guidelines
```

---

## Credits

Original code by **hjltu@ya.ru** (2019).  
Libraries: `Wire`, `LiquidCrystal_I2C`, `Keypad`, `math.h`.