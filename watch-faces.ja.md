# ウォッチフェイス一覧

`watch-faces/*_face.h` 各ファイルの機能を1行で要約。

## clock/

| ファイル | 機能 |
|---|---|
| beats_face.h | Swatch Internet Time(.beat、1日1000ビート)を表示 |
| clock_face.h | 通常のデジタル時計。ALARM長押しで毎時チャイムのON/OFF |
| close_enough_face.h | 時刻を5分単位のふんわりした言い回しで表示(例:「10 P 5」=5時10分過ぎ) |
| ish_face.h | 3段階のあいまいさで時刻をぼかして表示するバケーション向けフェイス |
| ke_decimal_time_face.h | 十進時制「刻(こく)」で時刻を表示 |
| mars_time_face.h | 火星時間(Mars Sol Date)を着陸地点基準で表示 |
| solar_time_face.h | 設定した緯度経度に基づく太陽時(南中・時角)を表示 |
| world_clock2_face.h | 複数タイムゾーンを切り替えられる世界時計(拡張版) |
| world_clock_face.h | 曜日表示欄をタイムゾーン表示に置き換えた世界時計 |

## complication/

| ファイル | 機能 |
|---|---|
| advanced_alarm_face.h | 最大16個のアラームスロットを持つ高機能アラーム |
| alarm_face.h | シンプルな毎日アラーム(旧WAKE Face) |
| baby_kicks_face.h | 胎動(キック)の回数を記録する妊娠中向けフェイス |
| blackjack_face.h | ブラックジャックのミニゲーム(加速度センサーでタップ操作も可) |
| breathing_face.h | ボックス呼吸法をガイドするフェイス |
| countdown_face.h | 最大23時間59分59秒のカウントダウンタイマー |
| counter_face.h | ラップ回数などを数える単純なカウンター(99でループ) |
| counter_term_face.h | タリーカウンターとリセットからの経過日数を同時に表示 |
| days_since_face.h | 指定日からの経過日数/残り日数を表示(旧Day One) |
| deadline_face.h | 最大4件の締切日時までの残り時間を管理・表示 |
| endless_runner_face.h | 障害物を避けて走り続けるエンドレスランナーゲーム |
| fast_stopwatch_face.h | 標準ストップウォッチ |
| fluid_face.h | 衝撃で数字が砂のように崩れ、傾きに沿って溜まり、静止後に再構成される時計 |
| higher_lower_game_face.h | 次のカードが前より大きいか小さいかを当てるゲーム |
| hydration_face.h | 1日の水分摂取量を記録・管理 |
| interval_face.h | HIITなどに使える9スロットのインターバルタイマー |
| kitchen_conversions_face.h | 料理の単位換算(計量カップ・スプーン等) |
| kyurekisekki_face.h | 日本の旧暦(太陰太陽暦)・六曜を表示 |
| lander_face.h | 月面着陸ゲームのリメイク |
| minute_repeater_face.h | LIGHT長押しで現在時刻をビープ音で読み上げる「ミニッツリピーター」 |
| moon_phase_ascii_face.h | 月相をASCIIアート風に表示(カスタムLCD専用) |
| moon_phase_face.h | 月相をLCDセグメント図形で表示 |
| periodic_table_face.h | 元素周期表の各元素データ(原子量・発見年・電気陰性度など)を閲覧 |
| ping_face.h | 加速度センサーやボタンでパドルを操作するPong風ゲーム |
| probability_face.h | 2〜100面のサイコロを振る乱数生成フェイス |
| pulsometer_face.h | 心拍/呼吸数を計測する古典的パルソメーター複雑機構の再現 |
| simon_face.h | 「Simon」風の音と光の記憶ゲーム |
| simple_coin_flip_face.h | シンプルなコイントス |
| squash_face.h | スカッシュの2人対戦スコアを記録 |
| stopwatch_face.h | 標準ストップウォッチ |
| sunrise_sunset_face.h | 設定した緯度経度での日の出・日の入り時刻を表示 |
| tally_face.h | 増減できる汎用タリーカウンター(TCG向けライフ計算プリセットあり) |
| tarot_face.h | タロットカードを引いて占うフェイス |
| tide_face.h | 潮汐(満潮・干潮)を計算して表示 |
| timer_face.h | プリセット時間付きの高機能タイマー |
| tomato_face.h | ポモドーロ・テクニック用のタイマー |
| totp_face.h | TOTP(二要素認証用ワンタイムパスワード)を生成(認証情報はコード埋め込み) |
| totp_lfs_face.h | TOTPをファイルシステム上の設定ファイルから読み込んで生成する版 |
| wadokei_face.h | 日本の和時計(十二支の刻)を模した時刻表示 |
| wareki_face.h | 和暦(元号)を表示 |
| wordle_face.h | Wordle風の5文字単語当てゲーム |

## demo/

| ファイル | 機能 |
|---|---|
| all_segments_face.h | LCDの全セグメントを点灯させる表示確認用フェイス |
| character_set_face.h | Sensor Watchのフォント文字セットを1文字ずつ確認できる開発者向けフェイス |
| light_sensor_face.h | 光センサーの実験用フェイス(暫定・要IR_SENSOR) |
| peek_memory_face.h | 指定したメモリ番地の値をデバッグ表示(カスタムLCD専用) |
| rtccount_face.h | RTCカウンターモードの各種メトリクスを検証するテスト用フェイス |

## io/

| ファイル | 機能 |
|---|---|
| chirpy_demo_face.h | 音でデータ送信するchirpy-txライブラリのデモ(周波数較正・活動データ送信など) |
| irda_upload_face.h | IR経由でファイルを受信してファイルシステムに書き込む(要IR_SENSOR) |

## sensor/

| ファイル | 機能 |
|---|---|
| accelerometer_status_face.h | 加速度センサーの動作状態表示とモーション閾値設定(activity_logging_faceと併用が前提) |
| activity_logging_face.h | 加速度センサーによる直近14日間の活動(アクティブ)時間ログを表示 |
| lis2dw_monitor_face.h | LIS2DW12加速度センサーの生値・各種設定(レンジ/データレート等)を確認・調整 |
| temperature_display_face.h | サーミスタによる現在の気温表示 |
| temperature_logging_face.h | 気温の記録・ログ表示 |
| voltage_face.h | バッテリー電圧を表示 |

## settings/

| ファイル | 機能 |
|---|---|
| finetune_face.h | サブ秒単位で時計を微調整し、ppm補正値を計算・適用(NANOSEC face併用) |
| nanosec_face.h | 温度補正込みの高精度な周波数較正を行う(FINETUNE face併用) |
| set_location_face.h | 緯度経度を設定(カスタムLCD専用) |
| set_time_face.h | 日付・時刻・タイムゾーンを設定 |
| set_timelocation_face.h | 日時と位置をまとめて設定(カスタムLCD専用) |
| settings_face.h | 各種本体設定(旧Preferences) |
