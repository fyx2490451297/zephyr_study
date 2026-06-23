# Copilot instructions

## Project shape

This repository is a set of Zephyr study examples. The main target for active work is `Example/nrf52833DK`, which follows the same pattern as the older STM32 example: a thin top-level `CMakeLists.txt`, board-specific devicetree overlay, and separate `hardware/` and `app/` module trees.

`hardware/package_*` modules provide low-level board drivers or helpers, usually auto-initialized with `SYS_INIT`. `app/package_*` modules hold demo logic and learning examples such as BLE advertising/scanning, semaphores, message queues, mutexes, and UART transport.

## Build and flash

From `Example/nrf52833DK`:

```sh
west build -p always -b nrf52833dk/nrf52833
west build
west build -t clean
west flash --runner jlink
```

Use `west build -p always ...` after changing `prj.conf`, `CMakeLists.txt`, or the board overlay. Use plain `west build` for incremental rebuilds after `.c`/`.h` changes.
If you are already in the repo root, run `west build -p always -b nrf52833dk/nrf52833 Example/nrf52833DK`.

## Key conventions

- Kconfig symbols are named `PACKAGE_*` and gate the matching CMake targets with `if(CONFIG_PACKAGE_...)`.
- Package init code usually lives in `SYS_INIT(..., APPLICATION, CONFIG_*_INIT_PRIORITY)` so modules start automatically at boot.
- Demo threads are commonly created with `K_THREAD_DEFINE`; shared kernel objects are declared statically with `K_SEM_DEFINE`, `K_MSGQ_DEFINE`, or `K_MUTEX_DEFINE`.
- `main()` stays minimal; most behavior is pushed into package modules.
- Board-specific pin and alias setup belongs in `boards/nrf52833dk_nrf52833.overlay`. The LED package depends on the `led1`-`led4` aliases defined there.
- `package_mcu_transport` assumes `uart1` is enabled in devicetree and depends on `SERIAL` plus `UART_INTERRUPT_DRIVEN`.
- Use `LOG_MODULE_REGISTER(<module>, ...)` at the top of each C file so logs stay attributable to the owning package.

---

## Coding standards

### Naming

| Category | Convention | Example |
|---|---|---|
| Functions | `<module>_<verb>[_<noun>]` snake_case | `led_toggle()`, `mcu_transport_send()` |
| Public types / typedefs | `snake_case_t` | `led_id_t`, `key_event_t` |
| Function-pointer typedefs | `<module>_<role>_t` | `mcu_transport_rx_callback_t` |
| Enum members | `UPPER_SNAKE_CASE` with module prefix | `LED_CMD_ON`, `KEY_EVENT_PRESSED` |
| Macros / `#define` constants | `UPPER_SNAKE_CASE` | `UART1_NODE`, `TARGET_NAME_LEN` |
| Local variables | `snake_case`, descriptive noun | `rx_ring_buf`, `transport_ready` |
| Module-scope static variables | `snake_case`, no extra prefix | `static bool transport_ready;` |
| Thread entry functions | `<module>_<role>_thread` | `led_blink_thread`, `mcu_transport_rx_thread` |
| `SYS_INIT` init functions | `<module>_init` | `led_init`, `mcu_transport_init` |
| Kconfig symbols | `PACKAGE_<MODULE>[_<ITEM>]` all-caps | `CONFIG_PACKAGE_LED_INIT_PRIORITY` |

Forbidden: `camelCase`, single-letter names outside loop counters (`i`, `j`), Hungarian prefixes (`p_`, `g_`, `b_`).

### File and header structure

**Header files (`.h`)**

```c
#ifndef <MODULE>_H_          /* include guard: UPPER_SNAKE_CASE + _H_ suffix */
#define <MODULE>_H_

/* 1. Zephyr / system includes needed by the public API only */
#include <stdint.h>

/* 2. Public type definitions */
typedef enum { ... } foo_state_t;

/* 3. Public API — every function must have a Doxygen block */

/**
 * @brief One-line summary of what the function does.
 *
 * Optional longer description when the summary is not self-explanatory.
 *
 * @param name  Description of parameter.
 * @return 0 on success, negative errno on failure.
 */
int foo_do_something(foo_state_t state);

#endif /* <MODULE>_H_ */
```

**Source files (`.c`)**

```c
/* 1. Own header first */
#include "foo.h"

/* 2. Zephyr headers in angle brackets, alphabetical within groups */
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* 3. Register this file with the Zephyr log subsystem */
LOG_MODULE_REGISTER(foo, LOG_LEVEL_INF);

/* 4. Module-private macros */
#define FOO_TIMEOUT_MS  500

/* 5. Module-private types */

/* 6. Module-private variables — always static */
static bool foo_ready;

/* 7. Internal function implementations — always static */
static void foo_internal_helper(void) { ... }

/* 8. Public function implementations */
int foo_do_something(foo_state_t state) { ... }

/* 9. SYS_INIT registration last */
SYS_INIT(foo_init, APPLICATION, CONFIG_FOO_INIT_PRIORITY);
```

### Comments

- All **public API** in header files must have a Doxygen `/** @brief … @param … @return … */` block.
- **Non-obvious logic** inside `.c` files gets a `/* … */` block comment directly above the relevant lines. Do not restate what the code obviously does.
- Use `/* --- Section title --- */` dividers to group related logic inside long source files (see `mcu_transport.c` and `ble_scan_conn_demo.c` as reference).
- Inline `//` comments are forbidden; use `/* … */` style throughout.
- ISR bodies and spinlock-protected sections must have a short comment identifying the concurrency constraint.

### Error handling

- Every function that can fail returns `int` (0 = success, negative errno = failure).
- **Always** check the return value of Zephyr API calls. Log the error code before propagating it:

```c
err = bt_enable(NULL);
if (err) {
    LOG_ERR("bt_enable failed (%d)", err);
    return err;
}
```

- Use `LOG_ERR` for unrecoverable errors, `LOG_WRN` for degraded-but-continuing conditions, `LOG_INF` for normal lifecycle events, `LOG_DBG` for verbose diagnostic output.
- Validate function arguments at entry; return `-EINVAL` for bad input without logging (the caller is responsible for logging if needed).

### Threads and kernel objects

- Thread stack size and priority **must** be exposed as Kconfig integers (`CONFIG_PACKAGE_*_STACK_SIZE`, `CONFIG_PACKAGE_*_PRIORITY`) — never hard-coded in `K_THREAD_DEFINE`.
- Thread entry functions always declare three `void *` parameters and call `ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);` for any unused ones.
- Kernel objects shared across threads (`k_mutex`, `k_sem`, `k_msgq`) are always `static` and module-scope; never passed as raw pointers across package boundaries.
- ISR code must not call blocking APIs (`k_sleep`, `k_sem_take` with non-zero timeout, `k_mutex_lock`). Use `k_sem_give`, `ring_buf_put`, and `atomic_*` only.

### Module (package) structure

Every new `package_*` must provide:

1. `Kconfig` — a `bool` symbol `CONFIG_PACKAGE_<NAME>` (default `n`) with `help` text, plus sub-options for stack size, priority, and init priority where applicable.
2. `CMakeLists.txt` — guarded by `if(CONFIG_PACKAGE_<NAME>)`.
3. A single `SYS_INIT` entry point named `<module>_init`; no manual init calls from `main()`.
4. Public API only in `inc/<module>.h`; all implementation details `static` in `src/<module>.c`.

### `prj.conf` and Kconfig dependencies

When enabling a package that declares `depends on <SYMBOL>` in its Kconfig, **all dependency symbols must be explicitly set in `prj.conf`**. Kconfig silently forces an unsatisfied symbol to `n` — the module will not initialize, and no error is printed.

Example: `PACKAGE_MCU_TRANSPORT` requires both `SERIAL` and `UART_INTERRUPT_DRIVEN`. The correct `prj.conf` entry is:

```conf
# UART subsystem — required by PACKAGE_MCU_TRANSPORT
CONFIG_SERIAL=y
CONFIG_UART_INTERRUPT_DRIVEN=y
CONFIG_PACKAGE_MCU_TRANSPORT=y
```

Use `menuconfig` (run `west build -t menuconfig`) to inspect unmet dependencies; an `(!)` marker indicates a symbol forced off by its `depends on` chain.

### Devicetree and hardware abstraction

- Never hard-code GPIO port/pin numbers. Always use `DT_ALIAS` or `DT_NODELABEL` macros and let the `.overlay` file supply the actual hardware mapping.
- Add a `#if !DT_NODE_HAS_STATUS(NODE, okay)` compile-time guard when a package has a mandatory devicetree dependency (see `mcu_transport.c` for the pattern).
