<!-- コメント -->
# mz700win version 0.58 (beta1)

- (C) Marukun, Youkan, Snail(SnailBarbarianMacho)
  - Marukun: [http://retropc.net/mz-memories/mz700/]
  - Youkan: [http://www.maroon.dti.ne.jp/youkan/mz700/]
  - Snail: [https://github.com/SnailBarbarianMacho/mz700win]
- based on Russell Marks's 'mz700em'.<br>
  MZ700 emulator 'mz700em' for VGA PCs running Linux (C) 1996 Russell Marks.
- Z80 emulation from 'Z80em' Copyright (C) Marcel de Kogel 1996,1997

# About mz700win

- mz700win は 1998 年に Marukun 氏が mz700em をベースに開発した, 歴史のある MZ-700 エミュレータです.<br>
  2021 年に Snail が Z80 エミュレータの不具合を発見したので, それをきっかけに少しメンテをすることにしました.<br>
  The mz700win is a historic Windows MZ-700 emulator developed by Marukun in 1998 based on the mz700em. <br>
  In 2021, Snail discovered a bug in his Z80 emulator, so I decided to do some maintenance.

- コンパイル済バイナリと key.def は Release フォルダにあります<br>
  Compiled binaries and key.def are in the Release folder.

- その他のツール(ramfile_dump, tapeload, tapeload_g) は, まとめて tools/ に移動しました<br>
  Other tools (ramfile_dump, tapeload, tapeload_g) have been moved together to tools/

# To build

- VS2022 Community でのビルドを確認しています.<br>
  ワークロードは「C++ によるデスクトップ開発」と「C++ によるゲーム開発」が必要です.<br>
  I'm building with VS2022 Community. <br>
   Workload requires "Desktop Development in C ++" and "Game Development in C ++"

# license

- MIT


# history

```
0.57(Marukun)      0.58 beta1(Snail)
  |                   |
  :                   |
0.56a(Youkan)         |
  |                   |
  +-------------------+
  |
0.56(Youkan)
  |
  |
0.55b(Youkan)
  |
  :
  |
0.001(Marukun)
```

## 2021.12.xx 0.58(Snail)
- Based on 0.56
- Z80 add, adc, sub, sbc 命令のフラグの挙動を修正<br>
  Fixed the flag behavior of Z80 add, adc, sub, sbc instruction
- 8253 Ch#0 のカウンタに 0 を書くとクラッシュするバグ修正<br>
  Fixed a bug that caused crash when writing 0 to the 8253 ch#0 counter
- "Open File" は拡張子 MZF も認識<br>
  "Open File" also recognizes .MZF
- "Speed Control" は 200% まで対応
  "Speed control" up to 200%
- ポート E5 に 0～3 以外の値を書いても PCG が切り替わらないバグ修正(0.56aと同じ修正, たぶん)<br>
  Fixed a bug that PCG does not switch even if you write a value other than 0 to 3 on port $E5 (same fix as 0.56a, maybe)
- フル スクリーン モードは正常に動作しないので, まだ非対応です<br>
  Full screen mode does not work properly, so it is not yet supported.