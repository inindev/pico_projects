//
//  W65C02S processor implementation in C++
//
//  Copyright 2018-2026, John Clark
//
//  Released under the GNU General Public License
//  https://www.gnu.org/licenses/gpl.html
//
//  ref: http://www.wdesignc.com/wdc/documentation/w65c02s.pdf
//       http://www.6502.org/tutorials/vflag.html
//

#pragma once
#define W65C02S_HPP_INCLUDED

#include <cstdint>
#include <array>

class W65C02S;  // forward declaration

// ============================================================================
//  W65C02S Instruction Set Definition
// ============================================================================
//
#define __ (-1)
#define W65C02S_ISA(X) \
    /*       abs ,absxi, absx, absy, absi, acum, imm , imp , rel ,zprel, stck,  zp , zpxi, zpx , zpy , zpi , zpiy, handler  */ \
    X(adc,   0x6d,  __ , 0x7d, 0x79,  __ ,  __ , 0x69,  __ ,  __ ,  __ ,  __ , 0x65, 0x61, 0x75,  __ , 0x72, 0x71, op_adc)  \
    X(and,   0x2d,  __ , 0x3d, 0x39,  __ ,  __ , 0x29,  __ ,  __ ,  __ ,  __ , 0x25, 0x21, 0x35,  __ , 0x32, 0x31, op_and)  \
    X(asl,   0x0e,  __ , 0x1e,  __ ,  __ , 0x0a,  __ ,  __ ,  __ ,  __ ,  __ , 0x06,  __ , 0x16,  __ ,  __ ,  __ , op_asl)  \
    X(bbr0,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x0f,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_bbr)  \
    X(bbr1,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x1f,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_bbr)  \
    X(bbr2,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x2f,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_bbr)  \
    X(bbr3,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x3f,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_bbr)  \
    X(bbr4,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x4f,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_bbr)  \
    X(bbr5,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x5f,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_bbr)  \
    X(bbr6,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x6f,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_bbr)  \
    X(bbr7,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x7f,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_bbr)  \
    X(bbs0,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x8f,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_bbs)  \
    X(bbs1,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x9f,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_bbs)  \
    X(bbs2,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xaf,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_bbs)  \
    X(bbs3,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xbf,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_bbs)  \
    X(bbs4,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xcf,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_bbs)  \
    X(bbs5,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xdf,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_bbs)  \
    X(bbs6,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xef,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_bbs)  \
    X(bbs7,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xff,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_bbs)  \
    X(bcc,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x90,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_bcc)  \
    X(bcs,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xb0,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_bcs)  \
    X(beq,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xf0,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_beq)  \
    X(bit,   0x2c,  __ , 0x3c,  __ ,  __ ,  __ , 0x89,  __ ,  __ ,  __ ,  __ , 0x24,  __ , 0x34,  __ ,  __ ,  __ , op_bit)  \
    X(bmi,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x30,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_bmi)  \
    X(bne,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xd0,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_bne)  \
    X(bpl,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x10,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_bpl)  \
    X(bra,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x80,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_bra)  \
    X(brk,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x00,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_brk)  \
    X(bvc,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x50,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_bvc)  \
    X(bvs,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x70,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_bvs)  \
    X(clc,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x18,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_clc)  \
    X(cld,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xd8,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_cld)  \
    X(cli,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x58,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_cli)  \
    X(clv,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xb8,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_clv)  \
    X(cmp,   0xcd,  __ , 0xdd, 0xd9,  __ ,  __ , 0xc9,  __ ,  __ ,  __ ,  __ , 0xc5, 0xc1, 0xd5,  __ , 0xd2, 0xd1, op_cmp)  \
    X(cpx,   0xec,  __ ,  __ ,  __ ,  __ ,  __ , 0xe0,  __ ,  __ ,  __ ,  __ , 0xe4,  __ ,  __ ,  __ ,  __ ,  __ , op_cpx)  \
    X(cpy,   0xcc,  __ ,  __ ,  __ ,  __ ,  __ , 0xc0,  __ ,  __ ,  __ ,  __ , 0xc4,  __ ,  __ ,  __ ,  __ ,  __ , op_cpy)  \
    X(dec,   0xce,  __ , 0xde,  __ ,  __ , 0x3a,  __ ,  __ ,  __ ,  __ ,  __ , 0xc6,  __ , 0xd6,  __ ,  __ ,  __ , op_dec)  \
    X(dex,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xca,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_dex)  \
    X(dey,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x88,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_dey)  \
    X(eor,   0x4d,  __ , 0x5d, 0x59,  __ ,  __ , 0x49,  __ ,  __ ,  __ ,  __ , 0x45, 0x41, 0x55,  __ , 0x52, 0x51, op_eor)  \
    X(inc,   0xee,  __ , 0xfe,  __ ,  __ , 0x1a,  __ ,  __ ,  __ ,  __ ,  __ , 0xe6,  __ , 0xf6,  __ ,  __ ,  __ , op_inc)  \
    X(inx,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xe8,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_inx)  \
    X(iny,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xc8,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_iny)  \
    X(jmp,   0x4c, 0x7c,  __ ,  __ , 0x6c,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_jmp)  \
    X(jsr,   0x20,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_jsr)  \
    X(lda,   0xad,  __ , 0xbd, 0xb9,  __ ,  __ , 0xa9,  __ ,  __ ,  __ ,  __ , 0xa5, 0xa1, 0xb5,  __ , 0xb2, 0xb1, op_lda)  \
    X(ldx,   0xae,  __ ,  __ , 0xbe,  __ ,  __ , 0xa2,  __ ,  __ ,  __ ,  __ , 0xa6,  __ ,  __ , 0xb6,  __ ,  __ , op_ldx)  \
    X(ldy,   0xac,  __ , 0xbc,  __ ,  __ ,  __ , 0xa0,  __ ,  __ ,  __ ,  __ , 0xa4,  __ , 0xb4,  __ ,  __ ,  __ , op_ldy)  \
    X(lsr,   0x4e,  __ , 0x5e,  __ ,  __ , 0x4a,  __ ,  __ ,  __ ,  __ ,  __ , 0x46,  __ , 0x56,  __ ,  __ ,  __ , op_lsr)  \
    X(nop,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xea,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_nop)  \
    X(ora,   0x0d,  __ , 0x1d, 0x19,  __ ,  __ , 0x09,  __ ,  __ ,  __ ,  __ , 0x05, 0x01, 0x15,  __ , 0x12, 0x11, op_ora)  \
    X(pha,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x48,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_pha)  \
    X(php,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x08,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_php)  \
    X(phx,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xda,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_phx)  \
    X(phy,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x5a,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_phy)  \
    X(pla,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x68,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_pla)  \
    X(plp,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x28,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_plp)  \
    X(plx,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xfa,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_plx)  \
    X(ply,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x7a,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_ply)  \
    X(rmb0,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x07,  __ ,  __ ,  __ ,  __ ,  __ , op_rmb)  \
    X(rmb1,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x17,  __ ,  __ ,  __ ,  __ ,  __ , op_rmb)  \
    X(rmb2,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x27,  __ ,  __ ,  __ ,  __ ,  __ , op_rmb)  \
    X(rmb3,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x37,  __ ,  __ ,  __ ,  __ ,  __ , op_rmb)  \
    X(rmb4,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x47,  __ ,  __ ,  __ ,  __ ,  __ , op_rmb)  \
    X(rmb5,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x57,  __ ,  __ ,  __ ,  __ ,  __ , op_rmb)  \
    X(rmb6,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x67,  __ ,  __ ,  __ ,  __ ,  __ , op_rmb)  \
    X(rmb7,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x77,  __ ,  __ ,  __ ,  __ ,  __ , op_rmb)  \
    X(rol,   0x2e,  __ , 0x3e,  __ ,  __ , 0x2a,  __ ,  __ ,  __ ,  __ ,  __ , 0x26,  __ , 0x36,  __ ,  __ ,  __ , op_rol)  \
    X(ror,   0x6e,  __ , 0x7e,  __ ,  __ , 0x6a,  __ ,  __ ,  __ ,  __ ,  __ , 0x66,  __ , 0x76,  __ ,  __ ,  __ , op_ror)  \
    X(rti,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x40,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_rti)  \
    X(rts,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x60,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_rts)  \
    X(sbc,   0xed,  __ , 0xfd, 0xf9,  __ ,  __ , 0xe9,  __ ,  __ ,  __ ,  __ , 0xe5, 0xe1, 0xf5,  __ , 0xf2, 0xf1, op_sbc)  \
    X(sec,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x38,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_sec)  \
    X(sed,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xf8,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_sed)  \
    X(sei,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x78,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_sei)  \
    X(smb0,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x87,  __ ,  __ ,  __ ,  __ ,  __ , op_smb)  \
    X(smb1,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x97,  __ ,  __ ,  __ ,  __ ,  __ , op_smb)  \
    X(smb2,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xa7,  __ ,  __ ,  __ ,  __ ,  __ , op_smb)  \
    X(smb3,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xb7,  __ ,  __ ,  __ ,  __ ,  __ , op_smb)  \
    X(smb4,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xc7,  __ ,  __ ,  __ ,  __ ,  __ , op_smb)  \
    X(smb5,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xd7,  __ ,  __ ,  __ ,  __ ,  __ , op_smb)  \
    X(smb6,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xe7,  __ ,  __ ,  __ ,  __ ,  __ , op_smb)  \
    X(smb7,   __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xf7,  __ ,  __ ,  __ ,  __ ,  __ , op_smb)  \
    X(sta,   0x8d,  __ , 0x9d, 0x99,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x85, 0x81, 0x95,  __ , 0x92, 0x91, op_sta)  \
    X(stp,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xdb,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_stp)  \
    X(stx,   0x8e,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x86,  __ ,  __ , 0x96,  __ ,  __ , op_stx)  \
    X(sty,   0x8c,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x84,  __ , 0x94,  __ ,  __ ,  __ , op_sty)  \
    X(stz,   0x9c,  __ , 0x9e,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x64,  __ , 0x74,  __ ,  __ ,  __ , op_stz)  \
    X(tax,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xaa,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_tax)  \
    X(tay,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xa8,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_tay)  \
    X(trb,   0x1c,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x14,  __ ,  __ ,  __ ,  __ ,  __ , op_trb)  \
    X(tsb,   0x0c,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x04,  __ ,  __ ,  __ ,  __ ,  __ , op_tsb)  \
    X(tsx,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xba,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_tsx)  \
    X(txa,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x8a,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_txa)  \
    X(txs,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x9a,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_txs)  \
    X(tya,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0x98,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_tya)  \
    X(wai,    __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , 0xcb,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ ,  __ , op_wai)

// ============================================================================
//  AddressMode - represents a 65C02 addressing mode
// ============================================================================

class AddressMode {
public:
    using GetFn     = uint8_t  (*)(W65C02S&, AddressMode&);
    using WriteFn   = void     (*)(W65C02S&, AddressMode&, uint8_t);
    using ResolveFn = uint16_t (*)(W65C02S&, AddressMode&);

    const char* name{};
    GetFn       get{};          // fetch operand + read value
    WriteFn     write{};        // write to eff_addr
    ResolveFn   resolve{};      // fetch operand + return address (for jmp/branch/store)
    uint8_t     bytes{};        // instruction length
    uint8_t     cycles{};       // base cycle count
    uint8_t     write_extra{};  // additional cycles for write operations
    uint8_t     branch_extra{}; // additional cycles when branch taken

    uint16_t eff_addr{};
    uint8_t  page_penalty{};

    constexpr AddressMode() = default;
    constexpr AddressMode(const char* name, GetFn get, WriteFn write, ResolveFn resolve,
                          uint8_t bytes, uint8_t cycles, uint8_t write_extra = 0, uint8_t branch_extra = 0)
        : name(name), get(get), write(write), resolve(resolve),
          bytes(bytes), cycles(cycles), write_extra(write_extra), branch_extra(branch_extra) {}
};

// ============================================================================
//  Flags6502 - processor status register
// ============================================================================

class Flags6502 {
    union {
        uint8_t reg;
        struct {
            uint8_t c : 1;  // bit 0 - Carry
            uint8_t z : 1;  // bit 1 - Zero
            uint8_t i : 1;  // bit 2 - Interrupt disable
            uint8_t d : 1;  // bit 3 - Decimal mode
            uint8_t b : 1;  // bit 4 - Break (not a real flag, set on push)
            uint8_t u : 1;  // bit 5 - Unused (always 1 when pushed)
            uint8_t v : 1;  // bit 6 - Overflow
            uint8_t n : 1;  // bit 7 - Negative
        } flag;
    } p;

public:
    Flags6502() : p{} {}

    bool n() const { return p.flag.n; }
    bool v() const { return p.flag.v; }
    bool b() const { return p.flag.b; }
    bool d() const { return p.flag.d; }
    bool i() const { return p.flag.i; }
    bool z() const { return p.flag.z; }
    bool c() const { return p.flag.c; }

    void set_n(bool val) { p.flag.n = val; }
    void set_v(bool val) { p.flag.v = val; }
    void set_b(bool val) { p.flag.b = val; }
    void set_d(bool val) { p.flag.d = val; }
    void set_i(bool val) { p.flag.i = val; }
    void set_z(bool val) { p.flag.z = val; }
    void set_c(bool val) { p.flag.c = val; }

    uint8_t value() const { return p.reg | 0x20; }  // bit 5 always reads as 1
    void set_value(uint8_t val) { p.reg = val; }

    void test_n(uint8_t val)  { p.flag.n = (val & 0x80) != 0; }
    void test_z(uint8_t val)  { p.flag.z = (val == 0); }
    void test_nz(uint8_t val) { test_n(val); test_z(val); }
    void test_c(uint16_t val) { p.flag.c = (val & 0x100) != 0; }

    // Overflow for addition: +a + +b = -r or -a + -b = +r
    void test_av(uint8_t a, uint8_t b, uint16_t r) { p.flag.v = (((a ^ r) & (b ^ r)) & 0x80) != 0; }
    // Overflow for subtraction: +a - -b = -r or -a - +b = +r
    void test_sv(uint8_t a, uint8_t b, uint16_t r) { p.flag.v = (((a ^ b) & (a ^ r)) & 0x80) != 0; }

    void reset() { p.reg = 0; }
};

// ============================================================================
//  Register6502 - processor registers
// ============================================================================

struct Register6502 {
    uint8_t  a{};
    uint8_t  x{};
    uint8_t  y{};
    uint16_t pc{};
    uint8_t  sp{0xff};
    Flags6502 flag{};

    void reset() {
        a = x = y = 0;
        pc = 0;
        sp = 0xff;
        flag.reset();
    }
};

// ============================================================================
//  W65C02S - the processor
// ============================================================================

class W65C02S {
public:
    // Opcode entry - pairs an addressing mode with an instruction
    struct OpcodeEntry {
        AddressMode mode;
        uint8_t (W65C02S::*handler)(AddressMode&, uint8_t opcode);
    };

    Register6502 reg{};

    // Processor state
    uint64_t cycles{};      // Total cycle counter
    bool halted{};          // STP instruction executed
    bool waiting{};         // WAI instruction executed, waiting for interrupt
    bool irq_pending{};     // IRQ line asserted (level-triggered)
    bool nmi_pending{};     // NMI triggered (edge-triggered)

    // Memory interface - to be connected to actual RAM/ROM
    uint8_t (*ram_read)(uint16_t addr) = nullptr;
    void (*ram_write)(uint16_t addr, uint8_t val) = nullptr;

    W65C02S() { build_opcode_table(); }

    // Convenience for reading 16-bit values (little-endian)
    uint16_t ram_read_word(uint16_t addr) {
        return ram_read(addr) | (ram_read(addr + 1) << 8);
    }

    // Fetch bytes from PC
    uint8_t pop_byte_pc() { return ram_read(reg.pc++); }
    uint16_t pop_word_pc() { return pop_byte_pc() | (pop_byte_pc() << 8); }

    // Stack operations
    void stack_push(uint8_t val) { ram_write(0x0100 | reg.sp--, val); }
    void stack_push_word(uint16_t val) { stack_push(val >> 8); stack_push(val & 0xff); }
    uint8_t stack_pull() { return ram_read(0x0100 | ++reg.sp); }
    uint16_t stack_pull_word() { return stack_pull() | (stack_pull() << 8); }

    void reset() {
        reg.reset();
        cycles = 0;
        halted = false;
        waiting = false;
        irq_pending = false;
        nmi_pending = false;
    }

    // Interrupt interface
    void trigger_nmi() { nmi_pending = true; }
    void trigger_irq() { irq_pending = true; }
    void clear_irq() { irq_pending = false; }

    // Execute one instruction, returns cycle count
    int step() {
        // Halted by STP - only reset can recover
        if (halted) {
            cycles += 1;
            return 1;
        }

        // NMI has highest priority (edge-triggered)
        if (nmi_pending) {
            nmi_pending = false;
            waiting = false;  // NMI wakes from WAI
            stack_push_word(reg.pc);
            stack_push(reg.flag.value());  // NMI/IRQ push with B=0
            reg.flag.set_i(true);
            reg.flag.set_d(false);  // 65C02 clears D on interrupt
            reg.pc = ram_read_word(0xfffa);
            cycles += 7;
            return 7;
        }

        // IRQ (level-triggered, masked by I flag)
        if (irq_pending && !reg.flag.i()) {
            waiting = false;  // IRQ wakes from WAI
            stack_push_word(reg.pc);
            stack_push(reg.flag.value());  // B=0 for IRQ
            reg.flag.set_i(true);
            reg.flag.set_d(false);  // 65C02 clears D on interrupt
            reg.pc = ram_read_word(0xfffe);
            cycles += 7;
            return 7;
        }

        // WAI - stay waiting until interrupt arrives
        if (waiting) {
            cycles += 1;
            return 1;
        }

        // Normal instruction execution
        uint8_t opcode = ram_read(reg.pc++);
        auto& entry = op_[opcode];
        if (!entry.handler) {
            // Undefined opcode - treat as 1-cycle NOP
            cycles += 1;
            return 1;
        }
        int cyc = (this->*entry.handler)(entry.mode, opcode);
        cycles += cyc;
        return cyc;
    }

private:
    std::array<OpcodeEntry, 256> op_{};

public:
    // ========================================================================
    //  Instruction implementations (public for ISA table access)
    // ========================================================================

    // ------------------------------------------------------------------------
    //  Flag operations
    // ------------------------------------------------------------------------
    //                                            n v b d i z c
    // CLC   0 -> c                               - - - - - - 0
    uint8_t op_clc(AddressMode& m, uint8_t) { reg.flag.set_c(false); return m.cycles; }
    // CLD   0 -> d                               - - - 0 - - -
    uint8_t op_cld(AddressMode& m, uint8_t) { reg.flag.set_d(false); return m.cycles; }
    // CLI   0 -> i                               - - - - 0 - -
    uint8_t op_cli(AddressMode& m, uint8_t) { reg.flag.set_i(false); return m.cycles; }
    // CLV   0 -> v                               - 0 - - - - -
    uint8_t op_clv(AddressMode& m, uint8_t) { reg.flag.set_v(false); return m.cycles; }
    // SEC   1 -> c                               - - - - - - 1
    uint8_t op_sec(AddressMode& m, uint8_t) { reg.flag.set_c(true);  return m.cycles; }
    // SED   1 -> d                               - - - 1 - - -
    uint8_t op_sed(AddressMode& m, uint8_t) { reg.flag.set_d(true);  return m.cycles; }
    // SEI   1 -> i                               - - - - 1 - -
    uint8_t op_sei(AddressMode& m, uint8_t) { reg.flag.set_i(true);  return m.cycles; }

    // ------------------------------------------------------------------------
    //  Transfer operations
    // ------------------------------------------------------------------------
    //                                            n v b d i z c
    // TAX   a -> x                               + - - - - + -
    uint8_t op_tax(AddressMode& m, uint8_t) { reg.x = reg.a;  reg.flag.test_nz(reg.x); return m.cycles; }
    // TAY   a -> y                               + - - - - + -
    uint8_t op_tay(AddressMode& m, uint8_t) { reg.y = reg.a;  reg.flag.test_nz(reg.y); return m.cycles; }
    // TXA   x -> a                               + - - - - + -
    uint8_t op_txa(AddressMode& m, uint8_t) { reg.a = reg.x;  reg.flag.test_nz(reg.a); return m.cycles; }
    // TYA   y -> a                               + - - - - + -
    uint8_t op_tya(AddressMode& m, uint8_t) { reg.a = reg.y;  reg.flag.test_nz(reg.a); return m.cycles; }
    // TSX   sp -> x                              + - - - - + -
    uint8_t op_tsx(AddressMode& m, uint8_t) { reg.x = reg.sp; reg.flag.test_nz(reg.x); return m.cycles; }
    // TXS   x -> sp                              - - - - - - -
    uint8_t op_txs(AddressMode& m, uint8_t) { reg.sp = reg.x; return m.cycles; }

    // ------------------------------------------------------------------------
    //  Load operations
    // ------------------------------------------------------------------------
    //                                            n v b d i z c
    // LDA   m -> a                               + - - - - + -
    uint8_t op_lda(AddressMode& m, uint8_t) {
        reg.a = m.get(*this, m);
        reg.flag.test_nz(reg.a);
        return m.cycles + m.page_penalty;
    }

    //                                            n v b d i z c
    // LDX   m -> x                               + - - - - + -
    uint8_t op_ldx(AddressMode& m, uint8_t) {
        reg.x = m.get(*this, m);
        reg.flag.test_nz(reg.x);
        return m.cycles + m.page_penalty;
    }

    //                                            n v b d i z c
    // LDY   m -> y                               + - - - - + -
    uint8_t op_ldy(AddressMode& m, uint8_t) {
        reg.y = m.get(*this, m);
        reg.flag.test_nz(reg.y);
        return m.cycles + m.page_penalty;
    }

    // ------------------------------------------------------------------------
    //  Store operations
    // ------------------------------------------------------------------------
    //                                            n v b d i z c
    // STA   a -> m                               - - - - - - -
    uint8_t op_sta(AddressMode& m, uint8_t) {
        m.resolve(*this, m);
        m.write(*this, m, reg.a);
        return m.cycles;
    }

    //                                            n v b d i z c
    // STX   x -> m                               - - - - - - -
    uint8_t op_stx(AddressMode& m, uint8_t) {
        m.resolve(*this, m);
        m.write(*this, m, reg.x);
        return m.cycles;
    }

    //                                            n v b d i z c
    // STY   y -> m                               - - - - - - -
    uint8_t op_sty(AddressMode& m, uint8_t) {
        m.resolve(*this, m);
        m.write(*this, m, reg.y);
        return m.cycles;
    }

    //                                            n v b d i z c
    // STZ   0 -> m                               - - - - - - -
    uint8_t op_stz(AddressMode& m, uint8_t) {
        m.resolve(*this, m);
        m.write(*this, m, 0);
        return m.cycles;
    }

    // ------------------------------------------------------------------------
    //  Stack operations
    // ------------------------------------------------------------------------
    //                                            n v b d i z c
    // PHA   a -> push stack                      - - - - - - -
    uint8_t op_pha(AddressMode& m, uint8_t) { stack_push(reg.a); return m.cycles; }
    // PHX   x -> push stack                      - - - - - - -
    uint8_t op_phx(AddressMode& m, uint8_t) { stack_push(reg.x); return m.cycles; }
    // PHY   y -> push stack                      - - - - - - -
    uint8_t op_phy(AddressMode& m, uint8_t) { stack_push(reg.y); return m.cycles; }
    // PHP   proc status -> push stack            - - - - - - -
    uint8_t op_php(AddressMode& m, uint8_t) { stack_push(reg.flag.value() | 0x10); return m.cycles; }

    // PLA   pull stack -> a                      + - - - - + -
    uint8_t op_pla(AddressMode&, uint8_t) { reg.a = stack_pull(); reg.flag.test_nz(reg.a); return 4; }
    // PLX   pull stack -> x                      + - - - - + -
    uint8_t op_plx(AddressMode&, uint8_t) { reg.x = stack_pull(); reg.flag.test_nz(reg.x); return 4; }
    // PLY   pull stack -> y                      + - - - - + -
    uint8_t op_ply(AddressMode&, uint8_t) { reg.y = stack_pull(); reg.flag.test_nz(reg.y); return 4; }
    // PLP   pull stack -> proc status            from stack
    uint8_t op_plp(AddressMode&, uint8_t) { reg.flag.set_value(stack_pull()); reg.flag.set_b(false); return 4; }

    // ------------------------------------------------------------------------
    //  Logic operations
    // ------------------------------------------------------------------------
    //                                            n v b d i z c
    // AND   a & m -> a                           + - - - - + -
    uint8_t op_and(AddressMode& m, uint8_t) {
        reg.a &= m.get(*this, m);
        reg.flag.test_nz(reg.a);
        return m.cycles + m.page_penalty;
    }

    //                                            n v b d i z c
    // ORA   a | m -> a                           + - - - - + -
    uint8_t op_ora(AddressMode& m, uint8_t) {
        reg.a |= m.get(*this, m);
        reg.flag.test_nz(reg.a);
        return m.cycles + m.page_penalty;
    }

    //                                            n v b d i z c
    // EOR   a ^ m -> a                           + - - - - + -
    uint8_t op_eor(AddressMode& m, uint8_t) {
        reg.a ^= m.get(*this, m);
        reg.flag.test_nz(reg.a);
        return m.cycles + m.page_penalty;
    }

    // ------------------------------------------------------------------------
    //  Compare operations
    // ------------------------------------------------------------------------
    //                                            n v b d i z c
    // CMP   a - m                                + - - - - + +
    uint8_t op_cmp(AddressMode& m, uint8_t) {
        uint8_t val = m.get(*this, m);
        uint16_t res = reg.a - val;
        reg.flag.set_c(reg.a >= val);
        reg.flag.test_nz(res & 0xff);
        return m.cycles + m.page_penalty;
    }

    //                                            n v b d i z c
    // CPX   x - m                                + - - - - + +
    uint8_t op_cpx(AddressMode& m, uint8_t) {
        uint8_t val = m.get(*this, m);
        uint16_t res = reg.x - val;
        reg.flag.set_c(reg.x >= val);
        reg.flag.test_nz(res & 0xff);
        return m.cycles;
    }

    //                                            n v b d i z c
    // CPY   y - m                                + - - - - + +
    uint8_t op_cpy(AddressMode& m, uint8_t) {
        uint8_t val = m.get(*this, m);
        uint16_t res = reg.y - val;
        reg.flag.set_c(reg.y >= val);
        reg.flag.test_nz(res & 0xff);
        return m.cycles;
    }

    // ------------------------------------------------------------------------
    //  Arithmetic operations
    // ------------------------------------------------------------------------
    //                                            n v b d i z c
    // ADC   a + m + c -> a, c                    + + - - - + +
    uint8_t op_adc(AddressMode& m, uint8_t) {
        uint8_t val = m.get(*this, m);
        uint8_t a = reg.a;
        uint16_t res;

        if (reg.flag.d()) {  // BCD mode
            res = (a & 0x0f) + (val & 0x0f) + (reg.flag.c() ? 1 : 0);
            if (res > 0x09) res += 0x06;
            res += (a & 0xf0) + (val & 0xf0);
            reg.flag.test_av(a, val, res);
            if (res > 0x99) res += 0x60;
        } else {
            res = a + val + (reg.flag.c() ? 1 : 0);
            reg.flag.test_av(a, val, res);
        }

        reg.a = res & 0xff;
        reg.flag.test_nz(reg.a);
        reg.flag.test_c(res);
        return m.cycles + m.page_penalty;
    }

    //                                            n v b d i z c
    // SBC   a - m - c -> a                       + + - - - + +
    uint8_t op_sbc(AddressMode& m, uint8_t) {
        uint8_t val = m.get(*this, m);
        uint8_t a = reg.a;
        uint16_t res;

        if (reg.flag.d()) {  // BCD mode
            uint8_t vc = val ^ 0xff;
            res = (a & 0x0f) + (vc & 0x0f) + (reg.flag.c() ? 1 : 0);
            if (res < 0x10) res -= 0x06;
            res += (a & 0xf0) + (vc & 0xf0);
            reg.flag.test_sv(a, val, res);
            if (res < 0x100) res -= 0x60;
        } else {
            res = a + (val ^ 0xff) + (reg.flag.c() ? 1 : 0);
            reg.flag.test_sv(a, val, res);
        }

        reg.a = res & 0xff;
        reg.flag.test_nz(reg.a);
        reg.flag.test_c(res);
        return m.cycles + m.page_penalty;
    }

    // ------------------------------------------------------------------------
    //  Increment/Decrement operations
    // ------------------------------------------------------------------------
    //                                            n v b d i z c
    // INX   x + 1 -> x                           + - - - - + -
    uint8_t op_inx(AddressMode& m, uint8_t) { reg.x++; reg.flag.test_nz(reg.x); return m.cycles; }
    // INY   y + 1 -> y                           + - - - - + -
    uint8_t op_iny(AddressMode& m, uint8_t) { reg.y++; reg.flag.test_nz(reg.y); return m.cycles; }
    // DEX   x - 1 -> x                           + - - - - + -
    uint8_t op_dex(AddressMode& m, uint8_t) { reg.x--; reg.flag.test_nz(reg.x); return m.cycles; }
    // DEY   y - 1 -> y                           + - - - - + -
    uint8_t op_dey(AddressMode& m, uint8_t) { reg.y--; reg.flag.test_nz(reg.y); return m.cycles; }

    //                                            n v b d i z c
    // INC   m + 1 -> m                           + - - - - + -
    uint8_t op_inc(AddressMode& m, uint8_t) {
        uint8_t val = m.get(*this, m);
        val++;
        m.write(*this, m, val);
        reg.flag.test_nz(val);
        return m.cycles + m.write_extra;
    }

    //                                            n v b d i z c
    // DEC   m - 1 -> m                           + - - - - + -
    uint8_t op_dec(AddressMode& m, uint8_t) {
        uint8_t val = m.get(*this, m);
        val--;
        m.write(*this, m, val);
        reg.flag.test_nz(val);
        return m.cycles + m.write_extra;
    }

    // ------------------------------------------------------------------------
    //  Shift/Rotate operations
    // ------------------------------------------------------------------------
    //                                            n v b d i z c
    // ASL   c <- [76543210] <- 0                 + - - - - + +
    uint8_t op_asl(AddressMode& m, uint8_t) {
        uint8_t val = m.get(*this, m);
        reg.flag.set_c(val & 0x80);
        val <<= 1;
        m.write(*this, m, val);
        reg.flag.test_nz(val);
        return m.cycles + m.write_extra;
    }

    //                                            n v b d i z c
    // LSR   0 -> [76543210] -> c                 0 - - - - + +
    uint8_t op_lsr(AddressMode& m, uint8_t) {
        uint8_t val = m.get(*this, m);
        reg.flag.set_c(val & 0x01);
        val >>= 1;
        m.write(*this, m, val);
        reg.flag.test_nz(val);
        return m.cycles + m.write_extra;
    }

    //                                            n v b d i z c
    // ROL   c <- [76543210] <- c                 + - - - - + +
    uint8_t op_rol(AddressMode& m, uint8_t) {
        uint8_t val = m.get(*this, m);
        uint8_t carry_in = reg.flag.c() ? 0x01 : 0x00;
        reg.flag.set_c(val & 0x80);
        val = (val << 1) | carry_in;
        m.write(*this, m, val);
        reg.flag.test_nz(val);
        return m.cycles + m.write_extra;
    }

    //                                            n v b d i z c
    // ROR   c -> [76543210] -> c                 + - - - - + +
    uint8_t op_ror(AddressMode& m, uint8_t) {
        uint8_t val = m.get(*this, m);
        uint8_t carry_in = reg.flag.c() ? 0x80 : 0x00;
        reg.flag.set_c(val & 0x01);
        val = (val >> 1) | carry_in;
        m.write(*this, m, val);
        reg.flag.test_nz(val);
        return m.cycles + m.write_extra;
    }

    // ------------------------------------------------------------------------
    //  Branch operations
    // ------------------------------------------------------------------------
    //                                            n v b d i z c
    // BCC   branch on carry clear (c = 0)        - - - - - - -
    uint8_t op_bcc(AddressMode& m, uint8_t) {
        uint16_t target = m.resolve(*this, m);
        if (!reg.flag.c()) { reg.pc = target; return m.cycles + m.branch_extra + m.page_penalty; }
        return m.cycles;
    }

    //                                            n v b d i z c
    // BCS   branch on carry set (c = 1)          - - - - - - -
    uint8_t op_bcs(AddressMode& m, uint8_t) {
        uint16_t target = m.resolve(*this, m);
        if (reg.flag.c()) { reg.pc = target; return m.cycles + m.branch_extra + m.page_penalty; }
        return m.cycles;
    }

    //                                            n v b d i z c
    // BEQ   branch on result zero (z = 1)        - - - - - - -
    uint8_t op_beq(AddressMode& m, uint8_t) {
        uint16_t target = m.resolve(*this, m);
        if (reg.flag.z()) { reg.pc = target; return m.cycles + m.branch_extra + m.page_penalty; }
        return m.cycles;
    }

    //                                            n v b d i z c
    // BNE   branch on result not zero (z = 0)    - - - - - - -
    uint8_t op_bne(AddressMode& m, uint8_t) {
        uint16_t target = m.resolve(*this, m);
        if (!reg.flag.z()) { reg.pc = target; return m.cycles + m.branch_extra + m.page_penalty; }
        return m.cycles;
    }

    //                                            n v b d i z c
    // BMI   branch on result minus (n = 1)       - - - - - - -
    uint8_t op_bmi(AddressMode& m, uint8_t) {
        uint16_t target = m.resolve(*this, m);
        if (reg.flag.n()) { reg.pc = target; return m.cycles + m.branch_extra + m.page_penalty; }
        return m.cycles;
    }

    //                                            n v b d i z c
    // BPL   branch on result plus (n = 0)        - - - - - - -
    uint8_t op_bpl(AddressMode& m, uint8_t) {
        uint16_t target = m.resolve(*this, m);
        if (!reg.flag.n()) { reg.pc = target; return m.cycles + m.branch_extra + m.page_penalty; }
        return m.cycles;
    }

    //                                            n v b d i z c
    // BVC   branch on overflow clear (v = 0)     - - - - - - -
    uint8_t op_bvc(AddressMode& m, uint8_t) {
        uint16_t target = m.resolve(*this, m);
        if (!reg.flag.v()) { reg.pc = target; return m.cycles + m.branch_extra + m.page_penalty; }
        return m.cycles;
    }

    //                                            n v b d i z c
    // BVS   branch on overflow set (v = 1)       - - - - - - -
    uint8_t op_bvs(AddressMode& m, uint8_t) {
        uint16_t target = m.resolve(*this, m);
        if (reg.flag.v()) { reg.pc = target; return m.cycles + m.branch_extra + m.page_penalty; }
        return m.cycles;
    }

    //                                            n v b d i z c
    // BRA   branch always                        - - - - - - -
    uint8_t op_bra(AddressMode& m, uint8_t) {
        uint16_t target = m.resolve(*this, m);
        reg.pc = target;
        return m.cycles + m.branch_extra + m.page_penalty;
    }

    // ------------------------------------------------------------------------
    //  Jump/Return operations
    // ------------------------------------------------------------------------
    //                                            n v b d i z c
    // JMP   m -> pc                              - - - - - - -
    uint8_t op_jmp(AddressMode& m, uint8_t opcode) {
        reg.pc = m.resolve(*this, m);
        return (opcode == 0x4c) ? 3 : m.cycles;  // absolute is 3 cycles, indirect modes are 6
    }

    //                                            n v b d i z c
    // JSR   push pc, m -> pc                     - - - - - - -
    uint8_t op_jsr(AddressMode& m, uint8_t) {
        uint16_t target = m.resolve(*this, m);
        stack_push_word(reg.pc - 1);
        reg.pc = target;
        return 6;
    }

    //                                            n v b d i z c
    // RTS   pull stack -> pc                     - - - - - - -
    uint8_t op_rts(AddressMode&, uint8_t) {
        reg.pc = stack_pull_word() + 1;
        return 6;
    }

    //                                            n v b d i z c
    // RTI   pull stack -> sr, pull stack -> pc   from stack
    uint8_t op_rti(AddressMode&, uint8_t) {
        reg.flag.set_value(stack_pull());
        reg.flag.set_b(false);
        reg.pc = stack_pull_word();
        return 6;
    }

    // ------------------------------------------------------------------------
    //  Bit test operations
    // ------------------------------------------------------------------------
    //                                            n v b d i z c
    // BIT   a & m -> z, m7 -> n, m6 -> v        m7 m6 - - - + -
    uint8_t op_bit(AddressMode& m, uint8_t opcode) {
        uint8_t val = m.get(*this, m);
        reg.flag.test_z(val & reg.a);
        // Immediate mode (0x89) does not affect N and V
        if (opcode != 0x89) {
            reg.flag.set_n(val & 0x80);
            reg.flag.set_v(val & 0x40);
        }
        return m.cycles + m.page_penalty;
    }

    //                                            n v b d i z c
    // TRB   m & a -> z, m & ~a -> m              - - - - - + -
    uint8_t op_trb(AddressMode& m, uint8_t) {
        uint8_t val = m.get(*this, m);
        reg.flag.test_z(val & reg.a);
        m.write(*this, m, val & ~reg.a);
        return m.cycles + m.write_extra;
    }

    //                                            n v b d i z c
    // TSB   m & a -> z, m | a -> m               - - - - - + -
    uint8_t op_tsb(AddressMode& m, uint8_t) {
        uint8_t val = m.get(*this, m);
        reg.flag.test_z(val & reg.a);
        m.write(*this, m, val | reg.a);
        return m.cycles + m.write_extra;
    }

    // ------------------------------------------------------------------------
    //  65C02 bit manipulation operations
    // ------------------------------------------------------------------------
    //                                            n v b d i z c
    // RMB   reset memory bit b                   - - - - - - -
    uint8_t op_rmb(AddressMode& m, uint8_t opcode) {
        uint8_t bit = (opcode >> 4) & 0x07;
        uint8_t val = m.get(*this, m);
        m.write(*this, m, val & ~(1 << bit));
        return m.cycles + m.write_extra;
    }

    //                                            n v b d i z c
    // SMB   set memory bit b                     - - - - - - -
    uint8_t op_smb(AddressMode& m, uint8_t opcode) {
        uint8_t bit = (opcode >> 4) & 0x07;
        uint8_t val = m.get(*this, m);
        m.write(*this, m, val | (1 << bit));
        return m.cycles + m.write_extra;
    }

    //                                            n v b d i z c
    // BBR   branch on bit b reset                - - - - - - -
    uint8_t op_bbr(AddressMode& m, uint8_t opcode) {
        uint8_t bit = (opcode >> 4) & 0x07;
        // MODE_ZP_REL: get() reads from zp, resolve() returns branch target
        uint8_t val = m.get(*this, m);
        // Need to fetch the relative offset and compute target
        int8_t off = static_cast<int8_t>(ram_read(reg.pc++));
        uint16_t target = (reg.pc + off) & 0xffff;
        m.page_penalty = ((reg.pc ^ target) & 0xff00) ? 1 : 0;

        if (!((val >> bit) & 0x01)) {
            reg.pc = target;
            return m.cycles + m.branch_extra + m.page_penalty;
        }
        return m.cycles;
    }

    //                                            n v b d i z c
    // BBS   branch on bit b set                  - - - - - - -
    uint8_t op_bbs(AddressMode& m, uint8_t opcode) {
        uint8_t bit = (opcode >> 4) & 0x07;
        uint8_t val = m.get(*this, m);
        int8_t off = static_cast<int8_t>(ram_read(reg.pc++));
        uint16_t target = (reg.pc + off) & 0xffff;
        m.page_penalty = ((reg.pc ^ target) & 0xff00) ? 1 : 0;

        if ((val >> bit) & 0x01) {
            reg.pc = target;
            return m.cycles + m.branch_extra + m.page_penalty;
        }
        return m.cycles;
    }

    // ------------------------------------------------------------------------
    //  Special operations
    // ------------------------------------------------------------------------
    //                                            n v b d i z c
    // BRK   break                                - - 1 0 1 - -
    uint8_t op_brk(AddressMode&, uint8_t) {
        reg.pc++;  // BRK skips the signature byte
        stack_push_word(reg.pc);
        stack_push(reg.flag.value() | 0x10);  // B flag set
        reg.flag.set_d(false);
        reg.flag.set_i(true);
        reg.pc = ram_read_word(0xfffe);
        return 7;
    }

    //                                            n v b d i z c
    // NOP   no operation                         - - - - - - -
    uint8_t op_nop(AddressMode& m, uint8_t) {
        reg.pc += m.bytes - 1;
        return m.cycles;
    }

    //                                            n v b d i z c
    // STP   processor halt                       - - - - - - -
    uint8_t op_stp(AddressMode& m, uint8_t) {
        halted = true;
        return m.cycles;
    }

    //                                            n v b d i z c
    // WAI   wait for interrupt                   - - - - - - -
    uint8_t op_wai(AddressMode& m, uint8_t) {
        waiting = true;
        return m.cycles;
    }

    // Opcode table construction - defined after W65C02S_ISA_TABLE
    inline void build_opcode_table();
};

// ============================================================================
//  Addressing mode function implementations
// ============================================================================

// --- Get functions ---

inline uint8_t get_abs(W65C02S& cpu, AddressMode& m) {
    m.eff_addr = cpu.pop_word_pc();
    m.page_penalty = 0;
    return cpu.ram_read(m.eff_addr);
}

inline uint8_t get_abs_x(W65C02S& cpu, AddressMode& m) {
    uint16_t base = cpu.pop_word_pc();
    m.eff_addr = (base + cpu.reg.x) & 0xffff;
    m.page_penalty = ((base ^ m.eff_addr) & 0xff00) ? 1 : 0;
    return cpu.ram_read(m.eff_addr);
}

inline uint8_t get_abs_y(W65C02S& cpu, AddressMode& m) {
    uint16_t base = cpu.pop_word_pc();
    m.eff_addr = (base + cpu.reg.y) & 0xffff;
    m.page_penalty = ((base ^ m.eff_addr) & 0xff00) ? 1 : 0;
    return cpu.ram_read(m.eff_addr);
}

inline uint8_t get_zp(W65C02S& cpu, AddressMode& m) {
    m.eff_addr = cpu.pop_byte_pc();
    m.page_penalty = 0;
    return cpu.ram_read(m.eff_addr);
}

inline uint8_t get_zp_x(W65C02S& cpu, AddressMode& m) {
    m.eff_addr = (cpu.pop_byte_pc() + cpu.reg.x) & 0xff;
    m.page_penalty = 0;
    return cpu.ram_read(m.eff_addr);
}

inline uint8_t get_zp_y(W65C02S& cpu, AddressMode& m) {
    m.eff_addr = (cpu.pop_byte_pc() + cpu.reg.y) & 0xff;
    m.page_penalty = 0;
    return cpu.ram_read(m.eff_addr);
}

inline uint8_t get_zp_ind(W65C02S& cpu, AddressMode& m) {
    m.eff_addr = cpu.ram_read_word(cpu.pop_byte_pc());
    m.page_penalty = 0;
    return cpu.ram_read(m.eff_addr);
}

inline uint8_t get_zp_x_ind(W65C02S& cpu, AddressMode& m) {
    m.eff_addr = cpu.ram_read_word((cpu.pop_byte_pc() + cpu.reg.x) & 0xff);
    m.page_penalty = 0;
    return cpu.ram_read(m.eff_addr);
}

inline uint8_t get_zp_ind_y(W65C02S& cpu, AddressMode& m) {
    uint16_t base = cpu.ram_read_word(cpu.pop_byte_pc());
    m.eff_addr = (base + cpu.reg.y) & 0xffff;
    m.page_penalty = ((base ^ m.eff_addr) & 0xff00) ? 1 : 0;
    return cpu.ram_read(m.eff_addr);
}

inline uint8_t get_imm(W65C02S& cpu, AddressMode& m) {
    m.page_penalty = 0;
    return cpu.pop_byte_pc();
}

inline uint8_t get_acc(W65C02S& cpu, AddressMode& m) {
    m.page_penalty = 0;
    return cpu.reg.a;
}

// --- Resolve functions ---

inline uint16_t resolve_abs(W65C02S& cpu, AddressMode& m) {
    m.eff_addr = cpu.pop_word_pc();
    m.page_penalty = 0;
    return m.eff_addr;
}

inline uint16_t resolve_abs_x(W65C02S& cpu, AddressMode& m) {
    uint16_t base = cpu.pop_word_pc();
    m.eff_addr = (base + cpu.reg.x) & 0xffff;
    m.page_penalty = ((base ^ m.eff_addr) & 0xff00) ? 1 : 0;
    return m.eff_addr;
}

inline uint16_t resolve_abs_y(W65C02S& cpu, AddressMode& m) {
    uint16_t base = cpu.pop_word_pc();
    m.eff_addr = (base + cpu.reg.y) & 0xffff;
    m.page_penalty = ((base ^ m.eff_addr) & 0xff00) ? 1 : 0;
    return m.eff_addr;
}

inline uint16_t resolve_abs_ind(W65C02S& cpu, AddressMode& m) {
    m.eff_addr = cpu.ram_read_word(cpu.pop_word_pc());
    m.page_penalty = 0;
    return m.eff_addr;
}

inline uint16_t resolve_abs_x_ind(W65C02S& cpu, AddressMode& m) {
    m.eff_addr = cpu.ram_read_word((cpu.pop_word_pc() + cpu.reg.x) & 0xffff);
    m.page_penalty = 0;
    return m.eff_addr;
}

inline uint16_t resolve_zp(W65C02S& cpu, AddressMode& m) {
    m.eff_addr = cpu.pop_byte_pc();
    m.page_penalty = 0;
    return m.eff_addr;
}

inline uint16_t resolve_zp_x(W65C02S& cpu, AddressMode& m) {
    m.eff_addr = (cpu.pop_byte_pc() + cpu.reg.x) & 0xff;
    m.page_penalty = 0;
    return m.eff_addr;
}

inline uint16_t resolve_zp_y(W65C02S& cpu, AddressMode& m) {
    m.eff_addr = (cpu.pop_byte_pc() + cpu.reg.y) & 0xff;
    m.page_penalty = 0;
    return m.eff_addr;
}

inline uint16_t resolve_zp_ind(W65C02S& cpu, AddressMode& m) {
    m.eff_addr = cpu.ram_read_word(cpu.pop_byte_pc());
    m.page_penalty = 0;
    return m.eff_addr;
}

inline uint16_t resolve_zp_x_ind(W65C02S& cpu, AddressMode& m) {
    m.eff_addr = cpu.ram_read_word((cpu.pop_byte_pc() + cpu.reg.x) & 0xff);
    m.page_penalty = 0;
    return m.eff_addr;
}

inline uint16_t resolve_zp_ind_y(W65C02S& cpu, AddressMode& m) {
    uint16_t base = cpu.ram_read_word(cpu.pop_byte_pc());
    m.eff_addr = (base + cpu.reg.y) & 0xffff;
    m.page_penalty = ((base ^ m.eff_addr) & 0xff00) ? 1 : 0;
    return m.eff_addr;
}

inline uint16_t resolve_rel(W65C02S& cpu, AddressMode& m) {
    int8_t off = static_cast<int8_t>(cpu.pop_byte_pc());
    uint16_t base = cpu.reg.pc;
    m.eff_addr = (base + off) & 0xffff;
    m.page_penalty = ((base ^ m.eff_addr) & 0xff00) ? 1 : 0;
    return m.eff_addr;
}

inline uint16_t resolve_zp_rel(W65C02S& cpu, AddressMode& m) {
    m.eff_addr = cpu.pop_byte_pc();  // zp address for bit test
    int8_t off = static_cast<int8_t>(cpu.pop_byte_pc());
    uint16_t base = cpu.reg.pc;
    uint16_t target = (base + off) & 0xffff;
    m.page_penalty = ((base ^ target) & 0xff00) ? 1 : 0;
    // Note: eff_addr holds zp address, target is computed for branch
    // Caller must handle this specially for BBR/BBS
    return target;
}

// --- Write functions ---

inline void write_mem(W65C02S& cpu, AddressMode& m, uint8_t val) {
    cpu.ram_write(m.eff_addr, val);
}

inline void write_acc(W65C02S& cpu, AddressMode&, uint8_t val) {
    cpu.reg.a = val;
}

// ============================================================================
//  Addressing mode constants
// ============================================================================

// Ordered to match W65C02S_ISA column order: abs, absxi, absx, absy, absi, acum, imm, imp, rel, zprel, stck, zp, zpxi, zpx, zpy, zpi, zpiy
//                                       name                      get            write      resolve             bytes cyc  wr  br
inline constexpr AddressMode MODE_ABS      {"absolute",             get_abs,       write_mem, resolve_abs,        3,    4,   2,  0};
inline constexpr AddressMode MODE_ABS_X_IND{"absolute_x_indirect",  nullptr,       nullptr,   resolve_abs_x_ind,  3,    6,   0,  0};
inline constexpr AddressMode MODE_ABS_X    {"absolute_x",           get_abs_x,     write_mem, resolve_abs_x,      3,    4,   2,  0};
inline constexpr AddressMode MODE_ABS_Y    {"absolute_y",           get_abs_y,     write_mem, resolve_abs_y,      3,    4,   0,  0};
inline constexpr AddressMode MODE_ABS_IND  {"absolute_indirect",    nullptr,       nullptr,   resolve_abs_ind,    3,    6,   0,  0};
inline constexpr AddressMode MODE_ACC      {"accumulator",          get_acc,       write_acc, nullptr,            1,    2,   0,  0};
inline constexpr AddressMode MODE_IMM      {"immediate",            get_imm,       nullptr,   nullptr,            2,    2,   0,  0};
inline constexpr AddressMode MODE_IMP      {"implied",              nullptr,       nullptr,   nullptr,            1,    2,   0,  0};
inline constexpr AddressMode MODE_REL      {"relative",             nullptr,       nullptr,   resolve_rel,        2,    2,   0,  1};
inline constexpr AddressMode MODE_ZP_REL   {"zero_page_relative",   get_zp,        nullptr,   resolve_zp_rel,     3,    5,   0,  1};
inline constexpr AddressMode MODE_STACK    {"stack",                nullptr,       nullptr,   nullptr,            1,    3,   0,  0};
inline constexpr AddressMode MODE_ZP       {"zero_page",            get_zp,        write_mem, resolve_zp,         2,    3,   2,  0};
inline constexpr AddressMode MODE_ZP_X_IND {"zero_page_x_indirect", get_zp_x_ind,  write_mem, resolve_zp_x_ind,   2,    6,   0,  0};
inline constexpr AddressMode MODE_ZP_X     {"zero_page_x",          get_zp_x,      write_mem, resolve_zp_x,       2,    4,   2,  0};
inline constexpr AddressMode MODE_ZP_Y     {"zero_page_y",          get_zp_y,      write_mem, resolve_zp_y,       2,    4,   0,  0};
inline constexpr AddressMode MODE_ZP_IND   {"zero_page_indirect",   get_zp_ind,    write_mem, resolve_zp_ind,     2,    5,   0,  0};
inline constexpr AddressMode MODE_ZP_IND_Y {"zero_page_indirect_y", get_zp_ind_y,  write_mem, resolve_zp_ind_y,   2,    5,   0,  0};

// ============================================================================
//  ISA Table (generated from X-macro)
// ============================================================================

struct InstructionDef {
    const char* mnemonic;
    int16_t opcodes[17];  // -1 means addressing mode not available
    uint8_t (W65C02S::*handler)(AddressMode&, uint8_t);
};

#define MAKE_ENTRY(name, abs, absxi, absx, absy, absi, acum, imm, imp, rel, zprel, stck, zp, zpxi, zpx, zpy, zpi, zpiy, handler) \
    { #name, { abs, absxi, absx, absy, absi, acum, imm, imp, rel, zprel, stck, zp, zpxi, zpx, zpy, zpi, zpiy }, &W65C02S::handler },

inline const InstructionDef W65C02S_ISA_TABLE[] = {
    W65C02S_ISA(MAKE_ENTRY)
};

#undef MAKE_ENTRY
#undef __

// ============================================================================
//  Opcode table construction
// ============================================================================

inline void W65C02S::build_opcode_table() {
    // Addressing mode lookup table (matches column order in W65C02S_ISA macro)
    static constexpr const AddressMode* ADDR_MODES[] = {
        &MODE_ABS,       // abs
        &MODE_ABS_X_IND, // absxi (absolute indexed indirect)
        &MODE_ABS_X,     // absx
        &MODE_ABS_Y,     // absy
        &MODE_ABS_IND,   // absi (absolute indirect)
        &MODE_ACC,       // acum
        &MODE_IMM,       // imm
        &MODE_IMP,       // imp
        &MODE_REL,       // rel
        &MODE_ZP_REL,    // zprel
        &MODE_STACK,     // stck
        &MODE_ZP,        // zp
        &MODE_ZP_X_IND,  // zpxi (zero page indexed indirect)
        &MODE_ZP_X,      // zpx
        &MODE_ZP_Y,      // zpy
        &MODE_ZP_IND,    // zpi (zero page indirect)
        &MODE_ZP_IND_Y,  // zpiy (zero page indirect indexed)
    };

    // Initialize all entries to nullptr (handled as 1-cycle NOP in step())
    op_.fill({MODE_IMP, nullptr});

    // Populate from ISA table
    for (const auto& instr : W65C02S_ISA_TABLE) {
        for (int mode = 0; mode < 17; ++mode) {
            int16_t opcode = instr.opcodes[mode];
            if (opcode >= 0) {
                op_[opcode] = {*ADDR_MODES[mode], instr.handler};
            }
        }
    }

    // Undefined opcodes - WDC 65C02 treats these as NOPs with various byte/cycle counts
    // 1-byte undefined opcodes (1 cycle) - $x3 and $xB patterns
    for (uint8_t hi = 0; hi < 0x10; hi++) {
        uint8_t op1 = (hi << 4) | 0x03;
        uint8_t op2 = (hi << 4) | 0x0b;
        if (!op_[op1].handler) op_[op1] = {{"undefined", nullptr, nullptr, nullptr, 1, 1, 0, 0}, &W65C02S::op_nop};
        if (!op_[op2].handler) op_[op2] = {{"undefined", nullptr, nullptr, nullptr, 1, 1, 0, 0}, &W65C02S::op_nop};
    }

    // 2-byte undefined opcodes
    op_[0x02] = {{"undefined", nullptr, nullptr, nullptr, 2, 2, 0, 0}, &W65C02S::op_nop};
    op_[0x22] = {{"undefined", nullptr, nullptr, nullptr, 2, 2, 0, 0}, &W65C02S::op_nop};
    op_[0x42] = {{"undefined", nullptr, nullptr, nullptr, 2, 2, 0, 0}, &W65C02S::op_nop};
    op_[0x62] = {{"undefined", nullptr, nullptr, nullptr, 2, 2, 0, 0}, &W65C02S::op_nop};
    op_[0x82] = {{"undefined", nullptr, nullptr, nullptr, 2, 2, 0, 0}, &W65C02S::op_nop};
    op_[0xc2] = {{"undefined", nullptr, nullptr, nullptr, 2, 2, 0, 0}, &W65C02S::op_nop};
    op_[0xe2] = {{"undefined", nullptr, nullptr, nullptr, 2, 2, 0, 0}, &W65C02S::op_nop};
    op_[0x44] = {{"undefined", nullptr, nullptr, nullptr, 2, 3, 0, 0}, &W65C02S::op_nop};
    op_[0x54] = {{"undefined", nullptr, nullptr, nullptr, 2, 4, 0, 0}, &W65C02S::op_nop};
    op_[0xd4] = {{"undefined", nullptr, nullptr, nullptr, 2, 4, 0, 0}, &W65C02S::op_nop};
    op_[0xf4] = {{"undefined", nullptr, nullptr, nullptr, 2, 4, 0, 0}, &W65C02S::op_nop};

    // 3-byte undefined opcodes
    op_[0x5c] = {{"undefined", nullptr, nullptr, nullptr, 3, 8, 0, 0}, &W65C02S::op_nop};
    op_[0xdc] = {{"undefined", nullptr, nullptr, nullptr, 3, 4, 0, 0}, &W65C02S::op_nop};
    op_[0xfc] = {{"undefined", nullptr, nullptr, nullptr, 3, 4, 0, 0}, &W65C02S::op_nop};
}
