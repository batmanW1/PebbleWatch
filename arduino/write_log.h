
FILE* f; 
pthread_mutex_t lock_f;

int setup_log_file() {
	f = NULL;
 f = fopen("log.txt", "a+");
 // fprintf(f, "abc\n");
 // fflush(f);
 if(f == NULL) {
 	printf("file open failed\n");
 	return 0; //fail
 }
 return 1;
}

void write_log(double temp, struct tm* tm) {
	printf("write_log temp: %f, time: %d\n", (float) temp, tm->tm_sec);
	if (temp < THRESHOLD && temp > LOW_THRESHOLD) {
		fprintf(f, "%d-%d-%d@%d:%d:%d\t%f\n", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, 
			tm->tm_hour, tm->tm_min, tm->tm_sec, temp);
		fflush(f);
	}
}

char* read_from_file(/*void* p*/) {
	
	char time[30];
	float temperature = -1;

	char max_time[30];
	char min_time[30];
	float max_temp = LOW_THRESHOLD;
	float min_temp = THRESHOLD;

	float total_temp = 0;
	int counter = 0;

	// FILE* fp = fopen("log.txt", "r");

	pthread_mutex_lock(&lock_f);
	rewind(f);
	// int num = fscanf(fp, "%s\t%f", time, &temperature);
	// printf("Length read: %d\n", num);

	while (fscanf(f, "%s\t%f", time, &temperature) == 2) {
		// current_time = time;
		printf("%s -> %f\n", time, temperature);
		// update max temperature if current temp is larger than max
		if (temperature > max_temp) {
			max_temp = temperature;
			strcpy(max_time, time);
		}
		// update min temperature if current temp is smaller than min
		if (temperature < min_temp) {
			min_temp = temperature;
			strcpy(min_time, time);
		}
		// update total sum of temperatures and increment counter
		total_temp += temperature;
		counter++;
	}
	pthread_mutex_unlock(&lock_f);
	// fclose(fp);

	// calculate average temperature
	float avg_temp = total_temp / counter;

	char* output = malloc(1000 * sizeof(char));
	sprintf(output, "{\n\"name\": \"Max: %fC\\n%s\\nMin: %fC\\n%s\\nAvg: %fC\"\n}\n", max_temp, max_time, min_temp, min_time, avg_temp);
	return output;
	// pthread_exit(output);

}