# lhat 側への報告事項

lhatove の移植中に見つかった、lhat 本体で直すべき事項。解決したら行を消す。
基準: lhat HEAD `ab37a60`（2026-08-22）。

## 1. `lhat_type_of_text` の heap-use-after-free（構造型のメンバ名）

**症状**: ホスト登録のシグネチャに構造型 `t^{ update : p^number^ -> nil^;, draw : p^ -> nil^; }` を書くと、
`h.update(0.5)` が「this value has no such member: update」になったりならなかったりする（プロセス・登録順・
直前の登録内容で結果が変わる）。

**原因**（ASan 確認済み）: `lhat_type_of_text`（src/check.c:3057）が末尾で `lhat_source_dispose(&source)` を呼び、
シグネチャ文字列のコピーを解放する。しかし `chk_resolve_type` が作った構造型の `LhatTypeMember.name` は
そのバッファ内を指したまま。後の `chk_infer_member`（src/check_expr.c:2317）の `memcmp` が解放済みメモリを読む。

```text
ERROR: AddressSanitizer: heap-use-after-free
  READ of size 6 in memcmp  <- chk_infer_member check_expr.c:2317
  freed by lhat_source_dispose source.c:159 <- lhat_type_of_text check.c:3057 <- register_into program.c:1538
```

`lhat_register_func` 経由のモジュールメンバ名は呼び出し側の文字列リテラルを指すので無事。
壊れるのはシグネチャ文字列**内部**に書かれた名前（構造型のフィールド名）だけ。

**修正案**: (a) `lhat_type_of_text` が返す型木を走査しメンバ名をアリーナへ複製する、
(b) `LhatSource` を program の寿命で保持する（登録ごとに小さな文字列が残るだけ）。

**再現**: `tests/` に以下を足せば決定的に再現する（ASan ビルド）。

```c
lhat_register_func(p, "love.probe", "handlers",
    "f^ -> t^{ update : p^number^ -> nil^; };", host_nop, NULL);
// main.lh: import^ love \n let^ h = love.probe.handlers() \n h.update(0.5)
```

lhatove 側の暫定対処: `lovec --probe` で当該パスを分離し、hello ユニットは構造型戻り値に依存させていない。

## 2. `stdlib/*.h` に `extern "C"` ガードが無い

`lhat.h` 配下にはあるが `stdlib/debug.h` / `error.h` / `regex.h` / `load.h` 等には無く、
C++ から include すると `lhatstdlib_*_register` がリンクできない。lhatove は `extern "C" { #include ... }` で包んでいる。

## 3. `stdlib/math.h` がインストール対象から漏れている

`CMakeLists.txt` の `install(FILES ...)` に `stdlib/math.h` が無い（`lhatstdlib_math_register` を外部から呼べない）。
lhatove は in-tree 参照なので影響なし。
