# Coding Style

**The single most important rule: every group member must be able to read this code and
explain it in a viva.** Marks are deducted for code a student cannot explain. Write for a
second-year student, not for a professional C programmer.

Before writing anything, read `.vscode/code_samples/` and match what you find there.

---

## Write like this

**Plain C. Obvious names. One thing per line.**

```c
// Good — you can read it top to bottom
int people = core_people_inside();
if (people > 0) {
    ESP_LOGI(TAG, "Building has %d people", people);
}
```

**Not like this:**

```c
// Bad — clever, compact, hard to explain under questioning
#define CHK(x) do { esp_err_t __e=(x); if(__e) return __e; } while(0)
ESP_LOGI(TAG,"%d",({int p=core_people_inside();p>0?p:-1;}));
```

---

## Specific rules

**Naming**
- Functions: `module_action()` — `core_record_entry()`, `lighting_init()`
- Private functions and variables: `static`, lowercase with underscores
- Constants and macros: `UPPER_CASE`
- One `static const char *TAG = "module_name";` per `.c` file, for logging

**Avoid**
- Function-like macros. Write a real function instead.
- Pointer arithmetic where an array index works.
- Ternaries nested inside anything.
- `goto` (including the `goto cleanup` pattern — just return).
- Bit-field structs, unions, and `void *` gymnastics.
- Clever one-liners. If it needs a comment to explain *what* it does, rewrite it.

**Prefer**
- `if / else if / else` over lookup tables of function pointers.
- Explicit `for` loops over anything implicit.
- Early `return` on error, with a log line saying what failed.
- Small functions — if it doesn't fit on one screen, split it.

---

## Error handling

Check return values. Log what failed. Return an `esp_err_t`.

```c
esp_err_t lighting_init(room_id_t room, gpio_num_t pir, gpio_num_t led)
{
    esp_err_t err = gpio_set_direction(pir, GPIO_MODE_INPUT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure PIR pin %d", pir);
        return err;
    }
    // ...
    return ESP_OK;
}
```

`ESP_ERROR_CHECK()` is fine in `app_main` for setup that must succeed. Do not use it on
FreeRTOS calls — `xTaskCreate` returns `pdPASS` (1), not `ESP_OK` (0), so
`ESP_ERROR_CHECK(xTaskCreate(...))` panics on success.

---

## Module shape

Every module looks the same:

```c
// lighting.h — public interface only
#pragma once
#include "esp_err.h"
#include "core.h"

esp_err_t lighting_init(room_id_t room, gpio_num_t pir, gpio_num_t led);
```

```c
// lighting.c
#include "lighting.h"
#include "esp_log.h"

static const char *TAG = "lighting";

static void lighting_task(void *arg);   // private — declared here, defined below

esp_err_t lighting_init(room_id_t room, gpio_num_t pir, gpio_num_t led)
{
    // 1. configure hardware
    // 2. start the task
    xTaskCreate(lighting_task, "lighting", 2048, ctx, 5, NULL);
    return ESP_OK;
}

static void lighting_task(void *arg)
{
    while (1) {
        // ... work ...
        vTaskDelay(pdMS_TO_TICKS(100));   // always yield
    }
}
```

**Public `init()` that starts a `static` task. State stays inside the `.c` file.**

---

## FreeRTOS rules

- **Always yield.** Every task loop needs a `vTaskDelay()` or a blocking call
  (`xQueueReceive`, `xSemaphoreTake`). A loop that never yields trips the task watchdog.
- **Microsecond hardware timing uses `esp_rom_delay_us()`**, not `vTaskDelay` — `vTaskDelay`
  cannot go below one tick.
- **Take the mutex, copy, release, then do slow work.** Never hold a lock during I/O.
- **ISRs: `IRAM_ATTR`, short, no logging, no delays.** Set a flag or give a semaphore and return.
- **Variables shared between an ISR and a task must be `volatile`.**

---

## Comments

In `components/` — comment *why*, not *what*. The code says what.

```c
// Grace period stops lights flickering when someone sits still briefly
if (now - last_motion > LIGHT_GRACE_SECONDS) {
```

In `sandbox/` — comment everything. See `docs/sandbox.md`.

---

## Configuration

All pins, timeouts and tuning values live in one shared `config.h`:

```c
#define PIN_PIR_ROOM1       GPIO_NUM_34
#define PIN_LED_ROOM1       GPIO_NUM_25
#define LIGHT_GRACE_SECONDS 30
```

Never a bare pin number inside a `.c` file. If a value might change, it belongs here.
