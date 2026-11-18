    if (dc->condexec_mask && !thumb_insn_is_unconditional(dc, insn)) {
        uint32_t cond = dc->condexec_cond;

        /*
         * Conditionally skip the insn. Note that both 0xe and 0xf mean
         * "always"; 0xf is not "never".
         */
        if (cond < 0x0e) {
            arm_skip_unless(dc, cond);
        }
    }
    
    // Unicorn:
    //   We can't stop in the middle of the IT block.
    //   In other words, treat the whole IT block as
    //   a single instruction.
    uc->no_exit_request = (dc->condexec_mask != 0);

    // Unicorn: trace this instruction on request
    insn_size = is_16bit ? 2 : 4;
    if (HOOK_EXISTS_BOUNDED(uc, UC_HOOK_CODE, dc->base.pc_next - insn_size)) {
        
        // Sync PC in advance
        gen_set_pc_im(dc, dc->base.pc_next - insn_size);

        if (uc->no_exit_request) {
            gen_uc_tracecode(tcg_ctx, insn_size, UC_HOOK_CODE_IDX | UC_HOOK_FLAG_NO_STOP, uc, dc->base.pc_next - insn_size);
        } else {
            gen_uc_tracecode(tcg_ctx, insn_size, UC_HOOK_CODE_IDX, uc, dc->base.pc_next - insn_size);
        }
        // the callback might want to stop emulation immediately
        check_exit_request(tcg_ctx);
    }
