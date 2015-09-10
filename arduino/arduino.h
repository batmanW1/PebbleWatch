void send_to_arduino(char signal);
char* get_watch_msg();
int initialize_arduino();
int close_communication();
char* create_log_reader_thread();