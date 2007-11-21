#!/bin/bash
echo "1" > /proc/sys/net/ipv4/ip_forward
ifconfig eth0:0 192.168.11.1
iptables -F
iptables -t nat -F
iptables -t nat -X
iptables -t nat -P PREROUTING ACCEPT
iptables -t nat -P POSTROUTING ACCEPT
iptables -t nat -P OUTPUT ACCEPT
iptables -t nat -A POSTROUTING -o eth1 -j MASQUERADE
#iptables -A INPUT -m state --state NEW -i eth1 -j REJECT
