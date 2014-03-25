#ifndef DISTRIBUTED_LOCKING_SUZUKI_KASAMI_HPP
#define DISTRIBUTED_LOCKING_SUZUKI_KASAMI_HPP

#include "DLM.hpp"
#include "Agent.hpp"
#include <fipa_acl/fipa_acl.h>

#include <deque>
#include <map>

namespace fipa {
namespace distributed_locking {
/**
 * Implementation of the Suzuki Kasami algorithm. For more information, see http://en.wikipedia.org/wiki/Suzuki-Kasami_algorithm
 */
class SuzukiKasami : public DLM
{
public:
    /**
     * The implemented protocol
     */
    static const protocol::Protocol protocol;
    
    /**
     * Default constructor
     */
    SuzukiKasami();
    /**
     * Constructor
     */
    SuzukiKasami(const Agent& self, const std::vector<std::string>& resources);

    /**
     * Tries to lock a resource. Subsequently, isLocked() must be called to check the status.
     */
    virtual void lock(const std::string& resource, const std::list<Agent>& agents);
    /**
     * Unlocks a resource, that must have been locked before
     */
    virtual void unlock(const std::string& resource);
    /**
     * Gets the lock state for a resource.
     */
    virtual lock_state::LockState getLockState(const std::string& resource);
    /**
     * This message is triggered by the higher instance that uses this library, if a message is received. Sequential calls must be guaranteed.
     * Subclasses MUST call the base implementation, as there are also direct DLM messages not belonging to any underlying protocol.
     */
    virtual void onIncomingMessage(const fipa::acl::ACLMessage& message);
    /**
     * This message is called by the DLM, if an agent does not respond PROBE messages with SUCCESS after a certain timeout.
     * Subclasses can and should react according to the algorithm.
     */
    virtual void agentFailed(const std::string& agentName);

private:
    /**
     * A token used in this protocol.
     */
    struct Token
    {
        // LastRequestNumber for each of the agents
        std::map<std::string, int> mLastRequestNumber;
         // Queue of agents waiting for the token
        std::deque<std::string> mQueue;
        
        /**
         * Boost serialization method
         */
        template<class Archive>
        void serialize(Archive& ar, unsigned int version)
        {
            ar & mLastRequestNumber;
            ar & mQueue;
        }
    };
    
    /**
     * Nested class representing an inner state for a certain resource.
     * It is mapped to its resource name.
     */
    struct ResourceLockState
    {
        // The token.
        Token mToken;
        // Whether the token is currently held
        bool mHoldingToken;
        // Everyone to inform when locking
        std::list<Agent> mCommunicationPartners;
        // Last known request number for each of the agents
        std::map<std::string, int> mRequestNumber;
        // The lock state, initially not interested (=0)
        lock_state::LockState mState;
        // The conversationID, which is relevant if we're interested and get a failure message back
        std::string mConversationID;
    };
    
    // All resources mapped to the their ResourceLockStates
    std::map<std::string, ResourceLockState> mLockStates;

    /**
     * Handles an incoming request
     */
    void handleIncomingRequest(const fipa::acl::ACLMessage& message);
    /**
     * Handles an incoming response
     */
    void handleIncomingResponse(const fipa::acl::ACLMessage& message);
    /**
     * Handles an incoming failure
     */
    void handleIncomingFailure(const fipa::acl::ACLMessage& message);
    /**
     * Actually handles an incoming failure.
     */
    void handleIncomingFailure(const std::string& resource, std::string intendedReceiver);
    /**
     * Extracts the information from the content and saves it in the passed references
     */
    void extractInformation(const fipa::acl::ACLMessage& message, std::string& resource, int& sequence_number);
    /**
     * Extracts the information from the content and saves it in the passed references
     */
    void extractInformation(const fipa::acl::ACLMessage& message, std::string& resource, Token& token);
    /**
     * Send the token to the receiver. No checks (token held, lock not held) are made!
     */
    void sendToken(const fipa::acl::AgentID& receiver, const std::string& resource, const std::string& conversationID);
    
};
} // namespace distributed_locking
} // namespace fipa

#endif // DISTRIBUTED_LOCKING_SUZUKI_KASAMI_HPP