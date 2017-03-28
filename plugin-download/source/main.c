#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <3ds.h>

// Uncomment to display debug strings
//#define DEBUG

enum
{
    Latest = 0,
    Beta3 = 1
};

Result http_download(const char *url, u8 **output, u32 *outSize)
{
    Result ret=0;
    httpcContext context;
    char *newurl=NULL;
    u32 statuscode=0;
    u32 contentsize=0, readsize=0, size=0;
    u8 *buf, *lastbuf;

    do {
        ret = httpcOpenContext(&context, HTTPC_METHOD_GET, url, 1);
        #ifdef DEBUG
            printf("return from httpcOpenContext: %"PRId32"\n",ret);
        #endif

        // This disables SSL cert verification, so https:// will be usable
        ret = httpcSetSSLOpt(&context, SSLCOPT_DisableVerify);
        #ifdef DEBUG
            printf("return from httpcSetSSLOpt: %"PRId32"\n",ret);
        #endif

        // Set a User-Agent header so websites can identify your application
        ret = httpcAddRequestHeaderField(&context, "User-Agent", "httpc-example/1.0.0");
        #ifdef DEBUG
            printf("return from httpcAddRequestHeaderField: %"PRId32"\n",ret);
        #endif

        // Tell the server we can support Keep-Alive connections.
        // This will delay connection teardown momentarily (typically 5s)
        // in case there is another request made to the same server.
        ret = httpcAddRequestHeaderField(&context, "Connection", "Keep-Alive");
        #ifdef DEBUG
            printf("return from httpcAddRequestHeaderField: %"PRId32"\n",ret);
        #endif

        ret = httpcBeginRequest(&context);
        if(ret!=0){
            httpcCloseContext(&context);
            if(newurl!=NULL) free(newurl);
            return ret;
        }

        ret = httpcGetResponseStatusCode(&context, &statuscode);
        if(ret!=0){
            httpcCloseContext(&context);
            if(newurl!=NULL) free(newurl);
            return ret;
        }

        if ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308)) {
            if(newurl==NULL) newurl = malloc(0x1000); // One 4K page for new URL
            if (newurl==NULL){
                httpcCloseContext(&context);
                return -1;
            }
            ret = httpcGetResponseHeader(&context, "Location", newurl, 0x1000);
            url = newurl; // Change pointer to the url that we just learned
            #ifdef DEBUG
                printf("redirecting to url: %s\n",url);
            #endif
            httpcCloseContext(&context); // Close this context before we try the next
        }
    } while ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308));

    if(statuscode!=200){
        #ifdef DEBUG
            printf("URL returned status: %"PRId32"\n", statuscode);
        #endif
        httpcCloseContext(&context);
        if(newurl!=NULL) free(newurl);
        return -2;
    }

    // This relies on an optional Content-Length header and may be 0
    ret = httpcGetDownloadSizeState(&context, NULL, &contentsize);
    if(ret!=0){
        httpcCloseContext(&context);
        if(newurl!=NULL) free(newurl);
        return ret;
    }
    #ifdef DEBUG
        printf("reported size: %"PRId32"\n",contentsize);
    #endif

    // Start with a single page buffer
    buf = (u8*)malloc(0x1000);
    if(buf==NULL){
        httpcCloseContext(&context);
        if(newurl!=NULL) free(newurl);
        return -1;
    }

    do {
        // This download loop resizes the buffer as data is read.
        ret = httpcDownloadData(&context, buf+size, 0x1000, &readsize);
        size += readsize; 
        if (ret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING){
            lastbuf = buf; // Save the old pointer, in case realloc() fails.
            buf = realloc(buf, size + 0x1000);
            if(buf==NULL){ 
                httpcCloseContext(&context);
                free(lastbuf);
                if(newurl!=NULL) free(newurl);
                return -1;
            }
        }

        // Display download status
        printf("\33[2K\rDownloading:   [");

        float   progress = (float)(size) / (float)(contentsize);
        int     barWidth = 25;
        int     pos = barWidth * progress;

        for (int i = 0; i < barWidth; ++i) 
        {
            if (i < pos) printf("=");
            else if (i == pos) printf(">");
            else printf(" ");
        }
        printf("] %d%%", (int)(progress * 100.0f));
        // Flush and swap framebuffers
        gfxFlushBuffers();
        gfxSwapBuffers();

    } while (ret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING);

    printf("\n");

    if(ret!=0)
    {
        httpcCloseContext(&context);
        if(newurl!=NULL) free(newurl);
        free(buf);
        return -1;
    }

    // Resize the buffer back down to our actual final size
    lastbuf = buf;
    buf = realloc(buf, size);
    if(buf==NULL){ // realloc() failed.
        httpcCloseContext(&context);
        free(lastbuf);
        if(newurl!=NULL) free(newurl);
        return -1;
    }

    #ifdef DEBUG
        printf("downloaded size: %"PRId32"\n",size);
    #endif

    *output = buf;
    *outSize = size;
    return 0;
}

int    CreateFiles(void *buffer, u32 size)
{
    struct stat st = {0};

    FILE *usa;
    FILE *eur;
    FILE *jap;


    if (stat("sdmc:/plugin/0004000000086200", &st) == -1) 
    {
        mkdir("sdmc:/plugin/0004000000086200", 0700);
    }
    if (stat("sdmc:/plugin/0004000000086300", &st) == -1) 
    {
        mkdir("sdmc:/plugin/0004000000086300", 0700);
    }
    if (stat("sdmc:/plugin/0004000000086400", &st) == -1) 
    {
        mkdir("sdmc:/plugin/0004000000086400", 0700);
    }

    // Delete any existing plugins in the USA, EUR or JAP directory
    remove("sdmc:/plugin/0004000000086300/ACNL_Multi.plg");
    remove("sdmc:/plugin/0004000000086200/ACNL_Multi.plg");
    remove("sdmc:/plugin/0004000000086400/ACNL_Multi.plg");
    remove("sdmc:/plugin/0004000000086300/ACNL_Multi_USA.plg");
    remove("sdmc:/plugin/0004000000086200/ACNL_Multi_JAP.plg");
    remove("sdmc:/plugin/0004000000086400/ACNL_Multi_EUR.plg");

    if (!buffer)
        return (-1);

    usa = fopen("sdmc:/plugin/0004000000086300/ACNL_MULTI.plg", "w+");
    fwrite(buffer, 1, size, usa);
    fclose(usa);
    eur = fopen("sdmc:/plugin/0004000000086400/ACNL_MULTI.plg", "w+");
    fwrite(buffer, 1, size, eur);
    fclose(eur);
    jap = fopen("sdmc:/plugin/0004000000086200/ACNL_MULTI.plg", "w+");
    fwrite(buffer, 1, size, jap);
    fclose(jap);

    // Free buffer
    free(buffer);

    return (0);
}

int    DownloadPlugin(int version)
{
    static const  char *urls[2] = 
    {
        "https://github.com/RyDog199/ACNL-NTR-Cheats/blob/master/ACNL_MULTI.plg?raw=true",
        "https://github.com/RyDog199/ACNL-NTR-Cheats/releases/download/v3.0B1/ACNL_MULTI.plg"
    };
    static const  char *downloadVersion[2] = 
    {
        "Updating plugin to the last version...\n\n",
        "Downloading 3.0 Beta...\n\n"
    };

    u8      *buffer = NULL;
    u32     size = 0;

    printf(downloadVersion[version]);

    if (!http_download(urls[version], &buffer, &size))
    {
        if (!CreateFiles(buffer, size))
        {
            printf("Plugin has been downloaded.\n\n");
            return (0);
        }
        else
        {
            printf("An error occurred while creating the files !\n");
        }
    }
    else
    {
        printf("Download failed !\n");
    }
    return (-1);    
}

int main()
{
    bool    isRunning = true;

    gfxInitDefault();
    httpcInit(0); // Buffer size when POST/PUT.

    consoleInit(GFX_TOP,NULL);

    printf("--- ACNL Multi NTR Plugin Downloader V1.1 ---\n\n");
    printf("Press A to download the latest version \n");
    printf("Press B to download 3.0 beta (for people without\nthe Amiibo update) \n");
    printf("Press Start to exit.\n\n");


    gfxFlushBuffers();


    while (isRunning && aptMainLoop())
    {
        gspWaitForVBlank();
        hidScanInput();

        u32 kDown = hidKeysDown();

        if (kDown == KEY_A)
        {
            if (!DownloadPlugin(Latest))
            {
                printf("Returning to homemenu...\n");
                isRunning = false;
            }
        }
        if (kDown == KEY_B)
        {
            if (!DownloadPlugin(Beta3))
            {
                printf("Returning to homemenu...\n");
                isRunning = false;
            }
        }
        if (kDown & KEY_START)
            break; // break in order to return to hbmenu

        // Flush and swap framebuffers
        gfxFlushBuffers();
        gfxSwapBuffers();
    }

    if (!isRunning)
        svcSleepThread(2000000000);
    // Exit services
    httpcExit();
    gfxExit();
    return 0;
}

