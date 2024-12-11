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

// Defining mutex
pthread_mutex_t lock;

const char *frontend_name = "RP Streaming Frontend";
const char *frontend_file_name = __FILE__;

BOOL frontend_call_loop = false;

INT display_period = 0; // update this later

INT max_event_size = 450; // update later
INT max_event_size_frag = 0;

INT event_buffer_size = 10*450; //update later

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
INT rbh; // Ring buffer handle

BOOL equipment_common_overwrite = true;

void* data_acquisition_thread(void* param);
//void* data_analysis_thread(void* param);

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

	pthread_mutex_init(&lock, NULL);
	
	INT status = rb_create(event_buffer_size, max_event_size, &rbh);

    if (status != DB_SUCCESS) {
        printf("Error creating ring buffer: %d\n", status);
        return FE_ERR_HW;
	}

	printf("Ring buffer created successfully with handle: %d\n", rbh);

	// Initialize data acquisition and analysis threads
	pthread_t acquisition_thread; //, analysis_thread;
	pthread_create(&acquisition_thread, NULL, data_acquisition_thread, NULL);
	//pthread_create(&analysis_thread, NULL, data_analysis_thread, NULL);

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

	return SUCCESS;
}

/*******************************************************************\
	Thread 1: Data Acquisition Thread
\*******************************************************************/
void* data_acquisition_thread(void* param)
{
	printf("Data acquisition thread started\n");
	
	EVENT_HEADER *pevent = NULL;
	ssize_t *pdata; 
	ssize_t buffer[4500];
	ssize_t bytes_read;
	INT status;
	INT pbuffer = 0;

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

		if (rbh <= 0) 
		{
    		printf("Invalid ring buffer handle: %d\n", rbh);
    		return NULL;
		}


		if (rb_get_buffer_level(rbh, &pbuffer) >= event_buffer_size) 
		{
    		printf("Buffer is full; skipping data write\n");
    		usleep(50);
    		continue;
		}

		// Acquire a write pointer in the ring buffer
		do {
			pthread_mutex_lock(&lock);
			status = rb_get_wp(rbh, (void **)&pdata, 0);
			pthread_mutex_unlock(&lock);
			printf("Status: %d\n:", status);
			if (status == DB_TIMEOUT) 
			{
				usleep(50);
				if (!is_readout_thread_enabled()) break;
			}
		} while (status != DB_SUCCESS);

		bytes_read = recv(stream_sockfd, buffer, max_event_size, 0);
		printf("Data received: %ld bytes\n", bytes_read);


		if (bytes_read <= 0)
		{
			if (bytes_read == 0)
			{
				printf("Red Pitaya disconnected\n");
				break;

			} else if (errno == EWOULDBLOCK || errno ==EAGAIN)
			{
				printf("Receive timeout\n");			
				continue;
			}

			else
			{
				printf("Error reading from the Red Pitaya: %s\n", strerror(errno));
				break;
			}

			//pthread_mutex_lock(&lock);
			//rb_increment_wp(rbh, 3*max_event_size); //sizeof(EVENT_HEADER));
			//pthread_mutex_unlock(&lock);

			continue;
		}

		ssize_t num_samples = bytes_read;

		if (num_samples > event_buffer_size)
		{
    		printf("Error: num_samples exceeds buffer size\n");
    		continue;
		}

		pthread_mutex_lock(&lock);
		for (ssize_t i = 1; i < num_samples; i++)
		{
			ssize_t derivative = (buffer[i] - buffer[i-1]) / 10000000000000;

			if (derivative == 0)
			{
				continue;
			}

			pdata[i-1] = derivative;
			printf("Derivative at sample %ld: %ld\n", i, derivative);
		}
		pthread_mutex_unlock(&lock);
		 // Adjust data pointers after reading
        if (pdata == NULL) 
		{
    		printf("Error: pdata is null in data_acquisition_thread\n");
    		continue;
		}


		if (pevent == NULL) 
		{
    		printf("Error: pevent is null in data_acquisition_thread\n");
    		continue;
		}

        pevent->data_size = static_cast<DWORD>(bytes_read);//bk_size(pevent);
		printf("Calling bk_size with event pointer: %p\n", (void *)pevent);
	
		// Unlock mutex after writing to the buffer
		pthread_mutex_lock(&lock);
		rb_increment_wp(rbh, static_cast<int>(sizeof(EVENT_HEADER) + pevent->data_size)); 
		pthread_mutex_unlock(&lock);
		
	}
	
	//free(pevent);
	printf("Exiting the data acquisition thread\n");
	return NULL;
}


/*******************************************************************\
	Thread 2: Data Analysis
\*******************************************************************/
//void* data_analysis_thread(void* param)
//{
//	printf("Data analysis thread started\n");
//	EVENT_HEADER *pevent;
//	WORD *pdata;
//	
//	while(is_readout_thread_enabled())
//	{
//		// Poll the ring buffer for new events
//		int status;
//		do{
//			pthread_mutex_lock(&lock);
//
//			status = rb_get_rp(rbh, (void **) &pevent, 0);
//			//printf("Ring buffer status: %d\n", status);
//			pthread_mutex_unlock(&lock);
//
//			if (status == DB_TIMEOUT)
//			{
//				usleep(50);
//				if (!is_readout_thread_enabled()) break;
//			}
//		} while (status != DB_SUCCESS);
//
//		if (status != DB_SUCCESS) 
//		{
//			printf("Error accessing the ring buffer: %d\n", status);
//			//pthread_mutex_unlock(&lock);
//			continue;
//		}

//		if (pevent == nullptr) 
//        {
//            printf("Error: pevent is null\n");
            //pthread_mutex_unlock(&lock);
//            continue;
//        }
        
//        if (pevent->data_size <= 0)
//        {
//            printf("Error: data_size is not valid: %d\n", pevent->data_size);
//            //pthread_mutex_unlock(&lock);
//            continue;
//        }

//		pdata = (WORD *)(pevent + 1);

        // Perform data analysis here (e.g., calculating derivatives)
//        int num_samples = pevent->data_size / sizeof(WORD);
//        for (int i = 1; i < num_samples; i++)
//        {
//            int derivative = pdata[i] - pdata[i - 1];
//            printf("Derivative at sample %d: %d\n", i, derivative);
//        }

//		pthread_mutex_lock(&lock);
		// Mark the event as processed
//		rb_increment_rp(rbh, sizeof(EVENT_HEADER) + pevent->data_size);
//		pthread_mutex_unlock(&lock);	

//	}
//	printf("Exiting the data analysis thread\n");
//	return NULL;
//}

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
	if (stream_sockfd < 0)
	{	
		printf("Stream connection lost, attempting to reconnect...\n");
		return frontend_init(); // Reinitialize the connection
	}
	
	usleep(50); // Prevent CPU overload, adjust as needed
	return SUCCESS;
}


/*******************************************************************\
	Poll for trigger event: Check if new data is available from Red Pitaya
\*******************************************************************/
INT poll_event(INT source, INT count, BOOL test)
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

	return 0;
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
	ssize_t *pdata;
	INT status;
	
	bk_init32(pevent);
	
	bk_create(pevent, "TPDA", TID_INT32, (void **) &pdata);
	//pthread_mutex_lock(&lock);

	EVENT_HEADER *ring_event = nullptr;
	pthread_mutex_lock(&lock);
	status = rb_get_rp(rbh, (void **)&pdata, 0);
	pthread_mutex_unlock(&lock);

	if (status == DB_SUCCESS && ring_event != nullptr)
	{
		ssize_t *ring_data = (ssize_t *)(ring_event + 1);
		ssize_t num_words = ring_event->data_size / sizeof(ssize_t);

		for (int i = 0; i < num_words; i++)
		{
			pdata[i] = ring_data[i];
		}

		bk_close(pevent, pdata + num_words);
		header->data_size = bk_size(pevent);

		pthread_mutex_lock(&lock);
		rb_increment_rp(rbh, static_cast<int>(sizeof(EVENT_HEADER) + header->data_size));
		pthread_mutex_unlock(&lock);
	}
		
	return bk_size(pevent); 
}

/*********************************************************************\
        Read a periodic event
\*********************************************************************/
INT read_periodic_event(char *pevent, INT off)
{
	EVENT_HEADER *header = (EVENT_HEADER *)pevent;
    ssize_t *pdata = NULL, *padc = NULL;
	INT status;

	bk_init32(pevent);

    // Create a bank with dummy data
    bk_create(pevent, "DATA", TID_INT32, (void **)&pdata);

	pthread_mutex_lock(&lock);
	status = rb_get_rp(rbh, (void **)&padc, 0);
	pthread_mutex_unlock(&lock);
	//printf("Ring buffer read status: %d\n", status);

	if (status == DB_SUCCESS && padc != NULL)  
	{
		printf("The status is: %d\n", status);

		// Ensure we don't exceed the buffer size
        ssize_t num_samples = max_event_size / sizeof(ssize_t);
        //if (num_samples <= 0)
        //{
        //    printf("Error: num_samples is invalid: %ld\n", num_samples);
        //    return FE_ERR_HW;
        //}
		
        // Safely copy data to the event bank
        memcpy(pdata, padc, num_samples * sizeof(ssize_t));  
		bk_close(pevent, pdata); 
		header->data_size = bk_size(pevent)/4;
		printf("event size: %d\n", bk_size(pevent));

		//pthread_mutex_lock(&lock);
		//rb_increment_rp(rbh, static_cast<int>(sizeof(EVENT_HEADER) + header->data_size)); 
		//pthread_mutex_unlock(&lock);
	}

	
	pthread_mutex_lock(&lock);
	rb_increment_rp(rbh, static_cast<int>(sizeof(EVENT_HEADER) + header->data_size)); 
	pthread_mutex_unlock(&lock);

	return bk_size(pevent);  
}