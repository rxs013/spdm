
// PIC16F1827 Configuration Bit Settings

// 'C' source line config statements

// CONFIG1
#pragma config FOSC = INTOSC       // Oscillator Selection (ECH, External Clock, High Power Mode (4-32 MHz): device clock supplied to CLKIN pin)
#pragma config WDTE = OFF        // Watchdog Timer Enable (WDT enabled)
#pragma config PWRTE = ON      // Power-up Timer Enable (PWRT disabled)
#pragma config MCLRE = OFF       // MCLR Pin Function Select (MCLR/VPP pin function is MCLR)
#pragma config CP = OFF         // Flash Program Memory Code Protection (Program memory code protection is disabled)
#pragma config CPD = OFF        // Data Memory Code Protection (Data memory code protection is disabled)
#pragma config BOREN = ON       // Brown-out Reset Enable (Brown-out Reset enabled)
#pragma config CLKOUTEN = OFF   // Clock Out Enable (CLKOUT function is disabled. I/O or oscillator function on the CLKOUT pin)
#pragma config IESO = OFF        // Internal/External Switchover (Internal/External Switchover mode is enabled)
#pragma config FCMEN = OFF       // Fail-Safe Clock Monitor Enable (Fail-Safe Clock Monitor is enabled)

// CONFIG2
#pragma config WRT = OFF        // Flash Memory Self-Write Protection (Write protection off)
#pragma config PLLEN = OFF       // PLL Enable (4x PLL enabled)
#pragma config STVREN = ON      // Stack Overflow/Underflow Reset Enable (Stack Overflow or Underflow will cause a Reset)
#pragma config BORV = HI        // 電源電圧降下常時監視電圧(2.5V)設定(HI)
#pragma config LVP = OFF        // 低電圧プログラミング機能使用しない(OFF)

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

#include <xc.h>
#include <stdio.h>
#include "skI2Clib.h"
#include "skI2CLCDlib.h"
#include <stdlib.h>


#define _XTAL_FREQ 8000000 //PICのクロックをHzで設定(8MHz)
#define bat_sig RB3 //バッ直引き込み信号
#define survo_sig RA1 //サーボモータPWM信号
unsigned int pulse, spdval; //総走行距離内部カウント変数, 0.05秒間の平均速度変数
unsigned long odo; //総走行距離表示値変数
// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.



/*******************************************************************************
*  InterFunction()   割り込みの処理                                            *
*******************************************************************************/
unsigned int interrupt InterFunction()
{   static unsigned char width, count; //PWMのON時間用変数, 速度検出タイミング制御用噛ませ犬
    static unsigned int spdpl; //拾ったパルスの一時保管箱
    
    
    
    if(IOCIF){  //キースイッチOFFの場合
    unsigned char od1, od2, od3, pl1, pl2; //EEPROM記録用の贄
    unsigned int sw; //贄の贄
    bat_sig = 1; //バッ直駆動に切り替え
    GIE = 0;
    od1 = odo & 0xFF; //マスクして8bit化
    sw = odo >> 8;
    od2 = sw & 0xFF; //ビットシフトしてマスク
    sw = odo >> 16;
    od3 = sw & 0xFF; //ビットシフトしてマスク
    pl1 = pulse & 0xFF; //マスクして8bit化
    sw = pulse >> 8;
    pl2 = sw & 0xFF; //ビットシフトしてマスク
    EEPROM_WRITE(0x00, od1); //総走行距離1
    EEPROM_WRITE(0x01, od2); //総走行距離2
    EEPROM_WRITE(0x02, od3); //総走行距離3
    EEPROM_WRITE(0x03, pl1); //内部カウント値1
    EEPROM_WRITE(0x04, pl2); //内部カウント値2
    bat_sig = 0; //シャットダウン
    IOCBP = 0x00; //一応フラグリセット
    GIE = 1;
    
  }
    
    
    
    if(C1IF){ //車輪速センサパルス検知
        spdpl = spdpl + 1;
        if(C1OUT){ //+信号だったら
        pulse = pulse + 1; //総走行距離内部カウント値+1
                if(pulse == 37800){ //60km/hで1400rpmのシャフトに27丁の歯車。あとはお察し
                odo = odo + 1; //走行距離+1km
                /*---------------------------------------------------*/
				/* 変えられるかも?@ここから                                */
				/*---------------------------------------------------*/
				//旧コード
				//if(odo > 99999){
                //odo = 0;    //こんなに使わないとは思うが一応リセット
                //                }
				//新コード
				odo = odo % 100000 ;
				/*---------------------------------------------------*/
				/* 変えられるかも?@ここまで                                */
				/*---------------------------------------------------*/
                pulse = 0; //内部カウントリセット
                                   }
                       }
    C1IF    = 0 ;   
   }

    if(TMR0IF){ //21Hz
		/*---------------------------------------------------*/
		/* 変えられるかも?Aここから                                */
		/*---------------------------------------------------*/
		//旧コード
        //count = count+1; //約0.05秒に一度パルス数検知するため1回休み
        //TMR0 = 70;
        //if(count >= 2){
        //    spdval = spdpl; //パルス数=約0.05秒間の平均速度になるよう設計してあるんやな
        //    if(spdval <= 83){ //メータの仕様上針がπradで83km/h
        //        width = (spdval * 228 / 100) + 49; //0?83の数値を49?239に変換
        //    }
        //    else{
        //        width = 239; //サーボモータはπrad以上動かないのでリミットを設ける
        //    }
        //    spdpl = 0;
        //     count = 0;
        //    }
		//新コード
        TMR0 = 70;
		//約0.05秒に一度パルス数検知するため1回休み
        if(count = count ^ 1){
            spdval = spdpl; //パルス数=約0.05秒間の平均速度になるよう設計してあるんやな
            if(spdval <= 83){ //メータの仕様上針がπradで83km/h
                width = (spdval * 228 / 100) + 49; //0?83の数値を49?239に変換
            }
            else{
                width = 239; //サーボモータはπrad以上動かないのでリミットを設ける
            }
            spdpl = 0;
            }
		/*---------------------------------------------------*/
		/* 変えられるかも?Aここまで                                */
		/*---------------------------------------------------*/
        TMR0IF = 0;
        }
    
	/*---------------------------------------------------*/
	/* 変えられるかも?B　　　　                                */
	/*---------------------------------------------------*/
	//サーボ用PWMはCCP機能で自動にしてしまえば良いのでは？
	//と思ったけどもしかして8MHzだとプリスケーラ全開でも周期足りなかったり…？
    if(TMR2IF){ //サーボ制御サイクル開始　50Hz
        PR4 = width; //49?239が0.5ms?2.4msと化す
        TMR4ON = 1; //サーボ信号(PWM)ON時間計測開始
        survo_sig = 1; //PWM信号H
        TMR2IF = 0;
        }
    
    if(TMR4IF){ //PWM信号ONになって時が満ちたら
        survo_sig = 0; //PWM信号L
        TMR4ON = 0; //次のサイクルまで待機
        TMR4 = 0;
        TMR4IF = 0;
    }
}
/*******************************************************************************
*  メインの処理                                                                *
*******************************************************************************/
void main() //メイン関数はLCDの表示でお茶を濁す
{  OSCCON     = 0b01110010 ; 
    ANSELA     = 0b00000100 ; // C12IN-のみアナログとする
    ANSELB     = 0b00000000 ; // AN5-AN11は使用しない全てデジタルI/Oとする
    TRISA      = 0b00000110 ; // ピン(RA)は全て出力に割当てる(RA5は入力のみとなる)
    TRISB      = 0b10010010 ; // ピン(RB)はRB4(SCL1)/RB1(SDA1)/RB7(キースイッチ信号)が入力
    WPUB       = 0b10000000 ; // RB7は内部プルアップ抵抗を指定する
    PORTA      = 0b00000000 ; // RA出力ピンの初期化(全てLOWにする)
    PORTB      = 0b00000000 ; // RB出力ピンの初期化(全てLOWにする)
    INTCON = 0b10101000; //割り込み設定
    PORTA = 0b00000000;//ポートをクリア
    PORTB = 0b00000000;//ポートクリア
    DACCON0 = 0b11000000 ;   // VDD/VSSを使用、DACOUTピン(RA2)使わない
    DACCON1 = 6;           // 約0.9Vを出力( 5V*(6/2^5)=0.9375 )
    CM1CON0 = 0b10010100 ;   // ?＞＋でON、高速モード、出力は反転、ヒステリシス無効
    CM1CON1 = 0b11010010 ;   // 立上り、立下りで割込み利用、＋はDAC入力、?はRA3から入力
    C1IF    = 0 ;            // コンパレータ1割込フラグを0にする
    C1IE    = 1 ;            // コンパレータ1割込みを許可する
    TMR0    = 70 ;
    OPTION_REG = 0b00001111; //内部プルアップ許可, TMR0ON, 割り込み許可, PS1:256
    T2CON = 0b01001110; //タイマ2利用　POS1:10　PRS1:16
    PR2 = 247;
    T4CON = 0b00100101; //タイマ4利用　POS1:5　PRS1:4
    PR4 = 83;
    IOCBP1 = 1; //キースイッチOFF検知割り込み許可
    
   char lodo[6], lspd[3]; //LCD表示用文字列
   
     // Ｉ２Ｃの初期化処理(通信速度100KHz)
     InitI2C_Master(0) ;
     

     
     // ＬＣＤモジュールの初期化処理
     // ICON OFF,コントラスト(0-63),VDD=5Vで使う,LCDは8文字列
     LCD_Init(LCD_NOT_ICON,32,LCD_VDD5V,8) ;
     LCD_Clear();
       LCD_SetCursor(0,0) ;        // 表示位置を設定する
     LCD_Puts("STREAM") ;
        LCD_SetCursor(3,1) ;        // 表示位置を設定する
     LCD_Puts("REV.E") ;

   
odo = EEPROM_READ(0x02); //前回記憶した総走行距離1の読み込み
odo <<= 8;
odo = odo | EEPROM_READ(0x01); //前回記憶した総走行距離2の読み込み
odo <<= 8;
odo = odo | EEPROM_READ(0x00); //前回記憶した総走行距離3の読み込み
pulse = EEPROM_READ(0x04); //前回記憶した内部カウント数1の読み込み
pulse <<= 8;
pulse = pulse | EEPROM_READ(0x03); //前回記憶した内部カウント数2の読み込み
     
     
    
    __delay_ms(1000); 
            
 while(1) {
     LCD_SetCursor(0,0);
     LCD_Puts("ODO");
     LCD_SetCursor(5,0);
     LCD_Puts("SPD");
     sprintf(lodo, "%05lu", odo);
     sprintf(lspd, "%02u", spdval);
     LCD_SetCursor(0,1);
     LCD_Puts("%lodo");
     LCD_SetCursor(6,1);
     LCD_Puts("%lspd");
 
     
     __delay_ms(10);
 }

}
