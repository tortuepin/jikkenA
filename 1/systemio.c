#include<stdio.h>
#include<unistd.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<fcntl.h>
#include<errno.h>
#define FILE_NAME "testfile"


void closefile(int hundle);

int main(void){
	int fd = 0;	
	char *buf = "X";
	char buf2[16];
	char *test = "test";	
	int i=0;	
	int checkbite = 0;
	struct stat bufstat;	

	//ファイルが存在していたら削除
	if(access(FILE_NAME , F_OK) == 0){
		if(unlink(FILE_NAME) < 0){
			printf("%s not deleted\n" ,FILE_NAME);
		}else{
			printf("%s deleted\n",FILE_NAME);
		}
	}
	//ファイルを作る
	fd = creat(FILE_NAME, (S_IREAD|S_IWRITE));
	if(fd < 0){
		printf("cannnot create %s\n", FILE_NAME);
		return -1;
	}
	printf("file created\n");

	//Xを書き込む
	for(i = 0; i < 256; i++){
		checkbite = write(fd, buf, 1);
		if(checkbite < 0){
			printf("cannot write\n");		
			closefile(fd);
			return -1;
		}
	}
	printf("kakikometaX\n");
	
	//ぽいんた移動する
	checkbite = lseek(fd,156,0);
	if(checkbite < 0){
		printf("cannnot seek\n");
		closefile(fd);
		return -1;
	}
	printf("I'm at %d\n", checkbite);
	
	//testって書き込む
	checkbite = write(fd, test, 4);
	if(checkbite < 0){
		printf("cannot write\n");
		closefile(fd);
		return -1;
	}
	printf("kakikometatest\n");


	closefile(fd);
	open(FILE_NAME, O_RDWR);
	//ポインタ移動
	checkbite = lseek(fd, 152, 0);
	if(checkbite < 0){
		printf("cannot seek\n");
		closefile(fd);
		return -1;
	}
	printf("I'm at %d\n", checkbite);


	//読み込み
	checkbite = read(fd, buf2, 10);
	if(checkbite == -1){
		printf("cannot read\n%d %d\n", errno, strerror(errno));
		closefile(fd);
		return -1;
	}
	buf2[checkbite] = '\0';
	printf("%s\n", buf2);

	
	//情報を読み込む 
	checkbite = stat(FILE_NAME, &bufstat);
	if(checkbite == -1){
		printf("じょうほうよみこみしっぱい");
	}else{
		printf("ファイルサイズ: %ld\n", (long)bufstat.st_size);	
	}
	//ファイルをクローズ
	closefile(fd);
	return 0;
}


void closefile(int hundle){
	
	if(close(hundle)==0){
		printf("file closed\n");
	}else{
		printf("cannot close file\n");
	}
}
