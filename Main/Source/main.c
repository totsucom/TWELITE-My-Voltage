/*
 * １秒毎に電源電圧を測定してシリアルに表示
 */

#include <AppHardwareApi.h>
#include "utils.h"
#include "ToCoNet.h"
#include "serial.h"
#include "string.h"
#include "sprintf.h"


#define UART_BAUD 115200 	        // シリアルのボーレート
static tsFILE sSerStream;           // シリアル用ストリーム
static tsSerialPortSetup sSerPort;  // シリアルポートデスクリプタ

// シリアルにメッセージを出力する
#define debug(...) vfPrintf(&sSerStream, LB __VA_ARGS__)


// デバッグ出力用に UART を初期化
static void vSerialInit() {
    static uint8 au8SerialTxBuffer[96];
    static uint8 au8SerialRxBuffer[32];

    sSerPort.pu8SerialRxQueueBuffer = au8SerialRxBuffer;
    sSerPort.pu8SerialTxQueueBuffer = au8SerialTxBuffer;
    sSerPort.u32BaudRate = UART_BAUD;
    sSerPort.u16AHI_UART_RTS_LOW = 0xffff;
    sSerPort.u16AHI_UART_RTS_HIGH = 0xffff;
    sSerPort.u16SerialRxQueueSize = sizeof(au8SerialRxBuffer);
    sSerPort.u16SerialTxQueueSize = sizeof(au8SerialTxBuffer);
    sSerPort.u8SerialPort = E_AHI_UART_0;
    sSerPort.u8RX_FIFO_LEVEL = E_AHI_UART_FIFO_LEVEL_1;
    SERIAL_vInit(&sSerPort);

    sSerStream.bPutChar = SERIAL_bTxChar;
    sSerStream.u8Device = E_AHI_UART_0;
}

// ユーザ定義のイベントハンドラ
// ウィンドウズのwndProc()みたいなもん
// 比較的重めの処理を書いてもいいけどブロックしてはいけません
static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg)
{
    if(eEvent == E_EVENT_TICK_SECOND) {

        // 1) アナログ部の電源投入
        if (!bAHI_APRegulatorEnabled()) {
            vAHI_ApConfigure(E_AHI_AP_REGULATOR_ENABLE,
                E_AHI_AP_INT_ENABLE,        // DISABLE にするとアナログ部の電源断
                E_AHI_AP_SAMPLE_4,          // サンプル数 2,4,6,8 が選択可能
                E_AHI_AP_CLOCKDIV_500KHZ,   // 周波数 250K/500K/1M/2M
                E_AHI_AP_INTREF);

            // レギュレーター部安定待ち
            while(!bAHI_APRegulatorEnabled()) ;
        }

        // 2) ADC 開始
        vAHI_AdcEnable(
                E_AHI_ADC_SINGLE_SHOT,
                    // E_AHI_ADC_SINGLE_SHOT １回のみ
                    // E_AHI_ADC_CONTINUOUS 連続実行
                E_AHI_AP_INPUT_RANGE_2,
                    // E_AHI_AP_INPUT_RANGE_1 (0-1.2V)
                    // または E_AHI_AP_INPUT_RANGE_2 (0-2.4V)
                E_AHI_ADC_SRC_VOLT
                    // E_AHI_ADC_SRC_ADC_1 (ADC1)
                    // E_AHI_ADC_SRC_ADC_2 (ADC2)
                    // E_AHI_ADC_SRC_ADC_3 (ADC3)
                    // E_AHI_ADC_SRC_ADC_4 (ADC4)
                    // E_AHI_ADC_SRC_TEMP (温度)
                    // E_AHI_ADC_SRC_VOLT (電圧)
                );

        vAHI_AdcStartSample(); // ADC開始
    }
}

// 電源オンによるスタート
void cbAppColdStart(bool_t bAfterAhiInit)
{
	if (!bAfterAhiInit) {

	} else {
        // SPRINTF 初期化
        SPRINTF_vInit128();

        // ユーザ定義のイベントハンドラを登録
        ToCoNet_Event_Register_State_Machine(vProcessEvCore);

		// シリアル出力用
		vSerialInit();
		ToCoNet_vDebugInit(&sSerStream);
		ToCoNet_vDebugLevel(0);
	}
}

// スリープからの復帰
void cbAppWarmStart(bool_t bAfterAhiInit)
{
    //今回は使わない
}

// ネットワークイベント発生時
void cbToCoNet_vNwkEvent(teEvent eEvent, uint32 u32arg)
{
	switch(eEvent) {
	default:
		break;
	}
}

// パケット受信時
void cbToCoNet_vRxEvent(tsRxDataApp *pRx)
{
    //今回は使わない
}

// パケット送信完了時
void cbToCoNet_vTxEvent(uint8 u8CbId, uint8 bStatus)
{
    //今回は使わない
}

// ハードウェア割り込み発生後（遅延呼び出し）
void cbToCoNet_vHwEvent(uint32 u32DeviceId, uint32 u32ItemBitmap)
{
    //割り込みに対する処理は通常ここで行う。
	switch (u32DeviceId) {

    // ADC変換が完了した
	case E_AHI_DEVICE_ANALOGUE:;

        // 電圧に換算
        // 電源電圧が２／３に分圧されていて、それを測定してるらしい。
        // なので10bit最大1023のとき2400mVとなり、分圧を逆算してx1.5 = 3600mV
        uint32 u32MilliVolt = ((uint32)u16AHI_AdcRead() * 3600) / 1023;

        debug("My Voltage = %u[mV]", u32MilliVolt);
        break;
    }
}

// ハードウェア割り込み発生時
uint8 cbToCoNet_u8HwInt(uint32 u32DeviceId, uint32 u32ItemBitmap)
{
    //割り込みで最初に呼ばれる。最短で返さないといけない。
	return FALSE;//FALSEによりcbToCoNet_vHwEvent()が呼ばれる
}

// メイン
void cbToCoNet_vMain(void)
{
}
