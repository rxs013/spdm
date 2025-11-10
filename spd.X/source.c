
// PIC16F1827 Configuration Bit Settings

// 'C' source line config statements

// コンフィギュレーション１の設定
//#pragma config FOSC = ECH    // 内部ｸﾛｯｸを使用する(INTOSC)
#pragma config FOSC = INTOSC    // 内部ｸﾛｯｸを使用する(INTOSC)
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
void __interrupt() isr(void);
void rotate(void);
void initialize_system(void);
void shutdown(void);
uint16_t lambda_to_step(uint16_t lbd);
uint8_t lambda_to_spd(uint16_t lbd);
void initialize_motor(void);
void LcdUpdate();

#define _XTAL_FREQ 32000000 //PICのクロックをHzで設定(32MHz)
#define RO_TURN_R (1)
#define RO_TURN_L (-1)
#define RO_STOP   (0)
#define MAX_X27_COUNT_STEP  (945)   // 315degree x 3step = 945
#define bat_sig RB3 //バッ直引き込み信号
#define key_sig RB0 //キーON信号
#define OD1 0x00
#define OD2 0x01
#define OD3 0x02
#define PL1 0x03
#define PL2 0x04
#define LZERO 65535
#define LONE 47619 //1km/h時の周期が約47.619ms
#define L10 4762
#define L20 2381
#define L30 1587
#define L40 1190
#define L50 952
#define DISP_MAX 0xFF

// motor macro
#define MTHLLH()    {LATA=0;LATA=0b01000010;} //pin1/4
#define MTHLLL()    {LATA=0;LATA=0b01000000;} //pin1
#define MTLLHL()    {LATA=0;LATA=0b00000001;} //pin3
#define MTLHHL()    {LATA=0;LATA=0b10000001;} //pin2/3
#define MTLHLL()    {LATA=0;LATA=0b10000000;} //pin2
#define MTLLLH()    {LATA=0;LATA=0b00000010;} //pin4

#define ACC_MODE_LO         (13)
#define ACC_MODE_HI         (4)     // <<setting>>If the needle is heavy,
                                    // you may need to increase this number.
                                    // Default is 4.
#define MAX_SPD_STEPS     (540)   // Maximum value on the spdmeter scale. 
                                    // 540 means that the shaft rotates 180 degrees.
#define MAX_SPD_LAMBDA    (187)
#define MAX_STEP_LAMBDA   (587)
#define MT_BUFFER_COUNT     (ACC_MODE_LO-ACC_MODE_HI)
#define MT_BUFFER_COUNT_END (MT_BUFFER_COUNT-1)


unsigned int pulse; //総走行距離内部カウント変数
unsigned char spdval; //速度値
unsigned long odo; //総走行距離表示値変数
uint16_t g_motor_pos_step = 0;
uint16_t g_motor_target_step = 0;
uint16_t lambda = 65535;

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

/* ------------------------------------------------------------------
 * <<setting>>
 * uint16_t lambda_to_step(uint8_t lbd)
 * Make adjustments here to suit the specifications of the various
 * spdmeters.
 * <<note>>
 * By customizing this function, you can create an unevenly spaced gauge
 * that takes advantage of the characteristics of a stepper motor.
 * For example, you could set it to 30 degrees per 1000 revolutions
 * above 6000 revolutions, and 15 degrees below that.
 * The X27 stepper motor has 3 steps per degree.
 * However, please do not use division, as it takes a lot of time to
 * calculate and it will affect the response.
-------------------------------------------------------------------*/
uint16_t lambda_to_step(uint16_t lbd) {
    uint16_t target_step;
    //10km/h = 15° = 45step
    //20km/h = 38°= 114step
    //30km/h = 63°= 189step
    //40km/h = 86 = 258step
    //50km/h = 109°= 327step
    //60km/h = 133°= 399step
    float stpfunc = 0;
    if(lbd < MAX_STEP_LAMBDA){
        return MAX_SPD_STEPS;
    }
    else if(lbd < L50) //51km/h over
    {
        stpfunc = 6.65;
    } else if(lbd < L40)  // 31-40km/h
    {          
        stpfunc = 6.54;
    } else if(lbd < L30)  // 31-40km/h
    {
        stpfunc = 6.45;
    } else if(lbd < L20)  // 21-30km/h
    {          
        stpfunc = 6.3;
    } else if(lbd < L10) //11-20km/h
    {
        stpfunc = 5.7;
    }  else if(lbd < LZERO) // 1-10km/h
    {                    
        stpfunc = 4.5;
    } else 
    {        
        return 0;
    }
    target_step = (uint16_t)(stpfunc * LONE / lbd);
    if(target_step > MAX_SPD_STEPS)    
    {
        target_step = MAX_SPD_STEPS;
    }
    return target_step;
}

/*******************************************************************************
*  信号の波長を速度数値へ変換
*******************************************************************************/
unsigned char lambda_to_spd(uint16_t lbd) {

    if(lbd < MAX_SPD_LAMBDA){
        return DISP_MAX;
    }else if(lbd <= LONE){
        return (unsigned char)(LONE / lbd);
    }else{
        return 0;
    }
}

/*******************************************************************************
*  メインの処理
*　メイン関数は基本LCDの表示と数値変換でお茶を濁す
*******************************************************************************/
void main() 
{         
    initialize_system();
    initialize_motor();
    LCD_Clear();    
    T1CONbits.TMR1ON = 1;
    TMR1IE = 1;
    bat_sig = 1;
    uint16_t lbuf;
    while(1) 
    {        
        lbuf = lambda;
        spdval = lambda_to_spd(lbuf);
        g_motor_target_step = lambda_to_step(lbuf);
        LcdUpdate();
        if(key_sig == 1)
        {
            shutdown();
            break;
        }
    }     
}

/** -----------------------------------------------------------------
 * void LcdUpdate(void)
 * LCD表示更新
------------------------------------------------------------------ */
void LcdUpdate()
{   
    char s[8];
    char i, j, temp;
    unsigned long odotmp;
    
    //ODO=12345km,SPD=9km/h
    //↓
    //9  54321
    i = 0x00;
    temp = spdval;
    s[i] = temp % 10 + '0';
    temp /= 10;    
    for(i = 1; i < 3; i++){
        if(temp == 0){
            s[i] = ' ';
        }else{
            s[i] = temp % 10 + '0';
            temp /= 10;
        }    
    } 
    
    odotmp = odo;
    for(i = 3; i < 8; i++){
        if(odotmp == 0){
            s[i] = '0';
        }else{
            s[i] = odotmp % 10 + '0';
            odotmp /= 10;
        }
    }
    
    //9  54321
    //↓
    //12345  9
    for (i = 0, j = 7; i < j; i++, j--){
        temp = s[i];
        s[i] = s[j];
        s[j] = temp;
    }
    
    //ODO  SPD
    //12345  9
    LCD_SetCursor(0,0);
    LCD_Puts("ODO  SPD"); 
    LCD_SetCursor (0,1);
    LCD_Puts(s);    
}

/* ------------------------------------------------------------------
 * PIC Interrupt
-------------------------------------------------------------------*/
void __interrupt() isr(void)
{      
     InterI2C() ;    
     
    if(C1IF)    //車輪速センサパルス検知
    { 
        C1IF    = 0 ;   
        if(TMR1IE) //オープニング中はラムダ値にはノータッチ
        {
            lambda = TMR1;
            TMR1 = 0;        
            TMR1IF = 0;
        }

        if(C1OUT) //+信号だったら
        { 
            pulse++; //総走行距離内部カウント値+1
            if(pulse >= 37800) //60km/hで1400rpmのシャフトに27丁の歯車。あとはお察し
            {
                odo++; //走行距離+1km
                if(odo > 99999)
                {
                    odo = 0;    //こんなに使わないとは思うが一応リセット
                }
                pulse = 0; //内部カウントリセット
            }
        }

    }

    if(TMR1IF) //タイムアウト(0km/h)
    { 
        lambda = TMR1;
    }

     // MOTOR ROTATION TIMING
    if (TMR2IF) 
    {
        TMR2IF = 0;
        rotate();
    } // end of TMR2IF 
}

/* ------------------------------------------------------------------
Rotate stepping motor
-------------------------------------------------------------------*/
void rotate(void) 
{
    static int8_t l_motor_phase = 0;
    static uint8_t l_acceleration_mode = ACC_MODE_LO;
    uint8_t ii;

    int8_t new_dir = RO_STOP;
    static  int8_t dir_buffer[MT_BUFFER_COUNT] = {RO_STOP};
    uint8_t unmatch_flag;

    /*
     * acceleration mode list
     * Set a timer2 here to control the motor at the most accurate time possible.
     */
    switch (l_acceleration_mode) {
        case  0: {T2CON = 0b00100101;PR2=222;} break; // 556us
        case  1: {T2CON = 0b00100101;PR2=228;} break; // 570us
        case  2: {T2CON = 0b00100101;PR2=240;} break; // 600us
        case  3: {T2CON = 0b00100101;PR2=252;} break; // 630us
        case  4: {T2CON = 0b00101101;PR2=225;} break; // 676us
        case  5: {T2CON = 0b00101101;PR2=247;} break; // 740us
        case  6: {T2CON = 0b00110101;PR2=239;} break; // 838us
        case  7: {T2CON = 0b01000101;PR2=231;} break; // 1040us
        case  8: {T2CON = 0b01010101;PR2=235;} break; // 1292us
        case  9: {T2CON = 0b01100101;PR2=251;} break; // 1630us
        case 10: {T2CON = 0b01111101;PR2=251;} break; // 2008us
        case 11: {T2CON = 0b00100110;PR2=250;} break; // 2500us
        case 12: {T2CON = 0b00110110;PR2=226;} break; // 3168us
        default: {T2CON = 0b00111110;PR2=250;} break; // 4000us
    }
    
    if (g_motor_pos_step > g_motor_target_step) {
        new_dir = RO_TURN_L;
        g_motor_pos_step--;
    } else if (g_motor_pos_step < g_motor_target_step) {
        new_dir = RO_TURN_R;
        g_motor_pos_step++;
    } else {
        new_dir = RO_STOP;
    }
    
    /*
     *  Set motor phase.
     * X27 has 6 different phases. By changing the phase in sequence with
     * the digital output signal, X27 rotates.
     * For details, please check the X27 specifications.
     */
    if (RO_TURN_L == dir_buffer[0]) {
        if (5 == l_motor_phase) {   // 5 is max of l_motor_phase
            l_motor_phase = 0;
        } else {
            l_motor_phase++;
        }
    } else if (RO_TURN_R == dir_buffer[0]) {
        if (0 == l_motor_phase) {
            l_motor_phase = 5;
        } else {
            l_motor_phase--;
        }
    } else {
        // RO_STOP does nothing.
    }

    // Rotate motor.
    switch (l_motor_phase) {
        case 0: MTHLLH(); break;
        case 1: MTHLLL(); break;
        case 2: MTLLHL(); break;
        case 3: MTLHHL(); break;
        case 4: MTLHLL(); break;
        case 5: MTLLLH(); break;
        default: break;
    }
    

    /*
     * Set up a buffer for look-ahead.
     * If it is found that the direction of the motor will change,
     * unmatch_flag turn on, and reduce the speed of the needle in advance.
     */
    unmatch_flag = 0;
    for (ii = 0; ii < (MT_BUFFER_COUNT_END); ii++) {
        dir_buffer[ii] = dir_buffer[ii + 1];
        
        if (new_dir != dir_buffer[ii]) {
            unmatch_flag = 1;
        }
    }
    dir_buffer[ii] = new_dir;

    /*
     * Next acceleration mode
     * If the new rotation direction is different from the rotation in the buffer,
     * the rotation speed will be slowed down.
     * If they are all the same, the rotation speed will be accelerated.
     */
    if (unmatch_flag) {
        // Lower limit of needle speed.
        if (l_acceleration_mode < ACC_MODE_LO) {
            l_acceleration_mode++;
        }
    } else {
        // Upper limit of needle speed.
        if (l_acceleration_mode > ACC_MODE_HI) {
            l_acceleration_mode--;
        }
    }
}

/* ------------------------------------------------------------------
 * void initialize_motor()
 * Initialize the motor. A startup demo will also be run here.
-------------------------------------------------------------------*/
void initialize_motor(void) 
{    
    TMR2IF = 0 ;
    TMR2IE = 1 ;
    uint8_t ii;
    uint16_t tmp;
    // Start ISR with timer
    rotate();

    /*
     * First, turn the left fully to fix the position.
     * We never know the state of the shaft when the power is turned on.
     * By turning it beyond its limit, the shaft alignment is complete.
     */
    g_motor_target_step = 0;
    g_motor_pos_step = MAX_X27_COUNT_STEP;
    // Kick first rotation
    while (0!=g_motor_pos_step);
    __delay_ms(200);
    
    /*
     * Second, turn the left fully to fix the position 0.
     */
    g_motor_target_step = 0;
    g_motor_pos_step = MAX_SPD_STEPS;
    // Kick first rotation
    while (g_motor_pos_step>0);
    __delay_ms(200);
    
    /*
     * Just a demonstration, feel free to do as you like with it.
     */
    for(ii=0;;ii+=10) 
    {
        tmp = (uint16_t)(LONE / ii);
        g_motor_target_step = lambda_to_step(tmp);
        if(g_motor_target_step<MAX_SPD_STEPS) {
            while (g_motor_pos_step != g_motor_target_step);
            __delay_ms(200);
            continue;
        }
        break;
    }
    g_motor_target_step = MAX_SPD_STEPS;
    while (g_motor_pos_step != g_motor_target_step);
    __delay_ms(100);
}

/** -----------------------------------------------------------------
 * void initialize_system(void)
 * Initialize PIC microchip system
------------------------------------------------------------------ */
void initialize_system(void) {
    OSCCON     = 0b11110000;   // 内部クロックは32ＭＨｚとする
    OPTION_REG = 0b00000000 ;
    ANSELA     = 0b00000100 ; // C12IN-のみアナログとする
    ANSELB     = 0b00000000 ; // AN5-AN11は使用しない全てデジタルI/Oとする
    TRISA      = 0b00000100 ; // ピン(RA)はRA2(速度信号)が入力(RA5は入力のみとなる)
    TRISB      = 0b00010011 ; // ピン(RB)はRB4(SCL1)/RB1(SDA1)/RB0(キースイッチ信号)が入力
    WPUB       = 0b00000001 ; // RB0は内部プルアップ抵抗を指定する
    PORTA      = 0b00000000 ; // RA出力ピンの初期化(全てLOWにする)
    PORTB      = 0b00000000 ; // RB出力ピンの初期化(全てLOWにする)
    INTCON = 0b11010000; //割り込み設定
    DACCON0 = 0b11000000 ;   // VDD/VSSを使用、DACOUTピン(RA2)使わない
    DACCON1 = 7;           // 約1.2Vを出力( 5V*(7/2^5)=1.19xxx )
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

    CM1CON0 = 0b11010100 ;   // -＞＋でON、高速モード、出力は反転、ヒステリシス無効
    CM1CON1 = 0b11010010 ;   // 立上り、立下りで割込み利用、＋はDAC入力、-はRA3から入力
    C1IF    = 0 ;            // コンパレータ1割込フラグを0にする
    C1IE    = 1 ;            // コンパレータ1割込みを許可する
    T1CON = 0b00110000;
    TMR1IF = 0 ;   
    TMR1IE = 0 ;
    TMR1 = 0;

    // Ｉ２Ｃの初期化処理(通信速度400KHz※書類上)
    InitI2C_Master(1) ;
    // ＬＣＤモジュールの初期化処理
    // ICON OFF,コントラスト(0-63),VDD=5Vで使う,LCDは8文字列
    LCD_Init(LCD_NOT_ICON,63,LCD_VDD5V,8) ;
    LCD_SetCursor(0,0) ;        // 表示位置を設定する
    LCD_Puts("STREAM") ;
    LCD_SetCursor(1,1) ;        // 表示位置を設定する
    LCD_Puts("EVOLVED") ;    
}

/** -----------------------------------------------------------------
 * void shutdown(void)
 * memory distances and reset motor pos
------------------------------------------------------------------ */
void shutdown()
{    
     C1IE = 0;
     TMR1IE= 0;
     LCD_Clear();
     LCD_SetCursor(0,0) ;        // 表示位置を設定する
     LCD_Puts("MOVABLE") ;
     LCD_SetCursor(3,1) ;        // 表示位置を設定する
     LCD_Puts("STUFF") ;
    g_motor_target_step = 0;    //針を0km/hへ
    unsigned char od1, od2, od3, pl1, pl2; //EEPROM記録用の贄
    unsigned int sw; //贄の贄
    od1 = odo & 0xFF; //マスクして8bit化
    sw = (unsigned int)(odo >> 8);
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
    __delay_ms(1000); 
    bat_sig = 0; //シャットダウン
}
