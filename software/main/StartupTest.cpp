extern "C"
{
    #include "spiffs.h"
    #include <dirent.h>
    #include <sys/stat.h>
}

#include "FilesystemHandler.h"

bool do_startup_test()
{
    FilesystemHandler *fs_handler = FilesystemHandler::get_instance(0x210000 /*Start address on flash*/,
                                 0x100000  /*Size*/,
                                 (char *)"/spiffs"      /*Mount point */);
    #define CHUNK 150
    char buf[CHUNK];
    int nread;

    //Read using spiffs api
    printf("Trying to read using SPIFFS API!\n");
    spiffs_file f = SPIFFS_open(&fs_handler->fs, "/html/README", SPIFFS_O_RDONLY, 0);

    printf("%d\n", (size_t)f);
    if (f) {
        while ((nread = SPIFFS_read(&fs_handler->fs, f, buf, sizeof buf)) > 0)
        {
            printf("%.*s", nread, buf);
        }
        printf("%d\n", nread);
        SPIFFS_close(&fs_handler->fs, f);
        printf("\n");
    }
    printf("Now trying to read using VFS API!\n");

    FILE *file;
    file = fopen("/spiffs/html/README", "rw");
    printf("%d\n", (size_t)f);
    if (file) {
        while (fgets(buf, 50, file) != nullptr)
        {
            printf("%s", buf);
        }
        fclose(file);
        printf("\n");
    }
    else return false;
    printf("Now trying to list the files in the spiffs partition!\n");
    printf("We first try using SPIFFS calls directly.\n");

    spiffs_DIR dir;
	struct spiffs_dirent dirEnt;
	const char rootPath[] = "/";


	if (SPIFFS_opendir(&fs_handler->fs, rootPath, &dir) == NULL) {
		printf("Unable to open %s dir", rootPath);
		return false;
	}
	while(SPIFFS_readdir(&dir, &dirEnt) != NULL) {
		int len = strlen((char *)dirEnt.name);
		// Skip files that end with "/."
		if (len>=2 && strcmp((char *)(dirEnt.name + len -2), "/.") == 0) {
			continue;
		}
        printf("%s\n", dirEnt.name);
    }
    SPIFFS_closedir(&dir);

    printf("We now try through the filesystem.\n");
    DIR *midir;
    struct dirent* info_archivo;
    if ((midir = opendir("/spiffs/")) == NULL)
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
    return true;
}
