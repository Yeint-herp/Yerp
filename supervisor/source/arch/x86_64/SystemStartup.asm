.section .text, "ax", @progbits
.balign 16

.extern Hal_InitializeEarly

.global Supervisor_SystemStartup
.global Hal_HaltCatchFire

Supervisor_SystemStartup:
    call Hal_InitializeEarly
    call Core_StackGuardInit

Hal_HaltCatchFire:
    cli
1:
    hlt
    jmp 1b
