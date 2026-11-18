// clang --target=arm-none-eabi -mcpu=cortex-m4 -mthumb -c firmware_menu_boot.S
// Relocation-free .text; the builder supplies the trailing native descriptor.
#ifndef MENU_INFO
#define MENU_INFO 1
#endif
#ifndef MENU_VOLTAGE
#define MENU_VOLTAGE 1
#endif
#ifndef MENU_INFO_FIELDS
#define MENU_INFO_FIELDS 7
#endif
#ifndef MENU_PUFF_EDITOR
#define MENU_PUFF_EDITOR 0
#endif
#ifndef MENU_SCORE_EDITOR
#define MENU_SCORE_EDITOR 0
#endif
#ifndef MENU_RANK_SELECTOR
#define MENU_RANK_SELECTOR 0
#endif
#define MENU_ACHIEVEMENTS (MENU_SCORE_EDITOR || MENU_RANK_SELECTOR)
#define MENU_EDITORS (MENU_PUFF_EDITOR || MENU_ACHIEVEMENTS)
#define MENU_NUMERIC (MENU_PUFF_EDITOR || MENU_SCORE_EDITOR)
#if MENU_INFO || MENU_NUMERIC
#define MENU_DIGITS 1
#else
#define MENU_DIGITS 0
#endif
#define MENU_BATTERY (MENU_INFO && (MENU_INFO_FIELDS & 1))
#define MENU_RESISTANCE (MENU_INFO && (MENU_INFO_FIELDS & 2))
#define MENU_DURATION (MENU_INFO && (MENU_INFO_FIELDS & 4))
#define HAS_BATTERY (MENU_BATTERY || MENU_VOLTAGE)
#define HAS_COIL (MENU_RESISTANCE || MENU_VOLTAGE)
.syntax unified
.cpu cortex-m4
.thumb
.text
.global firmware_menu_queue, firmware_menu_main
#if MENU_VOLTAGE
.global firmware_menu_voltage
#endif
.global firmware_menu_start0, firmware_menu_start1, firmware_menu_start2
.global firmware_menu_save, firmware_menu_offsave, firmware_menu_hud, firmware_menu_paint_target
.global firmware_menu_descriptor
.equ ST_ACTIVE,4
.equ ST_SELECT,5
.equ ST_BATTERY,6
.equ ST_SEQUENCE,7
.equ ST_PRESS,8
.equ ST_DIRTY,9
.equ ST_POWER,12
.equ ST_MODE,16
.equ ST_LIMIT,20
.equ ST_LOCK,24
// INFO samples are inactive in editor state 4. No new RAM is reserved.
.equ ST_PUFF_VALUE,32
.equ ST_PUFF_ORIGINAL,36
.equ ST_PUFF_DIGIT,40
.equ ST_PUFF_ERROR,48
// Editors reuse inactive INFO/recognition words in the same 56-byte RTX box.
.equ ST_HOLD_START,28
.equ ST_HOLD_LAST,52
// ST_PRESS: 0 idle, 1 Plus, 2 Minus, 3 mixed-key barrier, 4 release barrier,
// 5 pending Fire. Fire acts only on release; >=700 ms cancels before any store.

// Common descriptor; optional field/module metadata is physically omitted.
.equ D_FLAGS,4
.equ D_GPIOA,8
.equ D_GPIOB,12
.equ D_PAGE,16
.equ D_POWER_A,20
.equ D_POWER_B,24
.equ D_MODE,28
.equ D_LIMIT,32
.equ D_LOCK,36
.equ D_FILL,40
.equ D_QUEUE,44
.equ D_MAIN,48
.equ D_WIDTH,52
.equ D_ADC_COMPARE,56
.equ D_START0,60
.equ D_START1,64
.equ D_START2,68
.equ D_ACTIVITY,72
.equ D_ACTIVITY_TIMER,76
.equ D_SCANNER,80
.equ D_QUEUE_BASE,84
.equ D_MASK0,88
.equ D_MASK1,92
.equ D_MASK2,96
.equ D_TICK,100
.equ D_SAVE,104
.equ D_OFFSAVE,108
.equ D_REVERSE,112
.equ D_ITEM_COUNT,116
.equ D_ITEMS,120
#if HAS_BATTERY
.equ D_BATTERY,128
#endif
#if HAS_COIL
.equ D_COIL,128+4*HAS_BATTERY
#endif
#if MENU_DURATION
.equ D_DURATION,128+4*HAS_BATTERY+4*HAS_COIL
#endif
#if MENU_VOLTAGE
.equ D_INITIAL_VOLTAGE,128+4*HAS_BATTERY+4*HAS_COIL+4*MENU_DURATION
.equ D_COMBINED,D_INITIAL_VOLTAGE+4
.equ D_VOLTAGE,D_INITIAL_VOLTAGE+8
#endif
.equ D_HUD,128+4*HAS_BATTERY+4*HAS_COIL+4*MENU_DURATION+12*MENU_VOLTAGE
#if MENU_EDITORS
.equ D_PUFF,D_HUD+4
.equ D_PUFF_FORMAT,D_PUFF+4
.equ D_PUFF_LIMIT,D_PUFF+8
.equ D_PUFF_SAVE,D_PUFF+12
.equ D_PUFF_FLASH,D_PUFF+16
.equ D_SETTINGS,D_PUFF+20
.equ D_SETTINGS_BYTES,D_PUFF+24
.equ D_PUFF_BATTERY,D_PUFF+28
.equ D_PUFF_GUARD_MASK,D_PUFF+32
#if MENU_PUFF_EDITOR && MENU_ACHIEVEMENTS
.equ D_SCORE,D_PUFF+36
.equ D_SCORE_FORMAT,D_PUFF+40
#endif
#endif

.thumb_func
firmware_menu_queue:
    push {r4-r7,lr}
    sub sp,#4
    adr.w r7,.Lmenu_descriptor
    ldr r3,[r7,#D_QUEUE]
    blx r3
    mov r4,r0
    bl menu_init
    bl menu_expire_sequence
    bl menu_event
    mov r4,r0
#if MENU_VOLTAGE
    bl menu_live_voltage
#endif
#if MENU_INFO
    bl menu_live_info
#endif
    ldrb r0,[r6,#ST_DIRTY]
    cbz r0,1f
    movs r0,#0
    strb r0,[r6,#ST_DIRTY]
    ldrb r0,[r6,#ST_ACTIVE]
    cmp r0,#1
    beq 2f
#if MENU_EDITORS
    cmp r0,#4
    bhs 2f
#endif
    cmp r0,#2
    bne 1f
2:
    bl menu_draw
1:  mov r0,r4
    add sp,#4
    pop {r4-r7,pc}

// Replaces a complete PUSH.W, not an instruction in an IT block. The inactive
// path executes its exact original PUSH in the builder's native trampoline.
.thumb_func
firmware_menu_main:
    push {r0-r7,r12,lr}
    adr.w r7,.Lmenu_descriptor
    bl menu_init
    ldr r0,[r7,#D_FLAGS]
    ldr r0,[r0]
    tst r0,#0x400
    beq 3f
    ldr r0,[r7,#D_PAGE]
    ldrb r0,[r0]
    cmp r0,#1
    bne 3f
    ldrb r0,[r6,#ST_ACTIVE]
    cmp r0,#1
    beq 2f
#if MENU_EDITORS
    cmp r0,#4
    bhs 2f
#endif
    cmp r0,#2
    bne 1f
2:
    bl menu_draw
    pop {r0-r7,r12,pc}
3:
#if MENU_EDITORS
    bl menu_puff_discard
#endif
    ldrb r0,[r6,#ST_ACTIVE]
    cmp r0,#0
    it ne
    movne r0,#3
    strb r0,[r6,#ST_ACTIVE]
1:  pop {r0-r7,r12,lr}
    adr.w r12,.Lmenu_descriptor
    ldr.w r12,[r12,#D_MAIN]
    bx r12

// Paint-only native entries share this guard. The battery trampoline is in D_HUD.
// Paint stubs pass their native continuation in r0 and an optional native exit
// in r1. The caller reserves a return slot and saves r0/r1 without changing APSR.
// Preserve every input register and APSR on both paths. State 3 remains native
// so exit/rearm can restore the complete original Main.
.thumb_func
firmware_menu_hud:
    sub.w sp,sp,#4
    push {r0,r1}
    adr.w r1,.Lmenu_descriptor
    ldr r0,[r1,#D_HUD]
    mov.w r1,#0
.thumb_func
firmware_menu_paint_target:
    push {r2,r3,r12,lr}
    mrs r3,apsr
    push {r3}
    adr.w r2,.Lmenu_descriptor
    ldr r2,[r2]
    ldrb r2,[r2,#ST_ACTIVE]
    cmp r2,#1
    beq 1f
    cmp r2,#2
    beq 1f
#if MENU_EDITORS
    cmp r2,#4
    bhs 1f
#endif
    b 2f
1:  cmp r1,#0
    it eq
    ldreq r1,[sp,#16]
    mov r0,r1
2:  str r0,[sp,#28]
    pop {r3}
    msr APSR_nzcvq,r3
    pop {r2,r3,r12,lr}
    pop {r0,r1}
    pop {pc}

#if MENU_VOLTAGE
// The original native voltage formatter accepts centivolts in r0. r1 retains
// its native redraw argument; all original layout and formatting stay native.
.thumb_func
firmware_menu_voltage:
    push {r0-r3,r12,lr}
    adr.w r2,.Lmenu_descriptor
    ldr r3,[r2]
    ldr r1,[r2,#D_FLAGS]
    ldr r1,[r1]
    tst r1,#0x400
    beq 1f
    ldr r1,[r2,#D_PAGE]
    ldrb r1,[r1]
    cmp r1,#1
    bne 1f
    ldrb r1,[r3,#ST_BATTERY]
    cbz r1,1f
    ldr r3,[r2,#D_BATTERY]
    ldrh r0,[r3]
    movs r1,#10
    udiv r0,r0,r1
    ldr r3,[r2,#D_COMBINED]
    cmp r3,#0
    ite eq
    streq r0,[sp]
    strne r0,[sp,#8]
1:  pop {r0-r3,r12,lr}
    adr.w r12,.Lmenu_descriptor
    ldr.w r12,[r12,#D_VOLTAGE]
    bx r12
#endif

// Defer ONLY the ordinary asynchronous save while a bounded code candidate is
// pending. Native power changes/rendering remain immediate. OFF is never
// deferred: it cancels recognition and follows its original native save path.
// The refcount excludes capture from an already-running writer. Only 24 extra
// PSP bytes are used; no new settings-save invocation or Flash protocol exists.
.macro SAVE_ENTER name,trampoline
.thumb_func
\name:
    // No additional MSP frame, even if a native exceptional caller reaches a
    // saver. Such a caller invalidates recognition and keeps its stock path.
    mrs r12,ipsr
    cmp r12,#0
    beq 1f
    adr.w r12,.Lmenu_descriptor
    ldr.w r0,[r12]
    movs r1,#0
    strb r1,[r0,#ST_SEQUENCE]
    strb r1,[r0,#ST_PRESS]
    strb r1,[r0,#11]
    ldr.w r12,[r12,#\trampoline]
    bx r12
1:
    push {r4-r7,lr}
    sub sp,#4
    adr.w r7,.Lmenu_descriptor
    bl menu_init
    mrs r5,primask
    cpsid i
.endm
SAVE_ENTER firmware_menu_save,D_SAVE
#if MENU_EDITORS
    ldrb r0,[r6,#10]
    cmp r0,#0
    bne menu_save_defer
    // Editor hold bytes are not recognition state. Never expire/reset them
    // through a background saver, nor serialize a staged edit asynchronously.
    ldrb r0,[r6,#ST_ACTIVE]
    cmp r0,#4
    bhs menu_save_defer
    cmp r0,#0
    bne menu_save_allowed
#endif
    ldrb r0,[r6,#ST_SEQUENCE]
    ldrb r1,[r6,#ST_PRESS]
    orrs r0,r1
    beq menu_save_allowed
    // Repeated mismatches/restarts cannot postpone normal saving forever:
    // a single continuous deferral epoch is capped at five native seconds.
    ldr r0,[r7,#D_TICK]
    ldr r0,[r0]
    ldr r1,[r6,#40]
    subs r0,r0,r1
    movw r1,#5000
    cmp r0,r1
    blo menu_save_defer
    movs r0,#0
    strb r0,[r6,#ST_SEQUENCE]
    strb r0,[r6,#ST_PRESS]
menu_save_allowed:
    movs r0,#0
    strb r0,[r6,#11]
    ldr r4,[r7,#D_SAVE]
    b menu_save_run
SAVE_ENTER firmware_menu_offsave,D_OFFSAVE
#if MENU_EDITORS
    bl menu_puff_discard
#endif
    movs r0,#0
    strb r0,[r6,#ST_SEQUENCE]
    strb r0,[r6,#ST_PRESS]
    strb r0,[r6,#11]
    ldr r4,[r7,#D_OFFSAVE]
menu_save_run:
    ldrb r0,[r6,#10]
    adds r0,#1
    strb r0,[r6,#10]
    msr primask,r5
    blx r4
    mov r2,r0
    // The stock writer may deliberately unmask IRQs. Preserve its POST-call
    // mask around cleanup, not the mask captured before the original function.
    mrs r5,primask
    cpsid i
    ldrb r0,[r6,#10]
    subs r0,#1
    strb r0,[r6,#10]
    msr primask,r5
    mov r0,r2
    add sp,#4
    pop {r4-r7,pc}
menu_save_defer:
    msr primask,r5
    add sp,#4
    pop {r4-r7,pc}

// Replace complete unconditional ORR.W instructions with B.W, preserving LR.
.macro START_ENTRY name,offset,maskoffset
.thumb_func
\name:
    sub.w sp,sp,#4
    push {r0-r3,r12,lr}
    mrs r1,apsr
    mrs r12,primask
    cpsid i
    adr.w r2,.Lmenu_descriptor
    ldr r3,[r2,#\offset]
    str r3,[sp,#24]
    ldr r3,[r2,#\maskoffset]
    orr r0,r0,r3
    b menu_start
.endm
START_ENTRY firmware_menu_start0,D_START0,D_MASK0
START_ENTRY firmware_menu_start1,D_START1,D_MASK1
START_ENTRY firmware_menu_start2,D_START2,D_MASK2
menu_start:
    ldr r3,[r2]
    ldrb r3,[r3,#ST_ACTIVE]
    cbz r3,1f
    bic r0,r0,#1
    b 2f
1:  orr r0,r0,#1
2:  ldr r3,[r2,#D_FLAGS]
    str r0,[r3]
    str r0,[sp]
    msr APSR_nzcvq,r1
    msr primask,r12
    pop {r0-r3,r12,lr}
    pop {pc}

// Native RTX time is a 1-ms counter in all sixteen images: SysTick reload is
// 95999 at 96 MHz or 71999 at 72 MHz, CTRL=7. Bounds survive WFI and wraparound.
menu_expire_sequence:
    push {r0-r3,lr}
    sub sp,#4
    ldrb r0,[r6,#ST_ACTIVE]
    cbnz r0,9f
    ldr r0,[r7,#D_TICK]
    ldr r0,[r0]
    ldr r1,[r6,#44]
    subs r1,r0,r1
    movw r2,#700
    cmp r1,r2
    bhi 1f
    ldr r1,[r6,#52]
    subs r1,r0,r1
    movw r2,#5000
    cmp r1,r2
    bls 2f
1:  movs r1,#0
    strb r1,[r6,#ST_SEQUENCE]
    strb r1,[r6,#ST_PRESS]
2:  cmp r4,#11
    beq 3f
    cmp r4,#12
    beq 3f
    cmp r4,#21
    beq 3f
    cmp r4,#22
    bne 9f
3:  str r0,[r6,#44]
9:  add sp,#4
    pop {r0-r3,pc}

// State is the excluded final 56-byte RTX fixed TCB box, never a guessed SRAM
// tail or movable graphics allocation. Scatter reset clears it. Fixed-pool
// initialization/reinitialization uses the reduced bound and cannot touch it.
menu_init:
    mrs r3,primask
    cpsid i
    ldr r6,[r7]
    ldr r0,[r6]
    movw r1,#0x4e55
    movt r1,#0x4d45
    cmp r0,r1
    beq 2f
    movs r0,#0
    movs r2,#0
1:  str r0,[r6,r2]
    adds r2,#4
    cmp r2,#56
    blo 1b
#if MENU_VOLTAGE
    ldr r0,[r7,#D_INITIAL_VOLTAGE]
    strb r0,[r6,#ST_BATTERY]
#endif
    str r1,[r6]
2:  msr primask,r3
    bx lr

// r0 points to a native settings member; revisions have 16- or 32-bit ABI.
menu_read:
    ldr r1,[r7,#D_WIDTH]
    cmp r1,#2
    ite eq
    ldrheq r0,[r0]
    ldrne r0,[r0]
    bx lr
menu_power_address:
    ldr r0,[r6,#ST_MODE]
    cmp r0,#2
    itt ne
    cmpne r0,#3
    ldrne r0,[r7,#D_POWER_A]
    it eq
    ldreq r0,[r7,#D_POWER_B]
    bx lr

// Only an idle, powered Main can be entered/rearmed. Background ADC and coil
// probes still execute; no TIM/ADC routine is skipped or replaced.
menu_idle:
    movw r2,#0xc04
    b menu_idle_common
menu_capture_idle:
    movw r2,#0x800
menu_idle_common:
    ldr r0,[r7,#D_FLAGS]
    ldr r0,[r0]
    tst r0,#1
    bne 1f
    tst r0,#0x400
    beq 1f
    ldr r0,[r7,#D_PAGE]
    ldrb r0,[r0]
    cmp r0,#1
    bne 1f
    ldr r0,[r7,#D_ADC_COMPARE]
    ldr r0,[r0]
    uxth r0,r0
    cbnz r0,1f
    ldr r0,[r7,#D_GPIOA]
    ldr r0,[r0,#16]
    tst r0,#16
    bne 1f
    ldr r0,[r7,#D_GPIOB]
    ldr r0,[r0,#16]
    mov r1,r2
    ands r0,r1
    cmp r0,r1
    bne 1f
    movs r0,#1
    bx lr
1:  movs r0,#0
    bx lr

// Snapshot before the first Plus is delivered to native code. A partial code
// is never delayed/replayed and retains ordinary clamping/power adjustment.
menu_capture:
    push {r4,lr}
    ldr r0,[r7,#D_MODE]
    bl menu_read
    str r0,[r6,#ST_MODE]
    cmp r0,#3
    bhi 9f
    ldr r0,[r7,#D_LIMIT]
    bl menu_read
    str r0,[r6,#ST_LIMIT]
    ldr r0,[r7,#D_LOCK]
    bl menu_read
    str r0,[r6,#ST_LOCK]
    bl menu_power_address
    bl menu_read
    str r0,[r6,#ST_POWER]
    cmp r0,#5
    blo 9f
    ldr r1,[r6,#ST_LIMIT]
    cmp r0,r1
    bhi 9f
    movs r0,#1
    pop {r4,pc}
9:  movs r0,#0
    pop {r4,pc}

menu_same_context:
    push {r4,lr}
    bl menu_idle
    cbz r0,9f
    ldr r0,[r7,#D_MODE]
    bl menu_read
    ldr r1,[r6,#ST_MODE]
    cmp r0,r1
    bne 9f
    ldr r0,[r7,#D_LIMIT]
    bl menu_read
    ldr r1,[r6,#ST_LIMIT]
    cmp r0,r1
    bne 9f
    ldr r0,[r7,#D_LOCK]
    bl menu_read
    ldr r1,[r6,#ST_LOCK]
    cmp r0,r1
    bne 9f
    movs r0,#1
    pop {r4,pc}
9:  movs r0,#0
    pop {r4,pc}

// Queue consumer/thread context only. There is no SPI work in TIM2 IRQ.
menu_event:
    push {r4,r5,lr}
    sub sp,#4
    ldr r0,[r7,#D_FLAGS]
    ldr r0,[r0]
    tst r0,#0x400
    beq menu_cede
    ldr r0,[r7,#D_PAGE]
    ldrb r0,[r0]
    cmp r0,#1
    bne menu_cede
    ldrb r0,[r6,#ST_ACTIVE]
    cbz r0,1f
    cmp r4,#0
    beq 1f
    // Exact native activity stores, only on actual menu input events.
    ldr r1,[r7,#D_ACTIVITY]
    movs r0,#1
    strb r0,[r1]
    ldr r1,[r7,#D_ACTIVITY_TIMER]
    movs r0,#0
    strh r0,[r1]
    // All sixteen stock profiles use this LCD backlight compare register.
    // Restore the captured native brightness only for actual input, never polls.
    movw r1,#0x2c3c
    movt r1,#0x4001
    ldr r0,[r6,#44]
    strh r0,[r1]
    cmp r4,#2
    bne 1f
    bl menu_cancel_fire_clicks
1:
    ldrb r0,[r6,#ST_ACTIVE]
    cmp r0,#3
    beq menu_rearm
#if MENU_INFO
    cmp r0,#2
    beq menu_info_event
#endif
#if MENU_EDITORS
    cmp r0,#4
    bhs menu_editor_event
#endif
    cmp r0,#1
    beq menu_open_event
    cmp r4,#0
    beq menu_event_keep
    // Native per-key click summaries arrive after their press/release pair.
    // They are not new physical input and must not interrupt the code.
    cmp r4,#15
    blo menu_physical_event
    cmp r4,#19
    bls menu_event_keep
    cmp r4,#25
    blo menu_physical_event
    cmp r4,#29
    bls menu_event_keep
menu_physical_event:
    cmp r4,#21
    beq menu_plus_press
    cmp r4,#11
    beq menu_minus_press
    cmp r4,#22
    beq menu_plus_release
    cmp r4,#12
    beq menu_minus_release
menu_reset:
    movs r0,#0
    strb r0,[r6,#ST_SEQUENCE]
    strb r0,[r6,#ST_PRESS]
menu_event_keep:
    mov r0,r4
menu_event_done:
    add sp,#4
    pop {r4,r5,pc}
menu_cede:
#if MENU_EDITORS
    bl menu_puff_discard
#endif
    ldrb r0,[r6,#ST_ACTIVE]
    cmp r0,#0
    it ne
    movne r0,#3
    strb r0,[r6,#ST_ACTIVE]
    b menu_reset
menu_plus_press:
    ldrb r0,[r6,#ST_SEQUENCE]
    tst r0,#1
    bne menu_restart_plus
    cbnz r0,1f
menu_restart_plus:
    mrs r5,primask
    cpsid i
    movs r0,#0
    strb r0,[r6,#ST_SEQUENCE]
    ldrb r0,[r6,#10]
    cmp r0,#0
    bne menu_capture_cancel
    bl menu_capture_idle
    cmp r0,#0
    beq menu_capture_cancel
    bl menu_capture
    cmp r0,#0
    beq menu_capture_cancel
    ldr r0,[r7,#D_TICK]
    ldr r0,[r0]
    str r0,[r6,#52]
    str r0,[r6,#48]
    ldrb r1,[r6,#11]
    cmp r1,#0
    it eq
    streq r0,[r6,#40]
    movs r1,#1
    strb r1,[r6,#11]
    movs r0,#1
    strb r0,[r6,#ST_PRESS]
    msr primask,r5
    b menu_event_keep
menu_capture_cancel:
    msr primask,r5
    b menu_reset
1:  movs r0,#1
    strb r0,[r6,#ST_PRESS]
    ldr r0,[r7,#D_TICK]
    ldr r0,[r0]
    str r0,[r6,#48]
    b menu_event_keep
menu_minus_press:
    ldrb r0,[r6,#ST_SEQUENCE]
    tst r0,#1
    beq menu_reset
    movs r0,#2
    strb r0,[r6,#ST_PRESS]
    ldr r0,[r7,#D_TICK]
    ldr r0,[r0]
    str r0,[r6,#48]
    b menu_event_keep
menu_plus_release:
    ldrb r0,[r6,#ST_PRESS]
    cmp r0,#1
    bne menu_reset
    b menu_release
menu_minus_release:
    ldrb r0,[r6,#ST_PRESS]
    cmp r0,#2
    bne menu_reset
menu_release:
    ldr r0,[r7,#D_TICK]
    ldr r0,[r0]
    ldr r1,[r6,#48]
    subs r0,r0,r1
    movw r1,#400
    cmp r0,r1
    bhi menu_reset
    movs r0,#0
    strb r0,[r6,#ST_PRESS]
    ldrb r0,[r6,#ST_SEQUENCE]
    adds r0,#1
    strb r0,[r6,#ST_SEQUENCE]
    cmp r0,#5
    bne menu_event_keep
    bl menu_same_context
    cmp r0,#0
    beq menu_reset
    // START filters see the inhibit before the second idle/context check.
    // Preserve the caller's PRIMASK rather than unconditionally enabling IRQs.
    mrs r5,primask
    cpsid i
    movs r0,#1
    strb r0,[r6,#ST_ACTIVE]
    dmb sy
    bl menu_same_context
    cbz r0,menu_entry_cancel
    bl menu_power_address
    ldr r1,[r6,#ST_POWER]
    ldr r2,[r7,#D_WIDTH]
    cmp r2,#2
    ite eq
    strheq r1,[r0]
    strne r1,[r0]
    // Recognition timestamps are unused while a menu is active. Preserve the
    // user's native brightness here so sleep/wake does not invent a new level.
    movw r0,#0x2c3c
    movt r0,#0x4001
    ldrh r0,[r0]
    str r0,[r6,#44]
    movs r0,#0
    strb r0,[r6,#ST_SEQUENCE]
    strb r0,[r6,#11]
    strb r0,[r6,#ST_SELECT]
    movs r0,#1
    strb r0,[r6,#ST_DIRTY]
    msr primask,r5
    b menu_consume
menu_entry_cancel:
    movs r0,#0
    strb r0,[r6,#ST_ACTIVE]
    msr primask,r5
    b menu_reset
menu_open_event:
#if MENU_EDITORS
    // Swallow delayed Fire summaries after an editor finishes or is canceled.
    ldrb r0,[r6,#ST_PRESS]
    cmp r0,#4
    bne 1f
    ldr r0,[r7,#D_GPIOB]
    ldr r0,[r0,#16]
    movw r1,#0xc04
    ands r0,r1
    cmp r0,r1
    bne menu_consume
    ldr r0,[r7,#D_SCANNER]
    ldrb r0,[r0,#0x11]
    cmp r0,#0
    bne menu_consume
    bl menu_cancel_fire_clicks
    movs r0,#0
    strb r0,[r6,#ST_PRESS]
    b menu_consume
1:
#endif
    cmp r4,#22
    beq menu_navigate
    cmp r4,#12
    beq menu_navigate
    cmp r4,#2
    bne menu_consume
    ldrb r0,[r6,#ST_SELECT]
    add r1,r7,#D_ITEMS
    ldrb r0,[r1,r0]
#if MENU_INFO
    cmp r0,#0
    beq menu_show_info
#endif
#if MENU_VOLTAGE
    cmp r0,#1
    beq menu_toggle_voltage
#endif
#if MENU_PUFF_EDITOR
    cmp r0,#3
    beq menu_show_puffs
#endif
#if MENU_SCORE_EDITOR
    cmp r0,#4
    beq menu_show_score
#endif
#if MENU_RANK_SELECTOR
    cmp r0,#5
    beq menu_show_rank
#endif
    movs r0,#3
    strb r0,[r6,#ST_ACTIVE]
    b menu_consume
#if MENU_INFO
menu_show_info:
    movs r0,#2
    strb r0,[r6,#ST_ACTIVE]
    b menu_dirty
#endif
#if MENU_VOLTAGE
menu_toggle_voltage:
    ldrb r0,[r6,#ST_BATTERY]
    eor r0,r0,#1
    strb r0,[r6,#ST_BATTERY]
    b menu_dirty
#endif
// Zero means Plus DOWN / Minus UP; one reverses physical release actions.
menu_navigate:
    ldr r0,[r7,#D_REVERSE]
    cmp r4,#22
    it ne
    eorne r0,r0,#1
    cbnz r0,menu_previous
menu_next:
    ldrb r0,[r6,#ST_SELECT]
    adds r0,#1
    ldr r1,[r7,#D_ITEM_COUNT]
    cmp r0,r1
    it hs
    movhs r0,#0
    strb r0,[r6,#ST_SELECT]
    b menu_dirty
menu_previous:
    ldrb r0,[r6,#ST_SELECT]
    ldr r1,[r7,#D_ITEM_COUNT]
    subs r1,#1
    cmp r0,#0
    ite eq
    moveq r0,r1
    subne r0,#1
    strb r0,[r6,#ST_SELECT]
    b menu_dirty
#if MENU_EDITORS
// Poll physical levels, not native click/release summaries. Single-key presses
// step immediately; repeats start at 350 ms, then 120 ms (50 ms after 1 s).
// Unsigned tick subtraction is wrap-safe. Mixed keys stay blocked until release.
menu_editor_event:
    ldr r0,[r7,#D_GPIOB]
    ldr r0,[r0,#16]
    mvn r0,r0
    movw r1,#0xc04
    ands r0,r1
    ldrb r1,[r6,#ST_PRESS]
    cmp r1,#3
    beq menu_editor_barrier
    cmp r1,#4
    beq menu_editor_barrier
    cmp r0,#0
    beq menu_editor_release
    cmp r0,#4
    beq 1f
    cmp r0,#0x400
    beq 2f
    cmp r0,#0x800
    beq 3f
    b menu_editor_mixed
1:  movs r2,#1
    b 4f
2:  movs r2,#2
    b 4f
3:  movs r2,#5
4:  cmp r1,#0
    beq menu_editor_press
    cmp r1,r2
    bne menu_editor_mixed
    ldr r0,[r7,#D_TICK]
    ldr r0,[r0]
    ldr r3,[r6,#ST_HOLD_START]
    subs r3,r0,r3
    cmp r2,#5
    beq menu_editor_fire_held
    movw r1,#350
    cmp r3,r1
    blo menu_consume
    movs r1,#120
    cmp r3,#1000
    it hs
    movhs r1,#50
    ldr r3,[r6,#ST_HOLD_LAST]
    subs r3,r0,r3
    cmp r3,r1
    blo menu_consume
    str r0,[r6,#ST_HOLD_LAST]
    b menu_editor_step
menu_editor_press:
    strb r2,[r6,#ST_PRESS]
    ldr r0,[r7,#D_TICK]
    ldr r0,[r0]
    str r0,[r6,#ST_HOLD_START]
    str r0,[r6,#ST_HOLD_LAST]
    cmp r2,#5
    beq menu_consume
menu_editor_step:
    movs r4,#12
    cmp r2,#1
    it eq
    moveq r4,#22
#if MENU_RANK_SELECTOR
    ldrb r0,[r6,#ST_ACTIVE]
    cmp r0,#6
    beq menu_rank_change
#endif
#if MENU_NUMERIC
    ldrb r0,[r6,#ST_PUFF_ERROR]
    cmp r0,#2
    beq menu_consume
    b menu_puff_change
#endif
menu_editor_fire_held:
    movw r0,#700
    cmp r3,r0
    blo menu_consume
    b menu_editor_cancel
menu_editor_release:
    movs r0,#0
    strb r0,[r6,#ST_PRESS]
    cmp r1,#5
    bne menu_consume
    ldr r0,[r7,#D_TICK]
    ldr r0,[r0]
    ldr r1,[r6,#ST_HOLD_START]
    subs r0,r1
    movw r1,#700
    cmp r0,r1
    bhs menu_editor_cancel
    bl menu_cancel_fire_clicks
#if MENU_RANK_SELECTOR
    ldrb r0,[r6,#ST_ACTIVE]
    cmp r0,#6
    beq menu_rank_confirm
#endif
#if MENU_NUMERIC
    b menu_puff_fire
#endif
menu_editor_mixed:
    movs r0,#3
    strb r0,[r6,#ST_PRESS]
    b menu_consume
menu_editor_barrier:
    cmp r0,#0
    bne menu_consume
    movs r0,#0
    strb r0,[r6,#ST_PRESS]
    b menu_consume
menu_editor_cancel:
    // Root retains heater inhibit until release; never enter the save path.
    movs r0,#1
    strb r0,[r6,#ST_ACTIVE]
    b menu_puff_back
// Counter ABI: 0=halfword, 1=HIGH then LOW halfwords, 2=native word.
menu_editor_address:
#if MENU_PUFF_EDITOR && MENU_ACHIEVEMENTS
    ldrb r2,[r6,#ST_ACTIVE]
    cmp r2,#4
    bne 1f
#endif
    ldr r1,[r7,#D_PUFF]
    ldr r2,[r7,#D_PUFF_FORMAT]
    bx lr
#if MENU_PUFF_EDITOR && MENU_ACHIEVEMENTS
1:  ldr r1,[r7,#D_SCORE]
    ldr r2,[r7,#D_SCORE_FORMAT]
    bx lr
#endif
#if MENU_NUMERIC
menu_editor_limit:
#if MENU_ACHIEVEMENTS
    ldrb r12,[r6,#ST_ACTIVE]
    cmp r12,#4
    bne 1f
#endif
    ldr r12,[r7,#D_PUFF_LIMIT]
    bx lr
#if MENU_ACHIEVEMENTS
1:  movw r12,#0x869f
    movt r12,#1
    bx lr
#endif
#endif
#if MENU_ACHIEVEMENTS
// Convert only on confirmation. Points retain fractional native units; rank resets them.
menu_editor_raw:
    ldr r0,[r6,#ST_PUFF_VALUE]
    ldrb r1,[r6,#ST_ACTIVE]
    cmp r1,#4
    beq 2f
    movw r1,#1000
    muls r0,r1,r0
#if MENU_SCORE_EDITOR
    ldrb r1,[r6,#ST_ACTIVE]
    cmp r1,#5
    bne 2f
    movw r1,#1000
    ldr r2,[r6,#ST_PUFF_ORIGINAL]
    udiv r3,r2,r1
    mls r2,r3,r1,r2
    adds r0,r2
#endif
2:  bx lr
#endif
#if MENU_RANK_SELECTOR
menu_show_rank:
    movs r0,#6
    strb r0,[r6,#ST_ACTIVE]
    movs r0,#4
    strb r0,[r6,#ST_PRESS]
    bl menu_puff_read
    str r0,[r6,#ST_PUFF_ORIGINAL]
    movw r1,#1000
    udiv r0,r0,r1
    adr.w r1,menu_rank_points
    movs r2,#7
1:  ldr r3,[r1,r2,lsl #2]
    cmp r0,r3
    bhs 2f
    subs r2,#1
    b 1b
2:  str r2,[r6,#ST_PUFF_DIGIT]
    movs r0,#0
    str r0,[r6,#ST_PUFF_ERROR]
    b menu_dirty
menu_rank_change:
    ldr r0,[r7,#D_REVERSE]
    cmp r4,#22
    it ne
    eorne r0,r0,#1
    ldr r1,[r6,#ST_PUFF_DIGIT]
    cmp r0,#0
    ite eq
    addeq r1,#1
    addne r1,#8
    cmp r1,#9
    it hs
    subhs r1,#9
    str r1,[r6,#ST_PUFF_DIGIT]
    movs r0,#0
    str r0,[r6,#ST_PUFF_ERROR]
    b menu_dirty
menu_rank_confirm:
    ldr r1,[r6,#ST_PUFF_DIGIT]
    cmp r1,#8
    beq menu_rank_back
    adr.w r0,menu_rank_points
    ldr r0,[r0,r1,lsl #2]
    str r0,[r6,#ST_PUFF_VALUE]
    b menu_puff_confirm
menu_rank_back:
    movs r0,#1
    strb r0,[r6,#ST_ACTIVE]
    b menu_puff_back
.balign 4
menu_rank_points: .word 0,478,2250,6000,11800,21400,31900,46550
#endif
menu_puff_read:
    push {r3,lr}
    bl menu_editor_address
    pop {r3,lr}
    cmp r2,#2
    beq 2f
    ldrh r0,[r1]
    cmp r2,#0
    beq 3f
    ldrh r1,[r1,#2]
    orr r0,r1,r0,lsl #16
    bx lr
2:  ldr r0,[r1]
3:  bx lr
menu_puff_store:
    push {r3,lr}
    bl menu_editor_address
    pop {r3,lr}
    cmp r2,#2
    beq 2f
    cmp r2,#0
    beq 1f
    lsrs r2,r0,#16
    strh r2,[r1]
    strh r0,[r1,#2]
    bx lr
1:  strh r0,[r1]
    bx lr
2:  str r0,[r1]
    bx lr
#if MENU_PUFF_EDITOR
menu_show_puffs:
    movs r0,#4
    strb r0,[r6,#ST_ACTIVE]
    b menu_show_numeric
#endif
#if MENU_SCORE_EDITOR
menu_show_score:
    movs r0,#5
    strb r0,[r6,#ST_ACTIVE]
#endif
#if MENU_NUMERIC
menu_show_numeric:
    movs r0,#4
    strb r0,[r6,#ST_PRESS]
    bl menu_puff_read
    str r0,[r6,#ST_PUFF_ORIGINAL]
#if MENU_SCORE_EDITOR
    ldrb r1,[r6,#ST_ACTIVE]
    cmp r1,#5
    itt eq
    moveq r1,#1000
    udiveq r0,r0,r1
#endif
    movw r1,#0x869f
    movt r1,#1
    str r0,[r6,#ST_PUFF_VALUE]
    movs r2,#0
    str r2,[r6,#ST_PUFF_DIGIT]
    cmp r0,r1
    bhi menu_puff_unrepresentable
    movs r0,#0
    str r0,[r6,#ST_PUFF_ERROR]
    b menu_dirty
#endif
#if MENU_NUMERIC
menu_puff_fire:
    ldrb r0,[r6,#ST_PUFF_ERROR]
    cmp r0,#2
    beq menu_puff_back
    ldrb r0,[r6,#ST_PUFF_DIGIT]
    cmp r0,#4
    beq menu_puff_confirm
    adds r0,#1
    strb r0,[r6,#ST_PUFF_DIGIT]
    b menu_dirty
menu_puff_change:
    ldrb r0,[r6,#ST_PUFF_DIGIT]
    adr.w r1,menu_puff_places
    ldr r1,[r1,r0,lsl #2]
    ldr r0,[r6,#ST_PUFF_VALUE]
    udiv r2,r0,r1
    movs r3,#10
    udiv r5,r2,r3
    mls r2,r5,r3,r2
    mls r0,r2,r1,r0
    movs r5,#10
    strb r5,[r6,#ST_SEQUENCE]
1:  cmp r4,#22
    ite eq
    addeq r2,#1
    addne r2,#9
    udiv r5,r2,r3
    mls r2,r5,r3,r2
    mla r5,r2,r1,r0
    bl menu_editor_limit
    cmp r5,r12
    bls 2f
    ldrb r5,[r6,#ST_SEQUENCE]
    subs r5,#1
    strb r5,[r6,#ST_SEQUENCE]
    bne 1b
    b 3f
2:  str r5,[r6,#ST_PUFF_VALUE]
3:  movs r0,#0
    strb r0,[r6,#ST_SEQUENCE]
    strb r0,[r6,#ST_PUFF_ERROR]
    b menu_dirty
#endif
menu_puff_confirm:
#if MENU_ACHIEVEMENTS
    bl menu_editor_raw
#else
    ldr r0,[r6,#ST_PUFF_VALUE]
#endif
    ldr r1,[r6,#ST_PUFF_ORIGINAL]
    cmp r0,r1
    beq menu_puff_back
    // Flash transactions are owned by the native UI thread, never exceptions
    // or a caller already holding an interrupt exclusion.
    mrs r0,ipsr
    cmp r0,#0
    bne menu_puff_error
    mrs r0,primask
    cmp r0,#0
    bne menu_puff_error
    // Read the exact native readiness flags and ADC gate before any RAM store.
    bl menu_puff_ready
    cmp r0,#0
    beq menu_puff_error
    mrs r5,primask
    cpsid i
    ldrb r0,[r6,#10]
    cmp r0,#0
    bne 8f
    bl menu_puff_ready
    cmp r0,#0
    beq 8f
    movs r0,#1
    strb r0,[r6,#10]
#if MENU_ACHIEVEMENTS
    bl menu_editor_raw
#else
    ldr r0,[r6,#ST_PUFF_VALUE]
#endif
    bl menu_puff_store
    dmb sy
    msr primask,r5
    ldr r0,[r7,#D_PUFF_SAVE]
    blx r0
    bl menu_puff_read
    mov r4,r0
#if MENU_ACHIEVEMENTS
    bl menu_editor_raw
#else
    ldr r0,[r6,#ST_PUFF_VALUE]
#endif
    cmp r0,r4
    beq 1f
    movs r0,#0
    b 2f
1:
    // Native saver return values are not a success ABI on all profiles.
    // Validate the complete serialized native CRC record in either stock slot.
    ldr r4,[r7,#D_PUFF_FLASH]
    bl menu_puff_verify
    cmp r0,#0
    bne 2f
    ldr r4,[r7,#D_PUFF_FLASH]
    ldr r1,[r7,#D_PUFF_FORMAT]
    cmp r1,#2
    ite eq
    addeq r4,#0x800
    addne r4,#0x400
    bl menu_puff_verify
2:  mov r4,r0
    mrs r5,primask
    cpsid i
    movs r0,#0
    strb r0,[r6,#10]
    cmp r4,#0
    bne 3f
    ldr r0,[r6,#ST_PUFF_ORIGINAL]
    bl menu_puff_store
3:  msr primask,r5
    cmp r4,#0
    beq menu_puff_error
menu_puff_back:
    ldrb r1,[r6,#ST_ACTIVE]
    movs r0,#0
    str r0,[r6,#ST_PUFF_VALUE]
    str r0,[r6,#ST_PUFF_ORIGINAL]
    str r0,[r6,#ST_PUFF_DIGIT]
    str r0,[r6,#ST_PUFF_ERROR]
    movs r0,#4
    strb r0,[r6,#ST_PRESS]
    movs r0,#1
#if MENU_RANK_SELECTOR
    cmp r1,#6
    it eq
    moveq r0,#3
#endif
    strb r0,[r6,#ST_ACTIVE]
    b menu_dirty
8:  msr primask,r5
menu_puff_error:
    // Keep editing on a failed commit; never display a false saved state.
    movs r0,#1
    strb r0,[r6,#ST_PUFF_ERROR]
    b menu_dirty
#if MENU_NUMERIC
menu_puff_unrepresentable:
    movs r0,#2
    strb r0,[r6,#ST_PUFF_ERROR]
    b menu_dirty
#endif
menu_puff_discard:
    ldrb r0,[r6,#ST_ACTIVE]
    cmp r0,#4
    blo 1f
    movs r0,#0
    str r0,[r6,#ST_PUFF_VALUE]
    str r0,[r6,#ST_PUFF_ORIGINAL]
    str r0,[r6,#ST_PUFF_DIGIT]
    str r0,[r6,#ST_PUFF_ERROR]
    movs r0,#3
    strb r0,[r6,#ST_ACTIVE]
1:  bx lr
menu_puff_ready:
    push {r4,lr}
    bl menu_idle
    cbz r0,9f
    ldr r0,[r7,#D_FLAGS]
    ldr r0,[r0]
    ldr r1,[r7,#D_PUFF_GUARD_MASK]
    tst r0,r1
    bne 8f
    ldr r0,[r7,#D_PUFF_FORMAT]
    cmp r0,#2
    beq 1f
    movw r0,#0x42
    movt r0,#0x2000
    ldrb r0,[r0]
    cbnz r0,8f
1:  ldr r0,[r7,#D_PUFF_BATTERY]
    ldrh r0,[r0]
    movw r1,#3000
    cmp r0,r1
    blo 8f
2:  movs r0,#1
    pop {r4,pc}
8:  movs r0,#0
9:  pop {r4,pc}
menu_puff_verify:
    ldr r0,[r7,#D_SETTINGS]
    ldr r1,[r7,#D_SETTINGS_BYTES]
    ldr r2,[r7,#D_PUFF_FORMAT]
1:  cmp r1,#0
    beq 4f
    cmp r2,#2
    bne 2f
    ldr r3,[r0],#4
    rev r3,r3
    ldr r12,[r4],#4
    cmp r3,r12
    bne 5f
    subs r1,#4
    b 1b
2:  ldrb r3,[r0],#1
    ldrb r12,[r4],#1
    cmp r3,r12
    bne 5f
    subs r1,#1
    b 1b
4:  movs r0,#1
    bx lr
5:  movs r0,#0
    bx lr
.balign 4
#if MENU_NUMERIC
menu_puff_places: .word 10000,1000,100,10,1
#endif
#endif
#if MENU_INFO
menu_info_event:
    cmp r4,#2
    bne menu_consume
    movs r0,#1
    strb r0,[r6,#ST_ACTIVE]
#endif
menu_dirty:
    movs r0,#1
    strb r0,[r6,#ST_DIRTY]
menu_consume:
    movs r0,#0
    b menu_event_done
menu_rearm:
    bl menu_idle
    cmp r0,#0
    beq menu_rearm_keep
    bl menu_cancel_fire_clicks
    ldr r0,[r7,#D_SCANNER]
    ldrb r0,[r0,#0x11]
    cmp r0,#0
    bne menu_rearm_keep
    movs r0,#0
    strb r0,[r6,#ST_PRESS]
    strb r0,[r6,#ST_SEQUENCE]
    str r0,[r6,#28]
    // This is the native last-rendered-page cache: Main sets it back to 1.
    // Invalidate it so static frames/labels are redrawn as well as digits.
    ldr r1,[r7,#D_PAGE]
    strb r0,[r1]
    bl menu_clear
    ldr r3,[r7,#D_MAIN]
    blx r3
    movs r0,#0
    strb r0,[r6,#ST_ACTIVE]
menu_rearm_keep:
    cmp r4,#9
    bhi menu_event_keep
    b menu_consume

// Cancel only the released Fire scanner's delayed multi-click state, with its
// producer IRQ excluded. Compact only queued Fire-derived events 1..9; keep
// every Plus/Minus/combination event in its existing order. No shared flush.
menu_cancel_fire_clicks:
    push {r0-r5,lr}
    sub sp,#4
    mrs r5,primask
    cpsid i
    ldr r0,[r7,#D_GPIOB]
    ldr r0,[r0,#16]
    tst r0,#0x800
    beq 9f
    ldr r0,[r7,#D_SCANNER]
    ldrb r1,[r0,#0x11]
    cbnz r1,9f
    movs r1,#0
    strb r1,[r0,#0x14]
    strb r1,[r0,#0x15]
    ldr r0,[r7,#D_QUEUE_BASE]
    ldrb r1,[r0,#5]
    ldrb r2,[r0,#6]
    mov r3,r1
1:  cmp r1,r2
    beq 4f
    ldrb r4,[r0,r1]
    cmp r4,#1
    blo 2f
    cmp r4,#9
    bls 3f
2:  strb r4,[r0,r3]
    adds r3,#1
    cmp r3,#5
    it hs
    movhs r3,#0
3:  adds r1,#1
    cmp r1,#5
    it hs
    movhs r1,#0
    b 1b
4:  strb r3,[r0,#6]
    dmb sy
9:  msr primask,r5
    add sp,#4
    pop {r0-r5,pc}

#if MENU_INFO
// Refresh only through the real native voltage/statistics renderer, on a
// INFO follows changing native values without resetting native inactivity.
menu_live_info:
    push {r4,r5,r6,lr}
    ldrb r0,[r6,#ST_ACTIVE]
    cmp r0,#2
    bne 9f
    movs r5,#0
#if MENU_BATTERY
    ldr r0,[r7,#D_BATTERY]
    ldrh r0,[r0]
    ldr r1,[r6,#32]
    cmp r0,r1
    it ne
    movne r5,#1
    str r0,[r6,#32]
#endif
#if MENU_RESISTANCE
    ldr r0,[r7,#D_COIL]
    bl menu_read
    ldr r1,[r6,#36]
    cmp r0,r1
    it ne
    movne r5,#1
    str r0,[r6,#36]
#endif
#if MENU_DURATION
    ldr r0,[r7,#D_DURATION]
    ldrh r0,[r0]
    ldr r1,[r6,#40]
    cmp r0,r1
    it ne
    movne r5,#1
    str r0,[r6,#40]
#endif
    cmp r5,#0
    it ne
    strbne r5,[r6,#ST_DIRTY]
9:  pop {r4,r5,r6,pc}
#endif

#if MENU_VOLTAGE
// changed native ADC battery value and a released, idle Main. No host RAM writes.
menu_live_voltage:
    push {r4,r5,lr}
    sub sp,#4
    ldrb r0,[r6,#ST_ACTIVE]
    cbnz r0,9f
    ldrb r0,[r6,#ST_BATTERY]
    cbz r0,9f
    bl menu_idle
    cbz r0,9f
    ldr r0,[r7,#D_BATTERY]
    ldrh r4,[r0]
    ldr r0,[r6,#28]
    cmp r0,r4
    beq 9f
    str r4,[r6,#28]
    movs r0,#10
    udiv r4,r4,r0
    ldr r0,[r7,#D_COMBINED]
    cbnz r0,1f
    mov r0,r4
    movs r1,#1
    b 2f
1:  ldr r0,[r7,#D_COIL]
    bl menu_read
    mov r5,r0
    ldr r0,[r7,#D_MODE]
    bl menu_read
    mov r1,r5
    mov r2,r4
    movs r3,#1
2:  ldr r12,[r7,#D_VOLTAGE]
    blx r12
9:  add sp,#4
    pop {r4,r5,pc}
#endif
menu_fill:
    // fifth AAPCS argument; all calls originate on an 8-byte-aligned stack.
    push {r4,lr}
    sub sp,#8
    str r4,[sp]
    ldr r4,[r7,#D_FILL]
    blx r4
    add sp,#8
    pop {r4,pc}

// Native Main renderers can leave unchanged regions alone. Remove the menu
// surface before returning, instead of painting partial Main widgets over it.
menu_clear:
    push {r4,lr}
    movs r0,#0
    movs r1,#0
    movs r2,#80
    movs r3,#160
    movs r4,#0
    bl menu_fill
    pop {r4,pc}

menu_draw:
    push {r4,r5,lr}
    sub sp,#4
    bl menu_clear
    movs r0,#16
    movs r1,#10
    adr.w r2,text_settings
#if MENU_RANK_SELECTOR
    ldrb r3,[r6,#ST_ACTIVE]
    cmp r3,#6
    it eq
    adreq.w r2,text_set_rank
#endif
    bl menu_text
#if MENU_EDITORS
    ldrb r0,[r6,#ST_ACTIVE]
#if MENU_RANK_SELECTOR
    cmp r0,#6
    beq menu_draw_ranks
#endif
#if MENU_NUMERIC
    cmp r0,#4
    bhs menu_draw_puffs
#endif
#endif
#if MENU_INFO
    ldrb r0,[r6,#ST_ACTIVE]
    cmp r0,#2
    beq menu_draw_info
#endif
    movs r4,#0
menu_draw_item:
    add r0,r7,#D_ITEMS
    ldrb r0,[r0,r4]
#if MENU_INFO
    cmp r0,#0
    beq menu_draw_info_label
#endif
#if MENU_VOLTAGE
    cmp r0,#1
    beq menu_draw_voltage_label
#endif
#if MENU_PUFF_EDITOR
    cmp r0,#3
    beq menu_draw_puff_label
#endif
#if MENU_SCORE_EDITOR
    cmp r0,#4
    beq menu_draw_score_label
#endif
#if MENU_RANK_SELECTOR
    cmp r0,#5
    beq menu_draw_rank_label
#endif
    adr.w r2,text_exit
    b menu_draw_label
#if MENU_INFO
menu_draw_info_label:
    adr.w r2,text_info
    b menu_draw_label
#endif
#if MENU_PUFF_EDITOR
menu_draw_puff_label:
    adr.w r2,text_puff_count
    b menu_draw_label
#endif
#if MENU_SCORE_EDITOR
menu_draw_score_label:
    adr.w r2,text_points
    b menu_draw_label
#endif
#if MENU_RANK_SELECTOR
menu_draw_rank_label:
    adr.w r2,text_set_rank
    b menu_draw_label
#endif
#if MENU_VOLTAGE
menu_draw_voltage_label:
    ldrb r0,[r6,#ST_BATTERY]
    cmp r0,#0
    ite eq
    adreq.w r2,text_output
    adrne.w r2,text_battery
#endif
menu_draw_label:
    movs r1,#20
    muls r1,r4,r1
    adds r1,#35
    movs r0,#7
    bl menu_text
    adds r4,#1
    ldr r0,[r7,#D_ITEM_COUNT]
    cmp r4,r0
    blo menu_draw_item
    ldrb r0,[r6,#ST_SELECT]
    movs r1,#20
    muls r1,r0,r1

    adds r1,#35
    movs r0,#0
    adr.w r2,text_arrow
    bl menu_text
    b menu_draw_done
#if MENU_INFO
menu_draw_info:
#if MENU_BATTERY
    movs r0,#4
    movs r1,#35
    adr.w r2,text_bat
    bl menu_text
    ldr r0,[r7,#D_BATTERY]
    ldrh r0,[r0]
    movs r1,#35
    bl menu_number
#endif
#if MENU_RESISTANCE
    movs r0,#4
    movs r1,#35+20*MENU_BATTERY
    adr.w r2,text_coil
    bl menu_text
    ldr r0,[r7,#D_COIL]
    bl menu_read
    movs r1,#35+20*MENU_BATTERY
    bl menu_number
#endif
#if MENU_DURATION
    movs r0,#4
    movs r1,#35+20*MENU_BATTERY+20*MENU_RESISTANCE
    adr.w r2,text_puff
    bl menu_text
    ldr r0,[r7,#D_DURATION]
    ldrh r0,[r0]
    movs r1,#35+20*MENU_BATTERY+20*MENU_RESISTANCE
    bl menu_number
#endif
#endif
    b menu_draw_done
#if MENU_RANK_SELECTOR
menu_draw_ranks:
    movs r4,#0
    adr.w r5,text_rank_iron
1:  movs r0,#7
    movs r1,#14
    muls r1,r4,r1
    adds r1,#30
    mov r2,r5
    bl menu_text
2:  ldrb r0,[r5],#1
    cmp r0,#0
    bne 2b
    adds r4,#1
    cmp r4,#9
    blo 1b
    ldr r0,[r6,#ST_PUFF_DIGIT]
    movs r1,#14
    muls r1,r0,r1
    adds r1,#30
    movs r0,#0
    adr.w r2,text_arrow
    bl menu_text
    ldrb r0,[r6,#ST_PUFF_ERROR]
    cmp r0,#1
    bne menu_draw_done
    movs r0,#4
    movs r1,#150
    adr.w r2,text_save_error
    bl menu_text
    b menu_draw_done
#endif
#if MENU_NUMERIC
menu_draw_puffs:
#if MENU_PUFF_EDITOR
    movs r0,#10
    adr.w r2,text_puff_count
#endif
#if MENU_SCORE_EDITOR
    ldrb r1,[r6,#ST_ACTIVE]
    cmp r1,#5
    itt eq
    moveq r0,#22
    adreq.w r2,text_points
#endif
    movs r1,#35
    bl menu_text
    ldrb r0,[r6,#ST_PUFF_ERROR]
    cmp r0,#2
    beq menu_draw_puff_range_error
    movs r4,#0
1:  adr.w r0,menu_puff_places
    ldr r1,[r0,r4,lsl #2]
    ldr r0,[r6,#ST_PUFF_VALUE]
    udiv r0,r0,r1
    movs r1,#10
    udiv r2,r0,r1
    mls r0,r2,r1,r0
    mov r5,r4
    bl menu_puff_digit_draw
    adds r4,#1
    cmp r4,#5
    blo 1b
    ldrb r0,[r6,#ST_PUFF_DIGIT]
    movs r1,#12
    muls r0,r1,r0
    adds r0,#11
    movs r1,#72
    movs r2,#10
    movs r3,#2
    movw r4,#0xffff
    bl menu_fill
    movs r0,#4
    movs r1,#90
    adr.w r2,text_limit
    bl menu_text
    bl menu_editor_limit
    mov r0,r12
    movs r1,#90
    bl menu_number
    ldrb r0,[r6,#ST_PUFF_ERROR]
    cmp r0,#1
    bne menu_draw_done
    movs r0,#4
    movs r1,#110
    adr.w r2,text_save_error
    bl menu_text
    b menu_draw_done
menu_draw_puff_range_error:
    movs r0,#4
    movs r1,#55
    adr.w r2,text_range_error
    bl menu_text
#endif
menu_draw_done:
    add sp,#4
    pop {r4,r5,pc}

#if MENU_DIGITS
// Decimal conversion uses native RAM values unchanged, not simulated UI data.
menu_number:
    push {r4,r5,lr}
    sub sp,#20
    mov r4,r1
    add r2,sp,#15
    movs r1,#0
    strb r1,[r2]
    movs r3,#10
1:  udiv r1,r0,r3
    mls r5,r1,r3,r0
    adds r5,#48
    strb r5,[r2,#-1]!
    mov r0,r1
    cmp r0,#0
    bne 1b
    movs r0,#34
    mov r1,r4
    bl menu_text
    add sp,#20
    pop {r4,r5,pc}
#endif

// Native rectangular fill sends every glyph pixel through the original SPI/LCD
// driver, retaining shifted/vendor panel handling. No IRQ or original SPI code
// is modified. Maximum menu-owned drawing call frame: 48+16+16+32=112 bytes,
// excluding the original native fill's independently bounded frame.
menu_text:
    push {r4-r11,lr}
    sub sp,#12
    mov r8,r0
    mov r9,r1
    mov r10,r2
1:  ldrb r0,[r10],#1
    cbz r0,9f
#if MENU_DIGITS
    cmp r0,#48
    blo 3f
    cmp r0,#57
    bls 2f
#endif
    cmp r0,#65
    blo 3f
    cmp r0,#90
    bhi 3f
    subs r0,#(65-10*MENU_DIGITS)
    b 4f
#if MENU_DIGITS
2:  subs r0,#48
    b 4f
#endif
3:  cmp r0,#62
    ite eq
    moveq r0,#(26+10*MENU_DIGITS)
    movne r0,#(27+10*MENU_DIGITS)
4:  adr.w r5,menu_font
    movs r1,#5
    muls r0,r1,r0
    adds r5,r0
    movs r11,#0
5:  ldrb r6,[r5],#1
    movs r4,#0
6:  tst r6,#1
    beq 7f
    add r0,r8,r11
    cmp r0,#80
    bhs 7f
    add r1,r9,r4
    movs r2,#1
    movs r3,#1
    movw r12,#0xffff
    str r12,[sp]
    ldr r12,[r7,#D_FILL]
    blx r12
7:  lsrs r6,#1
    adds r4,#1
    cmp r4,#7
    blo 6b
    adds r11,#1
    cmp r11,#5
    blo 5b
    adds r8,#6
    b 1b
9:  ldr r6,[r7]
    add sp,#12
    pop {r4-r11,pc}
#if MENU_NUMERIC
// r0 digit, r5 index. Exactly the host 5x7 glyph scaled 2x at (11+12*i,55).
menu_puff_digit_draw:
    push {r4-r11,lr}
    sub sp,#12
    adr.w r8,menu_font
    movs r1,#5
    mla r8,r0,r1,r8
    movs r0,#12
    mul r9,r5,r0
    add r9,#11
    movs r10,#0
1:  ldrb r11,[r8],#1
    movs r5,#0
2:  tst r11,#1
    beq 3f
    add r0,r9,r10,lsl #1
    lsls r1,r5,#1
    adds r1,#55
    movs r2,#2
    movs r3,#2
    movw r4,#0xffff
    bl menu_fill
3:  lsr r11,#1
    adds r5,#1
    cmp r5,#7
    blo 2b
    add r10,#1
    cmp r10,#5
    blo 1b
    add sp,#12
    pop {r4-r11,pc}
#endif

.balign 4
#define MENU_TEXT(name,value) text_##name: .asciz value
#include "firmware_menu_visuals.inc"
#undef MENU_TEXT
.balign 4
menu_font:
#define MENU_GLYPH(...) .byte __VA_ARGS__
#include "firmware_menu_visuals.inc"
#undef MENU_GLYPH
.balign 4
firmware_menu_descriptor:
.Lmenu_descriptor:
