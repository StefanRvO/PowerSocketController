extern "C"
{
    #include <dirent.h>
    #include <sys/stat.h>
}

#include "FilesystemHandler.h"

bool do_startup_test()
{
    #define CHUNK 150
    char buf[CHUNK];

    printf("Now trying to read using VFS API!\n");

    FILE *file;
    file = fopen("/spiffs/html/README.md", "rw");
    if (file) {
        while (fgets(buf, 50, file) != nullptr)
        {
            printf("%s", buf);
        }
        fclose(file);
        printf("\n");
    }
    else return false;
    printf("We now try through the filesystem.\n");
    DIR *midir;
    struct dirent* info_archivo;
    if ((midir = opendir("/spiffs/html/")) == NULL)
    {
        perror("Error in opendir\n");
    }
    else
    {
        while ((info_archivo = readdir(midir)) != 0)
            printf ("%s \n", info_archivo->d_name);
    }
    closedir(midir);

    printf("We see if stat is working by accessing a file which we know is there.\n");
    struct stat stat_buf;
    printf("Stat result:%d\n",stat("/spiffs/html/README.md", &stat_buf));

    printf("Stat result:%d\n",stat("/spiffs/html/.", &stat_buf));
    printf("Stat result:%d\n",stat("/spiffs/ht/.", &stat_buf));
    printf("Stat result:%d\n",stat("/spiffs/html", &stat_buf));
    printf("Stat result:%d\n",stat("/spiffs/html/", &stat_buf));

    return true;
}
