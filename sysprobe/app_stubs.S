/* Exactly the vendor's three-instruction group-7 stubs. No jr $ra. */
    .set noreorder
    .set nomips16
    .set nomicromips
    .text
    .macro APP_STUB name, index
    .balign 4
    .globl \name
    .type \name, @function
\name:
    lui $v1, 7
    ori $v1, $v1, \index
    syscall
    .size \name, .-\name
    .endm
    APP_STUB app_get_pid, 0x04
    APP_STUB app_release_pid, 0x05
    APP_STUB app_get_process, 0x06
    APP_STUB app_change_thread_pid, 0x0e
    APP_STUB app_getpid, 0x16
    APP_STUB app_set_envp, 0x1d
    APP_STUB app_current_pid, 0x25
    APP_STUB app_insert_child, 0x27
    APP_STUB app_remove_child, 0x28
    APP_STUB app_thread_create, 0x60
    APP_STUB app_attr_init, 0x70
    APP_STUB app_attr_destroy, 0x71
    APP_STUB app_attr_inheritsched, 0x72
    APP_STUB app_attr_detachstate, 0x74
    APP_STUB app_exit, 0x82
    /* Group-18 stubs, verified against calculat.app's applib_init/quit. */
    .macro UI_STUB name, index
    .balign 4
    .globl \name
    .type \name, @function
\name:
    lui $v1, 0x12
    ori $v1, $v1, \index
    syscall
    .size \name, .-\name
    .endm
    UI_STUB app_ui_init, 0x36
    UI_STUB app_ui_quit, 0x37
    .section .note.GNU-stack,"",@progbits
