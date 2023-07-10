#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>


int main()
{
    int fd = open("./tw_txt", O_RDWR | O_CREAT, 0777);
    char s[6] = {255,0,255,0,0};
    write(fd, s, 0);
    close(fd);


    return 0;
}