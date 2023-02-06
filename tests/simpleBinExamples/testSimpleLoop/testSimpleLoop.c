#include <stdio.h>

#define N 10000

int main(int a)
{
    int c=0;
    for (int b=0; b<N; b++)
    {
        //printf("a+b=%d\n", a+b);
        c=c+a;
    }

    printf("Exit\n");
    return c;
}

