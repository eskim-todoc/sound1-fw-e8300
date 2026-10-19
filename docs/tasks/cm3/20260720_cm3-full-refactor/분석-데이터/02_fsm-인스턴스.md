# FSM/상태 보유 함수 후보 (본문 static 보유 또는 switch+state)

총 79개

| static수 | switch | state어휘 | 함수 | 파일:라인 |
|---|---|---|---|---|
| 16 | O | O | `impedanceMeasurement` | `Cortex-M3-src/internalDevice/isd_interface_mapping_impedanceMeasurement.c:29` |
| 15 | O | O | `testStimulation` | `Cortex-M3-src/internalDevice/isd_interface_mapping_testStimulation.c:30` |
| 15 | O | O | `specificStimulation` | `Cortex-M3-src/internalDevice/isd_interface_mapping_SepcificStimulation.c:29` |
| 15 | O | O | `eCapMeasurement_masking` | `Cortex-M3-src/internalDevice/isd_interface_mapping_eCAP_Measurement.c:104` |
| 14 | O | O | `liveStimulation` | `Cortex-M3-src/internalDevice/isd_interface_mapping_Live.c:28` |
| 9 | O | O | `init_ISD` | `Cortex-M3-src/internalDevice/isd_interface_init_FPGA.c:270` |
| 8 | O | O | `settingStimulPara_biPolarMode` | `Cortex-M3-src/internalDevice/isd_interface_stimulationParaSetting.c:398` |
| 6 | O | O | `update_isd_LinkConnection_byBacktel_withLiveStimulation` | `Cortex-M3-src/internalDevice/isd_interface.c:296` |
| 6 | O | O | `settingStimulPara_monoPolarMode` | `Cortex-M3-src/internalDevice/isd_interface_stimulationParaSetting.c:36` |
| 5 | O | O | `mappingControl` | `Cortex-M3-src/BleCommunication/mappingControl.c:1874` |
| 5 | O |  | `read_signal_processingPara` | `Cortex-M3-src/BleCommunication/remoteControl_read_SP_para.c:12` |
| 4 | O | O | `isd_path_Open` | `Cortex-M3-src/internalDevice/isd_interface_init_ISD.c:35` |
| 4 |  | O | `write_Original_isdInfo_N_userSetting_atFlash` | `Cortex-M3-src/internalDevice/isd_interface_mapping_readWrtieMapData.c:113` |
| 3 | O | O | `update_isd_LinkConnection_byBacktel_withMapping` | `Cortex-M3-src/internalDevice/isd_interface.c:548` |
| 3 | O | O | `remoteControl` | `Cortex-M3-src/BleCommunication/remoteControl.c:648` |
| 3 | O | O | `init_FPGA` | `Cortex-M3-src/internalDevice/isd_interface_init_FPGA.c:142` |
| 3 | O | O | `enableStimul_10v` | `Cortex-M3-src/internalDevice/isd_interface_init_ISD.c:735` |
| 3 | O |  | `reset_NVM_All_ISD_allData` | `Cortex-M3-src/internalDevice/isd_interface_mapping_readWrtieMapData.c:715` |
| 3 | O |  | `reset_NVM_2to4_ISD_allData` | `Cortex-M3-src/internalDevice/isd_interface_mapping_readWrtieMapData.c:798` |
| 3 | O |  | `read_stimulPara_fromFlash` | `Cortex-M3-src/internalDevice/isd_interface_mapping_readWrtieMapData.c:402` |
| 3 | O |  | `fetch_mappingControlPacket` | `Cortex-M3-src/BleCommunication/mappingControl.c:58` |
| 3 |  | O | `tdc_qcc_has_batt_level_rx_timed_out` | `Cortex-M3-src/main.c:779` |
| 3 |  |  | `stimulation_IndicatorOut` | `Cortex-M3-src/internalDevice/indicatorByStimul.c:23` |
| 3 |  |  | `stimulationStandAlone` | `Cortex-M3-src/internalDevice/isd_interface_stimulationStandAlone.c:55` |
| 2 | O | O | `init_txPowerIC` | `Cortex-M3-src/internalDevice/isd_interface_init_FPGA.c:40` |
| 2 | O | O | `bleCommunication` | `Cortex-M3-src/BleCommunication/ble_communication.c:227` |
| 2 | O |  | `read_isdInfo_N_userSetting_fromFlash` | `Cortex-M3-src/internalDevice/isd_interface_mapping_readWrtieMapData.c:212` |
| 2 | O |  | `read_Original_isdInfo_N_userSetting_fromFlash` | `Cortex-M3-src/internalDevice/isd_interface_mapping_readWrtieMapData.c:11` |
| 2 | O |  | `file_PCM_templete_eCAP` | `Cortex-M3-src/internalDevice/isd_interface_mapping_eCAP_Measurement.c:40` |
| 2 | O |  | `fetch_remoteControlPacket` | `Cortex-M3-src/BleCommunication/remoteControl.c:60` |
| 2 |  | O | `updateEarPieceStatus` | `Cortex-M3-src/systemControl/earpieceUpate.c:11` |
| 2 |  |  | `led_engine_run` | `Cortex-M3-src/systemControl/LedOutput.c:485` |
| 2 |  |  | `LED_OUT` | `Cortex-M3-src/systemControl/LedOutput.c:970` |
| 1 | O | O | `I2C_0_IRQHandler` | `Cortex-M3-src/systemControl/driver_i2c.c:163` |
| 1 |  | O | `write_change_TxPowerLevel` | `Cortex-M3-src/internalDevice/isd_interface_FPGA.c:792` |
| 1 |  | O | `write_FPGA_reset` | `Cortex-M3-src/internalDevice/isd_interface_FPGA.c:647` |
| 1 |  | O | `write_FPGA_enable_RF_tx` | `Cortex-M3-src/internalDevice/isd_interface_FPGA.c:674` |
| 1 |  | O | `write_FPGA_disable_RF_tx` | `Cortex-M3-src/internalDevice/isd_interface_FPGA.c:736` |
| 1 |  | O | `write_FPGA_clear_FIFO` | `Cortex-M3-src/internalDevice/isd_interface_FPGA.c:850` |
| 1 |  | O | `update_mapNum` | `Cortex-M3-src/main.c:146` |
| 1 |  | O | `read_txPowerLevel` | `Cortex-M3-src/internalDevice/isd_interface_FPGA.c:463` |
| 1 |  | O | `read_FPGA_version` | `Cortex-M3-src/internalDevice/isd_interface_FPGA.c:137` |
| 1 |  | O | `read_FPGA_systemResgister_1st` | `Cortex-M3-src/internalDevice/isd_interface_FPGA.c:160` |
| 1 |  | O | `read_FPGA_backtel_FIFO` | `Cortex-M3-src/internalDevice/isd_interface_FPGA.c:420` |
| 1 |  | O | `read_FPGA_backtelError_Flag` | `Cortex-M3-src/internalDevice/isd_interface_FPGA.c:291` |
| 1 |  | O | `read_FPGA_backtelConfig` | `Cortex-M3-src/internalDevice/isd_interface_FPGA.c:379` |
| 1 |  | O | `read_FPGA_PulseWidth` | `Cortex-M3-src/internalDevice/isd_interface_FPGA.c:313` |
| 1 |  | O | `read_FPGA_FIFO_counter` | `Cortex-M3-src/internalDevice/isd_interface_FPGA.c:338` |
| 1 |  | O | `led_arbiter_tick` | `Cortex-M3-src/systemControl/LedOutput.c:689` |
| 1 |  | O | `is_RF_tx_eanble` | `Cortex-M3-src/internalDevice/isd_interface_FPGA.c:704` |
| 1 |  | O | `i2c_write` | `Cortex-M3-src/systemControl/tdc_touch_iqs323.c:57` |
| 1 |  | O | `check_FPGA_PCM_Error` | `Cortex-M3-src/internalDevice/isd_interface_FPGA.c:201` |
| 1 |  | O | `check_FPGA_FIFO_empty` | `Cortex-M3-src/internalDevice/isd_interface_FPGA.c:236` |
| 1 |  | O | `NRF_On_OFF` | `Cortex-M3-src/systemControl/systemControl.c:53` |
| 1 |  |  | `write_stimulPara_atFlash` | `Cortex-M3-src/internalDevice/isd_interface_mapping_readWrtieMapData.c:577` |
| 1 |  |  | `write_isdInfo_N_userSetting_atFlash` | `Cortex-M3-src/internalDevice/isd_interface_mapping_readWrtieMapData.c:329` |
| 1 |  |  | `transfer_AccelerationVlaue` | `Cortex-M3-src/systemControl/driver_MIS2DH.c:523` |
| 1 |  |  | `tdc_ui_command_poll` | `Gen1_5/ui/tdc_ui_command.c:913` |
| 1 |  |  | `reset_NVM_Selected_ISD_allData` | `Cortex-M3-src/internalDevice/isd_interface_mapping_readWrtieMapData.c:637` |
| 1 |  |  | `reset_NVM_MapData` | `Cortex-M3-src/internalDevice/isd_interface_mapping_readWrtieMapData.c:897` |
| 1 |  |  | `ci_map_write_user_setting_value` | `Gen1_5/FS/ci_map.c:80` |
| 1 |  |  | `ci_map_write_map_stamp` | `Gen1_5/FS/ci_map.c:95` |
| 1 |  |  | `ci_map_write_map_data` | `Gen1_5/FS/ci_map.c:110` |
| 1 |  |  | `ci_map_write_isd_info` | `Gen1_5/FS/ci_map.c:66` |
| 1 |  |  | `ci_map_read_user_setting_value` | `Gen1_5/FS/ci_map.c:20` |
| 1 |  |  | `ci_map_read_map_stamp` | `Gen1_5/FS/ci_map.c:33` |
| 1 |  |  | `ci_map_read_map_data` | `Gen1_5/FS/ci_map.c:45` |
| 1 |  |  | `ci_map_read_isd_info` | `Gen1_5/FS/ci_map.c:7` |
| 1 |  |  | `aes128_test` | `Cortex-M3-src/main.c:279` |
| 1 |  |  | `_ci_day_in_month` | `Gen1_5/common/ci_timer.c:36` |
| 0 | O | O | `tdc_touch_state_name` | `Cortex-M3-src/systemControl/tdc_touch.c:91` |
| 0 | O | O | `tdc_touch_process` | `Cortex-M3-src/systemControl/tdc_touch.c:243` |
| 0 | O | O | `tdc_led_set_ind_state` | `Cortex-M3-src/systemControl/LedOutput.c:327` |
| 0 | O | O | `stuck_eval` | `Cortex-M3-src/systemControl/tdc_touch_logic.c:17` |
| 0 | O | O | `snd_qcc_set_isd` | `Cortex-M3-src/QCC/snd_qcc.c:21` |
| 0 | O | O | `snd_charger_set_state` | `Cortex-M3-src/systemControl/batteryNPowerControl.c:52` |
| 0 | O | O | `setting_nrf_ble_adv_info` | `Cortex-M3-src/BleCommunication/ble_communication.c:46` |
| 0 | O | O | `isd_interface` | `Cortex-M3-src/internalDevice/isd_interface.c:163` |
| 0 | O | O | `ci_boot_handle_fsm` | `Cortex-M3-src/ci_boot.c:282` |
