#pragma comment(lib, "ws2_32.lib")
#include <stdio.h>
#include <tchar.h>
#include <SDKDDKVer.h>

#include <winsock2.h>
#include "ws2tcpip.h"
#include "boost/crc.hpp"
#include <string>
#include <boost/uuid/detail/md5.hpp>
#include <ctime> 
#include <Windows.h>
#include <deque>
#include <time.h>

#define SENDER
#define TARGET_IP "127.0.0.1"	// CHANGE HERE
#define TARGET_PORT 14000		// CHANGE HERE
#define LOCAL_PORT 15001			// CHANGE HERE

#define BUFFERS_LEN 982
#define MY_CRC_POS 978
#define PACKET_SIZE	974
#define CRC_POS 4
#define CRC_COUNT 1
#define PATH_LEN 63

using boost::uuids::detail::md5;
md5 hash;
md5::digest_type digest;

struct Data_Packet {
	unsigned int pos;
	char buffer[BUFFERS_LEN];
};

std::deque<Data_Packet> window_q;



void copy_data(char* myorig, char* mynew, int from, int to) {
	for (int i = 0; i < (to - from); i++) {
		mynew[i + from] = myorig[i];
	}
}

unsigned int data_from_packet(char* buffer_tx, int pos_in_buff) {
	// Returns 4 bytes of data on position pos_in_buff
	unsigned int x0 = (unsigned char)buffer_tx[0 + pos_in_buff];
	unsigned int x1 = (unsigned char)buffer_tx[1 + pos_in_buff];
	unsigned int x2 = (unsigned char)buffer_tx[2 + pos_in_buff];
	unsigned int x3 = (unsigned char)buffer_tx[3 + pos_in_buff];
	unsigned int pos = (x3 << 24) + (x2 << 16) + (x1 << 8) + (x0 << 0);
	return pos;
}

void data_to_buffer(char* buffer_tx, int pos_in_buff, unsigned int data) {
	// Size of unsigned int is 4 bytes. stores them into buffer byte by byte
	buffer_tx[0+pos_in_buff] = (data & (255 >> 0));			// ulozi spodnich 8 bitu
	buffer_tx[1+pos_in_buff] = (data & (255 << 8)) >> 8;	// 8-16
	buffer_tx[2+pos_in_buff] = (data & (255 << 16)) >> 16;	// 16-34
	buffer_tx[3+pos_in_buff] = (data & (255 << 24)) >> 24;	// 24-32
}

void add_crc(char* buffer_tx, int size_of_packet) {
	boost::crc_32_type  result;
	result.process_bytes(buffer_tx, size_of_packet);
	unsigned int checksum = result.checksum();
	data_to_buffer(buffer_tx, MY_CRC_POS, checksum);
}

bool check_crc(char* buffer) {
	boost::crc_32_type  result;
	result.process_bytes(buffer, CRC_POS);
	unsigned int packet_checksum = data_from_packet(buffer, CRC_POS);
	unsigned int computed_checksum = result.checksum();
	return (packet_checksum == computed_checksum);
}

bool check_ack(char* buffer, unsigned int* ack) {
	unsigned int rcv = data_from_packet(buffer, 0);
	printf("Checking ACK: rcv = %u, ack = %u\n", rcv, *ack);
	return(rcv == (*ack) ?  true :	false);
}

Data_Packet CreatePacket(FILE* file, unsigned int* ack) {

	// Stores ack data in the beginning of the buffer
	Data_Packet DATA;
	DATA.pos = *ack; //start of data
	int pos_in_buff = 0; //position of first data byte
	data_to_buffer(DATA.buffer, pos_in_buff, *ack); //stores ack number instead of data to the front

	// Read data from file
	char data[PACKET_SIZE];
	unsigned int size_of_packet = (unsigned int) fread(data, 1, PACKET_SIZE, file); //read 1 byte, PACKET_SIZE times, return number of succeesfully read bytes

	// Copy data to DATA.buffer
	copy_data(data, DATA.buffer, 4, MY_CRC_POS); //first 4 bytes are ACK/position number
	hash.process_bytes(data, size_of_packet);
	
	// Update ack position + CRC
	*ack = *ack + size_of_packet;
	add_crc(DATA.buffer, MY_CRC_POS);
	return DATA;
}

/////////////////////////////////////////////////////////////////////////
// MAIN
/////////////////////////////////////////////////////////////////////////
int main()
{
	// OPEN THE SOCKETS
	SOCKET socketS;
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	struct sockaddr_in local;
	struct sockaddr_in from;
	local.sin_family = AF_INET;
	local.sin_port = htons(LOCAL_PORT);
	local.sin_addr.s_addr = INADDR_ANY;
	socketS = socket(AF_INET, SOCK_DGRAM, 0);

	if (bind(socketS, (sockaddr*)&local, sizeof(local)) != 0) {
		printf("Binding Error!\n");
		printf("Ukonceni programu za 2s...\n");
		Sleep(2000);
		return -1;
	}
	sockaddr_in addrDest;
	addrDest.sin_family = AF_INET;
	addrDest.sin_port = htons(TARGET_PORT);
	InetPton(AF_INET, _T(TARGET_IP), &addrDest.sin_addr.s_addr);

	// INITIALIZE VARIABLES
	char mode[3];
	bool chosen = false;
	int cnt_max;
	int win_size = 1; // 1 for stop and wait
	unsigned int ack = 0;
	char file_name[PATH_LEN+1];
	int ret;
	int fromlen = sizeof(from);
	char buffer_rx[BUFFERS_LEN];
	char buffer_tx[BUFFERS_LEN];

	// TIMEOUT STRUCT
	fd_set fds;
	int n;
	struct timeval tv;
	struct timeval end_tv;
	tv.tv_sec;
	tv.tv_usec;
	end_tv.tv_sec = 2;
	end_tv.tv_usec = 0;

	// FILE LOCATION
	FD_ZERO(&fds);
	FD_SET(socketS, &fds);

	/////////////////////////////////////////////////////////////////////////
	// CHOOSE MODE
	/////////////////////////////////////////////////////////////////////////
	while (!chosen){
		printf("Zvolte si metodu:\n \tSW - Stop and Wait\n \tSR - Selective Repeat\n");
		printf("Vami zvolena metoda: ");
		ret = scanf("%2s", mode);
		if (strcmp(mode, "SW\0") == 0) {
			printf("Zvoleny Stop and Wait\n\n\n");
			chosen = true;
			tv.tv_sec = 2; // Timeout 1 sec
			tv.tv_usec = 0;
			cnt_max = 1;
			puts("");
			puts("");
		}
		else if (strcmp(mode, "SR\0") == 0) {
			printf("Zvoleny Selective Repeat\n");
			printf("Zadejte velikost okna: ");
			int ret = scanf("%d", &win_size);
			chosen = true;
			tv.tv_sec = 0; // Timeout 0.01 sec
			tv.tv_usec = 10000;
			cnt_max = 3;
			puts("");
			puts("");
		}
		else {
			printf("Spatna metoda, zkuste znovu\n\n");
		}
	}


	/////////////////////////////////////////////////////////////////////////
	// LOAD FILE
	/////////////////////////////////////////////////////////////////////////
	printf("Zadejte cestu k souboru (bez uvozovek) nebo zadejte 0 defaultni:");
	ret = scanf("%63s", file_name); //63 == PATH_LEN
	if (strcmp(file_name, "0\0") == 0)
	{
		strcpy(file_name, "C:\\Users\\Lukas\\Desktop\\Test.pdf");
		printf("Zvolena defaultni cesta k souboru: %s\n", file_name);
		
	}
	FILE* filehandle = fopen(file_name, "rb\0");
	if (filehandle == NULL) {
		printf("Invalid file location!!!\n");
		int ret = getchar(); //wait for press Enter
		return 1;
	}
	fseek(filehandle, 0L, SEEK_END);
	unsigned int file_size = ftell(filehandle);
	rewind(filehandle);
	printf("File size: %d\n", file_size);

	/////////////////////////////////////////////////////////////////////////
	// PREAPRE THE PACKETS
	/////////////////////////////////////////////////////////////////////////
	
	// Initialize Buffer
	int pck_buff_size = (file_size / PACKET_SIZE);
	if ((file_size % PACKET_SIZE) > 0) {
		pck_buff_size += 1;
	}
	printf("Pocet paketu k poslani: %d\n", pck_buff_size);
	Data_Packet* packet_buffer = (Data_Packet*)malloc(pck_buff_size * sizeof(Data_Packet));

	// Prepapre All Data_Packets
	for (int j = 0; j < pck_buff_size; j++) {
		packet_buffer[j] = CreatePacket(filehandle, &ack); //one index j is size of Data_Packet
	}

	// Save the start message and the file size into buffer
	char start[6] = "Start";
	copy_data(start, buffer_tx, 0, 5);
	data_to_buffer(buffer_tx, 5, file_size);

	// Add the filename
	file_name[strlen(file_name)] = '\x03'; //terminate char
	copy_data(file_name, buffer_tx, 9, (9 + PATH_LEN));
	add_crc(buffer_tx, MY_CRC_POS);
	ack = 0; //reset ack

	/////////////////////////////////////////////////////////////////////////
	// INITIALIZE COMMUNICATION
	/////////////////////////////////////////////////////////////////////////
	printf("Inicializace komunikace\n");
	while (true) {
		sendto(socketS, buffer_tx, sizeof(buffer_tx), 0, (sockaddr*)&addrDest, sizeof(addrDest));
		n = select(2, &fds, NULL, &fds, &end_tv);
		FD_SET(socketS, &fds);
		if (n == 0) {
			printf("Opetovne zaslani init packetu\n");
		}
		else {
			if (recvfrom(socketS, buffer_rx, sizeof(buffer_rx), 0, (sockaddr*)&from, &fromlen) == SOCKET_ERROR) {
				printf("Socket error!\n");
				printf("Ukonceni programu za 2s...\n");
				Sleep(2000);
				return -1;
			}
			if (check_crc(buffer_rx)) {
				if (check_ack(buffer_rx, &ack)) {
					printf("Init ACK uspesne prijat\n");
					char* begin = &buffer_tx[0];
					char* end = begin + sizeof(buffer_tx);
					std::fill(begin, end, 0);
					break;
				}
				else {
					printf("Neshoduje se ACK\n");
					printf("Opetovne zaslani init packetu\n");
					continue;
				}
			}
			else {
				printf("Chybne CRC\n");
				printf("Opetovne zaslani init packetu\n");
			}
		}
	}

	/////////////////////////////////////////////////////////////////////////
	// SEND DATA
	/////////////////////////////////////////////////////////////////////////
	printf("\n\nSending data packets\n");
	Data_Packet LAST;
	Data_Packet RESEND;
	unsigned int rcv_ack = 0;
	unsigned int prev_ack = 8;
	unsigned int last_ack = ack;
	int index = 0;
	int cnt = 0;
	
	/////////////////////////////////////////////////////////////////////////
	// SEND DATA
	/////////////////////////////////////////////////////////////////////////

		while (true) { // poslani jednoho packetu?
			if (window_q.empty() && index >= pck_buff_size) break;

			// SEND PACKET
			if (index < pck_buff_size && window_q.size() < win_size) { //posila win_size packetu
				if (!window_q.empty()) {
					while (window_q.back().pos < window_q.front().pos) {
						window_q.push_front(window_q.back());
						window_q.pop_back();
					}
				}
				sendto(socketS, packet_buffer[index].buffer, sizeof(buffer_tx), 0, (sockaddr*)&addrDest,
					sizeof(addrDest));
				printf("Packet Position: %8.0u , Packet ID: %4d \n", packet_buffer[index].pos, index); //total packets = pck_buff_size
				window_q.push_back(packet_buffer[index]);
				LAST = window_q.front();
				index++;
			}

			// WAIT FOR ACK
			n = select(2, &fds, NULL, &fds, &tv);
			FD_SET(socketS, &fds);

			// NO ACK RECEIVED
			if (n == 0) {
				
				if (WSAGetLastError() == SOCKET_ERROR) {
					printf("Socket error!\n");
					printf("Ukonceni programu za 2s...\n");
					Sleep(2000);
					return -1;
				}
				

				if (!window_q.empty()) { //send another packet and push it to queue
					RESEND = window_q.front();
					sendto(socketS, RESEND.buffer, sizeof(buffer_tx), 0, (sockaddr*)&addrDest, sizeof(addrDest));
					window_q.push_back(window_q.front());
					window_q.pop_front();
					cnt = 0;
				}
			}

			// RECV FAILED
			else if (n == -1) {
				printf("ERROR: recv failed: %ld\n", WSAGetLastError());
				printf("Ukonceni programu za 2s...\n");
				Sleep(2000);
				return -1;
			}

			// SUCCESS
			else {
				recvfrom(socketS, buffer_rx, sizeof(buffer_rx), 0, (sockaddr*)&from, &fromlen);
				//Resolve ACK
				if (check_crc(buffer_rx) == false) {
					printf("Chyba CRC - Snaha o opetovne zaslani packetu\n");
				}
				else {
					rcv_ack = data_from_packet(buffer_rx, 0);
					//printf("recieved ACK: %u, last position: %u\n", rcv_ack, LAST.pos);

					// ERROR ACK
					if (rcv_ack == UINT_MAX - 5) {
						printf("Problem with config ack noted\n");
						continue;
					}

					// Send packets until we reach rcv_ack
					while (LAST.pos < rcv_ack && !window_q.empty()) {
						while (window_q.back().pos < window_q.front().pos) {
							window_q.push_front(window_q.back());
							window_q.pop_back();
						}
						if (!window_q.empty()) {
							window_q.pop_front();
							if (!window_q.empty()) {
								LAST = window_q.front();
							}
						}
					}

					// If packet is missing
					if (prev_ack == rcv_ack) {
						cnt++;
					}
					else {
						prev_ack = rcv_ack;
						cnt = 0;
					}

					// Resend if the package is lost
					if (cnt >= cnt_max) { //kiled stop and wait
						if (!window_q.empty()) {
							printf("Packet ztracen: posilam znovu packet:%u\n", RESEND.pos);
							RESEND = window_q.front();
							//printNumber(RESEND.buffer, 0);
							sendto(socketS, RESEND.buffer, sizeof(buffer_tx), 0, (sockaddr*)&addrDest, sizeof(addrDest));
							window_q.push_back(window_q.front());
							window_q.pop_front();
						}
						cnt = 0;

					}

					// SUCCESS - POP LAST ITEM
					if (rcv_ack == UINT_MAX - 3) {
						if (!window_q.empty()) {
							//printf("poping last \n");
							window_q.pop_front();
						}
					}
				}
			}

		}


		/////////////////////////////////////////////////////////////////////////
		// MD5 SUM
		/////////////////////////////////////////////////////////////////////////
		
		// if last packet
		if ((rcv_ack == UINT_MAX - 3) && window_q.empty()) {
			char* begin = &buffer_tx[0]; //clear buffer
			char* end = begin + sizeof(buffer_tx);
			std::fill(begin, end, 0);
			data_to_buffer(buffer_tx, 0, UINT_MAX - 2);
			hash.get_digest(digest);
			unsigned int checksum = *digest;
			data_to_buffer(buffer_tx, 4, checksum);
			printf("MD5: %u\n", checksum);
			add_crc(buffer_tx, MY_CRC_POS);
			sendto(socketS, buffer_tx, sizeof(buffer_tx), 0, (sockaddr*)&addrDest, sizeof(addrDest));

			// Send the checksum (including crc) to receiver
			unsigned int final_data = 0;

			// MAX 20 ATTEMPTS
			for (int i = 0; i < 20; i++) {
				n = select(2, &fds, NULL, &fds, &end_tv);
				FD_SET(socketS, &fds);

				// SOCKET ERROR
				if (n == 0) {
					printf("Chyba pri poslani checksum, opetovne zaslani\n");
					if (WSAGetLastError() == SOCKET_ERROR) {
						printf("Socket error (checksum)!\n");
						printf("Ukonceni programu za 2s...\n");
						Sleep(2000);
						return -1;
					}
					sendto(socketS, buffer_tx, sizeof(buffer_tx), 0, (sockaddr*)&addrDest, sizeof(addrDest));
				}

				// RECV FAILED
				else if (n == -1) {
					printf("ERROR: recv failed (checksum): %ld\n", WSAGetLastError());
					printf("Ukonceni programu za 2s...\n");
					Sleep(2000);
					return -1;
				}

				// SUCCESS
				else {
					recvfrom(socketS, buffer_rx, sizeof(buffer_rx), 0, (sockaddr*)&from, &fromlen);
					if (check_crc(buffer_rx) == false) {
						printf("Chybne CRC (checksum)");
						printf("Snaha o opetovne zaslani");
						i--;
						continue;
					}
					else {
						final_data = data_from_packet(buffer_rx, 0);

						// MD5 MATCH
						if (final_data == UINT_MAX - 4) {
							printf("MD5 se shoduje s receiverem\n");
							break;
						}

						// LOOP DONE, MD5 NOT MATCHING
						else if (final_data == UINT_MAX - 5 || final_data == 0) {
							printf("MD5 se neshoduje, nutno zopakovat cely prenos\n");
							printf("Ukonceni programu za 2s...\n");
							Sleep(2000);
							return -1;
						}
						else {
							i--;
							continue;
						}
					}
				}
			}
		}
	free(packet_buffer);
	closesocket(socketS);
	printf("\nUspesne poslano!\n");
	printf("Ukonceni programu za 60s...\n");
	Sleep(60000);
	return 0;
}
