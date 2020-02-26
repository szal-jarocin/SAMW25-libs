#ifndef _TIMER_SAMW_H_
#define _TIMER_SAMW_H_

#include <tc.h>

#define CONF_TC_UART TC3
#define CONF_TC_TIMEOUT TC4
#define CONF_TC_MEASURE	TC5

//! [module_inst]
struct tc_module tc_instance_uart;
struct tc_module tc_instance_timeout;
struct tc_module tc_instance_aws;
struct tc_module tc_instance_measure;
//! [module_inst]

void configure_timer_uart(void);
void configure_timer_timeout(void);
void configure_timer_AWS(void);
void configure_timer_measure(void);
void timer_AWS_enable(void);
void timer_AWS_disable(void);
void timer_measure_enable(void);
void timer_measure_disable(void);
void configure_tc_callbacks(void);
void timer_init(void);
void timer_disable(void);
extern void tc_callback_read_usart(struct tc_module *const module_inst);
void tc_callback_increment_timeout(struct tc_module *const module_inst);
void tc_callback_aws(struct tc_module *const module_inst);
void tc_callback_measure(struct tc_module *const module_inst);

#endif /* _TIMER_SAMW_H_ */