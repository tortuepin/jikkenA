/*
 * datamanip.c -- データ操作モジュール
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include "microdb.h"
#include "error.h"
/*
 * DATA_FILE_EXT -- データファイルの拡張子
 */
#define DATA_FILE_EXT ".dat"


Result checkDistinct(RecordSet *recordSet, RecordData *data, Condition *condition);

/*
 * initializeDataManipModule -- データ操作モジュールの初期化
 *
 * 引数:
 *	なし
 *
 * 返り値;
 *	成功ならOK、失敗ならNGを返す
 */
Result initializeDataManipModule()
{
    return OK;
}

/*
 * finalizeDataManipModule -- データ操作モジュールの終了処理
 *
 * 引数:
 *	なし
 *
 * 返り値;
 *	成功ならOK、失敗ならNGを返す
 */
Result finalizeDataManipModule()
{
    return OK;
}

/*
 * getRecordSize -- 1レコード分の保存に必要なバイト数の計算
 *
 * 引数:
 *	tableInfo: データ定義情報を収めた構造体
 *
 * 返り値:
 *	tableInfoのテーブルに収められた1つのレコードを保存するのに
 *	必要なバイト数
 */
static int getRecordSize(TableInfo *tableInfo)
{
    int total = 0;
    int i;

    for (i = 0; i < tableInfo->numField; i++) {
        /* i番目のフィールドがINT型かSTRING型か調べる */
        switch (tableInfo->fieldInfo[i].dataType) {
            case TYPE_INTEGER:
                /* INT型ならtotalにsizeof(int)を加算 */
                total += sizeof(int);
                break;
            case TYPE_STRING:
                /* STRING型ならtotalにMAX_STRINGを加算 */
                total += MAX_STRING;
                break;
            case TYPE_UNKNOWN:
                break;
        }

    }

    /* フラグの分の1を足す */
    total++;

    return total;
}

/*
 * insertRecord -- レコードの挿入
 *
 * 引数:
 *	tableName: レコードを挿入するテーブルの名前
 *	recordData: 挿入するレコードのデータ
 *
 * 返り値:
 *	挿入に成功したらOK、失敗したらNGを返す
 */
Result insertRecord(char *tableName, RecordData *recordData)
{
    TableInfo *tableInfo;
    int numPage;
    char *record;
    char *p;
    char page[PAGE_SIZE];
    char *filename;
    int i;
    int j;
    int recordSize;
    int len;
    File *file;

    /* テーブルの情報を取得する */
    if ((tableInfo = getTableInfo(tableName)) == NULL) {
        printErrorMessage(ERR_MSG_READ, __func__, __LINE__);
        return NG;
    }

    /* 1レコード分のデータをファイルに収めるのに必要なバイト数を計算する */
    recordSize = getRecordSize(tableInfo);

    /* 必要なバイト数分のメモリを確保する */
    if ((record = (char *)malloc(recordSize)) == NULL) {
        printErrorMessage(ERR_MSG_MALLOC, __func__, __LINE__);
    }
    p = record;

    /* 先頭に、「使用中」を意味するフラグを立てる */
    memset(p, 1, 1);
    p += 1;

    /* 確保したメモリ領域に、フィールド数分だけ、順次データを埋め込む */
    for (i = 0; i < tableInfo->numField; i++) {
        switch (tableInfo->fieldInfo[i].dataType) {
            case TYPE_INTEGER:
                memcpy(p, &(recordData->fieldData[i].intValue), sizeof(int));
                p += sizeof(int);
                break;
            case TYPE_STRING:
                memcpy(p, &(recordData->fieldData[i].stringValue), sizeof(char)*MAX_STRING);
                p += MAX_STRING;
                break;
            default:
                /* ここにくることはないはず */
                freeTableInfo(tableInfo);
                free(record);
                assert(0);
                return NG;
        }
    }

    /* 使用済みのtableInfoデータのメモリを解放する */
    freeTableInfo(tableInfo);

    /*
     * ここまでで、挿入するレコードの情報を埋め込んだバイト列recordができあがる
     */

    /*[tableName].datという文字列を作る*/
    len = strlen(tableName) + strlen(DATA_FILE_EXT) + 1;
    if ((filename = malloc(len)) == NULL ){
        printErrorMessage(ERR_MSG_MALLOC, __func__, __LINE__);
        return NG;
    }
    snprintf(filename, len, "%s%s", tableName, DATA_FILE_EXT);



    /* [tableName].datというファイルがなかったら作る*/
    if(getNumPages(filename) == -1){
        if(createFile(filename) != OK){
            printErrorMessage(ERR_MSG_CREATE, __func__, __LINE__);
            return NG;
        }
    }

    /* データファイルをオープンする */
    if((file = openFile(filename)) == NULL){
        return NG;
    }




    /* データファイルのページ数を調べる */
    numPage = getNumPages(filename);

    free(filename);


    /*pageの初期化 */
    memset(page, 0, PAGE_SIZE);

    /* レコードを挿入できる場所を探す */
    for (i = 0; i < numPage; i++) {
        /* 1ページ分のデータを読み込む */
        if (readPage(file, i, page) != OK) {
            free(record);
            closeFile(file);
            printErrorMessage(ERR_MSG_READ, __func__, __LINE__);
            return NG;
        }

        /* pageの先頭からrecordSizeバイトずつ飛びながら、先頭のフラグが「0」(未使用)の場所を探す */
        for (j = 0; j < (PAGE_SIZE / recordSize); j++) {
            char *q;
            q = page+(j*recordSize);
            if (*q == 0) {
                /* 見つけた空き領域に上で用意したバイト列recordを埋め込む */
                memcpy(q, record, recordSize);

                /* ファイルに書き戻す */
                if (writePage(file, i, page) != OK) {
                    free(record);
                    closeFile(file);
                    printErrorMessage(ERR_MSG_WRITE, __func__, __LINE__);
                    return NG;
                }
                closeFile(file);
                free(record);
                return OK;
            }
        }
    }

    /*
     * ファイルの最後まで探しても未使用の場所が見つからなかったら
     * ファイルの最後に新しく空のページを用意し、そこに書き込む
     */

    /* PAGE_SIZEバイト分の大きさを持つ配列pageを初期化*/
    memset(page, 0, PAGE_SIZE);
    /* recordを埋め込む */
    memcpy(page, record, recordSize);
    /* ファイルに書き込む */
    writePage(file, numPage, page);


    closeFile(file);
    free(record);
    return OK;
}



/*
 * checkCondition -- レコードが条件を満足するかどうかのチェック
 *
 * 引数:
 *	recordData: チェックするレコード
 *	condition: チェックする条件
 *
 * 返り値:
 *	レコードrecordが条件conditionを満足すればOK、満足しなければNGを返す
 */
Result checkCondition(RecordData *recordData, Condition *condition)
{
    /*フィールド名の確認*/
    int i=0;
    while(1){
        if(strcmp(recordData->fieldData[i].name, condition->name)==0){
            break;
        }
        i++;
    }


    /*比較演算子を確認*/
    switch(condition->operator){
        case OPR_EQUAL:
            if(condition->dataType == TYPE_INTEGER){
                if(condition->intValue == recordData->fieldData[i].intValue){
                    return OK;
                }
                else return NG;
            }
            else{
                if(strcmp(condition->stringValue, recordData->fieldData[i].stringValue)==0){
                    return OK;
                }
                else return NG;
            }
            break;

        case OPR_NOT_EQUAL:

            if(condition->dataType == TYPE_INTEGER){
                if(condition->intValue != recordData->fieldData[i].intValue){
                    return OK;
                }
                else return NG;
            }
            else{
                if(strcmp(condition->stringValue, recordData->fieldData[i].stringValue)!=0){
                    return OK;
                }
                else return NG;
            }
            break;


        case OPR_GREATER_THAN:

            if(condition->dataType == TYPE_INTEGER){

                if(condition->intValue < recordData->fieldData[i].intValue){
                    return OK;
                }
                else return NG;
            }
            else{

                if(strcmp(condition->stringValue, recordData->fieldData[i].stringValue)<0){
                    return OK;
                }
                else return NG;
            }

        case OPR_LESS_THAN:

            if(condition->dataType == TYPE_INTEGER){

                if(condition->intValue > recordData->fieldData[i].intValue){
                    return OK;
                }
                else return NG;
            }
            else{
                if(strcmp(condition->stringValue, recordData->fieldData[i].stringValue)>0){
                    return OK;
                }
                else return NG;
            }
            break;
    }        




}

/*
 * selectRecord -- レコードの検索
 *
 * 引数:
 *	tableName: レコードを検索するテーブルの名前
 *	condition: 検索するレコードの条件
 *
 * 返り値:
 *	検索に成功したら検索されたレコード(の集合)へのポインタを返し、
 *	検索に失敗したらNULLを返す。
 *	検索した結果、該当するレコードが1つもなかった場合も、レコードの
 *	集合へのポインタを返す。
 *
 * ***注意***
 *	この関数が返すレコードの集合を収めたメモリ領域は、不要になったら
 *	必ずfreeRecordSetで解放すること。
 */
RecordSet *selectRecord(char *tableName, Condition *condition)
{   

    File *file;
    int len;
    char* filename;
    TableInfo *tableInfo;
    char page[PAGE_SIZE];
    int numPage;
    int i, j, k;
    int recordSize;
    RecordData *recordData;
    RecordSet *recordSet;


    /*レコードセットの初期化*/
    if((recordSet = (RecordSet *)malloc(sizeof(RecordSet))) == NULL){
        printErrorMessage(ERR_MSG_MALLOC, __func__, __LINE__);
        return NULL;
    }
    recordSet->numRecord=0;
    recordSet->recordData=NULL;
    /*[tableName].datという文字列を作る*/
    len = strlen(tableName) + strlen(DATA_FILE_EXT) + 1;
    if ((filename = malloc(len)) == NULL ){
        printErrorMessage(ERR_MSG_MALLOC, __func__, __LINE__);
        return NULL;
    }
    snprintf(filename, len, "%s%s", tableName, DATA_FILE_EXT);


    /*データファイルをオープン*/
    if((file = openFile(filename)) == NULL){
        printErrorMessage(ERR_MSG_OPEN, __func__, __LINE__);
        free(filename);
        return NULL;
    }

    /*データ構造を読み取る*/
    if((tableInfo = getTableInfo(tableName)) == NULL){
        printErrorMessage(ERR_MSG_READ, __func__, __LINE__);
        closeFile(file);
        free(tableInfo);
        return NULL;
    }

    /*ページ数を取得*/
    if((numPage = getNumPages(filename)) == -1){
        printErrorMessage(ERR_MSG_STAT, __func__, __LINE__);
        closeFile(file);
        free(filename);
        return NULL;
    }

    /*レコードサイズの取得*/
    recordSize = getRecordSize(tableInfo);
    free(filename);
    /*ページ数分だけループ*/
    for(i=0; i<numPage; i++){

        /*一ページ読みこむ*/
        if(readPage(file, i, page)){
            closeFile(file);
            printErrorMessage(ERR_MSG_READ, __func__, __LINE__);
            return NULL;
        }


        /*recordSizeごとに処理*/
        for(j=0; j < (PAGE_SIZE/recordSize); j++){
            char *p;
            p = page + recordSize * j;

            /*先頭のフラグを調べる*/
            if(*p == 1){
                p++;
                /*recordDataのメモリ確保*/
                if((recordData = (RecordData *)malloc(sizeof(RecordData))) == NULL){
                    printErrorMessage(ERR_MSG_MALLOC, __func__, __LINE__);
                    freeTableInfo(tableInfo);
                    closeFile(file);
                    return NULL;
                }
                /*レコードのデータをRecordDataへ*/
                recordData -> numField = tableInfo -> numField;
                recordData -> next = NULL;
                for(k=0; k < (tableInfo -> numField); k++){
                    strcpy(recordData->fieldData[k].name, tableInfo->fieldInfo[k].name);
                    recordData -> fieldData[k].dataType = tableInfo -> fieldInfo[k].dataType;
                    switch (tableInfo -> fieldInfo[k].dataType){
                        case TYPE_INTEGER:
                            memcpy(&(recordData -> fieldData[k].intValue), p, sizeof(int));
                            p += sizeof(int);
                            break;
                        case TYPE_STRING:
                            memcpy(&(recordData -> fieldData[k].stringValue), p, MAX_STRING);
                            p += MAX_STRING;
                            break;
                        default:
                            /*ここには来ない*/
                            freeTableInfo(tableInfo);
                            free(recordData);
                            closeFile(file);
                            return NULL;
                    }
                }


                /*条件に合ったらRecordSetに挿入*/
                if(checkCondition(recordData, condition) == OK && checkDistinct(recordSet, recordData, condition) == OK){
                    RecordData *r;
                    r = recordSet->recordData;
                    if(r==NULL){
                        recordSet->recordData=recordData;
                        recordSet->numRecord=1;
                    }
                    else{
                        while(r->next!=NULL){
                            /*終わりを見つける*/
                            r=r->next;
                        }
                        r->next = recordData;
                        recordSet->numRecord++;
                    }
                }
            }
        }
    }
    freeTableInfo(tableInfo);
    if((closeFile(file) != OK)){
        printErrorMessage(ERR_MSG_STAT, __func__, __LINE__);
        return NULL;
    }

    return recordSet;

}
/*
 * fieldDataCmp
 *
 * フィールドデータを比較
 * 同じならOK
 *
 */
Result fieldDataCmp(FieldData a, FieldData b){
    if(a.dataType != b.dataType){
        return NG;
    }
    if(strcmp(a.name, b.name) != 0){
        return NG;
    }
    if(a.dataType == TYPE_STRING){
        if(strcmp(a.stringValue, b.stringValue) != 0){
            return NG;
        }
    }
    else if(a.dataType == TYPE_INTEGER){
        if(a.intValue != b.intValue){
            return NG;
        }
    }

    return OK;
}
    



/*
 * checkDistinct
 *
 * 重複確認
 *
 * 重複しなければ OK
 */
Result checkDistinct(RecordSet *recordSet, RecordData *data, Condition *condition){
    RecordData *r;
    r = recordSet->recordData;
    int i;
    int num = data->numField;

    if(condition->distinct == NOT_DISTINCT){
        return OK;
    }
    //rがnullなら重複するはずなし
    if(r == NULL){
        return OK;
    }
    while(r!=NULL){
       if(r->numField==data->numField){
           for(i=0;i<num;i++){
              if(fieldDataCmp(r->fieldData[i], data->fieldData[i]) != OK){
                 break; 
              }
           }
       }
       r=r->next;
    }
    if(i==num){
        return NG;
    }
    return OK;
}


/*
 * freeRecordSet -- レコード集合の情報を収めたメモリ領域の解放
 *
 * 引数:
 *	recordSet: 解放するメモリ領域
 *
 * 返り値:
 *	なし
 *
 * ***注意***
 *	関数selectRecordが返すレコードの集合を収めたメモリ領域は、
 *	不要になったら必ずこの関数で解放すること。
 */
void freeRecordSet(RecordSet *recordSet)
{
    RecordData *p, *n;
    p = recordSet ->recordData;

    while(p != NULL){
        n = p->next;
        free(p);
        p = n;
    }

    free(recordSet);

}

/*
 * deleteRecord -- レコードの削除
 *
 * 引数:
 *	tableName: レコードを削除するテーブルの名前
 *	condition: 削除するレコードの条件
 *
 * 返り値:
 *	削除に成功したらOK、失敗したらNGを返す
 */
Result deleteRecord(char *tableName, Condition *condition)
{
    int len;
    int recordSize;
    int numPage;
    int i, j, k;
    File *file;
    TableInfo *tableInfo;
    char *filename;
    char page[PAGE_SIZE];
    int delcatch = 0;


    /*[tableName].datという文字列をつくる*/
    len = strlen(tableName) + strlen(DATA_FILE_EXT) + 1;
    if((filename = malloc(len)) == NULL){
        printErrorMessage(ERR_MSG_MALLOC, __func__, __LINE__);
        return NG;
    }
    snprintf(filename, len, "%s%s", tableName, DATA_FILE_EXT);

    if((file=openFile(filename)) == NULL){
        printErrorMessage(ERR_MSG_OPEN, __func__, __LINE__);
        free(filename);
        return NG;
    }

    /*ページ数、tableInfo、レコードのサイズを取得*/
    numPage = getNumPages(filename);
    tableInfo = getTableInfo(tableName);
    recordSize = getRecordSize(tableInfo);


    free(filename);

    /*レコードを一つずつ取り出し、条件を満足するかどうかチェックする*/
    for (i=0; i<numPage; i++){
        /*1ページぶんのデータを読み込む*/
        if (readPage(file, i, page) != OK){
            closeFile(file);
            printErrorMessage(ERR_MSG_READ, __func__, __LINE__);
            return NG;
        }

        /*pageの先頭からrecord_sizeバイトずつ切り取って処理する*/
        for (j=0; j<(PAGE_SIZE/recordSize); j++){
            RecordData *recordData;
            char *p;

            /*先頭フラグが0なら読み飛ばす*/
            p = &page[recordSize * j];
            if(*p == 0){
                continue;
            }

            /*RecordData構造体のためのメモリを確保する*/
            if((recordData = (RecordData *)malloc(sizeof(RecordData))) == NULL){
                closeFile(file);
                printErrorMessage(ERR_MSG_MALLOC, __func__, __LINE__);
                return NG;
            }

            /*フラグぶんだけポインタを進める*/
            p++;


            /*レコードのデータをRecordDataへ*/
            recordData -> numField = tableInfo -> numField;
            recordData -> next = NULL;
            for(k=0; k < (tableInfo -> numField); k++){
                strcpy(recordData->fieldData[k].name, tableInfo->fieldInfo[k].name);
                recordData -> fieldData[k].dataType = tableInfo -> fieldInfo[k].dataType;
                switch (tableInfo -> fieldInfo[k].dataType){
                    case TYPE_INTEGER:
                        memcpy(&(recordData -> fieldData[k].intValue), p, sizeof(int));
                        p += sizeof(int);
                        break;
                    case TYPE_STRING:
                        memcpy(&(recordData -> fieldData[k].stringValue), p, MAX_STRING);
                        p += MAX_STRING;
                        break;
                    default:
                        /*ここには来ない*/
                        freeTableInfo(tableInfo);
                        closeFile(file);
                        free(recordData);
                        return NG;
                }
            }

            /*条件を満たしたレコードを削除*/
            if(checkCondition(recordData, condition) == OK){
                page[recordSize * j] = 0;
                delcatch = 1;
            }
            free(recordData);
        }

        /*delcatchの値が1の場合、ページの内容を書き戻す*/
        if(delcatch == 1){
            if(writePage(file, i, page) != OK){
                closeFile(file);
                printErrorMessage(ERR_MSG_WRITE, __func__, __LINE__);
                return NG;
            }
        }
    }

    /*ファイルを閉じる*/
    if((closeFile(file)) != OK){
        return NG;
    }
    return OK;
}

/*
 * createDataFile -- データファイルの作成
 *
 * 引数:
 *	tableName: 作成するテーブルの名前
 *
 * 返り値:
 *	作成に成功したらOK、失敗したらNGを返す
 */
Result createDataFile(char *tableName)
{
    int len;
    char *filename;

    /*[tableName].datという文字列をつくる*/
    len = (int)strlen(tableName) + strlen(DATA_FILE_EXT) + 1;
    if((filename = malloc(len)) == NULL){
        printErrorMessage(ERR_MSG_MALLOC, __func__, __LINE__);
        return NG;
    }
    snprintf(filename, len, "%s%s", tableName, DATA_FILE_EXT);

    if((createFile(filename)) == NG){
        printErrorMessage(ERR_MSG_CREATE, __func__, __LINE__);
        return NG;
    }
    return OK;
}

/*
 * deleteDataFile -- データファイルの削除
 *
 * 引数:
 *	tableName: 削除するテーブルの名前
 *
 * 返り値:
 *	削除に成功したらOK、失敗したらNGを返す
 */
Result deleteDataFile(char *tableName)
{

    int len;
    char *filename;

    /*[tableName].datという文字列をつくる*/
    len = (int)strlen(tableName) + strlen(DATA_FILE_EXT) + 1;
    if((filename = malloc(len)) == NULL){
        printErrorMessage(ERR_MSG_MALLOC, __func__, __LINE__);
        return NG;
    }
    snprintf(filename, len, "%s%s", tableName, DATA_FILE_EXT);

    if((deleteFile(filename)) == NG){
        printErrorMessage(ERR_MSG_UNLINK, __func__, __LINE__);
        return NG;
    }
    return OK;
}

/*
 * printTableData -- すべてのデータの表示(テスト用)
 *
 * 引数:
 *	tableName: データを表示するテーブルの名前
 */
void printTableData(char *tableName)
{
    TableInfo *tableInfo;
    File *file;
    int len;
    int i, j, k;
    int recordSize;
    int numPage;
    char *filename;
    char page[PAGE_SIZE];

    /* テーブルのデータ定義情報を取得する */
    if ((tableInfo = getTableInfo(tableName)) == NULL) {
        return;
    }

    /* 1レコード分のデータをファイルに収めるのに必要なバイト数を計算する */
    recordSize = getRecordSize(tableInfo);

    /* データファイルのファイル名を保存するメモリ領域の確保 */
    len = strlen(tableName) + strlen(DATA_FILE_EXT) + 1;
    if ((filename = malloc(len)) == NULL) {
        freeTableInfo(tableInfo);
        return;
    }

    /* ファイル名の作成 */
    snprintf(filename, len, "%s%s", tableName, DATA_FILE_EXT);

    /* データファイルのページ数を求める */
    numPage = getNumPages(filename);

    /* データファイルをオープンする */
    if ((file = openFile(filename)) == NULL) {
        free(filename);
        freeTableInfo(tableInfo);
        return;
    }

    free(filename);

    /* レコードを1つずつ取りだし、表示する */
    for (i = 0; i < numPage; i++) {
        /* 1ページ分のデータを読み込む */
        readPage(file, i, page);

        /* pageの先頭からrecord_sizeバイトずつ切り取って処理する */
        for (j = 0; j < (PAGE_SIZE / recordSize); j++) {
            /* 先頭の「使用中」のフラグが0だったら読み飛ばす */
            char *p = &page[recordSize * j];
            if (*p == 0) {
                continue;
            }

            /* フラグの分だけポインタを進める */
            p++;

            /* 1レコード分のデータを出力する */
            for (k = 0; k < tableInfo->numField; k++) {
                int intValue;
                char stringValue[MAX_STRING];

                printf("Field %s = ", tableInfo->fieldInfo[k].name);

                switch (tableInfo->fieldInfo[k].dataType) {
                    case TYPE_INTEGER:
                        memcpy(&intValue, p, sizeof(int));
                        p += sizeof(int);
                        printf("%d\n", intValue);
                        break;
                    case TYPE_STRING:
                        memcpy(stringValue, p, MAX_STRING);
                        p += MAX_STRING;
                        printf("%s\n", stringValue);
                        break;
                    default:
                        /* ここに来ることはないはず */
                        return;
                }
            }

            printf("\n");
        }
    }
}

/*
 * printRecordSet -- レコード集合の表示
 *
 * 引数:
 *	recordSet: 表示するレコード集合
 *
 * 返り値:
 *	なし
 */
void printRecordSet(RecordSet *recordSet)
{
    RecordData *record;
    int i, j, k;

    /* レコード数の表示 */
    printf("Number of Records: %d\n", recordSet->numRecord);

    /* レコードを1つずつ取りだし、表示する */
    for (record = recordSet->recordData; record != NULL; record = record->next) {
        /* すべてのフィールドのフィールド名とフィールド値を表示する */
        for (i = 0; i < record->numField; i++) {
            printf("Field %s = ", record->fieldData[i].name);

            switch (record->fieldData[i].dataType) {
                case TYPE_INTEGER:
                    printf("%d\n", record->fieldData[i].intValue);
                    break;
                case TYPE_STRING:
                    printf("%s\n", record->fieldData[i].stringValue);
                    break;
                default:
                    /* ここに来ることはないはず */
                    return;
            }
        }

        printf("\n");
    }
}
