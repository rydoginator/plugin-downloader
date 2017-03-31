#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "jsmn.h"

#include <3ds.h>


#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// Uncomment to display debug strings
//#define DEBUG


static const char  *g_version = "v1.2";

enum
{
    Latest = 0,
    Beta3 = 1
};

// str_replace(haystack, haystacksize, oldneedle, newneedle) --
//  Search haystack and replace all occurences of oldneedle with newneedle.
//  Resulting haystack contains no more than haystacksize characters (including the '\0').
//  If haystacksize is too small to make the replacements, do not modify haystack at all.
//
// RETURN VALUES
// str_replace() returns haystack on success and NULL on failure. 
// Failure means there was not enough room to replace all occurences of oldneedle.
// Success is returned otherwise, even if no replacement is made.
char *str_replace(char *haystack, size_t haystacksize,
                    const char *oldneedle, const char *newneedle);

// ------------------------------------------------------------------
// Implementation of function
// ------------------------------------------------------------------
#define SUCCESS (char *)haystack
#define FAILURE (void *)NULL

static bool
locate_forward(char **needle_ptr, char *read_ptr, 
        const char *needle, const char *needle_last);
static bool
locate_backward(char **needle_ptr, char *read_ptr, 
        const char *needle, const char *needle_last);

char *str_replace(char *haystack, size_t haystacksize,
                    const char *oldneedle, const char *newneedle)
{   
    size_t oldneedle_len = strlen(oldneedle);
    size_t newneedle_len = strlen(newneedle);
    char *oldneedle_ptr;    // locates occurences of oldneedle
    char *read_ptr;         // where to read in the haystack
    char *write_ptr;        // where to write in the haystack
    const char *oldneedle_last =  // the last character in oldneedle
        oldneedle +             
        oldneedle_len - 1;      

    // Case 0: oldneedle is empty
    if (oldneedle_len == 0)
        return SUCCESS;     // nothing to do; define as success

    // Case 1: newneedle is not longer than oldneedle
    if (newneedle_len <= oldneedle_len) {       
        // Pass 1: Perform copy/replace using read_ptr and write_ptr
        for (oldneedle_ptr = (char *)oldneedle,
            read_ptr = haystack, write_ptr = haystack; 
            *read_ptr != '\0';
            read_ptr++, write_ptr++)
        {
            *write_ptr = *read_ptr;         
            bool found = locate_forward(&oldneedle_ptr, read_ptr,
                        oldneedle, oldneedle_last);
            if (found)  {   
                // then perform update
                write_ptr -= oldneedle_len;
                memcpy(write_ptr+1, newneedle, newneedle_len);
                write_ptr += newneedle_len;
            }               
        } 
        *write_ptr = '\0';
        return SUCCESS;
    }

    // Case 2: newneedle is longer than oldneedle
    else {
        size_t diff_len =       // the amount of extra space needed 
            newneedle_len -     // to replace oldneedle with newneedle
            oldneedle_len;      // in the expanded haystack

        // Pass 1: Perform forward scan, updating write_ptr along the way
        for (oldneedle_ptr = (char *)oldneedle,
            read_ptr = haystack, write_ptr = haystack;
            *read_ptr != '\0';
            read_ptr++, write_ptr++)
        {
            bool found = locate_forward(&oldneedle_ptr, read_ptr, 
                        oldneedle, oldneedle_last);
            if (found) {    
                // then advance write_ptr
                write_ptr += diff_len;
            }
            if (write_ptr >= haystack+haystacksize)
                return FAILURE; // no more room in haystack
        }

        // Pass 2: Walk backwards through haystack, performing copy/replace
        for (oldneedle_ptr = (char *)oldneedle_last;
            write_ptr >= haystack;
            write_ptr--, read_ptr--)
        {
            *write_ptr = *read_ptr;
            bool found = locate_backward(&oldneedle_ptr, read_ptr, 
                        oldneedle, oldneedle_last);
            if (found) {    
                // then perform replacement
                write_ptr -= diff_len;
                memcpy(write_ptr, newneedle, newneedle_len);
            }   
        }
        return SUCCESS;
    }
}

// locate_forward: compare needle_ptr and read_ptr to see if a match occured
// needle_ptr is updated as appropriate for the next call
// return true if match occured, false otherwise
static inline bool 
locate_forward(char **needle_ptr, char *read_ptr,
        const char *needle, const char *needle_last)
{
    if (**needle_ptr == *read_ptr) {
        (*needle_ptr)++;
        if (*needle_ptr > needle_last) {
            *needle_ptr = (char *)needle;
            return true;
        }
    }
    else 
        *needle_ptr = (char *)needle;
    return false;
}

// locate_backward: compare needle_ptr and read_ptr to see if a match occured
// needle_ptr is updated as appropriate for the next call
// return true if match occured, false otherwise
static inline bool
locate_backward(char **needle_ptr, char *read_ptr, 
        const char *needle, const char *needle_last)
{
    if (**needle_ptr == *read_ptr) {
        (*needle_ptr)--;
        if (*needle_ptr < needle) {
            *needle_ptr = (char *)needle_last;
            return true;
        }
    }
    else 
        *needle_ptr = (char *)needle_last;
    return false;
}

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

        // Display download status only if the size is greater than 10kb
        if (size > 10000)
        {
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
    	}
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

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
	if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
			strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
		return 0;
	}
	return -1;
}

char* readFile(char* filename)
{
    FILE* file = fopen(filename,"r");
    if(file == NULL)
    {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long int size = ftell(file);
    rewind(file);

    char* content = calloc(size + 1, 1);

    fread(content,1,size,file);

    return content;
}

static Result startInstall(u32 *handle)
{
    return (AM_StartCiaInstall(MEDIATYPE_SD, handle));
}

static Result cancelInstall(u32 handle)
{
    return (AM_CancelCIAInstall(handle));
}

static Result endInstall(u32 handle)
{
    return (AM_FinishCiaInstall(handle));
}

Result installUpdate(const char *url)
{
    int userChoice = 0;

    u8      *buffer = NULL;
    u32     size = 0;
    u32     res;

    while (userChoice == 0)
    {
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown == KEY_A)
        {
            if (!http_download(url, &buffer, &size))
            {
                res = startInstall(&buffer);
                if (res == 0)
                {
                    printf("Update installed!\n");
                    userChoice = 1;
                    return 0;
                }
                else
                {
                    printf("Unknown error occured\n");
                    userChoice = 1;
                    return -2;
                }
                printf("ok");
            }
            else
            {
                printf("Error downloading updates!\n");
                return -1;
            }

        }
        if (kDown == KEY_B)
        {
            printf("Aborted!\n");
            userChoice = 1;
            return 1;
        }
    }
    return 0;      
}
int     downloadUpdate(void)
{
    char            *json = NULL;
    char            *changeLog;
    static const char  *urlDownload[89];
    u32             size = 0;
    int             i;
    int             r;
    int 			ret;
    jsmn_parser     jParser;
    jsmntok_t       tokens[128];


    if (!http_download("https://api.github.com/repos/RyDog199/plugin-downloader/releases/latest", (u8 *)&json, &size))
    {
        jsmn_init(&jParser);
        r = jsmn_parse(&jParser, json, size, tokens, sizeof(tokens)/sizeof(tokens[0]));
        if (r < 0) 
        {
            printf(ANSI_COLOR_RED "Failed to parse JSON: %d\n" ANSI_COLOR_RESET, r);
            return 1;
        }
        
        if (r < 1 || tokens[0].type != JSMN_OBJECT) 
        {
            printf(ANSI_COLOR_RED "Object expected\n" ANSI_COLOR_RESET);
            return 1;
        }

        /* Loop over all keys of the root object */
        for (i = 1; i < r; i++) 
        {
            if (jsoneq(json, &tokens[i], "tag_name") == 0) 
            {
            	ret = strcmp(json + tokens[i + 1].start, g_version);

                if (ret < 0)
                {
                    printf(ANSI_COLOR_GREEN "New update! Version: %.*s\n" ANSI_COLOR_RESET, tokens[i + 1].end-tokens[i + 1].start,
                        json + tokens[i + 1].start);
                    i++;
                }
            }
            if (jsoneq(json, &tokens[i], "browser_download_url") == 0) 
            {
                strncpy(urlDownload, json + tokens[i + 1].start, 89);
                str_replace(urlDownload, 89, "\"}", "");
                i++;
            }
            if (jsoneq(json, &tokens[i], "body") == 0) 
            {
                if (ret < 0)
                {
                    changeLog = json + tokens[i + 1].start;

                    str_replace(changeLog, tokens[i + 1].end-tokens[i + 1].start, "# ", "");
                    str_replace(changeLog, tokens[i + 1].end-tokens[i + 1].start, "What's New\\r\\n* ", "\n\n* ");
                    str_replace(changeLog, tokens[i + 1].end-tokens[i + 1].start, "\\n", "\n");
                    str_replace(changeLog, tokens[i + 1].end-tokens[i + 1].start, "\\r", "\n");
                    str_replace(changeLog, tokens[i + 1].end-tokens[i + 1].start, ".\"}", "");
                    printf("What's new: %s\n\n", changeLog);
                    printf("Press A to install\n");
                    printf("Press B to abort\n");
                    installUpdate(urlDownload);
                }
            }
        }
    }
    else
    {
        printf("An error occured while checking for an update !\n");
        return (-1);
    }
    return 0;
}

int main()
{
    bool    isRunning = true;

    gfxInitDefault();
    httpcInit(0); // Buffer size when POST/PUT.

    consoleInit(GFX_TOP,NULL);

    printf("--- ACNL Multi NTR Plugin Downloader %s ---\n\n", g_version);
    printf("Press A to download the latest version \n");
    printf("Press B to download 3.0 beta (for people without\nthe Amiibo update) \n");
    printf("Press Y to check for updates.\n");
    printf("Press Start to exit.\n\n");
    //check for an update
    



    gfxFlushBuffers();


    while (isRunning && aptMainLoop())
    {
        gspWaitForVBlank();
        hidScanInput();

        u32 kDown = hidKeysDown();

        if (kDown == KEY_Y)
        {
            printf("Checking for an update...");
            downloadUpdate();
        }

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

