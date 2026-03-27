#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>


#define INTERFACE "br-mesh"
#define PORT 25566
#define MAX_OUTPUT_SIZE 1024

// gets the IP of the corresponding EE node by getting the IP for interface 
// This interface IP is returned after subtracting 256 from it to get the EE node IP
// the IP will be put in addr
void getInAddrToEEFromSocketAndInterface(char* interface_name, int socket, struct in_addr* addr);

// send data via socket to dest_addr
// returns 0 if success, -1 if fails
ssize_t trySendPacket(int socket, char* data, struct sockaddr_in dest_addr);

// executes the command and puts the result in buffer
void executeBash(char* command, char* buffer, int buffer_size);

int main() {
	int sock;
	struct sockaddr_in dest_addr;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("socket failed\n");
		return 1;
	}
	struct in_addr addr;
	getInAddrToEEFromSocketAndInterface(INTERFACE, sock, &addr);

	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(PORT);
	dest_addr.sin_addr = addr;
							   
	time_t start;
	time_t end;

	// main loop:
	// execute command
	// send to EE node
	// wait until loop took 10 seconds in total
	while(1) {
		start = time(NULL);
		
		char result[MAX_OUTPUT_SIZE] = "";

		executeBash("batctl n", result, MAX_OUTPUT_SIZE);
		
		
		if (trySendPacket(sock, result, dest_addr) < 0) {
			perror("send failed, continueueing execution\n");

		}

		end = time(NULL);	
		if (start + 10 > end) {
			sleep(start + 10 - end);
		}

	};

	close(sock);
	return 0;
}

void getInAddrToEEFromSocketAndInterface(char* interface_name, int socket, struct in_addr* addr) {

	struct ifreq ifr;
	strncpy(ifr.ifr_name, interface_name, IFNAMSIZ -1);

	while (ioctl(socket, SIOCGIFADDR, &ifr) < 0) {
		printf("Failed to get interface IP, retrying ...\n");
		sleep(1);
	}

	struct sockaddr_in * ipaddr = (struct sockaddr_in *)&ifr.ifr_addr;
	printf("My IP: %s\n", inet_ntoa(ipaddr->sin_addr));

	uint32_t ip = ntohl(ipaddr->sin_addr.s_addr);

	ip -= 256; // subtract 256 to go from batman ip to ee ip
	addr->s_addr = htonl(ip);
	printf("Target IP: %s\n", inet_ntoa(*addr));
	return;

}



ssize_t trySendPacket(int socket, char* data, struct sockaddr_in dest_addr) {
	ssize_t send = sendto(
		socket,
		data,
		strnlen(data, MAX_OUTPUT_SIZE),
		0,
		(struct sockaddr *)&dest_addr,
		sizeof(dest_addr)
	);
	return send;
}


void executeBash(char* command, char* buffer, int buffer_size) {

	FILE *fp = popen("batctl n", "r");
	if (fp == NULL) {
		perror("failed to run command\n");
		return;
	}

	char buf[100] = "";
	int char_amt = 0;
	
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		char_amt += strnlen(buf, 100);
		if (char_amt > buffer_size) {
			break; // avoid overflow
		}
		strncat(buffer, buf, sizeof(buf));
	}
	pclose(fp);
	return;
}
