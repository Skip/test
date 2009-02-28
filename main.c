#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define TOTAL_COMMANDS  2
#define SERVER_ADDRESS  "10.10.10.10"
#define SERVER_PORT     "80"
#define LOG_FILE        "/tmp/main.log"

const char *start_command[TOTAL_COMMANDS] = { 
    "GET /announce.php?uk=1234567890&&peer_id=-KT2280-1eMLTnp9UtQR&port=6881&uploaded=0&downloaded=0&left=0&compact=1&numwant=100&key=1237450489&event=started&info_hash=%8dP%dbc*%1aN%12%ac%fe*c%04%eb%94%60%8e%caa%ce HTTP/1.1\n" \
    "User-Agent: git/1.2.1\n" \
    "Accept: text/html, image/gif, image/jpeg, *; q=.2, */*; q=.2\n" \
    "Accept-Encoding: x-gzip, x-deflate, gzip, deflate\n" \
    "Host: gt.git.org\n" \
    "Connection: Keep-Alive\n\n", 

    "GET /announce.php?uk=0987654321&&peer_id=-KT2280-6RvFOwrayU3V&port=6881&uploaded=0&downloaded=0&left=0&compact=1&numwant=100&key=454186080&event=started&info_hash=%0aQ0'%7f%06j%91%3b-%8e%b9%ab~%ca%e2%96ej%91 HTTP/1.1\n" \
    "User-Agent: git/1.2.1\n" \
    "Accept: text/html, image/gif, image/jpeg, *; q=.2, */*; q=.2\n" \
    "Accept-Encoding: x-gzip, x-deflate, gzip, deflate\n" \
    "Host: gt.git.org\n" \
    "Connection: Keep-Alive\n\n"
    };

const char *stop_command1[TOTAL_COMMANDS] = {
    "GET /announce.php?uk=1234567890&&peer_id=-KT2280-uYQgktmnGcPx&port=6881&uploaded=",
    "GET /announce.php?uk=0987654321&&peer_id=-KT2280-6RvFOwrayU3V&port=6881&uploaded="
};

const char *stop_command2[TOTAL_COMMANDS] = {
    "&downloaded=0&left=0&compact=1&numwant=0&key=624878738&event=stopped&info_hash=%8dP%dbc*%1aN%12%ac%fe*c%04%eb%94%60%8e%caa%ce HTTP/1.1\n" \
    "User-Agent: vuze/1.2.1\n" \
    "Accept: text/html, image/gif, image/jpeg, *; q=.2, */*; q=.2\n" \
    "Accept-Encoding: x-gzip, x-deflate, gzip, deflate\n" \
    "Host: gt.git.org\n" \
    "Connection: Keep-Alive\n\n",
    
    "&downloaded=0&left=0&compact=1&numwant=0&key=454186080&event=stopped&info_hash=%0aQ0'%7f%06j%91%3b-%8e%b9%ab~%ca%e2%96ej%91 HTTP/1.1\n" \
    "User-Agent: vuze/1.2.1\n" \
    "Accept: text/html, image/gif, image/jpeg, *; q=.2, */*; q=.2\n" \
    "Accept-Encoding: x-gzip, x-deflate, gzip, deflate\n" \
    "Host: gt.git.org\n" \
    "Connection: Keep-Alive\n\n"
};

static int program_exit = 0;

static uint8_t get_random_byte(void)
{
    int tmp, file_id;
    uint8_t ret = 0x05; /* default return value */

    file_id = open("/dev/random", O_RDONLY);
    if (file_id == -1)
        printf("open() failed: %s\n", strerror(errno));
    else {
        tmp = read(file_id, &ret, sizeof(ret));
        if (tmp == -1) {
            printf("read() failed: %s\n", strerror(errno));
            ret = 0x05; /* may be corrupted, restore */
        }

        close(file_id);
    }

    return ret;
}

static int make_tmp_file_name(char *tmp_file_name)
{
    int file_id = -1;

    file_id = mkstemp(tmp_file_name);
    if (file_id == -1)
        printf("mkstemp() failed: %s\n", strerror(errno));
    else
        close(file_id);

    return file_id;
}

static int send_command_to_server(const char *tmp_file_name)
{
    char shell_command[250];

    memset(shell_command, 0, 250);
    snprintf(shell_command, 250, "cat %s | telnet %s %s", 
            tmp_file_name, SERVER_ADDRESS, SERVER_PORT);
    if (system(shell_command) == -1) {
	    printf("%s error\n", __FUNCTION__);
        unlink(tmp_file_name);
        return -1;
    }
    unlink(tmp_file_name);

    return 0;
}

static void log_data(int uploaded, int delay, int record)
{
    time_t systime;
    struct tm *_time = NULL;
    FILE *log_file = NULL;

    log_file = fopen(LOG_FILE, "a");
    if (log_file == NULL) {
        printf("fopen() failed: %s\n", strerror(errno));
        return;
    }
    time(&systime);
    _time = localtime(&systime);
    fprintf(log_file, "%02d:%02d:%02d  %02d:%02d:%02d  -  uploaded %2d Mb in %2d mins for %2d record\n", 
            _time->tm_hour, _time->tm_min, _time->tm_sec, 
            _time->tm_mday, _time->tm_mon + 1, _time->tm_year - 100, 
            uploaded, delay, record);
    fclose(log_file);
}

static void log_message(const char *msg)
{
    time_t systime;
    struct tm *_time = NULL;
    FILE *log_file = NULL;

    log_file = fopen(LOG_FILE, "a");
    if (log_file == NULL) {
        printf("fopen() failed: %s\n", strerror(errno));
        return;
    }
    time(&systime);
    _time = localtime(&systime);
    fprintf(log_file, "%02d:%02d:%02d  %02d:%02d:%02d  -  %s\n", 
            _time->tm_hour, _time->tm_min, _time->tm_sec, 
            _time->tm_mday, _time->tm_mon + 1, _time->tm_year - 100, msg);
    fclose(log_file);
}

static void increment_upload(void)
{
    char tmp_file_name[19] = "/tmp/testXXXXXX";
    FILE *command_file = NULL;
    int file_id, uploaded = 5, delay, record, remainder;
    

    record = get_random_byte() % TOTAL_COMMANDS; /* choose record */

    file_id = make_tmp_file_name(tmp_file_name);
    if (file_id == -1) goto increment_upload_quit;

    command_file = fopen(tmp_file_name, "w");
    if (command_file == NULL) {
        printf("fopen() failed: %s\n", strerror(errno));
        unlink(tmp_file_name);
        goto increment_upload_quit;
    }
    fprintf(command_file, "%s", start_command[record]);
    fclose(command_file);

    if (send_command_to_server(tmp_file_name) == -1) goto increment_upload_quit;

    /* make random delay: 40 min <= delay <= 55 mins */
    delay = (get_random_byte() & 0x0f) + 40;
    printf("delay between start and stop packets %d mins\n", delay);
    delay *= 60; /* to seconds */
    remainder = delay;
    do {
        remainder = sleep(remainder);
        if (remainder) printf("program exit in %d mins\n", remainder / 60);
    } while (remainder);

    /* make random upload size: 5 Mb <= size <= 15 Mb */
    uploaded = get_random_byte() & 0x0f;
    if (uploaded < 5) uploaded += 5;
    printf("uploaded %d Mb\n", uploaded);
    uploaded *= (1024 * 1024); /* to bytes */

    memset(&tmp_file_name[12], 'X', sizeof(char) * 6);
    file_id = make_tmp_file_name(tmp_file_name);
    if (file_id == -1) goto increment_upload_quit;

    command_file = fopen(tmp_file_name, "w");
    if (command_file == NULL) {
        printf("fopen() failed: %s\n", strerror(errno));
        unlink(tmp_file_name);
        goto increment_upload_quit;
    }
    fprintf(command_file, "%s%d%s", stop_command1[record], 
            uploaded, stop_command2[record]);
    fclose(command_file);

    if (send_command_to_server(tmp_file_name) == -1) goto increment_upload_quit;

    log_data(uploaded / (1024 * 1024), delay / 60, record);

    return;

increment_upload_quit:

    log_message("error, stage not completed");
}

void sigint_handler(void)
{
    printf("caught SIGINT\n");
    program_exit = 1;
}

int main(void)
{
    struct sigaction sact;
    int delay;

    memset(&sact, 0, sizeof(sact));
    sact.sa_handler = sigint_handler;
    sigaction(SIGINT, &sact, NULL);

    do {
        increment_upload();
        
        if (!program_exit) {
            /* make random delay: 5 min <= delay <= 15 mins */
            delay = get_random_byte();
            delay = (delay & 0x0f);
            if (delay < 5) delay += 5;
            printf("delay between before start %d mins\n", delay);
            delay *= 60; /* to seconds */
            sleep(delay);
        }

    } while (!program_exit);

    printf("exiting...\n");

    return 0;
}
