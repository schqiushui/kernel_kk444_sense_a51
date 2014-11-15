#ifndef __HTC_UTIL_H__
#define __HTC_UTIL_H__

void htc_pm_monitor_init(void);
void htc_monitor_init(void);
void htc_idle_stat_add(int sleep_mode, u32 time);
int htc_set_pon_reason(unsigned int is_cold_boot, int pon_idx, int warm_reset_idx, int poff_idx);

#endif
