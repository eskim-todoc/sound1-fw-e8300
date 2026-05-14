/**
 * @file system_control.c
 */

#include <system_control.h>

/* 내부기가 새로 연결되거나, 리모콘 앱을 통해 맵 번호가 변경되는 등의 이벤트 발생 시,
 * CM3가 공유 메모리에 매핑 데이터를 업데이트 한다.
 * 이후 CFX가 Normal_PowerMode_event_mapChange() 함수에서
 * 공유 메모리에 저장된 매핑 데이터를 아래의 변수들로 복사하며, CFX의 신호처리 과정에서 사용된다. */
int _XMEM addr_MapProgramData_mappingDate[6]               = {0};
int _XMEM addr_MapProgramData_stimulationStrategy          = 0;
int _XMEM addr_MapProgramData_firstPulsePhase              = 0;
int _XMEM addr_MapProgramData_StimulationMode              = 0;  // 0 : 모노폴라, 1 : 바이폴라, 2 : 공통 접지
int _XMEM addr_MapProgramData_StimulationPulsePhaseWidth   = 0;
int _XMEM addr_MapProgramData_FrequencyAnalysisBandNumbers = 0;
int _XMEM addr_MapProgramData_indicatorStimulCannel_index  = 0;
int _XMEM addr_MapProgramData_indicatorStimulLevel_uA      = 0;

int _XMEM addr_MapProgramData_StimulusChannelAssignedElectrodIndex[32]  = {0};  // 사용 가능한 자극 전극  번호 1 ~ 32
int _XMEM addr_MapProgramData_ReferenceChannelAssignedElectrodIndex[32] = {0};  // 사용 가능한 기준 전극  번호 1 ~ 32
int _XMEM addr_MapProgramData_CIS_FreqBandOrder[32]                     = {0};  // 자극 주파수 채널 순서       1 ~ 32

int _XMEM addr_MapProgramData_T_Level_uA[32] = {0};
int _XMEM addr_MapProgramData_C_Level_uA[32] = {0};

int _XMEM addr_MapProgramData_xMinLevel[32] = {0};
int _XMEM addr_MapProgramData_xMaxLevel[32] = {0};

/* CM3에서 채널별 C-T 값을 계산하고 공유 메모리에 저장하면,
 * CFX가 Normal_PowerMode_event_audioParameterCalculation() 함수에서
 * 공유 메모리에 저장된 채널별 C-T 값을 아래 배열로 복사한다.
 * 이후 Logaritm Mapping의 coeff 계산 및 stimulation level 계산에 사용한다. */
int _XMEM g_mapping_stimulus_amplitude_T_level[32] = {0};
int _XMEM g_mapping_stimulus_amplitude_C_level[32] = {0};

int _XMEM m_previous_audio_volume       = 1;
int _XMEM m_previous_stimulation_volume = 1;  // range : 1 ~ 10
int _XMEM m_current_isd                 = 0;
int _XMEM m_previous_isd                = 0;

void LB_Normal_PowerMode(void)
{
    m_current_isd = Addr_SharedMem->connected_ISD_num;

    /* If any ISD has connected, the number is definitely not zero */
    if (m_current_isd != 0)
    {
        Normal_PowerMode_isdConnected();
    }
    else
    {
        Normal_PowerMode_isdDisonnected();
    }
}

void Normal_PowerMode_event_mapChange(void)
{
    /* If the map(program) is changed, the CM3 will let the CFX know. */
    if (Addr_SharedMem->mapChangeFlag.cm3Command_mapChange == 1)
    {
        /* Stand alone mode 또는 Remote App에 의한 맵 변경일 경우 맵 번호가 0 이상임 */
        if (0 <= Addr_SharedMem->userSettingValue.mapNum)
        {
            fn_PcmBitStream_Mode_NopStandby(); /* Fill the PCM with NOP data before change map(program) */
            copy_MappingData();                /* For stand alone mode */
        }
        else /* Mapping App의 Live 기능 실행 중 맵 변경일 경우 맵 번호가 0 보다 작을 수 있음 */
        {
            fn_PcmBitStream_Mode_NopStandby();
            copy_MappingData_without_mappingDate(); /* Mapping app live mode */
        }

        /* NOTE: 프로그램 변경 시 NofM 전략이라면 NofM Phase가 0부터 수행될 수 있게 초기화. */
        if (addr_MapProgramData_stimulationStrategy == df_stimulationStrategy_NofM)
        {
            g_NofM_Phase = 0;
        }

        prepare_pcmStimulationPacketHeader(); /* Stimulation pulse phase setting */
        read_FFT_PassBin_index();             /* Read FFT pass bin from FS memory to HEAR memory */

        /* Alert to the CM3 that the CFX has reloaded the map(program) data */
        Addr_SharedMem->mapChangeFlag.cfx_Reloaded_MapdataFlag = 1;

        /* Clear map change command (Then, CM3 will calculate stimulation parameters) */
        Addr_SharedMem->mapChangeFlag.cm3Command_mapChange = 0;
    }
}

void Normal_PowerMode_event_audioParameterCalculation(void)
{
    /* After CM3 calculate stimulation parameters */
    if (Addr_SharedMem->mapChangeFlag.cm3_audioParameterCalculationDone_Flag == 1)
    {
        read_transferableChannelNum_fromCM3(); /* Update number of transferable channel based on stimulation pulse width */
        read_C_T_Level_fromCM3();              /* Update C-T value calculated by CM3 */

        /* clear buffer for magnitudeOfFFT */
        for (register int i = 0; i < HALF_FFT_SIZE; i++)
            chess_loop_range(HALF_FFT_SIZE, HALF_FFT_SIZE)
            {
                ((int _XMEM *) HEAR_ADDR_VMAG_OUTPUT)[i] = 0;
            }

        /* Set the 'addr_prev_stimulationVolume' to '-1'.
         * So, 'userSettingValue.stimulVolume' and 'addr_prev_stimulationVolume' must will be different.
         * This makes that the CFX calculate and update the stimulation volume parameters. */
        m_previous_stimulation_volume = -1;
        m_previous_audio_volume       = -1;

        Addr_SharedMem->mapChangeFlag.cm3_audioParameterCalculationDone_Flag = 0; /* Clear flag */
    }
}

void Normal_PowerMode_event_stimulationVolumeChange(void)
{
    /* Calculate and Update stimulation volume parameters */
    if (Addr_SharedMem->userSettingValue.stimulVolume != m_previous_stimulation_volume)
    {
        /* Fill the PCM with NOP data before calculate and update stimulation volume parameters */
        fn_PcmBitStream_Mode_NopStandby();

        /* Calculate C level (stimulation volume 1 - 4) */
        calculate_newStimulation_maxLevel(Addr_SharedMem->userSettingValue.stimulVolume);

        /* Calculate Log Coefficients (A, B) */
        calculate_logaritmMapping_coeff();

        m_previous_stimulation_volume = Addr_SharedMem->userSettingValue.stimulVolume; /* Update stimulation volume value */
    }
}

void Normal_PowerMode_event_both_stimulation_audio_volumeChange(void)
{
    if ((Addr_SharedMem->userSettingValue.stimulVolume != m_previous_stimulation_volume) || (Addr_SharedMem->userSettingValue.audioVolume != m_previous_audio_volume))
    {
        if ((0 < Addr_SharedMem->userSettingValue.stimulVolume) && (0 < Addr_SharedMem->userSettingValue.audioVolume))
        {
            /* Fill the PCM with NOP data before calculate and update stimulation volume parameters */
            fn_PcmBitStream_Mode_NopStandby();

            m_previous_stimulation_volume = Addr_SharedMem->userSettingValue.stimulVolume; /* Update stimulation volume value */
            m_previous_audio_volume       = Addr_SharedMem->userSettingValue.audioVolume;  /* Update audio volume value */
            m_audio_volume                = Addr_SharedMem->userSettingValue.audioVolume - 1;

            /* Calculate C level (stimulation volume 1 - 4) */
            calculate_newStimulation_maxLevel(Addr_SharedMem->userSettingValue.stimulVolume);

            /* Calculate Log Coefficients with Audio Volume (A, B) */
            calculate_logaritmMapping_coeff_with_audioVolume();
        }
    }
}

void Normal_PowerMode_isdConnected(void)
{
    if (m_previous_isd != m_current_isd)
    {
        /* 기존 연결된 ISD와 다르거나, 연결 해제 중인 상태에서 새로 ISD와 연결된 경우 이 구문을 수행한다.
         * 기존 방식과 다르게 CFX가 아닌 CM3가 사용자 설정 정보를 파일시스템에서 읽고,
         * 해당 사용자 설정 정보를 공유 메모리에 업데이트 하도록 수정되었다. */

        Addr_SharedMem->userSettingValueLoadedFlag = 1;             /* Alert to the CM3 that the CFX has confirmed the ISD number change */
        m_previous_isd                             = m_current_isd; /* Update the ISD number */
    }

    /* If the map(program) is changed, the CM3 will let the CFX know. */
    Normal_PowerMode_event_mapChange();

    /* Stimulation volume은 Logaritm Mapping에 사용되는 자극 Amplitude와 관련되고,
     * Audio volume은 AGC에서 사용되는 오디오 입력과 관련되어 있음을 주의할 것. */

    /* After CM3 calculate stimulation parameters */
    Normal_PowerMode_event_audioParameterCalculation();

    /* 로그매핑의 xMin 관련 기능 추가한 코드
     * 자극 볼륨, 오디오 볼륨 중 어떠한 것이라도 변경되면 전체 계수를 다시 계산하도로 한다. */

#if 0  // xMin 기능 추가하지 않은 기존 코드

    /* Calculate and Update stimulation volume parameters */
    Normal_PowerMode_event_stimulationVolumeChange();

    /* Audio volume can be update in realtime. */
    if (0 != Addr_SharedMem->userSettingValue.audioVolume)
    {
        m_audio_volume = Addr_SharedMem->userSettingValue.audioVolume - 1;
    }

#else  // xMin 기능 추가된 코드

    /* 기존의 Normal_PowerMode_event_stimulationVolumeChange() 함수에서
     * 오디오 볼륨 조정시 로그매핑의 A, B 계수 구하는걸 다시 하도록 똑같이 적용하자.
     * xMin, xMax에 게인 곱하고 '>>' 하는 그 계산을 적용하면 된다.
     * 기존 A, B 계수 구하는 함수 내에 주석 했으니 참고하자. */

    /* Calculate and Update stimulation and audio volume parameters */
    Normal_PowerMode_event_both_stimulation_audio_volumeChange();

#endif  // xMin 기능 관련 블록 끝
}

void Normal_PowerMode_isdDisonnected(void)
{
    /* 연결된 ISD가 없을 때, Mapping app 연결 상태가 아닌 경우에는 Link connection check counter를 초기화 한다. */
    if (Addr_SharedMem->isMappingProgramConneted != 1)
    {
        m_link_connection_check_counter = -df_connectionCheckPeriod_ms; /* Init link connection check timer */

        /* 연결된 상태가 아닐 때는 Check 상태를 초기화 시킨다. */
        Addr_SharedMem->cfx_PCM_interface.conneded_ISDCheckPCM_state = BackelCircuitDisabled_FpagFifoCleared_duringLiveStimulation;
    }

    if (m_previous_isd != 0) /* ISD와 연결이 해제되는 첫 시점인 경우 */
    {
        if (Addr_SharedMem->isMappingProgramConneted != 1) /* Mapping app 연결 상태가 아닌 경우 */
        {
            /* attackRelease의 게인 값이 ISD 연결 해제시 다시 0으로 초기화 되도록 추가 */
            m_prev_agc_10db_gain_Q8_16 = 0;

            /* 기존에는 ISD 연결이 해제되는 시점에 사용자 설정 정보를 EEPROM에 업데이트하는 방식이었음.
             * 현재는 리모콘 앱을 통해 CM3에서 볼륨, LED 알림 등의 이벤트가 발생할 때마다
             * CM3에서 파일시스템의 사용자 설정 정보를 바로 바로 업데이트 하도록 변경하였음. */

            Addr_SharedMem->userSettingValueLoadedFlag = 0; /* Alert to the CM3 that the CFX knows that the ISD has been disconnected */
        }
    }

    m_previous_isd = 0; /* Update the ISD number */
}

void copy_MappingData(void)
{
    int _IOMEM *pMappingDate = (int _IOMEM *) &Addr_SharedMem->currentMapData.mappingDate;

    /* copy MappingDate */

    for (register int i = 0; i < df_24bitWordLength_mappingDate; i++)
        chess_loop_range(df_24bitWordLength_mappingDate, df_24bitWordLength_mappingDate)
        {
            addr_MapProgramData_mappingDate[i] = pMappingDate[i];
        }

    copy_MappingData_without_mappingDate();
}

void copy_MappingData_without_mappingDate(void)
{
    /* copy MappingData after MappingDate */

    addr_MapProgramData_stimulationStrategy          = Addr_SharedMem->currentMapData.stimulationStrategy;
    addr_MapProgramData_firstPulsePhase              = Addr_SharedMem->currentMapData.firstPulsePhase;
    addr_MapProgramData_StimulationMode              = Addr_SharedMem->currentMapData.stimulationMode;
    addr_MapProgramData_StimulationPulsePhaseWidth   = Addr_SharedMem->currentMapData.stimulationPulsePhaseWidth;
    addr_MapProgramData_FrequencyAnalysisBandNumbers = Addr_SharedMem->currentMapData.numFrequencyBand;
    addr_MapProgramData_indicatorStimulCannel_index  = Addr_SharedMem->currentMapData.stimulationIndicatorChannelNum;
    addr_MapProgramData_indicatorStimulLevel_uA      = Addr_SharedMem->currentMapData.stimulationIndicatorAmplitude_uA;

    for (register int i = 0; i < df_MaxNumOfElectrode; i++)
        chess_loop_range(df_MaxNumOfElectrode, df_MaxNumOfElectrode)
        {
            addr_MapProgramData_StimulusChannelAssignedElectrodIndex[i]  = Addr_SharedMem->currentMapData.usableStimulationElectrodIndex[i];
            addr_MapProgramData_ReferenceChannelAssignedElectrodIndex[i] = Addr_SharedMem->currentMapData.usableReferenceElectrodIndex[i];
            addr_MapProgramData_CIS_FreqBandOrder[i]                     = Addr_SharedMem->currentMapData.CIS_FreqBandOrder[i];
            addr_MapProgramData_T_Level_uA[i]                            = Addr_SharedMem->currentMapData.T_level_uA[i];
            addr_MapProgramData_C_Level_uA[i]                            = Addr_SharedMem->currentMapData.C_level_uA[i];
            addr_MapProgramData_xMinLevel[i]                             = Addr_SharedMem->currentMapData.audio_input_x_mim[i];
            addr_MapProgramData_xMaxLevel[i]                             = Addr_SharedMem->currentMapData.audio_input_x_max[i];
        }
}
