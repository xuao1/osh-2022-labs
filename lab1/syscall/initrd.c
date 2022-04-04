#include <stdio.h>

int main() {
    printf("Hello, Linux!\n");
	char buf1[20],buf2[20];	
    long flag1=syscall(548,buf1,20);
    printf("the result of the first test is %ld\n",flag1);
    if(flag1==0) printf("%s\n",buf1);
    long flag2=syscall(548,buf2,1);
    printf("the result of the second test is %ld\n",flag2);
    if(flag2==0) printf("%s\n",buf2);
    while (1) {}
}

