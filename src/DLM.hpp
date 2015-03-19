#ifndef DISTRIBUTED_LOCKING_DLM_HPP
#define DISTRIBUTED_LOCKING_DLM_HPP

#include <boost/shared_ptr.hpp>
#include <fipa_acl/fipa_acl.h>

/** \mainpage Distributed Locking Mechanism
 *  This library provides an interface for a locking mechanism on distributed systems. This interface is given by the abstract class DLM.
 *
 * Currently, the Ricart Agrawala algorithm ( http://en.wikipedia.org/wiki/Ricart-Agrawala_algorithm )
 * and the Suzuki Kasami algorithm ( http://en.wikipedia.org/wiki/Suzuki-Kasami_algorithm ) are implemented.
 *
 * \section Code
 * The following code snippet shows the basic usage of this library. This is relevant implementing
 * wrapping tasks, e.g., using Orogen. The task has to use the methods seen below and take care of the
 * ACL message transportation.
 *
 \verbatim
    #include <distributed_locking/DLM.hpp>

    using namespace fipa::distributed_locking;

    // Create an agent
    fipa::acl::AgentID agent("agent1");
    // Create the DLM with the desired protocol using the factory method.
    DLM::Ptr dlm = DLM::create(protocol::RICART_AGRAWALA, a1);

    ...
    // Get outgoing messages:
    if(dlm->hasOutgoingMessages())
        ACLMessage msg = dlm->popNextOutgoingMessage();

    ...
    // Forward incoming messages
    dlm->onIncomingMessage(otherMsg);

    ...
    // Lock with a list of agents (using boost::assign::list_of)
    dlm1->lock("resource_name", boost::assign::list_of(agent2)(agent3));
    ...
    // Check lock status
    if(dlm->getLockState("resource_name") == lock_state::LOCKED) // Can be LOCKED, INTERESTED, or NOT_INTERESTED
    {
        // We are in the criticial section
    }
    ...
    // Unlock
    dlm->unlock("resource_name");

 \endverbatim
 *
 */

namespace fipa {
namespace distributed_locking {

namespace lock_state {
/**
    \enum LockState
    \brief an enum of all the possible lock states per resource
*/
enum LockState { UNREACHABLE = -1, NOT_INTERESTED = 0, INTERESTED, LOCKED };
} // namespace lock_state

namespace protocol {
/**
    \enum Protocol
    \brief an enum of all the implementations
*/
enum Protocol { DLM_DISCOVER = -2, DLM_PROBE = -1, RICART_AGRAWALA = 0, RICART_AGRAWALA_EXTENDED, SUZUKI_KASAMI, SUZUKI_KASAMI_EXTENDED,
    // Following values only for enumerating over this enum
    PROTOCOL_START = RICART_AGRAWALA, PROTOCOL_END = SUZUKI_KASAMI_EXTENDED
};

} // namespace protocol

/**
 * A distributed locking mechanism. This class is abstract.
 */
class DLM
{

public:
    typedef boost::shared_ptr<DLM> Ptr;

    /**
     * Factory method to create an instance of a certain DLM implementation
     */
    static DLM::Ptr create(protocol::Protocol protocol, const fipa::acl::AgentID& self, const std::vector<std::string>& resources);

    /**
     * Always include a virtual destructor in polymorphic base classes.
     */
    virtual ~DLM();

    /**
     * For a given Protocol, returns its textual value.
     */
    static std::string getProtocolTxt(protocol::Protocol protocol);

    /**
     * Get the active protocol
     */
    protocol::Protocol getProtocol() const;

    /**
     * Return name of active protocol
     */
    std::string getProtocolName() const { return getProtocolTxt(mProtocol); }

    /**
     * Sets the agent this DLM works with.
     */
    void setSelf(const fipa::acl::AgentID& self);

    /**
     * Gets the agent this DLM works with.
     */
    const fipa::acl::AgentID& getSelf() const;

    /**
     * Gets the next outgoing message, and removes it from the internal queue. Used by the higher instance that uses this library.
     * Must only be called if hasOutgoingMessages == true.
     */
    fipa::acl::ACLMessage popNextOutgoingMessage();

    /**
     * True, if there are outgoing messages than can be obtained with popNextOutgoingMessage.
     */
    bool hasOutgoingMessages() const;

    /**
     * This method MUST be called periodically by the wrapping component. It runs everything, that needs to be done regularly.
     * For DLM, this is sending PROBE messages and checking if SUCCESS messages were received.
     */
    virtual void trigger();

    /**
     * Tries to lock a resource. Subsequently, isLocked() must be called to check the status.
     */
    virtual void lock(const std::string& resource, const fipa::acl::AgentIDList& agents);

    /**
     * Unlocks a resource, that should have been locked before.
     */
    virtual void unlock(const std::string& resource);

    /**
     * Gets the lock state for a resource.
     */
    virtual lock_state::LockState getLockState(const std::string& resource) const;

    /**
     * This message is triggered by the higher instance that uses this library, if a message is received. Sequential calls must be guaranteed.
     * Subclasses MUST call the base implementation, to handle default messages
     * \return true if message was handled, false otherwise
     */
    virtual bool onIncomingMessage(const fipa::acl::ACLMessage& message);

    /**
     * This message is called by the DLM, if an agent does not respond REQUEST messages with CONFIRM after a certain timeout.
     * Subclasses can and should react according to the algorithm.
     */
    virtual void agentFailed(const fipa::acl::AgentID& agent);

    /**
     * Discover a resource from a set of given agents
     */
    void discover(const std::string& resource, const fipa::acl::AgentIDList& agents);

    /**
     * Check if the owner of the given resource is known
     */
    bool hasKnownOwner(const std::string& resource) const;

    /**
     * Set the probe timeout in seconds
     */
    void setProbeTimeout(double timeInS);

    /**
     * Get the probe timeout in seconds
     * default is 5 seconds
     */
    double getProbeTimeout() const { return mProbeTimeoutInS; }

protected:
    /**
     * A structure for organizing sending probe messages
     */
    struct ProbeRunner
    {
        // The timestamp, when the last PROBE was sent.
        base::Time mTimeStamp;
        // A list of resources for which probes have been requested.
        // Sending them will only be stopped, if the list is empty.
        std::list<std::string> mResources;
        // Whether the partner responded.
        bool mSuccess;
    };

    // A mapping between protocols and strings
    static std::map<protocol::Protocol, std::string> protocolTxt;

    /**
     * Protected constructor with the agent to manage and a list of physically owned resources.
     */
    DLM(protocol::Protocol protocol, const fipa::acl::AgentID& self, const std::vector<std::string>& resources);

    // The agent represented by this DLM
    fipa::acl::AgentID mSelf;
    // Protocol that is active
    protocol::Protocol mProtocol;

    // List of outgoing messages.
    std::list<fipa::acl::ACLMessage> mOutgoingMessages;
    // Current number for conversation IDs
    int mConversationIDnum;

    typedef std::map<std::string, fipa::acl::AgentID> ResourceAgentMap;
    // The physically owned resources of all agents known. Maps resource->agent
    ResourceAgentMap mOwnedResources;
    // The (logical) lock holders of the owned resources. Maps resource->agent
    ResourceAgentMap mLockHolders;

    // All probe runners. agent -> ProbeRunner
    typedef std::map<fipa::acl::AgentID, ProbeRunner> ProbeRunnerMap;
    ProbeRunnerMap mProbeRunners;

    /**
     * Set active protocol
     */
    void setProtocol(protocol::Protocol protocol) { mProtocol = protocol; }

    /**
     * This method MUST be called by implementing subclasses, when the lock is obtained.
     * Like this we can keep track of logical owners of our own resources.
     */
    void lockObtained(const std::string& resource, const std::string& conversationId);

    /**
     * This method MUST be called by implementing subclasses, when the lock is released.
     * Like this we can keep track of logical owners of our own resources.
     */
    void lockReleased(const std::string& resource, const std::string& conversationId);

    /**
     * Tells the DLM to send PROBE messages to the agent in intervals, and call agentFailed, if it does not respond.
     */
    void startRequestingProbes(const fipa::acl::AgentID& agent, const std::string resourceName);

    /**
     * Tells the DLM to stop sending PROBE messages to the agent.
     */
    void stopRequestingProbes(const fipa::acl::AgentID& agent, const std::string resourceName);

    /**
     * Prepares an message with this agent as sender -- by default creates an
     * new conversation id
     */
    fipa::acl::ACLMessage prepareMessage(fipa::acl::ACLMessage::Performative performative, const std::string& protocol, const std::string& content = "");

    /**
     * Send a message
     */
    void sendMessage(const fipa::acl::ACLMessage& msg);

private:
    fipa::acl::ConversationMonitor mConversationMonitor;
    // The timeout of probe messages in seconds
    double mProbeTimeoutInS;

    /**
     * Send a probe message to the agent
     */
    void sendProbe(const fipa::acl::AgentID& agentName);

    /**
     * Tries to handles incoming messages
     * \return true if message was handled
     */
    bool onIncomingDLMMessage(const fipa::acl::ACLMessage& message);

    /**
     * Handles incoming Probe messages
     * \return true if message was handled
     */
    bool onIncomingProbeMessage(const fipa::acl::ACLMessage& message);
};

} // namespace distributed_locking
} // namespace fipa

#endif // DISTRIBUTED_LOCKING_DLM_HPP
