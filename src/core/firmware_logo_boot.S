/* Position-independent Cortex-M4 Thumb executor, embedded in the patcher.
 * clang --target=arm-none-eabi -mcpu=cortex-m4 -mthumb -c firmware_logo_boot.S
 * AAPCS: 36 saved + 188 local = 224 bytes; BLX stack is 8-byte aligned.
 * SRAM contains exactly one 160-byte row, plus arguments/timing state.
 * Four drawing stubs retain their v2..v7 offsets. CHIP ID has a separate
 * native-close wrapper and exit-only entry; no resident image/state allocation.
 * Entrance returns to the unmodified service wait; only its native close path
 * runs the exit. Boot/shutdown retain their one-shot entrance/hold/exit.
 * Descriptor words: rgb, fill, flag, watchdog, cycles/ms, entrance, entrance/ms,
 * compressed image, exit, hold/ms, exit/ms. Image encoding is unchanged.
 * r5 is the completed 1-based frame ordinal at frame_boundary, not progress.
 * Directional hard/soft wipes, slides and blinds share eight orientations.
 * All use visibility 0..16, including reversed exits.
 */
.syntax unified
.cpu cortex-m4
.thumb
.section .text,"ax",%progbits
.global firmware_logo_boot
.type firmware_logo_boot,%function
.thumb_func
firmware_logo_boot:
    adr.w r12, descriptor
    b.w executor
.global firmware_logo_shutdown
.thumb_func
firmware_logo_shutdown:
    adr.w r12, descriptor_shutdown
    b.w executor
.global firmware_logo_device_info
.thumb_func
firmware_logo_device_info:
    adr.w r12, descriptor_info
    b.w executor
.global firmware_logo_first
.thumb_func
firmware_logo_first:
    adr.w r12, descriptor_first
    b.w executor
.global firmware_logo_device_info_exit
.thumb_func
firmware_logo_device_info_exit:
info_exit_entry:
    adr.w r12, descriptor_info
    orr r12, r12, #1              /* aligned descriptor bit selects exit only */
    b.w executor
executor:
    push {r4-r11, lr}
    sub sp, #188
    and r0, r12, #1
    str r0, [sp, #180]             /* entrance / exit selector, stack-local */
    bic r4, r12, #1
    ldr r7, demcr
    ldr r0, [r7]
    str r0, [sp, #4]
    orr r0, r0, #0x01000000
    str r0, [r7]
    ldr r7, dwt
    ldr r0, [r7]
    str r0, [sp, #8]
    orr r0, r0, #1
    str r0, [r7]
    movs r0, #0
    str r0, [sp]
    ldr r0, [sp, #180]
    cbnz r0, info_exit
    movs r1, #0
    movs r2, #80
    movs r3, #160
    ldr r7, [r4, #4]
    blx r7
    movs r5, #1
    ldr r0, [r4, #20]
    str r0, [sp, #172]             /* current animation */
    cmp r0, #0
    ite eq
    moveq r0, #16
    movne r0, #1
    str r0, [sp, #176]             /* image visibility, 0..16 */
    ldr r1, [r4, #24]
    b start_timeline
info_exit:
    ldr r0, [r4, #20]
    cmp r0, #0
    ite eq
    moveq r5, #1
    movne r5, #16                 /* continue the preview's completed ordinal */
    b begin_exit
start_timeline:
    ldr r0, [r4, #16]
    mul r0, r1
    lsrs r0, #4
    mov r11, r0
    ldr r7, dwt
    ldr.w r10, [r7, #4]
phase:
    movs r6, #0
row:
    mov r3, r6
    ldr r0, [sp, #172]
    cmp r0, #29
    blo legacy_row_slide
    cmp r0, #32
    bhi load_row
    ldr r1, [sp, #176]
    rsb r1, r1, #16
    movs r2, #10
    muls r1, r2
    cmp r0, #31
    ite lo
    sublo r3, r1
    addhs r3, r1
    b check_row_slide
legacy_row_slide:
    cmp r0, #15
    blo load_row
    cmp r0, #16
    bhi load_row
    ldr r1, [sp, #176]
    rsb r1, r1, #16
    movs r2, #10
    muls r1, r2
    cmp r0, #15
    ite eq
    addeq r3, r1
    subne r3, r1
check_row_slide:
    cmp r3, #160
    bhs clear_row
load_row:
    ldr r7, [r4, #28]
    ldrh r0, [r7]
    add r1, r7, #4
    mov r8, r1
    ldrh r1, [r7, #2]
    mov r9, r1
    add r0, r7, r0, lsl #1
    adds r0, #4
    ldrh.w r0, [r0, r3, lsl #1]
    add r7, r0
    ldrh r0, [r7], #2
    add r2, sp, #12
    tst r0, #0x8000
    bne decode_indexed
    cbnz r0, decode_runs
    movs r3, #80
decode_raw:
    ldrh r1, [r7], #2
    strh r1, [r2], #2
    subs r3, #1
    bne decode_raw
    b decoded_row
decode_runs:
    ldrh r3, [r7], #2
    ldrh r1, [r7], #2
decode_repeat:
    strh r1, [r2], #2
    subs r3, #1
    bne decode_repeat
    subs r0, #1
    bne decode_runs
    b decoded_row
decode_indexed:
    movs r3, #0
decode_index:
    mul r1, r3, r9
    lsrs r0, r1, #3
    ldrb r0, [r7, r0]
    and r1, r1, #7
    lsrs r0, r1
    movs r1, #1
    lsl.w r1, r1, r9
    subs r1, #1
    ands r0, r1
    ldrh.w r0, [r8, r0, lsl #1]
    strh r0, [r2], #2
    adds r3, #1
    cmp r3, #80
    bne decode_index
    b decoded_row
clear_row:
    add r2, sp, #12
    movs r0, #0
    movs r3, #80
clear_pixel:
    strh r0, [r2], #2
    subs r3, #1
    bne clear_pixel
    b draw_row
decoded_row:
    add r7, sp, #12
    ldr r0, [sp, #172]
    mov r12, r0
    cmp r0, #0
    beq draw_row
    cmp r0, #15
    beq draw_row
    cmp r0, #16
    beq draw_row
    ldr r0, [sp, #176]
    mov r9, r0
    cmp r12, #11
    beq slide_right
    cmp r12, #14
    beq slide_left
    cmp r12, #29
    blo not_diagonal_slide
    cmp r12, #32
    bhi not_diagonal_slide
    tst r12, #1
    bne slide_right
    b slide_left
not_diagonal_slide:
    movs r0, #0
    mov r8, r0
clip_pixel:
    cmp r9, #0
    beq hidden
    cmp r12, #17
    bhs directional_pixel
    cmp r12, #1
    beq wipe_down
    cmp r12, #2
    beq wipe_right
    cmp r12, #3
    beq fade
    cmp r12, #8
    beq blinds
    cmp r12, #9
    beq dissolve
    cmp r12, #10
    beq soft_wipe
    cmp r12, #12
    beq checker
    cmp r12, #13
    beq diagonal
    lsl.w r0, r8, #1
    subs r0, #79
    it mi
    rsbmi r0, r0, #0
    lsls r1, r6, #1
    subs r1, #159
    it mi
    rsbmi r1, r1, #0
    cmp r12, #6
    beq center_horizontal
    cmp r12, #7
    beq center_vertical
    cmp r12, #5
    beq diamond
    muls r0, r0
    mla r0, r1, r1, r0
    mul r1, r9, r9
    movs r2, #124
    muls r1, r2
    cmp r0, r1
    bhi hidden
    b visible
diamond:
    adds r0, r1
    movs r1, #15
    mul r1, r9, r1
    cmp r0, r1
    bhs hidden
    b visible
center_horizontal:
    movs r1, #5
    mul r1, r9, r1
    cmp r0, r1
    bhs hidden
    b visible
center_vertical:
    movs r0, #10
    mul r0, r9, r0
    cmp r1, r0
    bhs hidden
    b visible
wipe_down:
    movs r0, #10
    mul r0, r9, r0
    cmp r6, r0
    bhs hidden
    b visible
wipe_right:
    movs r0, #5
    mul r0, r9, r0
    cmp r8, r0
    bhs hidden
    b visible
blinds:
    movs r0, #20
    udiv r1, r6, r0
    mls r1, r1, r0, r6
    movs r0, #5
    mul r0, r9, r0
    lsrs r0, #2
    cmp r1, r0
    bhs hidden
    b visible
dissolve:
    movs r0, #13
    mul r0, r8, r0
    movs r1, #7
    mla r0, r6, r1, r0
    and r0, r0, #15
    cmp r0, r9
    bhs hidden
    b visible
soft_wipe:
    movs r0, #11
    mul r0, r9, r0
    subs r0, r0, r6
    ble hidden
    cmp r0, #16
    bhs visible
    b shade
checker:
    movs r0, #10
    udiv r1, r8, r0
    mls r0, r1, r0, r8
    lsls r0, #3
    movs r2, #10
    udiv r0, r0, r2
    movs r2, #20
    udiv r2, r6, r2
    eors r1, r2
    and r1, r1, #1
    add r0, r0, r1, lsl #3
    cmp r0, r9
    bhs hidden
    b visible
diagonal:
    add r0, r6, r8, lsl #1
    movs r1, #20
    mul r1, r9, r1
    cmp r0, r1
    bhs hidden
    b visible
/* New direction IDs use mirrored normalized coordinates, shared by families. */
directional_pixel:
    adr.w r0, direction_modes
    sub r1, r12, #17
    ldrb r2, [r0, r1]
    and r3, r2, #7
    mov r0, r8
    mov r1, r6
    cmp r3, #3
    beq mirror_x
    cmp r3, #5
    beq mirror_x
    cmp r3, #7
    bne test_mirror_y
mirror_x:
    rsb r0, r0, #79
test_mirror_y:
    cmp r3, #1
    beq mirror_y
    cmp r3, #6
    beq mirror_y
    cmp r3, #7
    bne gradient_axis
mirror_y:
    rsb r1, r1, #159
gradient_axis:
    cmp r3, #2
    blo vertical_gradient
    lsls r0, #1
    cmp r3, #4
    blo gradient_ready
    adds r0, r1
    cmp r2, #8
    blo hard_gradient
    lsrs r0, #1
    b gradient_ready
vertical_gradient:
    mov r0, r1
gradient_ready:
    cmp r2, #8
    blo hard_axis
    cmp r2, #16
    bhs directional_blinds
    movs r1, #11
    mul r1, r9, r1
    subs r0, r1, r0
    ble hidden
    cmp r0, #16
    bhs visible
    b shade
hard_axis:
    movs r1, #10
    b hard_compare
hard_gradient:
    movs r1, #20
hard_compare:
    mul r1, r9, r1
    cmp r0, r1
    bhs hidden
    b visible
directional_blinds:
    movs r1, #20
    udiv r3, r0, r1
    mls r0, r3, r1, r0
    movs r1, #5
    mul r1, r9, r1
    lsrs r1, #2
    cmp r0, r1
    bhs hidden
    b visible
.balign 4
direction_modes:
    .byte 1,3,5,6,7,9,10,11,12,13,14,15
    .byte 4,5,6,7,17,18,19,20,21,22,23
    .balign 2
slide_right:
    rsb r0, r9, #16
    movs r1, #5
    muls r0, r1
    movs r1, #79
slide_pixel:
    subs r2, r1, r0
    ite mi
    movmi r2, #0
    ldrhpl r2, [r7, r2, lsl #1]
    strh r2, [r7, r1, lsl #1]
    subs r1, #1
    bpl slide_pixel
    b draw_row
slide_left:
    rsb r0, r9, #16
    movs r1, #5
    muls r0, r1
    movs r1, #0
slide_left_pixel:
    adds r2, r1, r0
    cmp r2, #80
    ite hs
    movhs r2, #0
    ldrhlo r2, [r7, r2, lsl #1]
    strh r2, [r7, r1, lsl #1]
    adds r1, #1
    cmp r1, #80
    blo slide_left_pixel
    b draw_row
fade:
    mov r0, r9
shade:
    ldrh r1, [r7]
    ubfx r2, r1, #11, #5
    mul r2, r2, r0
    lsrs r2, #4
    lsls r2, #11
    ubfx r3, r1, #5, #6
    mul r3, r3, r0
    lsrs r3, #4
    orr.w r2, r2, r3, lsl #5
    and r1, r1, #31
    mul r1, r1, r0
    lsrs r1, #4
    orrs r1, r2
    strh r1, [r7]
    b visible
hidden:
    movs r0, #0
    strh r0, [r7]
visible:
    adds r7, #2
    add r8, r8, #1
    cmp r8, #80
    bne clip_pixel
draw_row:
    add r7, sp, #12
    str r7, [sp]
    movs r0, #0
    mov r1, r6
    movs r2, #80
    movs r3, #1
    ldr r7, [r4]
    blx r7
    ldr r7, [r4, #12]
    blx r7
    adds r6, #1
    cmp r6, #160
    bne row
.global firmware_logo_frame_boundary
firmware_logo_frame_boundary:      /* completed ordinal r5 = 1..FrameCount */
    ldr r0, [sp, #172]
    cbz r0, entrance_done
    ldr r0, [sp, #176]
    ldr r1, [sp, #180]
    cbz r1, phase_deadline
    rsb r0, r0, #16
phase_deadline:
    mul r9, r11, r0
    bl wait_deadline
    ldr r0, [sp, #176]
    ldr r1, [sp, #180]
    cbnz r1, exit_next
    cmp r0, #16
    beq entrance_done
    adds r0, #1
    b next_frame
exit_next:
    cmp r0, #0
    beq complete
    subs r0, #1
next_frame:
    str r0, [sp, #176]
    adds r5, #1
    b phase
entrance_done:
    adr.w r0, descriptor_info
    cmp r4, r0
    beq complete                  /* native service owns the wait, never holdMs */
    ldr r7, dwt
    ldr.w r10, [r7, #4]
    ldr r0, [r4, #36]
    ldr r1, [r4, #16]
    mul r9, r0, r1
    bl wait_deadline
begin_exit:
    ldr r0, [r4, #32]
    cbz r0, complete
    str r0, [sp, #172]
    movs r0, #1
    str r0, [sp, #180]
    movs r0, #15
    str r0, [sp, #176]
    adds r5, #1
    ldr r1, [r4, #40]
    b start_timeline
wait_deadline:
    push {r0, lr}
wait_cycle:
    ldr r7, [r4, #12]
    blx r7
    ldr r7, dwt
    ldr r0, [r7, #4]
    sub r0, r0, r10
    cmp r0, r9
    blo wait_cycle
    pop {r0, pc}
complete:
    ldr r7, dwt
    ldr r0, [sp, #8]
    str r0, [r7]
    ldr r7, demcr
    ldr r0, [sp, #4]
    str r0, [r7]
    ldr r1, [r4, #8]
    cbz r1, restore_frame
    movs r0, #0
    strb r0, [r1]
restore_frame:
    add sp, #188
    pop {r4-r11, pc}
/* Native wrapper stack: 32 saved + 8 helper + 224 executor = 264 bytes max.
 * Only the stock page/history-subview bytes are read; no resident allocation.
 * Native parameters, callee-saved registers, LR and APSR survive unchanged. */
.macro save_native_call
    push {r0-r4, r12, lr}
    mrs r4, apsr
    push {r4}
.endm
.macro restore_native_call
    pop {r4}
    msr APSR_nzcvqg, r4
    pop {r0-r4, r12, lr}
.endm
.global firmware_logo_device_info_close
.thumb_func
firmware_logo_device_info_close:
    save_native_call
    adr.w r4, descriptor_close
    bl close_if_info
    restore_native_call
    b.w tail_main
.global firmware_logo_device_info_history_close
.thumb_func
firmware_logo_device_info_history_close:
    save_native_call
    bl info_exit_entry
    restore_native_call
    ldr.w pc, history_target
.global firmware_logo_device_info_timeout_close
.thumb_func
firmware_logo_device_info_timeout_close:
    save_native_call
    adr.w r4, descriptor_close
    bl close_if_info
    cbz r0, timeout_original
    /* The native timeout really leaves CHIP ID. Clear its real history subview
     * so the subsequent native Main call cannot exit twice; history-open resets it. */
    ldr r1, [r4, #8]
    movs r0, #0
    strb r0, [r1]
timeout_original:
    restore_native_call
    ldr.w pc, timeout_target
close_if_info:
    push {r4, lr}
    ldr r0, [r4]
    ldrb r0, [r0]
    ldr r1, [r4, #4]
    cmp r0, r1
    bne not_info
    ldr r0, [r4, #8]
    cbz r0, is_info
    ldrb r0, [r0]
    cmp r0, #1
    bne not_info
is_info:
    bl info_exit_entry
    movs r0, #1
    pop {r4, pc}
not_info:
    movs r0, #0
    pop {r4, pc}
.balign 4
demcr:
    .word 0xe000edfc
dwt:
    .word 0xe0001000
.global firmware_logo_descriptor
firmware_logo_descriptor:
descriptor:
    .space 44, 0
descriptor_shutdown:
    .space 44, 0
descriptor_info:
    .space 44, 0
descriptor_first:
    .space 44, 0
/* Close descriptor: page pointer/value, optional subview pointer, history
 * target, executable tail branch to original Main, original backlight target. */
descriptor_close:
    .space 12, 0
history_target:
    .space 4, 0
tail_main:
    .hword 0xf8df, 0xf000          /* LDR.W PC,[PC,#0]; original Main; padding */
    .space 8, 0
timeout_target:
    .space 4, 0
.size firmware_logo_boot, .-firmware_logo_boot
