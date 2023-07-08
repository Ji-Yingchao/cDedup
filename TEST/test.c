#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>


int main()
{
    int fd = open("./tw_txt", O_RDWR, 0777);
    printf("%d\n", fd);
    lseek(fd,2,SEEK_SET);
    __uint8_t a=10;
    read(fd,&a,1);
    printf("%d\n",a);
    lseek(fd,2,SEEK_SET);
    char s[6] = {255,0,255,0,0};
    write(fd, s, 1);
    close(fd);


    return 0;
}