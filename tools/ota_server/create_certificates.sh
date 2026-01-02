#!/bin/bash

echo "Use the IP address of the server as the Common Name (CN) for the certificate."
hostname=${1:-"$(hostname -I | awk '{print $1}')"}

echo "Using hostname/IP: $hostname"

openssl req -x509 -newkey rsa:2048 -keyout ca_key.pem -out ca_cert.pem -days 365 -nodes -subj /CN=$hostname
