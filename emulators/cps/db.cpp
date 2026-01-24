#include "db.h"
#include "../../../utilities/miniz/miniz.h"
#include <algorithm>
#include <cstring>

namespace cps {

// ============================================================================
// Graphics ROM Bank Mapper Tables
// ============================================================================

static const struct GfxRange mapper_LWCHR_table[] = {
    { GFXTYPE_SPRITES, 0x00000, 0x07fff, 0 },
    { GFXTYPE_SCROLL1, 0x00000, 0x1ffff, 0 },
    { GFXTYPE_STARS,   0x00000, 0x1ffff, 1 },
    { GFXTYPE_SCROLL2, 0x00000, 0x1ffff, 1 },
    { GFXTYPE_SCROLL3, 0x00000, 0x1ffff, 1 },
    { 0              ,       0,       0, 0 }
};

static const struct GfxRange mapper_LW621_table[] = {
    { GFXTYPE_SPRITES, 0x00000, 0x07fff, 0 },
    { GFXTYPE_SCROLL1, 0x00000, 0x1ffff, 0 },
    { GFXTYPE_STARS,   0x00000, 0x1ffff, 1 },
    { GFXTYPE_SCROLL2, 0x00000, 0x1ffff, 1 },
    { GFXTYPE_SCROLL3, 0x00000, 0x1ffff, 1 },
    { 0              ,       0,       0, 0 }
};

static const struct GfxRange mapper_DM620_table[] = {
    { GFXTYPE_SCROLL3, 0x8000, 0xbfff, 1 },
    { GFXTYPE_SPRITES, 0x2000, 0x3fff, 2 },
    { GFXTYPE_STARS | GFXTYPE_SPRITES | GFXTYPE_SCROLL1 | GFXTYPE_SCROLL2 | GFXTYPE_SCROLL3, 0x00000, 0x1ffff, 0 },
    { 0                                                                                    ,       0,       0, 0 }
};

static const struct GfxRange mapper_ST24M1_table[] =
{
    { GFXTYPE_STARS,   0x00000, 0x003ff, 0 },
    { GFXTYPE_SPRITES, 0x00000, 0x04fff, 0 },
    { GFXTYPE_SCROLL2, 0x04000, 0x07fff, 0 },
    { GFXTYPE_SCROLL3, 0x00000, 0x07fff, 1 },
    { GFXTYPE_SCROLL1, 0x07000, 0x07fff, 1 },
    { 0              ,       0,       0, 0  }
};

static const struct GfxRange mapper_TK22B_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x3fff, 0 },
    { GFXTYPE_SPRITES, 0x4000, 0x5fff, 1 },
    { GFXTYPE_SCROLL1, 0x6000, 0x7fff, 1 },
    { GFXTYPE_SCROLL3, 0x0000, 0x3fff, 2 },
    { GFXTYPE_SCROLL2, 0x4000, 0x7fff, 3 },
    { 0              ,      0,      0, 0 }
};

static const struct GfxRange mapper_WL24B_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x4fff, 0 },
    { GFXTYPE_SCROLL3, 0x5000, 0x6fff, 0 },
    { GFXTYPE_SCROLL1, 0x7000, 0x7fff, 0 },
    { GFXTYPE_SCROLL2, 0x0000, 0x3fff, 1 },
    { 0              ,      0,      0, 0 }
};

static const struct GfxRange mapper_S224B_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x43ff, 0 },
    { GFXTYPE_SCROLL1, 0x4400, 0x4bff, 0 },
    { GFXTYPE_SCROLL3, 0x4c00, 0x5fff, 0 },
    { GFXTYPE_SCROLL2, 0x6000, 0x7fff, 0 },
    { 0              ,      0,      0, 0 }
};

static const struct GfxRange mapper_YI24B_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x1fff, 0 },
    { GFXTYPE_SCROLL3, 0x2000, 0x3fff, 0 },
    { GFXTYPE_SCROLL1, 0x4000, 0x47ff, 0 },
    { GFXTYPE_SCROLL2, 0x4800, 0x7fff, 0 },
    { 0              ,      0,      0, 0 }
};

static const struct GfxRange mapper_AR24B_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x2fff, 0 },
    { GFXTYPE_SCROLL1, 0x3000, 0x3fff, 0 },
    { GFXTYPE_SCROLL2, 0x4000, 0x5fff, 0 },
    { GFXTYPE_SCROLL3, 0x6000, 0x7fff, 0 },
    { 0              ,      0,      0, 0 }
};

static const struct GfxRange mapper_AR22B_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x2fff, 0 },
    { GFXTYPE_SCROLL1, 0x3000, 0x3fff, 0 },
    { GFXTYPE_SCROLL2, 0x4000, 0x5fff, 1 },
    { GFXTYPE_SCROLL3, 0x6000, 0x7fff, 1 },
    { 0              ,      0,      0, 0 }
};

static const struct GfxRange mapper_O224B_table[] = {
    { GFXTYPE_SCROLL1, 0x0000, 0x0bff, 0 },
    { GFXTYPE_SCROLL2, 0x0c00, 0x3bff, 0 },
    { GFXTYPE_SCROLL3, 0x3c00, 0x4bff, 0 },
    { GFXTYPE_SPRITES, 0x4c00, 0x7fff, 0 },
    { GFXTYPE_SPRITES, 0x8000, 0xa7ff, 1 },
    { GFXTYPE_SCROLL2, 0xa800, 0xb7ff, 1 },
    { GFXTYPE_SCROLL3, 0xb800, 0xbfff, 1 },
    { 0              ,      0,      0, 0 }
};

static const struct GfxRange mapper_MS24B_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x3fff, 0 },
    { GFXTYPE_SCROLL1, 0x4000, 0x4fff, 0 },
    { GFXTYPE_SCROLL2, 0x5000, 0x6fff, 0 },
    { GFXTYPE_SCROLL3, 0x7000, 0x7fff, 0 },
    { 0              ,      0,      0, 0 }
};

static const struct GfxRange mapper_CK24B_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x2fff, 0 },
    { GFXTYPE_SCROLL1, 0x3000, 0x3fff, 0 },
    { GFXTYPE_SCROLL2, 0x4000, 0x6fff, 0 },
    { GFXTYPE_SCROLL3, 0x7000, 0x7fff, 0 },
    { 0              ,      0,      0, 0 }
};

static const struct GfxRange mapper_NM24B_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x3fff, 0 },
    { GFXTYPE_SCROLL2, 0x0000, 0x3fff, 0 },
    { GFXTYPE_SCROLL1, 0x4000, 0x47ff, 0 },
    { GFXTYPE_SPRITES, 0x4800, 0x67ff, 0 },
    { GFXTYPE_SCROLL2, 0x4800, 0x67ff, 0 },
    { GFXTYPE_SCROLL3, 0x6800, 0x7fff, 0 },
    { 0              ,      0,      0, 0 }
};

static const struct GfxRange mapper_CA24B_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x2fff, 0 },
    { GFXTYPE_SCROLL2, 0x0000, 0x2fff, 0 },
    { GFXTYPE_SCROLL3, 0x3000, 0x4fff, 0 },
    { GFXTYPE_SCROLL1, 0x5000, 0x57ff, 0 },
    { GFXTYPE_SPRITES, 0x5800, 0x7fff, 0 },
    { GFXTYPE_SCROLL2, 0x5800, 0x7fff, 0 },
    { 0              ,      0,      0, 0 }
};

static const struct GfxRange mapper_STF29_table[] = {
    { GFXTYPE_SPRITES, 0x00000, 0x07fff, 0 },
    { GFXTYPE_SPRITES, 0x08000, 0x0ffff, 1 },
    { GFXTYPE_SPRITES, 0x10000, 0x11fff, 2 },
    { GFXTYPE_SCROLL3, 0x02000, 0x03fff, 2 },
    { GFXTYPE_SCROLL1, 0x04000, 0x04fff, 2 },
    { GFXTYPE_SCROLL2, 0x05000, 0x07fff, 2 },
    { 0              ,       0,       0, 0 }
};

static const struct GfxRange mapper_RT24B_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x53ff, 0 },
    { GFXTYPE_SCROLL1, 0x5400, 0x6fff, 0 },
    { GFXTYPE_SCROLL3, 0x7000, 0x7fff, 0 },
    { GFXTYPE_SCROLL3, 0x0000, 0x3fff, 1 },
    { GFXTYPE_SCROLL2, 0x2800, 0x7fff, 1 },
    { GFXTYPE_SPRITES, 0x5400, 0x7fff, 1 },
    { 0              ,      0,      0, 0 }
};

static const struct GfxRange mapper_KD29B_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x7fff, 0 },
    { GFXTYPE_SPRITES, 0x8000, 0x8fff, 1 },
    { GFXTYPE_SCROLL2, 0x9000, 0xbfff, 1 },
    { GFXTYPE_SCROLL1, 0xc000, 0xd7ff, 1 },
    { GFXTYPE_SCROLL3, 0xd800, 0xffff, 1 },
    { 0              ,      0,      0, 0 }
};

static const struct GfxRange mapper_CC63B_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x7fff, 0 },
    { GFXTYPE_SCROLL2, 0x0000, 0x7fff, 0 },
    { GFXTYPE_SPRITES, 0x8000, 0xffff, 1 },
    { GFXTYPE_SCROLL1, 0x8000, 0xffff, 1 },
    { GFXTYPE_SCROLL2, 0x8000, 0xffff, 1 },
    { GFXTYPE_SCROLL3, 0x8000, 0xffff, 1 },
    { 0              ,      0,      0, 0 }
};

static const struct GfxRange mapper_KR63B_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x7fff, 0 },
    { GFXTYPE_SCROLL2, 0x0000, 0x7fff, 0 },
    { GFXTYPE_SCROLL1, 0x8000, 0x9fff, 1 },
    { GFXTYPE_SPRITES, 0x8000, 0xcfff, 1 },
    { GFXTYPE_SCROLL2, 0x8000, 0xcfff, 1 },
    { GFXTYPE_SCROLL3, 0xd000, 0xffff, 1 },
    { 0              ,      0,      0, 0 }
};

static const struct GfxRange mapper_S9263B_table[] = {
    { GFXTYPE_SPRITES, 0x00000, 0x07fff, 0 },
    { GFXTYPE_SPRITES, 0x08000, 0x0ffff, 1 },
    { GFXTYPE_SPRITES, 0x10000, 0x11fff, 2 },
    { GFXTYPE_SCROLL3, 0x02000, 0x03fff, 2 },
    { GFXTYPE_SCROLL1, 0x04000, 0x04fff, 2 },
    { GFXTYPE_SCROLL2, 0x05000, 0x07fff, 2 },
    { 0              ,       0,       0, 0 }
};

static const struct GfxRange mapper_VA63B_table[] = {
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL1 | GFXTYPE_SCROLL2 | GFXTYPE_SCROLL3, 0x00000, 0x07fff, 0 },
    { 0              , 0,      0     , 0 }
};

static const struct GfxRange mapper_Q522B_table[] = {
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL2, 0x0000, 0x6fff, 0 },
    { GFXTYPE_SCROLL3,                   0x7000, 0x77ff, 0 },
    { GFXTYPE_SCROLL1,                   0x7800, 0x7fff, 0 },
    { 0                                ,      0,      0, 0 }
};

static const struct GfxRange mapper_TK263B_table[] = {
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL1 | GFXTYPE_SCROLL2 | GFXTYPE_SCROLL3, 0x00000, 0x07fff, 0 },
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL1 | GFXTYPE_SCROLL2 | GFXTYPE_SCROLL3, 0x08000, 0x0ffff, 1 },
    { 0                                ,      0,      0, 0 }
};

static const struct GfxRange mapper_CD63B_table[] = {
    { GFXTYPE_SCROLL1,                   0x0000, 0x0fff, 0 },
    { GFXTYPE_SPRITES,                   0x1000, 0x7fff, 0 },
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL2, 0x8000, 0xdfff, 1 },
    { GFXTYPE_SCROLL3,                   0xe000, 0xffff, 1 },
    { 0                                ,      0,      0, 0 }
};

static const struct GfxRange mapper_PS63B_table[] = {
    { GFXTYPE_SCROLL1,                   0x0000, 0x0fff, 0 },
    { GFXTYPE_SPRITES,                   0x1000, 0x7fff, 0 },
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL2, 0x8000, 0xdbff, 1 },
    { GFXTYPE_SCROLL3,                   0xdc00, 0xffff, 1 },
    { 0                                ,      0,      0, 0 }
};

static const struct GfxRange mapper_MB63B_table[] = {
    { GFXTYPE_SCROLL1,                   0x00000, 0x00fff, 0 },
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL2, 0x01000, 0x07fff, 0 },
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL2, 0x08000, 0x0ffff, 1 },
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL2, 0x10000, 0x167ff, 2 },
    { GFXTYPE_SCROLL3,                   0x16800, 0x17fff, 2 },
    { 0                                 ,      0,       0, 0 }
};

static const struct GfxRange mapper_QD22B_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x3fff, 0 },
    { GFXTYPE_SCROLL1, 0x0000, 0x3fff, 0 },
    { GFXTYPE_SCROLL2, 0x0000, 0x3fff, 0 },
    { GFXTYPE_SCROLL3, 0x0000, 0x3fff, 0 },
    { 0              ,      0,      0, 0 }
};

static const struct GfxRange mapper_TN2292_table[] = {
    { GFXTYPE_SCROLL1,                   0x0000, 0x0fff, 0 },
    { GFXTYPE_SCROLL3,                   0x1000, 0x3fff, 0 },
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL2, 0x4000, 0x7fff, 0 },
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL2, 0x8000, 0xffff, 1 },
    { 0                                ,      0,      0, 0 }
};

static const struct GfxRange mapper_RCM63B_table[] = {
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL1 | GFXTYPE_SCROLL2 | GFXTYPE_SCROLL3, 0x00000, 0x07fff, 0 },
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL1 | GFXTYPE_SCROLL2 | GFXTYPE_SCROLL3, 0x08000, 0x0ffff, 1 },
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL1 | GFXTYPE_SCROLL2 | GFXTYPE_SCROLL3, 0x10000, 0x17fff, 2 },
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL1 | GFXTYPE_SCROLL2 | GFXTYPE_SCROLL3, 0x18000, 0x1ffff, 3 },
    { 0                                  ,      0,      0, 0 }
};

static const struct GfxRange mapper_PKB10B_table[] = {
    { GFXTYPE_SCROLL1,                   0x0000, 0x0fff, 0 },
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL2, 0x1000, 0x5fff, 0 },
    { GFXTYPE_SCROLL3,                   0x6000, 0x7fff, 0 },
    { 0                                ,      0,      0, 0 }
};

static const struct GfxRange mapper_CP1B1F_table[] =
{
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL1 | GFXTYPE_SCROLL2 | GFXTYPE_SCROLL3, 0x0000, 0xffff, 0 },
    { 0                                                                    ,      0,      0, 0 }
};

static const struct GfxRange mapper_pokonyan_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x2fff, 0 },
    { GFXTYPE_SCROLL1, 0x7000, 0x7fff, 0 },
    { GFXTYPE_SCROLL3, 0x3000, 0x3fff, 0 },
    { GFXTYPE_SCROLL2, 0x4000, 0x6fff, 0 },
    { 0              ,      0,      0, 0 }
};

static const struct GfxRange mapper_gulun_table[] = {
    { GFXTYPE_SPRITES, 0x6000, 0x7fff, 0 },
    { GFXTYPE_SCROLL1, 0x0000, 0x7fff, 0 },
    { GFXTYPE_SCROLL3, 0x4000, 0x5fff, 0 },
    { GFXTYPE_SCROLL2, 0x2000, 0x3fff, 0 },
    { 0              ,      0,      0, 0 }
};

static const struct GfxRange mapper_sfzch_table[] = {
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL1 | GFXTYPE_SCROLL2 | GFXTYPE_SCROLL3, 0x00000, 0x1ffff, 0 },
    { 0                                                                    ,       0,       0, 0 }
};

BoardConfig GameDatabase::getBoardConfig(CPSBoard board) {
    BoardConfig config = {};
    
    switch (board) {
        case CPSBoard::CPS_B_01: {
            config.boardIdOffset = 0;
            config.boardIdValue1 = 0;
            config.boardIdValue2 = 0;
            config.layerControlReg = 0x66;
            config.paletteControlReg = 0x70;
            config.maskAddr[0] = 0x68;
            config.maskAddr[1] = 0x6a;
            config.maskAddr[2] = 0x6c;
            config.maskAddr[3] = 0x6e;
            config.layerEnable[1] = 0x02;
            config.layerEnable[2] = 0x04;
            config.layerEnable[3] = 0x08;
            config.layerEnable[4] = 0x30;
            config.layerEnable[5] = 0x30;
            break;
        }
        case CPSBoard::CPS_B_02: {
            config.boardIdOffset = 0x60;
            config.boardIdValue1 = 0x00;
            config.boardIdValue2 = 0x02;
            config.layerControlReg = 0x6c;
            config.paletteControlReg = 0x62;
            config.maskAddr[0] = 0x6a;
            config.maskAddr[1] = 0x68;
            config.maskAddr[2] = 0x66;
            config.maskAddr[3] = 0x64;
            config.layerEnable[1] = 0x02;
            config.layerEnable[2] = 0x04;
            config.layerEnable[3] = 0x08;
            config.layerEnable[4] = 0x00;
            config.layerEnable[5] = 0x00;
            break;
        }
        case CPSBoard::CPS_B_03: {
            config.boardIdOffset = 0x00;
            config.boardIdValue1 = 0x00;
            config.boardIdValue2 = 0x00;
            config.layerControlReg = 0x70;
            config.paletteControlReg = 0x66;
            config.maskAddr[0] = 0x6e;
            config.maskAddr[1] = 0x6c;
            config.maskAddr[2] = 0x6a;
            config.maskAddr[3] = 0x68;
            config.layerEnable[1] = 0x02;
            config.layerEnable[2] = 0x10;
            config.layerEnable[3] = 0x08;
            config.layerEnable[4] = 0x00;
            config.layerEnable[5] = 0x00;
            break;
        }
        case CPSBoard::CPS_B_04: {
            config.boardIdOffset = 0x60;
            config.boardIdValue1 = 0x00;
            config.boardIdValue2 = 0x04;
            config.layerControlReg = 0x6e;
            config.paletteControlReg = 0x6a;
            config.maskAddr[0] = 0x66;
            config.maskAddr[1] = 0x70;
            config.maskAddr[2] = 0x68;
            config.maskAddr[3] = 0x72;
            config.layerEnable[1] = 0x02;
            config.layerEnable[2] = 0x04;
            config.layerEnable[3] = 0x08;
            config.layerEnable[4] = 0x00;
            config.layerEnable[5] = 0x00;
            break;
        }
        case CPSBoard::CPS_B_05: {
            config.boardIdOffset = 0x60;
            config.boardIdValue1 = 0x00;
            config.boardIdValue2 = 0x05;
            config.layerControlReg = 0x68;
            config.paletteControlReg = 0x72;
            config.maskAddr[0] = 0x6a;
            config.maskAddr[1] = 0x6c;
            config.maskAddr[2] = 0x6e;
            config.maskAddr[3] = 0x70;
            config.layerEnable[1] = 0x02;
            config.layerEnable[2] = 0x08;
            config.layerEnable[3] = 0x20;
            config.layerEnable[4] = 0x14;
            config.layerEnable[5] = 0x14;
            break;
        }
        case CPSBoard::CPS_B_11: {
            config.boardIdOffset = 0x72;
            config.boardIdValue1 = 0x04;
            config.boardIdValue2 = 0x01;
            config.layerControlReg = 0x66;
            config.paletteControlReg = 0x70;
            config.maskAddr[0] = 0x68;
            config.maskAddr[1] = 0x6a;
            config.maskAddr[2] = 0x6c;
            config.maskAddr[3] = 0x6e;
            config.layerEnable[1] = 0x08;
            config.layerEnable[2] = 0x10;
            config.layerEnable[3] = 0x20;
            config.layerEnable[4] = 0x00;
            config.layerEnable[5] = 0x00;
            break;
        }
        case CPSBoard::CPS_B_12: {
            config.boardIdOffset = 0x60;
            config.boardIdValue1 = 0x04;
            config.boardIdValue2 = 0x02;
            config.layerControlReg = 0x6c;
            config.paletteControlReg = 0x62;
            config.maskAddr[0] = 0x6a;
            config.maskAddr[1] = 0x68;
            config.maskAddr[2] = 0x66;
            config.maskAddr[3] = 0x64;
            config.layerEnable[1] = 0x02;
            config.layerEnable[2] = 0x04;
            config.layerEnable[3] = 0x08;
            config.layerEnable[4] = 0x00;
            config.layerEnable[5] = 0x00;
            break;
        }
        case CPSBoard::CPS_B_13: {
            config.boardIdOffset = 0x6e;
            config.boardIdValue1 = 0x04;
            config.boardIdValue2 = 0x03;
            config.layerControlReg = 0x62;
            config.paletteControlReg = 0x6c;
            config.maskAddr[0] = 0x64;
            config.maskAddr[1] = 0x66;
            config.maskAddr[2] = 0x68;
            config.maskAddr[3] = 0x6e;
            config.layerEnable[1] = 0x20;
            config.layerEnable[2] = 0x02;
            config.layerEnable[3] = 0x04;
            config.layerEnable[4] = 0x00;
            config.layerEnable[5] = 0x00;
            break;
        }
        case CPSBoard::CPS_B_14: {
            config.boardIdOffset = 0x5e;
            config.boardIdValue1 = 0x04;
            config.boardIdValue2 = 0x04;
            config.layerControlReg = 0x52;
            config.paletteControlReg = 0x5c;
            config.maskAddr[0] = 0x54;
            config.maskAddr[1] = 0x56;
            config.maskAddr[2] = 0x58;
            config.maskAddr[3] = 0x5a;
            config.layerEnable[1] = 0x08;
            config.layerEnable[2] = 0x20;
            config.layerEnable[3] = 0x10;
            config.layerEnable[4] = 0x00;
            config.layerEnable[5] = 0x00;
            break;
        }
        case CPSBoard::CPS_B_15: {
            config.boardIdOffset = 0x4e;
            config.boardIdValue1 = 0x04;
            config.boardIdValue2 = 0x05;
            config.layerControlReg = 0x42;
            config.paletteControlReg = 0x4c;
            config.maskAddr[0] = 0x44;
            config.maskAddr[1] = 0x46;
            config.maskAddr[2] = 0x48;
            config.maskAddr[3] = 0x4a;
            config.layerEnable[1] = 0x04;
            config.layerEnable[2] = 0x02;
            config.layerEnable[3] = 0x20;
            config.layerEnable[4] = 0x00;
            config.layerEnable[5] = 0x00;
            break;
        }
        case CPSBoard::CPS_B_16: {
            config.boardIdOffset = 0x40;
            config.boardIdValue1 = 0x04;
            config.boardIdValue2 = 0x06;
            config.layerControlReg = 0x4c;
            config.paletteControlReg = 0x42;
            config.maskAddr[0] = 0x4a;
            config.maskAddr[1] = 0x48;
            config.maskAddr[2] = 0x46;
            config.maskAddr[3] = 0x44;
            config.layerEnable[1] = 0x10;
            config.layerEnable[2] = 0x0a;
            config.layerEnable[3] = 0x0a;
            config.layerEnable[4] = 0x00;
            config.layerEnable[5] = 0x00;
            break;
        }
        case CPSBoard::CPS_B_21_BT1: {
            config.boardIdOffset = 0x72;
            config.boardIdValue1 = 0x08;
            config.boardIdValue2 = 0x00;
            config.memProt[0] = 0x4e;
            config.memProt[1] = 0x4c;
            config.memProt[2] = 0x4a;
            config.memProt[3] = 0x48;
            config.layerControlReg = 0x68;
            config.paletteControlReg = 0x70;
            config.maskAddr[0] = 0x66;
            config.maskAddr[1] = 0x64;
            config.maskAddr[2] = 0x62;
            config.maskAddr[3] = 0x60;
            config.layerEnable[1] = 0x20;
            config.layerEnable[2] = 0x04;
            config.layerEnable[3] = 0x08;
            config.layerEnable[4] = 0x12;
            config.layerEnable[5] = 0x12;
            break;
        }
        case CPSBoard::CPS_B_21_BT2: {
            config.boardIdOffset = 0x00;
            config.boardIdValue1 = 0x00;
            config.boardIdValue2 = 0x00;
            config.memProt[0] = 0x5e;
            config.memProt[1] = 0x5c;
            config.memProt[2] = 0x5a;
            config.memProt[3] = 0x58;
            config.layerControlReg = 0x60;
            config.paletteControlReg = 0x70;
            config.maskAddr[0] = 0x6e;
            config.maskAddr[1] = 0x6c;
            config.maskAddr[2] = 0x6a;
            config.maskAddr[3] = 0x68;
            config.layerEnable[1] = 0x30;
            config.layerEnable[2] = 0x08;
            config.layerEnable[3] = 0x30;
            config.layerEnable[4] = 0x00;
            config.layerEnable[5] = 0x00;
            break;
        }
        case CPSBoard::CPS_B_21_BT3: {
            config.boardIdOffset = 0x00;
            config.boardIdValue1 = 0x00;
            config.boardIdValue2 = 0x00;
            config.memProt[0] = 0x46;
            config.memProt[1] = 0x44;
            config.memProt[2] = 0x42;
            config.memProt[3] = 0x40;
            config.layerControlReg = 0x60;
            config.paletteControlReg = 0x70;
            config.maskAddr[0] = 0x6e;
            config.maskAddr[1] = 0x6c;
            config.maskAddr[2] = 0x6a;
            config.maskAddr[3] = 0x68;
            config.layerEnable[1] = 0x20;
            config.layerEnable[2] = 0x12;
            config.layerEnable[3] = 0x12;
            config.layerEnable[4] = 0x00;
            config.layerEnable[5] = 0x00;
            break;	
        }
        case CPSBoard::CPS_B_21_BT4: {
            config.boardIdOffset = 0x00;
            config.boardIdValue1 = 0x00;
            config.boardIdValue2 = 0x00;
            config.memProt[0] = 0x46;
            config.memProt[1] = 0x44;
            config.memProt[2] = 0x42;
            config.memProt[3] = 0x40;
            config.layerControlReg = 0x68;
            config.paletteControlReg = 0x70;
            config.maskAddr[0] = 0x66;
            config.maskAddr[1] = 0x64;
            config.maskAddr[2] = 0x62;
            config.maskAddr[3] = 0x60;
            config.layerEnable[1] = 0x20;
            config.layerEnable[2] = 0x10;
            config.layerEnable[3] = 0x02;
            config.layerEnable[4] = 0x00;
            config.layerEnable[5] = 0x00;
            break;
        }
        case CPSBoard::CPS_B_21_BT6: {
            config.boardIdOffset = 0x00;
            config.boardIdValue1 = 0x00;
            config.boardIdValue2 = 0x00;
            config.layerControlReg = 0x60;
            config.paletteControlReg = 0x70;
            config.maskAddr[0] = 0x6e;
            config.maskAddr[1] = 0x6c;
            config.maskAddr[2] = 0x6a;
            config.maskAddr[3] = 0x68;
            config.layerEnable[1] = 0x20;
            config.layerEnable[2] = 0x14;
            config.layerEnable[3] = 0x14;
            config.layerEnable[4] = 0x00;
            config.layerEnable[5] = 0x00;
            break;
        }
        case CPSBoard::CPS_B_21_BT7: {
            config.boardIdOffset = 0x00;
            config.boardIdValue1 = 0x00;
            config.boardIdValue2 = 0x00;
            config.layerControlReg = 0x6c;
            config.paletteControlReg = 0x52;
            config.maskAddr[0] = 0x00;
            config.maskAddr[1] = 0x00;
            config.maskAddr[2] = 0x00;
            config.maskAddr[3] = 0x00;
            config.layerEnable[1] = 0x14;
            config.layerEnable[2] = 0x02;
            config.layerEnable[3] = 0x14;
            config.layerEnable[4] = 0x00;
            config.layerEnable[5] = 0x00;
            break;
        }
        case CPSBoard::CPS_B_21_DEF: {
            config.boardIdOffset = 0x32;
            config.boardIdValue1 = 0x00;
            config.boardIdValue2 = 0x00;
            config.memProt[0] = 0x40;
            config.memProt[1] = 0x42;
            config.memProt[2] = 0x44;
            config.memProt[3] = 0x46;
            config.layerControlReg = 0x66;
            config.paletteControlReg = 0x70;
            config.maskAddr[0] = 0x68;
            config.maskAddr[1] = 0x6a;
            config.maskAddr[2] = 0x6c;
            config.maskAddr[3] = 0x6e;
            config.layerEnable[1] = 0x02;
            config.layerEnable[2] = 0x04;
            config.layerEnable[3] = 0x08;
            config.layerEnable[4] = 0x30;
            config.layerEnable[5] = 0x30;
            break;
        }
        case CPSBoard::CPS_B_21_QS1: {
            config.boardIdOffset = 0x00;
            config.boardIdValue1 = 0x00;
            config.boardIdValue2 = 0x00;
            config.layerControlReg = 0x62;
            config.paletteControlReg = 0x6c;
            config.maskAddr[0] = 0x64;
            config.maskAddr[1] = 0x66;
            config.maskAddr[2] = 0x68;
            config.maskAddr[3] = 0x6a;
            config.layerEnable[1] = 0x10;
            config.layerEnable[2] = 0x08;
            config.layerEnable[3] = 0x04;
            config.layerEnable[4] = 0x00;
            config.layerEnable[5] = 0x00;
            break;
        }
        case CPSBoard::CPS_B_21_QS2: {
            config.boardIdOffset = 0x00;
            config.boardIdValue1 = 0x00;
            config.boardIdValue2 = 0x00;
            config.layerControlReg = 0x4a;
            config.paletteControlReg = 0x44;
            config.maskAddr[0] = 0x4c;
            config.maskAddr[1] = 0x4e;
            config.maskAddr[2] = 0x40;
            config.maskAddr[3] = 0x42;
            config.layerEnable[1] = 0x16;
            config.layerEnable[2] = 0x16;
            config.layerEnable[3] = 0x16;
            config.layerEnable[4] = 0x00;
            config.layerEnable[5] = 0x00;
            break;
        }
        case CPSBoard::CPS_B_21_QS3: {
            config.boardIdOffset = 0x4e;
            config.boardIdValue1 = 0x0c;
            config.boardIdValue2 = 0x00;
            config.layerControlReg = 0x52;
            config.paletteControlReg = 0x4c;
            config.maskAddr[0] = 0x54;
            config.maskAddr[1] = 0x56;
            config.maskAddr[2] = 0x48;
            config.maskAddr[3] = 0x4a;
            config.layerEnable[1] = 0x04;
            config.layerEnable[2] = 0x02;
            config.layerEnable[3] = 0x20;
            config.layerEnable[4] = 0x00;
            config.layerEnable[5] = 0x00;
            break;
        }
        case CPSBoard::CPS_B_21_QS4: {
            config.boardIdOffset = 0x6e;
            config.boardIdValue1 = 0x0c;
            config.boardIdValue2 = 0x01;
            config.layerControlReg = 0x56;
            config.paletteControlReg = 0x6c;
            config.maskAddr[0] = 0x40;
            config.maskAddr[1] = 0x42;
            config.maskAddr[2] = 0x68;
            config.maskAddr[3] = 0x6a;
            config.layerEnable[1] = 0x04;
            config.layerEnable[2] = 0x08;
            config.layerEnable[3] = 0x10;
            config.layerEnable[4] = 0x00;
            config.layerEnable[5] = 0x00;
            break;
        }
        case CPSBoard::CPS_B_21_QS5: {
            config.boardIdOffset = 0x5e;
            config.boardIdValue1 = 0x0c;
            config.boardIdValue2 = 0x02;
            config.layerControlReg = 0x6a;
            config.paletteControlReg = 0x5c;
            config.maskAddr[0] = 0x6c;
            config.maskAddr[1] = 0x6e;
            config.maskAddr[2] = 0x70;
            config.maskAddr[3] = 0x72;
            config.layerEnable[1] = 0x04;
            config.layerEnable[2] = 0x08;
            config.layerEnable[3] = 0x10;
            config.layerEnable[4] = 0x00;
            config.layerEnable[5] = 0x00;
            break;
        }
        default:
            throw std::runtime_error("Unsupported board type");
    }
    
    return config;
}

const GameInfo* GameDatabase::findGame(const std::string& romSetName) {
    // Convert to lowercase for case-insensitive matching
    std::string lower = romSetName;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    for (u32 i = 0; i < s_cps1_gameCount; i++) {
        std::string gameName = s_cps1_games[i].romSetName;
        std::transform(gameName.begin(), gameName.end(), gameName.begin(), ::tolower);
        
        if (lower == gameName) {
            return &s_cps1_games[i];
        }
    }
    
    for (u32 i = 0; i < s_cps2_gameCount; i++) {
        std::string gameName = s_cps2_games[i].romSetName;
        std::transform(gameName.begin(), gameName.end(), gameName.begin(), ::tolower);
        
        if (lower == gameName) {
            return &s_cps2_games[i];
        }
    }
    
    // No game found
    return nullptr;
}

u32 GameDatabase::calculateCRC32(const std::vector<u8>& data) {
    return static_cast<u32>(mz_crc32(0, data.data(), static_cast<size_t>(data.size())));
}

bool GameDatabase::validateROM(const std::string& filename, const std::vector<u8>& data, const ROMEntry& entry) {
    // Check filename (case-insensitive)
    std::string lowerFilename = filename;
    std::transform(lowerFilename.begin(), lowerFilename.end(), lowerFilename.begin(), ::tolower);
    std::string lowerEntry = entry.filename;
    std::transform(lowerEntry.begin(), lowerEntry.end(), lowerEntry.begin(), ::tolower);
    
    if (lowerFilename != lowerEntry) {
        return false;
    }
    
    // Check size
    if (data.size() != entry.size) {
        return false;
    }
    
    // Check CRC32 (skip for optional ROMs and encryption keys)
    if (!(entry.flags & ROM_FLAG_OPTIONAL) && entry.type != ROMType::ENCRYPTION_KEY) {
        u32 calculatedCRC = calculateCRC32(data);
        if (calculatedCRC != entry.crc32) {
            return false;
        }
    }
    
    return true;
}
void GameDatabase::getGraphicsMapper(CPSMapper mapper, u32 sizes[4], const GfxRange*& mapperTable) {
    switch (mapper) {
        case CPSMapper::MAPPER_LWCHR: {
            sizes[0] = 0x8000;
            sizes[1] = 0x8000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_LWCHR_table;
            break;
        }
        case CPSMapper::MAPPER_LW621: {
            sizes[0] = 0x8000;
            sizes[1] = 0x8000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_LW621_table;
            break;
        }
        case CPSMapper::MAPPER_DM620: {
            sizes[0] = 0x8000;
            sizes[1] = 0x2000;
            sizes[2] = 0x2000;
            sizes[3] = 0x0000;
            mapperTable = mapper_DM620_table;
            break;
        }
        case CPSMapper::MAPPER_ST24M1: {
            sizes[0] = 0x8000;
            sizes[1] = 0x8000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_ST24M1_table;
            break;
        }
        case CPSMapper::MAPPER_TK22B: {
            sizes[0] = 0x4000;
            sizes[1] = 0x4000;
            sizes[2] = 0x4000;
            sizes[3] = 0x4000;
            mapperTable = mapper_TK22B_table;
            break;
        }
        case CPSMapper::MAPPER_WL24B: {
            sizes[0] = 0x8000;
            sizes[1] = 0x8000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_WL24B_table;
            break;
        }
        case CPSMapper::MAPPER_S224B: {
            sizes[0] = 0x8000;
            sizes[1] = 0x0000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_S224B_table;
            break;
        }
        case CPSMapper::MAPPER_YI24B: {
            sizes[0] = 0x8000;
            sizes[1] = 0x0000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_YI24B_table;
            break;
        }
        case CPSMapper::MAPPER_AR24B: {
            sizes[0] = 0x8000;
            sizes[1] = 0x0000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_AR24B_table;
            break;
        }
        case CPSMapper::MAPPER_AR22B: {
            sizes[0] = 0x4000;
            sizes[1] = 0x4000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_AR22B_table;
            break;
        }
        case CPSMapper::MAPPER_O224B: {
            sizes[0] = 0x8000;
            sizes[1] = 0x4000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_O224B_table;
            break;
        }
        case CPSMapper::MAPPER_MS24B: {
            sizes[0] = 0x8000;
            sizes[1] = 0x0000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_MS24B_table;
            break;
        }
        case CPSMapper::MAPPER_CK24B: {
            sizes[0] = 0x8000;
            sizes[1] = 0x0000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_CK24B_table;
            break;
        }
        case CPSMapper::MAPPER_NM24B: {
            sizes[0] = 0x8000;
            sizes[1] = 0x0000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_NM24B_table;
            break;
        }
        case CPSMapper::MAPPER_CA24B: {
            sizes[0] = 0x8000;
            sizes[1] = 0x0000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_CA24B_table;
            break;
        }
        case CPSMapper::MAPPER_STF29: {
            sizes[0] = 0x08000;
            sizes[1] = 0x08000;
            sizes[2] = 0x08000;
            sizes[3] = 0x00000;
            mapperTable = mapper_STF29_table;
            break;
        }
        case CPSMapper::MAPPER_RT24B: {
            sizes[0] = 0x8000;
            sizes[1] = 0x8000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_RT24B_table;
            break;
        }
        case CPSMapper::MAPPER_KD29B: {
            sizes[0] = 0x8000;
            sizes[1] = 0x8000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_KD29B_table;
            break;
        }
        case CPSMapper::MAPPER_CC63B: {
            sizes[0] = 0x8000;
            sizes[1] = 0x8000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_CC63B_table;
            break;
        }
        case CPSMapper::MAPPER_KR63B: {
            sizes[0] = 0x8000;
            sizes[1] = 0x8000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_KR63B_table;
            break;
        }
        case CPSMapper::MAPPER_S9263B: {
            sizes[0] = 0x08000;
            sizes[1] = 0x08000;
            sizes[2] = 0x08000;
            sizes[3] = 0x00000;
            mapperTable = mapper_S9263B_table;
            break;
        }
        case CPSMapper::MAPPER_VA63B: {
            sizes[0] = 0x8000;
            sizes[1] = 0x0000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_VA63B_table;
            break;
        }
        case CPSMapper::MAPPER_Q522B: {
            sizes[0] = 0x8000;
            sizes[1] = 0x0000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_Q522B_table;
            break;
        }
        case CPSMapper::MAPPER_TK263B: {
            sizes[0] = 0x8000;
            sizes[1] = 0x8000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_TK263B_table;
            break;
        }
        case CPSMapper::MAPPER_CD63B: {
            sizes[0] = 0x8000;
            sizes[1] = 0x8000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_CD63B_table;
            break;
        }
        case CPSMapper::MAPPER_PS63B: {
            sizes[0] = 0x8000;
            sizes[1] = 0x8000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_PS63B_table;
            break;
        }
        case CPSMapper::MAPPER_MB63B: {
            sizes[0] = 0x08000;
            sizes[1] = 0x08000;
            sizes[2] = 0x08000;
            sizes[3] = 0x00000;
            mapperTable = mapper_MB63B_table;
            break;
        }
        case CPSMapper::MAPPER_QD22B: {
            sizes[0] = 0x4000;
            sizes[1] = 0x0000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_QD22B_table;
            break;
        }
        case CPSMapper::MAPPER_TN2292: {
            sizes[0] = 0x8000;
            sizes[1] = 0x8000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_TN2292_table;
            break;
        }
        case CPSMapper::MAPPER_RCM63B: {
            sizes[0] = 0x8000;
            sizes[1] = 0x8000;
            sizes[2] = 0x8000;
            sizes[3] = 0x8000;
            mapperTable = mapper_RCM63B_table;
            break;
        }
        case CPSMapper::MAPPER_PKB10B: {
            sizes[0] = 0x8000;
            sizes[1] = 0x0000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_PKB10B_table;
            break;
        }
        case CPSMapper::MAPPER_CP1B1F: {
            sizes[0] = 0x10000;
            sizes[1] = 0x00000;
            sizes[2] = 0x00000;
            sizes[3] = 0x00000;
            mapperTable = mapper_CP1B1F_table;
            break;
        }
        case CPSMapper::MAPPER_POKONYAN: {
            sizes[0] = 0x8000;
            sizes[1] = 0x8000;
            sizes[2] = 0x8000;
            sizes[3] = 0x0000;
            mapperTable = mapper_pokonyan_table;
            break;
        }
        case CPSMapper::MAPPER_GULUN: {
            sizes[0] = 0x8000;
            sizes[1] = 0x0000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            mapperTable = mapper_gulun_table;
            break;
        }
        case CPSMapper::MAPPER_SFZCH: {
            sizes[0] = 0x20000;
            sizes[1] = 0x00000;
            sizes[2] = 0x00000;
            sizes[3] = 0x00000;
            mapperTable = mapper_sfzch_table;
            break;
        }
        default:
            sizes[0] = 0;
            sizes[1] = 0;
            sizes[2] = 0;
            sizes[3] = 0;
            break;
    }
}

} // namespace cps
