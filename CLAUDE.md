# Sensor Watch Pro プロジェクト

## ハードウェア構成
- ボード: Sensor Watch Pro (BOARD=sensorwatch_pro)
- 液晶: カスタムLCD、92セグメント (DISPLAY=custom)
- 加速度センサー: LIS2DW12 (OSO-SWAB-B1、9ピンコネクタ経由)
- ケース: Casio F-91W (Module 593)

## ビルドコマンド
make BOARD=sensorwatch_pro DISPLAY=custom
emmake make BOARD=sensorwatch_pro DISPLAY=custom  # エミュレーター向け

## 判明している仕様・ハマりどころ
- accelerometer_status_face単体では、A4ピンを駆動する処理がなく、
  activity_logging_faceのような別のフェイスがバックグラウンドで
  加速度センサーを有効化しないと機能しない
- movement.cのwatch_register_extwake_callback(HAL_GPIO_A4_pin(), ...)は
  コメントアウトされており、加速度センサーによるスリープからの
  即時復帰は現状無効(電池消耗が理由と推測)
- スリープからの復帰はALARMボタンのみ(movement.c:1363)
- movement_faces.hが全フェイスのヘッダーを一括includeしているので、
  movement_config.hで個別に#includeすると重複定義エラーになることがある
- lis2dw_monitor_faceのRANGE設定を変えると、mg変換の係数が
  正しく反映されないバグらしき挙動がある(2gと16gで8倍の差)
- BOARD/DISPLAYを変えて再ビルドする際は必ず`make clean`を挟むこと。
  Makefileの依存関係追跡はヘッダーの変更しか見ておらず、DISPLAYが
  -DFORCE_CUSTOM_LCD_TYPE等のコマンドライン定義に変換されるだけの
  ため、変更してもwatch_slcd.c等が再コンパイルされず、古い設定の
  .oが混在して全フェイスの表示が崩れることがある

## やりたいこと
- 傾きに応じて液晶が重力で「溜まる」演出のウォッチフェイス
- 強い振動で表示が崩れる演出(LIS2DW12のwake-up割り込み活用)