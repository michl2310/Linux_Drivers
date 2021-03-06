/*
 *  Copyright (c) 2015 Warren J. Jasper <wjasper@tx.ncsu.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "E-1608.h"

#include <fcntl.h>
#include <math.h>
#include <ctype.h>


/* Test Program */
int toContinue()
{
  int answer;
  answer = 0; //answer = getchar();
  printf("Continue [yY]? ");
  while((answer = getchar()) == '\0' ||
    answer == '\n');
  return ( answer == 'y' || answer == 'Y');
}

int main(int argc, char**argv)
{
  struct tm date;
  struct in_addr network[3];
  DeviceInfo_E1608 device_info;
  double frequency;
  double volts;
  int i, j, k;
  uint16_t status;
  uint16_t value;
  uint16_t dataIn[512];
  uint32_t counter;
  uint32_t count;
  int flag;
  int ch;
  int corrected_data;

  uint8_t options;
  uint8_t input;
  uint8_t channel;
  uint8_t range;
  uint8_t nchan;
  int temp;
  char ip[16];
  char subnetmask[16];
  char gateway[16];

  device_info.device.connectCode = 0x0;   // default connect code
  device_info.device.frameID = 0;         // zero out the frameID
  device_info.queue[0] = 0;               // set count in gain queue to zero
  device_info.timeout = 1000;             // set default timeout to 1000 ms.

  if (argc == 2) {
    printf("E-1608 IP address = %s\n", argv[1]);
    device_info.device.Address.sin_family = AF_INET;
    device_info.device.Address.sin_port = htons(COMMAND_PORT);
    device_info.device.Address.sin_addr.s_addr = INADDR_ANY;
    if (inet_aton(argv[1], &device_info.device.Address.sin_addr) == 0) {
      printf("Improper destination address.\n");
      return -1;
    }
  } else if (discoverDevice(&device_info.device, E1608_PID) <= 0) {
    printf("No device found.\n");
    return -1;
  }

  if ((device_info.device.sock = openDevice(inet_addr(inet_ntoa(device_info.device.Address.sin_addr)),
					    device_info.device.connectCode)) < 0) {
    printf("Error opening socket\n");
    return -1;
  }

  buildGainTableAIn_E1608(&device_info);
  for (i = 0; i < NGAINS; i++) {
    printf("Calibration Table (Differential): Range = %d Slope = %f  Intercept = %f\n",
	   i, device_info.table_AInDF[i].slope, device_info.table_AInDF[i].intercept);
  }
  for (i = 0; i < NGAINS; i++) {
    printf("Calibration Table (Single Ended): Range = %d Slope = %f  Intercept = %f\n",
	   i, device_info.table_AInSE[i].slope, device_info.table_AInSE[i].intercept);
  }

  buildGainTableAOut_E1608(&device_info);
  for (i = 0; i < NCHAN_AOUT; i++) {
    printf("Calibration Table Analog Output: Channel = %d Slope = %f  Intercept = %f\n",
	   i, device_info.table_AOut[i].slope, device_info.table_AOut[i].intercept);
  }

  while(1) {
    printf("\nE-1608 Testing\n");
    printf("----------------\n");
    printf("Hit 'b' to blink\n");
    printf("Hit 'c' to test counter.\n");
    printf("Hit 'd' to test digitial IO.\n");
    printf("Hit 'i' to test Analog Input.\n");
    printf("Hit 'I' to test Analog Input Scan.\n");
    printf("Hit 'o' to test Analog Output.\n");
    printf("Hit 'r' to reset the device\n");
    printf("Hit 'n' to get networking information\n");
    printf("Hit 'N' to set networking information\n");
    printf("Hit 's' to get Status\n");
    printf("Hit 'e' to exit\n");

    while((ch = getchar()) == '\0' || ch == '\n');
    switch(ch) {
      case 'b': /* test to see if LED blinks */
        printf("Enter number or times to blink: ");
        scanf("%hhd", &options);
	BlinkLED_E1608(&device_info, options);
        break;
      case 'c':
	printf("connect DIO0 to CTR\n");
	DConfigW_E1608(&device_info, 0xf0);  // set pins 0-3 output
	ResetCounter_E1608(&device_info);    // reset the counter
	flag = fcntl(fileno(stdin), F_GETFL);
	fcntl(0, F_SETFL, flag | O_NONBLOCK);
	do {
	  DOut_E1608(&device_info, 0x1);
	  usleep(5000);
	  DOut_E1608(&device_info, 0x0);
	  usleep(5000);
	  CounterR_E1608(&device_info, &counter);
	  printf("Counter = %d\n", counter);
	} while (!isalpha(getchar()));
	fcntl(fileno(stdin), F_SETFL, flag);
	break;
      case 'd':
	printf("\nTesting Digital I/O...\n");
	printf("connect pins DIO[0-3] <--> DIO[4-7]\n");
	DConfigW_E1608(&device_info, 0xf0);  // set pins 0-3 output
	DConfigR_E1608(&device_info, &input);  
        printf("Digital Port Tristate Register = %#x\n", input);
	do {
	  printf("Enter a number [0-0xf] : ");
	  scanf("%x", &temp);
	  temp &= 0xf;
	  DOut_E1608(&device_info, (uint8_t) temp);
	  DOutR_E1608(&device_info, (uint8_t *) &temp);
	  DIn_E1608(&device_info, &input);
	  input = (input >> 4) & 0xf;
          printf("The number you entered = %#x   Latched value = %#x\n\n",input, temp);
	  for (i = 0; i < 4; i++) {
	    printf("Bit %d = %d\n", i, (temp>>i)&0x1);
	  }
        } while (toContinue());
        break;
      case 'i':
	printf("Input channel [0-11]: ");
	scanf("%hhd", &channel);
	printf("Input range [0-3]: ");
	scanf("%hhd", &range);
	for (i = 0; i < 20; i++) {
	  AIn_E1608(&device_info, channel, range, &value);
	  printf("Range %d  Channel %d   Sample[%d] = %#x Volts = %lf\n",
		 range, channel,  i, value, volts_E1608(value, range));
	  usleep(50000);	  
	}
        break;
      case 'I':
	printf("Testing E-1608 Analog Input Scan.\n");
	AInScanStop_E1608(&device_info, 0);  //Stop the scan if running.
	printf("Enter sampling frequency [Hz]: ");
	scanf("%lf", &frequency);
	printf("Enter number of scans (less than 512): ");
        scanf("%d", &count);
	printf("Enter number of channels per scan [1-8]: ");
	scanf("%hhd", &nchan);
	if (nchan < 1 || nchan > 8) {
	  printf("Number of channels must be between 1 and 8.\n");
	  break;
	}
	// set up the gain queue
	device_info.queue[0] = nchan;
	for (i = 0; i < nchan; i++) {
	  printf("Enter %d channel in gain queue [0-11]: ", i+1);
	  scanf("%hhd", &device_info.queue[2*i+1]);
	  printf("Enter range [0-3]: ");
	  scanf("%hhd", &device_info.queue[2*i+2]);
	}
        AInQueueW_E1608(&device_info);
        AInQueueR_E1608(&device_info);
        for (i = 0; i < device_info.queue[0]; i++) {
	  printf("%d.  channel = %hhd   range = %hhd\n", i+1, device_info.queue[2*i+1], device_info.queue[2*i+2]);
	}

        options = 0x0;
        AInScanStart_E1608(&device_info, count, frequency, options);
        AInScanRead_E1608(&device_info, count, nchan, dataIn);
        close(device_info.device.scan_sock);
        
        for (i = 0; i < count; i++) {    // scan count
	  for (j = 0; j < nchan; j++) {  
              k = i*nchan + j;  // sample number
              channel = device_info.queue[2*j+1];  // channel
              range = device_info.queue[2*j+2];    // range value
	      if (channel < DF) {  // single ended
		corrected_data = rint(dataIn[k]*device_info.table_AInSE[range].slope + device_info.table_AInSE[range].intercept);
	      } else {  // differential
		corrected_data = rint(dataIn[k]*device_info.table_AInDF[range].slope + device_info.table_AInDF[range].intercept);
	      }
	      if (corrected_data > 65536) {
		corrected_data = 65535;  // max value
	      } else if (corrected_data < 0) {
		corrected_data = 0;
	      }
	      printf("Range %d Channel %d  Sample[%d] = %#x Volts = %lf\n", range, channel,
		     k, corrected_data, volts_E1608(corrected_data, range));
	    }
	}
	break;
      case 'o':
	printf("Input channel [0-1]: ");
	scanf("%hhd", &channel);
	printf("Enter desired output voltage [-10 to 10V]: ");
	scanf("%lf", &volts);
	value = valueAOut_E1608(volts);
        printf("AOut: channel = %d  value = %#x\n", channel, value);
	if (AOut_E1608(&device_info, channel, value) == false) {
	  printf("Error in AOut_E1608.\n");
	}
	break;
      case 'e':
	close(device_info.device.sock);
	printf("Success!\n");
	return 0;
	break;
      case 'r':
	Reset_E1608(&device_info);
	break;
      case 's':
	// Print the calibration date
	getMFGCAL_E1608(&device_info, &date);
	printf("\nLast Calibration date: %s", asctime(&date));
	// print out the status
	if(Status_E1608(&device_info, &status)) {
	  printf("status = %#x\n", status);
	} else {
	  printf("Error in reading status\n");
	}
	break;
      case 'n':
        // get network configuration parameters
	NetworkConfig_E1608(&device_info, network);
	printf("Network configuration values: \n");
	printf("  IP address = %s\n", inet_ntoa(network[0]));
	printf("  subnet mask = %s\n", inet_ntoa(network[1]));
	printf("  gateway = %s\n", inet_ntoa(network[2]));
	break;

	case 'N':
		printf("Set network configuration values [xxx.xxx.xxx.xxx]: \n");
		uint8_t setIPData[22] = {0};
		
		setIPData[1] = 0;

		printf("  DHCP[0,1]: ");
		scanf("%d", &setIPData[0]);
		if(setIPData[0]){
			setIPData[0] = 0; // index 0 = dhcp on; index 3 = static;
		}else{

			setIPData[0] = 3;
			printf("  IP address: ");
			scanf("%s",ip);
			sscanf(ip, "%d.%d.%d.%d", &setIPData[2], &setIPData[3], &setIPData[4], &setIPData[5]);

			printf("  subnet mask: ");
			scanf("%s",subnetmask);
			sscanf(subnetmask, "%d.%d.%d.%d", &setIPData[6], &setIPData[7], &setIPData[8], &setIPData[9]);

			printf("  gateway: ");
			scanf("%s",gateway);
			sscanf(gateway, "%d.%d.%d.%d", &setIPData[10], &setIPData[11], &setIPData[12], &setIPData[13]);
		}
		


		if(!SetNetworkW_E1608(&device_info, setIPData, sizeof(setIPData)/sizeof(setIPData[0]))){
			printf("Error during setting...!\n");
		}else{
			
			printf("\n");
		}
	
		break;
    }
  }
}
