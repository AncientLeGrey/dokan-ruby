
    Dokan Rubyバインディング

 Copyright(c) Hiroki Asakawa http://decas-dev.net


■ このソフトウェアについて

Windows 用の仮想ファイルシステム Dokan の Ruby バインディングです．
Ruby 言語で Windows用のファイルシステムを記述することが可能です．
実行には Dokan ライブラリが必要です．


■ ライセンスについて

MIT ライセンスに従います．license.txt をご覧ください．


■ 動作環境

dokan_lib.so は VC6 でコンパイルされています．
mswin32 Rubyと，One-Click Rubyで動作確認を行っています．


■ 使い方

  dokan_lib.so (このソフトウェアに含まれる)

を同一ディレクトリに配置して，Rubyスクリプトを実行します．
実行には Administrator 権限が必要です．

    > ruby sample\test.rb

また

    > dokanctl.exe /u DriveLetter

によりアンマウントします．
Dokan ライブラリのドキュメントも必ず参照してください．

■ ファイルシステムの作成方法

API.txt に記述されているメソッドを class Hello に実装し，
Dokan.mount("d", Hello.new) を実行することでマウントします．
各メソッドは成功したら true 失敗したら false を返して下さい．

    open(path, fileinfo)

最初の引数は開こうとしているファイルのパス名，次の引数は補助的な情報が渡されます．
FileInfo#directory?，FileInfo#process_id，FileInfo#context，FileInfo#context=
が定義されています．FileInfo#context に代入すると，同一ファイルハンドルによる操作
の場合に，FileInfo#context に代入した値が渡されてきます．

WindowsAPI で使用す定数 FILE_ATTRIBUTE_* が定義されています
(Dokan::NORMAL，Dokan::DIRECTORY等)．stat でファイル属性を返す場合に使用します．

Dokan.debug = true をセットすることでデバッグ情報を出力できます．


sample 以下にサンプルスクリプトがあります．

lib\dokanfs.rb は FuseFS (Linux 用のユーザモードファイルシステムである FUSE
の Ruby バインディング) 互換ライブラリです．requireすることで，FuseFSと同一
インタフェースでファイルシステムが作成出来ます．
dokanfs.rb については，sample\hello.rb を参照してください．

