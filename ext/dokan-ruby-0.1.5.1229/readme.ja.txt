
    Dokan Ruby�o�C���f�B���O

 Copyright(c) Hiroki Asakawa http://decas-dev.net


�� ���̃\�t�g�E�F�A�ɂ���

Windows �p�̉��z�t�@�C���V�X�e�� Dokan �� Ruby �o�C���f�B���O�ł��D
Ruby ����� Windows�p�̃t�@�C���V�X�e�����L�q���邱�Ƃ��\�ł��D
���s�ɂ� Dokan ���C�u�������K�v�ł��D


�� ���C�Z���X�ɂ���

MIT ���C�Z���X�ɏ]���܂��Dlicense.txt ���������������D


�� �����

dokan_lib.so �� VC6 �ŃR���p�C������Ă��܂��D
mswin32 Ruby�ƁCOne-Click Ruby�œ���m�F���s���Ă��܂��D


�� �g����

  dokan_lib.so (���̃\�t�g�E�F�A�Ɋ܂܂��)

�𓯈�f�B���N�g���ɔz�u���āCRuby�X�N���v�g�����s���܂��D
���s�ɂ� Administrator �������K�v�ł��D

    > ruby sample\test.rb

�܂�

    > dokanctl.exe /u DriveLetter

�ɂ��A���}�E���g���܂��D
Dokan ���C�u�����̃h�L�������g���K���Q�Ƃ��Ă��������D

�� �t�@�C���V�X�e���̍쐬���@

API.txt �ɋL�q����Ă��郁�\�b�h�� class Hello �Ɏ������C
Dokan.mount("d", Hello.new) �����s���邱�ƂŃ}�E���g���܂��D
�e���\�b�h�͐��������� true ���s������ false ��Ԃ��ĉ������D

    open(path, fileinfo)

�ŏ��̈����͊J�����Ƃ��Ă���t�@�C���̃p�X���C���̈����͕⏕�I�ȏ�񂪓n����܂��D
FileInfo#directory?�CFileInfo#process_id�CFileInfo#context�CFileInfo#context=
����`����Ă��܂��DFileInfo#context �ɑ������ƁC����t�@�C���n���h���ɂ�鑀��
�̏ꍇ�ɁCFileInfo#context �ɑ�������l���n����Ă��܂��D

WindowsAPI �Ŏg�p���萔 FILE_ATTRIBUTE_* ����`����Ă��܂�
(Dokan::NORMAL�CDokan::DIRECTORY��)�Dstat �Ńt�@�C��������Ԃ��ꍇ�Ɏg�p���܂��D

Dokan.debug = true ���Z�b�g���邱�ƂŃf�o�b�O�����o�͂ł��܂��D


sample �ȉ��ɃT���v���X�N���v�g������܂��D

lib\dokanfs.rb �� FuseFS (Linux �p�̃��[�U���[�h�t�@�C���V�X�e���ł��� FUSE
�� Ruby �o�C���f�B���O) �݊����C�u�����ł��Drequire���邱�ƂŁCFuseFS�Ɠ���
�C���^�t�F�[�X�Ńt�@�C���V�X�e�����쐬�o���܂��D
dokanfs.rb �ɂ��ẮCsample\hello.rb ���Q�Ƃ��Ă��������D

