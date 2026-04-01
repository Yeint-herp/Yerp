# Yerp

A modern, monolithic operating system designed for efficiency and modularity.
Yerp uses the Limine bootloader and boot protocol.

## Core Architecture

Supervisor is built with a strict architecture isolation approach, not allow any architecture specific code to leak into the core executive subsystems.

### Key Subsystems

* **Memory Management:**
  * **PFN Database:** Tracks physical page states and reference counts.
  * **Slab/Pool Allocator:** Object caching heap with a magazine layer.
  * **Virtual Address Space (VAS/VAD):** Virtual memory region tracking and management.
  * **Page Table Management:** Architecture-agnostic page table abstraction.
* **Processor Management:**
  * **SPCB (Supervisor Processor Control Block):** Per-core data structures for managing CPU state.
  * **IRQLs:** Interrupt request level management for synchronization.
* **Interrupt Handling:**
  * **Local APIC / I/O APIC:** Full APIC abstraction for interrupt routing and delivery.
  * **HPET:** High Precision Event Timer abstraction.
  * **IRQ dispatch:** Generic interrupt dispatch integrated with the APIC layer.
* **Object Manager:**
  * **Object lifetime:** Reference-counted supervisor objects with type-safe creation.
  * **Object directories:** Hierarchical namespace for supervisor objects.
  * **Handle tables:** Per-process handle management.
  * **Access Control Lists (ACLs):** Object-level access control lists.

## Project Structure

* `supervisor/source/arch/x86_64/`: Low-level CPU initialization and other x86_64 specific code.
* `supervisor/source/core/`: High-level executive logic (Memory, SPCBs, Sync).
* `supervisor/source/mm/`: Physical and virtual memory management.
* `supervisor/source/executive/`: Core supervisor executive facilitating the boot process.
* `supervisor/source/debug/`: Supervisor logging and debugging facilities.

## Build Requirements

To build and run Yerp, you will need:

* **LLVM/Clang** (Cross-compilation support for `x86_64-elf`)
* **GNU Make**
* **Git**
* **Internet connection or prefetched binaries**
* **QEMU**
* **mtools / libisoburn**

## Roadmap

* [x] **Object Caching Slab Allocator:** Magazine-based supervisor heap implementation.
* [x] **Object Manager:** Reference-counted objects with handle tables, directories, and ACLs.
* [x] **APIC / I/O APIC:** Interrupt controller abstraction and IRQ dispatch.
* [x] **HPET:** High precision timer abstraction.
* [x] **Virtual Address Space:** VAD-based virtual memory region management.
* [X] **Symmetric Multiprocessing (SMP):** Full initialization of Application Processors (APs).
* [X] **Deffered Procedure Calls (DPCs):** Support for dispatching DPCs when lowering IRQL to passive.
* [ ] **Per-core Timer Wheel:** Support for registering, cohering and acting upon timers.
* [ ] **LAPIC Timer Calibration:** Timer calibration using PIT / PM Timer fallbacks.
* [ ] **Dispatcher:** Preemptive multi-threading and priority levels.
* [ ] **VFS:** Virtual File System layer for storage abstraction.
* [ ] **User Mode:** Ring 3 transition and System Call interface.
