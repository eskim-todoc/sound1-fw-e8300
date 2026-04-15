#ifndef UI_CMD_H__
#define UI_CMD_H__

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UI_CMD_SENTINEL      ':'
#define UI_CMD_MAX_LINE      128
#define UI_CMD_MAX_ARGC      8
#define UI_CMD_MAX_HANDLERS  32

typedef int (*ui_cmd_fn_t)(int argc, char *argv[]);

typedef struct {
    const char *name;        /* sentinel 제외. 예: "led", "batt" */
    ui_cmd_fn_t  fn;
    const char *help;        /* 한 줄 도움말 */
} ui_cmd_entry_t;

void ui_cmd_init(void);
bool ui_cmd_register(const ui_cmd_entry_t *entry);
void ui_cmd_poll(void);      /* 기존 do-while 대체 */

/* 핸들러 내부에서 사용 */
void ui_cmd_printf(const char *fmt, ...);
void ui_cmd_print_help(void);

#endif
