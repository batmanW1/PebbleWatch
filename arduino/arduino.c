/*Authors: Soumya Saxena,Dichen Li, Jingcheng Cao, Alan Tang*/

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h> // for usleep
#include "write_log.h"
#include "msleep.h"

char* create_log_reader_thread();

#define BUFFER 1000
#define HOUR 3600 //a hour has 3600 seconds
#define THRESHOLD 60 //temperature threshold, higher than that, the reading is probably wrong
#define INVALID 99999 //write in invalid record to buffer. Must be larger than THRESHOLD
#define LOW_INVALID -300 //some impossible temperature
#define LOW_THRESHOLD 10 //bad value threshold

char watch_msg[BUFFER]; //message to be sent to watch

struct circ_buffer { //to record one hour of temperature
  double buffer[HOUR];  //temperature buffer
  struct tm time[HOUR];
  unsigned int ptr; //points to buffer front, which is the value to be modified
  unsigned int log_ptr; //points to the front of log file writting
} record;
pthread_mutex_t lock_record;

int arduino;
pthread_mutex_t lock_arduino;

pthread_t read_arduino_t; //the thread to read temperature from arduino
// void* r; //pointer to void* read_arduino()

pthread_t log_writer_t;

char buf[BUFFER]; //buffer to read from serial port

char message[BUFFER]; //string to record message coming from serial port, only one line



int running; //if the program is running
pthread_mutex_t lock_running;

int celcius = 1; //1: temperature is celcius. 0: temperature is fahrenheit
// pthread_mutex_t lock_celcius;

int standby = 0; //standby mode: false by default.

char* get_watch_msg() {
  return watch_msg;
}

double c_to_f(double celcius) {
  return celcius * 9 / 5 + 32;
}

double f_to_c(double fahrenheit) {
  return (fahrenheit - 32) * 5 / 9;
}

//if the temperature is a normal record
int normal_temperature(double t) { //t: temperature
  return (t < THRESHOLD && t > LOW_THRESHOLD);
}

/*read the most recent valid temperature record, ignore all 
invalid ones. it's INVALID if no record exists.
*/
double read_current() {
  pthread_mutex_lock(&lock_record);
  int i = record.ptr - 1;
  double current = INVALID;
  while(!normal_temperature(current) && i >= 0) {
    current = record.buffer[i % HOUR];
    i--;
  }
  pthread_mutex_unlock(&lock_record);
  return current;
}

/*write a temperature to record
*/
void write_new(double temp) {
  pthread_mutex_lock(&lock_record);
  record.buffer[record.ptr % HOUR] = temp;
  record.time[record.ptr % HOUR] = *current_time();
  record.ptr++;
  if(record.ptr >= HOUR * 2) {
    record.ptr -= HOUR; //prevent bit overflow
  }
  pthread_mutex_unlock(&lock_record);
}

/*add a new temperature record to record, mutex lock the record
The idea is, we record temperature whether or not it is valid.
If temp is invalid, we record INVALID, otherwise we record the real
temperature. This is because we constantly want to record the past 1 hour
of record, despite possible bad values in that hours. The record is 
always in celcius. 
Reasons for invalid read: abrupt change, temperature too high or too low,
no reading. 
*/
void add_new(double temp) {

  if(!celcius) { //no need to use mutex lock when reading celcius. If reading value is wrong due to concurrent RW, there will be abrupt temperature change. Just dump the readed temperature
    temp = f_to_c(temp);
    // printf("!celcius temp=%f\n", temp);
  }

  //if anything abnormal happens below, set temp to INVALID so that the record will
  //recognize it as trash record
  double last = read_current(); 
  if(temp > THRESHOLD) {//outlier, due to serial port buffer error
    temp = INVALID; //record temperature anyway, but record the trash
  } else if(standby) {
    temp = INVALID;
  } else if(last < THRESHOLD //valid last record
    && (last - temp > 10 || last - temp < - 10)) {//abrupt change, error
    // printf("last: %f new temp: %f Abrupt change\n", last, temp);
    temp = INVALID; // don't add record
  }

  //start to write to record
  write_new(temp);
}


/*create a msg to show temperature to pebble watch. 
The message is stored in watch_msg[] global variable, and will
overwrite the previous report when this method is called
*/
void create_temp_report(struct circ_buffer* record) {
  if(standby) {
    sprintf(watch_msg, "{\n\"name\": \"In standby mode\"\n}\n");
    printf("The message to watch was:\n%s", watch_msg);
    return;
  }
  double sum = 0;
  double avg = 0;
  double min = INVALID + 300; //not possible from reading
  double max = -300; //not possible
  int high_idx = HOUR; //highest record index, also it's the number of records available
  int i;
  int invalid_count = 0; //count of invalid record
  double current = read_current();
  

  //iterate the record, calculate min, max, avg
  pthread_mutex_lock(&lock_record);
  if(record->ptr < HOUR) { //ptr too small, the record hasn't been updated to fill one hour
    high_idx = record->ptr;
  }
  for(i = 0; i < high_idx; i++) {
    double read = record->buffer[i];
    if(read > THRESHOLD) { //bad record
      invalid_count++;
      continue;
    }
    sum += read;
    min = (min < read ? min : read);
    max = (max > read ? max : read);
  }
  pthread_mutex_unlock(&lock_record);

  if(high_idx - invalid_count <= 0) {
    sprintf(watch_msg, "{\n\"name\": \"No data\"\n}\n");
    printf("The message to watch was:\n%s", watch_msg);
    return;
  }

  //process raw data
  avg = sum / (high_idx - invalid_count);
  // printf("invalid_count: %d\n", invalid_count);
  if(!celcius) {
    // printf("!celcius current=%f\n", current);
    current = c_to_f(current);
    avg = c_to_f(avg);
    min = c_to_f(min);
    max = c_to_f(max);
  }
  char unit = celcius? 'C' : 'F'; //celcius or Fahrenheit

  //create msg
  sprintf(watch_msg, 
    "{\n\"name\": \"Current: %.2f%c\\nAverage: %.2f%c\\nMinimum: %.2f%c\\nMaximum: %.2f%c\"\n}\n", 
    current, unit, avg, unit, min, unit, max, unit);
  // sprintf(watch_msg, "{\n\"name\": \"reporttemp\"\n}\n");
  printf("The message to watch was:\n%s", watch_msg);
}


void terminate() {
    pthread_mutex_lock(&lock_running);
    running = 0;
    pthread_mutex_unlock(&lock_running);
}

//send msg to arduino
void send_to_arduino(char signal) {
  
  /**message mapping list:
  a: switch Celcius and Fahrenheit
  b: create temperature report to watch
  c: switch in and out of standby mode
  d: switch party mode
  e: stop watch
  f: create historical temperature report to watch from log
  */

  if(signal == 'q') {
    terminate();
    pthread_exit(NULL);
  } else if(signal == 'a' && !standby) { //switch celcius fahrenheit, must be done in not standby
    pthread_mutex_lock(&lock_arduino);
    write(arduino, "a", 2);
    pthread_mutex_unlock(&lock_arduino);
    celcius = (celcius == 1 ? 0 : 1); //flip 1 and 0
    sprintf(watch_msg, "{\n\"name\": \"In %s\"\n}\n", (celcius? "Celcius" : "Fahrenheit"));
  } else if(signal == 'c') {
    pthread_mutex_lock(&lock_arduino);
    write(arduino, "c", 2);
    pthread_mutex_unlock(&lock_arduino);
    standby = (standby == 1 ? 0 : 1); //standby mode
    printf("arduino, c. standby: %d\n", standby);
    sprintf(watch_msg, "{\n\"name\": \"%s Standby Mode\"\n}\n", (standby? "In" : "Not In"));
  } else if(signal == 'b') { //create msg to watch
    pthread_mutex_lock(&lock_arduino);
    write(arduino, "b", 2);
    pthread_mutex_unlock(&lock_arduino);
    printf("\nb create_temp_report...\n");
    create_temp_report(&record); //print temperature report
    printf("\nb create_temp_report done\n");
  } else if(signal == 'e') {
    printf("arduino, e, before, standby: %d\n", standby);
    pthread_mutex_lock(&lock_arduino);

    write(arduino, "e", 2);
    pthread_mutex_unlock(&lock_arduino);
    sprintf(watch_msg, "{\n\"name\": \"Stop Watch\"\n}\n");
    standby = 1; //standby mode
    printf("arduino, e standby: %d\n", standby);
  } else if (signal == 'd' && !standby) {
    pthread_mutex_lock(&lock_arduino);
    write(arduino, "d", 2);
    pthread_mutex_unlock(&lock_arduino);
  } else if (signal == 'f' && !standby) {
    char* msg = read_from_file();
    printf("%s\n", msg);
    strcpy(watch_msg, msg);
    free(msg);
    // sprintf(watch_msg, "{\n\"name\": \"abc\"\n}\n");
  }
}

/*
read from arduino
*/
void* read_arduino() {
  char write_buf[BUFFER];
  sleep(4); //wait for 4 seconds to warm up
  int i, j = 0;
  // printf("Read arduino");
  while(1) {
    pthread_mutex_lock(&lock_running);
    if(running == 0) {
      pthread_mutex_unlock(&lock_running);
      break;
    }
    pthread_mutex_unlock(&lock_running);

    sleep(1);
    for(i = 0; i < BUFFER; i++) {
      buf[i] = 0;
    }
    printf("read_arduino lock...\n");
    pthread_mutex_lock(&lock_arduino);
    int chars_read = read(arduino, buf, BUFFER); 
    printf("chars read: %d\n", chars_read);
    pthread_mutex_unlock(&lock_arduino);
    printf("read_arduino unlock\n");
    for(i = 0; i < BUFFER; i++, j++) {
      if(buf[i] == '\0') {
        break;
      }
      write_buf[j] = buf[i];
      if(write_buf[j] == '\n') {
        write_buf[j] = '\0'; //no new line character
        sprintf(message, "%s", write_buf);
        // printf("message: %s\n", write_buf);
        double temp = atof(message);
        // printf("Temperature atof: %f\n", temp);
        add_new(temp);
        j = -1;
      }
    }
  }
  void* p;
  return p;
}

//helper for reading record
int get_index(int ptr) {
  return ptr % HOUR;
}

//called by the log_writer_t thread, write log file from record buffer
void write_logs() {
  // printf("write_logs, ptr: %d, log_ptr: %d\n", get_index(record.ptr), get_index(record.log_ptr));
  while(get_index(record.log_ptr) > get_index(record.ptr)) {
    write_log(record.buffer[record.log_ptr], &record.time[record.log_ptr]);
    record.log_ptr++;
  }

  if( record.log_ptr >= HOUR ) {
    record.log_ptr -= HOUR;
  }

  while( get_index(record.log_ptr) < get_index(record.ptr) ) {
    write_log(record.buffer[record.log_ptr], &record.time[record.log_ptr]);
    record.log_ptr++;
  }
}

/*The function to the log_writer_t thread, read temperature record from record in memory to file
*/
void* log_record() {
  printf("log_record\n");
  while(1) {
    pthread_mutex_lock(&lock_running);
    if(running == 0) {
      pthread_mutex_unlock(&lock_running);
      break;
    }
    pthread_mutex_unlock(&lock_running);

  printf("sleep....\n");
  fflush(stdout);
    sleep(2);
    // int trial = 0; //trial times, try 10 times
    while(pthread_mutex_trylock(&lock_record) != 0) { //lock fails
      // trial++;
      printf("sleep...");
      fflush(stdout);
      sleep_ms(500); //sleep for 100 milliseconds
    }
    pthread_mutex_lock(&lock_f);
    printf("start to write log\n");
    fflush(stdout);
    write_logs();
    pthread_mutex_unlock(&lock_f);
    pthread_mutex_unlock(&lock_record);
  }
  void* p;
  return p;
}

//create a new thread to parse historical record everytime the pebble wants it
//returns the message got from log

int create_log_writer_thread() {
  int rv = pthread_create(&log_writer_t, NULL, &log_record, NULL);
  if(rv != 0) {
    printf("file thread create error");
    close(arduino);
    terminate();
    return 0;
  }
  return 1;
}

/** create the thread to read temperature from arduino
*/
int create_read_thread() {
  //return value of the second thread
  //create thread, user listener
  int rv = pthread_create(&read_arduino_t, NULL, &read_arduino, NULL);
  if(rv != 0) {
    printf("read thread create error");
    close(arduino);
    terminate();
    return 0;
  }
  return 1;
}

int init_locks() {
  int rv = pthread_mutex_init(&lock_record, NULL); //1: failed
  rv |= pthread_mutex_init(&lock_arduino, NULL);
  rv |= pthread_mutex_init(&lock_running, NULL);
  return rv;
}

int destroy_locks() {
  int rv = pthread_mutex_destroy(&lock_record); //1: failed
  rv |= pthread_mutex_destroy(&lock_arduino);
  rv |= pthread_mutex_destroy(&lock_running);
  return rv;
}

int initialize_arduino() {
  record.ptr = 0; //initialize ptr
  record.log_ptr = 0;
  record.buffer[0] = INVALID; // make it an impossible temperature

  arduino = open("/dev/cu.usbmodem1421", O_RDWR);

  // int flags = fcntl(arduino, F_GETFL, 0);
  // arduino = fcntl(arduino, F_SETFL, flags | O_NONBLOCK);

  running = 1; //flag to tell the program is running
  if(arduino == -1) {
    fprintf(stderr, "can't open the device!\n");
    return 0;
  }
  
  //2. configure the connection
  struct termios options; 
  
  tcgetattr(arduino, &options); 
  cfsetispeed(&options, 9600); 
  cfsetospeed(&options, 9600);

  options.c_cc[VTIME] = 10;
  options.c_cc[VMIN] = 0;
  tcsetattr(arduino, TCSANOW, &options); 

  if(setup_log_file() == 0) {
    printf("log.txt Failed");
    return 0;
  }

  if(init_locks() != 0) {
    printf("init locks failed");
    return 0; //failed
  }

  printf("create_read_thread");
  int rv = create_read_thread();
  if(!rv) {
    return 0;
  }
  printf("create_log_writer_thread");
  return create_log_writer_thread();
}


/*
close communication to arduino
*/
int close_communication() {
  void *r = NULL;
  pthread_join(read_arduino_t, r);
  void *r_file = NULL;
  pthread_join(log_writer_t, r_file);
  // free(r_file);
  close(arduino);
  destroy_locks();
  exit(0);
}

// int main() {
//   int rv = initialize(); 
//   if(rv == 0) {
//     return 0;
//   }
//   return communicate_arduino();
// }




