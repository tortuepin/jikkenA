/*
 * file.c -- ファイルアクセスモジュール 
 */

#include "microdb.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>





/*
 * modifyFlag -- 変更フラグ
 */
typedef enum { UNMODIFIED = 0, MODIFIED = 1 } modifyFlag;

/*
 * Buffer -- 1ページ分のバッファを記憶する構造体
 */
typedef struct Buffer Buffer;
struct Buffer {
    File *file;				/* バッファの内容が格納されたファイル */
					/* file == NULLならこのバッファは未使用 */
    int pageNum;			/* ページ番号 */
    char page[PAGE_SIZE];		/* ページの内容を格納する配列 */
    struct Buffer *prev;		/* 一つ前のバッファへのポインタ */
    struct Buffer *next;		/* 一つ後ろのバッファへのポインタ */
    modifyFlag modified;		/* ページの内容が更新されたかどうかを示すフラグ */
};

/*
 * bufferListHead -- LRUリストの先頭へのポインタ
 */
static Buffer *bufferListHead = NULL;

/*
 * bufferListTail -- LRUリストの最後へのポインタ
 */
static Buffer *bufferListTail = NULL;


static void moveBufferToListHead(Buffer *buf);
static Result initializeBufferList();
static Result finalizeBufferList();

/*
 * initializeFileModule -- ファイルアクセスモジュールの初期化処理
 *
 * 引数:
 *	なし
 *
 * 返り値:
 *	成功の場合OK、失敗の場合NG
 */
Result initializeFileModule()
{

    if(initializeBufferList() == NG){
        printf("BufferListの初期化に失敗しました");
        return NG;
    }
    return OK;
}

/*
 * finalizeFileModule -- ファイルアクセスモジュールの終了処理
 *
 * 引数:
 *	なし
 *
 * 返り値:
 *	成功の場合OK、失敗の場合NG
 */
Result finalizeFileModule()
{
    if(finalizeBufferList() == NG){
        printf("BufferListの終了処理に失敗");
        return NG;
    }
    return OK;
}

/*
 * createFile -- ファイルの作成
 *
 * 引数:
 *	filename: 作成するファイルのファイル名
 *
 * 返り値:
 *	成功の場合OK、失敗の場合NG
 */
Result createFile(char *filename)
{
    if((creat(filename, S_IREAD | S_IWRITE)) == -1){
        //ERROR
        return NG;
    }
    return OK;
}

/*
 * deleteFile -- ファイルの削除
 *
 * 引数:
 *	filename: 削除するファイルの名前
 *
 * 返り値:
 *	成功の場合OK、失敗の場合NG
 */
Result deleteFile(char *filename)
{
    if(unlink(filename) == -1){
        //ERROR
        return NG;
    }
    return OK;
}

/*
 * openFile -- ファイルのオープン
 *
 * 引数:
 *	filename: 開くファイル名
 *
 * 返り値:
 *	オープンしたファイルのFile構造体
 *	オープンに失敗した場合にはNULLを返す
 */
File *openFile(char *filename)
{
    File *file;
    file = malloc(sizeof(File));
    if(file == NULL){
        //ERROR
        return NULL;
    }
    if ((file->desc = open(filename, O_RDWR)) == -1){
        //ERROR
        free(file);
        return NULL;
    }
    strcpy(file->name, filename);
    return file;
}

/*
 * closeFile -- ファイルのクローズ
 *
 * 引数:
 *	クローズするファイルのFile構造体
 *
 * 返り値:
 *	成功の場合OK、失敗の場合NG
 */
Result closeFile(File *file)
{
    Buffer *buf = NULL;
    /* バッファ探し*/
    /* 見つけたらファイルに書き込む*/
    for (buf = bufferListHead; buf != NULL; buf = buf->next) {
        /* 要求されたページがリストの中にあるかどうかチェックする */
        if (buf->file == file) {
            if(buf->modified == MODIFIED){
                /* 要求されたページがバッファにあったので、その内容をファイルに書き込む */
                if(lseek(buf->file->desc, buf->pageNum*PAGE_SIZE, SEEK_SET) == -1){
                    /* エラー*/
                    printf("close失敗");
                    return NG;
                }
                if (write(buf->file->desc, buf->page, PAGE_SIZE) < PAGE_SIZE) {
                    /* エラー処理 */
                    printf("クローズシッパイ");
                    return NG;
                }

                /* アクセスされたバッファを、空にする */
                buf->file = NULL;
                buf->pageNum = -1;
                buf->modified = UNMODIFIED;
                memset(buf->page, 0, PAGE_SIZE);
            }
        }
    }


    if(close(file->desc) == -1) {
        //ERROR
        return NG;
    }
    free(file);
    return OK;
}
/*
 * readPage -- 1ページ分のデータのファイルからの読み出し
 *
 * 引数:
 *	file: アクセスするファイルのFile構造体
 *	pageNum: 読み出すページの番号
 *	page: 読み出した内容を格納するPAGE_SIZEバイトの領域
 *
 * 返り値:
 *	成功の場合OK、失敗の場合NG
 */
Result readPage(File *file, int pageNum, char *page)
{
    Buffer *buf = NULL;;
    Buffer *emptyBuf = NULL;

    /*
     * 読み出しを要求されたページがバッファに保存されているかどうか、
     * リストの先頭から順に探す
     */
    for (buf = bufferListHead; buf != NULL; buf = buf->next) {
        /* 要求されたページがリストの中にあるかどうかチェックする */
        if (buf->file == file && buf->pageNum == pageNum) {
            /* 要求されたページがバッファにあったので、その内容を引数のpageにコピーする */
            memcpy(page, buf->page, PAGE_SIZE);

            /* アクセスされたバッファを、リストの先頭に移動させる */
            moveBufferToListHead(buf);

            return OK;
        }
        if(buf->file == NULL && buf->pageNum == -1){
            /* bufの中身が空だったら保存 */
            emptyBuf = buf;
        }
    }

    /*
     * emptyBuf==NULLなら空きなし
     * 一番最後のバッファを開ける
     */

    if(emptyBuf == NULL){
        //もし変更フラグが立っていたら書き込む
        if(bufferListTail->modified == MODIFIED){
            if(lseek(bufferListTail->file->desc, PAGE_SIZE*bufferListTail->pageNum, SEEK_SET) == -1){
                //ERROR
                return NG;
            }
            if(write(bufferListTail->file->desc, bufferListTail->page, PAGE_SIZE) == -1){
                //ERROR
                return NG;
            }
            bufferListTail->modified = UNMODIFIED;
        }
        /*初期化してempty*/
        bufferListTail->file = NULL;
        bufferListTail->pageNum = -1;
        memset(bufferListTail->page, 0, PAGE_SIZE);
        emptyBuf = bufferListTail;
    }

    /*
     * lseekとreadシステムコールで空きバッファにファイルの内容を読み込み、
     * Buffer構造体に保存する
     */

    /* 読み出し位置の設定 */
    if (lseek(file->desc, pageNum * PAGE_SIZE, SEEK_SET) == -1) {
        return NG;
    }

    /* 1ページ分のデータの読み出し */
    if (read(file->desc, page, PAGE_SIZE) < PAGE_SIZE) {
        return NG;
    }

    /* バッファの内容を引数のpageにコピー */
    memcpy(emptyBuf->page, page, PAGE_SIZE);

    /* Buffer構造体(emptyBuf)への各種情報の設定 */
    emptyBuf->file = file;
    emptyBuf->pageNum = pageNum;
    

    /* アクセスされたバッファ(emptyBuf)を、リストの先頭に移動させる */
    moveBufferToListHead(emptyBuf);

    return OK;


}

/*
 * writePage -- 1ページ分のデータのファイルへの書き出し
 *
 * 引数:
 *	file: アクセスするファイルのFile構造体
 *	pageNum: 書き出すページの番号
 *	page: 書き出す内容を格納するPAGE_SIZEバイトの領域
 *
 * 返り値:
 *	成功の場合OK、失敗の場合NG
 */
Result writePage(File *file, int pageNum, char *page)
{
    Buffer *buf = NULL;;
    Buffer *emptyBuf = NULL;

    

    /*
     * 書き出しを要求されたページがバッファに保存されているかどうか、
     * リストの先頭から順に探す
     */
    for (buf = bufferListHead; buf != NULL; buf = buf->next) {
        /* 要求されたページがリストの中にあるかどうかチェックする */
        if (buf->file == file && buf->pageNum == pageNum) {
            /* 要求されたページがバッファにあったので、その内容を引数のpageからコピーする */
            memcpy(buf->page, page, PAGE_SIZE);

            /*フラグを書き換える*/
            buf->modified = MODIFIED;

            /* アクセスされたバッファを、リストの先頭に移動させる */
            moveBufferToListHead(buf);

            return OK;
        }

        //ついでに空きを見つける
        if(buf->file == NULL && buf->pageNum == -1){
            emptyBuf = buf;
        }
        
    }



    /*
     * emptuBuf=NUlLなら空きなし
     * 一番最後のバッファを開ける
     */

    if(emptyBuf == NULL){
        //もし変更フラグが立っていたら書き込む
        if(bufferListTail->modified == MODIFIED){
            if(lseek(bufferListTail->file->desc, PAGE_SIZE*bufferListTail->pageNum, SEEK_SET) == -1){
                //ERROR
                return NG;
            }
            if(write(bufferListTail->file->desc, bufferListTail->page, PAGE_SIZE) == -1){
                //ERROR
                return NG;
            }
            bufferListTail->modified = UNMODIFIED;

        }
        /*初期化してemptyに*/
        bufferListTail->file = NULL;
        bufferListTail->pageNum = -1;
        memset(bufferListTail->page, 0, PAGE_SIZE);
        emptyBuf = bufferListTail;
        
    }

    /*あきバッファに変更内容を保存*/
    memcpy(emptyBuf->page, page, PAGE_SIZE);
    
    /*各種情報の設定*/
    emptyBuf->file = file;
    emptyBuf->pageNum = pageNum;
    emptyBuf->modified = MODIFIED;

    /*アクセスされたバッファをリストの先頭に*/
    moveBufferToListHead(emptyBuf);



    return OK;
}

/*
 * getNumPage -- ファイルのページ数の取得
 *
 * 引数:
 *	filename: ファイル名
 *
 * 返り値:
 *	引数で指定されたファイルの大きさ(ページ数)
 *	エラーの場合には-1を返す
 */
int getNumPages(char *filename)
{
    struct stat statBuffer;

    if(stat(filename, &statBuffer) == -1){
        //ERROR
        return -1;
    }
    return (int)statBuffer.st_size/PAGE_SIZE;
    
}





/*********バッファリング*/



/*
 * initializeBufferList -- バッファリストの初期化
 *
 * **注意**
 *	この関数は、ファイルアクセスモジュールを使用する前に必ず一度だけ呼び出すこと。
 *	(initializeFileModule()から呼び出すこと。)
 *
 * 引数:
 *	なし
 *
 * 返り値:
 *	初期化に成功すればOK、失敗すればNGを返す。
 */
static Result initializeBufferList()
{
    Buffer *oldBuf = NULL;
    Buffer *buf;
    int i;

    /*
     * NUM_BUFFER個分のバッファを用意し、
     * ポインタをつないで両方向リストにする
     */
    for (i = 0; i < NUM_BUFFER; i++) {
	/* 1個分のバッファ(Buffer構造体)のメモリ領域の確保 */
	if ((buf = (Buffer *) malloc(sizeof(Buffer))) == NULL) {
	    /* メモリ不足なのでエラーを返す */
	    return NG;
	}

	/* Buffer構造体の初期化 */
	buf->file = NULL;
	buf->pageNum = -1;
	buf->modified = UNMODIFIED;
	memset(buf->page, 0, PAGE_SIZE);
	buf->prev = NULL;
	buf->next = NULL;

	/* ポインタをつないで両方向リストにする */
	if (oldBuf != NULL) {
	    oldBuf->next = buf;
	}
	buf->prev = oldBuf;

	/* リストの一番最初の要素へのポインタを保存 */
	if (buf->prev == NULL) {
	    bufferListHead = buf;
	}

	/* リストの一番最後の要素へのポインタを保存 */
	if (i == NUM_BUFFER - 1) {
	    bufferListTail = buf;
	}

	/* 次のループのために保存 */
	oldBuf = buf;
    }

    return OK;
}

/*
 * finalizeBufferList -- バッファリストの終了処理
 *
 * **注意**
 *	この関数は、ファイルアクセスモジュールの使用後に必ず一度だけ呼び出すこと。
 *	(finalizeFileModule()から呼び出すこと。)
 *
 * 引数:
 *	なし
 *
 * 返り値:
 *	終了処理に成功すればOK、失敗すればNGを返す。
 */
static Result finalizeBufferList()
{
    return OK;
}

/*
 * moveBufferToListHead -- バッファをリストの先頭へ移動
 *
 * 引数:
 *	buf: リストの先頭に移動させるバッファへのポインタ
 *
 * 返り値:
 *	なし
 */
static void moveBufferToListHead(Buffer *buf)
{

    if(bufferListHead == buf){
    //bufが先頭の場合(何もしない)
    }
    else if(bufferListTail == buf){    
    //bufが最後尾の場合
        //bufのprevのnextをnullに
        buf->prev->next = NULL;
        //tailをbufのprevに
        bufferListTail = buf->prev;
        //bufのnextを現在のheadに
        buf->next = bufferListHead;
        //現在のheadのprevをbufに
        bufferListHead->prev = buf;
        //headをbufに
        bufferListHead = buf;
    }
    else{
    //その他場合
        //bufのprevのnextをbufのnextに
        buf->prev->next = buf->next;
        //bufのnextのprevをbufのprevに
        buf->next->prev = buf->prev;
        //bufのnextを現在のheadに
        buf->next = bufferListHead;
        //現在のheadのprevをbufに
        bufferListHead->prev = buf;
        //headをbufに
        bufferListHead = buf;
    }    
    //headのprevとtailのnextをnullに
    bufferListHead->prev = NULL;
    bufferListTail->next = NULL;

}


/*
 * printBufferList -- バッファのリストの内容の出力(テスト用)
 */
void printBufferList()
{
    Buffer *buf;

    printf("Buffer List:");

    /* それぞれのバッファの最初の3バイトだけ出力する */
    for (buf = bufferListHead; buf != NULL; buf = buf->next) {
	if (buf->file == NULL) {
	    printf("(empty) ");
	} else {
	    printf("    %c%c%c ", buf->page[0], buf->page[1], buf->page[2]);
	}
    }

    printf("\n");
}


Result readPage2(File *file, int pageNum, char *page){
    lseek(file->desc, pageNum*PAGE_SIZE, SEEK_SET);
    read(file->desc, page, PAGE_SIZE);

    return OK;
}

Result writePage2(File *file, int pageNum, char *page){
    lseek(file->desc, pageNum*PAGE_SIZE, SEEK_SET);
    write(file->desc, page, PAGE_SIZE);
    return OK;
}


