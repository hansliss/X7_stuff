/* Matches emulator.app sys_forbid_key_mapping at 0x6880d2f0. */
    .set noreorder
    .set nomips16
    .set nomicromips
    .text
    .balign 4
    .globl x7_forbid_key_mapping
    .type x7_forbid_key_mapping, @function
x7_forbid_key_mapping:
    lui $v1, 0x12
    ori $v1, $v1, 0xb9
    syscall
    .size x7_forbid_key_mapping, .-x7_forbid_key_mapping
    .section .note.GNU-stack,"",@progbits
