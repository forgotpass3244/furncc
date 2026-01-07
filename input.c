
void putdigit(int digit)
{
    putchar(digit + '0');
}

int main()
{
    int i = 0;

    while (++i < 10)
    {
        putdigit(i);
        puts(": Hello, World!\n");
    }

    return 67;
}
