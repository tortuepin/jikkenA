#include<stdio.h>
#include "error.h"


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
    "メモリの確保に失敗しました",                       /* ERR_MSG_MALLOC */
};
/*
 * printErrorMessage -- エラーメッセージの表示
 *
 * 引数:
 *   messageNo: 表示するメッセージ番号
 *
 * 返り値:
 *   なし
 */
void printErrorMessage(ErrorMessageNo messageNo, const char *funcName, int lineNum) {
    printf("Error: %s\n", errorMessage[messageNo]);
    printf("in %s line = %d\n", funcName, lineNum);
}
