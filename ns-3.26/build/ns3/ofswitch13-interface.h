/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 University of Campinas (Unicamp)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Luciano Chaves <luciano@lrc.ic.unicamp.br>
 */

/**
 * \defgroup ofswitch13 OpenFlow 1.3 softswitch
 *
 * This section documents the API of the ns-3 OpenFlow 1.3 compatible switch
 * and controller interface. It follows the OpenFlow 1.3 switch specification
 * <https://www.opennetworking.org/sdn-resources/technical-library>.
 * It depends on the CPqD ofsoftswitch13 software switch available at
 * <https://github.com/ljerezchaves/ofsoftswitch13>, compiled as a library.
 * Check the manual for tutorials on how to compile and use this module.
 */
#ifndef OFSWITCH13_INTERFACE_H
#define OFSWITCH13_INTERFACE_H

#include <cassert>

#include <ns3/simulator.h>
#include <ns3/log.h>
#include <ns3/packet.h>
#include <ns3/csma-module.h>
#include <ns3/socket.h>

#include <boost/static_assert.hpp>
#include "openflow/openflow.h"

extern "C"
{
// Workaround, as ofsoftswitch13 uses these two reserved words as member names
#define private _private
#define delete _delete
#define list List

#include "udatapath/packet.h"
#include "udatapath/pipeline.h"
#include "udatapath/flow_table.h"
#include "udatapath/group_table.h"
#include "udatapath/meter_table.h"
#include "udatapath/dp_ports.h"
#include "udatapath/dp_control.h"
#include "udatapath/dp_actions.h"
#include "udatapath/dp_buffers.h"

#include "lib/ofpbuf.h"
#include "lib/vlog.h"

#include "oflib/ofl-structs.h"
#include "oflib/oxm-match.h"

#include "utilities/dpctl.h"

// From udatapath/datapath.c
struct remote* remote_create (struct datapath *dp, struct rconn *rconn,
                              struct rconn *rconn_aux);

// From udatapath/dp_ports.c
uint32_t port_speed (uint32_t conf);

#undef list
#undef private
#undef delete
}

namespace ns3 {

/**
 * \ingroup ofswitch13
 * Class used to read a single complete OpenFlow messages from an open socket.
 */
class SocketReader : public SimpleRefCount<SocketReader>
{
public:
  /**
   * Complete constructor, with the socket pointer.
   * \param socket The socket pointer.
   */
  SocketReader (Ptr<Socket> socket);

  /**
   * \param packet The packet with the received OpenFlow message.
   * \param sender The address of the sender.
   */
  typedef Callback <void, Ptr<Packet>, Address > MessageCallback;

  /**
   * Set the callback to invoke whenever an OpenFlow message has been received
   * at the associated socket.
   * \param cb The callback to invoke.
   */
  void SetReceiveCallback (MessageCallback cb);

private:
  /**
   * Socket callback used to read bytes from the socket.
   * \param socket The TCP socket.
   */
  void Read (Ptr<Socket> socket);

  Ptr<Socket>     m_socket;         //!< TCP socket.
  Ptr<Packet>     m_pendingPacket;  //!< Buffer for receiving bytes.
  uint32_t        m_pendingBytes;   //!< Pending bytes for complete message.
  MessageCallback m_receivedMsg;    //!< OpenFlow message callback.
}; // Class SocketReader

namespace ofs {

/**
 * TracedCallback signature for sending packets from CsmaNetDevice to OpenFlow
 * pipeline.
 * \attention The packet can be modified by the OpenFlow pipeline.
 * \param netdev The underlying CsmaNetDevice switch port.
 * \param packet The packet.
 */
typedef void (*OpenFlowCallback)(Ptr<Packet> packet);

/**
 * \ingroup ofswitch13
 * Enable the logging system of the ofsoftswitch13 library.
 * By default, it will configure de logging system for maximum verbose dump on
 * console. You can set the \p printToFile parameter to dump messages to file
 * instead.
 * \param printToFile Dump log messages to file instead of console.
 * \param prefix Filename prefix to use for log files.
 * \param explicitFilename Treat the prefix as an explicit filename if true.
 * \param customLevels Customize vlog levels.
 */
void EnableLibraryLog (bool printToFile = false, std::string prefix = "",
                       bool explicitFilename = false,
                       std::string customLevels = "");

/**
 * \ingroup ofswitch13
 * Create an internal ofsoftswitch13 buffer from ns3::Packet. Takes a
 * Ptr<Packet> and generates a buffer (ofpbuf*) from it, loading the packet
 * data as well as its headers into the buffer.
 * \see ofsoftswitch13 function netdev_recv () at lib/netdev.c
 * \param packet The ns-3 packet.
 * \param bodyRoom The size to allocate for data.
 * \param headRoom The size to allocate for headers (left unitialized).
 * \return The OpenFlow Buffer created from the packet.
 */
ofpbuf* BufferFromPacket (Ptr<const Packet> packet, size_t bodyRoom,
                          size_t headRoom = 0);

/**
 * \ingroup ofswitch13
 * Create a new ns3::Packet from internal OFLib message. Takes a ofl_msg_*
 * structure, pack the message using wire format and generates a Ptr<Packet>
 * from it.
 * \param msg The OFLib message structure.
 * \param xid The transaction id to use.
 * \return The ns3::Packet created.
 */
Ptr<Packet> PacketFromMsg (ofl_msg_header *msg, uint32_t xid = 0);

/**
 * \ingroup ofswitch13
 * Create a new ns3::Packet from internal ofsoftswitch13 buffer. Takes a buffer
 * (ofpbuf*) and generates a Ptr<Packet> from it, load the data as well as its
 * headers into the packet.
 * \param buffer The internal buffer.
 * \return The ns3::Packet created.
 */
Ptr<Packet> PacketFromBuffer (ofpbuf* buffer);

} // namespace ofs
} // namespace ns3
#endif /* OFSWITCH13_INTERFACE_H */
