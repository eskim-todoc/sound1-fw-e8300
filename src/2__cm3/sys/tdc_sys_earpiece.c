#include <hw.h>
#include <stdbool.h>
#include "FPGA.h"
#include "cfx_cm3_sharedMemory.h"
#include "tdc_sys_error.h"
#include "commonDataProcessing.h"
#include "isd_interface.h"

#include "tdc_hal_i2c_isd.h"

bool tdc_sys_earpiece_update_status(void)
{
    static int updatecounter = 100;

    int readValue;
    int detectionValue;

    static int prev_detectionValue = 0;  // NOTE : for debugging

    updatecounter--;

    if (updatecounter == 0)
    {
        updatecounter = 100;
        return true;
    }
    else if (updatecounter == 3)  // 배터리 상태 업데이트를  100msec에 1회시 진행하고 있으면 동타임에 겹치지 않기 위해서..
    {
#if 1
        detectionValue = (Sys_GPIO_Read(DIO_PIN_INDEX_for_EARPIECE_DET_N) == 0) ? 1 : 0;

        // TDC_PRINTF("[DIO] EARPIECE DETECT VALUE : %u \r\n", detectionValue);

        if (detectionValue == 1)
        {
            // NOTE : for debugging
            if (detectionValue != prev_detectionValue)
            {
                prev_detectionValue = detectionValue;
                TDC_PRINTF_D("[EARPIRCE] CONNECTED \r\n");
            }

            updateEarpieceDetectionValue_toCFX(true);
        }
        else
        {
            // NOTE : for debugging
            if (detectionValue != prev_detectionValue)
            {
                prev_detectionValue = detectionValue;
                TDC_PRINTF_D("[EARPIECE] DISCONNECTED \r\n");
            }

            updateEarpieceDetectionValue_toCFX(false);
        }

        return true;
#else
        if (is_i2c_free())
        {

#ifdef CM3_I2C_controls_FPAG
            if (tdc_hal_i2c_isd_read(i2cAddr_FPGA_systemResgister_1st, &readValue, 1))
#else
            if (tdc_hal_i2c_cfx_read(i2cAddr_FPGA_systemResgister_1st, &readValue, 1))
#endif
            {

                detectionValue = data_ExtractionAndRigthShift(readValue, FPGA_BitPosition_EarPieceDetection, 1);

                if (detectionValue == 1)
                {
                    // NOTE : for debugging
                    if (detectionValue != prev_detectionValue)
                    {
                        prev_detectionValue = detectionValue;
                        TDC_PRINTF("[EARPIRCE] CONNECTED \r\n");
                    }

                    updateEarpieceDetectionValue_toCFX(true);
                }
                else
                {
                    // NOTE : for debugging
                    if (detectionValue != prev_detectionValue)
                    {
                        prev_detectionValue = detectionValue;
                        TDC_PRINTF("[EARPIECE] DISCONNECTED \r\n");
                    }

                    updateEarpieceDetectionValue_toCFX(false);
                }

                return true;
            }
            else
            {

                tdc_sys_error_update(en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_ReadingError, __LINE__);

                // I2C 읽기 실패, FPGA 초기화
                change_isd_state(en__isdStatus_PowerIC_OK);
                return false;
            }
        }
#endif
    }
    else
    {
        return true;
    }
}
