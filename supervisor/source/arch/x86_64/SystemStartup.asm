.intel_syntax noprefix

.section .text, "ax", @progbits
.balign 16

.extern Exec_InitializeEarly

.global Supervisor_SystemStartup
.type Supervisor_SystemStartup, @function
Supervisor_SystemStartup:
    .cfi_startproc
    .cfi_undefined rip

    call Exec_InitializeEarly
    call Core_StackGuardInit

#ifdef CI_BUILD
    call CiTest_Entry

    mov dx, 0xf4
    out dx, al
#endif

    .cfi_endproc
.size Supervisor_SystemStartup, . - Supervisor_SystemStartup

.global Exec_HaltCatchFire
.type Exec_HaltCatchFire, @function
Exec_HaltCatchFire:
    .cfi_startproc

    cli
1:
    hlt
    jmp 1b

    .cfi_endproc
.size Exec_HaltCatchFire, . - Exec_HaltCatchFire
