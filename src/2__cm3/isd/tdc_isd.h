#ifndef __tdc_isd_h__
#define __tdc_isd_h__

#include <stdbool.h>

#define ResetCounter      -1
#define DelayResetCounter -100

typedef enum
{
    en__isdStatus_NA = 0,
    en__isdStatus_PowerIC_Reset,    // PCM 초기화 시작
    en__isdStatus_PowerIC_OK,       // FPGA 초기화 시작
    en__isdStatus_FPGA_Ok,          // 내부기 파워 조정 시작
    en__isdStatus_ISD_Power_Ok,     // 내부기 파워 정상이므로 내부기 패스 오픈
    en__isdStatus_ISD_pathOpen_Ok,  // 내부기 패스가 열렸으며 10V 출력 활성화
    en__isdStatus_stimul_10V_Ok     // 내부기 출력 10v 정상. 자극 출력 설정 시작

} EN__ISD_CONTROL_STATE;

typedef struct
{
    EN__ISD_CONTROL_STATE isd_controlState;
    bool                  conneded_ISD;

} ST__ISD_STATUS;

ST__ISD_STATUS tdc_isd_get_state(void);
ST__ISD_STATUS tdc_isd_step(bool isd_enable, bool mappingConnection, EN__ISD_CONTROL_STATE isdControlCommand);
void           tdc_isd_change_state(EN__ISD_CONTROL_STATE ISD_controlState);
void           tdc_isd_clear_control_state_changed_flag(void);
void           tdc_isd_clear_command_start_flag(void);
bool           tdc_isd_is_connected(void);
void           tdc_isd_update_link_connected(void);
void           tdc_isd_update_link_disconnected(void);

void tdc_isd_fill_pcm_path_open_normal(int *p_pcm_index);
void tdc_isd_fill_pcm_last_stimulation_out(int *p_pcm_index);
void tdc_isd_fill_pcm_path_open_dup_zero(int *p_pcm_index);

void tdc_isd_update_link_by_backtel_live(void);
void tdc_isd_update_link_by_backtel_mapping(int connectionCheckCOUNTER);
bool tdc_isd_is_connection_check_with_mapping(void);

bool tdc_isd_is_i2c_free(void);
void tdc_isd_set_i2c_busy(void);
void tdc_isd_set_i2c_free(void);





#endif
