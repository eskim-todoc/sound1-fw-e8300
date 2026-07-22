#ifndef __tdc_drv_mis2dh_h__
#define __tdc_drv_mis2dh_h__

#if 1
#define TDC_DRV_MIS2DH_I2C_ADDR 0x19 //
#else
#define TDC_DRV_MIS2DH_I2C_ADDR 0x33 // 테스트 보드 i2c 주소
#endif
#define TDC_DRV_MIS2DH_GRAVITY 9.806

typedef enum
{
    TDC_DRV_MIS2DH_STATUS_REG_AUX  = 0X07,
    TDC_DRV_MIS2DH_OUT_TEMP_L      = 0X0C,
    TDC_DRV_MIS2DH_OUT_TEMP_H      = 0X0D,
    TDC_DRV_MIS2DH_INT_COUNTER_REG = 0X0E,
    TDC_DRV_MIS2DH_WHO_AM_I_REG    = 0X0F,
    TDC_DRV_MIS2DH_TEMP_CFG_REG    = 0X1F,
    TDC_DRV_MIS2DH_CTRL_REG1       = 0X20, // 동작 주파수 , 동작 모드, x-y-z 축 활성화  설정
    TDC_DRV_MIS2DH_CTRL_REG2       = 0X21, // 필터 설정(스트리밍 데이터, 클릭 , 인터럽트)
    TDC_DRV_MIS2DH_CTRL_REG3       = 0X22, // 인터럽트 PAD 1에 매핑되는 인터럽트 설정
    TDC_DRV_MIS2DH_CTRL_REG4       = 0X23, // 데이터 스트리밍에 적용될 설정
    TDC_DRV_MIS2DH_CTRL_REG5       = 0X24, // FIFO 및 인터럽트 pending 설정
    TDC_DRV_MIS2DH_CTRL_REG6       = 0X25, // 인터럽트 PAD 2에 매핑되는 인터럽트 설정
    TDC_DRV_MIS2DH_REF_DAT_CAP     = 0X26, // 인터럽트 발생 기준값??
    TDC_DRV_MIS2DH_STATUS_REG      = 0X27, // x-y-z 가속도 데이터 값 상태
    TDC_DRV_MIS2DH_OUT_X_L         = 0X28, // x축 가속도 데이터(하위)
    TDC_DRV_MIS2DH_OUT_X_H         = 0X29, // x축 가속도 데이터(하위)
    TDC_DRV_MIS2DH_OUT_Y_L         = 0X2A, // y축 가속도 데이터(하위)
    TDC_DRV_MIS2DH_OUT_Y_H         = 0X2B, // y축 가속도 데이터(하위)
    TDC_DRV_MIS2DH_OUT_Z_L         = 0X2C, // z축 가속도 데이터(하위)
    TDC_DRV_MIS2DH_OUT_Z_H         = 0X2D, // z축 가속도 데이터(하위)
    TDC_DRV_MIS2DH_FIFO_CTRL_REG   = 0X2E, // FIFO 동작 설정
    TDC_DRV_MIS2DH_FIFO_SCR_REG    = 0X2F, // FIFO 상태
    TDC_DRV_MIS2DH_INT1_CFG        = 0X30, // 인터럽트 1 설정(x:상,하 ;  y:상,하  ; z:상, 하)
    TDC_DRV_MIS2DH_INT1_SRC        = 0X31, // 인터럽트 1 상태
    TDC_DRV_MIS2DH_INT1_THS        = 0X32, // 인터럽트 1 문턱값
    TDC_DRV_MIS2DH_INT1_DURATION   = 0X33, // 인터럽트 1 최소 유지시간(동작 주파수에 따라 다르게 계산됨)
    TDC_DRV_MIS2DH_INT2_CFG        = 0X34, // 인터럽트 2설정(x:상,하 ;  y:상,하  ; z:상, 하)
    TDC_DRV_MIS2DH_INT2_SRC        = 0X35, // 인터럽트 2 상태
    TDC_DRV_MIS2DH_INT2_THS        = 0X36, // 인터럽트 2 문턱값
    TDC_DRV_MIS2DH_INT2_DURATION   = 0X37, // 인터럽트 2 유지 시간
    TDC_DRV_MIS2DH_CLICK_CFG       = 0X38, // 클릭 인터럽트 설정(더블클릭 x,y,z 싱글클릭 x,y,z)
    TDC_DRV_MIS2DH_CLICK_SRC       = 0X39, // 클릭 인터럽트 상태
    TDC_DRV_MIS2DH_CLICK_THS       = 0X3A, // 클릭 인터럽트 문턱값
    TDC_DRV_MIS2DH_TIME_LIMIT      = 0X3B, // 클릭 타임 제한
    TDC_DRV_MIS2DH_TIME_LATENCY    = 0X3C, // 클릭 타임 지연
    TDC_DRV_MIS2DH_TIME_WINDOW     = 0X3D, // 클릭 타임 윈도우
    TDC_DRV_MIS2DH_ACT_THS         = 0X3E, // 슬립 모드 전환 문턱값
    TDC_DRV_MIS2DH_ACT_DUR         = 0X3F, // 슬립모드 전환 유지시간

} tdc_drv_mis2dh_register_addr_t;

typedef enum
{
    TDC_DRV_MIS2DH_8_BIT_RES  = 0x00,
    TDC_DRV_MIS2DH_10_BIT_RES = 0x01,
    TDC_DRV_MIS2DH_12_BIT_RES = 0x02,

} tdc_drv_mis2dh_resolution_t;

typedef enum
{
    TDC_DRV_MIS2DH_2G_FS  = 0x00,
    TDC_DRV_MIS2DH_4G_FS  = 0x01,
    TDC_DRV_MIS2DH_8G_FS  = 0x02,
    TDC_DRV_MIS2DH_16G_FS = 0x03,

} tdc_drv_mis2dh_fullscalevalue_t;

typedef enum
{
    TDC_DRV_MIS2DH_POWER_DOWN = 0x00,
    TDC_DRV_MIS2DH_1HZ_ODR    = 0x01,
    TDC_DRV_MIS2DH_10HZ_ODR   = 0x02,
    TDC_DRV_MIS2DH_25HZ_ODR   = 0x03,
    TDC_DRV_MIS2DH_50HZ_ODR   = 0x04,
    TDC_DRV_MIS2DH_100HZ_ODR  = 0x05,
    TDC_DRV_MIS2DH_200HZ_ODR  = 0x06,
    TDC_DRV_MIS2DH_400HZ_ODR  = 0x07,
    TDC_DRV_MIS2DH_1620HZ_ODR = 0x08,
    TDC_DRV_MIS2DH_1344HZ_ODR = 0x09,

} tdc_drv_mis2dh_samplingrate_t;

typedef enum
{
    TDC_DRV_MIS2DH_LOW_POWER_MODE = 0x00,
    TDC_DRV_MIS2DH_NORMAL_MODE    = 0x01,
    TDC_DRV_MIS2DH_HI_RES_MODE    = 0x02,

} tdc_drv_mis2dh_operatingmode_t;

typedef struct
{
    char addrSubRegister;
    char data;
} tdc_drv_mis2dh_register_t;

typedef struct
{
    int x_acceleration;
    int y_acceleration;
    int z_acceleration;
} tdc_drv_mis2dh_stream_xyz_t;

#define TDC_DRV_MIS2DH_CLICK_WAITING_TIME            35 // 300mse
#define TDC_DRV_MIS2DH_TH_ACCEL_X              20
#define TDC_DRV_MIS2DH_TH_ACCEL_Y              20
#define	TDC_DRV_MIS2DH_TH_ACCEL_Z				120


	bool tdc_drv_mis2dh_configure_xyz_stream(void);
	bool tdc_drv_mis2dh_reset(void);
	bool tdc_drv_mis2dh_configure_click_mode(int numActivation);
	/* 호출처 0 이나 본문에 전처리기 분기가 있어 정의를 남겼다 - 선언도 함께 유지.
	 * 제거 판단은 G4(pwr) 또는 별도 정리 작업에서 (게이트-노트/G3-drv.md §사장) */
	bool tdc_drv_mis2dh_update_xyz_acceleration(void);

#ifdef UART_isDedicated_CM3_DATA

	void tdc_drv_mis2dh_transfer_acceleration_value(bool resultTrue);

#endif

#endif
