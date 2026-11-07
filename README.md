# woody-woodpacker

A 64-bit ELF crypter/packer in C and x86-64 assembly. It encrypts the target binary's `.text` execution segment, injects an assembly/C decryption stub with a custom `PT_LOAD` program header, updates the ELF entry point, and at runtime decrypts the executable in memory before restoring stack and register state and transferring execution back to the original entry point.

## Build

```bash
# Build the packer, the decryption stub, and all test targets
make all
```

This compiles:
- `woody_woodpacker` — The ELF packer executable.
- `stub.bin` — Raw machine code decryption stub extracted from `stub.o`.
- `victim` — Minimal freestanding ELF binary (no standard library).
- `sample` — Standard C binary linked with glibc.
- `game` — Mid-sized terminal roguelike dungeon crawler (~864 KB).

Individual targets can also be built:
```bash
make stub      # Builds stub.o and extracts stub.bin
make victim    # Builds test/victim.c
make sample    # Builds test/sample.c
make game      # Builds test/game.c
```

Other available rules:
```bash
make clean     # Removes object files and test binaries
make fclean    # Removes object files, test binaries, and woody_woodpacker
make re        # fclean + make
```

---

## Usage

```bash
./woody_woodpacker <elf64_binary>
```

When run, `woody_woodpacker` encrypts the executable code of the input ELF and produces a packed binary named **`woody`** in the current directory:

```bash
./woody [args...]
```

---

## Testing with Different ELF Targets

The project includes three target binaries showcasing different execution environments and binary sizes:

### 1. Freestanding Binary (No Libc) — `victim`

A minimal freestanding ELF program written using raw Linux system calls (`sys_write`, `sys_exit`), without `libc` or runtime startup code.

```bash
# 1. Build the target
make victim

# 2. Pack the binary
./woody_woodpacker victim

# 3. Execute the packed binary
./woody
```

**Expected output:**
```text
it works!
```

---

### 2. Standard C Runtime (Glibc / TLS) — `sample`

A standard C executable compiled with glibc (`printf`, exit handlers, TLS initialization, and System V ABI calling convention at `_start`).

```bash
# 1. Build the target
make sample

# 2. Pack the binary
./woody_woodpacker sample

# 3. Execute the packed binary
./woody
```

**Expected output:**
```text
Hello, World!
```

---

### 3. Mid-Sized Program — `game` (Dungeon of Woody)

A complete ANSI terminal roguelike dungeon crawler (~864 KB statically linked binary). It features procedural room and corridor generation, monster AI, combat mechanics, fog-of-war, item pickups, and raw terminal mode via `termios`.

```bash
# 1. Build the target
make game

# 2. Pack the binary
./woody_woodpacker game
```

You can run the packed game in two modes:

#### A. Automated Simulation (`--test`)
Simulates 200 turns of AI exploration, dungeon traversal, and combat to verify memory safety and runtime stability:

```bash
./woody --test
```

**Expected output:**
```text
[*] Starting automated dungeon test (200 turns simulation)...
[+] Simulation completed successfully!
    Turns played : 200
    Floor reached: 1
    Player Level : 1
    Monsters slain: 1
    Gold gathered: 0
    Status       : SURVIVED
```

#### B. Interactive Terminal Gameplay
Launches the full interactive roguelike with ANSI color display:

```bash
./woody
```
- **Controls:** `W` `A` `S` `D` or Arrow Keys to move, `Q` to quit.
