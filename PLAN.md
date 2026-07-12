# nano_calc Development Plan

**Project:** Arduino Nano Calculator (nano_calc)  
**Current Version:** 30may26_v4  
**Target:** Incremental improvements without breaking existing behavior

---

## Phase 1: Bug Fixes & Stability (High Priority)

### 1.1 Fix GRAD Toggle (F1 + `/`)
- **Issue:** F1+`/` inserts `w` → displays `GRAD` but does nothing
- **Fix:** In `my_check_mode()`, detect `GRAD` prefix and toggle `degMode` (GRAD ↔ RAD)
- **Clarification:** This toggles **GRAD ↔ RAD**. `GRAD` label is correct; `degMode` variable name is legacy.
- **Display:** Show `Mode = GRAD` or `Mode = RAD` until next input (2s timeout **not needed**)
- **Cleanup:** Remove unused timeout code (`modeDisplayUntil`, related logic in `my_print`)
- **Files:** `nano_calc_30may26_v4.ino`
- **Status:** ✅ **IMPLEMENTED** — toggle works; **CLEANUP NEEDED** (remove dead timeout code)

### 1.2 Modulo Operator `%` — **NO ACTION NEEDED (defer)**
- Already accessible via **F2 + `/`** (inserts `%`)
- Parser supports `%` in `parseTerm()`
- Will revisit for usability improvements later
- **Status:** ⏸️ DEFERRED

### 1.3 Division by Zero — **NO ACTION NEEDED (defer)**
- Current behavior: returns `INF` (Arduino `float` division by zero)
- No silent zero result
- Will revisit for explicit error handling later
- **Status:** ⏸️ DEFERRED

### 1.4 Fix Buffer Overflow Risk
- **Issue:** `clc[199]` no bounds check in `my_add_key()` when `cursor >= 198`
- **Fix:** Add guard, beep warning (tone 450Hz), ignore key if full
- **Files:** `nano_calc_30may26_v4.ino`
- **Status:** ✅ **IMPLEMENTED** (lines 565, 575, 610: guards in normal/F1/F2 branches)

### 1.5 Fix `my_check_mode()` Re-trigger Bug
- **Issue:** Mode commands (`LIGHT`, `BLINK`, `A0`, `A1`, `OUT`) re-trigger on every redraw because check uses `indexOf() == 0`
- **Fix:** Track last-executed command (`lastModeCmd[8]`); only trigger on **new input**
- **Clarification:** `LIGHT` / `BLINK` should toggle **only when screen is blank** (no active equation input), not during equation entry
- **Files:** `nano_calc_30may26_v4.ino`
- **Status:** ⏸️ **DEFERRED** — partial implementation exists but not fully functional; needs redesign

### 1.6 x, y, z Result Storage — Change `int` to `float`
- **Current:** `xyz[3]` declared as `int`, truncates decimal results
- **Fix:** Change to `float xyz[3]` — preserves full precision
- **Impact:** Update `my_solve_equation()` shift logic and any usage
- **Status:** ✅ **IMPLEMENTED** (line 43: `float xyz[3]`; line 380: removed `(float)` cast)

---

## Phase 2: Usability Improvements (Medium Priority)

### 2.1 GRAD/RAD Toggle from Keypad
- **Current:** Serial only (`deg`/`rad`)
- **Add:** **F1 + `/`** (currently `GRAD`) → toggle angle mode (GRAD ↔ RAD)
- **Display:** Show `Mode = GRAD` or `Mode = RAD` until next input (2s timeout **not needed**)
- **Cleanup:** Remove unused timeout code (`modeDisplayUntil`, related logic in `my_print`)
- **Status:** ✅ **IMPLEMENTED** (same as 1.1: F1+/ inserts `w`→`GRAD`, my_check_mode toggles degMode); **CLEANUP NEEDED**

### 2.2 ANS / Last Answer Key — **NO ACTION NEEDED**
- **Current:** F1+`X` inserts `$` → displays `ANS` (shows value)
- **User feedback:** Already works, no change needed
- **Status:** ✅ DONE

### 2.3 Memory Store/Recall (STO/RCL) — **DEFERRED**
- **User feedback:** "I do not understand" — unclear workflow
- **Defer** until usability design clarified
- **Status:** ⏸️ DEFERRED

### 2.4 Clear Entry (CE) vs Clear All (C) — **NO ACTION NEEDED**
- **Current:** `B` = backspace (clear last char), `C` = clear all
- **User feedback:** Already implemented as desired
- **Status:** ✅ DONE

### 2.5 Expression History / Recall
- **Add:** Store last 5 evaluated expressions + results (in RAM, lost on reset)
- **Keys:** F1+`<` / F1+`>` (already insert `<` / `>` chars) → scroll history
- **Behavior:** Recall populates `clc[]` buffer for editing/re-evaluation
- **Persistence:** No EEPROM — lost on reset
- **Status:** ✅ **IMPLEMENTED** (histPush in my_solve_equation, histRecall in my_add_key for F1+Y/Z)

### 2.6 F2+X (lowercase x) Variable Not Evaluated — **BUG**
- **Issue:** F2+`X` inserts lowercase `x` → **displays** `x` on LCD but **parser only handles uppercase `X`** (xyz[0])
- **Result:** Expression `x+1` shows correctly on display but evaluates as `0+1=1` (or stale value) instead of `xyz[0]+1`
- **Example:** Last result=2.2 (xyz[0]=2.2), type `x-2=` → display shows `x-2=` but **result shows 2.2** (expect 0.2)
- **Root cause found:** `my_solve_equation()` does NOT reset `cursor=0` after `=`. Next input appends to old buffer (e.g., `10=` + `x-5=` → buffer=`10=x-5=`, parser reads `10=` only).
- **Fix needed:** Reset `clc` buffer (not cursor) after solve, while preserving display of `equation=answer`.
- **Parser fix:** Add lowercase `x`, `y`, `z` cases in `parseFactor()` to return `xyz[0]`, `xyz[1]`, `xyz[2]`
- **Constraint:** **Do not touch keymap** — only parser fix
- **Status:** ✅ **IMPLEMENTED**  PARSER FIX DONE**  **BUFFER FIX DONE** in `my_char2string()`
- **Manual Test (History Stack Model):**
  1. Reset (F1+C) → `xyz` = `[0,0,0]`
  2. Type `10=` → display `10=10` → `xyz` = `[10,0,0]`
  3. Press X (`x`) → display `X = 10.00`
  4. Press F2+X (`x`) → display `x`
  5. Type `-5=` → **expect** `x-5=5`
  6. Type `y` (F2+Y) → display `y` (expect `0`)
  7. Type `x*2=` → **expect** `x*2=10`
  8. Verify `xyz` = `[10,10,0]` after step 6

### 2.7 Multiple Decimal Points Allowed — **BUG**
- **Issue:** Parser accepts `2.2.2` as valid number (e.g., `2.2.2-6=-3.8` computes correctly but input is invalid)
- **Expected:** Only one decimal point per number; display should reject/ignore extra dots
- **Fix:** In `parseNumber()`, stop at second dot; add keypad guard in `my_add_key()` to reject extra dots
- **Status:** 🔧 **PARTIAL — parser stops at 2nd dot** (line 249: `break`), but **keypad guard NOT YET ADDED**; display still shows extra dots (e.g., `2.2.2-1=` shows `2.2.2-1` but parses as `2.2-1=1.2`); **DEFERRED** per user request
- **Manual Test (Parser Only):**
  1. Type `2.2.2-1=` via serial or keypad
  2. Display shows `2.2.2-1` (keypad allows extra dots)
  3. **Expect result:** `1.2` (parser computes `2.2-1`)
  4. Type `3.1.4+0=` → expect `3.1` (parser stops at 2nd dot)
  5. Type `10..5=` → expect `10` (stops at 2nd dot)
- **When keypad guard added:** Extra `.` keypresses should beep and not appear on display

### 2.8 Long Input Buffer Overflow — **BUG**
- **Issue:** Input > ~65 chars breaks everything, requires reboot
- **Root cause:** `clcStr[64]` overflow or `clc[199]` not properly bounded
- **Expected:** Max input = **63 chars** (match `clcStr[64]` = 63 + null), reject further keys with beep
- **Fix:** 1) Shrink `clc[199]` → `clc[64]` (saves ~135 bytes RAM), 2) In `my_add_key()`, limit `cursor < 63` with overflow guard + beep
- **Status:** ✅ **IMPLEMENTED & TESTED** (clc[64] at line 37; my_add_key guards at 3 branches using `sizeof(clc) - 1`)
- **Manual Test:**
  1. Type 63 digits: `111...111` (63x `1`)
  2. Press `1` again → **expect beep**, no new char
  3. Press F1+key → **expect beep** if at limit
  4. Press F2+key → **expect beep** if at limit
  5. Clear (F1+C), type 64 chars → should beep on 64th

### 2.9 Invalid Operator Sequences — **BUG**
- **Issue:** `4*/2=0.0` accepted but invalid (should be `4*2=8` or error)
- **Expected:** Reject or normalize invalid `*/`, `/+`, `-*`, etc.
- **Preserve:** Valid unary minus like `5+-1=4`, `5-+1=4`
- **Keep:** `**` as future power operator (deferred)
- **Fix:** In `my_add_key()`, overwrite last operator for invalid sequences; append for valid unary minus
- **Status:** ✅ **IMPLEMENTED & TESTED** (my_add_key: REPLACE cases `*/` `/+` `-*` `*+` `//` `**` overwrite with cursor--; KEEP cases `/-` `*-` `+-` `-+` `--` append)
- **Manual Test:**
  1. Type `5//2=` → display `5/2=2.5` (normalize `//`→`/`)
  2. Type `5*/2=` → display `5*2=10` (normalize `*/`→`*`)
  3. Type `5**2=` → display `5*2=10` (normalize `**`→`*`, not power)
  4. Type `5*+2=` → display `5*2=10` (normalize `*+`→`*`)
  5. Type `5/+2=` → display `5/2=2.5` (normalize `/+`→`/`)
  6. Type `5*-2=` → display `5*-2=-10` (keep `*-` unary minus)
  7. Type `5+-2=` → display `5+-2=3` (keep `+-` unary minus)
  8. Type `5-+2=` → display `5-+2=3` (keep `-+`)
  9. Type `5--2=` → display `5--2=7` (keep `--`)
  10. Type `5/-2=` → display `5/-2=-2.5` (keep `/-`)
  11. Verify `5*(2+3)=` still works → `25` (parentheses not broken)

---

## Phase 3: Feature Extensions (Lower Priority)

### 3.1 Power Operator `^` and `**`
- **Parser:** Already accepts `^` token (F1+`8`) — **IMPLEMENTED** in `parseFactor()` (right-associative `powf`)
- **Add:** `**` as Python-style power (same as `^`) — **IMPLEMENTED**
- **Precedence:** Right-associative, binds tighter than `* /`
- **Keypad:** F1+`8` inserts `^`; `**` via serial or future key combo
- **Status:** `^` ✅ **IMPLEMENTED**; `**` ✅ **IMPLEMENTED** (parseFactor lines 382-386: checks `*p == '*' && *(p+1) == '*'`)

### 3.2 Constants Menu — **NO ACTION NEEDED**
- User feedback: "what is 'combo'?" — unclear
- **Status:** ❌ NOT NEEDED

### 3.3 Persistent Memory (EEPROM) — **NO ACTION NEEDED**
- User feedback: limited memory
- **Status:** ❌ NOT NEEDED

### 3.4 NTC Calibration — **NO ACTION NEEDED**
- User feedback: no
- **Status:** ❌ NOT NEEDED

### 3.5 Buzzer Config — **NO ACTION NEEDED**
- User feedback: no
- **Status:** ❌ NOT NEEDED

### 3.6 Startup Splash with Version — **NO ACTION NEEDED**
- User feedback: limited memory
- **Status:** ❌ NOT NEEDED

---

## Phase 4: Code Quality & Maintainability

### 4.1 Modularize Parser — **NO ACTION NEEDED**
- User feedback: keep all code in one file
- **Status:** ❌ NOT NEEDED

### 4.2 Replace String with char Arrays
- `clcStr` (String) causes heap fragmentation on Nano (2 KB RAM)
- Use fixed `char clcStr[64]` + `snprintf` / manual concat
- **Constraint:** Must not break existing code
- **Status:** ✅ **IMPLEMENTED** — all `String` ops converted to `strcat/snprintf/dtostrf/itoa`

### 4.3 Add Error State Machine
- Enum: `OK`, `ERR_SYNTAX`, `ERR_DIV0`, `ERR_OVERFLOW`, `ERR_PARSE`
- Display error code on LCD, clear on next keypress

### 4.4 Unit Tests (Host-side) — **NO ACTION NEEDED**
- User feedback: tests done manually
- **Status:** ❌ NOT NEEDED

---

## Implementation Order (Suggested)

| Week | Tasks | Status |
|------|-------|--------|
| 1 | 1.1, 1.4, 1.6 | ✅ **DONE** |
| 2 | 2.1, 2.5 | 2.1 ✅ **DONE** (same as 1.1), 2.5 ✅ **DONE** |
| 3 | 4.2, 4.3 | 4.2 ✅ **DONE**, 4.3 📋 PLANNED |
| 4 | 1.5 (deferred), documentation update | 📋 PLANNED |
| **Now** | **2.6, 2.7, 2.8, 2.9, 3.1(`**`)** | 🔧 **CODE CHANGES MADE, NEEDS TESTING** |

---

## Constraints

- **Arduino Nano (ATmega328P):** 32 KB Flash, 2 KB SRAM, 1 KB EEPROM
- **No external dependencies** beyond current libraries
- **Preserve existing keypad behavior** — changes must be additive
- **Single .ino file** (per project rules)
- **No git** — version via filename/date in header

---

## Testing Checklist Per Change

- [ ] Compiles on Arduino IDE 1.8.x / 2.x
- [ ] Fits in Flash (< 30 KB) and RAM (< 1.8 KB)
- [ ] All existing keypad sequences still work
- [ ] New feature works via keypad AND serial
- [ ] LCD display correct (scroll, status, mode)
- [ ] Serial output matches LCD
- [ ] No crash on edge cases (empty input, long input, div/0)
