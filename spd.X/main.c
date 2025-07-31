//　自動車用スピードメータキット
// 2013/01/08 Ver012		brightness_disp変更時のチラツキをなくす為、バッファ化
// 2011/02/04 Ver011		num_disp_filtをlongに変更（オーバーフローを防ぐ為）
//							hold_timer2が0になってしまった時、速度計算を行わない処理を追加
//							FILT_FACTORを64Lに変更
// 2010/12/23 Ver010		車速表示の180度回転モードの追加(JP3を2-3に設定)
// 2010/12/21 Ver009_2_3	割り込み処理内でWriteTimer2(6);を行っていた事に起因するINT0計測時の精度問題のバグを修正
//							C18コンパイラのバージョンを3.37に変更（それに伴い、configurationをCCP2MX = PORTBに変更
// 2010/12/05 Ver009_2_2	計測モード変更処理を修正／車速1000以上の時の表示方法を変更(disklessfun)
// 2010/11/11 Ver009_2_1	一部変数名を修正
// 2010/11/06 Ver009_2 割り込みタイミングによるバグを修正（SP thx for disklessfun）
// 2010/10/25 Ver009_1
// 2010/10/13 Ver009 VW パサートワゴンのパルス逓倍数に対応
//					オープニングデモにverを表示
//					RC3ポートに入力周波数の1/2のパルスを出力
//					最高速度表示をnum_disp_filtからnum_dispに変更
//					フィルタの定数FILT_FACTORを32に変更
//					逓倍数１の時のオーバーフローバグをFIX
//					CCPモード時にタイマーが一周回った時のバグをFIX
// 2010/05/06 Ver008 対ノイズフィルタを追加
// 2010/03/16 Assembly Desk 初回

#include <p18f2420.h>
#include <timers.h>
#include <capture.h>
#include <adc.h>
#include <stdlib.h>
#include "demo.h"

//
#define FALSE 			0
#define TRUE 			1
#define BOOL			char
#define NONE 			10
#define CULC_INT0		0
#define CULC_CCP		1
#define	CULC_ZERO		2
#define	MODE_OPENING	0
#define	MODE_SPEED		1
#define MAX_ACC			1280	//最高加速度、これを超えて加速した場合はノイズとして無視する（車速×FILT_FACTORの値にする）
#define FILT_FACTOR		64L	//フィルタの定数
#define MIN_LED_BLINK	60	//LED点滅の最低速度(km/h)
#define PULSE_DEVIDE	1	///1/2の分周の場合は1、1/4の分周の場合は2

#define PORT_DISP_HUD		PORTAbits.RA6	//HUD表示の設定ピン
#define PORT_DISP_REVERSE	PORTAbits.RA7	//180度反転表示の設定ピン

//コンフィグレーション設定
#pragma config OSC = INTIO67
#pragma config FCMEN = OFF
#pragma config IESO = OFF
#pragma config PWRT = OFF
#pragma config BOREN = OFF
#pragma config WDT = OFF
#pragma config MCLRE = OFF
#pragma config LPT1OSC = OFF
#pragma config PBADEN = OFF
#pragma config CCP2MX = PORTB
#pragma config STVREN = OFF
#pragma config LVP = OFF
#pragma config XINST = OFF
#pragma config DEBUG = OFF
#pragma config CP0 = OFF
#pragma config CP1 = OFF
#pragma config CPB = OFF
#pragma config CPD = OFF
#pragma config WRT0 = OFF
#pragma config WRT1 = OFF
#pragma config WRTB = OFF
#pragma config WRTC = OFF
#pragma config WRTD = OFF
#pragma config EBTR0 = OFF
#pragma config EBTR1 = OFF
#pragma config EBTRB = OFF

//7segLED表示用ルーチン
void disp_led(unsigned char);
void disp_led_reverse(unsigned char);
void disp_led_op(unsigned char);
void (*fun_disp_led)(unsigned char);

//車両のパルスタイプの違いのデータ
const struct car_type_set
{
	long num_CCP;				//CCP計測時の変換係数
	int speed_CCP_to_INT0;		//CCP計測からINT0計測へ移る速度
	unsigned int num_INT0;		//INT0計測時の変換係数
	int speed_INT0_to_CCP;		//INT0計測からCCP計測へ移る速度
	int count_INT0_to_ZERO;		//INT0計測から車速０モードへ移るカウント数
}	car_type_set[] = 
{
	{500000 ,1000 ,977,1440 ,1000},	//通常の周波数への変換(JP1 NC, JP2 NC)
	{2825746,3840,5519,5120,5519},	//車速センサ1倍 (JP1 2-3,JP2 NC)
	{1412873,1920 ,2760,2560,2760},	//車速センサ2倍 (JP1 1-2,JP2 NC)
	{706436 ,1280 ,1380,1920 ,1380},	//車速センサ4倍 (JP1 NC, JP2 2-3)
	{353218 ,640 ,690 ,960 ,690 },	//車速センサ8倍 (JP1 2-3,JP2 2-3)
	{176609 ,384 ,345 ,640 ,345 },	//車速センサ16倍(JP1 1-2,JP2 2-3)
	{141287 ,320 ,276 ,480 ,276 },	//車速センサ20倍(JP1 NC, JP2 1-2)
	{113030 ,256 ,221 ,384 ,221 },	//車速センサ25倍(JP1 2-3,JP2 1-2)
	{423862 ,960 ,828 ,1440 ,828 },	//VW(JP1 1-2,JP2 1-2)
};
char car_type = 0;	//車のパルスタイプ

//LEDダイナミック点灯用
char timer_7seg_disp = 0;	//7segLEDダイナミック点灯に使用するタイマ
char brightness_disp = 0;	//明るさ設定。ダイナミック点灯の点灯時間を変える
char brightness_disp_buff = 0;	//明るさ設定のバッファ。メインルーチンで明るさを変える時にはこちらを変更する。
int num_disp = 0;			//LEDに表示される数値
long num_disp_filt = 0;		//num_disp計算用のフィルタ状態変数,フィルタの計算式は以下のとおり
							//x[n+1] = 31/32 x[n] + 1/32 u
							//ここで、xはフィルタ状態変数、uは計算された速度、フィルタの定数は32の時とする
							//計算を簡単化する為、状態変数は32をかけた値として保存する。num_disp_filt = 32 * x
							//num_disp_filt[n+1] = num_disp_filt[n] + (u - num_disp_filt)/32
int timer_led_disp = 0;		//LED点灯用に使用するタイマ
char count_led_light = 5;	//LEDの点灯回数
unsigned char put_led[3];	//桁毎の表示

//キャプチャ機能による速度計算用
//車速が速い時
unsigned int hold_timer1 = 0;		//キャプチャ割り込み時のタイマ１
unsigned int pre_hold_timer1 = 0;	//タイマ１の前回値
long culced_speed_CCP = 0;	//キャプチャ機能により計算された速度
unsigned int diff_hold_timer1 = 65535;	//(hold_timer1 - pre_hold_timer1)を保存

//INT0による速度計算用
//車速が遅い時
long culced_speed_INT0 = 0;				//INT0で計算された車速
unsigned int timer2_count = 0;			//timer2によるカウント
unsigned int hold_timer2 = 0;			//計算用に値をホールド
unsigned int temp_hold_timer2 = 0;		//hold_timer2をメインルーチンで使う為の

//スピードメータ機能用
int	max_speed = 0;						//最高車速(最初は0km/hとしておく)
unsigned char culc_mode = CULC_ZERO;	//車速計算のモード
char main_mode = MODE_OPENING;			//最初はオープニングモード
int devide_counter = PULSE_DEVIDE;		//パルス分周処理用のカウンター
int flg_initial = 1;					// 起動時にだけ使用する

//オープニングデモ用
unsigned int timer_opening = 0;
unsigned int timer_opening2 = 0;

void main()
{
	unsigned int temp;
	
	fun_disp_led = disp_led_op;
	
	OSCCON = 0b01110010; //内部クロックを8MHzで使用
	
	ADCON0 = 0;
	ADCON1 = 0x0d;	//AN0,AN1のみ使用
	ADCON2 = 0;

	TRISA = 0xc3;	//RA0,RA1,RA6,RA7を入力
	TRISB = 0x1;	//RB0を入力
	TRISC = 0xc5;	//RC0,RC2,RC6,RC7を入力
	LATA = 0xff;
	LATB = 0xff;
	LATC = 0xfd;	//LEDを消灯

//JP1とJP2の組み合わせにより車種を9種類より選択
//	ADC変換
	OpenADC(ADC_FOSC_16 & ADC_RIGHT_JUST & ADC_8_TAD, ADC_CH0 & ADC_INT_OFF & ADC_REF_VDD_VSS, 0xd);
	SetChanADC(ADC_CH0);
	ConvertADC();
	while(BusyADC());
	car_type = ReadADC()/342;
	SetChanADC(ADC_CH1);
	ConvertADC();
	while(BusyADC());
	car_type += ((int)(ReadADC()/342)) * 3;

//タイマ1の設定（キャプチャ用）
	OpenTimer1(TIMER_INT_OFF & T1_16BIT_RW & T1_SOURCE_INT & T1_PS_1_4 & T1_OSC1EN_OFF);
	WriteTimer1(0);
//キャプチャソースの設定（タイマ１を使用）
	T3CONbits.T3CCP1 = 0;
//キャプチャ1の設定（立ち上がりエッジのみを使用）
	OpenCapture1(CAPTURE_INT_ON & C1_EVERY_RISE_EDGE);

//タイマ2の設定		（LED点灯用　1.024ms毎に割り込み）
	OpenTimer2(TIMER_INT_ON & T2_PS_1_1 & T2_POST_1_8);
	WriteTimer2(0);

//INT0割り込み（低速時のパルス計測）
	INTCONbits.INT0IE = 1;
	INTCON2bits.INTEDG0 = 1;

//割り込みの開始設定
	RCONbits.IPEN = 0;
	IPR1bits.CCP1IP = 1;
	PIE1bits.CCP1IE = 1;
	PIE1bits.TMR2IE = 1;
	INTCONbits.PEIE = 1;
	INTCONbits.GIE = 1;

	while(1)
	{
//明るさ調整　0～3の４段階で設定
		brightness_disp_buff = (PORTCbits.RC6) ? 0 : 3;

		if(main_mode == MODE_OPENING)
		{	//オープニングデモ
			if(timer_opening2 <= 68)
			{	//
				put_led[0] = op_demo(timer_opening2,0);
				put_led[1] = op_demo(timer_opening2,1);
				put_led[2] = op_demo(timer_opening2,2);	
			}	
			if(timer_opening2 >= 80)
			{
				main_mode = MODE_SPEED;
				if(PORT_DISP_REVERSE)
					fun_disp_led = disp_led;
				else
					fun_disp_led = disp_led_reverse;
				count_led_light=2;
			}
		}
		else
		{	//スピードメータ
//INT0モード用車速計算
			temp_hold_timer2 = hold_timer2;
			if(temp_hold_timer2 != 0)		//hold_timer2 == 0であった場合は極端に短いパルスであるので処理しない
				culced_speed_INT0 = (car_type_set[car_type].num_INT0 * FILT_FACTOR) / temp_hold_timer2;
//CCPモード用車速計算
//					diff_hold_timer1は0とはならない。また、ここではオーバーフローも起こらないので除算のチェックを割愛
			culced_speed_CCP = (car_type_set[car_type].num_CCP * FILT_FACTOR) / diff_hold_timer1;
			
//計測モード遷移処理
//　culced_speed_CCPは車速がとても低い時に信頼出来ない値を取る可能性がある為、culed_speed_INT0で判定する
			if(culc_mode == CULC_CCP && culced_speed_INT0 < car_type_set[car_type].speed_CCP_to_INT0)
				culc_mode = CULC_INT0;
			else if(culc_mode == CULC_INT0 && culced_speed_INT0 > car_type_set[car_type].speed_INT0_to_CCP)
				culc_mode = CULC_CCP;

//車速計算モードにより、フィルタの計算を振り分け
			if(culc_mode == CULC_INT0)
			{
				if (flg_initial){
					num_disp_filt = culced_speed_INT0;
					flg_initial = 0;
				}
				if((culced_speed_INT0 - num_disp_filt) < MAX_ACC)
					num_disp_filt = num_disp_filt + (culced_speed_INT0 - num_disp_filt)/FILT_FACTOR;
			}
			else if(culc_mode == CULC_CCP)
			{
				if (flg_initial){
					num_disp_filt = culced_speed_CCP;
					flg_initial = 0;
				}
				if((culced_speed_CCP - num_disp_filt) < MAX_ACC)
					num_disp_filt = num_disp_filt + (culced_speed_CCP - num_disp_filt)/FILT_FACTOR;
			}
			else
				num_disp_filt = 0;

			num_disp = (int)(num_disp_filt / FILT_FACTOR);

//最高速の更新
			if(num_disp > max_speed)
			{
				max_speed = num_disp;
				if (max_speed >= MIN_LED_BLINK)
					count_led_light = 2;
			}

//最高速表示
			if(!PORTCbits.RC7)
				num_disp = max_speed;

//7segLED表示用データ作成
			put_led[0] = num_disp % 10;
			if(num_disp >= 10)
				put_led[1] = (num_disp % 100)/10;
			else
				put_led[1] = NONE;
			if(num_disp >= 100)
				put_led[2] = (num_disp/100) % 10;
			else
				put_led[2] = NONE;
		}
	}
}

#pragma interrupt isr0
#pragma code isrcode = 0x08
void isr_direct(void)
{ _asm goto isr0 _endasm}

#pragma code
void isr0(void)
{
	if(PIR1bits.CCP1IF)
	{	//キャプチャ1割り込み
		PIR1bits.CCP1IF = 0;
		pre_hold_timer1 = hold_timer1;
		hold_timer1 = ReadCapture1();
		diff_hold_timer1 = hold_timer1 - pre_hold_timer1;
		
		if (!(--devide_counter)){
			LATCbits.LATC3 = !LATCbits.LATC3;	//パルス出力ポートの値をトグルする
			devide_counter = PULSE_DEVIDE;
		}
	}
	
	if(INTCONbits.INT0IF)
	{	//INT0割り込み
		INTCONbits.INT0IF = 0;

		hold_timer2 = timer2_count;
		timer2_count = 0;

		if(culc_mode == CULC_ZERO)
		{
			hold_timer2 = car_type_set[car_type].count_INT0_to_ZERO; //車速を1km/hにする為
			culc_mode = CULC_INT0;
		}
	}

	if(PIR1bits.TMR1IF == 1)
		PIR1bits.TMR1IF = 0;

	if(PIR1bits.TMR2IF)
	{	//タイマ２割り込み	LEDのダイナミック点灯
		PIR1bits.TMR2IF = 0;

//INT0モード用のタイマ
		timer2_count++;
		if(timer2_count >= car_type_set[car_type].count_INT0_to_ZERO)
		{	//車速0モードへ
			timer2_count = 0;
			culced_speed_INT0 = 0;
			culc_mode = CULC_ZERO;
		}

//オープニングモード
		timer_opening++;
		if(timer_opening > 50)
		{
			timer_opening2++;
			timer_opening = 0;
		}
		
//LED点灯
		if(count_led_light > 0)
		{
			timer_led_disp++;
			if(timer_led_disp > 120)
			{
				timer_led_disp = 0;
				count_led_light--;
			}
			else if(timer_led_disp > 60)
			{//led消灯
				LATCbits.LATC1 = 0;
			}
			else
			{//led点灯
				LATCbits.LATC1 = 1;
			}
		}

//7segLED点灯
		timer_7seg_disp++;
		if(timer_7seg_disp == 1)
		{	//一桁目点灯
			if(PORT_DISP_REVERSE)
				LATBbits.LATB1 = 0;
			else
				LATCbits.LATC5 = 0;
			(*fun_disp_led)(put_led[0]);
		}
		else if(timer_7seg_disp == (5-brightness_disp))
		{	//一桁目消灯
			if(PORT_DISP_REVERSE)
				LATBbits.LATB1 = 1;
			else
				LATCbits.LATC5 = 1;
			disp_led(NONE);
		}
		else if(timer_7seg_disp == 6)
		{	//二桁目点灯
			LATCbits.LATC4 = 0;
			(*fun_disp_led)(put_led[1]);
		}
		else if(timer_7seg_disp == (10-brightness_disp))
		{	//二桁目消灯
			LATCbits.LATC4 = 1;
			disp_led(NONE);
		}
		else if(timer_7seg_disp == 11)
		{	//三桁目点灯
			if(PORT_DISP_REVERSE)
				LATCbits.LATC5 = 0;
			else
				LATBbits.LATB1 = 0;
			(*fun_disp_led)(put_led[2]);			
		}
		else if(timer_7seg_disp == (15-brightness_disp))
		{	//三桁目消灯
			if(PORT_DISP_REVERSE)
				LATCbits.LATC5 = 1;
			else
				LATBbits.LATB1 = 1;
			disp_led(NONE);
		}
		else if(timer_7seg_disp >= 15)
		{	//一桁目に戻る
			timer_7seg_disp = 0;
			brightness_disp = brightness_disp_buff;
		}
	}
}

//LED点灯パターン（数字／通常）
//７セグの表示パターン　0で点灯、1で消灯
//	dot:	LATA	3bit目
//	a:		LATA	4bit目
//	b:		LATA	5bit目
//	c:		LATA	6bit目
//	d:		LATB	6bit目
//	e:		LATB	5bit目
//	f:		LATB	4bit目
//	g:		LATB	3bit目
void disp_led(unsigned char num)
{

	LATA |= 0x3c;
	LATB |= 0x3c;
	
	if(PORT_DISP_HUD)
	{	//通常表示
		switch(num)
		{
			case 0:	LATA &= 0xc7;	LATB &= 0xc7;	break;
			case 1:	LATA &= 0xcf;	LATB &= 0xff;	break;
			case 2:	LATA &= 0xe7;	LATB &= 0xcb;	break;
			case 3:	LATA &= 0xc7;	LATB &= 0xdb;	break;
			case 4:	LATA &= 0xcf;	LATB &= 0xf3;	break;
			case 5:	LATA &= 0xd7;	LATB &= 0xd3;	break;
			case 6:	LATA &= 0xd7;	LATB &= 0xc3;	break;
			case 7:	LATA &= 0xc7;	LATB &= 0xf7;	break;
			case 8:	LATA &= 0xc7;	LATB &= 0xc3;	break;
			case 9:	LATA &= 0xc7;	LATB &= 0xd3;	break;
			default:	break;
		}
	}
	else
	{	//ヘッドアップディスプレイ表示
		switch(num)
		{
			case 0:	LATA &= 0xc7;	LATB &= 0xc7;	break;
			case 1:	LATA &= 0xcf;	LATB &= 0xff;	break;
			case 2:	LATA &= 0xd7;	LATB &= 0xd3;	break;
			case 3:	LATA &= 0xc7;	LATB &= 0xdb;	break;
			case 4:	LATA &= 0xcf;	LATB &= 0xeb;	break;
			case 5:	LATA &= 0xe7;	LATB &= 0xcb;	break;
			case 6:	LATA &= 0xe7;	LATB &= 0xc3;	break;
			case 7:	LATA &= 0xcf;	LATB &= 0xcf;	break;
			case 8:	LATA &= 0xc7;	LATB &= 0xc3;	break;
			case 9:	LATA &= 0xc7;	LATB &= 0xcb;	break;
			default:	break;
		}
	}	
}

//LED点灯パターン（数字／180度反転）
void disp_led_reverse(unsigned char num)
{
	LATA |= 0x3c;
	LATB |= 0x3c;

	if(PORT_DISP_HUD)
	{	//180度反転通常表示
		switch(num)
		{
			case 0:	LATA &= 0xc7;	LATB &= 0xc7;	break;
			case 1:	LATA &= 0xff;	LATB &= 0xe7;	break;
			case 2:	LATA &= 0xe7;	LATB &= 0xcb;	break;
			case 3:	LATA &= 0xf7;	LATB &= 0xc3;	break;
			case 4:	LATA &= 0xdf;	LATB &= 0xe3;	break;
			case 5:	LATA &= 0xd7;	LATB &= 0xd3;	break;
			case 6:	LATA &= 0xc7;	LATB &= 0xd3;	break;
			case 7:	LATA &= 0xdf;	LATB &= 0xc7;	break;
			case 8:	LATA &= 0xc7;	LATB &= 0xc3;	break;
			case 9:	LATA &= 0xd7;	LATB &= 0xc3;	break;
			default:	break;
		}
	}
	else
	{	//180度反転ヘッドアップディスプレイ表示
		switch(num)
		{
			case 0:	LATA &= 0xc7;	LATB &= 0xc7;	break;
			case 1:	LATA &= 0xff;	LATB &= 0xe7;	break;
			case 2:	LATA &= 0xd7;	LATB &= 0xd3;	break;
			case 3:	LATA &= 0xf7;	LATB &= 0xc3;	break;
			case 4:	LATA &= 0xef;	LATB &= 0xe3;	break;
			case 5:	LATA &= 0xe7;	LATB &= 0xcb;	break;
			case 6:	LATA &= 0xc7;	LATB &= 0xcb;	break;
			case 7:	LATA &= 0xe7;	LATB &= 0xe7;	break;
			case 8:	LATA &= 0xc7;	LATB &= 0xc3;	break;
			case 9:	LATA &= 0xe7;	LATB &= 0xc3;	break;
			default:	break;
		}
	}	
}

//LED点灯パターン（直接設定）
void disp_led_op(unsigned char num)
{
	LATA |= 0x3c;
	LATA &= ~((num & 0x0f) << 2);
	LATB |= 0x3c;
	LATB &= ~((num & 0xf0) >> 2);
}


