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
#define REDPITAYA_IP "169.254.182.13" // Enter "IP ADDRESS" HERE
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

INT max_event_size = 500000; // update later
INT max_event_size_frag = 0;

INT event_buffer_size = 10*500000; //update later

// Forward Declarations
INT frontend_init();
INT frontend_exit();
INT read_trigger_event(char *pevent, INT off);
INT read_periodic_event(char *pevent, INT off);
INT poll_event(INT source, INT count, BOOL test);
INT begin_of_run(INT run_number,  char *error);
INT end_of_run(INT run_number, char *error);
INT pause_run(INT run_number, char *error);
INT resume_run(INT run_number, char *error);
INT frontend_loop();
int rbh; // Ring buffer 
//INT trigger_thread(void *param);

BOOL equipment_common_overwrite = false;

void* data_acquisition_thread(void* param);
void* data_analysis_thread(void* param);

extern HNDLE hDB;
INT gbl_run_number;

EQUIPMENT equipment[] = {
	{"Trigger", 
		{1, 0, "SYSTEM", EQ_MULTITHREAD, 0, "MIDAS", TRUE,
			RO_RUNNING|RO_ODB, 100, 0, 0, 0, "", "", "",},
		read_trigger_event,
	},
	{"Periodic", 
		{2, 0, "SYSTEM", EQ_PERIODIC, 0, "MIDAS", TRUE,
			RO_RUNNING | RO_TRANSITIONS |RO_ODB, 1, 0, 0, 0, "", "", "",},
		read_periodic_event,
	},
	{""}
};

#ifdef __cplusplus
#endif

/****************************************************************************\

	Initialize TCP stream connection to Red Pitaya
\****************************************************************************/

INT frontend_init()
{
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
	INT status = bm_open_buffer("SYSTEM", 2048, &buf_handle);
	if (status != BM_SUCCESS)
	{
		printf("Error opening MIDAS buffer: %d\n", status);
		return FE_ERR_HW;
	}
	
	rbh = get_event_rbh(0); // Initialize the ring buffer here 

	pthread_mutex_init(&lock, NULL);

	// Initialize data acquisition and analysis threads
	pthread_t acquisition_thread, analysis_thread;
	pthread_create(&acquisition_thread, NULL, data_acquisition_thread, NULL);
	pthread_create(&analysis_thread, NULL, data_analysis_thread, NULL);

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
	EVENT_HEADER *pevent;
	WORD *pdata;
	int status;

	//Set a timeout for the recv function to prevent indefinite blocking
	struct timeval timeout;
	timeout.tv_sec = 10; //seconds
	timeout.tv_usec = 0; // 0 microseconds
	setsockopt(stream_sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));



	while (is_readout_thread_enabled())
	{

		if (!readout_enabled())
		{
			usleep(50); // do not produce events when run is stopped
			continue;
		}
		// Acquire a write pointer in the ring buffer
		int status;
		do {
			status = rb_get_wp(rbh, (void **) &pevent, 0);
			if (status == DB_TIMEOUT)
			{
				usleep(50);
				if (!is_readout_thread_enabled()) break;
			}
		} while (status != DB_SUCCESS);

		if (status != DB_SUCCESS) continue;

		// Lock mutex before accessing shared resources
		pthread_mutex_lock(&lock);

		bm_compose_event_threadsafe(pevent, 1, 0, 0, &equipment[0].serial_number);
        pdata = (WORD *)(pevent + 1);  // Set pdata to point to the data section of the event

		// Initialize the bank and read data directly into the bank
        bk_init32a(pevent);
        bk_create(pevent, "RPD0", TID_WORD, (void **)&pdata);

		//int data_limit = 16384; // max number of samples in circular memory buffer on the red pitaya

		int bytes_read = recv(stream_sockfd, pdata, max_event_size * sizeof(WORD), 0);
		printf("Data received: %d bytes\n", bytes_read);


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

		// Ensure bytes read doesn’t exceed max event size
        if (bytes_read > max_event_size) {
            printf("Error: Bytes read exceeds max_event_size limit.\n");
            pthread_mutex_unlock(&lock);
            continue;
        }
		
		 // Adjust data pointers after reading
        pdata += bytes_read / sizeof(WORD);
        bk_close(pevent, pdata);

        pevent->data_size = bk_size(pevent);
		printf("Event data size: %d\n", pevent->data_size);

		 // Verify event size does not exceed buffer size
        if (pevent->data_size > max_event_size) {
            printf("Warning: Event size (%d) exceeds max_event_size (%d)\n", pevent->data_size, max_event_size);
        }

		// Unlock mutex after writing to the buffer
		pthread_mutex_unlock(&lock);

		// Send event to ring buffer
		rb_increment_wp(rbh, sizeof(EVENT_HEADER) + pevent->data_size);
	}
	pthread_mutex_unlock(&lock);

	return NULL;
}


/*******************************************************************\
	Thread 2: Data Analysis
\*******************************************************************/
void* data_analysis_thread(void* param)
{
	printf("Data analysis thread started\n");
	// Obtain ring buffer for inter-thread data exchange
	// int rbh = get_event_rbh(0);
	EVENT_HEADER *pevent;
	WORD *pdata;
	
	while(is_readout_thread_enabled())
	{
		// Poll the ring buffer for new events
		int status;
		do{
			pthread_mutex_lock(&lock);

			status = rb_get_rp(rbh, (void **) &pevent, 0);
			printf("Ring buffer status: %d\n", status);

			if (status == DB_TIMEOUT)
			{
				pthread_mutex_unlock(&lock);
				usleep(50);
				if (!is_readout_thread_enabled()) break;
			}
		} while (status != DB_SUCCESS);

		if (status != DB_SUCCESS) 
		{
			pthread_mutex_unlock(&lock);
			continue;
		}

		if (pevent == nullptr) 
        {
            printf("Error: pevent is null\n");
            pthread_mutex_unlock(&lock);
            continue;
        }
        
        if (pevent->data_size <= 0)
        {
            printf("Error: data_size is not valid: %d\n", pevent->data_size);
            pthread_mutex_unlock(&lock);
            continue;
        }

		pdata = (WORD *)(pevent + 1);

        // Perform data analysis here (e.g., calculating derivatives)
        int num_samples = pevent->data_size / sizeof(WORD);
        for (int i = 1; i < num_samples; i++)
        {
            int derivative = pdata[i] - pdata[i - 1];
            printf("Derivative at sample %d: %d\n", i, derivative);
        }

		// Mark the event as processed
		rb_increment_rp(rbh, sizeof(EVENT_HEADER) + pevent->data_size);
	
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
	printf("Ending the run %d...\n", run_number);
	return SUCCESS;
}


/*******************************************************************\
	Pause run
\*******************************************************************/
INT pause_run(INT run_number, char *error)
{
	printf("Pausing the run %d...\n", run_number);
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
	//if (stream_sockfd < 0)
	//{	
	//printf("Stream connection lost, attempting to reconnect...\n");
	//return frontend_init(); // Reinitialize the connection
	//}
	
	usleep(1000); // Prevent CPU overload, adjust as needed
	return SUCCESS;
}


/*******************************************************************\
	Poll for trigger event: Check if new data is available from Red Pitaya
\*******************************************************************/
INT poll_event(INT source, INT count,BOOL test)
{
	//int i;
	//DWORD flag;
	//printf("Entering trigger event!\n");
	//for (i = 0; i < count; i++)
	//{
		//printf("Inside the triggered event\n");
		//flag = TRUE;
		//cm_yield(100);
		// Poll the stream for data availability
		//if (flag)
	//	if (!test) 
	//		return TRUE; // New event detected
	//}
	if (test)
	{
		ss_sleep(count);
	}

	return (0);
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

/*********************************************************************\
		Read a trigger event
\*********************************************************************/

INT read_trigger_event(char *pevent, INT off)
{
	EVENT_HEADER *header = (EVENT_HEADER *)pevent;
	WORD *pdata;
	
	bk_init32a(pevent);
	bk_create(pevent, "TPDA", TID_WORD, (void **) &pdata);
	pthread_mutex_lock(&lock);

	EVENT_HEADER *ring_event = nullptr;
	int status = rb_get_rp(rbh, (void **)&ring_event, 0);

	if (status == DB_SUCCESS && ring_event != nullptr)
	{
		WORD *ring_data = (WORD *)(ring_event + 1);
		int num_words = ring_event->data_size / sizeof(WORD);

		for (int i = 0; i < num_words; i++)
		{
			pdata[i] = ring_data[i];
		}

		bk_close(pevent, pdata + num_words);
		rb_increment_rp(rbh, sizeof(EVENT_HEADER) + ring_event->data_size);
	}

	else
	{
		bk_close(pevent, pdata);
	}

	pthread_mutex_unlock(&lock);

	header->data_size = bk_size(pevent);
		
	return bk_size(pevent);
}

/*********************************************************************\
        Read a periodic event
\*********************************************************************/
INT read_periodic_event(char *pevent, INT off)
{
	EVENT_HEADER *header = (EVENT_HEADER *)pevent;
    WORD *pdata;


	pthread_mutex_lock(&lock);
	EVENT_HEADER *ring_event = nullptr;
	int status = rb_get_rp(rbh, (void **)&ring_event, 0);

	if (status == DB_SUCCESS && ring_event != nullptr)
	{
		// If data is available in the ring buffer
		WORD *ring_data = (WORD *)(ring_event + 1);
		int data_size = ring_event->data_size;

		header->data_size = data_size;
		// Initialize the event
		bk_init32(pevent);
		// Create a bank with dummy data
    	bk_create(pevent, "DATA", TID_WORD, (void **)&pdata);

		// Copy data from the ring buffer into the bank
        int num_words = ring_event->data_size / sizeof(WORD);
        for (int i = 0; i < num_words; i++) 
		{
            pdata[i] = ring_data[i];

		}
    
		// Close the bank after copying data
    	bk_close(pevent, pdata + num_words);

    	// Mark the ring buffer data as read
    	rb_increment_rp(rbh, sizeof(EVENT_HEADER) + ring_event->data_size);
	}
    
	pthread_mutex_unlock(&lock);

    // Set the event header's data size
    header->data_size = bk_size(pevent);
		
	return bk_size(pevent);  //SUCCESS;
}

