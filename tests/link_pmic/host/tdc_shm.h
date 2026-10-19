#ifndef TDC_LINK_TEST_SHM_H
#define TDC_LINK_TEST_SHM_H
typedef struct { int tdc_backtel_period_100ms; int tdc_power_stable_nop_count; } tdc_test_shm_t;
extern tdc_test_shm_t cfx_cm3_sharedMemoryAll;
typedef struct { unsigned stimulationPulsePhaseWidth, stimulationStrategy; } tdc_test_map_t;
typedef struct { int frameNumPerChannel; } tdc_test_para_t;
tdc_test_map_t *getPointerCurrentMapData(void);
tdc_test_para_t *getPointerCalculatedStimulPara_byCM3(void);
#endif
