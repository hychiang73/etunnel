# etunnel
Etunnel is a linux module to communicate between ethernet and hwsim virtual ap interface.

# Configuration
In the test environment, this is the step to set up:
## 1. Insert module for etunnel
```bash=
sudo insmod compat.ko
sudo insmod cfg80211.ko
sudo insmod mac80211.ko
sudo insmod ath.ko
sudo insmod ath10k_core.ko
sudo insmod ath10k_pci.ko
sudo insmod kumo.ko
sudo insmod mac80211_hwsim.ko radios=1
sudo insmod etunnel.ko dbg_level=1
```
- hwsim shell be create only 1 radios for ap interface. If you create more than one radios, the etunnel will be confused which is the ap interface.
- etunnel modules has 5 debug level for development. Recommend to use level 1 to get etunnel basic information
	- NONE = 0
	- INFO = 1
	- MSG  = 2
	- DUMP = 3
	- ALL  = 4
- the hwsim module shell be insert before etunnel due to the dependency.

## 2. Bring up the ap interface by hostapd. In here, the "wlan0" is the hwsim radio interface.
```shell=
#!/bin/bash

IP=192.168.42.1

sudo ifconfig wlan0 up
sudo hostapd -B -f hostapd.log -i wlan0 hostapd.conf
sudo ifconfig wlan0 $IP
```
The hostapd will be executed in background and record log in hostapd.log. IP address is not necessary here.

hostapd.conf for reference:
```
interface=wlan0
driver=nl80211
ssid=hwsim
auth_algs=3
logger_syslog=-1
logger_syslog_level=2
logger_stdout=-1
logger_stdout_level=2
country_code=US
hw_mode=g
channel=6
beacon_int=100
```

## 3. Set the etunnel
```shell
#!/bin/bash

sudo ip link add dev etl0 type etl
sudo ifconfig hwsim0 up
sudo ip link set dev hwsim0 master etl0
sudo ip link set dev eth0 master etl0
```
First, register etl interface for kernel. Second, enslave hwsim0 and eth0 to etl0 interface.
The hwsim0 play a role for forwarding the ap packets to etunnel. The eth0 play a role for send the packet out to ethernet switch.

After the step is done, we can monitor the hwsim0 interface to check if the ap is work well by tcpdump or tshark.
```
$ tcpdump -i hwsim0 -w sniffer.pcap
$ tshark -i hwsim0
```

