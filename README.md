# Yerp

A modern, monolithic operating system designed for efficiency and modularity.
Yerp uses the Limine bootloader and boot protocol.

## Core Architecture
Supervisor is built with a strict architecture isolation approach, not allow any architecture specific code to leak into the core executive subsystems.

### Key Subsystems
* **Memory Management:**
    * **PFN Database:** Tracks physical page states and reference counts.
    * **Early Allocator:** Handles bootstrap-phase memory requirements.
* **Processor Management:**
    * **SPCB (Supervisor Processor Control Block):** Per-core data structures for managing CPU state.

## Project Structure
* `supervisor/source/arch/x86_64/`: Low-level CPU initialization and other x86_64 specific code.
* `supervisor/source/core/`: High-level executive logic (Memory, SPCBs, Sync).
* `supervisor/source/mm/`: Physical and virtual memory management.
* `supervisor/source/hal/`: Hardware Abstraction Layer facilitating the boot process.
* `supervisor/source/debug/`: Kernel logging and debugging facilities.

## Build Requirements
To build and run Supervisor, you will need:
- **LLVM/Clang** (Cross-compilation support for `x86_64-elf`)
- **GNU Make**
- **Git**
- **Internet connection or prefetched binaries**
- **QEMU**
- **mtools / libisoburn**

## Roadmap
- [ ] **Object Caching SLUB Allocator:** Modern kernel heap implementation.
- [ ] **Object Manager:** For centralized object lifetime tracking.
- [ ] **Symmetric Multiprocessing (SMP):** Full initialization of Application Processors (APs).
- [ ] **Scheduler:** Preemptive multi-threading and priority levels.
- [ ] **VFS:** Virtual File System layer for storage abstraction.
- [ ] **User Mode:** Ring 3 transition and System Call interface.
