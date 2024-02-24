# PDFViewer
![image](https://github.com/kenjinote/PDFViewer/assets/2605401/c9c5ef7f-bfee-4553-90a8-514064d3529a)

## 概要
PDFを閲覧するためのアプリです。Windows OS標準の機能を使っているため、軽量で高速に動作します。

## 操作方法
- PDFファイルを開く

　起動直後ウィンドウをクリックして、ファイルを開くダイアログから、PDFファイルを選択する。
　または、エクスプローラーから開きたいPDFをアプリにドラック＆ドロップする。
- ページ送り

  キーボードの`矢印キー`、または、`PageUpキー`、`PageDownキー`でページ移動ができます。
  `Homeキー`は先頭に、`Endキー`は末尾に移動。
  キーボードの数字キーで指定ページに移動ができます。例：連続で`2`,`3`と入力すると23ページが開きます
- 全画面表示

  `F11`キーを押すと全画面表示の切り替えができます。
- アプリ終了

  ウィンドウの閉じるボタン、または、`Escキー`、または、`Altキー`+`F4キー`でアプリを終了することができます。

## ビルド設定
![image](https://github.com/kenjinote/DrawPDF/assets/2605401/ccee53fe-24a0-445b-ba77-cff9fa02113c)

追加の #using ディレクトリ：%(AdditionalUsingDirectories)  
Windows ランタイム拡張機能の使用：はい
