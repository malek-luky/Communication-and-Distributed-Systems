# PROGRAMS

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
* Filter: udp and udp.port==5300 (depends what is set in Hercules)

# SUBMISSIONS
* Code
* UDP communication in Wireshark

# VISUAL STUDIO
* Properties -> C/C++ -> SDL checks -> No!
* Properties -> C/C++ -> Precompiled Headers -> Precompiled Heders -> Not using precompiled headers
* Pridat path složky boost do Properties -> C/C++ -> Additional Include Directories

# VYTVORENI PROJEKTU
* Create C/C++ Console App
* Pridat soubory do projektu
	* stdafx.cpp
	* stdafx.h
	* tergetver.h
