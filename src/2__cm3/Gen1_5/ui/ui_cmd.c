#include "ui_cmd.h"

#include <stdarg.h>
#include <string.h>

#include <SEGGER_RTT.h>

/* ======================================================================== */
/*  Static state                                                            */
/* ======================================================================== */

static ui_cmd_entry_t s_table_[UI_CMD_MAX_HANDLERS];
static uint8_t        s_table_n_;

static char    line_[UI_CMD_MAX_LINE];
static uint8_t line_len_;

/* ======================================================================== */
/*  Internal helpers                                                        */
/* ======================================================================== */

static void ui_cmd_echo_ch(char ch)
{
    SEGGER_RTT_Write(0, &ch, 1);
}

static void ui_cmd_echo(const char *s)
{
    SEGGER_RTT_Write(0, s, (unsigned) strlen(s));
}

static void ui_cmd_prompt(void)
{
    ui_cmd_echo("> ");
}

/* ======================================================================== */
/*  Tokenizer                                                               */
/* ======================================================================== */

static int ui_cmd_tokenize(char *line, char *argv[], int max_argc)
{
    int argc = 0;
    char *p  = line;

    while (*p && argc < max_argc)
    {
        /* skip whitespace */
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '\0')
            break;

        argv[argc++] = p;

        /* advance to next whitespace or end */
        while (*p && *p != ' ' && *p != '\t')
            p++;
        if (*p)
            *p++ = '\0';
    }

    return argc;
}

/* ======================================================================== */
/*  Dispatch                                                                */
/* ======================================================================== */

static void ui_cmd_dispatch(char *line)
{
    /* sentinel check */
    if (line[0] != UI_CMD_SENTINEL)
    {
        ui_cmd_printf("unknown, type ':help'\r\n");
        return;
    }

    /* skip sentinel */
    char *cmd_line = &line[1];

    char *argv[UI_CMD_MAX_ARGC];
    int   argc = ui_cmd_tokenize(cmd_line, argv, UI_CMD_MAX_ARGC);

    if (argc == 0)
    {
        ui_cmd_printf("unknown, type ':help'\r\n");
        return;
    }

    /* linear search */
    for (int i = 0; i < s_table_n_; i++)
    {
        if (strcmp(argv[0], s_table_[i].name) == 0)
        {
            int ret = s_table_[i].fn(argc, argv);
            if (ret < 0)
            {
                ui_cmd_printf("  usage: %s\r\n", s_table_[i].help);
            }
            return;
        }
    }

    ui_cmd_printf("unknown cmd '%s', type ':help'\r\n", argv[0]);
}

static void ui_cmd_dispatch_literal(const char *literal)
{
    char buf[UI_CMD_MAX_LINE];
    int  len = (int) strlen(literal);

    if (len >= UI_CMD_MAX_LINE - 1)
        len = UI_CMD_MAX_LINE - 2;

    buf[0] = UI_CMD_SENTINEL;
    memcpy(&buf[1], literal, len);
    buf[len + 1] = '\0';

    ui_cmd_dispatch(buf);
}

/* ======================================================================== */
/*  Built-in :help handler                                                  */
/* ======================================================================== */

static int ui_cmd_help_handler(int argc, char *argv[])
{
    (void) argc;
    (void) argv;
    ui_cmd_print_help();
    return 0;
}

/* ======================================================================== */
/*  Public API                                                              */
/* ======================================================================== */

void ui_cmd_init(void)
{
    s_table_n_ = 0;
    line_len_  = 0;

    /* register built-in :help */
    static const ui_cmd_entry_t help_entry = {"help", ui_cmd_help_handler, ":help  -- show all commands"};
    ui_cmd_register(&help_entry);

    ui_cmd_printf("\r\n[UI CMD] ready. type ':help'\r\n");
    ui_cmd_prompt();
}

bool ui_cmd_register(const ui_cmd_entry_t *entry)
{
    if (s_table_n_ >= UI_CMD_MAX_HANDLERS)
        return false;

    s_table_[s_table_n_] = *entry;
    s_table_n_++;
    return true;
}

void ui_cmd_poll(void)
{
    char ch;

    while (SEGGER_RTT_Read(0, &ch, 1) > 0)
    {
        if (ch == '\r' || ch == '\n')
        {
            ui_cmd_echo("\r\n");

            if (line_len_ == 0)
            {
                /* 빈 라인 → :led show 자동 */
                ui_cmd_dispatch_literal("led show");
            }
            else
            {
                line_[line_len_] = '\0';
                ui_cmd_dispatch(line_);
                line_len_ = 0;
            }
            ui_cmd_prompt();
        }
        else if (ch == 0x08 || ch == 0x7F) /* BS / DEL */
        {
            if (line_len_ > 0)
            {
                line_len_--;
                ui_cmd_echo("\b \b");
            }
        }
        else if (line_len_ < UI_CMD_MAX_LINE - 1)
        {
            line_[line_len_++] = ch;
            ui_cmd_echo_ch(ch);
        }
    }
}

void ui_cmd_printf(const char *fmt, ...)
{
    char    buf[256];
    va_list ap;

    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (n > 0)
    {
        if (n > (int) sizeof(buf) - 1)
            n = (int) sizeof(buf) - 1;
        SEGGER_RTT_Write(0, buf, (unsigned) n);
    }
}

void ui_cmd_print_help(void)
{
    ui_cmd_printf("--- commands ---\r\n");
    for (int i = 0; i < s_table_n_; i++)
    {
        ui_cmd_printf("  :%s\t%s\r\n", s_table_[i].name, s_table_[i].help);
    }
    ui_cmd_printf("----------------\r\n");
}
