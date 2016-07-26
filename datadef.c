/*
 * datadef.c - データ定義モジュール
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "microdb.h"
#include "error.h"
/*
 * DEF_FILE_EXT -- データ定義ファイルの拡張子
 */
#define DEF_FILE_EXT ".def"

/*
 * initializeDataDefModule -- データ定義モジュールの初期化
 *
 * 引数:
 *	なし
 *
 * 返り値;
 *	成功ならOK、失敗ならNGを返す
 */
Result initializeDataDefModule()
{
    return OK;
}

/*
 * finalizeDataDefModule -- データ定義モジュールの終了処理
 *
 * 引数:
 *	なし
 *
 * 返り値;
 *	成功ならOK、失敗ならNGを返す
 */
Result finalizeDataDefModule()
{
    return OK;
}

/*
 * createTable -- 表(テーブル)の作成
 *
 * 引数:
 *	tableName: 作成する表の名前
 *	tableInfo: データ定義情報
 *
 * 返り値:
 *	成功ならOK、失敗ならNGを返す
 *
 * データ定義ファイルの構造(ファイル名: tableName.def)
 *   +-------------------+----------------------+-------------------+----
 *   |フィールド数       |フィールド名          |データ型           |
 *   |(sizeof(int)バイト)|(MAX_FIELD_NAMEバイト)|(sizeof(int)バイト)|
 *   +-------------------+----------------------+-------------------+----
 * 以降、フィールド名とデータ型が交互に続く。
 */
Result createTable(char *tableName, TableInfo *tableInfo)
{
    int i, len;
    char *filename;
    File *file;
    char page[PAGE_SIZE];
    char *p;

    /*[tableName].defと言う文字列を作る*/
    len = strlen(tableName) + strlen(DEF_FILE_EXT) + 1;
    if ((filename = malloc(len)) == NULL ){
        printErrorMessage(ERR_MSG_MALLOC, __func__, __LINE__);
        return NG;
    }
    snprintf(filename, len, "%s%s", tableName, DEF_FILE_EXT);

    /*[tableName].defと言うファイルを作る*/
    if(createFile(filename) != OK){
        printErrorMessage(ERR_MSG_CREATE, __func__, __LINE__);
        free(filename);
        return NG;
    }

    /*[tableName].datというファイルを作る*/
    if(createDataFile(tableName) != OK){
        printErrorMessage(ERR_MSG_UNLINK, __func__, __LINE__);
        return NG;
    }

    /*[tableName].defをオープンする*/
    if((file = openFile(filename)) == NULL){
        printErrorMessage(ERR_MSG_OPEN, __func__, __LINE__);
        free(filename);
        return NG;
    }

    /* filenameをfree */
    free(filename);

    /* PAGE_SIZEバイト分の大きさを持つ配列pageを初期化する*/
    memset(page, 0, PAGE_SIZE);
    p = page;

    /*配列pageの先頭にフィールド数を記録する*/
    memcpy(p, &(tableInfo->numField), sizeof(tableInfo->numField));
    p += sizeof(tableInfo->numField);

    /*それぞれのフィールドについて、フィールド名とデータ型をpageに記録する*/
    for(i = 0; i < tableInfo->numField; i++) {
        /*i番目のフィールドの名前の記録*/
        memcpy(p, tableInfo->fieldInfo[i].name, MAX_FIELD_NAME);
        p += MAX_FIELD_NAME; 
    
        /*i番目のフィールドデータ型の記録*/
        memcpy(p, &(tableInfo->fieldInfo[i].dataType), sizeof(int));
        p += sizeof(int);

    }

    /*出来上がったpageをwritePageでファイル[tableName].defの0ページめに記録する*/
    if((writePage(file, 0, page)) == NG){
        printErrorMessage(ERR_MSG_WRITE, __func__, __LINE__);
        return NG;
    }

    /*[tableName].defをクローズする*/
    if((closeFile(file)) == NG){
        printErrorMessage(ERR_MSG_CLOSE, __func__, __LINE__);
        return NG;
    }

    /*OKを返す*/

    return OK;

}

/*
 * dropTable -- 表(テーブル)の削除
 *
 * 引数:
 *	tableName: 削除するテーブルの名前
 *
 * 返り値:
 *	Result
 */
Result dropTable(char *tableName)
{

    /*tableFileName.[DEF_FILE_EXT]という文字列を作る*/
    char tableFileName[260];
    int len = strlen(tableName) + strlen(DEF_FILE_EXT) + 1;
    memset(tableFileName, '\0', strlen(tableFileName));
    snprintf(tableFileName, len, "%s%s", tableName, DEF_FILE_EXT);
    /*tableFileNameという名前を持つファイルを削除する*/
    if((deleteFile(tableFileName) == NG)){
        printErrorMessage(ERR_MSG_UNLINK, __func__, __LINE__);
        return NG;
    }

    if(deleteDataFile(tableName) != OK){
        printErrorMessage(ERR_MSG_UNLINK, __func__, __LINE__);
        return NG;
    }

    /*okを返す*/
    return OK;
}

/*
 * getTableInfo -- 表のデータ定義情報を取得する関数
 *
 * 引数:
 *	tableName: 情報を表示する表の名前
 *
 * 返り値:
 *	tableNameのデータ定義情報を返す
 *	エラーの場合には、NULLを返す
 *
 * ***注意***
 *	この関数が返すデータ定義情報を収めたメモリ領域は、不要になったら
 *	必ずfreeTableInfoで解放すること。
 */
TableInfo *getTableInfo(char *tableName)
{
    File *file;
    TableInfo *tableinfo;
    char page[PAGE_SIZE];
    char *p;
    int num;
    char name[MAX_FIELD_NAME];
    DataType data;
    int i;

    /* tableFileName.[DEF_FILE_EXT]という文字列を作る*/
    char tableFileName[MAX_FILENAME+10];
    int len = strlen(tableName) + strlen(DEF_FILE_EXT) + 1;

    memset(tableFileName, '\0', strlen(tableFileName));
    snprintf(tableFileName, len, "%s%s", tableName, DEF_FILE_EXT);

    /*tableinfoをmalloc*/
    tableinfo = malloc(sizeof(TableInfo));
    if(tableinfo == NULL){
        printErrorMessage(ERR_MSG_MALLOC, __func__, __LINE__);
        return NULL;
    }
    /*tableinfoを初期化*/
    memset(tableinfo, 0, sizeof(TableInfo));

    /*tableNameという名前のファイルを開く*/
    if((file=openFile(tableFileName)) == NULL){
        printErrorMessage(ERR_MSG_OPEN, __func__, __LINE__);
        free(tableinfo);
        return NULL;
    }
    
    /*それぞれのデータを取り出し、新しく作ったTableInfoに代入する*/
    if(readPage(file, 0, page) == NG){
        printErrorMessage(ERR_MSG_READ, __func__, __LINE__);
        free(tableinfo);
        return NULL;
    }
    
    p = page;

    memcpy(&num, p, sizeof(int));
    p += sizeof(int);
    tableinfo->numField = num;

    for (i = 0; i < num; i++){
        /*i番目の名前の取得*/
        memcpy(name, p, MAX_FIELD_NAME);
        p += MAX_FIELD_NAME;
        strcpy(tableinfo->fieldInfo[i].name, name);

        /*i番目のデータ型の取得*/
        memcpy(&data, p, sizeof(int));
        p += sizeof(int);
        tableinfo->fieldInfo[i].dataType = data;
    }
        

    /*ファイルをクローズする*/
    if(closeFile(file) == NG){
        printErrorMessage(ERR_MSG_CLOSE, __func__, __LINE__);
        free(tableinfo);
        return NULL;
    }

    /*TableInfoを返す*/
    return tableinfo;
}

/*
 * freeTableInfo -- データ定義情報を収めたメモリ領域の解放
 *
 * 引数:
 *	TableInfo : 
 *
 * 返り値:
 *	なし
 *
 * ***注意***
 *	関数getTableInfoが返すデータ定義情報を収めたメモリ領域は、
 *	不要になったら必ずこの関数で解放すること。
 */
void freeTableInfo(TableInfo *tableInfo)
{
    free(tableInfo);
}

/*
 * printTableInfo -- テーブルのデータ定義情報を表示する(動作確認用)
 *
 * 引数:
 *  tableName: 情報を表示するテーブルの名前
 *
 * 返り値:
 *  なし
 */
void printTableInfo(char *tableName)
{
    TableInfo *tableInfo;
    int i;

    /* テーブル名を出力 */
    printf("\nTable %s\n", tableName);

    /* テーブルの定義情報を取得する */
    if ((tableInfo = getTableInfo(tableName)) == NULL) {
    /* テーブル情報の取得に失敗したので、処理をやめて返る */
    return;
    }

    /* フィールド数を出力 */
    printf("number of fields = %d\n", tableInfo->numField);

    /* フィールド情報を読み取って出力 */
    for (i = 0; i < tableInfo->numField; i++) {
    /* フィールド名の出力 */
    printf("  field %d: name = %s, ", i + 1, tableInfo->fieldInfo[i].name);

    /* データ型の出力 */
    printf("data type = ");
    switch (tableInfo->fieldInfo[i].dataType) {
    case TYPE_INTEGER:
        printf("integer\n");
        break;
    case TYPE_STRING:
        printf("string\n");
        break;
    default:
        printf("unknown\n");
    }
    }

    /* データ定義情報を解放する */
    freeTableInfo(tableInfo);

    return;
}

