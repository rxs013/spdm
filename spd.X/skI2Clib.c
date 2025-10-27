/*******************************************************************************
*  skI2Clib - Ｉ２Ｃ関数ライブラリ                                             *
*             このライブラリはI2Cデバイス(RTC/EEPROM等)と接続を行う為の関数集  *
*                                                                              *
*    InterI2C       - Ｉ２Ｃ関連の割り込み処理                                 *
*    InitI2C_Master - Ｉ２Ｃ通信のマスターモードで初期化を行う処理             *
*    I2C_Start      - スレーブにスタートコンディションを発行する処理           *
*    I2C_rStart     - スレーブにリピート・スタートコンディションを発行する処理 *
*    I2C_Stop       - スレーブにストップコンディションを発行する処理           *
*    I2C_Send       - スレーブにデータを１バイト送信する処理                   *
*    I2C_Receive    - スレーブからデータを１バイト受信する処理                 *
*                                                                              *
*    I2C_SlaveRead  - スレーブのデバイスから指定個数のデータを読み込む処理     *
*    I2C_SlaveWrite - スレーブのデバイスに指定個数のデータを書き込む処理       *
*                                                                              *
*    メモ：SDA/SCLピンは必ず「デジタル入力ピン」に設定を行って下さい。         *
*          通信クロックは100/400KHz(CPU8MHz)での初期化です。                   *
*          マルチマスター時にバス衝突が発生した場合は、"I2C_Start"から再送する *
*          SSP2を利用する場合は、skI2Clib.hに"#define I2C_MSSP2_USE"を宣言する *
*         __delay_us() and __delay_ms() を使用しているので "skI2CLCDlib.h" に  *
*         "#define _XTAL_FREQ 8000000"が記述されている、8MHz以外のCPUクロック*
*         にする人は書き換えましょう。                                         *
* ============================================================================ *
*  VERSION DATE        BY                    CHANGE/COMMENT                    *
* ---------------------------------------------------------------------------- *
*  1.00    2012-01-20  きむ茶工房(きむしげ)  Create                            *
*  1.01    2013-02-16  きむ茶工房(きむしげ)  XC8 C Compiler 対応に変更         *
*  2.00    2014-11-01  きむ茶工房(きむしげ)  マルチマスターに対応              *
*  2.10    2015-03-04  きむ茶工房(きむしげ)  初期化で100/400KHz指定可に変更    *
*  3.00    2015-04-20  きむ茶工房(きむしげ)  SSP1/SSP2両方と16F193xに対応      *
*  3.01    2015-07-19  きむ茶工房(きむしげ)  コメント追加                      *
*  3.10    2016-07-15  きむ茶工房(きむしげ)  デバイスのレジスタ読書き関数を追加*
* ============================================================================ *
*  PIC 12F1822 16F18xx 16F193x 18F2xK22 18F26J50                               *
*  MPLAB IDE(V8.63) MPLAB X(v2.15)                                             *
*  MPLAB(R) XC8 C Compiler Version 1.00/1.32                                   *
*******************************************************************************/
#include <xc.h>
#include "skI2Clib.h"

int AckCheck ;           // 相手からのACK待ち用フラグ変数
int CollisionCheck ;     // 他のマスターとのバス衝突検出用フラグ変数

// アイドル状態のチェック
// ACKEN RCEN PEN RSEN SEN R/W BF が全て０ならＯＫ
void I2C_IdleCheck(char mask)
{
     while (( I2C_SSPCON2 & 0x1F ) | (I2C_SSPSTAT & mask)) ;
}
/*******************************************************************************
*  InterI2C( void )                                                            *
*    Ｉ２Ｃ(マスターモード)関連の割り込み処理                                  *
*     この関数はメインプログラムの割込み関数で呼びます                         *
*******************************************************************************/
void InterI2C( void )
{
     if (I2C_SSPIF == 1) {       // SSP(I2C)割り込み発生か？
          if (AckCheck == 1) AckCheck = 0 ;
          I2C_SSPIF = 0 ;        // フラグクリア
     }
     if (I2C_BCLIF == 1) {       // MSSP(I2C)バス衝突割り込み発生か？
          CollisionCheck = 1 ;
          I2C_BCLIF = 0 ;        // フラグクリア
     }
}
/*******************************************************************************
*  InitI2C_Master(speed)                                                       *
*    Ｉ２Ｃ通信のマスターモードで初期化を行う処理                              *
*                                                                              *
*    speed : I2Cの通信速度を指定(0=100KHz 1=400KHz)                            *
*                                                                              *
*    注)クロック8MHzでの設定です、他のクロックはSSPADDを変更する必要が有ります *
*             4MHz  8MHz  16MHz  32MHz  40MHz  48MHz  64MHz                    *
*    100KHz   0x09  0x13   0x27   0x4F   0x63   0x77   0x9F                    *
*    400kHz         0x04   0x09   0x13   0x18   0x1D   0x27                    *
*    400KHzは実際には250KHz程しか速度は出ないでしょう、100KHzより速いよ程度です*
*    数値を小さくしていけば速度は速くなりますが計算速度ではないです。          * 
*******************************************************************************/
void InitI2C_Master(int speed)
{
     I2C_SSPSTAT= 0b10000000 ;     // 標準速度モード(スルーレート制御OFF)に設定する(100kHz/1MHz)
     I2C_SSPCON1= 0b00101000 ;     // SDA/SCLピンはI2Cで使用し、マスターモードとする
     if (speed == 0) {
          I2C_SSPADD = 0x27  ;     // クロック=FOSC/((SSPADD + 1)*4) 32MHz/((0x27+1)*4)=0.1(100KHz)
     } else {
          I2C_SSPADD = 0x04  ;     // クロック=FOSC/((SSPADD + 1)*4) 8MHz/((0x04+1)*4)=0.4(400KHz)
          I2C_SSPSTAT_SMP = 0 ;    // 高速速度モード(スルーレート制御ON)に設定する(400kHz)
     }
     I2C_SSPIE = 1 ;               // SSP(I2C)割り込みを許可する
     I2C_BCLIE = 1 ;               // MSSP(I2C)バス衝突割り込みを許可する
     PEIE      = 1 ;               // 周辺装置割り込みを許可する
     GIE       = 1 ;               // 全割り込み処理を許可する 
     I2C_SSPIF = 0 ;               // SSP(I2C)割り込みフラグをクリアする
     I2C_BCLIF = 0 ;               // MSSP(I2C)バス衝突割り込みフラグをクリアする
}
/*******************************************************************************
*  ans = I2C_Start(adrs,rw)                                                    *
*    スレーブにスタートコンディションを発行する処理                            *
*                                                                              *
*    adrs : スレーブのアドレスを指定します                                     *
*    rw   : スレーブに対する動作の指定をします                                 *
*           0=スレーブに書込みなさい要求　1=スレーブに送信しなさい要求         *
*    ans  : 0=正常                                                             *
*           1=異常(相手からACKが返ってこない) -1=他のマスターとのバス衝突発生  *
*******************************************************************************/
int I2C_Start(int adrs,int rw)
{
     CollisionCheck = 0 ;
     // スタート(START CONDITION)
     I2C_IdleCheck(0x5) ;
     I2C_SSPCON2_SEN = 1 ;
     // [スレーブのアドレス]を送信する
     I2C_IdleCheck(0x5) ;
     if (CollisionCheck == 1) return -1 ;
     AckCheck = 1 ;
     I2C_SSPBUF = (char)((adrs<<1)+rw) ;     // アドレス + R/Wを送信
     while (AckCheck) ;                      // 相手からのACK返答を待つ
     if (CollisionCheck == 1) return -1 ;
     return I2C_SSPCON2_ACKSTAT ;
}
/*******************************************************************************
*  ans = I2C_rStart(adrs,rw)                                                   *
*    スレーブにリピート・スタートコンディションを発行する処理                  *
*                                                                              *
*    adrs : スレーブのアドレスを指定します                                     *
*    rw   : スレーブに対する動作の指定をします                                 *
*           0=スレーブに書込みなさい要求　1=スレーブに送信しなさい要求         *
*    ans  : 0=正常                                                             *
*           1=異常(相手からACKが返ってこない) -1=他のマスターとのバス衝突発生  *
*******************************************************************************/
int I2C_rStart(int adrs,int rw)
{
     CollisionCheck = 0 ;
     // リピート・スタート(REPEATED START CONDITION)
     I2C_IdleCheck(0x5) ;
     I2C_SSPCON2_RSEN = 1 ;
     // [スレーブのアドレス]を送信する
     I2C_IdleCheck(0x5) ;
     if (CollisionCheck == 1) return -1 ;
     AckCheck = 1 ;
     I2C_SSPBUF = (char)((adrs<<1)+rw) ;     // アドレス + R/Wを送信
     while (AckCheck) ;                      // 相手からのACK返答を待つ
     if (CollisionCheck == 1) return -1 ;
     return I2C_SSPCON2_ACKSTAT ;
}
/*******************************************************************************
*  ans = I2C_Stop()                                                            *
*    スレーブにストップコンディションを発行する処理                            *
*    ans  :  0=正常                                                            *
*           -1=他のマスターとのバス衝突発生                                    *
*******************************************************************************/
int I2C_Stop()
{
     CollisionCheck = 0 ;
     // ストップ(STOP CONDITION)
     I2C_IdleCheck(0x5) ;
     I2C_SSPCON2_PEN = 1 ;
     if (CollisionCheck == 1) return -1 ;
     else                     return  0 ;
}
/*******************************************************************************
*  ans = I2C_Send(dt)                                                          *
*    スレーブにデータを１バイト送信する処理                                    *
*                                                                              *
*    dt  : 送信するデータを指定します                                          *
*    ans  : 0=正常                                                             *
*           1=異常(相手からACKが返ってこない又はNOACKを返した)                 *
*          -1=他のマスターとのバス衝突発生                                     *
*******************************************************************************/
int I2C_Send(char dt)
{
     CollisionCheck = 0 ;
     I2C_IdleCheck(0x5) ;
     if (CollisionCheck == 1) return -1 ;
     AckCheck = 1 ;
     I2C_SSPBUF = dt ;                  // データを送信
     while (AckCheck) ;                 // 相手からのACK返答を待つ
     if (CollisionCheck == 1) return -1 ;
     return I2C_SSPCON2_ACKSTAT ;
}
/*******************************************************************************
*  ans = I2C_Receive(ack)                                                      *
*    スレーブからデータを１バイト受信する処理                                  *
*                                                                              *
*    ack  : スレーブへの返答データを指定します                                 *
*           0:ACKを返す　1:NOACKを返す(受信データが最後なら1)                  *
*    ans  : 受信したデータを返す                                               *
*           -1=他のマスターとのバス衝突発生                                    *
*******************************************************************************/
int I2C_Receive(int ack)
{
     int dt ;

     CollisionCheck = 0 ;
     I2C_IdleCheck(0x5) ;
     I2C_SSPCON2_RCEN = 1 ;           // 受信を許可する
     I2C_IdleCheck(0x4) ;
     if (CollisionCheck == 1) return -1 ;
     dt = I2C_SSPBUF ;                // データの受信
     I2C_IdleCheck(0x5) ;
     if (CollisionCheck == 1) return -1 ;
     I2C_SSPCON2_ACKDT = (unsigned char)ack ;        // ACKデータのセット
     I2C_SSPCON2_ACKEN = 1 ;          // ACKデータを返す
     return dt ;
}
/*******************************************************************************
*  ans = I2C_SlaveRead(slv_adrs,reg_adrs,*data,kosu)                           *
*  スレーブのデバイスから指定個数のデータを読み込む処理                        *
*  センサー等のデバイスのレジスターから値を読み取ります。                      *
*  マルチマスターに対応しています。                                            *
*                                                                              *
*    slv_adrs : スレーブのデバイスアドレスを指定します(7bitで指定)             *
*    reg_adrs : 読出すデータのレジスターアドレスを指定します                   *
*               連続的に読出す場合は、読出すレジスターの先頭アドレスを指定     *
*    *data    : 読出したデータの格納先を指定します                             *
*    kosu     : 読出すデータのバイト数を指定します                             *
*    ans      : 0=正常　1=異常(相手からACKが返ってこない)                      *
*              -1=他のマスターとのバス衝突が発生してリトライオーバー           *
*******************************************************************************/
int I2C_SlaveRead(int slv_adrs,char reg_adrs,unsigned char *data,char kosu)
{
     int  ans , i , ack , j ;

     for (j=0 ; j<BUS_COLLISION_RETRY ; j++) {
          ans = I2C_Start(slv_adrs,RW_0);              // スタートコンディションを発行する
          if (ans == 0) {
               I2C_Send(reg_adrs) ;                    // レジスタアドレスを指定
               ans = I2C_rStart(slv_adrs,RW_1) ;       // リピート・スタートコンディションを発行する
               if (ans == 0) {
                    for (i=1 ; i<=kosu ; i++) {
                         if (i==kosu) ack = NOACK ;
                         else         ack = ACK ;
                         *data = (unsigned char)I2C_Receive(ack);     // 指定個数分読み出す(受信する)
                         data++ ;
                    }
               }
          }
          I2C_Stop() ;                                 // ストップコンディションを発行する
          if (ans != -1) break ;                       // バスの衝突以外なら終了
          else {
               for (i=0 ; i<10 ; i++) {
                    __delay_ms(10) ;                   // バスの衝突が発生した100ms後にリトライする
               }
          }
     }
     return ans ;
}
/*******************************************************************************
*  ans = I2C_SlaveWrite(slv_adrs,reg_adrs,*data,kosu)                          *
*  スレーブのデバイスに指定個数のデータを書き込む処理                          *
*  センサー等のデバイスのレジスターに値を書き込みます。                        *
*  マルチマスターに対応しています。                                            *
*                                                                              *
*    slv_adrs : スレーブのデバイスアドレスを指定します(7bitで指定)             *
*    reg_adrs : 書出すデータのレジスターアドレスを指定する                     *
*               連続的に書出す場合は、書出すレジスターの先頭アドレスを指定     *
*    *data    : 書出すデータの格納先を指定する                                 *
*    kosu     : 書出すデータのバイト数を指定する                               *
*    ans      : 0=正常　1=異常(相手からACKが返ってこない)                      *
*              -1=他のマスターとのバス衝突が発生してリトライオーバー           *
*******************************************************************************/
int I2C_SlaveWrite(int slv_adrs,char reg_adrs,unsigned char *data,char kosu)
{
     int  ans , i , j ;

     for (j=0 ; j<BUS_COLLISION_RETRY ; j++) {
          ans = I2C_Start(slv_adrs,RW_0);         // スタートコンディションを発行する
          if (ans == 0) {
               I2C_Send(reg_adrs) ;               // レジスタアドレスを指定
               for (i=0 ; i<kosu ; i++) {
                    I2C_Send(*data) ;             // 指定個数分書き込む(送信する)
                    data++ ;
               }
          }
          I2C_Stop() ;                            // ストップコンディションを発行する
          if (ans != -1) break ;                  // バスの衝突以外なら終了
          else {
               for (i=0 ; i<10 ; i++) {
                    __delay_ms(10) ;              // バスの衝突が発生した100ms後にリトライする
               }
          }
     }
     return ans ;
}
