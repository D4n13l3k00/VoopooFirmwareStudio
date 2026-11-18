    /*
     * Thumb hook integration for Unicorn 2.1.4; ARM translation remains native.
     * Original translator: Fabrice Bellard, CodeSourcery, OpenedHand, Ltd.
     * SPDX-License-Identifier: LGPL-2.0-or-later (see translate.c's full notice).
     * Expose the pre-instruction IT state before an unconditional code hook.
     * A stopped hook must preserve that state for exception stacking/resume;
     * otherwise restore QEMU's zero-while-running invariant before execution.
     */
    uc->no_exit_request = false;
    insn_size = is_16bit ? 2 : 4;
    if (HOOK_EXISTS_BOUNDED(uc, UC_HOOK_CODE, dc->base.pc_next - insn_size)) {
        gen_set_condexec(dc);
        gen_set_pc_im(dc, dc->base.pc_next - insn_size);
        gen_uc_tracecode(tcg_ctx, insn_size, UC_HOOK_CODE_IDX, uc, dc->base.pc_next - insn_size);
        check_exit_request(tcg_ctx);
        if (dc->condexec_mask) {
            TCGv_i32 tmp = tcg_temp_new_i32(tcg_ctx);
            tcg_gen_movi_i32(tcg_ctx, tmp, 0);
            store_cpu_field(tcg_ctx, tmp, condexec_bits);
        }
    }

    if (dc->condexec_mask && !thumb_insn_is_unconditional(dc, insn)) {
        uint32_t cond = dc->condexec_cond;

        /* Both 0xe and 0xf mean "always" here, not "never". */
        if (cond < 0x0e) {
            arm_skip_unless(dc, cond);
        }
    }
