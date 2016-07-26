/*
 * ファイルアクセス関連システムコール(低レベルI/O)を使った例題プログラム
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* テストに使うファイル名とファイルの大きさ */
#define FILENAME "testfile"
#define FILE_SIZE 256

/* テストに使う読み書きの位置と読み出しの長さ */
#define OVERWRITE_POINT 156
#define READ_POINT (OVERWRITE_POINT - 4)
#define READ_LENGTH 10

/*
 * ■■解説■■
 * 以下では、このプログラムが表示するメッセージ類を配列にまとめ、
 * その配列を使ってメッセージを表示する専用の関数を用意しています。
 * このようにしておくと、将来メッセージのフォーマットを変更したい
 * 場合や、メッセージを英語でも表示したい場合、メッセージの出力先を
 * 画面ではなくてログファイルに変更したい場合などに、プログラム全体を
 * 書き換える必要がなくなります。
 * (実際には、メッセージだけ別ファイルにするのが普通です)
 */

/* システムメッセージ番号 */
typedef enum SystemMessageNo {
    SYS_MSG_CREATE = 0,
    SYS_MSG_UNLINK = 1,
    SYS_MSG_OPEN = 2,
    SYS_MSG_CLOSE = 3,
    SYS_MSG_READ = 4,
    SYS_MSG_WRITE = 5,
    SYS_MSG_LSEEK = 6,
    SYS_MSG_ACCESS = 7,
    SYS_MSG_STAT = 8,
} SystemMessageNo;

/* システムメッセージ */
char *systemMessage[] = {
    "ファイルを作成します。",               /* SYS_MSG_CREATE */
    "ファイルを削除します。",               /* SYS_MSG_UNLINK */
    "ファイルをオープンします。",           /* SYS_MSG_OPEN */
    "ファイルをクローズします。",           /* SYS_MSG_CLOSE */
    "ファイルからデータを読み出します。",   /* SYS_MSG_READ */
    "ファイルにデータを書き込みます。",     /* SYS_MSG_WRITE */
    "ファイルのアクセス位置を変更します。", /* SYS_MSG_LSEEK */
    "ファイルの存在をチェックします。",     /* SYS_MSG_ACCESS */
    "ファイルの大きさをチェックします。",   /* SYS_MSG_STAT */
};

/* エラーメッセージ番号 */
typedef enum ErrorMessageNo {
    ERR_MSG_CREATE = 0,
    ERR_MSG_UNLINK = 1,
    ERR_MSG_OPEN = 2,
    ERR_MSG_CLOSE = 3,
    ERR_MSG_READ = 4,
    ERR_MSG_WRITE = 5,
    ERR_MSG_LSEEK = 6,
    ERR_MSG_ACCESS = 7,
    ERR_MSG_STAT = 8,
} ErrorMessageNo;

/* エラーメッセージ */
char *errorMessage[] = {
    "ファイルの作成に失敗しました。",                   /* ERR_MSG_CREATE */
    "ファイルの削除に失敗しました。",                   /* ERR_MSG_UNLINK */
    "ファイルのオープンに失敗しました。",               /* ERR_MSG_OPEN */
    "ファイルのクローズに失敗しました。",               /* ERR_MSG_CLOSE */
    "ファイルからのデータの読み出しに失敗しました。",   /* ERR_MSG_READ */
    "ファイルへのデータの書き込みに失敗しました。",     /* ERR_MSG_WRITE */
    "ファイルのアクセス位置を変更に失敗しました。",     /* ERR_MSG_LSEEK */
    "ファイルの存在のチェックに失敗しました。",         /* ERR_MSG_ACCESS */
    "ファイルの大きさのチェックに失敗しました。",       /* ERR_MSG_STAT */
};

/*
 * printSystemMessage -- システムメッセージの表示
 *
 * 引数:
 *   messageNo: 表示するメッセージ番号
 *
 * 返り値:
 *   なし
 */
void printSystemMessage(SystemMessageNo messageNo) {
    printf("%s\n", systemMessage[messageNo]);
}

/*
 * printErrorMessage -- エラーメッセージの表示
 *
 * 引数:
 *   messageNo: 表示するメッセージ番号
 *
 * 返り値:
 *   なし
 */
void printErrorMessage(ErrorMessageNo messageNo) {
    printf("Error: %s\n", errorMessage[messageNo]);
}

/* プログラムの終了ステータス番号 */
typedef enum StatusCode {
    EXIT_OK = 0,    /* 正常終了 */
    EXIT_NG = 1,    /* 異常終了 */
} StatusCode;

/*
 * exitProgram -- プログラムの終了
 *
 * 引数:
 *   status: 終了ステータス番号
 *
 * ■■解説■■
 * ここでは、このプログラムが表示するときに呼び出す関数を定義しています。
 * このようにしておくことで、将来「プログラムが終了するときには必ず一時
 * ファイルを消去したい」などを追加する必要ができたときに、プログラム全体を
 * 書き換えなくても済むようになります。
 */
void exitProgram(StatusCode status)
{
    exit(status);
}

int main(int argc, char **argv)
{
    int fd;
    char buffer[FILE_SIZE];
    struct stat stbuf;

    /* ファイルが存在するかどうかを確認 */
    printSystemMessage(SYS_MSG_ACCESS);
    if (access(FILENAME, F_OK) == 0) {
        /* ファイルを削除 */
        printSystemMessage(SYS_MSG_UNLINK);
        if (unlink(FILENAME) == -1) {
            printErrorMessage(ERR_MSG_UNLINK);
            exitProgram(EXIT_NG);
        }
    }

    /*
     * ファイルの作成
     *
     * ■■解説■■
     * creatシステムコールの第2引数には、作成するファイルに対する
     * アクセス権を指定します。以下では、ファイルを作った本人に
     * 読み出す権利(S_IRUSR)と書き込む権利(S_IWUSR)を与えています。
     */
    printSystemMessage(SYS_MSG_CREATE);
    if ((fd = creat(FILENAME, S_IRUSR | S_IWUSR)) == -1) {
        printErrorMessage(ERR_MSG_CREATE);
        exitProgram(EXIT_NG);
    }

    /*
     * ファイルのクローズ
     *
     * ■■解説■■
     * 実は、creatシステムコールは、ファイルの作成とオープンを同時に
     * 行うシステムコールです。したがって、作成したファイルをすぐに
     * 使うのであれば、creatの返すファイルディスクリプタ(fd)をそのまま
     * 使っても構いません。ここでは、openシステムコールの使い方を示す
     * ために、わざといったんクローズしています。
     */
    printSystemMessage(SYS_MSG_CLOSE);
    if (close(fd) == -1) {
        printErrorMessage(ERR_MSG_CLOSE);
        exitProgram(EXIT_NG);
    }

    /* ファイルのオープン */
    printSystemMessage(SYS_MSG_OPEN);
    if ((fd = open(FILENAME, O_RDWR)) == -1) {
        printErrorMessage(ERR_MSG_OPEN);
        exitProgram(EXIT_NG);
    }

    /* ファイルに書き込むデータの作成 */
    memset(buffer, 'X', FILE_SIZE);

    /* ファイルへの書き込み */
    printSystemMessage(SYS_MSG_WRITE);
    if (write(fd, buffer, FILE_SIZE) == -1) {
        printErrorMessage(ERR_MSG_WRITE);
        exitProgram(EXIT_NG);
    }

    /* アクセス位置の変更 */
    printSystemMessage(SYS_MSG_LSEEK);
    if (lseek(fd, OVERWRITE_POINT, SEEK_SET) == -1) {
        printErrorMessage(ERR_MSG_LSEEK);
        exitProgram(EXIT_NG);
    }

    /* データの上書き */
    printSystemMessage(SYS_MSG_WRITE);
    if (write(fd, "test", strlen("test")) == -1) {
        printErrorMessage(ERR_MSG_WRITE);
        exitProgram(EXIT_NG);
    }

    /* アクセス位置の変更 */
    printSystemMessage(SYS_MSG_LSEEK);
    if (lseek(fd, READ_POINT, SEEK_SET) == -1) {
        printErrorMessage(ERR_MSG_LSEEK);
        exitProgram(EXIT_NG);
    }

    /* 読み出しのために配列をリセット */
    memset(buffer, 0, FILE_SIZE);

    /* ファイルからのデータの読み出し */
    printSystemMessage(SYS_MSG_READ);
    if (read(fd, buffer, READ_LENGTH) == -1) {
        printErrorMessage(ERR_MSG_READ);
        exitProgram(EXIT_NG);
    }

    /* 読み出した内容を画面に出力 */
    printf("Data: %s\n", buffer);

    /* ファイルの大きさのチェック */
    printSystemMessage(SYS_MSG_STAT);
    if (stat(FILENAME, &stbuf) == -1) {
        printErrorMessage(ERR_MSG_READ);
        exitProgram(EXIT_NG);
    }

    /* ファイルのサイズを画面に出力 */
    printf("File size: %lld\n", stbuf.st_size);

    exitProgram(EXIT_OK);
}
