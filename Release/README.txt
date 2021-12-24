

Coded by Marukun, Youkan, Snail

- Coded by Marukun, Youkan, Snail
  - Marukun: [http://retropc.net/mz-memories/mz700/]
  - Youkan: [http://www.maroon.dti.ne.jp/youkan/mz700/]
  - Snail: [https://github.com/SnailBarbarianMacho/mz700win]
- based on Russell Marks's 'mz700em'.<br>
  MZ700 emulator 'mz700em' for VGA PCs running Linux (C) 1996 Russell Marks.
- Z80 emulation from 'Z80em' Copyright (C) Marcel de Kogel 1996,1997
- エミュレーション精度向上によるパフォーマンス低下が気になる場合は, z80.h の Z80_EMU_XY を undef してビルドしてください.
  If you are concerned about performance degradation due to improved emulation accuracy, undef Z80_EMU_XY in z80.h and build it. 


2021.12.24 0.58
- Z80 エミュレーション精度が少し向上しました<br>
  Z80 emulation accuracy has improved a bit
- 8253 のカウンタ 0 のモードをセットすると, 音は止まるようにしました<br>
  The sound stops when the mode of counter 0 of 8253 is set.

2021.12.23 0.58 beta2(Snail)
- Z80 エミュレーション精度を MAME 並に向上しました<br>
  Z80 emulation accuracy has been improved to the same level as MAME.
- "Speed Control" は 1600% まで対応しました<br>
  "Speed Control" up to 1600%.

2021.12.04 0.58 beta1(Snail)
- Based on 0.56
- Z80 dec, add, adc, sub, sbc 命令のフラグの挙動を修正<br>
  Fixed the flag behavior of Z80 dec, add, adc, sub, sbc instruction
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