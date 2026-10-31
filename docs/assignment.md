# The Assignment

**Smart Control and Monitoring System for CoCIS, Makerere University**
BSE III & CS II · 2026/2027 · Deadline 2 November 2026 · Wokwi + ESP-IDF

---

## The problem being solved

CoCIS houses valuable equipment needing access control, and wastes electricity on lights
left on in empty rooms. Management wants an automated pilot covering **two lecture rooms
and the main entrance**.

---

## Requirements (numbered as in the brief)

### 2.1 Access control and occupancy monitoring
- Main entrance split into **entry** and **exit** sections.
- Everyone is identified by an **RFID tag**, scanned at both entry and exit.
- Valid tag → verify identity → activate door mechanism.
- System keeps **records of entry and exit events**, so management can tell who is inside
  at any moment.

### 2.2 Automated lighting control
- Both lecture rooms detect whether they are **occupied**.
- Lights switch on when occupied, off when not — to cut electricity waste.

### 2.3 Distributed control
- Three control terminals: Room 1, Room 2, Entrance.
- Brief says each **may** use a separate ESP32 — permission, not requirement.
- **Our choice: one ESP32, three FreeRTOS tasks.** Wokwi runs one board. The report
  explains how the same tasks would map onto three physical boards.

### 2.4 Data storage
- Access logs, occupancy records and system activity are large, so use a
  **memory card module (microSD)** for additional storage.

### 2.5 Serial interface and management menus
- Users access system information through **commands entered via the serial interface**.
- Must provide: access records, current building occupancy, room occupancy status,
  lighting status, and other useful information.
- Additional output interfaces may be added (we use an LCD).

### 2.6 Configurable building access schedule
- Management can configure **the time period during which access is permitted**.
- Outside that period, access through entry and exit doors is restricted.
- Must be enforced consistently — including that people **should not leave outside the
  authorised period while still inside**.
- Schedule adjustable at any time, but **only by authenticated, authorised personnel**.

### 2.7 Resource synchronisation and reliability
- Multiple parts access shared information and resources.
- **Appropriate synchronisation mechanisms must prevent conflicts, inconsistent records,
  and race conditions.**
- This is where marks concentrate. See `docs/architecture.md`.

---

## Innovation features

The brief states **particular emphasis will be placed on innovation**. We implement three:

1. **Anti-passback** — a tag cannot enter twice without exiting, or exit without entering.
   Catches tag-sharing and tailgating.
2. **Overstay alert** — when the access window closes with people still inside, raise an
   alert, list who, and keep the exit door enabled for them only.
3. **Daily occupancy report** — console command reading the log: entries, exits, peak
   occupancy, hours each room's lights were on. Directly answers the electricity-cost problem.

---

## Dos and don'ts

**Do**
- Keep every feature explainable — each member must answer questions about their code.
- Log every state change as an event, so the report can show real data.
- Make the schedule and admin PIN survive reset (NVS).
- Keep the Wokwi diagram minimal so the simulation stays responsive.

**Don't**
- Add WiFi, NTP, web servers, or cloud anything. Not required, large scope, and WiFi
  disables ADC2.
- Add RFID readers to the lecture rooms — the brief only requires them at the entrance.
  Rooms use occupancy detection.
- Use the Arduino framework or Arduino libraries.
- Exceed the stated scope. Features not listed here need confirmation first.

---

## Assessment reality

Marks come from: report quality, **each student's ability to answer questions about their
own code**, clarity of the individual videos, and innovation.

> "Marks will be deducted if a student is unable to explain or answer questions about
> their stated contributions."

This is why the sandbox copies exist and why the style rules are strict. Code nobody in
the group can explain is worth negative marks.
