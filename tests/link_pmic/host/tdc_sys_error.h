#ifndef TDC_LINK_TEST_ERROR_H
#define TDC_LINK_TEST_ERROR_H
enum { en__EN__ISD_ERROR=7, en__BackTelCounterZero=8, en__BackterDataLengthError=9, en__RF_PowerIC_ERROR=10, en__I2C_RFPOW_WritingError=11, en__I2C_RFPOW_ReadingError=12 };
void tdc_sys_error_update(int,int,int);
#endif
