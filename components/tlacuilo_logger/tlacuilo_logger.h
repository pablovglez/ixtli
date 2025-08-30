#ifndef TLACUILO_LOGGER_H
#define TLACUILO_LOGGER_H

#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>

void tlacuilo_log(const char *tag, const char *format, ...);

void tlacuilo_logger_start(void);
void tlacuilo_logger_stop(void);
void tlacuilo_logger_dump_to_serial(void);
void tlacuilo_logger_erase(void);
void tlacuilo_logger_get_stats(uint32_t *total, uint32_t *dropped);

#endif