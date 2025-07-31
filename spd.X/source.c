
// PIC16F1827 Configuration Bit Settings

// 'C' source line config statements

// コンフィギュレーション１の設定
#pragma config FOSC = ECH    // 内部ｸﾛｯｸを使用する(INTOSC)
//#pragma config FOSC = INTOSC    // 内部ｸﾛｯｸを使用する(INTOSC)
#pragma config WDTE = OFF       // ｳｵｯﾁﾄﾞｯｸﾞﾀｲﾏｰ無し(OFF)
#pragma config PWRTE = ON     // 電源ONから64ms後にﾌﾟﾛｸﾞﾗﾑを開始する(ON)
#pragma config MCLRE = OFF      // 外部ﾘｾｯﾄ信号は使用せずにﾃﾞｼﾞﾀﾙ入力(RA5)ﾋﾟﾝとする(OFF)
#pragma config CP = OFF         // ﾌﾟﾛｸﾞﾗﾑﾒﾓﾘｰを保護しない(OFF)
#pragma config CPD = OFF        // ﾃﾞｰﾀﾒﾓﾘｰを保護しない(OFF)
#pragma config BOREN = ON       // 電源電圧降下常時監視機能ON(ON)
#pragma config CLKOUTEN = OFF   // CLKOUTﾋﾟﾝをRA6ﾋﾟﾝで使用する(OFF)
#pragma config IESO = OFF       // 外部・内部ｸﾛｯｸの切替えでの起動はなし(OFF)
#pragma config FCMEN = OFF      // 外部ｸﾛｯｸ監視しない(FCMEN_OFF)

// コンフィギュレーション２の設定
#pragma config WRT = OFF        // Flashﾒﾓﾘｰを保護しない(OFF)
#pragma config PLLEN = OFF      // 動作クロックを32MHzでは動作させない(OFF)
#pragma config STVREN = ON      // スタックがオーバフローやアンダーフローしたらリセットをする(ON)
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
#define OD1 0x00
#define OD2 0x01
#define OD3 0x02
#define PL1 0x03
#define PL2 0x04
unsigned int pulse; //総走行距離内部カウント変数, 0.05秒間の平均速度変数
unsigned char spdval;
unsigned long odo; //総走行距離表示値変数

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

void sd(void);

/*******************************************************************************
*  InterFunction()   割り込みの処理                                            *
*******************************************************************************/
unsigned int interrupt InterFunction()
{   static unsigned char spdpl, width, widbuf, svbuf, pos; //PWMのON時間用変数
    static unsigned int plbuf[4];
    
     InterI2C() ;
       
    
   
    
    
    
    if(C1IF){ //車輪速センサパルス検知
        spdpl += 1;
        if(C1OUT){ //+信号だったら
        pulse += 1; //総走行距離内部カウント値+1
                if(pulse >= 37800){ //60km/hで1400rpmのシャフトに27丁の歯車。あとはお察し
                odo += 1; //走行距離+1km
                if(odo > 99999){
                odo = 0;    //こんなに使わないとは思うが一応リセット
                                }
                pulse = 0; //内部カウントリセット
                                   }
                       }
    C1IF    = 0 ;   
   }

    if(TMR1IF){ //21Hz

        plbuf[pos] = spdpl; 
        if(pos >= 3)
        {
            svbuf = (unsigned int) ((plbuf[0] + plbuf[1] + plbuf[2] + plbuf[3]) / 4. + 0.5);
            pos = 0;
        }
        else
        {
            pos += 1;
        }
        spdpl = 0;
        if(spdval != svbuf){
        spdval = svbuf; //パルス数=約0.05秒間の平均速度になるよう設計してあるんやな
               if(spdval >= 84) //メータの仕様上針がπradで84km/h
                {
                width = 49; //サーボモータはπrad以上動かないのでリミットを設ける
                }
                else if(spdval <= 10)
                {
                   width = 239 - (unsigned int)(spdval * 1.7 + 0.5);
                }
                else if (spdval == 0)
                {
                   width = 239 ;
                }
                else
                {
                   width = 239 - (unsigned int)(spdval * 2.26 + 0.5); //0-83の数値を49-239に変換
                }
        } 
            TMR1H   = 69 ;
            TMR1L   = 253 ;
            TMR1IF = 0;
            }
        
    if(TMR2IF){ //サーボ制御サイクル開始　50Hz
         if(RB7 == 0){
         if(widbuf != width){
         if(widbuf < width)
         {
             widbuf ++ ;
         }
         else if(widbuf > width)
         {
             widbuf --;
         }
            PR4 = widbuf; //49?239が0.5ms?2.4msと化す
            survo_sig = 1; //PWM信号H
            TMR4ON = 1; //サーボ信号(PWM)ON時間計測開始
         }
            TMR2IF = 0;
        }
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
{
    OSCCON     = 0b00000000 ; // 内部クロックは８ＭＨｚとする
    OPTION_REG = 0b00000000 ;
    ANSELA     = 0b00000100 ; // C12IN-のみアナログとする
    ANSELB     = 0b00000000 ; // AN5-AN11は使用しない全てデジタルI/Oとする
    TRISA      = 0b00000100 ; // ピン(RA)は全て出力に割当てる(RA5は入力のみとなる)
    TRISB      = 0b10010010 ; // ピン(RB)はRB4(SCL1)/RB1(SDA1)/RB7(キースイッチ信号)が入力
    WPUB       = 0b10000000 ; // RB7は内部プルアップ抵抗を指定する
    PORTA      = 0b00000000 ; // RA出力ピンの初期化(全てLOWにする)
    PORTB      = 0b00000000 ; // RB出力ピンの初期化(全てLOWにする)
    INTCON = 0b11010000; //割り込み設定

    DACCON0 = 0b11000000 ;   // VDD/VSSを使用、DACOUTピン(RA2)使わない
    DACCON1 = 6;           // 約0.9Vを出力( 5V*(6/2^5)=0.9375 )
    CM1CON0 = 0b11010100 ;   // ?＞＋でON、高速モード、出力は反転、ヒステリシス無効
    CM1CON1 = 0b11010010 ;   // 立上り、立下りで割込み利用、＋はDAC入力、?はRA3から入力
    C1IF    = 0 ;            // コンパレータ1割込フラグを0にする
    C1IE    = 1 ;            // コンパレータ1割込みを許可する
    TMR1IF = 0 ;
    TMR1IE = 1 ;
    TMR2IF = 0 ;
    TMR2IE = 1 ;
    TMR4IF = 0 ;
    TMR4IE = 1 ;
    T1CON = 0b00010001;
    TMR1H   = 69 ;
    TMR1L   = 253 ;
    T2CON = 0b01001110; //タイマ2利用　POS1:10　PRS1:16
    PR2 = 247;
    T4CON = 0b00100101; //タイマ4利用　POS1:5　PRS1:4
    PR4 = 83;

    
    
    
char lcd[8]; //LCD表示用文字列
   
odo = EEPROM_READ(OD3); //前回記憶した総走行距離1の読み込み
odo <<= 8;
odo = odo | EEPROM_READ(OD2); //前回記憶した総走行距離2の読み込み
odo <<= 8;
odo = odo | EEPROM_READ(OD1); //前回記憶した総走行距離3の読み込み
pulse = EEPROM_READ(PL2); //前回記憶した内部カウント数1の読み込み
pulse <<= 8;
pulse = pulse | EEPROM_READ(PL1); //前回記憶した内部カウント数2の読み込み
 
if(odo>99999){
    odo = 0;    
    }
    if(pulse>37800){
    pulse = 0;    
    }
    // Ｉ２Ｃの初期化処理(通信速度400KHz※書類上)
     InitI2C_Master(1) ;
     // ＬＣＤモジュールの初期化処理
     // ICON OFF,コントラスト(0-63),VDD=5Vで使う,LCDは8文字列
     LCD_Init(LCD_NOT_ICON,63,LCD_VDD5V,8) ;
        LCD_SetCursor(0,0) ;        // 表示位置を設定する
     LCD_Puts("STREAM") ;
        LCD_SetCursor(1,1) ;        // 表示位置を設定する
     LCD_Puts("EVOLVED") ;
    __delay_ms(1500); 
     LCD_Clear();    
     
    while(RB7 == 0) 
      {
      bat_sig = 1;
      sprintf(lcd, "%05lu%3u", odo, spdval) ;
      LCD_SetCursor(0,0);
      LCD_Puts("ODO  SPD"); 
      LCD_SetCursor (0,1);
      LCD_Puts(lcd);
      }
     
     C1IE = 0;
     TMR1IE= 0 ;
     TMR2IE = 0;
     TMR4IE = 0;
     LCD_Clear();
     LCD_SetCursor(0,0) ;        // 表示位置を設定する
     LCD_Puts("MOVABLE") ;
     LCD_SetCursor(3,1) ;        // 表示位置を設定する
     LCD_Puts("STUFF") ;
    unsigned char od1, od2, od3, pl1, pl2; //EEPROM記録用の贄
    unsigned int sw; //贄の贄
    od1 = odo & 0xFF; //マスクして8bit化
    sw = odo >> 8;
    od2 = sw & 0xFF; //ビットシフトしてマスク
    sw = odo >> 16;
    od3 = sw & 0xFF; //ビットシフトしてマスク
    pl1 = pulse & 0xFF; //マスクして8bit化
    sw = pulse >> 8;
    pl2 = sw & 0xFF; //ビットシフトしてマスク
    if(EEPROM_READ(OD1) != od1){//上位8bitが記憶されている値と同じでなければ
    EEPROM_WRITE(OD1, od1); //総走行距離1 
    } 
    if(EEPROM_READ(OD2) != od2){//真ん中位8bitが記憶されている値と同じでなければ
    EEPROM_WRITE(OD2, od2); //総走行距離2
    }
    if(EEPROM_READ(OD3) != od3){//下位8bitが記憶されている値と同じでなければ
    EEPROM_WRITE(OD3, od3); //総走行距離3
    }
    if(EEPROM_READ(PL1) != pl1){//上位8bitが記憶されている値と同じでなければ
    EEPROM_WRITE(PL1, pl1); //内部カウント値1
    }
    if(EEPROM_READ(PL2) != pl2){//下位8bitが記憶されている値と同じでなければ
    EEPROM_WRITE(PL2, pl2); //内部カウント値2
    }
    GIE = 0;
     __delay_ms(1000); 
     bat_sig = 0; //シャットダウン

}


