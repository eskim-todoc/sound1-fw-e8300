#ifndef TDC_LINK_CONFIG_H
#define TDC_LINK_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#define TDC_LINK_CONFIG_MIN_LEVEL 112
#define TDC_LINK_CONFIG_MAX_LEVEL 214
#define TDC_LINK_CONFIG_OK 0
#define TDC_LINK_CONFIG_IO_ERROR (-1)
#define TDC_LINK_CONFIG_RANGE_ERROR (-2)

typedef struct
{
    uint32_t min_level;
    uint32_t mapping_min_level;
    uint32_t period_100ms;
    uint32_t step_up;
    uint32_t coin_count;
    uint32_t mode;
    uint32_t accel;
    uint32_t max_level;
} tdc_link_config_t;

/* CM3 전경 전용. get의 포인터는 정적 저장소이며 호출자가 수정하지 않는다.
 * set은 8F20 인덱스2/3/4/5/7/9/10/16만 받는다. 파일 동기화, 닫기와
 * 되읽기 확인이 모두 성공해야 RAM/CFX 주기를 게시한다. 실패 시 기존 RAM 유지.
 * init 실패 시 파일을 지우지 않고 현재 RAM(최초 기동 기본값)을 유지한다.
 * 이 API는 단일 FAT 파일의 전원 차단 원자성을 보장하지 않는다. */
const tdc_link_config_t *tdc_link_config_get(void);
bool tdc_link_config_valid(const tdc_link_config_t *config);
int tdc_link_config_init(void);
int tdc_link_config_set(unsigned index, unsigned value);

#endif
