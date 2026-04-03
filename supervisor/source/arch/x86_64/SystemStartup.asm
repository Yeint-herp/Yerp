.intel_syntax noprefix

.section .text, "ax", @progbits
.balign 16

.extern Ex_InitializeEarly

.global Supervisor_SystemStartup
.type Supervisor_SystemStartup, @function
Supervisor_SystemStartup:
    .cfi_startproc
    .cfi_undefined rip

    call Ex_InitializeEarly
    call Core_StackGuardInit

    .cfi_endproc
.size Supervisor_SystemStartup, . - Supervisor_SystemStartup

.global Ex_HaltCatchFire
.type Ex_HaltCatchFire, @function
Ex_HaltCatchFire:
    .cfi_startproc

    cli
1:
    hlt
    jmp 1b

    .cfi_endproc
.size Ex_HaltCatchFire, . - Ex_HaltCatchFire
