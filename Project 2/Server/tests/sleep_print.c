#include <unistd.h>
#include <stdio.h>
int main()
{
    for(int i = 0; i <= 50; i++)
    {
        printf("%d\n",i);
        sleep(1);
    }
}
