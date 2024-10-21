#undef NDEBUG

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "midas.h"
#include "mfe.h"
#include "msystem.h"
#include <pthread.h>

// Red Pitaya TCP streaming configuration
#ifndef REDPITAYA_IP
#define REDPITAYA_IP "169.254.182.13" // Enter "IP ADDRESS HERE"
#endif 

#define REDPITAYA_PORT 8900 //Enter port number here

int stream_sockfd = -1;
INT  buf_handle; // Handle for the MIDAS buffer

// Defining mutex
pthread_mutex_t lock;

const char *frontend_name = "RP Streaming Frontend";
const char *frontend_file_name = __FILE__;

BOOL frontend_call_loop = TRUE;

INT display_period = 0; // update this later

INT max_event_size = 8400000; // update later
INT max_event_size_frag = 0;

INT event_buffer_size = 20*10000000; //update later

// Forward Declarations
INT frontend_init();
INT frontend_exit();
INT read_trigger_event(char *pevent, INT off);
INT read_periodic_event(char *pevent, INT off);
INT poll_trigger_event(INT source, INT count, BOOL test);
INT begin_of_run(INT run_number,  char *error);
INT end_of_run(INT run_number, char *error);
INT pause_run(INT run_number, char *error);
INT resume_run(INT run_number, char *error);
INT frontend_loop();
INT trigger_thread(void *param);

BOOL equipment_common_overwrite = false;

void* data_acquisition_thread(void* param);
void* data_analysis_thread(void* param);

extern HNDLE hDB;
INT gbl_run_number;

EQUIPMENT equipment[] = {
	{"Trigger", 
		{1, 0, "SYSTEM", EQ_POLLED, 0, "MIDAS", TRUE,
			RO_RUNNING|RO_ODB, 100, 0, 0, 0, "", "", "", "", "", 0, 0},
		read_trigger_event,
	},
	{"Periodic", 
		{2, 0, "SYSTEM", EQ_PERIODIC, 0, "MIDAS", TRUE,
			RO_RUNNING | RO_TRANSITIONS |RO_ODB, 1, 0, 0, 0, "", "",
			 "", "", "", 0, 0},
		read_periodic_event,
	},
	{""}
};

#ifdef __cplusplus
#endif

// Structure to hold Red Pitaya data in the bank
typedef struct
{
	float variable_name[2048]; //Example data size, ajust as necessary
} RPDA_BANK; 

/****************************************************************************\

	Initialize TCP stream connection to Red Pitaya
\****************************************************************************/

INT frontend_init()
{
	//install_poll_event(poll_trigger_event);
	printf("Initializing frontend and Red Pitaya streaming connection ... \n");

	// Create a socket for TCP streaming
	struct sockaddr_in servaddr;
	if ((stream_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("Socket creation error\n");
		return FE_ERR_HW;		
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(REDPITAYA_PORT);

	if (inet_pton(AF_INET, REDPITAYA_IP, &servaddr.sin_addr) <= 0)
	{
		printf("Invalid Red Pitaya IP address\n");
		return FE_ERR_HW;
	}

	// Connect to the Red Pitaya streaming server
	if (connect(stream_sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
	{
		printf("Connection to Red Pitaya failed\n");
		return FE_ERR_HW;
	}

	printf("Red Pitaya streaming connected successfully!\n");

	// Open MIDAS buffer for use
	INT status = bm_open_buffer("SYSTEM", 2048,  &buf_handle);
	if (status != BM_SUCCESS)
	{
		printf("Error opening MIDAS buffer: %d\n", status);
		return FE_ERR_HW;
	}
	
	pthread_mutex_init(&lock, NULL);

	// Initialize data acquisition and analysis threads
	pthread_t acquisition_thread, analysis_thread;
	pthread_create(&acquisition_thread, NULL, data_acquisition_thread, NULL);
	pthread_create(&analysis_thread, NULL, data_analysis_thread, NULL);

	// Join the threads
	//pthread_join(acquisition_thread, NULL);
	//pthread_join(analysis_thread, NULL);

	return SUCCESS;
}

/***********************************************************************\
							Close TCP connection
\***********************************************************************/

INT frontend_exit()
{
	printf("Closing Red Pitaya streaming connection ...\n");
	if (stream_sockfd >= 0)
	{
		close(stream_sockfd);
	}

	// CLose the MIDAS buffer
	bm_close_buffer(buf_handle);
	return SUCCESS;
}

/*******************************************************************\
	Thread 1: Data Acquisition Thread
\*******************************************************************/
void* data_acquisition_thread(void* param)
{
	printf("Data acquisition thread started\n");
	// Obtain ring buffer for inter-thread data exchange
	int rbh = get_event_rbh(0);
	EVENT_HEADER *pevent;
	//Set a timeout for the recv function to prevent indefinite blocking
	struct timeval timeout;
	timeout.tv_sec = 10; //seconds
	timeout.tv_usec = 0; // 0 microseconds
	setsockopt(stream_sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

	while (is_readout_thread_enabled())
	{

		if (!readout_enabled())
		{
			ss_sleep(1); // do not produce events when run is stopped
			continue;
		}
		// Acquire a write pointer in the ring buffer
		int status;
		do {
			status = rb_get_wp(rbh, (void **) &pevent, 0);
			if (status == DB_TIMEOUT)
			{
				ss_sleep(1);
				if (!is_readout_thread_enabled()) break;
			}
		} while (status != DB_SUCCESS);

		if (status != DB_SUCCESS) continue;

		// Lock mutex before accessing shared resources
		pthread_mutex_lock(&lock);

		// Buffer for incoming data
		float temp_buffer[2048] = {0};
		// Read data from the Red Pitaya through TCP socket
		int bytes_read = recv(stream_sockfd,temp_buffer, sizeof(temp_buffer), 0);
		printf("Bytes read from device %d\n", bytes_read);

		if (bytes_read <= 0)
		{
			if (bytes_read == 0)
			{
				printf("Red Pitaya disconnected\n");
				pthread_mutex_unlock(&lock);
				break;

			} else if (errno == EWOULDBLOCK || errno ==EAGAIN)
			{
				printf("Receive timeout\n");
				pthread_mutex_unlock(&lock);
				continue;
			}

			else
			{
				printf("Error reading from the Red Pitaya: %s\n", strerror(errno));
				pthread_mutex_unlock(&lock);
				continue;
			}

		}
			
		// Prepare the event header
		pevent->event_id = 2;
		pevent->trigger_mask = 0;
		pevent->data_size = bytes_read / sizeof(float);

		memcpy((float *)(pevent + 1), temp_buffer, bytes_read);

		// Unlock mutex after writing to the buffer
		pthread_mutex_unlock(&lock);

		// Send event to ring buffer
		rb_increment_wp(rbh, sizeof(EVENT_HEADER) + pevent->data_size * sizeof(float));
	}
	return NULL;
}


/*******************************************************************\
	Thread 2: Data Analysis
\*******************************************************************/
void* data_analysis_thread(void* param)
{
	printf("Data analysis thread started\n");
	// Obtain ring buffer for inter-thread data exchange
	int rbh = get_event_rbh(0);
	EVENT_HEADER *pevent;
	
	while(is_readout_thread_enabled())
	{
		// Poll the ring buffer for new events
		int status;
		do{
			pthread_mutex_lock(&lock);

			status = rb_get_rp(rbh, (void **) &pevent, 0);
			if (status == DB_TIMEOUT)
			{
				pthread_mutex_unlock(&lock);
				ss_sleep(1);
				if (!is_readout_thread_enabled()) break;
			}
		} while (status != DB_SUCCESS);

		if (status != DB_SUCCESS) 
		{
			pthread_mutex_unlock(&lock);
			continue;
		}

	// Analyze data here
	// Example analysis:
		int num_samples = pevent->data_size;// number of samples in the event
		float* data = (float*)(pevent + 1); // pointer to the data 

		for (int i = 0; i < num_samples; i++)
		{
			if (data[i] > 10000.0f)
			{
				printf("Data %d exceeds threshold: %f\n", i, data[i]); 
			}

			else 
			{
			// Data does not meet criteria, so ignore it 
				printf("Data %d is below threshold: %f, ignore\n", i , data[i]);
			}
			
		} 

		// Mark the event as processed
		rb_increment_rp(rbh, sizeof(EVENT_HEADER) + num_samples * sizeof(float));
	
		pthread_mutex_unlock(&lock);	

	}

	return NULL;
}

/********************************************************************\
	Begin of Run
\********************************************************************/
INT begin_of_run(INT run_number, char *error)
{
	printf("Starting the run %d...\n", run_number);
	return SUCCESS;
}

/*********************************************************************\
	End of run
\*********************************************************************/
INT end_of_run(INT run_number, char *error)
{
	printf("Pausing the run %d...\n", run_number);
	return SUCCESS;
}


/*******************************************************************\
	Pause run
\*******************************************************************/
INT pause_run(INT run_number, char *error)
{

	return SUCCESS;
}

/*******************************************************************\
	Resume run
\*******************************************************************/
INT resume_run(INT run_number, char *error)
{
	printf("Resuming run %d...\n", run_number);
	return SUCCESS;
}

/********************************************************************\
	Frontend loop
\********************************************************************/
INT frontend_loop()
{
//	if (stream_sockfd < 0)
//	{	
//	printf("Stream connection lost, attempting to reconnect...\n");
//	return frontend_init(); // Reinitialize the connection
//	}
	
	ss_sleep(8); // Prevent CPU overload, adjust as needed
	return SUCCESS;
}

INT poll_event(INT source, INT count,BOOL test)
{
	return 0;
}
/*********************************************************************\
		Read data from the Red Pitaya TCP stream in a trigger event
\*********************************************************************/

INT read_trigger_event(char *pevent, INT off)
{
	RPDA_BANK *pdata;
	bk_init32(pevent);

	// Buffer for incoming data
	float buffer[2048] = {0};

	// Setting a timeout for the recv function
    struct timeval timeout;
    timeout.tv_sec = 10;
	timeout.tv_usec = 0;
	setsockopt(stream_sockfd, SOL_SOCKET, SO_RCVTIMEO,(char *)&timeout, sizeof(timeout));


	// Read from the Red Pitaya TCP stream
	int bytes_read = recv(stream_sockfd, buffer, sizeof(buffer), 0);
	if (bytes_read > 0)
	{
		// Create MIDAS bank called RPDA  and store the streamed data
		bk_create(pevent, "RPDA", TID_FLOAT, (void **) &pdata);
		memcpy(pdata->variable_name, buffer, sizeof(buffer)); // Change variable_name
		bk_close(pevent, pdata + bytes_read / sizeof(float));

		// Limit event rate to 100 Hz (test most suitable range for your equipment)
	//	ss_sleep(5);
	}

	if (bytes_read <= 0)
	{
		printf("Error: Failed to read from the Red Pitaya stream\n");
		close(stream_sockfd);
		frontend_init();
	}

	else {printf("Failed to read from Red Pitaya stream\n");}

	return bk_size(pevent);
}

INT read_periodic_event(char *pevent, INT off)
{
	RPDA_BANK *pdata;
	bk_init32(pevent);

	printf("Starting periodic event\n");
	// Buffer for incoming data
	float buffer[2048] = {0};

	// Setting a timeout for the recv function
	struct timeval timeout;
	timeout.tv_sec = 10;
	timeout.tv_usec = 0;
	setsockopt(stream_sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

	// Read from the Red Pitaya TCP stream
	int bytes_read = recv(stream_sockfd, buffer, sizeof(buffer), 0);
	if (bytes_read > 0)
	{	
		printf("Bytes read: %d\n", bytes_read);
		
		if (bytes_read > max_event_size)
		{
			printf("Limiting data to max event size\n");
			bytes_read = max_event_size;
		}
		

//		int16_t value = buffer[0];
//		HNDLE hKey;
//		INT status = db_find_key(hDB, 0, "/Equipment/Periodic/Variables/RPDA", &hKey);

//		if (status == DB_SUCCESS)
//		{	// If key exists, update its value
//			db_set_value(hDB, 0, "/Equipment/Periodic/Variables/RPDA", &value, sizeof(value), 1, TID_INT16);
//		}

//		else if (status == DB_NO_KEY)
//		{	// If key does not exist in ODB, create it and set its value
//			db_create_key(hDB, 0, "/Equipment/Periodic/Variables/RPDA", TID_INT16);
//			db_set_value(hDB, 0, "/Equipment/Periodic/Variables/RPDA", &value, sizeof(value), 1, TID_INT16);
//		}
//		else
//		{
//			cm_msg(MERROR, "read_periodic_event", "Error finding or creating ODB key: %d", status);
//		}

		// Create MIBAS bank called PRDA and store the streamed data
		bk_create(pevent, "RPDA", TID_FLOAT, (void **)&pdata);
		memcpy(pdata->variable_name, buffer, bytes_read);
		bk_close(pevent, pdata + bytes_read / sizeof(float));
	}

	else if (bytes_read == 0)
	{
		printf("Error: Failed to read from Red Pitaya stream\n");
		close(stream_sockfd); 
		frontend_init();
	} 
	
	return SUCCESS;   //bk_size(pevent);
}


/*******************************************************************\
	Poll for trigger event: Check if new data is available from Red Pitaya
\*******************************************************************/

INT poll_trigger_event(INT source, INT count, BOOL test)
{
	ss_sleep(20);
	printf("Entering trigger event!\n");
	for (int i = 0; i < count; i++)
	{
		printf("%d\n", i);
		printf("Inside the triggered event\n");
		// Poll the stream for data availability
		if (!test) return true; // New event detected
	}
	return 0; //No event detected
}

INT interrupt_configure(INT cmd, INT source, PTYPE adr)
{
	switch(cmd)
		{
			case CMD_INTERRUPT_ENABLE:
				break;
			case CMD_INTERRUPT_DISABLE:
				break;
			case CMD_INTERRUPT_ATTACH:
				break;
			case CMD_INTERRUPT_DETACH:
				break;
		}
	return SUCCESS;
}
