# SOFTWARE

## HERCULES
* Module IP: receiver IP adress (local IP 127.0.0.1?)
	* possible to open two hercules on one PC and switch the ports in each window
	* When using Softether, use the generated IP adress
* Port: receiver port
* Local Port: sender port

## SOFTETHER
* Host Name: psia.softether.net
* Port Number: 5555
* Auth Type: Radius or NT + FEL Heslo
* Username: user
* Rest is default

## WIRESHARK
* Communication is via VPN-VPN Client
* Filter: udp and udp.port==14000 or udp.port==14001 (depends what is set in Hercules)

## VISUAL STUDIO
* Properties -> C/C++ -> SDL checks -> No!
* Properties -> C/C++ -> Precompiled Headers -> Precompiled Heders -> Not using precompiled headers
* Pridat path složky boost do Properties -> C/C++ -> Additional Include Directories

## NETDERPER
* Simualtes broken pipe
* Edit config.json source/target port and IP!

### VYTVORENI PROJEKTU
* Create C/C++ Console App
* Pridat soubory do projektu
	* stdafx.cpp
	* stdafx.h
	* tergetver.h

# PROJECT
## VARIABLES
* filehandle = fd, resp. odkaz na soubor co posilame
* buffer_rx = pointer na Data_Packet.buffer
	* TX = transmiting
	* RX = receving
* ack = byte position and acknowledge bit at once?

## POSLANI PRES SOCKETY
* sendto() pošle
* recvfrom() přijme

## DATA
* Data.pos[unsigned int]
	* pos is just internally, we send only the buffer
* Data.buffer[982]

### DATA.BUFFER
* 4 position
* 974 data (PACKET_SIZE)
* 4 CRC
* Celkem 982 buffer length

## STRUCT DATA_PACKET
* unsigned int: 32 bits
* buffer: 982 bits

## ACK
* UINT_MAX-2 = ack on config not arrived (init packet)
* UINT_MAX-3 = last item
* UINT_MAX-4 = MD5 se shoduje
* UINT_MAX-5 = error ack on receiver site
* UINT_MAX-5 = error in MD5 checksum (only applies for final packet)
* UINT_MAX-6 = free position in buffer
