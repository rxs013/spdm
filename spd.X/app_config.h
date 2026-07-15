#ifndef _APP_CONFIG_H_
#define _APP_CONFIG_H_

// LCD Display Messages
// オープニング時にLCDに表示されるメッセージ
#define LCD_OPENING_LINE1 "STREAM "
#define LCD_OPENING_LINE2 " EVOLVED"

// シャットダウン時にLCDに表示されるメッセージ
#define LCD_SHUTDOWN_LINE1 "MOVABLE "
#define LCD_SHUTDOWN_LINE2 "   STUFF"

// Speedometer Step Coefficients for lambda_to_step function
// これらの係数は、スピードメーターの指針の非線形な応答を調整するために使用されます。
// 検出されたパルス周期 (lbd) に基づいて適用され、lbd は速度に反比例します。
// lbd の値が小さいほど、高速であることを示します。

// 50 km/h を超える速度域 (lbd < L50) の係数
#define STPFUNC_SPEED_OVER_50KMH    665
// 41-50 km/h の速度域 (L50 <= lbd < L40) の係数
#define STPFUNC_SPEED_41_50KMH      654
// 31-40 km/h の速度域 (L40 <= lbd < L30) の係数
#define STPFUNC_SPEED_31_40KMH      645
// 21-30 km/h の速度域 (L30 <= lbd < L20) の係数
#define STPFUNC_SPEED_21_30KMH      630
// 11-20 km/h の速度域 (L20 <= lbd < L10) の係数
#define STPFUNC_SPEED_11_20KMH      570
// 1-10 km/h の速度域 (L10 <= lbd < LZERO) の係数
#define STPFUNC_SPEED_1_10KMH       400

//画面更新インターバル (32.7ms * UPD_INT)ms間隔でLCD表示更新
#define UPD_INT 10

//パルス換算定数 60km/hで1400rpm(JIS規格)のシャフトに27丁の歯車
//タイヤ径が純正と異なる場合、歯数を変更する場合は要変更
#define PULSE_RATE 37800

// フィルター係数 (0〜255, 大きいほど応答が緩やか)
#define STEP_FILTER_K 220
#define LCD_FILTER_K 50

#endif // _APP_CONFIG_H_