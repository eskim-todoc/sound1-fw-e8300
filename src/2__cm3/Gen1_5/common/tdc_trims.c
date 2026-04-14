#include <tdc_trims.h>
// clang-format off
static MANU_TABLE_Type *tdc_manuTable = NULL;

static int tdc_Trims_FindTrim(uint32_t chess_storage(IOMEM) *manusection_ptr,
                              unsigned section_size, unsigned int target)
{
    uint32_t mask = 0xFFFF;
    if ((uint32_t chess_storage(IOMEM) *)manusection_ptr == (uint32_t chess_storage(IOMEM) *)&tdc_manuTable->MANU_CLK)
    {
        mask = 0xFFFFFFFF;
    }

    while (section_size > 0)
    {
        /* Check for trim in lower bitfield */
        if ((MANU_TABLE_ACCESS_TARGET(*manusection_ptr)) == target)
        {
            return (*manusection_ptr & mask);
        }

        /* If not using a 32-bit mask, check for trim in upper bitfield */
        if ((mask == 0xFFFF) && ((MANU_TABLE_ACCESS_TARGET((*manusection_ptr) >> 16)) == target))
        {
            return ((*manusection_ptr >> 16) & 0xFFFF);
        }
        manusection_ptr++;
        section_size--;
    }

    return (SYS_ERROR_SEARCH_FAILURE);
}

void tdc_Trims_LoadManuTable(uint32_t *p_manu_table)
{
    tdc_manuTable = (MANU_TABLE_Type *) p_manu_table;
}

unsigned int tdc_Trims_SetVREGAndLSAD()
{
    uint32_t chess_storage(IOMEM) *cal_ptr = (uint32_t chess_storage(IOMEM) *)&tdc_manuTable->MANU_VREG;

    /* Search for the trim */
    int result = tdc_Trims_FindTrim(cal_ptr, 1, VREG_0P9V);

    /* Return no match if the trim is not found. */
    if (result == SYS_ERROR_SEARCH_FAILURE)
    {
        return SYS_ERRNO_NO_MATCH;
    }

    /* Store the bandgap trim */
    ANALOG->BG_CTRL = (ANALOG->BG_CTRL & ~ANALOG_BG_CTRL_BG_LEVEL_Mask) |
            ((MANU_TABLE_ACCESS_TRIM(result) << ANALOG_BG_CTRL_BG_LEVEL_Pos) &
             ANALOG_BG_CTRL_BG_LEVEL_Mask);

    /* If the VREG settings are there, assume the LSAD settings are also
     * available (these don't have a target that can be searched for) */
    LSAD->GAIN_COMP = tdc_manuTable->MANU_LSAD_COMP.gain & LSAD_GAIN_COMP_GAIN_COMP_Mask;
    LSAD->OFFSET_COMP = tdc_manuTable->MANU_LSAD_COMP.offset & LSAD_OFFSET_COMP_OFFSET_COMP_Mask;

    return SYS_ERRNO_NO_ERROR;
}

unsigned int tdc_Trims_SetVDDIF(unsigned int target)
{
    uint32_t chess_storage(IOMEM) *cal_ptr = (uint32_t chess_storage(IOMEM) *)&tdc_manuTable->MANU_VDDIF[0];

    /* Search for the requested trim */
    int result = tdc_Trims_FindTrim(cal_ptr, (MANU_VDDIF_SIZE >> 1), target);

    /* Set the trim value to the appropriate register if it is found */
    if (result != SYS_ERROR_SEARCH_FAILURE)
    {
        ANALOG->CP_VDDIF_CTRL = (ANALOG->CP_VDDIF_CTRL & ~ANALOG_CP_VDDIF_CTRL_VTRIM_Mask) |
                                ((MANU_TABLE_ACCESS_TRIM(result) << ANALOG_CP_VDDIF_CTRL_VTRIM_Pos) &
                                 ANALOG_CP_VDDIF_CTRL_VTRIM_Mask);

        return SYS_ERRNO_NO_ERROR;
    }

    return SYS_ERRNO_NO_MATCH;
}

static unsigned int _tdc_Trims_SetVDDA(unsigned int target, unsigned int pmu_ref)
{
    uint32_t chess_storage(IOMEM) *cal_ptr = (pmu_ref == 0) ?
                                             (uint32_t chess_storage(IOMEM) *)&tdc_manuTable->MANU_VDDA[0] :
                                             (uint32_t chess_storage(IOMEM) *)&tdc_manuTable->MANU_VDDA_PMURef[0];

    /* Search for the requested trim */
    int result = tdc_Trims_FindTrim(cal_ptr, (MANU_VDDA_SIZE >> 1), target);

    /* Set the trim value to the appropriate register if it is found */
    if (result != SYS_ERROR_SEARCH_FAILURE)
    {
        ANALOG->CP_VDDA_CTRL = (ANALOG->CP_VDDA_CTRL & ~ANALOG_CP_VDDA_CTRL_VTRIM_Mask) |
                                ((MANU_TABLE_ACCESS_TRIM(result) << ANALOG_CP_VDDA_CTRL_VTRIM_Pos) &
                                ANALOG_CP_VDDA_CTRL_VTRIM_Mask);

        return SYS_ERRNO_NO_ERROR;
    }

    return SYS_ERRNO_NO_MATCH;
}

unsigned int tdc_Trims_SetVDDA(unsigned int target)
{
    return _tdc_Trims_SetVDDA(target, 0);
}

static unsigned int _tdc_Trims_SetVDDC(unsigned int target, unsigned int pmu_ref)
{
    uint32_t chess_storage(IOMEM) *cal_ptr = (pmu_ref == 0) ?
                                             (uint32_t chess_storage(IOMEM) *)&tdc_manuTable->MANU_VDDC[0] :
                                             (uint32_t chess_storage(IOMEM) *)&tdc_manuTable->MANU_VDDC_PMURef[0];

    /* Search for the requested trim */
    int result = tdc_Trims_FindTrim(cal_ptr, (MANU_VDDC_SIZE >> 1), target);

    /* Set the trim value to the appropriate register if it is found */
    if (result != SYS_ERROR_SEARCH_FAILURE)
    {
        ANALOG->CP_VDDC_TRIM = (ANALOG->CP_VDDC_TRIM & ~ANALOG_CP_VDDC_TRIM_LDO_TRIM_Mask) |
                               ((MANU_TABLE_ACCESS_TRIM(result) << ANALOG_CP_VDDC_TRIM_LDO_TRIM_Pos) &
                                ANALOG_CP_VDDC_TRIM_LDO_TRIM_Mask);

        return SYS_ERRNO_NO_ERROR;
    }

    return SYS_ERRNO_NO_MATCH;
}

unsigned int tdc_Trims_SetVDDC(unsigned int target)
{
    return _tdc_Trims_SetVDDC(target, 0);
}

unsigned int tdc_Trims_SetVDDC_CP(unsigned int target)
{
    uint32_t chess_storage(IOMEM) *cal_ptr = (uint32_t chess_storage(IOMEM) *)&tdc_manuTable->MANU_VDDC_CP;

    /* Search for the requested trim */
    int result = tdc_Trims_FindTrim(cal_ptr, 1, target);

    /* Set the trim value to the appropriate register if it is found */
    if (result != SYS_ERROR_SEARCH_FAILURE)
    {
        ANALOG->CP_VDDC_TRIM = (ANALOG->CP_VDDC_TRIM & ~ANALOG_CP_VDDC_TRIM_CP_DELTA_TRIM_Mask) |
                               ((MANU_TABLE_ACCESS_TRIM(result) << ANALOG_CP_VDDC_TRIM_CP_DELTA_TRIM_Pos) &
                                ANALOG_CP_VDDC_TRIM_CP_DELTA_TRIM_Mask);

        return SYS_ERRNO_NO_ERROR;
    }

    return SYS_ERRNO_NO_MATCH;
}

static unsigned int _tdc_Trims_SetVDDM(unsigned int target, unsigned int pmu_ref)
{
    uint32_t chess_storage(IOMEM) *cal_ptr = (pmu_ref == 0) ?
                                             (uint32_t chess_storage(IOMEM) *)&tdc_manuTable->MANU_VDDM[0] :
                                             (uint32_t chess_storage(IOMEM) *)&tdc_manuTable->MANU_VDDM_PMURef[0];

    /* Search for the requested trim */
    int result = tdc_Trims_FindTrim(cal_ptr, (MANU_VDDM_SIZE >> 1), target);

    /* Set the trim value to the appropriate register if it is found */
    if (result != SYS_ERROR_SEARCH_FAILURE)
    {
        ANALOG->CP_VDDM_TRIM = (ANALOG->CP_VDDM_TRIM & ~ANALOG_CP_VDDM_TRIM_LDO_TRIM_Mask) |
                               ((MANU_TABLE_ACCESS_TRIM(result) << ANALOG_CP_VDDM_TRIM_LDO_TRIM_Pos) &
                                ANALOG_CP_VDDM_TRIM_LDO_TRIM_Mask);

        return SYS_ERRNO_NO_ERROR;
    }

    return SYS_ERRNO_NO_MATCH;
}

unsigned int tdc_Trims_SetVDDM(unsigned int target)
{
    return _tdc_Trims_SetVDDM(target, 0);
}

unsigned int tdc_Trims_SetVDDM_CP(unsigned int target)
{
    uint32_t chess_storage(IOMEM) *cal_ptr = (uint32_t chess_storage(IOMEM) *)&tdc_manuTable->MANU_VDDM_CP;

    /* Search for the requested trim */
    int result = tdc_Trims_FindTrim(cal_ptr, 1, target);

    /* Set the trim value to the appropriate register if it is found */
    if (result != SYS_ERROR_SEARCH_FAILURE)
    {
        ANALOG->CP_VDDM_TRIM = (ANALOG->CP_VDDM_TRIM & ~ANALOG_CP_VDDM_TRIM_CP_DELTA_TRIM_Mask) |
                               ((MANU_TABLE_ACCESS_TRIM(result) << ANALOG_CP_VDDM_TRIM_CP_DELTA_TRIM_Pos) &
                                ANALOG_CP_VDDM_TRIM_CP_DELTA_TRIM_Mask);

        return SYS_ERRNO_NO_ERROR;
    }

    return SYS_ERRNO_NO_MATCH;
}

unsigned int tdc_Trims_SetVDDOD(unsigned int target)
{
    uint32_t chess_storage(IOMEM) *cal_ptr = (uint32_t chess_storage(IOMEM) *)&tdc_manuTable->MANU_VDDOD;

    /* Search for the requested trim */
    int result = tdc_Trims_FindTrim(cal_ptr, 1, target);

    /* Set the trim value to the appropriate register if it is found */
    if (result != SYS_ERROR_SEARCH_FAILURE)
    {
        ANALOG->VDDOD_CTRL = (ANALOG->VDDOD_CTRL & ~ANALOG_VDDOD_CTRL_VDDOD_REG_TRIM_Mask) |
                             ((MANU_TABLE_ACCESS_TRIM(result) << ANALOG_VDDOD_CTRL_VDDOD_REG_TRIM_Pos) &
                              ANALOG_VDDOD_CTRL_VDDOD_REG_TRIM_Mask);

        return SYS_ERRNO_NO_ERROR;
    }

    return SYS_ERRNO_NO_MATCH;
}

unsigned int tdc_Trims_SetVMIC(unsigned int target)
{
    uint32_t chess_storage(IOMEM) *cal_ptr = (uint32_t chess_storage(IOMEM) *)&tdc_manuTable->MANU_VMIC;

    /* Search for the requested trim */
    int result = tdc_Trims_FindTrim(cal_ptr, 1, target);

    /* Set the trim value to the appropriate register if it is found */
    if (result != SYS_ERROR_SEARCH_FAILURE)
    {
        ANALOG->VMIC_CTRL = (ANALOG->VMIC_CTRL & ~ANALOG_VMIC_CTRL_VMIC_REG_TRIM_Mask) |
                            ((MANU_TABLE_ACCESS_TRIM(result) << ANALOG_VMIC_CTRL_VMIC_REG_TRIM_Pos) &
                             ANALOG_VMIC_CTRL_VMIC_REG_TRIM_Mask);

        return SYS_ERRNO_NO_ERROR;
    }

    return SYS_ERRNO_NO_MATCH;
}

unsigned int tdc_Trims_SetOperatingFrequencyMult(unsigned int frequency_index, unsigned int multiplier)
{
    /* Given the frequency index, find the corresponding trim setting */
    uint32_t chess_storage(IOMEM) *cal_ptr = (uint32_t chess_storage(IOMEM) *)&tdc_manuTable->MANU_CLK;
    unsigned int result = tdc_Trims_FindTrim(cal_ptr, MANU_CLK_SIZE, frequency_index);

    if (result != SYS_ERROR_SEARCH_FAILURE)
    {
        /* Temporarily set the multiplier to 1 */
        ANALOG->OSC_CTRL_1 = (ANALOG->OSC_CTRL_1 & ~ANALOG_OSC_CTRL_1_MULT_Mask);

        ANALOG->OSC_CTRL_0 = MANU_TABLE_ACCESS_TRIM(result);

        ANALOG->OSC_CTRL_1 = (ANALOG->OSC_CTRL_1 & ~ANALOG_OSC_CTRL_1_MULT_Mask) |
                             (multiplier);

        SYSVAR_SET_FREQ(frequency_index);

        return SYS_ERRNO_NO_ERROR;
    }

    return SYS_ERRNO_NO_MATCH;
}

unsigned int tdc_Trims_SetOperatingFrequency(unsigned int frequency_index)
{
    /* Assume for any trim value over 30 MHz will be accounting for an
     * oscillator multiplier of 2, and multiplier of 4 if over 60 MHz */
    if (frequency_index > OSC_MUL_LIMIT2)
    {
        return tdc_Trims_SetOperatingFrequencyMult(frequency_index, OSC_MULTIPLY_BY_4);
    }
    else if (frequency_index > OSC_MUL_LIMIT1)
    {
        return tdc_Trims_SetOperatingFrequencyMult(frequency_index, OSC_MULTIPLY_BY_2);
    }
    else
    {
        return tdc_Trims_SetOperatingFrequencyMult(frequency_index, OSC_MULTIPLY_BY_1);
    }
}

unsigned int tdc_Trims_SetADCOffsets()
{
    volatile uint32_t chess_storage(IOMEM) *ADC_offset = (uint32_t chess_storage(IOMEM) *)(&(AUDIO->ADC_OFFSET_TRIM0[0]));
    uint32_t chess_storage(a0) reg_value;
    unsigned int i;

    for (i = 0; i < MANU_ADC_CHANNELS; i++)
    {
        /* Drive 2 & 4 offsets */
        reg_value = (uint32_t)(tdc_manuTable->MANU_ADC_TRIM0[i].upper << AUDIO_ADC_OFFSET_TRIM0_OFFSET_TRIM_DRIVE_4_Pos);
        reg_value |= (uint32_t)(tdc_manuTable->MANU_ADC_TRIM0[i].lower & AUDIO_ADC_OFFSET_TRIM0_OFFSET_TRIM_DRIVE_2_Mask);

        if ((reg_value == 0) || (reg_value >= 0x00FFFFFF))
        {
            return SYS_ERRNO_NO_MATCH;
        }
        *ADC_offset++ = reg_value;
    }

    for (i = 0; i < MANU_ADC_CHANNELS; i++)
    {
        /* Drive 8 & 16 offsets */
        reg_value = (uint32_t)(tdc_manuTable->MANU_ADC_TRIM1[i].upper << AUDIO_ADC_OFFSET_TRIM1_OFFSET_TRIM_DRIVE_16_Pos);
        reg_value |= (uint32_t)(tdc_manuTable->MANU_ADC_TRIM1[i].lower & AUDIO_ADC_OFFSET_TRIM1_OFFSET_TRIM_DRIVE_8_Mask);

        if ((reg_value == 0) || (reg_value == 0x00FFFFFF))
        {
            return SYS_ERRNO_NO_MATCH;
        }
        *ADC_offset++ = reg_value;
    }

    for (i = 0; i < MANU_ADC_CHANNELS; i++)
    {
        /* Drive 32 & 64 offsets */
        reg_value = (uint32_t)(tdc_manuTable->MANU_ADC_TRIM2[i].upper << AUDIO_ADC_OFFSET_TRIM2_OFFSET_TRIM_DRIVE_64_Pos);
        reg_value |= (uint32_t)(tdc_manuTable->MANU_ADC_TRIM2[i].lower & AUDIO_ADC_OFFSET_TRIM2_OFFSET_TRIM_DRIVE_32_Mask);

        if ((reg_value == 0) || (reg_value == 0x00FFFFFF))
        {
            return SYS_ERRNO_NO_MATCH;
        }
        *ADC_offset++ = reg_value;
    }

    return SYS_ERRNO_NO_ERROR;
}
// clang-format on


