#ifndef TDC_LINK_CONTROL_H
#define TDC_LINK_CONTROL_H

#include <stdbool.h>

/* Foreground-only controls. A force ACK accepts a setting; the next feedback
 * event applies it. The level getter reports successful PMIC I/O, not a request. */
int tdc_link_control_get_level(void);
int tdc_link_control_get_force(void);
int tdc_link_control_get_infinite(void);
int tdc_link_control_get_nop(void);
int tdc_link_control_get_override(void);
bool tdc_link_control_set_force(int level);
void tdc_link_control_set_infinite(int enabled);
bool tdc_link_control_set_nop(int count);
void tdc_link_control_set_override(int enabled);
void tdc_link_control_map_changed(int frame);

/* Common PMIC success boundary and lifecycle. Reset invalidates the observation
 * but does not forget that this boot has already connected to an internal unit. */
void tdc_link_control_note_level(int level);
void tdc_link_control_invalidate_level(void);
void tdc_link_control_connected(void);
int tdc_link_control_effective_max(void);
int tdc_link_control_period_ms(void);
void tdc_link_control_reset_feedback(void);

/* false means hardware failure or disconnection; caller must release its I2C
 * ownership and must not overwrite the recovery state with later operations. */
bool tdc_link_control_feedback(int power_state, int current, bool mapping, bool dedicated);

#endif
