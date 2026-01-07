

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

char *itos(uint64_t num)
{
    char buf[21];
    char *p = buf + (sizeof(buf) - 1);

    // set null terminator
    *(p--) = '\0';

    if (num <= 0)
    {
        *(p--) = '0';
    }

    while (num > 0)
    {
        uint64_t digit = num % 10;
        *(p--) = digit + '0';
        num /= 10;
    }

    size_t len = (buf + sizeof(buf)) - (p + 1);
    char *s = malloc(len * sizeof(char));
    memcpy(s, p + 1, len);

    return s;
}


int main()
{
    char *s = itos(123);
    printf("string: %s\n", s);

    free(s);
    return 0;
}
