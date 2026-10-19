#ifndef TDC_LINK_TEST_ISD_H
#define TDC_LINK_TEST_ISD_H
typedef enum { en__isdStatus_NA=0, en__isdStatus_PowerIC_Reset, en__isdStatus_PowerIC_OK, en__isdStatus_FPGA_Ok, en__isdStatus_ISD_Power_Ok, en__isdStatus_ISD_pathOpen_Ok, en__isdStatus_stimul_10V_Ok } EN__ISD_CONTROL_STATE;
void tdc_isd_change_state(EN__ISD_CONTROL_STATE);
#endif
