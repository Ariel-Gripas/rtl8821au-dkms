#include <rtl8812a_hal.h>
#include "rf.h"
#include "reg.h"
#include "phy.h"
#include "dm.h"

#undef RT_TRACE
static inline void RT_TRACE(struct rtl_priv *rtlpriv,
			    int comp, int level,
			    const char *fmt, ...)
{
}

void rtl8821au_phy_rf6052_set_bandwidth(struct rtl_priv *rtlpriv, enum CHANNEL_WIDTH	Bandwidth)	/* 20M or 40M */
{
	struct _rtw_hal	*pHalData = GET_HAL_DATA(rtlpriv);

	switch (Bandwidth) {
	case CHANNEL_WIDTH_20:
		rtw_hal_write_rfreg(rtlpriv, RF90_PATH_A, RF_CHNLBW_Jaguar, BIT11|BIT10, 3);
		rtw_hal_write_rfreg(rtlpriv, RF90_PATH_B, RF_CHNLBW_Jaguar, BIT11|BIT10, 3);
		break;
	case CHANNEL_WIDTH_40:
		rtw_hal_write_rfreg(rtlpriv, RF90_PATH_A, RF_CHNLBW_Jaguar, BIT11|BIT10, 1);
		rtw_hal_write_rfreg(rtlpriv, RF90_PATH_B, RF_CHNLBW_Jaguar, BIT11|BIT10, 1);
		break;

	case CHANNEL_WIDTH_80:
		rtw_hal_write_rfreg(rtlpriv, RF90_PATH_A, RF_CHNLBW_Jaguar, BIT11|BIT10, 0);
		rtw_hal_write_rfreg(rtlpriv, RF90_PATH_B, RF_CHNLBW_Jaguar, BIT11|BIT10, 0);
		break;

	default:
		dev_info(&(rtlpriv->ndev->dev), "rtl8821au_phy_rf6052_set_bandwidth(): unknown Bandwidth: %#X\n", Bandwidth);
		break;
	}
}

static void writeOFDMPowerReg8812(
	IN		struct rtl_priv *rtlpriv,
	IN		uint8_t		index,
	IN 		u32*		pValue
	)
{
	u16 RegOffset_A[6] = {
    	RTXAGC_A_OFDM18_OFDM6,
        RTXAGC_A_OFDM54_OFDM24,
        RTXAGC_A_MCS03_MCS00,
        RTXAGC_A_MCS07_MCS04,
        RTXAGC_A_MCS11_MCS08,
        RTXAGC_A_MCS15_MCS12
    };
	u16 RegOffset_B[6] = {
        RTXAGC_B_OFDM18_OFDM6,
        RTXAGC_B_OFDM54_OFDM24,
        RTXAGC_B_MCS03_MCS00,
        RTXAGC_B_MCS07_MCS04,
        RTXAGC_B_MCS11_MCS08,
        RTXAGC_B_MCS15_MCS12
    };

	uint8_t	i, rf, pwr_val[4];
	uint32_t	writeVal;
	u16	RegOffset;

	for(rf=0; rf<2; rf++)
	{
		writeVal = pValue[rf];
		for(i=0; i<RF_PATH_MAX_92C_88E; i++)
		{
			pwr_val[i] = (uint8_t)((writeVal & (0x7f<<(i*8)))>>(i*8));
			if (pwr_val[i]  > RF6052_MAX_TX_PWR)
				pwr_val[i]  = RF6052_MAX_TX_PWR;
		}
		writeVal = (pwr_val[3]<<24) | (pwr_val[2]<<16) |(pwr_val[1]<<8) |pwr_val[0];

		if(rf == 0)
			RegOffset = RegOffset_A[index];
		else
			RegOffset = RegOffset_B[index];
		rtl_set_bbreg(rtlpriv, RegOffset, bMaskDWord, writeVal);
		//RTPRINT(FPHY, PHY_TXPWR, ("Set 0x%x = %08x\n", RegOffset, writeVal));
	}
}

//
// powerbase0 for OFDM rates
// powerbase1 for HT MCS rates
//
void getPowerBase8812(
	IN	struct rtl_priv *rtlpriv,
	IN	uint8_t *			pPowerLevelOFDM,
	IN	uint8_t *			pPowerLevelBW20,
	IN	uint8_t *			pPowerLevelBW40,
	IN	uint8_t			Channel,
	IN OUT u32*		OfdmBase,
	IN OUT u32*		MCSBase
	)
{
	uint32_t			powerBase0, powerBase1;
	uint8_t			i, powerlevel[2];

	for(i=0; i<2; i++)
	{
		powerBase0 = pPowerLevelOFDM[i];

		powerBase0 = (powerBase0<<24) | (powerBase0<<16) |(powerBase0<<8) |powerBase0;
		*(OfdmBase+i) = powerBase0;
		//DBG_871X(" [OFDM power base index rf(%c) = 0x%x]\n", ((i==0)?'A':'B'), *(OfdmBase+i));
	}

	for(i=0; i< rtlpriv->phy.num_total_rfpath; i++)
	{
		//Check HT20 to HT40 diff
		if(rtlpriv->phy.current_chan_bw == CHANNEL_WIDTH_20)
		{
			powerlevel[i] = pPowerLevelBW20[i];
		}
		else
		{
			powerlevel[i] = pPowerLevelBW40[i];
		}
		powerBase1 = powerlevel[i];
		powerBase1 = (powerBase1<<24) | (powerBase1<<16) |(powerBase1<<8) |powerBase1;
		*(MCSBase+i) = powerBase1;
		//DBG_871X(" [MCS power base index rf(%c) = 0x%x]\n", ((i==0)?'A':'B'), *(MCSBase+i));
	}
}

void getTxPowerWriteValByRegulatory8812(
	IN		struct rtl_priv *rtlpriv,
	IN		uint8_t			Channel,
	IN		uint8_t			index,
	IN		u32*		powerBase0,
	IN		u32*		powerBase1,
	OUT		u32*		pOutWriteVal
	)
{
	struct rtl_phy *rtlphy = &(rtlpriv->phy);
	struct rtl_efuse *efuse = rtl_efuse(rtlpriv);
	uint8_t			i, chnlGroup=0, pwr_diff_limit[4], customer_pwr_limit;
	s8			pwr_diff=0;
	uint32_t 			writeVal, customer_limit, rf;
	uint8_t			Regulatory = efuse->eeprom_regulatory;

	//
	// Index 0 & 1= legacy OFDM, 2-5=HT_MCS rate
	//

	for(rf=0; rf<2; rf++)
	{
		switch(Regulatory)
		{
			case 0:	// Realtek better performance
					// increase power diff defined by Realtek for large power
				chnlGroup = 0;
				//RTPRINT(FPHY, PHY_TXPWR, ("MCSTxPowerLevelOriginalOffset[%d][%d] = 0x%x\n",
				//	chnlGroup, index, pHalData->MCSTxPowerLevelOriginalOffset[chnlGroup][index+(rf?8:0)]));
				writeVal = rtlphy->mcs_txpwrlevel_origoffset[chnlGroup][index+(rf?8:0)] +
					((index<2)?powerBase0[rf]:powerBase1[rf]);
				//RTPRINT(FPHY, PHY_TXPWR, ("RTK better performance, writeVal(%c) = 0x%x\n", ((rf==0)?'A':'B'), writeVal));
				break;
			case 1:	// Realtek regulatory
					// increase power diff defined by Realtek for regulatory
				{
					if(rtlphy->pwrgroup_cnt == 1)
						chnlGroup = 0;
					//if(pHalData->pwrGroupCnt >= pHalData->PGMaxGroup)
					{
						if (Channel < 3)			// Chanel 1-2
							chnlGroup = 0;
						else if (Channel < 6)		// Channel 3-5
							chnlGroup = 1;
						else	 if(Channel <9)		// Channel 6-8
							chnlGroup = 2;
						else if(Channel <12)		// Channel 9-11
							chnlGroup = 3;
						else if(Channel <14)		// Channel 12-13
							chnlGroup = 4;
						else if(Channel ==14)		// Channel 14
							chnlGroup = 5;

/*
						if(Channel <= 3)
							chnlGroup = 0;
						else if(Channel >= 4 && Channel <= 9)
							chnlGroup = 1;
						else if(Channel > 9)
							chnlGroup = 2;


						if(pHalData->CurrentChannelBW == CHANNEL_WIDTH_20)
							chnlGroup++;
						else
							chnlGroup+=4;
*/
					}
					//RTPRINT(FPHY, PHY_TXPWR, ("MCSTxPowerLevelOriginalOffset[%d][%d] = 0x%x\n",
					//chnlGroup, index, pHalData->MCSTxPowerLevelOriginalOffset[chnlGroup][index+(rf?8:0)]));
					writeVal = rtlphy->mcs_txpwrlevel_origoffset[chnlGroup][index+(rf?8:0)] +
							((index<2)?powerBase0[rf]:powerBase1[rf]);
					//RTPRINT(FPHY, PHY_TXPWR, ("Realtek regulatory, 20MHz, writeVal(%c) = 0x%x\n", ((rf==0)?'A':'B'), writeVal));
				}
				break;
			case 2:	// Better regulatory
					// don't increase any power diff
				writeVal = ((index<2)?powerBase0[rf]:powerBase1[rf]);
				//RTPRINT(FPHY, PHY_TXPWR, ("Better regulatory, writeVal(%c) = 0x%x\n", ((rf==0)?'A':'B'), writeVal));
				break;
			default:
				chnlGroup = 0;
				writeVal = rtlphy->mcs_txpwrlevel_origoffset[chnlGroup][index+(rf?8:0)] +
						((index<2)?powerBase0[rf]:powerBase1[rf]);
				//RTPRINT(FPHY, PHY_TXPWR, ("RTK better performance, writeVal rf(%c) = 0x%x\n", ((rf==0)?'A':'B'), writeVal));
				break;
		}

// 20100427 Joseph: Driver dynamic Tx power shall not affect Tx power. It shall be determined by power training mechanism.
// Currently, we cannot fully disable driver dynamic tx power mechanism because it is referenced by BT coexist mechanism.
// In the future, two mechanism shall be separated from each other and maintained independantly. Thanks for Lanhsin's reminder.
		//92d do not need this
		if(rtlpriv->dm.dynamic_txhighpower_lvl == TxHighPwrLevel_Level1)
			writeVal = 0x14141414;
		else if(rtlpriv->dm.dynamic_txhighpower_lvl == TxHighPwrLevel_Level2)
			writeVal = 0x00000000;

		// 20100628 Joseph: High power mode for BT-Coexist mechanism.
		// This mechanism is only applied when Driver-Highpower-Mechanism is OFF.
		if(rtlpriv->dm.dynamic_txhighpower_lvl == TxHighPwrLevel_BT1)
		{
			//RTPRINT(FBT, BT_TRACE, ("Tx Power (-6)\n"));
			writeVal = writeVal - 0x06060606;
		}
		else if(rtlpriv->dm.dynamic_txhighpower_lvl == TxHighPwrLevel_BT2)
		{
			//RTPRINT(FBT, BT_TRACE, ("Tx Power (-0)\n"));
			writeVal = writeVal ;
		}
		/*
		if(pMgntInfo->bDisableTXPowerByRate)
		{
		// add for  OID_RT_11N_TX_POWER_BY_RATE ,disable tx powre change by rate
			writeVal = 0x2c2c2c2c;
		}
		*/
		*(pOutWriteVal+rf) = writeVal;
	}
}

void rtl8821au_phy_rf6052_set_ofdm_txpower(struct rtl_priv *rtlpriv,
	IN	uint8_t *		pPowerLevelOFDM,
	IN	uint8_t *		pPowerLevelBW20,
	IN	uint8_t *		pPowerLevelBW40,
	IN	uint8_t		Channel)
{
	uint32_t writeVal[2], powerBase0[2], powerBase1[2], pwrtrac_value;
	uint8_t index = 0;


	//DBG_871X("PHY_RF6052SetOFDMTxPower, channel(%d) \n", Channel);

	getPowerBase8812(rtlpriv, pPowerLevelOFDM,pPowerLevelBW20,pPowerLevelBW40, Channel, &powerBase0[0], &powerBase1[0]);

	for(index=0; index<6; index++)
	{
		getTxPowerWriteValByRegulatory8812(rtlpriv, Channel, index,
			&powerBase0[0], &powerBase1[0], &writeVal[0]);

		writeOFDMPowerReg8812(rtlpriv, index, &writeVal[0]);
	}
}

/*
 * If you want to add a new IC, Please follow below template and generate a new one.
 */

void ODM_ConfigRFWithHeaderFile(struct rtl_priv *rtlpriv,
	ODM_RF_Config_Type ConfigType, enum radio_path eRFPath)
{
	struct rtl_hal	*rtlhal = rtl_hal(rtlpriv);

	RT_TRACE(rtlpriv, ODM_COMP_INIT, ODM_DBG_LOUD,
		"===>ODM_ConfigRFWithHeaderFile (%s)\n", (IS_NORMAL_CHIP(rtlpriv->VersionID)) ? "MPChip" : "TestChip");
	RT_TRACE(rtlpriv, ODM_COMP_INIT, ODM_DBG_LOUD,
		"pDM_Odm->SupportInterface: 0x%X, pDM_Odm->BoardType: 0x%X\n",
		rtlhal->interface, rtlhal->board_type);

	if (IS_HARDWARE_TYPE_8812AU(rtlhal))
		rtl8812au_phy_config_rf_with_headerfile(rtlpriv, eRFPath);

	if (IS_HARDWARE_TYPE_8821U(rtlhal)) {
		rtl8821au_phy_config_rf_with_headerfile(rtlpriv, RF90_PATH_A);
	}
}



static int _rtl8821au_phy_rf6052_config_parafile(struct rtl_priv *rtlpriv)
{
	uint8_t	eRFPath;
	int	rtStatus = _SUCCESS;
	struct _rtw_hal *pHalData = GET_HAL_DATA(rtlpriv);

	/*
	 * -----------------------------------------------------------------
	 * <2> Initialize RF
	 * -----------------------------------------------------------------
	 */
	/* for(eRFPath = RF90_PATH_A; eRFPath <pHalData->NumTotalRFPath; eRFPath++) */
	for (eRFPath = 0; eRFPath <  rtlpriv->phy.num_total_rfpath; eRFPath++) {
		/* ----Initialize RF fom connfiguration file---- */
		ODM_ConfigRFWithHeaderFile(rtlpriv, CONFIG_RF_RADIO, (enum radio_path)eRFPath);

	}


	/* RT_TRACE(COMP_INIT, DBG_LOUD, ("<---phy_RF6052_Config_ParaFile_8812()\n")); */

	return rtStatus;
}

int rtl8821au_phy_rf6052_config(struct rtl_priv *rtlpriv)
{
	int		rtStatus = _SUCCESS;

	/* Initialize general global value */
	if (rtlpriv->phy.rf_type == RF_1T1R)
		rtlpriv->phy.num_total_rfpath = 1;
	else
		rtlpriv->phy.num_total_rfpath = 2;

	/*
	 * Config BB and RF
	 */

	rtStatus = _rtl8821au_phy_rf6052_config_parafile(rtlpriv);

	return rtStatus;

}

/* currently noz used, as in rtl8821ae */

void rtl8821au_phy_rf6052_set_cck_txpower(struct rtl_priv *rtlpriv, uint8_t *pPowerlevel)
{
	struct rtl_phy *rtlphy = &(rtlpriv->phy);
	struct rtl_efuse *efuse = rtl_efuse(rtlpriv);
	struct rtl_hal *rtlhal = rtl_hal(rtlpriv);
	struct mlme_ext_priv 	*pmlmeext = &rtlpriv->mlmeextpriv;
	uint32_t		TxAGC[2] = {0, 0},
				tmpval = 0;
	BOOLEAN	TurboScanOff = _FALSE;
	uint8_t	idx1, idx2;
	uint8_t *ptr;

	/* FOR CE ,must disable turbo scan */
	TurboScanOff = _TRUE;

	if (pmlmeext->sitesurvey_res.state == SCAN_PROCESS) {
		TxAGC[RF90_PATH_A] = 0x3f3f3f3f;
		TxAGC[RF90_PATH_B] = 0x3f3f3f3f;

		TurboScanOff = _TRUE;	/* disable turbo scan */

		if (TurboScanOff) {
			for (idx1 = RF90_PATH_A; idx1 <= RF90_PATH_B; idx1++) {
				TxAGC[idx1] =
					pPowerlevel[idx1] | (pPowerlevel[idx1]<<8) |
					(pPowerlevel[idx1]<<16) | (pPowerlevel[idx1]<<24);
				/* 2010/10/18 MH For external PA module. We need to limit power index to be less than 0x20. */
				if (TxAGC[idx1] > 0x20 && rtlhal->external_pa_5g)
					TxAGC[idx1] = 0x20;
			}
		}
	} else {
/*
 * 20100427 Joseph: Driver dynamic Tx power shall not affect Tx power. It shall be determined by power training mechanism.
 * Currently, we cannot fully disable driver dynamic tx power mechanism because it is referenced by BT coexist mechanism.
 * In the future, two mechanism shall be separated from each other and maintained independantly. Thanks for Lanhsin's reminder.
 */
		if (rtlpriv->dm.dynamic_txhighpower_lvl == TxHighPwrLevel_Level1) {
			TxAGC[RF90_PATH_A] = 0x10101010;
			TxAGC[RF90_PATH_B] = 0x10101010;
		} else if (rtlpriv->dm.dynamic_txhighpower_lvl == TxHighPwrLevel_Level2) {
			TxAGC[RF90_PATH_A] = 0x00000000;
			TxAGC[RF90_PATH_B] = 0x00000000;
		} else {
			for (idx1 = RF90_PATH_A; idx1 <= RF90_PATH_B; idx1++) {
				TxAGC[idx1] =
					pPowerlevel[idx1] | (pPowerlevel[idx1]<<8) |
					(pPowerlevel[idx1]<<16) | (pPowerlevel[idx1]<<24);
			}

			if (efuse->eeprom_regulatory == 0) {
				tmpval = (rtlphy->mcs_txpwrlevel_origoffset[0][6]) +
						(rtlphy->mcs_txpwrlevel_origoffset[0][7]<<8);
				TxAGC[RF90_PATH_A] += tmpval;

				tmpval = (rtlphy->mcs_txpwrlevel_origoffset[0][14]) +
						(rtlphy->mcs_txpwrlevel_origoffset[0][15]<<24);
				TxAGC[RF90_PATH_B] += tmpval;
			}
		}
	}

	for (idx1 = RF90_PATH_A; idx1 <= RF90_PATH_B; idx1++) {
		ptr = (uint8_t *)(&(TxAGC[idx1]));
		for (idx2 = 0; idx2 < 4; idx2++) {
			if (*ptr > RF6052_MAX_TX_PWR)
				*ptr = RF6052_MAX_TX_PWR;
			ptr++;
		}
	}

	/* rf-A cck tx power */
	tmpval = TxAGC[RF90_PATH_A]&0xff;
	rtl_set_bbreg(rtlpriv, RTXAGC_A_CCK11_CCK1, MASKBYTE1, tmpval);
	/* RT_DISP(FPHY, PHY_TXPWR, ("CCK PWR 1M (rf-A) = 0x%x (reg 0x%x)\n", tmpval, rTxAGC_A_CCK1_Mcs32)); */
	tmpval = TxAGC[RF90_PATH_A]>>8;
	rtl_set_bbreg(rtlpriv, RTXAGC_A_CCK11_CCK1, 0xffffff00, tmpval);
	/* RT_DISP(FPHY, PHY_TXPWR, ("CCK PWR 2~11M (rf-A) = 0x%x (reg 0x%x)\n", tmpval, rTxAGC_B_CCK11_A_CCK2_11)); */

	/* rf-B cck tx power */
	tmpval = TxAGC[RF90_PATH_B]>>24;
	rtl_set_bbreg(rtlpriv, RTXAGC_B_CCK11_CCK1, MASKBYTE0, tmpval);
	/* RT_DISP(FPHY, PHY_TXPWR, ("CCK PWR 11M (rf-B) = 0x%x (reg 0x%x)\n", tmpval, rTxAGC_B_CCK11_A_CCK2_11)); */
	tmpval = TxAGC[RF90_PATH_B]&0x00ffffff;
	rtl_set_bbreg(rtlpriv, RTXAGC_B_CCK11_CCK1, 0xffffff00, tmpval);
	/* RT_DISP(FPHY, PHY_TXPWR, ("CCK PWR 1~5.5M (rf-B) = 0x%x (reg 0x%x)\n", tmpval, rTxAGC_B_CCK1_55_Mcs32)); */

}	/* PHY_RF6052SetCckTxPower */


