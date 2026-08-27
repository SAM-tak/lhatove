# 移植状況

設計判断は [lua-to-lhat.md](lua-to-lhat.md)。

## マイルストーン

| # | 内容 | 受け入れ条件 | 状態 |
| --- | --- | --- | --- |
| M0 | ビルド統合（CMake 手術、最小 Boot、CLAUDE.md 等整備） | hold した main.lh が `print` で lovec に出力。診断表示。U1/U4 検証 | **完了**（2026-08-22）。U1 OK。U4 は機構 OK だが lhat 側 UAF あり → [lhat-issues.md](lhat-issues.md) |
| M1 | hello world（lh コア + Boot.lh/handlers/run コルーチン + timer/event/window/keyboard/mouse + 即時グラフィックス） | 矩形が動き Esc で終了。run オーバーライド動作。タプル分解動作。GC 負荷計測 (U2) | **完了**（2026-08-22）。`testing/lh/*`。GC: live ≈750 obj、120 フレームで数百回収（軽微） |
| M2 | 実ゲーム対応（PhysfsLoader 完全化・conf・filesystem/image/font・エラー画面・fused） | ディスク上の実ゲームディレクトリ + zip 読込 | **完了**（2026-08-23）。`testing/lh/realgame` をディレクトリ・.love・fused exe の3形態で確認。blue screen・nogame 動作 |
| M3 | 拡幅（audio/sound/data/math/system/touch/sensor/joystick） | 各モジュールのサンプル動作 | **完了**（2026-08-23）。`testing/lh/m3` が全モジュールを1回ずつ呼ぶ（音再生・hash/lz4・乱数/Transform/noise・OS 情報・ジョイスティック列挙 + joystickadded・ImageData ピクセル） |
| M4 | physics（box2d コアの脱 Lua + バインディング21本） | コールバック含むソークテスト | **完了**（2026-08-23）。`testing/lh/physics` が begin/end/presolve/postsolve・contact filter・area query・ray cast・joint・userData を通す（exit=7、約 5 秒）。途中見つけた lhat の install 型爆発は `fea90e4` で解消（[lhat-issues.md](lhat-issues.md)） |
| M5 | threads/上級（love.thread・video・Canvas/Shader/Mesh 等） | スレッドサンプル + シェーダサンプル | **完了**（2026-08-23）。`testing/lh/thread`（file/code thread・Channel で table/closure 往復・performAtomic・threaderror、exit=8）、`testing/lh/shader`（Canvas 読み戻し・Shader uniform・Quad・Mesh・SpriteBatch・ParticleSystem・TextBatch・状態系・Video、exit=9） |
| M6 | 仕上げ（nogame.lh・restart・Lua 残骸削除・testing/ 移植） | 引数なし起動で nogame 表示 | **完了**（2026-08-24）。nogame.lh（physics のチェーン + 雲 + ドロップで restart）、restart（`love.event.restart(payload)` → 再 boot → `love.event.restartValue()`、`testing/lh/restart` exit=10）、Lua 残骸 294 ファイル削除、`testing/lh/suite`（バインド済み全モジュールを移植、451 チェック、pass=0 / fail=1） |

## モジュール別

| モジュール | wrap ファイル数（参考） | バインディング | 状態 |
| --- | --- | --- | --- |
| love (core) | - | src/modules/love/lh_love.cpp | M0 分のみ（print・getVersion・love.Error・probe） |
| timer | 1 | lh_Timer.cpp | M1: step/getDelta/getFPS/getAverageDelta/sleep/getTime |
| event | 1 | lh_Event.cpp | M1: pump/dispatch/quit/clear。M6: restart/restartValue（poll/wait/push は未） |
| window | 1 | lh_Window.cpp | M1-2: setMode(w,h[,settings])/getMode/title/isOpen/close/fullscreen/DPI/focus/vsync |
| keyboard | 1 | lh_Keyboard.cpp | M1: isDown/isScancodeDown/keyRepeat/textInput |
| mouse | 2 | lh_Mouse.cpp 他 | M1: position/isDown/visible（Cursor は未） |
| graphics | 12 | lh_Graphics.cpp / lh_GraphicsState.cpp / lh_Shader.cpp / lh_Mesh.cpp / lh_ParticleSystem.cpp / lh_Video.cpp | M1: 即時描画。M2: Texture・Font。M5: Canvas（`newCanvas` → Texture、setCanvas/getCanvas、readbackTexture）。M6 後: Texture / Mesh / SpriteBatch / ParticleSystem / TextBatch / Video を `Drawable` の下に宣言し、`draw` は `love.graphics.Drawable` 1 語、Shader（newShader/validateShader、send/sendColor/hasUniform）、Quad、Mesh（標準頂点形式のみ）、SpriteBatch、ParticleSystem、TextBatch、Video（newVideo）、状態系（blend/scissor/stencil/colorMask/defaultFilter/lineStyle/lineJoin/wireframe/shear/applyTransform/transformPoint/ellipse/arc/getRendererInfo/getStats/reset）。Buffer/compute/カスタム頂点形式/テクスチャ配列・立方体/drawInstanced/captureScreenshot は未 |
| filesystem | 4 | lh_Filesystem.cpp | M2: read/write/append/exists/getInfo/getDirectoryItems/createDirectory/remove/identity/source/isFused/load/newFile(File)/newFileData(FileData)。mount/lines/enumerate は未 |
| image | 3 | lh_Image.cpp | M2-3: newImageData(path \| w,h)、寸法、getPixel/setPixel/mapPixel。CompressedImageData・encode/paste は未 |
| font | 3 | （graphics 側で newFont） | M2: love.graphics.newFont(size \| path,size)、Font.getWidth/getHeight/getLineHeight。Rasterizer/GlyphData 直接公開は未 |
| audio | 3 | lh_Audio.cpp | M3: newSource(path,type \| SoundData)、play/stop/pause、volume、Source{play stop pause isPlaying looping volume pitch seek tell getDuration clone}。効果・フィルタ・queueable は未 |
| sound | 3 | lh_Sound.cpp | M3: newSoundData(path \| samples,rate,bits,ch)、SoundData 諸元。Decoder・サンプルアクセスは未 |
| data | 5 | lh_Data.cpp | M3: encode/decode(base64,hex)・hash・compress/decompress（文字列）。ByteData/DataView は未 |
| math | 4 | lh_Math.cpp | M3: random系・RandomGenerator・noise・gamma/linear・colorTo/FromBytes・isConvex・Transform。BezierCurve・triangulate は未 |
| system | 1 | lh_System.cpp | M3: getOS/getProcessorCount/clipboard/getPowerInfo/openURL/vibrate/locales |
| touch | 1 | lh_Touch.cpp | M3: getTouches/getPosition/getPressure（id は整数） |
| sensor | 1 | lh_Sensor.cpp | M3: hasSensor/isEnabled/setEnabled/getData |
| joystick | 2 | lh_Joystick.cpp | M3: getJoysticks/getJoystickCount、Joystick{connected name id guid axes buttons hats isDown isGamepad gamepadAxis isGamepadDown vibration}、joystick/gamepad コールバック |
| physics/box2d | 21 | lh_Physics.cpp / lh_World.cpp / lh_Body.cpp / lh_Shape.cpp / lh_Joint.cpp / lh_Contact.cpp（コア脱 Lua 済み） | M4: World/Body/Shape/Joint/Contact の 5 型（shape/joint の種別は 1 型に平坦化）、newWorld/newBody/new*Body/new*Shape/new*Joint/getDistance/meter/compute*、コールバック・filter・query・rayCast、userData。deprecated API（body 無し shape・newFixture・getChildEdge）は非対応 |
| thread | 3 | lh_Thread.cpp / LhThread.cpp（LuaThread 置換） | M5: newThread(path \| code \| File \| FileData)、Thread{start wait getError isRunning}、newChannel/getChannel、Channel{push supply pop demand peek getCount hasRead clear performAtomic}、`threaderror` コールバック。値は `lh::variantOf`（スカラー・LOVE オブジェクトはそのまま、table/closure は `lhat_carry` の複製） |
| video | 2 | graphics/lh_Video.cpp | M5: `love.graphics.newVideo(path[, {audio, dpiscale}])` → Video{play pause rewind seek tell isPlaying 寸法 getSource/setSource getFilename setFilter}。love.video.VideoStream は直接公開しない |
| luasocket / enet / luahttps / lua53 | - | 恒久廃止 | 確定 |
