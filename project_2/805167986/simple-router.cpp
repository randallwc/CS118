/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/***
 * Copyright (c) 2017 Alexander Afanasyev
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation, either version
 * 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "simple-router.hpp"
#include "core/utils.hpp"
#include "arp-cache.hpp"

#include <fstream>

namespace simple_router {

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//IMPLEMENT THIS METHOD
void
SimpleRouter::handlePacket(const Buffer& packet, const std::string& inIface)
{
  // print details about packet and interface
  std::cerr << "Got packet of size " << packet.size() << " on interface " << inIface << std::endl;

  const Interface* iface = findIfaceByName(inIface);
  if (iface == nullptr) {
    std::cerr << "Received packet, but interface is unknown, ignoring" << std::endl;
    return;
  }

  std::cerr << getRoutingTable() << std::endl;
  
  // Copy of packet that we will modify 
  Buffer outgoing_packet(packet);

  /////////////////////
  // Ethernet Frames //
  /////////////////////

  ethernet_hdr* ethHdr = (ethernet_hdr *)outgoing_packet.data();

  // Check that payload type is IPv4
  if(ethHdr->ether_type != htons(ethertype_ip)) {
    std::cerr << "Payload type is not IPv4" << std::endl;
    return;
  }

  // Check that frame is destined to router (MAC address are the same)
  uint8_t* dhost = ethHdr->ether_dhost;
  Buffer mac_adr = iface->addr;
  bool valid_broad = true;
  bool valid_mac = true;
  for(size_t i = 0; i < ETHER_ADDR_LEN; i++) {
    if(dhost[i] != BroadcastEtherAddr[i])
      valid_broad = false;
    if(dhost[i] != mac_adr[i])
      valid_mac = false;
  }
  // check if either broadcast ether address or mac address are not valid
  if(!valid_broad && !valid_mac) {
    std::cerr << "Frame is not destined to router" << std::endl;
    return;
  }
  
  //////////////////
  // IPv4 Packets //
  //////////////////

  // get ip header
  struct ip_hdr* ipHdr = (struct ip_hdr*)(outgoing_packet.data()+sizeof(struct ethernet_hdr));

  // check length
  if(ntohs(ipHdr->ip_len) < 20){
    std::cerr << "Packet does not meet minimum length" << std::endl;
    return;
  }

  // check checksum
  uint16_t check_sum = ipHdr->ip_sum;
  //Set checksum to 0
  ipHdr->ip_sum = 0;
  if (check_sum != cksum(ipHdr, sizeof(struct ip_hdr))){
    std::cerr << "Packet checksum is invalid" << std::endl;
    return;
  }
  
  // Decrement its TTL by 1
  ipHdr->ip_ttl -= 1;
  
  // check if ttl is 0
  if(ipHdr->ip_ttl == 0){
    std::cerr << "Time to Live is 0" << std::endl;
    return;
  }

  // recompute checksum  over 
  ipHdr->ip_sum = cksum(ipHdr, sizeof(struct ip_hdr));  

  // Check if IP packet is destined to router
  // 1. If it is, format response to ping packet
  // 2. If is not, perform next hop lookup and send packet
  // Check over IP address
  // of all router's interface (i.e. iterate over set m_ifaces)
  if(findIfaceByIp(ipHdr->ip_dst) != NULL) {
    //////////////////
    // ICMP Packets //
    //////////////////

    // ICMP protocol number is 1
    if(ipHdr->ip_p != ip_protocol_icmp) {
      std::cerr << "Protocol is not ICMP" << std::endl;
      return;
    }

    // get icmp header
    struct icmp_hdr* icmpHdr = (struct icmp_hdr*)(outgoing_packet.data() + sizeof(struct ethernet_hdr) + sizeof(struct ip_hdr));
    
    // Check if type is ECHO (8)
    if(icmpHdr->icmp_type != 8) {
      std::cerr << "Packet is not an ECHO request" << std::endl;
      return;
    }

    // Verify checksum
    // ICMP header checksum covers BOTH the ICMP header and the “Data” field
    uint16_t icmp_check_sum = icmpHdr->icmp_sum;
    int icmp_len = packet.size() - sizeof(struct ip_hdr) - sizeof(struct ethernet_hdr);
    icmpHdr->icmp_sum = 0;

    if(icmp_check_sum != cksum(icmpHdr, icmp_len)) {
      std::cerr << "Invalid checksum" << std::endl;
      return;
    }

    // Generate ECHO respone
    icmpHdr->icmp_type  = 0;
    icmpHdr->icmp_code  = 0;
    icmpHdr->icmp_sum   = cksum(icmpHdr, icmp_len);

    // Swap IP source and destination
    uint32_t temp_ip_src = ipHdr->ip_src;
    ipHdr->ip_src = ipHdr->ip_dst;
    ipHdr->ip_dst = temp_ip_src;
    ipHdr->ip_ttl = 64;
    ipHdr->ip_sum = 0; 
    ipHdr->ip_sum = cksum(ipHdr, sizeof(struct ip_hdr));
  }

  // Next hop lookup
  struct RoutingTableEntry forward_dest = m_routingTable.lookup(ipHdr->ip_dst);
  const Interface* outgoing_iface = findIfaceByName(forward_dest.ifName);

  //Set Ethernet frame source address to 
  // MAC address interface to send on
  for(size_t i = 0; i < ETHER_ADDR_LEN; i++) {
    ethHdr->ether_shost[i] = outgoing_iface->addr[i];
  }

  // Check ARP cache for next_hop MAC address
  auto arp_entry = m_arp.lookup(forward_dest.gw); // ved
  if(arp_entry != NULL) {
    // Set Ethernet frame destination address
    for(size_t i = 0; i < ETHER_ADDR_LEN; i++) {
      ethHdr->ether_dhost[i] = arp_entry->mac[i];
    }
    //Send Packet
    sendPacket(outgoing_packet, forward_dest.ifName);
  }
  
  // Queue Packet
  else {
    m_arp.queueRequest(forward_dest.gw, outgoing_packet, forward_dest.ifName);
  }
}
// //////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////

// You should not need to touch the rest of this code.
SimpleRouter::SimpleRouter()
  : m_arp(*this)
{
}

void
SimpleRouter::sendPacket(const Buffer& packet, const std::string& outIface)
{
  m_pox->begin_sendPacket(packet, outIface);
}

bool
SimpleRouter::loadRoutingTable(const std::string& rtConfig)
{
  return m_routingTable.load(rtConfig);
}

void
SimpleRouter::loadIfconfig(const std::string& ifconfig)
{
  std::ifstream iff(ifconfig.c_str());
  std::string line;
  while (std::getline(iff, line)) {
    std::istringstream ifLine(line);
    std::string iface, ip;
    ifLine >> iface >> ip;

    in_addr ip_addr;
    if (inet_aton(ip.c_str(), &ip_addr) == 0) {
      throw std::runtime_error("Invalid IP address `" + ip + "` for interface `" + iface + "`");
    }

    m_ifNameToIpMap[iface] = ip_addr.s_addr;
  }
}

void
SimpleRouter::printIfaces(std::ostream& os)
{
  if (m_ifaces.empty()) {
    os << " Interface list empty " << std::endl;
    return;
  }

  for (const auto& iface : m_ifaces) {
    os << iface << "\n";
  }
  os.flush();
}

const Interface*
SimpleRouter::findIfaceByIp(uint32_t ip) const
{
  auto iface = std::find_if(m_ifaces.begin(), m_ifaces.end(), [ip] (const Interface& iface) {
      return iface.ip == ip;
    });

  if (iface == m_ifaces.end()) {
    return nullptr;
  }

  return &*iface;
}

const Interface*
SimpleRouter::findIfaceByMac(const Buffer& mac) const
{
  auto iface = std::find_if(m_ifaces.begin(), m_ifaces.end(), [mac] (const Interface& iface) {
      return iface.addr == mac;
    });

  if (iface == m_ifaces.end()) {
    return nullptr;
  }

  return &*iface;
}

void
SimpleRouter::reset(const pox::Ifaces& ports)
{
  std::cerr << "Resetting SimpleRouter with " << ports.size() << " ports" << std::endl;

  m_arp.clear();
  m_ifaces.clear();

  for (const auto& iface : ports) {
    auto ip = m_ifNameToIpMap.find(iface.name);
    if (ip == m_ifNameToIpMap.end()) {
      std::cerr << "IP_CONFIG missing information about interface `" + iface.name + "`. Skipping it" << std::endl;
      continue;
    }

    m_ifaces.insert(Interface(iface.name, iface.mac, ip->second));
  }

  printIfaces(std::cerr);
}

const Interface*
SimpleRouter::findIfaceByName(const std::string& name) const
{
  auto iface = std::find_if(m_ifaces.begin(), m_ifaces.end(), [name] (const Interface& iface) {
      return iface.name == name;
    });

  if (iface == m_ifaces.end()) {
    return nullptr;
  }

  return &*iface;
}


} // namespace simple_router {
