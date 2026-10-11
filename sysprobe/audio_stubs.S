.set noreorder
.text
.macro SOUND name, slot
.globl \name
.type \name,@function
\name:
 lui $3, 0x15
 ori $3, $3, \slot
 syscall
.size \name, .-\name
.endm
SOUND audio_open, 4
SOUND audio_command, 5
SOUND audio_submit, 6
SOUND audio_close, 7
.section .note.GNU-stack,"",@progbits
