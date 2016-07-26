/*
 * datamanip.c -- データ操作モジュール
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "microdb.h"

/*
 * DATA_FILE_EXT -- データファイルの拡張子
 */
#define DATA_FILE_EXT ".dat"

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

    for (i=0; i < tableInfo -> numField; i++) {
      /* i番目のフィールドがINT型かSTRING型か調べる */
      /* INT型ならtotalにsizeof(int)を加算 */
      /* STRING型ならtotalにMAX_STRINGを加算 */
      /* INT型ならtotalにsizeof(int)を加算 */
      if( tableInfo -> fieldInfo[i].dataType == TYPE_INTEGER){
	  total += sizeof(int);
        }
        /* STRING型ならtotalにMAX_STRINGを加算 */
        if(tableInfo -> fieldInfo[i].dataType == TYPE_STRING){
	  total += MAX_STRING;
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
    int recordSize;
    int numPage;
    char *record;
    char *p;
    char *filename;
    File *file;
    int i,j,len;
    char page[PAGE_SIZE];
    
    /* テーブルの情報を取得する */
    if ((tableInfo = getTableInfo(tableName)) == NULL) {
      printf("テーブル情報の取得に失敗しました\n");
      return NG;
    }
    
    /* 1レコード分のデータをファイルに収めるのに必要なバイト数を計算する */
    recordSize = getRecordSize(tableInfo);
    
    /* 必要なバイト数分のメモリを確保する */
    if ((record = malloc(recordSize)) == NULL) {
      printf("insertRecord内でrecordのメモリ確保に失敗しました\n");
    }
    p = record;
    
    /* 先頭に、「使用中」を意味するフラグを立てる */
    memset(p, 1, 1);
    p += 1;
    
    /* 確保したメモリ領域に、フィールド数分だけ、順次データを埋め込む */
    for (i = 0; i < tableInfo->numField; i++) {
      switch (tableInfo->fieldInfo[i].dataType) {
      case TYPE_INTEGER:
	memcpy(p, &(recordData -> fieldData[i].intValue) , sizeof(int));
	p += sizeof(int);
	break;
      case TYPE_STRING:
	memcpy(p, &(recordData -> fieldData[i].stringValue), MAX_STRING);
	p += MAX_STRING;
	break;
      default:
	/* ここにくることはないはず */
	printf("ここにこないはず\n");
	freeTableInfo(tableInfo);
	free(record);
	return NG;
      }
    }
    
    /* 使用済みのtableInfoデータのメモリを解放する */
    freeTableInfo(tableInfo);

    /*
     * ここまでで、挿入するレコードの情報を埋め込んだバイト列recordができあがる
     */

    /*[tableName].datの文字列を作る*/
    len = strlen(tableName) + strlen(DATA_FILE_EXT) + 1;
    if ((filename = malloc(len)) == NULL) {
      return NG;
    }
    
    snprintf(filename, len, "%s%s", tableName, DATA_FILE_EXT);
    
    /* [tableName].datというファイルがなかったら作る */
    if(getNumPages(filename) == -1){
      if (createFile(filename) != OK);
    }

    if((file = openFile(filename)) == NULL){
      return NG;
    }
    
    /* データファイルのページ数を調べる */
    numPage = getNumPages(filename);

    /* pageの初期化 */
    memset(page, 0, PAGE_SIZE);
    
    /* レコードを挿入できる場所を探す */
    for (i = 0; i < numPage; i++) {
      /* 1ページ分のデータを読み込む */
      if (readPage(file, i, page) != OK) {
	free(record);
	return NG;
      }
      
      /* pageの先頭からrecordSizeバイトずつ飛びながら、先頭のフラグが「0」(未使用)の場所を探す */
      for (j = 0; j < (PAGE_SIZE / recordSize); j++) {
	char *q;
	q = page;
	q = q + recordSize*j;
	
	if (*q == 0) {
	  /* 見つけた空き領域に上で用意したバイト列recordを埋め込む */
	  memcpy(q, record, recordSize);
	  
	  /* ファイルに書き戻す */
	  if (writePage(file, i, page) != OK) {
	    free(record);
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
    
    char Epage[PAGE_SIZE];
    memset(Epage , 0 , PAGE_SIZE);
    /*空ページの書き込み*/
    if(writePage(file , numPage, Epage) != OK){
      return NG;
    }
    /*numPageが0のとき（初めてデータを書き込むとき）のみ*/
    if(numPage == 0){
      char *q;
      q = page;
      /*レコードを書き込む*/
      memcpy(q, record , recordSize);
    }
    
    /* ファイルに書き戻す */
    if (writePage(file, numPage , page) != OK) {
      free(record);
      return NG;
    }
    closeFile(file);
    free(record);
    return OK;
}


/* ------ ■■これ以降の関数は、次回以降の実験で説明の予定 ■■ ----- */

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
static Result checkCondition(RecordData *recordData, Condition *condition)
{
  int i;

  /*条件conditionに指定されているフィールド名をrecordから探す*/
  for(i = 0; i < recordData -> numField; i++){
    /*フィールド名が一致しているかcheck*/
    if(strcmp(recordData -> fieldData[i].name, condition -> name) == 0){
      
      /*比較演算子を満たすかのcheck*/
      /*まずデータが文字列の場合。一致するかどうかの判断*/
      if(recordData -> fieldData[i].dataType == TYPE_STRING){
	switch(condition -> operator){
	case OPR_EQUAL :/* = */
	  if(strcmp(recordData -> fieldData[i].stringValue, condition -> stringValue) == 0){
	  }else{
	    return NG;
	  }
	  break;
	case OPR_NOT_EQUAL :/* != */
	  if(strcmp(recordData -> fieldData[i].stringValue, condition -> stringValue) != 0){
	  }else{
	    return NG;
	  }
	  break;
	case OPR_GREATER_THAN :/* > */
	  return NG;
	case OPR_LESS_THAN :/* < */
	  return NG;
	default :
	  return NG;
	}
      }
      
      /*データが数値(integer型)の場合。各比較演算子でcheck*/
      if(recordData -> fieldData[i].dataType == TYPE_INTEGER){
	switch(condition -> operator){
	case OPR_EQUAL :/* = */
	  if(recordData -> fieldData[i].intValue == condition -> intValue){
	  }else{
	    return NG;
	  }
	  break;
	case OPR_NOT_EQUAL :/* != */
	  if(recordData -> fieldData[i].intValue != condition -> intValue){
	  }else{
	    return NG;
	  }
	  break;
	case OPR_GREATER_THAN :/* > */
	  if(recordData ->fieldData[i].intValue > condition -> intValue){
	  }else{
	    return NG;
	  }
	  break;
	case OPR_LESS_THAN :/* < */
	  if(recordData -> fieldData[i].intValue < condition -> intValue){
	  }else{
	    return NG;
	  }
	  break;
	default :
	  return NG;
	}
      }
      if(recordData -> fieldData[i].dataType == TYPE_UNKNOWN){
	return NG;
      }
    }
  }
  return OK;
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
  RecordSet *recordSet;
  RecordData *recordData;
  File *file;
  TableInfo *tableInfo;
  long len;
  char *filename;
  int numPage;
  char page[PAGE_SIZE];
  int recordSize;
  int i,j,k;

  /*レコードセットの初期化*/
  if((recordSet = (RecordSet *)malloc(sizeof(RecordSet))) == NULL){
    printf("レコードセットの「初期化に失敗しました\n");
    return NULL;
  }
  recordSet -> numRecord = 0;
  recordSet -> recordData = NULL;

  /*[tableName].datという文字列を作る*/
  len = strlen(tableName) + strlen(DATA_FILE_EXT) + 1;
  if((filename = malloc(len)) == NULL){
    return NULL;
  }
  snprintf(filename, len, "%s%s", tableName, DATA_FILE_EXT);

  /*ファイルのオープン*/
  if((file = openFile(filename)) == NULL){
    return NULL;
  }

  /*ページ数の取得*/
  numPage = getNumPages(filename);
  /*テーブル情報の取得*/
  tableInfo = getTableInfo(tableName);
  /*レコードサイズの取得*/
  recordSize = getRecordSize(tableInfo);

  /*データが挿入されていない場合、そのままrecordSetを返す*/
  if(numPage == 0){
    freeTableInfo(tableInfo);
    if((closeFile(file)) != OK){
      return NULL;
    }
    return recordSet;
  }

  /*ページ数だけループする*/
  for(i=0; i < numPage; i++){
    /*1ページ分読み込む*/
    if(readPage(file, i, page) != OK){
      printf("selectRecord内でファイルの読み込みに失敗しました\n");
      return NULL;
    }

    /*pageの先頭からrecordSizeバイトごとに切って処理*/
    for(j=0; j < (PAGE_SIZE/recordSize); j++){
      char *p;
      p = page + recordSize * j;

      /*先頭が1なら使用中なのでレコードを読み込む*/
      if(*p==1){
	/*0or1のフラグ分ポインタを先へ*/
	p++;
	/* RecordData構造体のメモリ確保 */
	if((recordData = (RecordData *)malloc(sizeof(RecordData))) == NULL){
	  printf("selectRecord内でRecordData構造体のメモリ確保に失敗しました\n");
	}

	/*1レコード分のデータをRecordData構造体へ*/
	recordData -> numField = tableInfo -> numField;
	recordData -> next = NULL;

	/*フィールド数だけループして、データ型ごとに読み込んで構造体を作る*/
	for(k=0; k < (tableInfo -> numField); k++){
	  /*フィールド情報*/
	  strcpy(recordData -> fieldData[k].name, tableInfo -> fieldInfo[k].name);
	  recordData -> fieldData[k].dataType = tableInfo -> fieldInfo[k].dataType;
	  switch (tableInfo -> fieldInfo[k].dataType){
	  case TYPE_INTEGER :
	    memcpy(&(recordData -> fieldData[k].intValue), p ,sizeof(int));
	    p += sizeof(int);
	    break;
	  case TYPE_STRING :
	    memcpy(&(recordData -> fieldData[k].stringValue), p, MAX_STRING);
	    p += MAX_STRING;
	    break;
	  default :
	    /* ここにはこないはず */
	    freeTableInfo(tableInfo);
	    free(recordData);
	    return NULL;
	  }
	}

	/*条件にあったレコードを挿入*/
	if(checkCondition(recordData, condition) == OK){
	  RecordData *r;
	  r = recordSet -> recordData;

	  if(r == NULL){
	    recordSet -> recordData = recordData;
	    recordSet -> tail = recordData;
	    recordSet -> numRecord++;
	    continue;
	  }
	  /*末尾にデータを追加*/
	  recordSet -> tail -> next = recordData;
	  /*追加したデータを末尾に登録*/
	  recordSet -> tail = recordData;
	  /*レコード数を増やす*/
	  recordSet -> numRecord++;
	}
      }
    }
  }

  freeTableInfo(tableInfo);
  if((closeFile(file)) != OK){
    return NULL;
  }
  return recordSet;
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
  RecordData *p;
  p = recordSet -> recordData;

  while(p != NULL){
    p = p -> next;
    free(recordSet -> recordData);
    recordSet -> recordData = p;
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
  File *file;
  TableInfo *tableInfo;
  int numPage;
  char *filename;
  char page[PAGE_SIZE];
  int recordSize;
  int len;
  int i,j,k;

  /*[tableName].dat文字列を作る*/
  len = strlen(tableName) + strlen(DATA_FILE_EXT) + 1;
  if((filename = malloc(len)) == NULL){
    return NG;
  }
  snprintf(filename, len, "%s%s", tableName, DATA_FILE_EXT);

  /*fileopen*/
  if((file = openFile(filename)) == NULL){
    return NG;
  }

  /*ページ数、tableInfo構造体のデータ、レコードのサイズを取得*/
  numPage = getNumPages(filename);
  tableInfo = getTableInfo(tableName);
  recordSize = getRecordSize(tableInfo);

  /*レコードをひとつづつ取り出して条件を満たすかチェック*/
  for(i=0; i < numPage; i++){
    /*1page読み込む*/
    if(readPage(file, i, page) != OK){
      printf("deleteRecord内で1pageの読み込みに失敗しました\n");
      return NG;
    }
    /*pageの先頭からrecord_sizeバイトずつ切り取って処理*/
    for(j=0; j < (PAGE_SIZE / recordSize); j++){
      RecordData *recordData;
      char *p;

      /*先頭のフラグが0なら未使用なので飛ばす*/
      p = &page[recordSize * j];
      if(*p == 0){
	continue;
      }

      /*RecordData構造体のメモリ確保*/
      if((recordData = (RecordData *) malloc(sizeof(RecordData))) == NULL){
	printf("deleteRecord内でRecordData構造体のメモリ確保に失敗しました\n");
	return NG;
      }
      /*フラグ分のポインタ進める*/
      p++;
      /*1レコード分をRecordData構造体に入れる*/
      recordData -> numField = tableInfo -> numField;
      recordData -> next = NULL;
      for(k=0; k < tableInfo -> numField; k++){
	/*TableInfo構造体からRecordData構造体にフィールド名とデータ型をコピー*/
	strcpy(recordData -> fieldData[k].name, tableInfo -> fieldInfo[k].name);
	recordData -> fieldData[k].dataType = tableInfo -> fieldInfo[k].dataType;
	/*フィールド値の読み取り*/
	switch (recordData -> fieldData[k].dataType) {
	case TYPE_INTEGER:
	  memcpy(&(recordData -> fieldData[k].intValue ), p , sizeof(int));
	  p += sizeof(int);
	  break;
	case TYPE_STRING:
	  memcpy(&(recordData -> fieldData[k].stringValue) ,p , MAX_STRING);
	  p += MAX_STRING;
	  break;
	default:
	  /* ここに来ることはないはず */
	  freeTableInfo(tableInfo);
	  free(recordData);
	  return NG;
	}
      }
      if(checkCondition(recordData, condition) == OK){
	/*条件を満たしたのでレコードを削除(フラグを0にする)*/
	page[recordSize * j] = 0;
      }
      free(recordData);
    }
    /*ページの内容を書き戻す*/
    if(writePage(file, i, page) != OK){
      printf("deleteRecord内でページの書き戻しに失敗しました\n");
      return NG;
    }
  }
  if((closeFile(file)) != OK){
    return NG;
  }
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

  /* [tableName].datという文字列を作る */   
  len = (int)strlen(tableName) + strlen(DATA_FILE_EXT) + 1;
  if ((filename = malloc(len)) == NULL) {
    return NG;
  }
  snprintf(filename, len, "%s%s", tableName, DATA_FILE_EXT);
  
  if( (createFile(filename)) == NG){
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

  /* [tableName].datという文字列を作る */   
  len = (int)strlen(tableName) + strlen(DATA_FILE_EXT) + 1;
  if ((filename = malloc(len)) == NULL) {
    return NG;
  }
  snprintf(filename, len, "%s%s", tableName, DATA_FILE_EXT);
  
  if( (deleteFile(filename)) == NG){
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

