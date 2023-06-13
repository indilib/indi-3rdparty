/* Copyright (C) 2008, 2009, 2010 Analog Devices, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef URJ_BFIN_H
#define URJ_BFIN_H

#include <stdint.h>

#include "types.h"
#include "tap.h"
#include "part.h"


/* High-Nibble: group code, low nibble: register code.  */
#define T_BFIN_REG_R                         0x00
#define T_BFIN_REG_P                         0x10
#define T_BFIN_REG_I                         0x20
#define T_BFIN_REG_B                         0x30
#define T_BFIN_REG_L                         0x34
#define T_BFIN_REG_M                         0x24
#define T_BFIN_REG_A                         0x40

enum core_regnum
{
    BFIN_REG_R0 = T_BFIN_REG_R, BFIN_REG_R1, BFIN_REG_R2, BFIN_REG_R3, BFIN_REG_R4, BFIN_REG_R5, BFIN_REG_R6, BFIN_REG_R7,
    BFIN_REG_P0 = T_BFIN_REG_P, BFIN_REG_P1, BFIN_REG_P2, BFIN_REG_P3, BFIN_REG_P4, BFIN_REG_P5, BFIN_REG_SP, BFIN_REG_FP,
    BFIN_REG_I0 = T_BFIN_REG_I, BFIN_REG_I1, BFIN_REG_I2, BFIN_REG_I3,
    BFIN_REG_M0 = T_BFIN_REG_M, BFIN_REG_M1, BFIN_REG_M2, BFIN_REG_M3,
    BFIN_REG_B0 = T_BFIN_REG_B, BFIN_REG_B1, BFIN_REG_B2, BFIN_REG_B3,
    BFIN_REG_L0 = T_BFIN_REG_L, BFIN_REG_L1, BFIN_REG_L2, BFIN_REG_L3,
    BFIN_REG_A0x = T_BFIN_REG_A, BFIN_REG_A0w, BFIN_REG_A1x, BFIN_REG_A1w,
    BFIN_REG_ASTAT = 0x46,
    BFIN_REG_RETS = 0x47,
    BFIN_REG_LC0 = 0x60, BFIN_REG_LT0, BFIN_REG_LB0, BFIN_REG_LC1, BFIN_REG_LT1, BFIN_REG_LB1,
    BFIN_REG_CYCLES, BFIN_REG_CYCLES2,
    BFIN_REG_USP = 0x70, BFIN_REG_SEQSTAT, BFIN_REG_SYSCFG,
    BFIN_REG_RETI, BFIN_REG_RETX, BFIN_REG_RETN, BFIN_REG_RETE, BFIN_REG_EMUDAT,
};

#define CLASS_MASK                      0xf0
#define GROUP(x)                        (((x) & CLASS_MASK) >> 4)
#define DREG_P(x)                       (((x) & CLASS_MASK) == T_BFIN_REG_R)
#define PREG_P(x)                       (((x) & CLASS_MASK) == T_BFIN_REG_P)


#define DTEST_COMMAND                   0xffe00300
#define DTEST_DATA0                     0xffe00400
#define DTEST_DATA1                     0xffe00404

#define ITEST_COMMAND                   0xffe01300
#define ITEST_DATA0                     0xffe01400
#define ITEST_DATA1                     0xffe01404

struct bfin_part_data
{
    int bypass;
    int scan;

    uint16_t dbgctl;
    uint16_t dbgstat;

    uint16_t dbgctl_sram_init;
    uint16_t dbgctl_wakeup;
    uint16_t dbgctl_sysrst;
    uint16_t dbgctl_esstep;
    uint16_t dbgctl_emudatsz_32;
    uint16_t dbgctl_emudatsz_40;
    uint16_t dbgctl_emudatsz_48;
    uint16_t dbgctl_emudatsz_mask;
    uint16_t dbgctl_emuirlpsz_2;
    uint16_t dbgctl_emuirsz_64;
    uint16_t dbgctl_emuirsz_48;
    uint16_t dbgctl_emuirsz_32;
    uint16_t dbgctl_emuirsz_mask;
    uint16_t dbgctl_empen;
    uint16_t dbgctl_emeen;
    uint16_t dbgctl_emfen;
    uint16_t dbgctl_empwr;

    uint16_t dbgstat_lpdec1;
    uint16_t dbgstat_in_powrgate;
    uint16_t dbgstat_core_fault;
    uint16_t dbgstat_idle;
    uint16_t dbgstat_in_reset;
    uint16_t dbgstat_lpdec0;
    uint16_t dbgstat_bist_done;
    uint16_t dbgstat_emucause_mask;
    uint16_t dbgstat_emuack;
    uint16_t dbgstat_emuready;
    uint16_t dbgstat_emudiovf;
    uint16_t dbgstat_emudoovf;
    uint16_t dbgstat_emudif;
    uint16_t dbgstat_emudof;

    uint64_t emuir_a;
    uint64_t emuir_b;

    uint64_t emudat_out;
    uint64_t emudat_in;

    uint32_t emupc;
    uint32_t emupc_orig;
};

#define BFIN_PART_DATA(part)       ((struct bfin_part_data *)((part)->params->data))
#define BFIN_PART_BYPASS(part)     (BFIN_PART_DATA (part)->bypass)

#define BFIN_PART_SCAN(part)       (BFIN_PART_DATA (part)->scan)
#define BFIN_PART_WPSTAT(part)     (BFIN_PART_DATA (part)->wpstat)
#define BFIN_PART_DBGCTL(part)     (BFIN_PART_DATA (part)->dbgctl)
#define BFIN_PART_DBGSTAT(part)    (BFIN_PART_DATA (part)->dbgstat)
#define BFIN_PART_EMUIR_A(part)    (BFIN_PART_DATA (part)->emuir_a)
#define BFIN_PART_EMUIR_B(part)    (BFIN_PART_DATA (part)->emuir_b)
#define BFIN_PART_EMUDAT_OUT(part) (BFIN_PART_DATA (part)->emudat_out)
#define BFIN_PART_EMUDAT_IN(part)  (BFIN_PART_DATA (part)->emudat_in)
#define BFIN_PART_EMUPC(part)      (BFIN_PART_DATA (part)->emupc)
#define BFIN_PART_EMUPC_ORIG(part) (BFIN_PART_DATA (part)->emupc_orig)

#define IDCODE_SCAN                     0
#define DBGSTAT_SCAN                    1
#define DBGCTL_SCAN                     2
#define EMUIR_SCAN                      3
#define EMUDAT_SCAN                     4
#define EMUPC_SCAN                      5
#define BYPASS                          6
#define EMUIR64_SCAN                    7
#define NUM_SCANS                       8

extern const char * const scans[];

#define INSN_NOP                        0x0000
#define INSN_RTE                        0x0014
#define INSN_CSYNC                      0x0023
#define INSN_SSYNC                      0x0024
#define INSN_ILLEGAL                    0xffffffff

#define INSN_BIT_MULTI                  0x08
#define INSN_IS_MULTI(insn)                                     \
    (((insn) & 0xc0) == 0xc0 && ((insn) & INSN_BIT_MULTI)       \
     && ((insn) & 0xe8) != 0xe8 /* not linkage */)

enum bfin_insn_type
{
    /* Instruction is a normal instruction.  */
    BFIN_INSN_NORMAL,

    /* Instruction is a value which should be set to EMUDAT.  */
    BFIN_INSN_SET_EMUDAT
};

struct bfin_insn
{
    /* The instruction or the value to be set to EMUDAT.  */
    uint64_t i;

    /* The type of this instruction.  */
    enum bfin_insn_type type;

    /* Instructions to be executed are kept on a linked list.
       This is the link.  */
    struct bfin_insn *next;
};

enum {
    LEAVE_NOP_DEFAULT,
    LEAVE_NOP_YES,
    LEAVE_NOP_NO
};

extern int bfin_check_emuready;
extern int bfin_wait_clocks;

/* From src/bfin/bfin.c */

int part_is_bfin (urj_chain_t *, int);
int part_scan_select (urj_chain_t *, int, int);

#define DECLARE_PART_DBGCTL_SET_BIT(name)                               \
    void part_dbgctl_bit_set_##name (urj_chain_t *chain, int n);

#define DECLARE_PART_DBGCTL_CLEAR_BIT(name)                             \
    void part_dbgctl_bit_clear_##name (urj_chain_t *chain, int n);

#define DECLARE_PART_DBGCTL_IS(name)                            \
    int part_dbgctl_is_##name (urj_chain_t *chain, int n);

#define DECLARE_DBGCTL_BIT_OP(name)             \
    DECLARE_PART_DBGCTL_SET_BIT(name)           \
    DECLARE_PART_DBGCTL_CLEAR_BIT(name)         \
    DECLARE_PART_DBGCTL_IS(name)

/* These functions check cached DBGSTAT. So before calling them,
   chain_dbgstat_get or part_dbgstat_get has to be called to update cached
   DBGSTAT value.  */

#define DECLARE_PART_DBGSTAT_BIT_IS(name)                       \
    int part_dbgstat_is_##name (urj_chain_t *chain, int n);

DECLARE_DBGCTL_BIT_OP (sram_init)
DECLARE_DBGCTL_BIT_OP (wakeup)
DECLARE_DBGCTL_BIT_OP (sysrst)
DECLARE_DBGCTL_BIT_OP (esstep)
DECLARE_DBGCTL_BIT_OP (emudatsz_32)
DECLARE_DBGCTL_BIT_OP (emudatsz_40)
DECLARE_DBGCTL_BIT_OP (emudatsz_48)
DECLARE_DBGCTL_BIT_OP (emuirlpsz_2)
DECLARE_DBGCTL_BIT_OP (emuirsz_64)
DECLARE_DBGCTL_BIT_OP (emuirsz_48)
DECLARE_DBGCTL_BIT_OP (emuirsz_32)
DECLARE_DBGCTL_BIT_OP (empen)
DECLARE_DBGCTL_BIT_OP (emeen)
DECLARE_DBGCTL_BIT_OP (emfen)
DECLARE_DBGCTL_BIT_OP (empwr)

DECLARE_PART_DBGSTAT_BIT_IS (lpdec1)
DECLARE_PART_DBGSTAT_BIT_IS (in_powrgate)
DECLARE_PART_DBGSTAT_BIT_IS (core_fault)
DECLARE_PART_DBGSTAT_BIT_IS (idle)
DECLARE_PART_DBGSTAT_BIT_IS (in_reset)
DECLARE_PART_DBGSTAT_BIT_IS (lpdec0)
DECLARE_PART_DBGSTAT_BIT_IS (bist_done)
DECLARE_PART_DBGSTAT_BIT_IS (emuack)
DECLARE_PART_DBGSTAT_BIT_IS (emuready)
DECLARE_PART_DBGSTAT_BIT_IS (emudiovf)
DECLARE_PART_DBGSTAT_BIT_IS (emudoovf)
DECLARE_PART_DBGSTAT_BIT_IS (emudif)
DECLARE_PART_DBGSTAT_BIT_IS (emudof)

uint16_t part_dbgstat_emucause (urj_chain_t *, int);
void part_dbgstat_get (urj_chain_t *, int);
uint32_t part_emupc_get (urj_chain_t *, int, int);
void part_dbgstat_clear_ovfs (urj_chain_t *, int);
void part_wait_in_reset (urj_chain_t *, int);
void part_wait_reset (urj_chain_t *, int);
void part_check_emuready (urj_chain_t *, int);
void part_emudat_set (urj_chain_t *, int, uint32_t, int);
uint32_t part_emudat_get (urj_chain_t *, int, int);
void part_emudat_defer_get (urj_chain_t *, int, int);
uint32_t part_emudat_get_done (urj_chain_t *, int, int);
uint64_t emudat_value (urj_tap_register_t *);
void emudat_init_value (urj_tap_register_t *, uint32_t);
uint32_t part_register_get (urj_chain_t *, int, enum core_regnum);
void part_register_set (urj_chain_t *, int, enum core_regnum, uint32_t);
void part_emuir_set (urj_chain_t *, int, uint64_t, int);
void part_emuir_set_2 (urj_chain_t *, int, uint64_t, uint64_t, int);
uint32_t part_get_r0 (urj_chain_t *, int);
uint32_t part_get_p0 (urj_chain_t *, int);
void part_set_r0 (urj_chain_t *, int, uint32_t);
void part_set_p0 (urj_chain_t *, int, uint32_t);
void part_emulation_enable (urj_chain_t *, int);
void part_emulation_disable (urj_chain_t *, int);
void part_emulation_trigger (urj_chain_t *, int);
void part_emulation_return (urj_chain_t *, int);
void part_execute_instructions (urj_chain_t *, int n, struct bfin_insn *);
void chain_system_reset (urj_chain_t *);
void bfin_core_reset (urj_chain_t *, int);
void software_reset (urj_chain_t *, int);
void part_emupc_reset (urj_chain_t *, int, uint32_t);
uint32_t part_mmr_read_clobber_r0 (urj_chain_t *, int, int32_t, int);
void part_mmr_write_clobber_r0 (urj_chain_t *, int, int32_t, uint32_t, int);
uint32_t part_mmr_read (urj_chain_t *, int, uint32_t, int);
void part_mmr_write (urj_chain_t *, int, uint32_t, uint32_t, int);

/* From src/bfin/insn-gen.c */

uint32_t gen_move (enum core_regnum dest, enum core_regnum src);
uint32_t gen_load32_offset (enum core_regnum dest, enum core_regnum base, int32_t offset);
uint32_t gen_store32_offset (enum core_regnum base, int32_t offset, enum core_regnum src);
uint32_t gen_load16z_offset (enum core_regnum dest, enum core_regnum base, int32_t offset);
uint32_t gen_store16_offset (enum core_regnum base, int32_t offset, enum core_regnum src);
uint32_t gen_load8z_offset (enum core_regnum dest, enum core_regnum base, int32_t offset);
uint32_t gen_store8_offset (enum core_regnum base, int32_t offset, enum core_regnum src);
uint32_t gen_load32pi (enum core_regnum dest, enum core_regnum base);
uint32_t gen_store32pi (enum core_regnum base, enum core_regnum src);
uint32_t gen_load16zpi (enum core_regnum dest, enum core_regnum base);
uint32_t gen_store16pi (enum core_regnum base, enum core_regnum src);
uint32_t gen_load8zpi (enum core_regnum dest, enum core_regnum base);
uint32_t gen_store8pi (enum core_regnum base, enum core_regnum src);
uint32_t gen_load32 (enum core_regnum dest, enum core_regnum base);
uint32_t gen_store32 (enum core_regnum base, enum core_regnum src);
uint32_t gen_load16z (enum core_regnum dest, enum core_regnum base);
uint32_t gen_store16 (enum core_regnum base, enum core_regnum src);
uint32_t gen_load8z (enum core_regnum dest, enum core_regnum base);
uint32_t gen_store8 (enum core_regnum base, enum core_regnum src);
uint32_t gen_iflush (enum core_regnum addr);
uint32_t gen_iflush_pm (enum core_regnum addr);
uint32_t gen_flush (enum core_regnum addr);
uint32_t gen_flush_pm (enum core_regnum addr);
uint32_t gen_flushinv (enum core_regnum addr);
uint32_t gen_flushinv_pm (enum core_regnum addr);
uint32_t gen_prefetch (enum core_regnum addr);
uint32_t gen_prefetch_pm (enum core_regnum addr);
uint32_t gen_jump_reg (enum core_regnum addr);

#endif /* URJ_BFIN_H */
