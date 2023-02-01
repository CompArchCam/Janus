#include <stdio.h>

int main()
{
    int a=0;
    if (a==0)
        printf("a=0\n");
    else
        a=15;
    a=a*5+1;
    if (a>0)
        printf("a=%d\n", a);
    else
        a=15;

    printf("Exit\n");
    return a;
}

