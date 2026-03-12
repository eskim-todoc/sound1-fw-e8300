#ifndef DRIVER_MIS2DH_H__
#define DRIVER_MIS2DH_H__

#if 1
#define MIS2DH_I2C_Addr 0x19 //
#else
#define MIS2DH_I2C_Addr 0x33 // 테스트 보드 i2c 주소
#endif
#define MIS2DH_GRAVITY 9.806

typedef enum
{
    MIS2DH_STATUS_REG_AUX  = 0X07,
    MIS2DH_OUT_TEMP_L      = 0X0C,
    MIS2DH_OUT_TEMP_H      = 0X0D,
    MIS2DH_INT_COUNTER_REG = 0X0E,
    MIS2DH_WHO_AM_I_REG    = 0X0F,
    MIS2DH_TEMP_CFG_REG    = 0X1F,
    MIS2DH_CTRL_REG1       = 0X20, // 동작 주파수 , 동작 모드, x-y-z 축 활성화  설정
    MIS2DH_CTRL_REG2       = 0X21, // 필터 설정(스트리밍 데이터, 클릭 , 인터럽트)
    MIS2DH_CTRL_REG3       = 0X22, // 인터럽트 PAD 1에 매핑되는 인터럽트 설정
    MIS2DH_CTRL_REG4       = 0X23, // 데이터 스트리밍에 적용될 설정
    MIS2DH_CTRL_REG5       = 0X24, // FIFO 및 인터럽트 pending 설정
    MIS2DH_CTRL_REG6       = 0X25, // 인터럽트 PAD 2에 매핑되는 인터럽트 설정
    MIS2DH_REF_DAT_CAP     = 0X26, // 인터럽트 발생 기준값??
    MIS2DH_STATUS_REG      = 0X27, // x-y-z 가속도 데이터 값 상태
    MIS2DH_OUT_X_L         = 0X28, // x축 가속도 데이터(하위)
    MIS2DH_OUT_X_H         = 0X29, // x축 가속도 데이터(하위)
    MIS2DH_OUT_Y_L         = 0X2A, // y축 가속도 데이터(하위)
    MIS2DH_OUT_Y_H         = 0X2B, // y축 가속도 데이터(하위)
    MIS2DH_OUT_Z_L         = 0X2C, // z축 가속도 데이터(하위)
    MIS2DH_OUT_Z_H         = 0X2D, // z축 가속도 데이터(하위)
    MIS2DH_FIFO_CTRL_REG   = 0X2E, // FIFO 동작 설정
    MIS2DH_FIFO_SCR_REG    = 0X2F, // FIFO 상태
    MIS2DH_INT1_CFG        = 0X30, // 인터럽트 1 설정(x:상,하 ;  y:상,하  ; z:상, 하)
    MIS2DH_INT1_SRC        = 0X31, // 인터럽트 1 상태
    MIS2DH_INT1_THS        = 0X32, // 인터럽트 1 문턱값
    MIS2DH_INT1_DURATION   = 0X33, // 인터럽트 1 최소 유지시간(동작 주파수에 따라 다르게 계산됨)
    MIS2DH_INT2_CFG        = 0X34, // 인터럽트 2설정(x:상,하 ;  y:상,하  ; z:상, 하)
    MIS2DH_INT2_SRC        = 0X35, // 인터럽트 2 상태
    MIS2DH_INT2_THS        = 0X36, // 인터럽트 2 문턱값
    MIS2DH_INT2_DURATION   = 0X37, // 인터럽트 2 유지 시간
    MIS2DH_CLICK_CFG       = 0X38, // 클릭 인터럽트 설정(더블클릭 x,y,z 싱글클릭 x,y,z)
    MIS2DH_CLICK_SRC       = 0X39, // 클릭 인터럽트 상태
    MIS2DH_CLICK_THS       = 0X3A, // 클릭 인터럽트 문턱값
    MIS2DH_TIME_LIMIT      = 0X3B, // 클릭 타임 제한
    MIS2DH_TIME_LATENCY    = 0X3C, // 클릭 타임 지연
    MIS2DH_TIME_WINDOW     = 0X3D, // 클릭 타임 윈도우
    MIS2DH_ACT_THS         = 0X3E, // 슬립 모드 전환 문턱값
    MIS2DH_ACT_DUR         = 0X3F, // 슬립모드 전환 유지시간

} MIS2DH_Register_Addr_t;

typedef enum
{
    MIS2DH_8_BIT_RES  = 0x00,
    MIS2DH_10_BIT_RES = 0x01,
    MIS2DH_12_BIT_RES = 0x02,

} EN__MIS2DH_Resolution_t;

typedef enum
{
    MIS2DH_2G_FS  = 0x00,
    MIS2DH_4G_FS  = 0x01,
    MIS2DH_8G_FS  = 0x02,
    MIS2DH_16G_FS = 0x03,

} EN__MIS2DH_FullScaleValue_t;

typedef enum
{
    MIS2DH_POWER_DOWN = 0x00,
    MIS2DH_1HZ_ODR    = 0x01,
    MIS2DH_10HZ_ODR   = 0x02,
    MIS2DH_25HZ_ODR   = 0x03,
    MIS2DH_50HZ_ODR   = 0x04,
    MIS2DH_100HZ_ODR  = 0x05,
    MIS2DH_200HZ_ODR  = 0x06,
    MIS2DH_400HZ_ODR  = 0x07,
    MIS2DH_1620HZ_ODR = 0x08,
    MIS2DH_1344HZ_ODR = 0x09,

} EN__MIS2DH_SamplingRate_t;

typedef enum
{
    MIS2DH_LOW_POWER_MODE = 0x00,
    MIS2DH_NORMAL_MODE    = 0x01,
    MIS2DH_HI_RES_MODE    = 0x02,

} EN__MIS2DH_OperatingMode_t;

typedef struct
{
    char addrSubRegister;
    char data;
} ST__MIS2DH_Register_t;

typedef struct
{
    int x_acceleration;
    int y_acceleration;
    int z_acceleration;
} ST__MIS2DH_Stream_XYZ_t;

#define clickWaitingTime            35 // 300mse
#define TH_Accelatin_X              20
#define TH_Accelatin_Y              20
#define	TH_Accelatin_Z				120


	bool updatae_XYZ_Accelation(void);
	bool configure_MIS2DH_asXyzStream(void);
	bool reset_MIS2DH(void);
	bool configure_MIS2DH_asClickMode(int numActivation);

#ifdef UART_isDedicated_CM3_DATA

	void transfer_AccelerationVlaue(bool resultTrue);

#endif

#endif
