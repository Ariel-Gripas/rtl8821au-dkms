/******************************************************************************
 *
 * Copyright(c) 2007 - 2011 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/
#ifndef __INC_HAL8812PHYCFG_H__
#define __INC_HAL8812PHYCFG_H__


/*--------------------------Define Parameters-------------------------------*/
#define LOOP_LIMIT				5
#define MAX_STALL_TIME			50		//us
#define AntennaDiversityValue	0x80	//(rtlpriv->bSoftwareAntennaDiversity ? 0x00:0x80)
#define MAX_TXPWR_IDX_NMODE_92S	63
#define Reset_Cnt_Limit			3


#define MAX_AGGR_NUM	0x07


/*--------------------------Define Parameters-------------------------------*/

/*------------------------------Define structure----------------------------*/


/* BB/RF related */

/*------------------------------Define structure----------------------------*/


/*------------------------Export global variable----------------------------*/
/*------------------------Export global variable----------------------------*/


/*------------------------Export Marco Definition---------------------------*/
/*------------------------Export Marco Definition---------------------------*/


/*--------------------------Exported Function prototype---------------------*/
//
// BB and RF register read/write
//
//
// Initialization related function
//
/* MAC/BB/RF HAL config */
int	PHY_BBConfig8812(IN struct rtl_priv *rtlpriv	);
void _rtl8812au_bb8812_config_1t(struct rtl_priv *rtlpriv);
int	PHY_RFConfig8812(IN struct rtl_priv *rtlpriv	);


/* RF config */

//
// BB TX Power R/W
//

void	PHY_SetTxPowerLevel8812(	IN struct rtl_priv *rtlpriv, IN uint8_t	Channel	);
u8 _rtl8821au_get_txpower_index(
	IN	struct rtl_priv *		rtlpriv,
	IN	uint8_t					RFPath,
	IN	uint8_t					Rate,
	IN	enum CHANNEL_WIDTH		BandWidth,
	IN	uint8_t					Channel
	);

u32 PHY_GetTxBBSwing_8812A(struct rtl_priv *rtlpriv, enum band_type Band, uint8_t RFPath
	);

//
// Switch bandwidth for 8192S
//
void
PHY_SetBWMode8812(
	IN	struct rtl_priv *		rtlpriv,
	IN	enum CHANNEL_WIDTH		Bandwidth,
	IN	uint8_t					Offset
);

//
// channel switch related funciton
//
void
PHY_SwChnl8812(
	IN	struct rtl_priv *rtlpriv,
	IN	uint8_t			channel
);


void
PHY_SetSwChnlBWMode8812(
	IN	struct rtl_priv *		rtlpriv,
	IN	uint8_t					channel,
	IN	enum CHANNEL_WIDTH		Bandwidth,
	IN	uint8_t					Offset40,
	IN	uint8_t					Offset80
);

//
// BB/MAC/RF other monitor API
//

void
storePwrIndexDiffRateOffset(
	IN	struct rtl_priv *rtlpriv,
	IN	u32		RegAddr,
	IN	u32		BitMask,
	IN	u32		Data
	);

/*--------------------------Exported Function prototype---------------------*/
#endif	// __INC_HAL8192CPHYCFG_H

