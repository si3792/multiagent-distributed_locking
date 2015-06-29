#ifndef DISTRIBUTED_LOCKING_RICARD_AGRAWALA_HPP
#define DISTRIBUTED_LOCKING_RICARD_AGRAWALA_HPP

#include <list>
#include <map>
#include <fipa_acl/fipa_acl.h>
#include <distributed_locking/DLM.hpp>

namespace fipa {
namespace distributed_locking {
/**
 * Implementation of the Ricart Agrawala algorithm. For more information, see http://en.wikipedia.org/wiki/Ricart-Agrawala_algorithm
 */
class RicartAgrawala : public DLM
{
    friend class ResourceLockState;

public:

    /**
     * Constructor
     */
    RicartAgrawala(const fipa::acl::AgentID& self, const std::vector<std::string>& resources);

    /**
     * Tries to lock a resource. Subsequently, isLocked() must be called to check the status.
     */
    virtual void lock(const std::string& resource, const fipa::acl::AgentIDList& agents);
    /**
     * Unlocks a resource, that must have been locked before
     */
    virtual void unlock(const std::string& resource);
    /**
     * Gets the lock state for a resource.
     */
    virtual lock_state::LockState getLockState(const std::string& resource) const;
    /**
     * This message is triggered by the higher instance that uses this library, if a message is received. Sequential calls must be guaranteed.
     * Subclasses MUST call the base implementation, as there are also direct DLM messages not belonging to any underlying protocol.
     */
    virtual bool onIncomingMessage(const fipa::acl::ACLMessage& message);
    /**
     * This message is called by the DLM, if an agent does not respond a REQUEST messages with CONFIRM after a certain timeout.
     * Subclasses can and should react according to the algorithm.
     */
    virtual void agentFailed(const fipa::acl::AgentID& agentName);

    // A typedef for the Lamport Clock and Timestamps.
    typedef unsigned long long LamportTime;

protected:

    // Represents the internal Lamport (Logical) clock
    LamportTime mLamportClock;

    /**
     * Must be called every time a message from another Agent is received, in order to sync with that Agent's clock
     */
    void synchronizeLamportClock(const LamportTime otherTime);

    /**
     * Returns a string representation of a LamportTime
     */
    static std::string toString(const LamportTime time);

    /**
     * Nested class representing an inner state for a certain resource.
     * It is mapped to its resource name.
     */
    struct ResourceLockState
    {
        // Everyone to inform when locking
        fipa::acl::AgentIDList mCommunicationPartners;
        // Every agent who responded the query. Has to be reset in lock().
        fipa::acl::AgentIDList mResponded;
        // Messages to be sent later, by leaving the associated critical resource
        std::list<fipa::acl::ACLMessage> mDeferredMessages;
        // The lock state, initially not interested (=0)
        lock_state::LockState mState;
        // The time we sent our request messages
        LamportTime mInterestTime;
        // The conversationID, which is relevant if we're interested and get a failure message back
        std::string mConversationID;

        void sort();
        void removeCommunicationPartner(const fipa::acl::AgentID& agent);
    };

    // All resources mapped to the their ResourceLockStates
    std::map<std::string, ResourceLockState> mLockStates;

    /**
     * Handles an incoming request
     */
    void handleIncomingRequest(const fipa::acl::ACLMessage& message);
    /**
     * Handles an incoming response.
     */
    void handleIncomingResponse(const fipa::acl::ACLMessage& message);
    /**
     * Handles an incoming failure
     */
    void handleIncomingFailure(const fipa::acl::ACLMessage& message);
    /**
     * Actually handles an incoming failure.
     */
    void handleIncomingFailure(const std::string& resource, const fipa::acl::AgentID& intendedReceiver);
    /**
     * Extracts the information from the content and saves it in the passed references
     */
    void extractInformation(const fipa::acl::ACLMessage& message, LamportTime& time, std::string& resource);
    /**
     * Sends all deferred messages for a certain resource by putting them into outgoingMessages
     */
    void sendAllDeferredMessages(const std::string& resource);

    /**
     * Adds an agent to the ones that responded. This one-liner is encapsulated in a virtual method,
     * so that the extended algorithm can easily extend the behaviour.
     */
    virtual void addRespondedAgent(const fipa::acl::AgentID& agent, std::string resource);

};
} // namespace distributed_locking
} // namespace fipa

#endif // DISTRIBUTED_LOCKING_RICARD_AGRAWALA_HPP
