/**
 * @file lib_pcm.c
 */

#include <lib_PCM.h>

void lib_init_PCM(uint32_t clk_dio, uint32_t frame_dio, uint32_t sero_dio)
{
    PCM->CTRL   = PCM_RESET;
    PCM->STATUS = (PCM_OVERRUN_CLEAR | PCM_UNDERRUN_CLEAR);

    /* Configure PCM DIO */
    // Sys_PCM_DIOConfig(PCM, PCM_SELECT_MASTER, LIB_PCM_DIO_CFG, clk_dio, frame_dio, seri_dio, sero_dio, DIO_MODE_USRCLK);
    LIB_PCM_DIOConfig(PCM, PCM_SELECT_MASTER, LIB_PCM_DIO_CFG, clk_dio, frame_dio, sero_dio, DIO_MODE_USRCLK);

    /* Configure PCM */
    Sys_PCM_Config(PCM, LIB_PCM_CFG);
}

void lib_enable_PCM(void)
{
    PCM->CTRL = PCM_ENABLE;
}

void lib_disable_PCM(void)
{
    PCM->CTRL = PCM_DISABLE;
}

void LIB_PCM_DIOConfig(const PCM_Type* pcm, uint32_t slave, uint32_t cfg, uint32_t clk, uint32_t frame, uint32_t sero, uint32_t clksrc)
{
    SYS_ASSERT(PCM_REF_VALID(pcm));
    unsigned int diff = (pcm - PCM);

    /* Configure PCM CLK pad configuration*/
    SYS_DIO_CONFIG(clk, (cfg | clksrc));

    /* Configure PCM SERO pad as output */
    SYS_DIO_CONFIG(sero, (cfg | (DIO_MODE_PCM0_SERO + (diff * PCM_PADS_NUM))));

    if (slave == PCM_SELECT_MASTER)
    {
        /* Configure PCM FRAME pad as output */
        SYS_DIO_CONFIG(frame, (cfg | (DIO_MODE_PCM0_FRAME + (diff * PCM_PADS_NUM))));
    }
    else
    {
        /* Configure PCM FRAME pad as input */
        SYS_DIO_CONFIG(frame, (cfg | DIO_MODE_INPUT));
    }

    DIO->SRC_PCM[diff] = ((PCM_SERI_SRC_CONST_HIGH & DIO_SRC_PCM_SERI_Mask) | ((frame << DIO_SRC_PCM_FRAME_Pos) & DIO_SRC_PCM_FRAME_Mask)
                          | ((clk << DIO_SRC_PCM_CLK_Pos) & DIO_SRC_PCM_CLK_Mask));
}
