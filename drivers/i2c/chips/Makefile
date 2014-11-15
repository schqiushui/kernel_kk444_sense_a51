#
# Makefile for miscellaneous I2C chip drivers.
#
obj-$(CONFIG_SENSORS_NFC_PN544) += pn544.o
obj-$(CONFIG_TI_TCA6418)      += tca6418_ioexpander.o
obj-$(CONFIG_BOSCH_BMA250)      += bma250_bosch.o
obj-$(CONFIG_KIONIX_KXTJ2)      += kxtj2.o
obj-$(CONFIG_YAMAHA_YAS532)      += yas532.o
obj-$(CONFIG_AMP_TFA9887)       += tfa9887.o
obj-$(CONFIG_AMP_TFA9887L)      += tfa9887l.o
obj-$(CONFIG_SENSORS_AK8963_DOE_PLUS)    += akm8963_doe_plus.o
obj-$(CONFIG_BOSCH_BMA250)      += bma250_bosch.o
obj-$(CONFIG_AMP_TPA6130A2)       += tpa6130a2.o
obj-$(CONFIG_INPUT_CWSTM32)    += CwMcuSensor.o
CFLAGS_CwMcuSensor.o := -DDEBUG

ifeq ($(CONFIG_INPUT_CAPELLA_CM36686),y)
proximity-objs := cm36686.o
obj-$(CONFIG_INPUT_CAPELLA_CM36686)      += proximity.o
endif

ifeq ($(CONFIG_I2C_DEBUG_CHIP),y)
EXTRA_CFLAGS += -DDEBUG
endif

