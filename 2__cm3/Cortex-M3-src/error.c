

#include "error.h"
#include "ble_commonProtocol.h"

#include "driver_SPI.h"

static ST__ERROR_CODE errorCode;

void errorCodeUpdate(EN__MAJOR_ERRORCODE majorError, int detailError, int lineNumber)
{
    int temp;

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

#if 0  // 특정 에러 검출 시 해당 라인 번호 설정하고 아래 중단점 적용
    if(lineNumber!=804)
    {
        temp=1;
    }
#endif
}

void update_FPGA_systemError(int value)
{
    errorCode.FPGA_systemError = value;
}

void update_FPGA_backtelError(int value)
{
    errorCode.FPGA_backtelError = value;
}

void clearErrorFlag(EN__MAJOR_ERRORCODE majorError)
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

void clearAllErrorFlag(void)
{

    errorCode.dataProcessingErrorFlag = en__NA;

    errorCode.accelerometerErrorFlag = en__NA;

    errorCode.PowerIcErrorFlag = en__NA;

    errorCode.FPGA_CommunicationErrorFlag = en__NA;

    errorCode.FPGA_ConfiguraionErrorFlag = en__NA;

    errorCode.ISD_ErrorFlag = en__NA;

    errorCode.data_logging_error = en__NA;

    update_FPGA_systemError(en__NA);
    update_FPGA_backtelError(en__NA);
}

ST__ERROR_CODE readErrorCode(void)
{
    return errorCode;
}

void sendErrorToApp(EN__MAPPING_COMMAND command, EN__MAJOR_ERRORCODE majorError, int minorError, int lineNumber)
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
    writeDataToSpiTxBuff(bufferForSPI_tx, buffer_tx_index);
}
