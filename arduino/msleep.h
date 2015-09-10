/*
Copied from http://stackoverflow.com/questions/1157209/is-there-an-alternative-sleep-function-in-c-to-milliseconds
Utility to enable sleep for miliseconds
*/


 
struct tm* current_time() {
    // FILE* fp = fopen("Time.txt","w+");
    // struct timeval usec_time;
    time_t now = time(0);
    // gettimeofday(&usec_time,NULL);
    // format time
    return localtime(&now);
    // printf("%d:%d:%d\n",current->tm_hour, current->tm_min, current->tm_sec);
    // fprintf(fp,"%d:%d:%d\n",current->tm_hour, current->tm_min, current->tm_sec);
}

void sleep_ms(int milliseconds) // cross-platform sleep function
{
// #ifdef WIN32
//     Sleep(milliseconds);
// #elif _POSIX_C_SOURCE >= 199309L
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = milliseconds * 1000000;
    nanosleep(&ts, NULL);
// #else
//     usleep(milliseconds * 1000);
// #endif
}