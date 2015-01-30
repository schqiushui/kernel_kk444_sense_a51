/*
 * ALSA SoC TPA6130A2 amplifier driver
 *
 * Copyright (C) Nokia Corporation
 *
 * Author: Peter Ujfalusi <peter.ujfalusi@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __TPA6130A2_H__
#define __TPA6130A2_H__

#define TPA6130A2_I2C_NAME "tpa6130a2"

#define TPA6130A2_REG_CONTROL		0x01
#define TPA6130A2_REG_VOL_MUTE		0x02
#define TPA6130A2_REG_OUT_IMPEDANCE	0x03
#define TPA6130A2_REG_VERSION		0x04

#define TPA6130A2_CACHEREGNUM	(TPA6130A2_REG_VERSION + 1)

#define TPA6130A2_SWS			(0x01 << 0)
#define TPA6130A2_TERMAL		(0x01 << 1)
#define TPA6130A2_MODE(x)		(x << 4)
#define TPA6130A2_MODE_STEREO		(0x00)
#define TPA6130A2_MODE_DUAL_MONO	(0x01)
#define TPA6130A2_MODE_BRIDGE		(0x02)
#define TPA6130A2_MODE_MASK		(0x03)
#define TPA6130A2_HP_EN_R		(0x01 << 6)
#define TPA6130A2_HP_EN_L		(0x01 << 7)

#define TPA6130A2_VOLUME(x)		((x & 0x3f) << 0)
#define TPA6130A2_MUTE_R		(0x01 << 6)
#define TPA6130A2_MUTE_L		(0x01 << 7)

#define TPA6130A2_HIZ_R			(0x01 << 0)
#define TPA6130A2_HIZ_L			(0x01 << 1)

#define TPA6130A2_VERSION_MASK		(0x0f)
#define MAX_REG_DATA 15

struct amp_reg_data {
	unsigned char addr;
	unsigned char val;
};

struct amp_config {
	unsigned int reg_len;
        struct amp_reg_data reg[MAX_REG_DATA];
};

struct amp_comm_data {
	unsigned int out_mode;
        struct amp_config config;
};

struct amp_config_data {
	unsigned int mode_num;
	struct amp_comm_data *cmd_data;  
};

enum {
        TPA6130_MUTE = 0,
        TPA6130_MAX_FUNC
};

enum TPA6130_Mode {
	TPA6130_MODE_OFF = TPA6130_MAX_FUNC,
	TPA6130_MODE_PLAYBACK_BOOMSOUND_OFF,
	TPA6130_MODE_PLAYBACK_BOOMSOUND_ON,
	TPA6130_MODE_VOICE,
	TPA6130_MODE_TTY,
	TPA6130_MODE_FM,
	TPA6130_MODE_RING,
	TPA6130_MODE_MFG,
	TPA6130_MODE_NUM
};

void tpa6130a2_HSMute(int mute);


#define AMP_IOCTL_MAGIC 't'
#define TPA6130_SET_UNMUTE      _IOW(AMP_IOCTL_MAGIC, 0x04, unsigned)
#define TPA6130_SET_MUTE        _IOW(AMP_IOCTL_MAGIC, 0x03, unsigned)
#define TPA6130_SET_MODE        _IOW(AMP_IOCTL_MAGIC, 0x02, unsigned)
#define TPA6130_SET_PARAM       _IOW(AMP_IOCTL_MAGIC, 0x01,  unsigned)

#endif 
