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
#define TARGET_PORT 14001		// CHANGE HERE
#define LOCAL_PORT 15000			// CHANGE HERE

#define BUFFERS_LEN 982
#define MY_CRC_POS 978
#define PACKET_SIZE	974
#define CRC_POS 4
#define CRC_COUNT 1
#define PATH_LEN 63

#define WINDOW_SIZE 200

using boost::uuids::detail::md5;
md5 hash;
md5::digest_type digest;

struct Data_Packet {
	unsigned int pos;
	char buffer[BUFFERS_LEN];
};

std::deque<Data_Packet> window_q;

/////////////////////////////////////////////////////////////////////////
// FUNCTIONS
/////////////////////////////////////////////////////////////////////////

// WORKING
void copy_data(char* source, char* dest, int num_chars, int source_offset, int dest_offset) {
	// Iterate over the number of characters to be copied
	for (int i = 0; i < num_chars; i++) {
		// Copy the character from the source array to the destination array,
		// taking into account the specified offsets for each array
		dest[i + dest_offset] = source[i + source_offset];
	}
}

// WORKING
void data_to_buffer(char* buffer_tx, int pos_in_buff, unsigned int data) {
	// Size of unsigned int is 4 bytes. stores them into buffer byte by byte
	buffer_tx[0 + pos_in_buff] = (data & (255 >> 0));			// ulozi spodnich 8 bitu
	buffer_tx[1 + pos_in_buff] = (data & (255 << 8)) >> 8;		// 8-16
	buffer_tx[2 + pos_in_buff] = (data & (255 << 16)) >> 16;	// 16-34
	buffer_tx[3 + pos_in_buff] = (data & (255 << 24)) >> 24;	// 24-32
}

// WORKING
void add_crc(char* buffer_tx, int size_of_packet) {
	boost::crc_32_type  result;
	result.process_bytes(buffer_tx, size_of_packet);
	unsigned int checksum = result.checksum();
	data_to_buffer(buffer_tx, CRC_POS, checksum);
}

//WORKING
unsigned int data_from_packet(char* buffer_tx, int pos_in_buff) {
	// Returns 4 bytes of data on position pos_in_buff
	unsigned int x0 = (unsigned char)buffer_tx[0 + pos_in_buff];
	unsigned int x1 = (unsigned char)buffer_tx[1 + pos_in_buff];
	unsigned int x2 = (unsigned char)buffer_tx[2 + pos_in_buff];
	unsigned int x3 = (unsigned char)buffer_tx[3 + pos_in_buff];
	unsigned int pos = (x3 << 24) + (x2 << 16) + (x1 << 8) + (x0 << 0);
	return pos;
}

// WORKING
bool is_start(char* buffer) {
	char start_sequence[5] = { 's', 't','a','r','t' }; //what we expect
	for (int i = 0; i < 5; i++) {
		if (buffer[i] != start_sequence[i])
			return false;
	}
	return true;
}

//WORKING
void send_ack(unsigned int number, SOCKET socket) {
	char buffer[8];
	data_to_buffer(buffer, 0, number); // insert to start of the buffer
	add_crc(buffer, CRC_POS); // append crc
	sockaddr_in addr; //destination
	addr.sin_family = AF_INET;
	addr.sin_port = htons(TARGET_PORT);
	InetPton(AF_INET, _T(TARGET_IP), &addr.sin_addr.s_addr);
	sendto(socket, buffer, 8, 0, (sockaddr*)&addr, sizeof(addr));
}

//WORKING
bool write_to_file(FILE* file, char* buffer, int packet_size) {
	// The data are written to the original location
	hash.process_bytes(buffer, packet_size);
	size_t bytes_written = fwrite(buffer, sizeof(char), packet_size, file);
	if (bytes_written < 1) {
		return false;
	}
	return true;
}

// WORKING
bool check_crc(char* buffer) {
	boost::crc_32_type  result;
	result.process_bytes(buffer, MY_CRC_POS);
	unsigned int packet_checksum = data_from_packet(buffer, MY_CRC_POS);
	unsigned int computed_checksum = result.checksum();
	return (packet_checksum == computed_checksum);
}

// WORKING
int get_filename_size(char* buffer) {
	int length = 0;
	while (true) {
		char c = buffer[length + 9]; // stored from the byte 9
		length++;
		if (c == '\x03')
			return length-1; //-1 because of terminal character
	}
}


void save_to_buffer(char* buffer, char* sequence_buffer, unsigned int* buffer_indexes, unsigned int packet_number) {
	// Check if the packet is already in the buffer
	for (int i = 0; i < WINDOW_SIZE; i++) {
		if (buffer_indexes[i] == packet_number)
			return;
	}

	// Find the first empty position in the buffer
	int pos = 0;
	while (pos < WINDOW_SIZE - 1) {
		if (buffer_indexes[pos] == UINT_MAX - 6) break;
		pos++;
	}

	// Save the packet number to the buffer indices and copy the packet data to the sequence buffer
	buffer_indexes[pos] = packet_number;
	copy_data(buffer, sequence_buffer, PACKET_SIZE, 4, PACKET_SIZE * pos);
}

unsigned int buffer_to_file(char* sequence_buffer, unsigned int* buffer_indexes, unsigned int next_expected_packet, FILE* file, unsigned int file_size) {
	// Loop until all expected packets have been written to the file
	while (true) {
		// Find the first packet in the buffer that matches the next expected packet
		int pos = 0;
		while (true) {
			if (pos >= WINDOW_SIZE) return next_expected_packet;
			if (buffer_indexes[pos] < next_expected_packet) buffer_indexes[pos] = UINT_MAX - 6;
			if (buffer_indexes[pos] == next_expected_packet) break;
			pos++;
		}
		buffer_indexes[pos] = UINT_MAX - 6; //mark as free
		unsigned int packet_size = min(file_size, next_expected_packet + PACKET_SIZE) - next_expected_packet;
		char packet[PACKET_SIZE];
		copy_data(sequence_buffer, packet, packet_size, pos * PACKET_SIZE, 0);
		if (!write_to_file(file, packet, packet_size)) {
			printf("Nelze zapisovat do souboru!\n");
			printf("Ukonceni programu za 2s...\n");
			Sleep(2000);
			return -1;
		}
		next_expected_packet = next_expected_packet + packet_size;
		printf("Packet %3.0u uspesne prijaty\n", next_expected_packet / PACKET_SIZE);
	}
}

/////////////////////////////////////////////////////////////////////////
// MAIN
/////////////////////////////////////////////////////////////////////////
int main()
{
	// OPEN SOCKETS 
	char buffer_rx[BUFFERS_LEN];
	SOCKET current_socket;
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	struct sockaddr_in local;
	struct sockaddr_in from;
	int fromlen = sizeof(from);
	local.sin_family = AF_INET;
	local.sin_port = htons(LOCAL_PORT);
	local.sin_addr.s_addr = INADDR_ANY;
	current_socket = socket(AF_INET, SOCK_DGRAM, 0);

	if (bind(current_socket, (sockaddr*)&local, sizeof(local)) != 0) {
		printf("Binding Error!\n");
		printf("Ukonceni programu za 2s...\n");
		Sleep(2000);
		return -1;
	}


	// INITIALIZE VARIABLES
	unsigned int file_byte_size;
	unsigned int buffer_indexes[WINDOW_SIZE];
	for (int i = 0; i < WINDOW_SIZE; i++) {
		buffer_indexes[i] = UINT_MAX - 6;
	}
	char* buffer_for_seq;
	buffer_for_seq = (char*)malloc(PACKET_SIZE * WINDOW_SIZE);
	unsigned int next_byte_expected = 0;
	unsigned int byte_number;
	int filename_size;
	char* filename;

	printf("Cekani na poslani packetu\n");

	/////////////////////////////////////////////////////////////////////////
	// INIT PACKET
	/////////////////////////////////////////////////////////////////////////
	while (true) {
		if (recvfrom(current_socket, buffer_rx, sizeof(buffer_rx), 0, (sockaddr*)&from, &fromlen) == SOCKET_ERROR) {
			printf("Socket error Init!\n");
			printf("Ukonceni programu za 2s...\n");
			Sleep(2000);
			free(buffer_for_seq);
			return -1;
		}
		if (check_crc(buffer_rx)) {
			printf("Velikost souboru:%u bajtu\n", data_from_packet(buffer_rx, 5));
			file_byte_size = data_from_packet(buffer_rx, 5); //get file size
			filename_size = get_filename_size(buffer_rx);	 //get name size
			filename = (char*)malloc(filename_size + 1);
			copy_data(buffer_rx, filename, filename_size, 9, 0); //filename
			filename[filename_size] = '\0';
			send_ack(next_byte_expected, current_socket);	 //ack 0
			break;
		}
		else {
			printf("Chybne CRC, informuji odesilatele");
			send_ack(UINT_MAX, current_socket);
		}
	}

	/////////////////////////////////////////////////////////////////////////
	// RECEIVE PACKETS
	/////////////////////////////////////////////////////////////////////////
	bool looping = true;
	while (looping) {

		// OPEN FILE);
		FILE* file = fopen("C:\\Users\\Lukas\\Desktop\\Test2.pdf", "wb");
		if (file == NULL) {
			printf("Nelze otevrit soubor!\n");
			closesocket(current_socket);
			free(buffer_for_seq);
			free(filename);
			printf("Ukonceni programu za 2s...\n");
			Sleep(2000);
			return -1;
		}

		// INIT BUFFER INDEXES
		for (int i = 0; i < WINDOW_SIZE; i++) {
			buffer_indexes[i] = UINT_MAX - 6;
		}

		// START LOOPING
		next_byte_expected = 0; //start from beginning
		while (true) {
			if (recvfrom(current_socket, buffer_rx, sizeof(buffer_rx), 0, (sockaddr*)&from, &fromlen) == SOCKET_ERROR) {
				printf("Socket error!\n");
				printf("Ukonceni programu za 2s...\n");
				Sleep(2000);
				fclose(file);
				remove(filename);
				free(filename);
				free(buffer_for_seq);
				return -1;
			}
			if (is_start(buffer_rx)) {
				send_ack(next_byte_expected, current_socket);
				continue;
			}

			if (check_crc(buffer_rx)==false)
				printf("Chybne CRC pri cteni bytu %u\n", next_byte_expected);

			else {
				// ACK OF RECEIVING CONFIG DATA NOT ARRIVED
				if (next_byte_expected != file_byte_size && data_from_packet(buffer_rx, 0) == (UINT_MAX - 2)) {	//ack of receiving config data didnt arrive
					send_ack(UINT_MAX - 5, current_socket);
					continue;
				}


				if ((next_byte_expected != file_byte_size)) {
					byte_number = data_from_packet(buffer_rx, 0);
					if (byte_number + PACKET_SIZE < next_byte_expected)
						continue;
					save_to_buffer(buffer_rx, buffer_for_seq, buffer_indexes, byte_number);
					next_byte_expected = buffer_to_file(buffer_for_seq, buffer_indexes, next_byte_expected, file, file_byte_size);
					// TODO Later: Handle error next_byte_expected==UINT_MAX
				}

				// LAST PACKET
				if (next_byte_expected == file_byte_size && data_from_packet(buffer_rx, 0) == (UINT_MAX - 2)) {
					fclose(file);

					// COMPARE MD5 CHECKSUM
					hash.get_digest(digest);
					unsigned int packet_md5 = data_from_packet(buffer_rx, 4);
					unsigned int computed_md5 = *digest;

					// LOOP DONE, MD5 NOT MATCHING
					if (computed_md5 != packet_md5) {
						printf("Chyba MD5 se neshoduje!\n");
						printf("Ocekavane: %u, Vypocitane: %u\n", packet_md5, computed_md5);
						send_ack(UINT_MAX - 5, current_socket);
						fclose(file);
						break;
					}

					// MD5 MATCH
					else {
						printf("MD5 se shoduje s odesilatelem\n");
						send_ack(UINT_MAX - 4, current_socket);
						looping = false;
						break;
					}

				}
			}

			// ALL DATA SEND, LAST ACK
			if (next_byte_expected == file_byte_size) {
				send_ack(UINT_MAX - 3, current_socket);
				continue;
			}
			// ALL SPECIAL SITUATION HANDLED, SEND ACK
			send_ack(next_byte_expected, current_socket);
		}
	}
	closesocket(current_socket);
	free(buffer_for_seq);
	free(filename);
	printf("Uspesne prijato!\n");
	printf("Ukonceni programu za 60s...\n");
	Sleep(60000);
	return 0;
}