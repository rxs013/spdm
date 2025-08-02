/*******************************************************************************
*  skI2CLCDlib - Ｉ２Ｃ接続ＬＣＤ関数ライブラリ                                *
*             ST7032iのコントローラ(8文字x2行／16文字x2行)タイプならＯＫです。 *
*                                                                              *
*    LCD_Init       - ＬＣＤの初期化を行う処理                                 *
*    LCD_Clear      - ＬＣＤモジュールの画面を消し初期状態に戻す処理           *
*    LCD_SetCursor  - ＬＣＤモジュール画面内のカーソル位置を移動する処理       *
*    LCD_Putc       - ＬＣＤにデータを１バイト出力する処理                     *
*    LCD_Puts       - ＬＣＤに文字列データを出力する処理                       *
*    LCD_CreateChar - オリジナルのキャラクタを登録します                       *
*       (以下はVr2.00にて追加)                                                 *
*    LCD_Contrast   - ＬＣＤのコントラスト調整を行う処理                       *
*    LCD_IconClear  - 全部のアイコン表示を消す処理                             *
*    LCD_IconOnOff  - 指定アイコンの表示ON/OFFを行う処理                       *
*    LCD_PageSet       - ページ画面の切り替えを行う処理                        *
*    LCD_PageClear     - 現在表示中のページ画面内の消去を行う処理              *
*    LCD_PageSetCursor - 現在表示中のページ画面内でのカーソル設定を行う処理    *
*    LCD_PageNowNo     - 現在表示中のページ画面番号を返す処理                  *
*                                                                              *
*    メモ：__delay_us() and __delay_ms() を使用しているので "skI2CLCDlib.h" に *
*         "#define _XTAL_FREQ 8000000"や"#define FCY 60000000"が記述されている*
*         8MHz以外のCPUクロックにする人は書き換えましょう。                   *
*         メインプログラムで"skI2Clib.c"のInitI2C_Master()をコールして使用する *
* ============================================================================ *
*  VERSION DATE        BY                    CHANGE/COMMENT                    *
* ---------------------------------------------------------------------------- *
*  1.00    2013-07-25  きむ茶工房(きむしげ)  Create                            *
*  1.01    2014-08-29  きむ茶工房(きむしげ)  18F25K22でも動作(delay_msの所変更)*
*  1.02    2015-02-02  きむ茶工房(きむしげ)  64MHzまで対応((delay_msの所変更)  *
*  2.00    2015-03-01  きむ茶工房(きむしげ)  ST7032iコントローラLCDに対応      *
*  2.01    2015-08-13  きむ茶工房(きむしげ)  PIC24Eでも動作可能                *
* ============================================================================ *
*  PIC 12F1822 16F1827 18F25K22 24EP256MC202                                   *
*  (このライブラリ自体は他のPICでもOKと思います)                               *
*  MPLAB IDE(V8.63) MPLAB X(v2.15)                                             *
*  MPLAB(R) XC8  C Compiler Version 1.00/1.32                                  *
*  MPLAB(R) XC16 C Compiler Version 1.24                                       *
*******************************************************************************/
#include <xc.h>
#include "skI2Clib.h"
#include "skI2CLCDlib.h"


static int LCD_ColumnSu ;          // 制御対象ＬＣＤの表示可能文字数(8/16)
static int LCD_NowPage ;           // 現在表示中ページ画面の番号を保存する変数
static unsigned int FuncSet_DT ;   // "function set"のコマンド設定内容
static unsigned int ContSet_DT ;   // "Contrast set"のコマンド設定内容

// 指定数値 x 10ms 程待つ
void Wait10ms(unsigned int num)
{
     do {
          __delay_ms(10) ;    // 10msプログラムの一時停止
     } while(--num != 0);
}
// 約27us程待つ
void Wait27us(void)
{
     __delay_us(26) ;
}
// ＬＣＤにコマンドを発行する処理
void command(unsigned char c)
{
     int  ans ;

     ans = I2C_Start(ST7032_ADRES,RW_0);     // スタートコンディションを発行する
     if (ans == 0) {
          // command word の送信
          I2C_Send(0b10000000) ;             // control byte の送信(コマンドを指定)
          I2C_Send(c) ;                      // data byte の送信
     }
     I2C_Stop() ;                            // ストップコンディションを発行する
     Wait27us() ;
}
/*******************************************************************************
*  LCD_Clear( )                                                                *
*    ＬＣＤモジュールの画面を消し初期状態に戻す処理                            *
*    全てのDisplay Data RAM(2x40byte)に20Hのスペースを書き込みます。           *
*    ページ画面も初期状態に戻す                                                *
*******************************************************************************/
void LCD_Clear(void)
{
     command(0x01) ;     // Clear Display : 画面全体に20Hのｽﾍﾟｰｽで表示、ｶｰｿﾙはcol=0,row=0に移動
     __delay_us(1100) ;  // LCDが処理(1.08ms)するのを待ちます
     command(0x02) ;     // Return Home   : 画面を初期ポジションに戻す
     __delay_us(1100) ;  // LCDが処理(1.08ms)するのを待ちます
     LCD_NowPage  = 0 ;  // 初期ページ画面は０ページとする
}
/*******************************************************************************
*  LCD_SetCursor(col,row)                                                      *
*    ＬＣＤモジュール画面内のカーソル位置を移動する処理                        *
*    実際のDisplay Data RAMは2x40byteのエリアが有ります。(但し2-line display)  *
*    指定位置の数値データ内容チェックは行っていないので数値は正しく設定する事。*
*                                                                              *
*    col : 横(列)方向のカーソル位置(0-39)                                      *
*    row : 縦(行)方向のカーソル位置(0-1)                                       *
*******************************************************************************/
void LCD_SetCursor(int col, int row)
{
     int row_offsets[] = { 0x00, 0x40 } ;

     command(0x80 | (unsigned char)(col + row_offsets[row])) ; // Set DDRAM Adddress : 00H-27H,40H-67H
}
/*******************************************************************************
*  LCD_Putc(c)                                                                 *
*    ＬＣＤにデータを１バイト出力する処理                                      *
*                                                                              *
*    c :  出力する文字データを指定                                             *
*******************************************************************************/
void LCD_Putc(char c)
{
     int  ans ;

     ans = I2C_Start(ST7032_ADRES,RW_0);     // スタートコンディションを発行する
     if (ans == 0) {
          // command word の送信
          I2C_Send(0b11000000) ;             // control byte の送信(データを指定)
          I2C_Send(c) ;                      // data byte の送信
     }
     I2C_Stop() ;                            // ストップコンディションを発行する
     Wait27us() ;
}
/*******************************************************************************
*  LCD_Puts(*s)                                                                *
*    ＬＣＤに文字列データを出力する処理                                        *
*    文字列は、NULL(0x00)まで繰返し出力します。                                *
*                                                                              *
*    *s :  出力する文字列のデータを格納した場所のアドレスを指定                *
*******************************************************************************/
void LCD_Puts(const char * s)
{
     int  ans ;

     ans = I2C_Start(ST7032_ADRES,RW_0);     // スタートコンディションを発行する
     if (ans == 0) {
          I2C_Send(0b01000000) ;             // control byte の送信(データを指定)
          while(*s) {
               I2C_Send(*s++) ;              // data byte の送信(連続送信)
               Wait27us() ;
          }
     }
     I2C_Stop() ;                            // ストップコンディションを発行する
}
/*******************************************************************************
*  LCD_CreateChar(p,*dt)                                                       *
*    オリジナルのキャラクタを登録します                                        *
*                                                                              *
*    p   : 登録する場所の指定(０～５の６ヶ所のみ)(LCDによっては0-15の16ヶ所有) *
*    *dt : 登録したいキャラクタのデータを格納したバッファを指定                *
********************************************************************************/
void LCD_CreateChar(int p,char *dt)
{
     int x ;

     x = I2C_Start(ST7032_ADRES,RW_0);     // スタートコンディションを発行する
     if (x == 0) {
          //  LCDにキャラ保存先のアドレスを指示する
          I2C_Send(0b10000000) ;             // control byte の送信(コマンドを指定)
          I2C_Send(0x40 | (char)(p << 3)) ;
          Wait27us() ;
          //  LCDにキャラデータを送信する
          I2C_Send(0b01000000) ;             // control byte の送信(データを指定)
          for (x=0; x < 7; x++) {
               I2C_Send(*dt++) ;
               Wait27us() ;
          }
     }
     I2C_Stop() ;                            // ストップコンディションを発行する
}
/*******************************************************************************
*  LCD_IconClear( )                                                            *
*    全部のアイコン表示を消す処理(アイコン対応LCDのみ)                         *
*******************************************************************************/
void LCD_IconClear(void)
{
     int x ;

     // 拡張コマンドの設定を有効にする
     command(0x39) ;
     x = I2C_Start(ST7032_ADRES,RW_0);       // スタートコンディションを発行する
     if (x == 0) {
          // LCDのアイコンアドレスを指示する
          I2C_Send(0b10000000) ;             // control byte の送信(コマンドを指定)
          I2C_Send(0x40) ;                   // data byte の送信
          Wait27us() ;
          // アイコンのデータビットを指定する
          I2C_Send(0b01000000) ;        // control byte の送信(データを指定)
          for (x=0x40 ; x<0x50 ; x++) {
               I2C_Send(0x00) ;
               Wait27us() ;
          }
     }
     I2C_Stop() ;                            // ストップコンディションを発行する
     // 拡張コマンドを設定を無効にする
     command((unsigned char)FuncSet_DT) ;
}
/*******************************************************************************
*  LCD_IconOnOff(flag,dt)                                                      *
*    指定アイコンの表示ON/OFFを行う処理(アイコン対応LCDのみ)                   *
*    消すのはアドレス単位で全て消去、例えば△▽を表示していれば両方消えます。  *
*                                                                              *
*    flag : 指定したアイコンの表示をするしない(1=ON 0=OFF)                     *
*    dt   : アイコンのアドレスとビットを指定します                             *
*           "skI2CLCDlib.hのdefine宣言で指定します。                           *
*******************************************************************************/
void LCD_IconOnOff(int flag, unsigned int dt)
{
     int ans ;

     // 拡張コマンドの設定を有効にする
     command(0x39) ;
     ans = I2C_Start(ST7032_ADRES,RW_0);     // スタートコンディションを発行する
     if (ans == 0) {
          // LCDのアイコンアドレスを指示する
          I2C_Send(0b10000000) ;             // control byte の送信(コマンドを指定)
          I2C_Send(dt >> 8) ;                // data byte の送信
          Wait27us() ;
          // アイコンのデータビットを指定する
          I2C_Send(0b11000000) ;             // control byte の送信(データを指定)
          if (flag == 1) I2C_Send(dt & 0x00FF) ;
          else           I2C_Send(0x00) ;
     }
     I2C_Stop() ;                            // ストップコンディションを発行する
     Wait27us() ;
     // 拡張コマンドを設定を無効にする
     command((unsigned char)FuncSet_DT) ;
}
/*******************************************************************************
*  ans = LCD_PageSet(no)                                                       *
*    ページ画面の切り替えを行う処理                                            *
*                                                                              *
*    no  : 表示するページ画面の番号を指定(8x2なら0-4／16x2なら0-1)             *
*    ans : ０＝正常  －１＝指定したページページ番号がおかしい                  *
*******************************************************************************/
int LCD_PageSet(int no)
{
     int ans, c , c2 ;

     ans = -1 ;
     if (LCD_NowPage != no) {      // 指定ページと現在のページが異なるなら処理する
          // 指定ページの先頭DDRAM位置を計算する
          c = no * LCD_ColumnSu ;
          // 指定ページ - 現在ページ の差は？
          c2 = c - (LCD_NowPage * LCD_ColumnSu) ;
          if (c < (40/LCD_ColumnSu)*LCD_ColumnSu) {    // 指定ページは有効範囲なら処理する
               if (c2 > 0) {
                    // 右方向に差の分だけ画面をシフトさせる
                    for (c=0 ; c < c2 ; c++) command(0x18) ;
               } else {
                    // 左方向に差の分だけ画面をシフトさせる
                    c2 = c2 * -1 ;
                    for (c=0 ; c < c2 ; c++) command(0x1C) ;
               }
               LCD_NowPage = no ;  // 指定ページ画面の番号を保存する
               ans = 0 ;
          }
     }
     return ans ;
}
/*******************************************************************************
*  LCD_PageClear( )                                                            *
*    現在表示中のページ画面内の消去を行う処理                                  *
*******************************************************************************/
void LCD_PageClear(void)
{
     int c , i ;

     // 現在ページの先頭DDRAM位置を計算する
     c = LCD_NowPage * LCD_ColumnSu ;
     // 1行目を消す
     LCD_SetCursor(c, 0) ;
     for (i=0 ; i < LCD_ColumnSu ; i++) {
          LCD_Putc(0x20) ;
     }
     // 2行目を消す
     LCD_SetCursor(c, 1) ;
     for (i=0 ; i < LCD_ColumnSu ; i++) {
          LCD_Putc(0x20) ;
     }
     LCD_SetCursor(c, 0) ;
}
/*******************************************************************************
*  LCD_PageSetCursor(col,row)                                                  *
*    現在表示中のページ画面内でのカーソル設定を行う処理                        *
*    指定位置の数値データ内容チェックは行っていないので数値は正しく設定する事。*
*                                                                              *
*    col : 横(列)方向のカーソル位置(8x2なら0-7／16x2なら0-15)                  *
*    row : 縦(行)方向のカーソル位置(0-1)                                       *
*******************************************************************************/
void LCD_PageSetCursor(int col, int row)
{
     LCD_SetCursor((LCD_NowPage * LCD_ColumnSu) + col, row) ;
}
/*******************************************************************************
*  ans = LCD_PageNowNo()                                                       *
*    現在表示中のページ画面番号を返す処理                                      *
*                                                                              *
*    ans : ページ画面番号を返します(8x2なら0-4／16x2なら0-1)                   *
*******************************************************************************/
int LCD_PageNowNo(void)
{
     return LCD_NowPage ;
}
/*******************************************************************************
*  LCD_Contrast(contrast)                                                      *
*    ＬＣＤのコントラスト調整を行う処理                                        *
*                                                                              *
*    contrast : LCDのコントラストを指定します(０－６３)                        *
*******************************************************************************/
void LCD_Contrast(int contrast)
{
     // 拡張コマンドの設定を有効にする
     command(0x39) ;
     // コントラストの下位４ビット
     command(0x70 | (contrast & 0x0F)) ;
     // コントラストの上位２ビット
     ContSet_DT = (ContSet_DT & 0xFC) | ((contrast & 0x30) >> 4) ;
     command((unsigned char)ContSet_DT) ;
     // 拡張コマンドを設定を無効にする
     command ((unsigned char)FuncSet_DT) ;
}
/*******************************************************************************
*  LCD_Init(icon,contrast,bon,colsu)                                           *
*    ＬＣＤの初期化を行う処理                                                  *
*                                                                              *
*    icon     : アイコンを使用するのかを指定します(1=使う 0=使わない)          *
*    contrast : LCDのコントラストを指定します(０－６３)                        *
*    bon      : 昇圧回路の利用有無  1:ON(VDD=3.3V) 0:OFF(VDD=5.0V)             *
*    colsu    : LCDの表示可能文字数を指定します(８/１６)                       *
*******************************************************************************/
void LCD_Init(int icon, int contrast, int bon, int colsu)
{
     unsigned int d ;

     LCD_ColumnSu = colsu ;                        // LCDの文字数をセット
     Wait10ms(4) ;                                 // 電源ＯＮ後40msまで待ってから初期化
     FuncSet_DT = 0x38 ;
     command((unsigned char)FuncSet_DT);// function set           : データ線は8本・表示は2行・フォントは5x8ドット
     command(0x39) ;     // function set           : 拡張コマンドの設定を有効にする
     command(0x14) ;     // Internal OSC frequency : バイアスの選択と内部OSC周波数の調整
     d = 0x70 | (contrast & 0x0F) ;                // (下位4ビット)の取り出し
     command((unsigned char)d) ;        // Contrast set           : コントラスト調整データ
     d = 0x50 | ((contrast & 0x30) >> 4) ;         // (上位2ビット)の取り出し
     if (icon == 1) d = d | 0x08 ;                 // アイコンを利用する場合
     if (bon  == 1) d = d | 0x04 ;                 // 昇圧回路を利用する場合
     ContSet_DT = d ;
     command((unsigned char)d) ;        // Contrast set           : 昇圧回路、コントラスト調整データ
     command(0x6C) ;     // Follower control       : フォロア回路をON、増幅率の調整を行う
     Wait10ms(20) ;                                // 電力が安定するまで待つ(200ms)
     command((unsigned char)FuncSet_DT);// function set           : 拡張コマンドを設定を無効にする
     command(0x0C) ;     // display control        : 画面表示はON・カーソル表示はOFF・カーソル点滅はOFF
     command(0x06) ;     // entry mode set         : 文字を表示した次にカーソルを移動するを指示
     LCD_Clear() ;       // Clear Display          : 画面を消去する、初期状態に戻す
}
