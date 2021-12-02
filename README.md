<!-- コメント -->
# mz700win version 0.56
code by Marukun, Youkan, Snail 

Specifically, please visit there.
Marukun: [http://retropc.net/mz-memories/mz700/]
Youkan: [http://www.maroon.dti.ne.jp/youkan/mz700/]
Snail: []
  
- To build this project, Please use Visual Studio 2022. "Fullscreen" is disabled. Because It doesn't run yet.


                          0.58(Snail)
                             | Fixed the flag behavior of Z80 add, adc, sub, sbc instruction 
2017.07.16 0.57(Marukun)     | Fixed a bug that caused crash when writing 0 to the 8253 ch#0 counter
| Some bugs are fixed        | Further support for MZF
| but unreleased...          | Speed control up to 200%
:                            | Bug about port $E5 is fixed
2017.04.03 0.56a(Youkan)     |
|Bug about port $E5 is fixed |
|                            |
+----------------------------+
|
2016.09.10 0.56(Youkan)
| Supports XInput(Cursor, Space, Shift, Z, X)
|
2016.08.28 0.55b(Youkan)
|
0.55a(Youkan)
|
0.55(Youkan)
|
2008.02.11 0.53(Marukun)

---
## master (2016/09/10)
### branch ver_2016_09_03 was merged.

このバージョンでXInput対応 Windows用 XBOXゲームパッドに対応しました。  
何故DirectInputではなくてXInputなのか？  
それは、作者がWindows用 XBOXゲームパッドを持っていたという事と、  
XInputのほうが将来性がありそうだと思ったからです。   
ゲームパッドとＭＺキーの対応は下記の通りです。  
方向キー = カーソルキー  
Ａボタン = スペースキー  
Ｂボタン = SHIFTキー  
Ｘボタン = Ｚキー  
Ｙボタン = Ｘキー  
……キーコンフィグ？　今のところは無いです。将来暇が出来たらやるかもしれません。  
---
This version is implemented XInput for XBOX360 Gamepad for Windows.  
Why isn't it DirectInput, and XInput?  
Author (Marukun) had a 'XBOX360 Gamepad for Windows' in that.  
It's because I thought XInput seemed to be with a bright future.  
... Key Gonfig ?  
I don't have that for the moment. If leisure is done in the future, it may be done.  

---
## MZ700Win Version 0.56
### branch ver_2016_09_03 (2016/09/09)

To build, You need DirectX SDK Feb2010. It includes 'ddraw.lib' and 'XInput.lib'.  
You can get it from below link.  
[http://www.microsoft.com/downloads/details.aspx?displaylang=en&FamilyID=2c7da5fb-ffbb-4af6-8c66-651cbd28ca15](http://www.microsoft.com/downloads/details.aspx?displaylang=en&FamilyID=2c7da5fb-ffbb-4af6-8c66-651cbd28ca15)  
MZ700Win needed 'ddraw.lib' and 'XInput.lib' from this version.  
But full screen mode doesn't  yet.  
It'll be merged with master branch later.  

** DirectX SDK Jun2010 is not include 'ddraw.lib'. Please don't use this version. **  

---
### branch ver_2016_09_03 (2016/09/08)
Allocation of game pad and keyboard was implemented by fixing.  
ARROW Key = CURSOR key  
A button = SPACE key  
B button = SHIFT key  
X button = Z key  
Y button = X key  

---
## Version 0.55b (2016/08/28)
Screen size setting is expanded even x4 for a person with 4K monitor.  
