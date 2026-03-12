#ifndef ISD_INTERFACE_H__
#define ISD_INTERFACE_H__

#include <stdbool.h>

#define ResetCounter      -1
#define DelayResetCounter -100

typedef enum
{
    en__isdStatus_NA = 0,
    en__isdStatus_PowerIC_Reset,   // PCM 초기화 시작
    en__isdStatus_PowerIC_OK,      // FPGA 초기화 시작
    en__isdStatus_FPGA_Ok,         // 내부기 파워 조정 시작
    en__isdStatus_ISD_Power_Ok,    // 내부기 파워 정상이므로 내부기 패스 오픈
    en__isdStatus_ISD_pathOpen_Ok, // 내부기 패스가 열렸으며 10V 출력 활성화
    en__isdStatus_stimul_10V_Ok    // 내부기 출력 10v 정상. 자극 출력 설정 시작

} EN__ISD_CONTROL_STATE;

typedef struct
{
    EN__ISD_CONTROL_STATE isd_controlState;
    bool                  conneded_ISD;

} ST__ISD_STATUS;

ST__ISD_STATUS isd_interface(bool isd_enable, bool mappingConnection, EN__ISD_CONTROL_STATE isdControlCommand);
void           change_isd_state(EN__ISD_CONTROL_STATE ISD_controlState);
void           clearIsdControlStateChagedFlag(void);
void           clearCommandStartFlag(void);
bool           isIsdConnected(void);
void           update_isd_Link_is_Connected(void);
void           update_isd_Link_is_Disconnected(void);

void fill_pcmBuff_check_ISD_PathOpen_normalValue(int *p_pcm_index);
void fill_pcmBuff_lastSimulationOut(int *p_pcm_index);
void fill_pcmBuff_check_ISD_PathOpen_duplicateZeroData(int *p_pcm_index);

void update_isd_LinkConnection_byBacktel_withLiveStimulation(void);
void update_isd_LinkConnection_byBacktel_withMapping(int connectionCheckCOUNTER);
bool isING_connectionCheckWithMapping(void);

bool is_i2c_free(void);
void change_i2c_is_busy(void);
void change_i2c_is_free(void);





#endif
