    .set noreorder
    .set nomips16
    .set nomicromips
    .text
    .macro STUB name, group, index
    .balign 4
    .globl \name
    .type \name, @function
\name:
    lui $v1, \group
    ori $v1, $v1, \index
    syscall
    .size \name, .-\name
    .endm
    STUB ui_create_window, 3, 0x48
    STUB ui_set_focus, 3, 0x4a
    STUB ui_delete_window, 3, 0x4b
    STUB ui_dc_get, 3, 0x56
    STUB ui_default_callback, 3, 0x40
    STUB ui_create_font, 3, 0x2b
    STUB ui_destroy_font, 3, 0x2c
    STUB ui_default_fontface, 3, 0x58
    STUB ui_text_mode, 3, 0x59
    STUB ui_color, 3, 0x5a
    STUB ui_background_color, 3, 0x5b
    STUB ui_font_size, 3, 0x5e
    STUB ui_clear_rect, 3, 0x77
    STUB ui_text, 3, 0x6b
    STUB ui_draw_bitmap, 3, 0x73
    STUB ui_screen_update, 3, 0x3c
    STUB ui_font_file, 0x12, 0x7b
    STUB ui_get_msg, 0x12, 0x38
    STUB ui_dispatch_msg, 0x12, 0x3d
    STUB ui_register_dispatcher, 0x12, 0x3e
    STUB ui_unregister_dispatcher, 0x12, 0x3f
    STUB ui_exit_loop, 0x12, 0x40
    STUB ui_set_timer, 0x12, 0x0f
    STUB ui_kill_timer, 0x12, 0x14
    .section .note.GNU-stack,"",@progbits
