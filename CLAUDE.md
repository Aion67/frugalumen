# CLAUDE.md — frugalumen

CoCIS Smart Control & Monitoring System. ESP32 + ESP-IDF, simulated in Wokwi.

**Read these before writing any code:**
- `docs/assignment.md` — what we must build and why
- `docs/architecture.md` — modules, peripherals, protocols, diagrams
- `docs/coding-style.md` — **how we write code (important: we are beginners)**
- `docs/sandbox.md` — the teaching copies in `sandbox/`

---

## What this branch is for

This is a **learning branch**. The work here will NOT be submitted. Its purpose is to
produce a correct, well-structured reference implementation that our group can read and
compare against our own attempt on the main branch.

That means: **clarity beats cleverness, always.** A version we can read and explain in a
viva is worth more than an elegant one we cannot.

---

## Directory layout (already exists — do not restructure)

```
frugalumen/
├── CMakeLists.txt
├── diagram.json          # Wokwi wiring
├── wokwi.toml
├── main/                 # app_main only — starts tasks, nothing else
├── components/           # the real, clean implementation
│   ├── core/             # shared state + mutex + event queue
│   ├── rfid_door/        # entrance: RFID, servo, buzzer
│   ├── lighting/         # rooms: PIR + LED
│   ├── storage/          # microSD logging
│   ├── console/          # serial menus
│   └── display/          # LCD status
├── sandbox/              # heavily-commented teaching copies (see docs/sandbox.md)
│   ├── joshua-core/
│   ├── jovia-rfid_door/
│   ├── shirley-lighting/
│   ├── hairat-storage/
│   ├── prince-console/
│   └── joel-display/
└── docs/
```

**Two parallel deliverables for every module:**
1. `components/<name>/` — the clean production version
2. `sandbox/<owner>-<name>/` — the same ideas, explained line by line for a beginner

---

## Hard rules

- **ESP-IDF only.** No Arduino framework, no Arduino libraries.
- **One ESP32.** Three "terminals" (Entrance, Room 1, Room 2) are FreeRTOS tasks, not
  separate boards. Wokwi runs one board.
- **All shared state goes through `core`.** No globals shared between modules. No module
  calls another module's internals — only `core.h` and the module's own public header.
- **Pin numbers and timeouts live in one config header.** Never hard-code a pin in a `.c`.
- **Logging via `ESP_LOGI(TAG, ...)`.** No `printf` inside modules (console module excepted —
  that is its job).
- **Nothing blocks more than 50 ms in a callback or ISR.** Long work goes in the module's task.
- **Every module exposes `<name>_init()`** which configures its hardware and starts its task.
  The task function itself is `static`.

---

## Before you start a module

1. Read `docs/architecture.md` for that module's responsibility and which peripherals it owns.
2. Check `.vscode/code_samples/` for our existing code — match that style.
3. Write the header (`include/<name>.h`) first, then the implementation.
4. Write the sandbox version alongside, with comments explaining every unfamiliar API.

## When you are unsure

Ask rather than assume — especially about scope. We have a deadline and the temptation to
add features is the main risk. If a feature is not in `docs/assignment.md`, confirm before
building it.
