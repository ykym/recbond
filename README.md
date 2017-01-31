--- recbond ---

これは、recpt1(http://hg.honeyplanet.jp/pt1/)+httpサーバパッチをベースにした
Linux用BonDriver録画コマンドです。
今までのソフト資産を生かしつつLinuxだけで完結する環境下での運用を想定しています。


[ビルド]
autogen.sh
configure [--prefix=/usr/local] [--enable-b25]
make
make install


[オプション]
recpt1のものをそのまま受け継いでいますが"--LNB"と"--device"を廃止、BonDriver指定
"--driver"とスペース指定"--space"を追加しています。
あと「字幕が出ない」と言われてしまうので無効化してありますが"--sid"のストリーム指定子に
"epg1seg"(1セグ用EPG)・"caption"(字幕)・"esdata"(データ放送等)が追加されています。
有効化する場合は、"EXTRA_SID=1 make"てな感じでビルド時に環境変数を渡してやってください。


[チャンネル指定]
BonDriverチャンネル指定に加えrecpt1で使われている従来のものをそのまま使用できます。
なおBonDriverチャンネル指定方法は、"Bn"(nは0から始まる数字)となります。

全てのチャンネル指定方法を有効にするにはBonDriverのチャンネル定義とrecbond.confを
同期させる必要があります。

WindowsのBonDriverをBonDriverProxy経由で使用する場合は、BonDriverのiniファイルの
チャンネル定義をrecbond.confと同期させる必要があります。同梱のrecbond.confを参考にして
変更してください。


[スペース指定]
省略時は、0が設定されます。


[BonDriver指定]
フルパス指定の他に短縮指定が行なえます。
またBonDriverチャンネル指定以外で省略された場合は、自動選択されます。

短縮指定と自動選択を利用する場合は、ビルド時に recbond/pt1_dev.hを編集して各BonDriver
のファイルパスをフルパスで登録してください。

短縮指定の指定方法は、地デジが"Tn"・BS/CSは"Sn"(nは0から始まる数字)です。

BonDriver_Proxy(クライアント)を利用する場合は、短縮指定時に"P"を頭に付加してください。
自動選択時は、"P"を単独で指定してください。


[備考]
・実録画時間が変動する
  録画コマンドで受け取る前にストリームを加工していると実録画時間が変動します。
  BonDriver直接利用で-1秒ぐらい、BonDriverProxy経由で±1秒ぐらい変動します。
  よって1～2秒長めに時間指示するようにしてください。
・BonDriverProxy使用時にBonDriver指定を省略した場合は、指定チャンネル受信中チューナー
  を探査して無ければ空きチューナーを探してチューニングを行います。


[更新履歴]
最新の履歴はGitHubを参照してください。
version 1.1.0 (2016/04/14)
	・チャンネル定義をrecbond.confから読み込むように変更

version 1.0.2 (2015/11/28)
	・b25->putがエラーになった場合のwithdraw処理を追加

version 1.0.1 (2015/04/04)
	・BS難視聴対策チャンネルを整理
	・CSが受信できないバグを修正
	・BonDriver自動選択処理を見直し
	・httpサーバーのリクエストコマンド取り込みバッファーのオーバーフロー対策

version 1.0.0 (2015/03/01)
	・初版リリース
