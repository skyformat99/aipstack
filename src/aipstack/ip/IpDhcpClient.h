/*
 * Copyright (c) 2017 Ambroz Bizjak
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AIPSTACK_IP_DHCP_CLIENT_H
#define AIPSTACK_IP_DHCP_CLIENT_H

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <aipstack/misc/Use.h>
#include <aipstack/misc/Assert.h>
#include <aipstack/misc/Hints.h>
#include <aipstack/misc/MinMax.h>
#include <aipstack/misc/NonCopyable.h>
#include <aipstack/misc/OneOf.h>
#include <aipstack/misc/Function.h>
#include <aipstack/misc/MemRef.h>
#include <aipstack/infra/Buf.h>
#include <aipstack/infra/BufUtils.h>
#include <aipstack/infra/TxAllocHelper.h>
#include <aipstack/infra/SendRetry.h>
#include <aipstack/infra/Options.h>
#include <aipstack/infra/Instance.h>
#include <aipstack/proto/Ip4Proto.h>
#include <aipstack/proto/DhcpProto.h>
#include <aipstack/proto/EthernetProto.h>
#include <aipstack/ip/IpAddr.h>
#include <aipstack/ip/IpStack.h>
#include <aipstack/ip/IpHwCommon.h>
#include <aipstack/ip/IpDhcpOptions.h>
#include <aipstack/udp/IpUdpProto.h>
#include <aipstack/eth/MacAddr.h>
#include <aipstack/eth/EthHw.h>
#include <aipstack/platform/PlatformFacade.h>

namespace AIpStack {

/**
 * @defgroup dhcp-client DHCP Client
 * @brief Performs automatic interface configuration using DHCP.
 * 
 * This module provides an implementation of a DHCP client compliant with RFC 2131
 * and (as applicable) RFC 2132.
 * 
 * The DHCP client is started by creating an instance of the @ref IpDhcpClient class.
 * Once created, this objects operates the DHCP protocol and manages applicable
 * configuration of the network interface. It can optionally report significant
 * DHCP events to the user via the @ref IpDhcpClientHandler callback.
 * 
 * The DHCP client currently supports only Ethernet network interfaces; more
 * specifically, the @ref IpHwType::Ethernet hardware-type specific interface
 * must be implemented by the network interface (see @ref eth-hw).
 * 
 * @{
 */

/**
 * Type of DHCP client event as reported by @ref AIpStack::IpDhcpClientHandler
 * "IpDhcpClientHandler".
 */
enum class IpDhcpClientEvent {
    /**
     * A lease has been obtained while no lease was owned.
     * 
     * This is reported just after addresses have been assigned no lease was
     * already owned (addresses were not assigned before).
     * 
     * This event can occur in the following contexts:
     * - When a lease is obtained after discovery.
     * - When a lease is obtained after the link was re-established via the
     *   REBOOTING state.
     */
    LeaseObtained,

    /**
     * A lease has been obtained while an existing leased was owned.
     * 
     * This is reported just after addresses have been assigned when an
     * existing lease was owned (addresses were already assigned). Note that
     * the new addresses may be different from those of the old lease.
     * 
     * This event occurs when a lease is obtained in the context of the
     * RENEWING or REBINDING state.
     */
    LeaseRenewed,

    /**
     * An existing lease has been lost.
     * 
     * This is reported just after existing address assignements have been
     * removed, except when due to the link going down (in that case the
     * LinkDown event is reported).
     * 
     * This event can occur in the following contexts:
     * - The lease has timed out.
     * - A NAK was received in reponse to a request in the constext of the
     *   RENEWING or REBINDING state.
     */
    LeaseLost,

    /**
     * The link went down while a lease was obtained.
     * 
     * This is reported when the link went down while a lease was owned, just
     * after the existing address assignements have been removed.
     * 
     * Note that the DHCP client has removed the address assignments because the
     * interface may later be reattached to a different network where these
     * assignements are not valid.
     * 
     * After the link goes up again, a subsequent LeaseObtained event indicates
     * that a lease has been re-obtained, regardless of whether this was via the
     * REBOOTING state or via discovery.
     */
    LinkDown,
};

/**
 * Type of callback used to reports significant DHCP client events.
 * 
 * It is not allowed to remove the interface (and therefore also the
 * DHCP client) from within the callback.
 * 
 * @param event_type Type of DHCP event.
 */
using IpDhcpClientHandler = Function<void(IpDhcpClientEvent event_type)>;

/**
 * Initialization options for the DHCP client.
 * 
 * These are passed to the @ref IpDhcpClient::IpDhcpClient constructor.
 */
class IpDhcpClientInitOptions {
public:
    /**
     * Constructor which sets default values.
     */
    inline IpDhcpClientInitOptions ()
    : client_id(MemRef::Null()),
      vendor_class_id(MemRef::Null()),
      request_ip_address(Ip4Addr::ZeroAddr())
    {}
    
    /**
     * Client identifier, empty/null to not send.
     * 
     * If given, the pointed-to memory must be valid as long as
     * the DHCP client is initialized.
     */
    MemRef client_id;
    
    /**
     * Vendor class identifier, empty/null to not send.
     * 
     * If given, the pointed-to memory must be valid as long as
     * the DHCP client is initialized.
     */
    MemRef vendor_class_id;
    
    /**
     * Address to request, zero for none.
     * 
     * If nonzero, then initially this address will be requested
     * through the REBOOTING state.
     */
    Ip4Addr request_ip_address;
};

template<typename Arg>
class IpDhcpClient;

/**
 * DHCP client implementation.
 * 
 * @tparam Arg An instantiation of the @ref IpDhcpClientService::Compose template
 *         or a dummy class derived from such; see @ref IpDhcpClientService for an
 *         example.
 */
template<typename Arg>
class IpDhcpClient final :
    private NonCopyable<IpDhcpClient<Arg>>
#ifndef IN_DOXYGEN
    ,private IpSendRetryRequest
#endif
{
    AIPSTACK_USE_TYPES(Arg, (PlatformImpl, StackArg, Params))

    using Platform = PlatformFacade<PlatformImpl>;
    AIPSTACK_USE_TYPES(Platform, (TimeType))

    using UdpArg = typename IpStack<StackArg>::template GetProtoArg<UdpApi>;
    AIPSTACK_USE_VALS(UdpApi<UdpArg>, (HeaderBeforeUdpData, MaxUdpDataLenIp4))
    
    static_assert(Params::MaxDnsServers > 0 && Params::MaxDnsServers < 32);
    static_assert(Params::XidReuseMax >= 1 && Params::XidReuseMax <= 5);
    static_assert(Params::MaxRequests >= 1 && Params::MaxRequests <= 5);
    static_assert(Params::MaxRebootRequests >= 1 && Params::MaxRebootRequests <= 5);
    static_assert(Params::BaseRtxTimeoutSeconds >= 1 &&
                  Params::BaseRtxTimeoutSeconds <= 4, "");
    static_assert(Params::MaxRtxTimeoutSeconds >= Params::BaseRtxTimeoutSeconds &&
                  Params::MaxRtxTimeoutSeconds <= 255, "");
    static_assert(Params::ResetTimeoutSeconds >= 1 &&
                  Params::ResetTimeoutSeconds <= 128, "");
    static_assert(Params::MinRenewRtxTimeoutSeconds >= 10 &&
                  Params::MinRenewRtxTimeoutSeconds <= 255, "");
    static_assert(Params::ArpResponseTimeoutSeconds >= 1 &&
                  Params::ArpResponseTimeoutSeconds <= 5, "");
    static_assert(Params::NumArpQueries >= 1 && Params::NumArpQueries <= 10);

    // Message text to include in the DECLINE response if the address
    // was not used due to an ARP response (defined outside of class).
    inline static constexpr char DeclineMessageArpResponse[] = "ArpResponse";
    inline static constexpr std::uint8_t MaxMessageSize =
        sizeof(DeclineMessageArpResponse) - 1;
    
    // Instatiate the options class with needed configuration.
    using Options = IpDhcpOptions<
        Params::MaxDnsServers, Params::MaxClientIdSize,
        Params::MaxVendorClassIdSize, MaxMessageSize>;
    AIPSTACK_USE_TYPES(Options, (DhcpRecvOptions, DhcpSendOptions))
    
    // DHCP client states
    enum class DhcpState {
        // Link is down
        LinkDown,
        // Resetting due to NAK after some time
        Resetting,
        // Trying to request a specific IP address
        Rebooting,
        // Send discover, waiting for offer
        Selecting,
        // Sent request after offer, waiting for ack
        Requesting,
        // Checking the address is available using ARP
        Checking,
        // We have a lease, not trying to renew yet
        Bound,
        // We have a lease and we're trying to renew it
        Renewing,
        // Like Renewing but requests are broadcast
        Rebinding,
    };
    
    // Maximum future time in seconds that the timer can be set to,
    // due to limited span of TimeType. For possibly longer periods
    // (start of renewal, lease timeout), multiple timer expirations
    // are used with keeping track of leftover seconds.
    inline static constexpr std::uint32_t MaxTimerSeconds = MinValueU(
        TypeMax<std::uint32_t>,
        Platform::WorkingTimeSpanTicks / TimeType(Platform::TimeFreq));
    
    static_assert(MaxTimerSeconds >= 255);
    
    // Determines the default renewal time if the server did not specify it.
    static constexpr std::uint32_t DefaultRenewTimeForLeaseTime (std::uint32_t lease_time_s)
    {
        return lease_time_s / 2;
    }
    
    // Determines the default rebinding time if the server did not specify it.
    static constexpr std::uint32_t DefaultRebindingTimeForLeaseTime (
        std::uint32_t lease_time_s)
    {
        return std::uint32_t(std::uint64_t(lease_time_s) * 7 / 8);
    }
    
    // Maximum UDP data size that we could possibly transmit.
    inline static constexpr std::size_t MaxDhcpSendMsgSize =
        DhcpHeaderSize + Options::MaxOptionsSendSize;

    static_assert(MaxDhcpSendMsgSize <= MaxUdpDataLenIp4);
    
public:
    /**
     * Encapsulates information about the current lease.
     * 
     * A reference to this type of structure if returned by
     * @ref getLeaseInfoMustHaveLease.
     */
    struct LeaseInfo {
        // These two are set already when the offer is received.
        Ip4Addr ip_address; // in LinkDown defines the address to reboot with or none
        std::uint32_t dhcp_server_identifier;
        
        // The rest are set when the ack is received.
        Ip4Addr dhcp_server_addr;
        std::uint32_t lease_time_s;
        std::uint32_t renewal_time_s;
        std::uint32_t rebinding_time_s;
        Ip4Addr subnet_mask;
        MacAddr server_mac;
        bool have_router;
        std::uint8_t domain_name_servers_count;
        Ip4Addr router;
        Ip4Addr domain_name_servers[Params::MaxDnsServers];
    };
    
private:
    typename Platform::Timer m_timer;
    IpIfaceStateObserver<StackArg> m_iface_observer;
    EthArpObserver m_arp_observer;
    UdpListener<UdpArg> m_udp_listener;
    IpStack<StackArg> *m_ipstack;
    IpIface<StackArg> *m_iface;
    IpDhcpClientHandler m_handler;
    MemRef m_client_id;
    MemRef m_vendor_class_id;
    std::uint32_t m_xid;
    std::uint8_t m_rtx_timeout;
    DhcpState m_state;
    std::uint8_t m_request_count;
    std::uint32_t m_lease_time_passed;
    TimeType m_request_send_time;
    std::uint32_t m_request_send_time_passed;
    LeaseInfo m_info;
    
public:
    /**
     * Construct the DHCP client.
     * 
     * The interface must exist as long as the DHCP client exists.
     * The DHCP client assumes that it has exclusive control over the
     * IP address and gateway address assignement for the interface and
     * that both of these are initially unassigned.
     * 
     * @param platform_ The platform facade, should be the same as passed to
     *        the @ref IpStack::IpStack constructor.
     * @param stack The IP stack.
     * @param iface The interface to run on. It must be an Ethernet based interface
     *              and support the @ref IpHwType::Ethernet hardware-type
     *              specific interface (see @ref eth-hw).
     * @param opts Initialization options. This structure itself is copied but
     *             any referenced memory is not.
     * @param handler Callback used to report significant events (may be null).
     */
    IpDhcpClient (PlatformFacade<PlatformImpl> platform_, IpStack<StackArg> *stack,
                  IpIface<StackArg> *iface, IpDhcpClientInitOptions const &opts,
                  IpDhcpClientHandler handler)
    :
        m_timer(platform_, AIPSTACK_BIND_MEMBER_TN(&IpDhcpClient::timerHandler, this)),
        m_iface_observer(AIPSTACK_BIND_MEMBER_TN(&IpDhcpClient::ifaceStateChanged, this)),
        m_arp_observer(AIPSTACK_BIND_MEMBER_TN(&IpDhcpClient::arpInfoReceived, this)),
        m_udp_listener(AIPSTACK_BIND_MEMBER_TN(&IpDhcpClient::udpIp4PacketReceived, this)),
        m_ipstack(stack),
        m_iface(iface),
        m_handler(handler),
        m_client_id(opts.client_id),
        m_vendor_class_id(opts.vendor_class_id)
    {
        // We only support Ethernet interfaces.
        AIPSTACK_ASSERT(iface->getHwType() == IpHwType::Ethernet);
        
        // Start listening for incoming DHCP UDP packets.
        UdpListenParams<UdpArg> listen_params;
        listen_params.port = DhcpClientPort;
        listen_params.accept_broadcast = true;
        listen_params.accept_nonlocal_dst = true;
        listen_params.iface = iface;
        m_udp_listener.startListening(udp(), listen_params);

        // Start observing interface state.
        m_iface_observer.observe(*iface);
        
        // Remember any requested IP address for Rebooting.
        m_info.ip_address = opts.request_ip_address;
        
        if (iface->getDriverState().link_up) {
            // Start discovery/rebooting.
            start_discovery_or_rebooting();
        } else {
            // Remain inactive until the link is up.
            m_state = DhcpState::LinkDown;
        }
    }
    
    /**
     * Destruct the DHCP client.
     * 
     * This will remove any IP address or gateway address assignment
     * from the interface.
     */
    ~IpDhcpClient ()
    {
        // Remove any configuration that might have been done (no callback).
        handle_dhcp_down(/*call_callback=*/false, /*link_down=*/false);
    }
    
    /**
     * Check if a IP address lease is currently active.
     * 
     * @return True if a lease is active, false if not.
     */
    inline bool hasLease () const
    {
        return m_state == OneOf(DhcpState::Bound, DhcpState::Renewing,
                                DhcpState::Rebinding);
    }
    
    /**
     * Get information about the current IP address lease.
     * 
     * This may only be called when a lease is active (@ref hasLease
     * returns true).
     * 
     * @return Reference to lease information (valid only temporarily).
     */
    inline LeaseInfo const & getLeaseInfoMustHaveLease () const
    {
        AIPSTACK_ASSERT(hasLease());
        
        return m_info;
    }
    
private:
    inline PlatformFacade<PlatformImpl> platform () const
    {
        return m_timer.platform();
    }

    inline IpIface<StackArg> * iface () const
    {
        return m_iface;
    }
    
    // Return the EthHwIface interface for the interface.
    inline EthHwIface * ethHw () const
    {
        return iface()->template getHwIface<EthHwIface>();
    }

    // Get the UDP protocol API pointer.
    inline UdpApi<UdpArg> & udp () const
    {
        return m_ipstack->template getProtoApi<UdpApi>();
    }
    
    // Convert seconds to ticks, requires seconds <= MaxTimerSeconds.
    inline static TimeType SecToTicks (std::uint32_t seconds)
    {
        AIPSTACK_ASSERT(seconds <= MaxTimerSeconds);
        return SecToTicksNoAssert(seconds);
    }
    
    // Same but without assert that seconds <= MaxTimerSeconds.
    inline static TimeType SecToTicksNoAssert (std::uint32_t seconds)
    {
        return seconds * TimeType(Platform::TimeFreq);
    }
    
    // Convert ticks to seconds, rounding down.
    inline static std::uint32_t TicksToSec (TimeType ticks)
    {
        TimeType sec_timetype = ticks / TimeType(Platform::TimeFreq);
        return MinValueU(sec_timetype, TypeMax<std::uint32_t>);
    }
    
    // Shortcut to last timer set time.
    inline TimeType getTimSetTime ()
    {
        return m_timer.getSetTime();
    }
    
    // Set m_rtx_timeout to BaseRtxTimeoutSeconds.
    void reset_rtx_timeout ()
    {
        m_rtx_timeout = Params::BaseRtxTimeoutSeconds;
    }
    
    // Double m_rtx_timeout, but to no more than MaxRtxTimeoutSeconds.
    void double_rtx_timeout ()
    {
        m_rtx_timeout = (m_rtx_timeout > Params::MaxRtxTimeoutSeconds / 2) ?
            Params::MaxRtxTimeoutSeconds : (2 * m_rtx_timeout);
    }
    
    // Set the timer to expire after m_rtx_timeout.
    void set_timer_for_rtx ()
    {
        m_timer.setAfter(SecToTicks(m_rtx_timeout));
    }
    
    // Start discovery process.
    void start_discovery_or_rebooting ()
    {
        // Generate an XID.
        new_xid();
        
        // Initialize the counter of discover/request messages.
        m_request_count = 1;
        
        if (m_info.ip_address.isZero()) {
            // Going to Selecting state.
            m_state = DhcpState::Selecting;
            
            // Send discover.
            send_discover();
        } else {
            // Go to Rebooting state.
            m_state = DhcpState::Rebooting;
            
            // Remember when the first request was sent.
            m_request_send_time = platform().getTime();
            
            // Send request.
            send_request();
        }
        
        // Set the timer for retransmission (or reverting from Rebooting to discovery).
        reset_rtx_timeout();
        set_timer_for_rtx();
    }
    
    // Start discovery (never rebooting).
    void start_discovery ()
    {
        // Clear ip_address to prevent Rebooting.
        m_info.ip_address = Ip4Addr::ZeroAddr();
        
        // Delegate to start_discovery_or_rebooting.
        start_discovery_or_rebooting();
    }
    
    void handle_expired_lease (bool had_lease)
    {
        // Start discovery.
        start_discovery();
        
        // If we had a lease, remove any IP configuration etc..
        if (had_lease) {
            return handle_dhcp_down(/*call_callback=*/true, /*link_down=*/false);
        }
    }
    
    void timerHandler ()
    {
        switch (m_state) {
            case DhcpState::Resetting:
                return handleTimerResetting();
            case DhcpState::Selecting:
                return handleTimerSelecting();
            case DhcpState::Rebooting:
            case DhcpState::Requesting:
                return handleTimerRebootingRequesting();
            case DhcpState::Checking:
                return handleTimerChecking();
            case DhcpState::Bound:
            case DhcpState::Renewing:
            case DhcpState::Rebinding:
                return handleTimerBoundRenewingRebinding();
            default:
                AIPSTACK_ASSERT(false);
        }
    }
    
    void handleTimerResetting ()
    {
        // Timer was set for restarting discovery.
        
        start_discovery();
    }
    
    void handleTimerSelecting ()
    {
        // Timer was set for retransmitting discover.
        
        // Update request count, generate new XID if needed.
        if (m_request_count >= Params::XidReuseMax) {
            m_request_count = 1;
            new_xid();
        } else {
            m_request_count++;
        }
        
        // Send discover.
        send_discover();
        
        // Set the timer for another retransmission.
        double_rtx_timeout();
        set_timer_for_rtx();
    }
    
    void handleTimerRebootingRequesting ()
    {
        // Timer was set for retransmitting request.
        
        // If we sent enough requests, start discovery.
        auto limit = (m_state == DhcpState::Rebooting) ?
            Params::MaxRebootRequests : Params::MaxRequests;
        if (m_request_count >= limit) {
            start_discovery();
            return;
        }
        
        // Increment request count.
        m_request_count++;
        
        // NOTE: We do not update m_request_send_time, it remains set
        // to when the first request was sent. This is so that that times
        // for renewing, rebinding and lease timeout will be relative to
        // when the first request was sent.
        
        // Send request.
        send_request();
        
        // Restart timer with doubled retransmission timeout.
        double_rtx_timeout();
        set_timer_for_rtx();
    }
    
    void handleTimerChecking ()
    {
        // Timer was set to continue after no response to ARP query.
        
        if (m_request_count < Params::NumArpQueries) {
            // Increment the ARP query counter.
            m_request_count++;
            
            // Start the timeout.
            m_timer.setAfter(SecToTicks(Params::ArpResponseTimeoutSeconds));
            
            // Send an ARP query.
            ethHw()->sendArpQuery(m_info.ip_address);
        } else {
            // Unsubscribe from ARP updates.
            m_arp_observer.reset();
            
            // Bind the lease.
            return go_bound();
        }
    }
    
    void handleTimerBoundRenewingRebinding ()
    {
        // Timer was set for:
        // - Bound: transition to Renewing
        // - Renewing: retransmitting a request or transition to Rebinding
        // - Rebinding: retransmitting a request or lease timeout
        // Or it might have been set for earlier if that was too far in the
        // future. We anyway check how much time has actually passed and we
        // may also skip one or more states if more has passed than expected.
        
        AIPSTACK_ASSERT(m_lease_time_passed <= m_info.lease_time_s);
        
        TimeType now = platform().getTime();
        
        // Calculate how much time in seconds has passed since the time this timer
        // was set to expire at.
        std::uint32_t passed_sec = TicksToSec(TimeType(now - getTimSetTime()));
        
        // Has the lease expired?
        if (passed_sec >= m_info.lease_time_s - m_lease_time_passed) {
            return handle_expired_lease(/*had_lease=*/true);
        }
        
        // Remember m_lease_time_passed (needed for seting the next timer).
        std::uint32_t prev_lease_time_passed = m_lease_time_passed;
        
        // Update m_lease_time_passed according to time passed so far.
        m_lease_time_passed += passed_sec;
        
        // Has the rebinding time expired?
        if (m_state != DhcpState::Rebinding &&
            m_lease_time_passed >= m_info.rebinding_time_s)
        {
            // Go to state Rebinding, generate XID.
            m_state = DhcpState::Rebinding;
            new_xid();
        }
        // Has the renewal time expired?
        else if (m_state == DhcpState::Bound &&
                 m_lease_time_passed >= m_info.renewal_time_s)
        {
            // Go to state Renewing, generate XID.
            m_state = DhcpState::Renewing;
            new_xid();
        }
        
        // We will choose after how many seconds the timer should next
        // expire, relative to the current m_lease_time_passed.
        std::uint32_t timer_rel_sec;
        
        if (m_state == DhcpState::Bound) {
            // Timer should expire at the renewal time.
            timer_rel_sec = m_info.renewal_time_s - m_lease_time_passed;
        } else {
            // Time to next state transition (Rebinding or lease timeout).
            std::uint32_t next_state_sec = (m_state == DhcpState::Renewing) ?
                m_info.rebinding_time_s : m_info.lease_time_s;
            std::uint32_t next_state_rel_sec = next_state_sec - m_lease_time_passed;
            
            // Time to next retransmission.
            // NOTE: Retransmission may actually be done earlier if this is
            // greater than MaxTimerSeconds, that is all right.
            std::uint32_t rtx_rel_sec = MaxValue(
                std::uint32_t(Params::MinRenewRtxTimeoutSeconds),
                std::uint32_t(next_state_rel_sec / 2));
            
            // Timer should expire at the earlier of the above two.
            timer_rel_sec = MinValue(next_state_rel_sec, rtx_rel_sec);
            
            // Send a request.
            send_request();
            
            // Remember the time when the request was sent including the
            // m_lease_time_passed corresponding to this.
            m_request_send_time = now;
            m_request_send_time_passed = m_lease_time_passed;
        }
        
        // Limit to how far into the future the timer can be set.
        timer_rel_sec = MinValue(timer_rel_sec, MaxTimerSeconds);
        
        // Set the timer and update m_lease_time_passed as deciced above. Note that
        // we need to account for the extra time passed by which m_lease_time_passed
        // was incremented at the top.
        m_lease_time_passed += timer_rel_sec;
        TimeType timer_time = getTimSetTime() +
            SecToTicksNoAssert(m_lease_time_passed - prev_lease_time_passed);
        m_timer.setAt(timer_time);
    }
    
    void retrySending () override final
    {
        // Retry sending a message after a send error, probably due to ARP cache miss.
        // To be complete we support retrying for all message types even broadcasts.
        
        // Note that send_dhcp_message calls IpSendRetryRequest::reset before
        // trying to send a message. This is enough to avoid spurious retransmissions,
        // because entry to all states which we handle here involves send_dhcp_message,
        // and we ignore this callback in other states.
        
        if (m_state == DhcpState::Selecting) {
            send_discover();
        }
        else if (m_state == OneOf(DhcpState::Requesting, DhcpState::Renewing,
                                  DhcpState::Rebinding, DhcpState::Rebooting)) {
            send_request();
        }
    }
    
    UdpRecvResult udpIp4PacketReceived (
        IpRxInfoIp4<StackArg> const &ip_info, UdpRxInfo<UdpArg> const &udp_info,
        IpBufRef udp_data)
    {
        // Check for expected source port.
        if (AIPSTACK_UNLIKELY(udp_info.src_port != DhcpServerPort)) {
            goto out;
        }
        
        // Sanity check source address - reject broadcast addresses.
        if (AIPSTACK_UNLIKELY(!IpStack<StackArg>::checkUnicastSrcAddr(ip_info))) {
            goto out;
        }

        // Process the DHCP message.
        processReceivedDhcpMessage(ip_info.src_addr, udp_data);
        
    out:
        // Accept the packet, inhibit further processing.
        return UdpRecvResult::AcceptStop;
    }

    void processReceivedDhcpMessage (Ip4Addr src_addr, IpBufRef msg)
    {
        // In these states we're not interested in any messages.
        if (m_state == OneOf(DhcpState::LinkDown, DhcpState::Resetting,
                             DhcpState::Checking, DhcpState::Bound))
        {
            return;
        }
        
        // Check that there is a DHCP header and that the first portion is contiguous.
        if (msg.tot_len < DhcpHeaderSize || !msg.hasHeader(DhcpHeader1::Size)) {
            return;
        }
        
        // Reference the first header part.
        auto dhcp_header1 = DhcpHeader1::MakeRef(msg.getChunkPtr());
        
        // Simple checks before further processing.
        // Note that we check that the XID matches the expected one here.
        bool sane =
            dhcp_header1.get(DhcpHeader1::DhcpOp())    == DhcpOp::BootReply &&
            dhcp_header1.get(DhcpHeader1::DhcpHtype()) == DhcpHwAddrType::Ethernet &&
            dhcp_header1.get(DhcpHeader1::DhcpHlen())  == MacAddr::Size &&
            dhcp_header1.get(DhcpHeader1::DhcpXid())   == m_xid &&
            MacAddr::readBinary(dhcp_header1.ref(DhcpHeader1::DhcpChaddr())) ==
                ethHw()->getMacAddr();
        if (!sane) {
            return;
        }
        
        // Skip the first header part.
        IpBufRef data = msg.hideHeader(DhcpHeader1::Size);
        
        // Get and skip the middle header part (sname and file).
        IpBufRef dhcp_header2 = data.subTo(DhcpHeader2::Size);
        data = ipBufSkipBytes(data, DhcpHeader2::Size);
        
        // Read and skip the final header part (magic number).
        DhcpHeader3::Val dhcp_header3;
        data = ipBufTakeBytes(data, DhcpHeader3::Size, dhcp_header3.data);
        
        // Check the magic number.
        if (dhcp_header3.get(DhcpHeader3::DhcpMagic()) != DhcpMagicField::Magic) {
            return;
        }
        
        // Parse DHCP options.
        DhcpRecvOptions opts;
        if (!Options::parseOptions(dhcp_header2, data, opts)) {
            return;
        }
        
        // Sanity check DHCP message type.
        if (!opts.have.dhcp_message_type || opts.dhcp_message_type !=
            OneOf(DhcpMessageType::Offer, DhcpMessageType::Ack, DhcpMessageType::Nak))
        {
            return;
        }
        
        // Check that there is a DHCP server identifier.
        if (!opts.have.dhcp_server_identifier) {
            return;
        }
        
        // Handle NAK message.
        if (opts.dhcp_message_type == DhcpMessageType::Nak) {
            // A NAK is only valid in states where we are expecting a reply to a request.
            if (m_state != OneOf(DhcpState::Requesting, DhcpState::Renewing,
                                 DhcpState::Rebinding, DhcpState::Rebooting)) {
                return;
            }
            
            // In Requesting state, verify the DHCP server identifier.
            if (m_state == DhcpState::Requesting) {
                if (opts.dhcp_server_identifier != m_info.dhcp_server_identifier) {
                    return;
                }
            }
            
            // Restart discovery. If in Requesting we go via Resetting state so that
            // a discover will be sent only after a delay. This prevents a tight loop
            // of discover-offer-request-NAK.
            bool discover_immediately = (m_state != DhcpState::Requesting);
            return go_resetting(discover_immediately);
            // Nothing else to do (further processing is for offer and ack).
        }
        
        // Get Your IP Address.
        Ip4Addr ip_address = dhcp_header1.get(DhcpHeader1::DhcpYiaddr());
        
        // Handle received offer in Selecting state.
        if (opts.dhcp_message_type == DhcpMessageType::Offer &&
            m_state == DhcpState::Selecting)
        {
            // Sanity check offer.
            if (!checkOffer(ip_address)) {
                return;
            }
            
            // Remember offer.
            m_info.ip_address = ip_address;
            m_info.dhcp_server_identifier = opts.dhcp_server_identifier;
            
            // Going to state Requesting.
            m_state = DhcpState::Requesting;
            
            // Leave existing XID because the request must use the XID of
            // the offer (which m_xid already is due to the check earlier).
            
            // Remember when the first request was sent.
            m_request_send_time = platform().getTime();
            
            // Send request.
            send_request();
            
            // Initialize the request count.
            m_request_count = 1;
            
            // Start timer for retransmitting request or reverting to discovery.
            reset_rtx_timeout();
            set_timer_for_rtx();
        }
        // Handle received ACK in Requesting/Renewing/Rebinding/Rebooting state.
        else if (opts.dhcp_message_type == DhcpMessageType::Ack &&
                 m_state == OneOf(DhcpState::Requesting, DhcpState::Renewing,
                                  DhcpState::Rebinding, DhcpState::Rebooting))
        {
            // Sanity check and fixup lease information.
            if (!checkAndFixupAck(ip_address, opts)) {
                return;
            }
            
            if (m_state == DhcpState::Requesting) {
                // In Requesting state, sanity check against the offer.
                if (ip_address != m_info.ip_address ||
                    opts.dhcp_server_identifier != m_info.dhcp_server_identifier)
                {
                    return;
                }
            }
            else if (m_state != DhcpState::Rebooting) {
                // In Renewing/Rebinding, check that not too much time has passed
                // that would make m_request_send_time invalid.
                // This check effectively means that the timer is still set for the
                // first expiration as set in request_in_renewing_or_rebinding and
                // not for a subsequent expiration due to needing a large delay.
                AIPSTACK_ASSERT(m_lease_time_passed >= m_request_send_time_passed);
                if (m_lease_time_passed - m_request_send_time_passed > MaxTimerSeconds) {
                    // Ignore the ACK. This should not be a problem because
                    // an ACK really should not arrive that long (MaxTimerSeconds)
                    // after a request was sent.
                    return;
                }
            }
            
            // Remember/update the lease information.
            m_info.ip_address = ip_address;
            m_info.dhcp_server_identifier = opts.dhcp_server_identifier;
            m_info.dhcp_server_addr = src_addr;
            m_info.lease_time_s = opts.ip_address_lease_time;
            m_info.renewal_time_s = opts.renewal_time;
            m_info.rebinding_time_s = opts.rebinding_time;
            m_info.subnet_mask = opts.subnet_mask;
            m_info.have_router = opts.have.router;
            m_info.router = opts.have.router ? opts.router : Ip4Addr::ZeroAddr();
            m_info.domain_name_servers_count = opts.have.dns_servers;
            std::memcpy(m_info.domain_name_servers, opts.dns_servers,
                     opts.have.dns_servers * sizeof(Ip4Addr));
            m_info.server_mac = ethHw()->getRxEthHeader().get(EthHeader::SrcMac());
            
            if (m_state == DhcpState::Requesting) {
                // In Requesting state, we need to do the ARP check first.
                go_checking();
            } else {
                // Bind the lease.
                return go_bound();
            }
        }
    }
    
    void ifaceStateChanged ()
    {
        IpIfaceDriverState driver_state = iface()->getDriverState();
        
        if (m_state == DhcpState::LinkDown) {
            // If the link is now up, start discovery/rebooting.
            if (driver_state.link_up) {
                start_discovery_or_rebooting();
            }
        } else {
            // If the link is no longer up, revert everything.
            if (!driver_state.link_up) {
                bool had_lease = hasLease();
                
                // Prevant later requesting the m_info.ip_address via the REBOOTING
                // state if it is not actually assigned or being requested via the
                // REBOOTING state.
                if (!(had_lease || m_state == DhcpState::Rebooting)) {
                    m_info.ip_address = Ip4Addr::ZeroAddr();
                }
                
                // Go to state LinkDown.
                m_state = DhcpState::LinkDown;
                
                // Reset resources to prevent undesired callbacks.
                m_arp_observer.reset();
                IpSendRetryRequest::reset();
                m_timer.unset();
                
                // If we had a lease, unbind and notify user.
                if (had_lease) {
                    return handle_dhcp_down(/*call_callback=*/true, /*link_down=*/true);
                }
            }
        }
    }
    
    void arpInfoReceived (Ip4Addr ip_addr, [[maybe_unused]] MacAddr mac_addr)
    {
        AIPSTACK_ASSERT(m_state == DhcpState::Checking);
        
        // Is this an ARP message from the IP address we are checking?
        if (ip_addr == m_info.ip_address) {
            // Send a Decline.
            send_decline();
            
            // Unsubscribe from ARP updates.
            m_arp_observer.reset();
            
            // Restart via Resetting state after a timeout.
            return go_resetting(false);
        }
    }
    
    // Do some sanity check of the offered IP address.
    static bool checkOffer (Ip4Addr addr)
    {
        // Check that it's not all zeros or all ones.
        if (addr.isZero() || addr.isAllOnes()) {
            return false;
        }
        
        // Check that it's not a loopback address.
        if ((addr & Ip4Addr::PrefixMask<8>()) == Ip4Addr(127, 0, 0, 0)) {
            return false;
        }
        
        // Check that that it's not a multicast address.
        if ((addr & Ip4Addr::PrefixMask<4>()) == Ip4Addr(224, 0, 0, 0)) {
            return false;
        }
        
        return true;
    }
    
    // Checks received address information in an Ack.
    // This may modify certain fields in the opts that are considered
    // invalid but not fatal, or fill in missing fields.
    static bool checkAndFixupAck (Ip4Addr addr, DhcpRecvOptions &opts)
    {
        // Do the basic checks that apply to offers.
        if (!checkOffer(addr)) {
            return false;
        }
        
        // Check that we have an IP Address lease time.
        if (!opts.have.ip_address_lease_time) {
            return false;
        }
        
        // If there is no subnet mask, choose one based on the address class.
        if (!opts.have.subnet_mask) {
            if (addr < Ip4Addr(128, 0, 0, 0)) {
                // Class A.
                opts.subnet_mask = Ip4Addr(255, 0, 0, 0);
            }
            else if (addr < Ip4Addr(192, 0, 0, 0)) {
                // Class C.
                opts.subnet_mask = Ip4Addr(255, 255, 0, 0);
            }
            else if (addr < Ip4Addr(224, 0, 0, 0)) {
                // Class D.
                opts.subnet_mask = Ip4Addr(255, 255, 255, 0);
            }
            else {
                // Class D or E, considered invalid.
                return false;
            }
        }
        
        // Check that the subnet mask is sane.
        if (opts.subnet_mask != Ip4Addr::PrefixMask(opts.subnet_mask.countLeadingOnes())) {
            return false;
        }
        
        // Check that it's not the local broadcast address.
        Ip4Addr local_bcast = Ip4Addr::Join(opts.subnet_mask, addr, Ip4Addr::AllOnesAddr());
        if (addr == local_bcast) {
            return false;
        }
        
        // If there is a router, check that it is within the subnet.
        if (opts.have.router) {
            if ((opts.router & opts.subnet_mask) != (addr & opts.subnet_mask)) {
                // Ignore bad router.
                opts.have.router = false;
            }
        }
        
        // If there is no renewal time, assume a default.
        if (!opts.have.renewal_time) {
            opts.renewal_time = DefaultRenewTimeForLeaseTime(opts.ip_address_lease_time);
        }
        // Make sure the renewal time does not exceed the lease time.
        opts.renewal_time =
            MinValue(opts.ip_address_lease_time, opts.renewal_time);
        
        // If there is no rebinding time, assume a default.
        if (!opts.have.rebinding_time) {
            opts.rebinding_time =
                DefaultRebindingTimeForLeaseTime(opts.ip_address_lease_time);
        }
        // Make sure the rebinding time is between the renewal time and the lease time.
        opts.rebinding_time =
            MaxValue(opts.renewal_time,
            MinValue(opts.ip_address_lease_time, opts.rebinding_time));
        
        return true;
    }
    
    void go_resetting (bool discover_immediately)
    {
        bool had_lease = hasLease();
        
        if (discover_immediately) {
            // Go directly to Selecting state without delay.
            start_discovery();
        } else {
            // Going to Resetting state.
            m_state = DhcpState::Resetting;
            
            // Set timeout to start discovery.
            m_timer.setAfter(SecToTicks(Params::ResetTimeoutSeconds));
        }
        
        // If we had a lease, remove it.
        if (had_lease) {
            return handle_dhcp_down(/*call_callback=*/true, /*link_down=*/false);
        }
    }
    
    void go_checking ()
    {
        // Go to state Checking.
        m_state = DhcpState::Checking;
        
        // Initialize counter of ARP queries.
        m_request_count = 1;
        
        // Subscribe to receive ARP updates.
        // NOTE: This must not be called if already registered,
        // so we reset it when we no longer need it.
        m_arp_observer.observe(*ethHw());
        
        // Start the timeout.
        m_timer.setAfter(SecToTicks(Params::ArpResponseTimeoutSeconds));
        
        // Send an ARP query.
        ethHw()->sendArpQuery(m_info.ip_address);
    }
    
    void go_bound ()
    {
        AIPSTACK_ASSERT(m_state == OneOf(DhcpState::Checking,
            DhcpState::Renewing, DhcpState::Rebinding, DhcpState::Rebooting));
        
        bool had_lease = hasLease();
        TimeType now = platform().getTime();
        
        // Calculate how much time in seconds has passed since the request was sent
        // and set m_lease_time_passed accordingly. There is no need to limit to this
        // to lease_time_s since we check that just below.
        m_lease_time_passed = TicksToSec(TimeType(now - m_request_send_time));
        
        // Has the lease expired already?
        if (m_lease_time_passed >= m_info.lease_time_s) {
            return handle_expired_lease(had_lease);
        }
        
        // Going to state Bound.
        // It is not necessary to check if we already need to go to Renewing
        // or Rebinding because if so the timer will take care of it.
        m_state = DhcpState::Bound;
        
        // Timer should expire at the renewal time.
        std::uint32_t timer_rel_sec;
        if (m_lease_time_passed <= m_info.renewal_time_s) {
            timer_rel_sec = m_info.renewal_time_s - m_lease_time_passed;
        } else {
            timer_rel_sec = 0;
        }
        
        // Limit to how far into the future the timer can be set.
        timer_rel_sec = MinValue(timer_rel_sec, MaxTimerSeconds);
        
        // Set the timer and update m_lease_time_passed to reflect the time
        // that the timer is being set for.
        m_lease_time_passed += timer_rel_sec;
        TimeType timer_time =
            m_request_send_time + SecToTicksNoAssert(m_lease_time_passed);
        m_timer.setAt(timer_time);
        
        // Apply IP configuration etc..
        return handle_dhcp_up(had_lease);
    }
    
    void handle_dhcp_up (bool renewed)
    {
        // Set IP address with prefix length.
        std::uint8_t prefix = std::uint8_t(m_info.subnet_mask.countLeadingOnes());
        iface()->setIp4Addr(IpIfaceIp4AddrSetting(prefix, m_info.ip_address));
        
        // Set gateway (or clear if none).
        auto gateway = m_info.have_router ?
            IpIfaceIp4GatewaySetting(m_info.router) : IpIfaceIp4GatewaySetting();
        iface()->setIp4Gateway(gateway);
        
        // Call the callback if specified.
        if (m_handler) {
            auto event_type = renewed ?
                IpDhcpClientEvent::LeaseRenewed : IpDhcpClientEvent::LeaseObtained;
            return m_handler(event_type);
        }
    }
    
    void handle_dhcp_down (bool call_callback, bool link_down)
    {
        // Remove gateway.
        iface()->setIp4Gateway(IpIfaceIp4GatewaySetting());
        
        // Remove IP address.
        iface()->setIp4Addr(IpIfaceIp4AddrSetting());
        
        // Call the callback if desired and specified.
        if (call_callback && m_handler) {
            auto event_type = link_down ?
                IpDhcpClientEvent::LinkDown : IpDhcpClientEvent::LeaseLost;
            return m_handler(event_type);
        }
    }
    
    // Send a DHCP discover message.
    void send_discover ()
    {
        AIPSTACK_ASSERT(m_state == DhcpState::Selecting);
        
        DhcpSendOptions send_opts;
        send_dhcp_message(DhcpMessageType::Discover, send_opts,
                          Ip4Addr::ZeroAddr(), Ip4Addr::AllOnesAddr());
    }
    
    // Send a DHCP request message.
    void send_request ()
    {
        AIPSTACK_ASSERT(m_state == OneOf(DhcpState::Requesting,
            DhcpState::Renewing, DhcpState::Rebinding, DhcpState::Rebooting));
        
        DhcpSendOptions send_opts;
        Ip4Addr ciaddr = Ip4Addr::ZeroAddr();
        Ip4Addr dst_addr = Ip4Addr::AllOnesAddr();
        
        if (m_state == DhcpState::Requesting) {
            send_opts.have.dhcp_server_identifier = true;
            send_opts.dhcp_server_identifier = m_info.dhcp_server_identifier;
        }
        
        if (m_state == DhcpState::Renewing) {
            dst_addr = m_info.dhcp_server_addr;
        }
        
        if (m_state == OneOf(DhcpState::Requesting, DhcpState::Rebooting)) {
            send_opts.have.requested_ip_address = true;
            send_opts.requested_ip_address = m_info.ip_address;
        } else {
            ciaddr = m_info.ip_address;
        }
        
        send_dhcp_message(DhcpMessageType::Request, send_opts, ciaddr, dst_addr);
    }
    
    void send_decline ()
    {
        AIPSTACK_ASSERT(m_state == DhcpState::Checking);
        
        DhcpSendOptions send_opts;
        
        send_opts.have.dhcp_server_identifier = true;
        send_opts.dhcp_server_identifier = m_info.dhcp_server_identifier;
        
        send_opts.have.requested_ip_address = true;
        send_opts.requested_ip_address = m_info.ip_address;
        
        send_opts.have.message = true;
        send_opts.message = DeclineMessageArpResponse;
        
        send_dhcp_message(DhcpMessageType::Decline, send_opts,
                          Ip4Addr::ZeroAddr(), Ip4Addr::AllOnesAddr());
    }
    
    // Send a DHCP message.
    void send_dhcp_message (DhcpMessageType msg_type, DhcpSendOptions &opts,
                            Ip4Addr ciaddr, Ip4Addr dst_addr)
    {
        // Reset send-retry (not interested in retrying sending previous messages).
        IpSendRetryRequest::reset();
        
        // Add client identifier if configured.
        if (m_client_id.len > 0) {
            opts.have.client_identifier = true;
            opts.client_identifier = m_client_id;
        }
        
        // Add vendor class identifier if configured and not for Decline.
        if (m_vendor_class_id.len > 0 && msg_type != DhcpMessageType::Decline) {
            opts.have.vendor_class_identifier = true;
            opts.vendor_class_identifier = m_vendor_class_id;
        }
        
        // Max DHCP message size and parameter request list are present for
        // all messages except Decline.
        if (msg_type != DhcpMessageType::Decline) {
            opts.have.max_dhcp_message_size = true;
            opts.have.parameter_request_list = true;
        }
        
        // Get a buffer for the message.
        using AllocHelperType = TxAllocHelper<MaxDhcpSendMsgSize, HeaderBeforeUdpData>;
        AllocHelperType dgram_alloc(MaxDhcpSendMsgSize);
        
        // Write the DHCP header.
        auto dhcp_header1 = DhcpHeader1::MakeRef(dgram_alloc.getPtr());
        std::memset(dhcp_header1.data, 0, DhcpHeaderSize); // zero entire DHCP header
        dhcp_header1.set(DhcpHeader1::DhcpOp(),     DhcpOp::BootRequest);
        dhcp_header1.set(DhcpHeader1::DhcpHtype(),  DhcpHwAddrType::Ethernet);
        dhcp_header1.set(DhcpHeader1::DhcpHlen(),   MacAddr::Size);
        dhcp_header1.set(DhcpHeader1::DhcpXid(),    m_xid);
        dhcp_header1.set(DhcpHeader1::DhcpCiaddr(), ciaddr);
        ethHw()->getMacAddr().writeBinary(dhcp_header1.ref(DhcpHeader1::DhcpChaddr()));
        auto dhcp_header3 = DhcpHeader3::MakeRef(
            dhcp_header1.data + DhcpHeader1::Size + DhcpHeader2::Size);
        dhcp_header3.set(DhcpHeader3::DhcpMagic(),  DhcpMagicField::Magic);
        
        // Write the DHCP options.
        char *opt_startptr = dgram_alloc.getPtr() + DhcpHeaderSize;
        char *opt_endptr =
            Options::writeOptions(opt_startptr, msg_type, iface()->getMtu(), opts);
        
        // Calculate the UDP data length.
        std::size_t data_len = std::size_t(opt_endptr - dgram_alloc.getPtr());
        AIPSTACK_ASSERT(data_len <= MaxDhcpSendMsgSize);
        
        // Construct the UDP data reference.
        dgram_alloc.changeSize(data_len);
        IpBufRef udp_data = dgram_alloc.getBufRef();

        // Determine addresses and send flags. When sending from zero address, we need
        // IpSendFlags::AllowNonLocalSrc for that to be allowed.
        Ip4AddrPair addrs = {ciaddr, dst_addr};
        IpSendFlags send_flags = IpSendFlags::AllowBroadcastFlag |
            (ciaddr.isZero() ? IpSendFlags::AllowNonLocalSrc : IpSendFlags());
        
        // Determine the UDP ports.
        UdpTxInfo<UdpArg> udp_info = {DhcpClientPort, DhcpServerPort};

        // Send the UDP packet.
        udp().sendUdpIp4Packet(addrs, udp_info, udp_data, iface(), this, send_flags);
    }
    
    void new_xid ()
    {
        m_xid = std::uint32_t(platform().getTime());
    }
};

/**
 * Static configuration options for @ref IpDhcpClientService.
 */
struct IpDhcpClientOptions {
    /**
     * TTL of outgoing DHCP datagrams.
     */
    AIPSTACK_OPTION_DECL_VALUE(DhcpTTL, std::uint8_t, 64)
    
    /**
     * Maximum number of DNS servers that can be stored.
     */
    AIPSTACK_OPTION_DECL_VALUE(MaxDnsServers, std::uint8_t, 2)
    
    /**
     * Maximum size of client identifier that can be sent.
     */
    AIPSTACK_OPTION_DECL_VALUE(MaxClientIdSize, std::uint8_t, 16)
    
    /**
     * Maximum size of vendor class ID that can be sent.
     */
    AIPSTACK_OPTION_DECL_VALUE(MaxVendorClassIdSize, std::uint8_t, 16)
    
    /**
     * Maximum times that an XID will be reused.
     */
    AIPSTACK_OPTION_DECL_VALUE(XidReuseMax, std::uint8_t, 3)
    
    /**
     * Maximum times to send a request after an offer before reverting to discovery.
     */
    AIPSTACK_OPTION_DECL_VALUE(MaxRequests, std::uint8_t, 3)
    
    /**
     * Maximum times to send a request in REBOOTING state before revering to discovery.
     */
    AIPSTACK_OPTION_DECL_VALUE(MaxRebootRequests, std::uint8_t, 2)
    
    /**
     * Base retransmission time in seconds, before any backoff.
     */
    AIPSTACK_OPTION_DECL_VALUE(BaseRtxTimeoutSeconds, std::uint8_t, 3)
    
    /**
     * Maximum retransmission timeout (except in RENEWING or REBINDING states).
     */
    AIPSTACK_OPTION_DECL_VALUE(MaxRtxTimeoutSeconds, std::uint8_t, 64)
    
    /**
     * Delay before sending a discover in certain error scenarios.
     *
     * This delay is used:
     * - after receing a NAK in response to a request following an offer,
     * - after receiving an ARP response while checking the offered address.
     */
    AIPSTACK_OPTION_DECL_VALUE(ResetTimeoutSeconds, std::uint8_t, 3)
    
    /**
     * Minimum request retransmission time when renewing a lease (in RENEWING or
     * REBINDING states).
     */
    AIPSTACK_OPTION_DECL_VALUE(MinRenewRtxTimeoutSeconds, std::uint8_t, 60)
    
    /**
     * How long to wait for a response to each ARP query when checking the address.
     */
    AIPSTACK_OPTION_DECL_VALUE(ArpResponseTimeoutSeconds, std::uint8_t, 1)
    
    /**
     * Number of ARP queries to send before proceeding with address assignment if no
     * response is received.
     * 
     * Normally when there is no response, ArpResponseTimeoutSeconds*NumArpQueries
     * will be spent for checking the address using ARP.
     */
    AIPSTACK_OPTION_DECL_VALUE(NumArpQueries, std::uint8_t, 2)
};

/**
 * Service definition for @ref IpDhcpClient.
 * 
 * The template parameters of this class are assignments of options defined in
 * @ref IpDhcpClientOptions, for example:
 * AIpStack::IpDhcpClientOptions::DhcpTTL::Is\<16\>. The defaults (achieved with
 * an emtpy parameter list) should be suitable for most uses.
 * 
 * An @ref IpDhcpClient class type can be obtained as follows:
 * 
 * ```
 * using MyDhcpClientService = AIpStack::IpDhcpClientService<...options...>;
 * class MyDhcpClientArg : public MyDhcpClientService::template Compose<
 *     PlatformImpl, IpStackArg> {};
 * using MyDhcpClient = AIpStack::IpDhcpClient<MyDhcpClientArg>;
 * ```
 * 
 * @tparam Options Assignments of options defined in @ref IpDhcpClientOptions.
 */
template<typename ...Options>
class IpDhcpClientService {
    template<typename>
    friend class IpDhcpClient;
    
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, DhcpTTL)
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, MaxDnsServers)
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, MaxClientIdSize)
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, MaxVendorClassIdSize)
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, XidReuseMax)
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, MaxRequests)
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, MaxRebootRequests)
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, BaseRtxTimeoutSeconds)
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, MaxRtxTimeoutSeconds)
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, ResetTimeoutSeconds)
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, MinRenewRtxTimeoutSeconds)
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, ArpResponseTimeoutSeconds)
    AIPSTACK_OPTION_CONFIG_VALUE(IpDhcpClientOptions, NumArpQueries)
    
public:
    /**
     * Template to get the template parameter for @ref IpDhcpClient.
     * 
     * See @ref IpDhcpClientService for an example of instantiating the @ref IpDhcpClient.
     * It is advised to not pass this type directly to @ref IpDhcpClient but pass a dummy
     * user-defined class which inherits from it.
     * 
     * @tparam PlatformImpl_ The platform implementation class, should be the same as
     *         passed to @ref IpStackService::Compose.
     * @tparam StackArg_ Template parameter of @ref IpStack.
     */
    template<typename PlatformImpl_, typename StackArg_>
    struct Compose {
#ifndef IN_DOXYGEN
        using PlatformImpl = PlatformImpl_;
        using StackArg = StackArg_;
        using Params = IpDhcpClientService;

        // This is for completeness and is not typically used.
        AIPSTACK_DEF_INSTANCE(Compose, IpDhcpClient)
#endif
    };
};

/** @} */

}

#endif
