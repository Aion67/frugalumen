# The `sandbox/` Folder

`components/` holds the clean implementation. `sandbox/` holds **teaching copies** — the
same ideas, written to be read and understood by the group member who owns that module.

```
sandbox/
├── joshua-core/
├── jovia-rfid_door/
├── shirley-lighting/
├── hairat-storage/
├── prince-console/
└── joel-display/
```

Each folder is named `<owner>-<module>`. Its job is to make the real version in
`components/<module>/` explainable.

---

## Why this exists

The assignment deducts marks when a student cannot explain their own code. The sandbox is
where each person learns what their module actually does, before they record their video
and face questions.

**The sandbox is for understanding. `components/` is for the system.**

---

## What goes in a sandbox folder

**1. Small runnable experiments.** One concept per file, each tiny enough to run alone in
Wokwi and watch the output.

```
shirley-lighting/
├── 01_read_pir.c           # just read the sensor and print
├── 02_led_on_off.c         # just drive the LED
├── 03_grace_timer.c        # add the delay before lights-off
├── 04_full_module.c        # all of it, as a task, talking to core
└── NOTES.md                # what I learned, what confused me
```

Number them in the order a beginner should read them.

**2. Heavy comments — explain every unfamiliar API.** This is the opposite of the
`components/` rule. Here, explain *what* as well as *why*.

```c
// gpio_get_level() reads the pin RIGHT NOW and returns 1 (HIGH) or 0 (LOW).
// The PIR sensor outputs HIGH while it sees movement, so 1 means "motion".
int motion = gpio_get_level(PIN_PIR_ROOM1);

// pdMS_TO_TICKS converts milliseconds into FreeRTOS "ticks" (the scheduler's
// unit of time). vTaskDelay puts THIS task to sleep for that long and lets
// other tasks run — unlike a busy-wait, which would hog the CPU.
vTaskDelay(pdMS_TO_TICKS(100));
```

**3. The gotchas, written down.** Every trap you hit belongs in the comments or `NOTES.md`,
because it will come up in questioning:

- why `volatile` is needed on ISR-shared variables
- why the task must yield or the watchdog fires
- why the mutex is taken, copied, released — not held during work
- why `.length` in an SPI transaction is in **bits**
- why active-low means pressed == 0

**4. A `NOTES.md`** with, in the owner's own words: what the module does, which pins it
uses, how it was tested, and what is still unclear.

---

## Rules

- Sandbox code **must run** — it is not pseudocode. Each file should build and demonstrate
  its one idea.
- Sandbox code **must not be imported** by `components/`. No dependencies either way.
- Keep each file focused on **one** concept. If a file needs two explanations, split it.
- Prefer the simplest version that shows the idea, even if the real module is more complex.
  Then add a comment saying what the real one does differently and why.

---

## Guidance for Claude

When implementing a module, produce **both**:

1. `components/<module>/` — clean, production, comments explain *why*
2. `sandbox/<owner>-<module>/` — numbered experiments building up to the same result,
   comments explain *everything*, written for someone meeting these APIs for the first time

The sandbox version is not an afterthought. For this branch it is the more important of
the two, because its purpose is for the group to understand the system well enough to
defend it.
