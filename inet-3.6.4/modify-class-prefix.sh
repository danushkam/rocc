#!/bin/bash

sed -i 's/@class(inet::/@class(/g' src/inet/common/misc/ThruputMeter.ned
sed -i 's/@class(inet::/@class(/g' src/inet/common/misc/ThruputMeteringChannel.ned
sed -i 's/@class(inet::/@class(/g' src/inet/common/queue/DropTailQueue.ned
sed -i 's/@class(inet::/@class(/g' src/inet/common/queue/PriorityScheduler.ned
sed -i 's/@class(inet::/@class(/g' src/inet/linklayer/ethernet/EtherEncap.ned
sed -i 's/@class(inet::/@class(/g' src/inet/linklayer/ethernet/EtherFrameClassifier.ned
sed -i 's/@class(inet::/@class(/g' src/inet/linklayer/ethernet/EtherMACFullDuplex.ned
sed -i 's/@class(inet::/@class(/g' src/inet/networklayer/arp/ipv4/ARP.ned
sed -i 's/@class(inet::/@class(/g' src/inet/networklayer/common/InterfaceTable.ned
sed -i 's/@class(inet::/@class(/g' src/inet/networklayer/configurator/ipv4/IPv4NetworkConfigurator.ned
sed -i 's/@class(inet::/@class(/g' src/inet/networklayer/configurator/ipv4/IPv4NodeConfigurator.ned
sed -i 's/@class(inet::/@class(/g' src/inet/networklayer/ipv4/ErrorHandling.ned
sed -i 's/@class(inet::/@class(/g' src/inet/networklayer/ipv4/IGMPv2.ned
sed -i 's/@class(inet::/@class(/g' src/inet/networklayer/ipv4/IPv4.ned
sed -i 's/@class(inet::/@class(/g' src/inet/networklayer/ipv4/IPv4RoutingTable.ned
sed -i 's/@class(inet::/@class(/g' src/inet/transportlayer/udp/UDP.ned
