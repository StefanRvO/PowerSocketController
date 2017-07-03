extern "C"
{
    #include <dirent.h>
    #include <sys/stat.h>
    #include <cstring>
}

#include "FilesystemHandler.h"

void printfile(const char *name)
{
    printf("Printing context of:%s\n", name);
    char buf[100];
    FILE *file;
    file = fopen(name, "rw");
    if (file) {
        while (fgets(buf, 50, file) != nullptr)
        {
            printf("%s", buf);
        }
        fclose(file);
        printf("\n");
    }

}



void printdir(const char *name, int level)
{
    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(name)))
        return;
    if (!(entry = readdir(dir)))
        return;

    do {
        if (entry->d_type == DT_DIR) {
            char path[1024];
            int len = snprintf(path, sizeof(path)-1, "%s/%s", name, entry->d_name);
            path[len] = 0;
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            printf("%*s[%s]\n", level*2, "", entry->d_name);
            printdir(path, level + 1);
        }
        else
            printf("%*s- %s\n", level*2, "", entry->d_name);
            //printfile(entry->d_name);
    } while ( (entry = readdir(dir)) );
    closedir(dir);
}

bool do_startup_test()
{
    #define CHUNK 150
    char buf[CHUNK];

    printf("Now trying to read using VFS API!\n");

    FILE *file;
    file = fopen("/data/html/index.html", "rw");
    if (file) {
        while (fgets(buf, 50, file) != nullptr)
        {
            printf("%s", buf);
        }
        fclose(file);
        printf("\n");
    }
    printf("We now try through the filesystem.\n");
    DIR *midir;
    struct dirent* info_archivo;
    if ((midir = opendir("/data/html/")) == NULL)
    {
        perror("Error in opendir\n");
    }
    else
    {
        while ((info_archivo = readdir(midir)) != 0)
            printf ("%s \n", info_archivo->d_name);
        closedir(midir);

    }

    printf("We see if stat is working by accessing a file which we know is there.\n");
    struct stat stat_buf;
    printf("Stat result:%d\n",stat("/data/html/README.md", &stat_buf));

    printf("Stat result:%d\n",stat("/data/html/index.html.", &stat_buf));
    printf("Stat result:%d\n",stat("/data/ht/.", &stat_buf));
    printf("Stat result:%d\n",stat("/data/html", &stat_buf));
    printf("Stat result:%d\n",stat("/data/html/", &stat_buf));
    //printdir("/data", 10);
    return true;
}
