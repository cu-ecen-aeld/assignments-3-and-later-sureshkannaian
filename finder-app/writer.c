#include <stdio.h>
#include <syslog.h>
#include <string.h>
int main(int argc, char* argv[])
{
    openlog(NULL, 0, LOG_USER);

    if(argc < 3)
    {
        syslog(LOG_ERR, "Invalid Number of arguments: %d", argc);    
        return 1;
    }
    const char* path = argv[1];
    const char* str  = argv[2];
    int bytes = strlen(str);
    syslog(LOG_DEBUG, "Writing %s to %s", str, path);

    FILE* dstFile = fopen(path, "wb");
    if(dstFile == NULL)
    {
        syslog(LOG_ERR, "Unable to open file %s", path);    
        return 1;
    }

    if(fwrite(str, 1, bytes, dstFile) != bytes)
    {
        syslog(LOG_ERR, "Unable to write with bytes %d", bytes);    
        return 1;
    }
    fclose(dstFile);

    closelog();
    return 0;
}