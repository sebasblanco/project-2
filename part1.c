#include <unistd.h>

int main()
{
    fork();
    sleep(3);
    getpid();
    getgid();
    sethostid(1001);

    return 0;
}