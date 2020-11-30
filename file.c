#include <stdio.h>

int main(int argc, char* argv[])
{
    FILE* file;
    char buf[1024] = {0, };
    
    //打开或创建文件
    file = fopen("1.txt", "a+");
    //写文件
    fwrite("hello, world!", 1, 13, file);
    //将游标放在文件的开头
    rewind(file);
    //读文件
    fread(buf, 1, 26, file);
    //关闭文件
    fclose(file);
    printf("buf:%s\n", buf);
    return 0;
}



