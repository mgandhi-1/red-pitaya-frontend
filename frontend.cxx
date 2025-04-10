/**
 * This frontend interfaces with a Red Pitaya board to acquire
 * high-frequency data via TCP streaming. 
 */


#undef NDEBUG

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <array>
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
#include <string>

// Red Pitaya TCP streaming configuration
#ifndef REDPITAYA_IP
#define REDPITAYA_IP "169.254.182.13" // Enter "IP ADDRESS" HERE
#endif 

#define REDPITAYA_PORT 8900 //Enter port number here

int stream_sockfd = -1; // TCP Socket

// MIDAS Frontend configuration
const char *frontend_name = "RP Streaming Frontend";
const char *frontend_file_name = __FILE__;

BOOL frontend_call_loop = false;
INT display_period = 0; // update this if needed
INT max_event_size = 10000; // update if needed
INT max_event_size_frag = 0;
INT event_buffer_size = 100*10000; //update later if needed

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
INT data_acquisition_thread(void* param); // Data thread

// Global Variables
INT rbh; // Ring buffer handle
BOOL equipment_common_overwrite = false;
extern HNDLE hDB; 
INT gbl_run_number;

EQUIPMENT equipment[] = {
	{"Trigger", 				// Equipment name 
		{1,     				// event ID
		 0,						// trigger mask
		"SYSTEM",				// event buffer
		EQ_MULTITHREAD, 		// equipment type
		0, 						// event source crate
		"MIDAS",             	// format
		TRUE,					// enabled
		RO_RUNNING|RO_ODB,		// read when running and update ODB
		 100, 					// poll for 100ms
		  0,  					// stop run after this event
		  0,					// number of sub events
		  0,					// log history
			 "", "", "","","",0,0},
		read_trigger_event,		// Readout Routine
	},
	{"Periodic", 				// Equipment name 
		{2,						// event ID
		 0,						// trigger mask
		"SYSTEM", 				// event buffer
		EQ_PERIODIC, 			// equipment type
		0, 						// event source crate
		"MIDAS", 				// format 
		TRUE,					// enabled
		RO_RUNNING | RO_TRANSITIONS |RO_ODB,  // read when running, transitions and update odb
		100, 					// poll for 100ms
		0, 						// stop run after this event
		0, 						// log history
		0, 
		"", "", "","", "", 0,0},
		read_periodic_event, 	// Readout Routine
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
	//printf("Initializing frontend and Red Pitaya streaming connection ... \n");

	// Create a socket for TCP streaming
	struct sockaddr_in servaddr;
	if ((stream_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("Socket creation error\n");
		return FE_ERR_HW;		
	}

	// Allocate memory to the socket
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

	// Initialize Ring Buffer
	INT status = rb_create(event_buffer_size, max_event_size, &rbh);

    if (status != DB_SUCCESS) {
        printf("Error creating ring buffer: %d\n", status);
        return FE_ERR_HW;
	}

	//printf("Ring buffer created with handle: %d\n", rbh);

	// Initialize data acquisition thread
	ss_thread_create(data_acquisition_thread, (void*)(PTYPE)0);
	
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

	// Delete ring buffer 
	rb_delete(rbh);

	return SUCCESS;
}

/*******************************************************************\
	Thread 1: Data Acquisition Thread
\*******************************************************************/
INT data_acquisition_thread(void* param)
{
	//printf("Data acquisition thread started\n");
	
	EVENT_HEADER *pevent = nullptr;
	int32_t *pdata = nullptr; 
	int32_t buffer[2057];
	ssize_t bytes_read;
	INT status = 0;

	/* tell framework that we are alive */
	signal_readout_thread_active(0, TRUE);

	//Set a timeout for the recv function to prevent indefinite blocking
	struct timeval timeout;
	timeout.tv_sec = 20; //seconds
	timeout.tv_usec = 0; // 0 microseconds
	setsockopt(stream_sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

	while (is_readout_thread_enabled())
	{

		if (!readout_enabled())
		{
			usleep(20); // do not produce events when run is stopped
			continue;
		}

		if (rbh <= 0) 
		{
    		printf("Invalid ring buffer handle: %d\n", rbh);
    		return FE_ERR_HW;
		}

		// Acquire a write pointer in the ring buffer
		do {
			status = rb_get_wp(rbh, (void **)&pevent, 500);
			//printf("rb_get_wp status in data acquisition thread: %d, pevent: %p\n", status, (void *)pevent);
			if (status == DB_TIMEOUT) 
			{
				usleep(20);
				if (!is_readout_thread_enabled()) break;
			}
		} while (status != DB_SUCCESS);

		pdata = (int32_t *)(pevent + 1);
		//printf("pdata : %p\n", (void *)pdata);

		bytes_read = recv(stream_sockfd, buffer, sizeof(buffer), 0);
		//printf("Data received: %ld bytes\n", bytes_read);

		// Check if data is available
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

			continue;
		}

		int32_t num_samples = static_cast<int32_t>(bytes_read / sizeof(int32_t));
		//printf("Number of Samples: %d\n", num_samples);
		
		for (int32_t i = 1; i < num_samples; i++)
		{
			// Calculate difference between consecutive data points
			int32_t derivative = (buffer[i] - buffer[i-1]);
			
			*pdata++ = derivative;
		
		}

		if (pdata == nullptr) 
		{
    		printf("Error: pdata is null in data_acquisition_thread\n");
    		continue;
		}
		 // Adjust data pointers after reading
		rb_increment_wp(rbh, static_cast<int>(sizeof(EVENT_HEADER) + bytes_read)); 
	}
	
	/* tell framework that we are finished */
	signal_readout_thread_active(0, FALSE);
	//printf("Exiting the data acquisition thread\n");
	return 0;
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
	if (stream_sockfd < 0)
	{	
		printf("Stream connection lost, attempting to reconnect...\n");
		return frontend_init(); // Reinitialize the connection
	}
	
	usleep(10); // Prevent CPU overload, adjust as needed
	return SUCCESS;
}


/*******************************************************************\
	Poll for trigger event: Check if new data is available from Red Pitaya
\*******************************************************************/
INT poll_event(INT source, INT count, BOOL test)
{
	int bufferLevel;
	int i;

	for (i = 0; i < count; i++) 
	{
		rb_get_buffer_level(rbh, &bufferLevel);
		//printf("Buffer Level %d\n", bufferLevel);
	}
	
	//DWORD flag;
	//printf("Entering trigger event!\n");
	//for (i = 0; i < count; i++)
	//{
		//flag = TRUE;
		//cm_yield(100);
		// Poll the stream for data availability
		//if (flag)
			//if (!test) 
				//return TRUE; // New event detected
	//}

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
	//printf("Entering Trigger event\n");
	int32_t *pdata;
	int32_t *padc;
	int a;
	int bufLevel;

	rb_get_buffer_level(rbh, &bufLevel);

	INT status = rb_get_rp(rbh, (void **)&padc, 500);

	if (status != DB_SUCCESS) 
	{
    	//printf("Error: rb_get_rp failed with status %d\n", status);
        return 0; // Exit if no data is available
    }

	//printf("Status in trigger event readout: %d\n", status);
	
	//int num_samples = bufLevel / sizeof(int32_t);
	//printf("Number of samples available : %d\n", num_samples);

	
	bk_init32(pevent);
	bk_create(pevent, "TADC", TID_INT32, (void **)&pdata);

	for (a=0; a < 100; a++)
	{
		if (padc[a] > 4100000 || padc[a] < -4100000)
		{
			*pdata++ = padc[a];
		}
		
	}

	bk_close(pevent, pdata);

	rb_increment_rp(rbh, static_cast<int>(sizeof(EVENT_HEADER) + 100 * sizeof(int32_t)));
	//printf("Exiting Trigger event\n");
	return bk_size(pevent);
}

/*********************************************************************\
        Read a periodic event
\*********************************************************************/
INT read_periodic_event(char *pevent, INT off)
{
	//printf("Entering Periodic event\n");
	int32_t *pdata;
	int32_t *padc;
	int a;
	int bufLevel;

	rb_get_buffer_level(rbh, &bufLevel);

	INT status = rb_get_rp(rbh, (void **)&padc, 500);

	if (status != DB_SUCCESS) 
	{
    	//printf("Error: rb_get_rp failed with status %d\n", status);
        return 0; // Exit if no data is available
    }

	//printf("Status in periodic event readout: %d\n", status);
	
	//int num_samples = bufLevel / sizeof(int32_t);
	//printf("Number of samples available : %d\n", num_samples);

	
	bk_init32(pevent);
	bk_create(pevent, "DATA", TID_INT32, (void **)&pdata);

	for (a=0; a < 100; a++)
	{
		*pdata++ = padc[a];
	}

	bk_close(pevent, pdata);

	rb_increment_rp(rbh, static_cast<int>(sizeof(EVENT_HEADER) + 100 * sizeof(int32_t)));
	//printf("Exiting Periodic event\n");
	return bk_size(pevent);
}
