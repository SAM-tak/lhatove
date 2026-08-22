# 移植状況

設計判断は [lua-to-lhat.md](lua-to-lhat.md)。

## マイルストーン

| # | 内容 | 受け入れ条件 | 状態 |
| --- | --- | --- | --- |
| M0 | ビルド統合（CMake 手術、最小 Boot、CLAUDE.md 等整備） | hold した main.lh が `print` で lovec に出力。診断表示。U1/U4 検証 | **完了**（2026-08-22）。U1 OK。U4 は機構 OK だが lhat 側 UAF あり → [lhat-issues.md](lhat-issues.md) |
| M1 | hello world（lh コア + Boot.lh/handlers/run コルーチン + timer/event/window/keyboard/mouse + 即時グラフィックス） | 矩形が動き Esc で終了。run オーバーライド動作。タプル分解動作。GC 負荷計測 (U2) | 未着手 |
| M2 | 実ゲーム対応（PhysfsLoader 完全化・conf.lh・filesystem/image/font・エラー画面・fused） | ディスク上の実ゲームディレクトリ + zip 読込 | 未着手 |
| M3 | 拡幅（audio/sound/data/math/system/touch/sensor/joystick） | 各モジュールのサンプル動作 | 未着手 |
| M4 | physics（box2d コアの脱 Lua + バインディング21本） | コールバック含むソークテスト | 未着手 |
| M5 | threads/上級（love.thread・video・Canvas/Shader/Mesh 等） | スレッドサンプル + シェーダサンプル | 未着手 |
| M6 | 仕上げ（nogame.lh・restart・Lua 残骸削除・testing/ 移植開始） | 引数なし起動で nogame 表示 | 未着手 |

## モジュール別

| モジュール | wrap ファイル数（参考） | バインディング | 状態 |
| --- | --- | --- | --- |
| love (core) | - | src/modules/love/lh_love.cpp | M0 分のみ（print・getVersion・love.Error・probe） |
| timer | 1 | lh_Timer.cpp | 未着手 |
| event | 1 | lh_Event.cpp | 未着手 |
| window | 1 | lh_Window.cpp | 未着手 |
| keyboard | 1 | lh_Keyboard.cpp | 未着手 |
| mouse | 2 | lh_Mouse.cpp 他 | 未着手 |
| graphics | 12 | lh_Graphics.cpp 他 | 未着手 |
| filesystem | 4 | lh_Filesystem.cpp 他 | 未着手 |
| image | 3 | lh_Image.cpp 他 | 未着手 |
| font | 3 | lh_Font.cpp 他 | 未着手 |
| audio | 3 | lh_Audio.cpp 他 | 未着手 |
| sound | 3 | lh_Sound.cpp 他 | 未着手 |
| data | 5 | lh_Data.cpp 他 | 未着手 |
| math | 4 | lh_Math.cpp 他 | 未着手 |
| system | 1 | lh_System.cpp | 未着手 |
| touch | 1 | lh_Touch.cpp | 未着手 |
| sensor | 1 | lh_Sensor.cpp | 未着手 |
| joystick | 2 | lh_Joystick.cpp 他 | 未着手 |
| physics/box2d | 21 | lh_Physics.cpp 他（コア脱 Lua 含む） | 未着手 |
| thread | 3 | lh_Thread.cpp 他（LuaThread 置換含む） | 未着手 |
| video | 2 | lh_Video.cpp 他 | 未着手 |
| luasocket / enet / luahttps / lua53 | - | 恒久廃止 | 確定 |
