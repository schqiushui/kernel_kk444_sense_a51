obj-y += io.o dma.o memory.o
ifndef CONFIG_ARM_ARCH_TIMER
obj-y += timer.o
endif
obj-$(CONFIG_USE_OF) += board-dt.o

obj-y += acpuclock.o
obj-y += radio_feedback.o
obj-$(CONFIG_HW_PERF_EVENTS) += perf_trace_counters.o
obj-$(CONFIG_ARCH_MSM_KRAIT) += msm-krait-l2-accessors.o perf_event_msm_krait_l2.o
obj-$(CONFIG_ARCH_MSM_KRAIT) += krait-scm.o
obj-$(CONFIG_DEBUG_FS) += perf_debug.o

obj-$(CONFIG_SMP) += headsmp.o platsmp.o
obj-$(CONFIG_HOTPLUG_CPU) += hotplug.o

obj-$(CONFIG_MSM_XPU_ERR_FATAL) += scm-xpu.o
obj-$(CONFIG_MSM_TEST_QMI_CLIENT) += kernel_test_service_v01.o test_qmi_client.o

obj-$(CONFIG_MSM_PCIE) += pcie.o pcie_irq.o pcie_phy.o

obj-$(CONFIG_MSM_DMA_TEST) += dma_test.o
obj-$(CONFIG_SURF_FFA_GPIO_KEYPAD) += keypad-surf-ffa.o

obj-$(CONFIG_MSM_BOOT_STATS) += boot_stats.o

obj-$(CONFIG_ARCH_FSM9900) += board-fsm9900.o board-fsm9900-gpiomux.o
obj-$(CONFIG_ARCH_FSM9900) += clock-fsm9900.o
obj-$(CONFIG_ARCH_FSM9900) += rfic-fsm9900.o bbif-fsm9900.o
obj-$(CONFIG_QPNP_BMS) += bms-batterydata.o bms-batterydata-desay.o
obj-$(CONFIG_QPNP_BMS) += bms-batterydata-oem.o bms-batterydata-qrd-4v35-2000mah.o bms-batterydata-qrd-4v2-1300mah.o
obj-$(CONFIG_HTC_BATT_CORE) += htc_battery_core.o
obj-$(CONFIG_HTC_BATT_8960) += htc_battery_8960.o htc_battery_cell.o
obj-$(CONFIG_ARCH_APQ8084) += board-8084.o board-8084-gpiomux.o
obj-$(CONFIG_ARCH_APQ8084) += clock-8084.o clock-mdss-8974.o
obj-$(CONFIG_ARCH_MSM8974) += board-8974.o board-8974-gpiomux.o
obj-$(CONFIG_ARCH_MSM8974) += clock-rpm-8974.o clock-gcc-8974.o clock-mmss-8974.o clock-lpass-8974.o clock-mdss-8974.o
obj-$(CONFIG_KRAIT_REGULATOR) += krait-regulator.o  krait-regulator-pmic.o
obj-$(CONFIG_ARCH_MDM9630) += board-9630.o board-9630-gpiomux.o
obj-$(CONFIG_MSM_PP2S_FEMTO) += pp2s.o
obj-$(CONFIG_ARCH_MSMSAMARIUM) += board-samarium.o board-samarium-gpiomux.o
obj-$(CONFIG_ARCH_MSMSAMARIUM) += clock-samarium.o clock-mdss-8974.o
obj-$(CONFIG_ARCH_MPQ8092) += board-8092.o board-8092-gpiomux.o
obj-$(CONFIG_ARCH_MSM8916) += board-8916.o htc_wifi_nvs.o htc_acoustic_alsa.o board-8x16-audio.o
obj-$(CONFIG_ARCH_MPQ8092) += clock-8092.o clock-vcap-8092.o clock-mdss-8974.o
obj-$(CONFIG_ARCH_MSM8226) += board-8226.o board-8226-gpiomux.o
obj-$(CONFIG_ARCH_MSM8226) += clock-8226.o clock-mdss-8974.o
obj-$(CONFIG_ARCH_MSM8610) += board-8610.o board-8610-gpiomux.o
obj-$(CONFIG_ARCH_MSM8610) += clock-8610.o
obj-$(CONFIG_ARCH_MSM8610) += clock-dsi-8610.o
obj-$(CONFIG_ARCH_MDM9630) += clock-9630.o
obj-$(CONFIG_ARCH_MSMZIRC) += board-zirc.o
obj-$(CONFIG_MSM_PM) += ext-buck-control.o

ifdef CONFIG_BT
  obj-$(CONFIG_ARCH_MSM8916) += htc_bdaddress.o
endif #CONFIG_BT

CFLAGS_msm_vibrator.o += -Idrivers/staging/android

obj-$(CONFIG_MSM_LPM_TEST) += test-lpm.o
obj-$(CONFIG_MSM_TZ_LOG) += tz_log.o
obj-$(CONFIG_MSM_BUSPM_DEV) += msm-buspm-dev.o


obj-$(CONFIG_ARCH_MSM8974) += gpiomux-v2.o gpiomux.o
obj-$(CONFIG_ARCH_MDM9630) += gpiomux-v2.o gpiomux.o
obj-$(CONFIG_ARCH_MSMSAMARIUM) += gpiomux-v2.o gpiomux.o
obj-$(CONFIG_ARCH_MPQ8092) += gpiomux-v2.o gpiomux.o
obj-$(CONFIG_ARCH_MSM8226) += gpiomux-v2.o gpiomux.o
obj-$(CONFIG_ARCH_MSM8610) += gpiomux-v2.o gpiomux.o
obj-$(CONFIG_ARCH_APQ8084) += gpiomux-v2.o gpiomux.o
obj-$(CONFIG_ARCH_FSM9900) += gpiomux-v2.o gpiomux.o

obj-$(CONFIG_MSM_CACHE_ERP) += cache_erp.o
obj-$(CONFIG_MSM_CACHE_DUMP) += msm_cache_dump.o

obj-$(CONFIG_MSM_HSIC_SYSMON) += hsic_sysmon.o
obj-$(CONFIG_MSM_HSIC_SYSMON_TEST) += hsic_sysmon_test.o

obj-$(CONFIG_MSM_RPC_USB) += rpc_hsusb.o rpc_fsusb.o

obj-$(CONFIG_MEMORY_HOLE_CARVEOUT) +=  msm_mem_hole.o

obj-$(CONFIG_MSM_SMCMOD) += smcmod.o

obj-$(CONFIG_ARCH_MSM8974) += msm_mpmctr.o

obj-$(CONFIG_ARCH_RANDOM) += early_random.o
obj-$(CONFIG_PERFMAP) += perfmap.o

obj-$(CONFIG_ARCH_MSM8916) += board-htc-8916.o
obj-$(CONFIG_ARCH_MSM8916) += devices_cmdline.o devices_dtb.o
obj-$(CONFIG_HTC_DEBUG_FOOTPRINT) += htc_mnemosyne.o
obj-$(CONFIG_HTC_DEBUG_FOOTPRINT) += htc_footprint.o
obj-$(CONFIG_HTC_RPM_CMD) += rpm_htc_cmd.o
obj-$(CONFIG_ARCH_MSM8916) += htc_awb_cal.o

obj-$(CONFIG_HTC_FEATURES_RIL_PCN0003_HTC_MONITOR) += htc_monitor.o
obj-$(CONFIG_HTC_FEATURES_RIL_PCN0004_HTC_GARBAGE_FILTER) += htc_port_list.o

obj-$(CONFIG_MSM_SCM) +=htc_sdservice.o htc_drm.o htc_rmtmsg.o htc_debug.o

obj-y += display/

obj-y += htc_sysinfo.o
