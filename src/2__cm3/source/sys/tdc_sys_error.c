

#include <tdc_sys_error.h>
#include <tdc_ble_protocol.h>

#include <tdc_hal_spi.h>

static tdc_sys_error_code_t errorCode;

void tdc_sys_error_update(tdc_sys_error_major_t majorError, int detailError, int lineNumber)
{

    switch (majorError)
    {
        case en__dataProcessing_ERROR:
        {
            errorCode.dataProcessingErrorFlag = detailError;
        }
        break;

        case en__ACCELEROMETER_ERROR:
        {
            errorCode.accelerometerErrorFlag = detailError;
        }
        break;

        case en__RF_PowerIC_ERROR:
        {
            errorCode.PowerIcErrorFlag = detailError;
        }
        break;

        case en__FPGA_COMMUNICATION_ERROR:
        {
            errorCode.FPGA_CommunicationErrorFlag = detailError;
        }
        break;

        case en__FPGA_CONFIGUARATION_ERROR:
        {
            errorCode.FPGA_ConfiguraionErrorFlag = detailError;
        }
        break;

        case en__EN__ISD_ERROR:
        {
            errorCode.ISD_ErrorFlag = detailError;
        }
        break;

        case en__MAJOR_ERRORCODE_DATA_LOGGING_ERROR:
        {
            errorCode.data_logging_error = detailError;
        }
        break;

        default:
            break;
    }

}

void tdc_sys_error_update_fpga_system(int value)
{
    errorCode.FPGA_systemError = value;
}

void tdc_sys_error_update_fpga_backtel(int value)
{
    errorCode.FPGA_backtelError = value;
}

void tdc_sys_error_clear_flag(tdc_sys_error_major_t majorError)
{
    switch (majorError)
    {
        case en__dataProcessing_ERROR:
        {
            errorCode.dataProcessingErrorFlag = en__NA;
        }
        break;

        case en__ACCELEROMETER_ERROR:
        {
            errorCode.accelerometerErrorFlag = en__NA;
        }
        break;

        case en__RF_PowerIC_ERROR:
        {
            errorCode.PowerIcErrorFlag = en__NA;
        }
        break;

        case en__FPGA_COMMUNICATION_ERROR:
        {
            errorCode.FPGA_CommunicationErrorFlag = en__NA;
        }
        break;

        case en__FPGA_CONFIGUARATION_ERROR:
        {
            errorCode.FPGA_ConfiguraionErrorFlag = en__NA;
        }
        break;

        case en__EN__ISD_ERROR:
        {
            errorCode.ISD_ErrorFlag = en__NA;
        }
        break;

        case en__MAJOR_ERRORCODE_DATA_LOGGING_ERROR:
        {
            errorCode.data_logging_error = en__NA;
        }
        break;
    }
}

void tdc_sys_error_clear_all(void)
{

    errorCode.dataProcessingErrorFlag = en__NA;

    errorCode.accelerometerErrorFlag = en__NA;

    errorCode.PowerIcErrorFlag = en__NA;

    errorCode.FPGA_CommunicationErrorFlag = en__NA;

    errorCode.FPGA_ConfiguraionErrorFlag = en__NA;

    errorCode.ISD_ErrorFlag = en__NA;

    errorCode.data_logging_error = en__NA;

    tdc_sys_error_update_fpga_system(en__NA);
    tdc_sys_error_update_fpga_backtel(en__NA);
}

tdc_sys_error_code_t tdc_sys_error_read(void)
{
    return errorCode;
}

void tdc_sys_error_send_to_app(EN__MAPPING_COMMAND command, tdc_sys_error_major_t majorError, int minorError, int lineNumber)
{
    int bufferForSPI_tx[BLE_DataPacketSize];
    int buffer_tx_index;
    int value;

    buffer_tx_index = 0;

    // 송신 데이터 준비
    // 에러 발생
    bufferForSPI_tx[buffer_tx_index++] = en__ERROR_Response;

    // command loop-back
    bufferForSPI_tx[buffer_tx_index++] = command;

    // 에러 종류
    bufferForSPI_tx[buffer_tx_index++] = majorError;

    // 에러 상세
    bufferForSPI_tx[buffer_tx_index++] = minorError;

    // 발생 파일의 라인 넘버..
    bufferForSPI_tx[buffer_tx_index++] = lineNumber >> 8;    // 상위 바이트
    bufferForSPI_tx[buffer_tx_index++] = lineNumber & 0xFF;  // 하위 바이트

    // 송신 데이터 SPI TX버퍼에 복사
    tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);
}
