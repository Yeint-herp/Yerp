.section .text, "ax", @progbits
.balign 16

.extern Exec_InitializeEarly

.global Supervisor_SystemStartup
.global Exec_HaltCatchFire

Supervisor_SystemStartup:
    call Exec_InitializeEarly
    call Core_StackGuardInit

Exec_HaltCatchFire:
    cli
1:
    hlt
    jmp 1b
